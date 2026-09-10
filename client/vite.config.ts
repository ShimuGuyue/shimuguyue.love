import { existsSync, readFileSync, readdirSync } from 'node:fs'
import { join, resolve } from 'node:path'
import { fileURLToPath, URL } from 'node:url'

import { defineConfig, loadEnv, type Plugin } from 'vite'
import vue from '@vitejs/plugin-vue'
import vueDevTools from 'vite-plugin-vue-devtools'

/** 虚拟模块 ID：vite.config.ts 通过它向 Friends.vue 注入构建期友链数据。 */
const FRIEND_LINKS_MODULE_ID = 'virtual:friend-links'

/** 虚拟模块解析后的内部 ID（`\0` 前缀避免与真实文件路径冲突）。 */
const RESOLVED_FRIEND_LINKS_MODULE_ID = `\0${FRIEND_LINKS_MODULE_ID}`

/** 友链条目：文本字段来自 meta.yaml，avatarPath 为头像文件绝对路径（无头像时为空串）。 */
interface FriendLinkEntry {
  id: string
  title: string
  url: string
  description: string
  avatarPath: string
}

/** 去掉 YAML 标量首尾的一对单引号/双引号。 */
function unquoteYamlScalar(value: string): string {
  const text = value.trim()
  if (text.length >= 2) {
    const quoted =
      (text.startsWith('"') && text.endsWith('"')) ||
      (text.startsWith("'") && text.endsWith("'"))
    if (quoted) return text.slice(1, -1)
  }
  return text
}

/**
 * 解析友链 meta.yaml 的顶层条目列表。
 */
function parseFriendLinksMeta(raw: string): Array<Omit<FriendLinkEntry, 'avatarPath'>> {
  const items: Array<Partial<FriendLinkEntry>> = []
  let current: Partial<FriendLinkEntry> | null = null
  const itemPattern = /^[ \t]*-[ \t]+([A-Za-z0-9_-]+)[ \t]*:[ \t]*(.*)$/
  const fieldPattern = /^[ \t]+([A-Za-z0-9_-]+)[ \t]*:[ \t]*(.*)$/

  for (const line of raw.split(/\r?\n/)) {
    const itemMatch = line.match(itemPattern)
    if (itemMatch) {
      current = {}
      items.push(current)
      const key = itemMatch[1]
      if (key === 'id' || key === 'title' || key === 'url' || key === 'description') {
        current[key] = unquoteYamlScalar(itemMatch[2] ?? '')
      }
      continue
    }

    const fieldMatch = line.match(fieldPattern)
    if (fieldMatch && current) {
      const key = fieldMatch[1]
      if (key === 'id' || key === 'title' || key === 'url' || key === 'description') {
        current[key] = unquoteYamlScalar(fieldMatch[2] ?? '')
      }
    }
  }

  return items.map((item, index) => {
    const id = item.id?.trim()
    const title = item.title?.trim()
    const url = item.url?.trim()
    if (!id || !title || !url) {
      throw new Error(
        `友链 meta.yaml 第 ${index + 1} 个条目缺少 id / title / url 字段。`
      )
    }
    return {
      id,
      title,
      url,
      description: item.description?.trim() ?? '',
    }
  })
}

/** 在友链目录中匹配 `${id}.*` 头像，返回头像文件绝对路径，无匹配则为空串。 */
function findFriendAvatarFile(friendLinksDir: string, id: string): string {
  const entries = readdirSync(friendLinksDir, { withFileTypes: true })
  const files = entries
    .filter((entry) => entry.isFile() && entry.name.startsWith(`${id}.`))
    .map((entry) => entry.name)
    .sort()
  const filename = files[0]
  if (!filename) return ''

  return join(friendLinksDir, filename)
}

/**
 * 读取 conf/page_size.yml 中的正整数配置项（形如 `key: value` 的简单 YAML）。
 *
 * 与友链 meta.yaml 一样按行解析，不为单个配置引入 YAML 依赖；
 * 文件缺失或字段非法时抛错，使构建立即失败，避免前后端分页数不一致。
 */
