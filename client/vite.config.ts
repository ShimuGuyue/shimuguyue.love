import { existsSync, readFileSync } from 'node:fs'
import { resolve } from 'node:path'
import { fileURLToPath, URL } from 'node:url'

import { defineConfig, loadEnv } from 'vite'
import vue from '@vitejs/plugin-vue'
import vueDevTools from 'vite-plugin-vue-devtools'

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

  return {
    plugins: [
      vue(),
      vueDevTools(),
    ],
    define: {
      __ABOUT_MARKDOWN__: JSON.stringify(aboutMarkdown),
    },
    resolve: {
      alias: {
        '@': fileURLToPath(new URL('./src', import.meta.url)),
      },
    },
    server: {
      proxy: {
        '/api': 'http://localhost:8080',
        '/image/home': 'http://localhost:8080',
        '/image/friend_avatars': 'http://localhost:8080',
      },
    },
    build: {
      outDir: env.BUILD_DIR || 'dist',
      emptyOutDir: true,
    },
  }
})
