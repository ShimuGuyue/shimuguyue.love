import { existsSync, readFileSync, readdirSync, statSync } from 'node:fs'
import { join, resolve } from 'node:path'
import { fileURLToPath, URL } from 'node:url'

import { defineConfig, loadEnv } from 'vite'
import vue from '@vitejs/plugin-vue'
import vueDevTools from 'vite-plugin-vue-devtools'

/** 友链条目结构。 */
interface FriendLinkItem {
  id: string
  title: string
  url: string
  description: string
  image: string
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
function parseFriendLinksMeta(raw: string): Array<Omit<FriendLinkItem, 'image'>> {
  const items: Array<Partial<FriendLinkItem>> = []
  let current: Partial<FriendLinkItem> | null = null
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

/** 在友链目录中匹配 `${id}.*` 头像，返回带修改时间版本号的访问 URL。 */
function findFriendAvatar(friendLinksDir: string, id: string): string {
  const entries = readdirSync(friendLinksDir, { withFileTypes: true })
  const files = entries
    .filter((entry) => entry.isFile() && entry.name.startsWith(`${id}.`))
    .map((entry) => entry.name)
    .sort()
  const filename = files[0]
  if (!filename) return ''

  const mtimeMs = statSync(join(friendLinksDir, filename)).mtimeMs
  return `/friend_links/${encodeURIComponent(filename)}?v=${Math.trunc(mtimeMs)}`
}

/** 读取 pull-friend-links.sh 拉取到本地的友链仓库，组装为静态条目数组。 */
function loadFriendLinks(friendLinksDir: string): FriendLinkItem[] {
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
    image: findFriendAvatar(friendLinksDir, item.id),
  }))
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

  // 《友情链接》条目：构建时直接读取 pull-friend-links.sh 拉取到本地的
  // meta.yaml 与 ${id}.* 头像，经 __FRIEND_LINKS__ 注入 Friends.vue。
  const friendLinksDir = env.FILE_PATH
    ? resolve(projectRoot, env.FILE_PATH, 'friend_links')
    : ''
  const friendLinks = loadFriendLinks(friendLinksDir)

  return {
    plugins: [
      vue(),
      vueDevTools(),
    ],
    define: {
      __ABOUT_MARKDOWN__: JSON.stringify(aboutMarkdown),
      __FRIEND_LINKS__: JSON.stringify(friendLinks),
    },
    resolve: {
      alias: {
        '@': fileURLToPath(new URL('./src', import.meta.url)),
      },
    },
    server: {
      proxy: {
        '/api': 'http://localhost:8080',
        '/photo_wall': 'http://localhost:8080',
        '/friend_links': 'http://localhost:8080',
      },
    },
    build: {
      outDir: env.BUILD_DIR || 'dist',
      emptyOutDir: true,
    },
  }
})