function readPageSizeYaml(path: string, key: string): number {
  if (!existsSync(path)) {
    throw new Error(
      `分页配置缺失：${path}\n` +
      `conf/page_size.yml 由前后端共同读取，请确认该文件存在。`
    )
  }

  for (const line of readFileSync(path, 'utf8').split(/\r?\n/)) {
    const text = line.replace(/#.*$/, '').trim()
    if (!text) continue

    const match = text.match(/^([A-Za-z0-9_-]+)\s*:\s*(.+)$/)
    if (!match || match[1] !== key) continue

    const raw = unquoteYamlScalar(match[2] ?? '')
    const value = Number(raw)
    if (!Number.isInteger(value) || value <= 0) {
      throw new Error(`conf/page_size.yml 字段 ${key} 必须是正整数，当前为：${raw}`)
    }
    return value
  }

  throw new Error(`conf/page_size.yml 缺少字段 ${key}。`)
}

/** 读取 pull-friend-links.sh 拉取到本地的友链仓库，组装条目与头像文件绝对路径。 */
function loadFriendLinks(friendLinksDir: string): FriendLinkEntry[] {
  if (!friendLinksDir) return []

  const metaPath = join(friendLinksDir, 'meta.yaml')
  if (!existsSync(metaPath)) {
    throw new Error(
      `《友情链接》meta.yaml 缺失：${metaPath}\n` +
      `请先运行 tools/pull-friend-links.sh 拉取友链仓库后再构建。`
    )
  }

  const raw = readFileSync(metaPath, 'utf8')
  return parseFriendLinksMeta(raw).map((item) => ({
    ...item,
    avatarPath: findFriendAvatarFile(friendLinksDir, item.id),
  }))
}

/**
 * 生成虚拟模块源码：文本字段内联为对象字面量，头像以 import 交给 Vite 资源管线，
 * 由 Vite 按内容哈希命名并复制进产物目录（URL 中不再需要 mtime 之类的版本参数）。
 */
function buildFriendLinksSource(entries: FriendLinkEntry[]): string {
  const imports: string[] = []
  const images: string[] = []

  entries.forEach((entry, index) => {
    if (!entry.avatarPath) {
      images.push("''")
      return
    }
    imports.push(`import avatar${index} from ${JSON.stringify(entry.avatarPath)}`)
    images.push(`avatar${index}`)
  })

  const items = entries.map(
    (entry, index) =>
      `  { id: ${JSON.stringify(entry.id)}, title: ${JSON.stringify(entry.title)}, ` +
      `url: ${JSON.stringify(entry.url)}, description: ${JSON.stringify(entry.description)}, ` +
      `image: ${entry.avatarPath ? `avatars[${index}]` : "''"} }`
  )

  return [
    ...imports,
    `const avatars = [${images.join(', ')}]`,
    `export default [\n${items.join(',\n')},\n]`,
    '',
  ].join('\n')
}

/**
 * 友链条目注入插件。
 *
 * 读取 $FILE_PATH/friend_links/meta.yaml 生成虚拟模块 virtual:friend-links：
 * 条目经 Friends.vue 引入，头像由 Vite 资源管线处理（构建期输出带内容哈希的文件，
 * dev 期以 /@fs/ 读取），因此前端产物自包含，无需服务端提供友链目录。
 */
function friendLinksPlugin(friendLinksDir: string): Plugin {
  return {
    name: 'friend-links',
    resolveId(id) {
      return id === FRIEND_LINKS_MODULE_ID ? RESOLVED_FRIEND_LINKS_MODULE_ID : null
    },
    load(id) {
      if (id !== RESOLVED_FRIEND_LINKS_MODULE_ID) return null
      return buildFriendLinksSource(loadFriendLinks(friendLinksDir))
    },
  }
}

export default defineConfig(({ mode }) => {
  // 从项目 conf/ 目录加载 .env（BUILD_DIR 等非 VITE_ 前缀变量）
  const env = loadEnv(mode, fileURLToPath(new URL('../conf', import.meta.url)), '')

  // 《关于我》正文：构建时直接读取 pull-readme.sh 拉取到本地的 README.md，
  // 经 __ABOUT_MARKDOWN__ 注入 About.vue。
  const projectRoot = fileURLToPath(new URL('..', import.meta.url))
  const aboutReadmePath = env.FILE_PATH
    ? resolve(projectRoot, env.FILE_PATH, 'README/README.md')
    : ''
  let aboutMarkdown = ''
  if (aboutReadmePath) {
    if (!existsSync(aboutReadmePath)) {
      throw new Error(
        `《关于我》README 缺失：${aboutReadmePath}\n` +
        `请先运行 tools/pull-readme.sh 拉取 README 仓库后再构建。`
      )
    }
    aboutMarkdown = readFileSync(aboutReadmePath, 'utf8')
  }

  // 《友情链接》：构建时读取 pull-friend-links.sh 拉取到本地的 meta.yaml 与
  // ${id}.* 头像，条目经 virtual:friend-links 注入 Friends.vue，头像交给
  // Vite 资源管线按内容哈希命名。
  const friendLinksDir = env.FILE_PATH
    ? resolve(projectRoot, env.FILE_PATH, 'friend_links')
    : ''

  // 博客分页每页条数：构建期注入 __BLOG_PAGE_SIZE__，前后端分页口径一致。
  const blogPageSize = readPageSizeYaml(
    resolve(projectRoot, 'conf/page_size.yml'),
    'blogs'
  )

  return {
    plugins: [
      vue(),
      vueDevTools(),
      friendLinksPlugin(friendLinksDir),
    ],
    define: {
      __ABOUT_MARKDOWN__: JSON.stringify(aboutMarkdown),
      __BLOG_PAGE_SIZE__: JSON.stringify(blogPageSize),
    },
    resolve: {
      alias: {
        '@': fileURLToPath(new URL('./src', import.meta.url)),
      },
    },
    server: {
      fs: {
        // 友链头像位于 $FILE_PATH（工作区之外），dev 期由 Vite 以 /@fs/ 提供，需显式放行
        allow: [projectRoot, friendLinksDir].filter((dir) => dir !== ''),
      },
      proxy: {
        '/api': 'http://localhost:8080',
        '/photo_wall': 'http://localhost:8080',
      },
    },
    build: {
      outDir: env.BUILD_DIR || 'dist',
      emptyOutDir: true,
      // 头像始终作为独立文件产出，不内联为 data URL（保持可缓存与按内容哈希命名）
      assetsInlineLimit: (file) =>
        friendLinksDir !== '' && file.startsWith(friendLinksDir) ? false : undefined,
    },
  }
})
