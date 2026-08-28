import { fileURLToPath, URL } from 'node:url'

import { defineConfig, loadEnv } from 'vite'
import vue from '@vitejs/plugin-vue'
import vueDevTools from 'vite-plugin-vue-devtools'

export default defineConfig(({ mode }) => {
  // 从项目 conf/ 目录加载 .env（BUILD_DIR 等非 VITE_ 前缀变量）
  const env = loadEnv(mode, fileURLToPath(new URL('../conf', import.meta.url)), '')

  return {
    plugins: [
      vue(),
      vueDevTools(),
    ],
    resolve: {
      alias: {
        '@': fileURLToPath(new URL('./src', import.meta.url)),
      },
    },
    server: {
      proxy: {
        '/api': 'http://localhost:8080',
        '/image/home': 'http://localhost:8080',
      },
    },
    build: {
      outDir: env.BUILD_DIR || 'dist',
      emptyOutDir: true,
    },
  }
})
