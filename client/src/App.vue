<script setup lang="ts">
import { onMounted, watch } from 'vue'

import { useAuthStore } from '@/stores/auth'
import NavBar from '@/components/NavBar.vue'
import '@/assets/background.css'
import '@/assets/normal/link.css'

const auth = useAuthStore()

let expiryTimer: ReturnType<typeof setTimeout> | undefined

/** 会话过期：清除登录状态。 */
function expireSession() {
    auth.logout()
}

/** 按过期时间安排自动退出；已过期则立即退出。 */
function scheduleExpiry() {
    clearTimeout(expiryTimer)
    if (!auth.isLoggedIn || !auth.expiresAt) return
    const delay = new Date(auth.expiresAt).getTime() - Date.now()
    if (delay <= 0) {
        expireSession()
    } else {
        expiryTimer = setTimeout(expireSession, delay)
    }
}

onMounted(scheduleExpiry)
watch(
    () => auth.isLoggedIn,
    (loggedIn) => {
        if (loggedIn) {
            scheduleExpiry()
        } else {
            clearTimeout(expiryTimer)
        }
    },
)
</script>

<template>
  <NavBar />
  <RouterView />
</template>

<style>
*,
*::before,
*::after {
  margin: 0;
  padding: 0;
  box-sizing: border-box;
}

:root {
  --color-nav-bg: #fff;
  --color-bg: #f5f5f5;
  --color-text: #000;
  --color-text-secondary: #666;
  --color-border: #e8e8e8;
  --color-hover: #f0f0f0;
  --transition-speed: 0.5s;
}

html.dark {
  --color-nav-bg: #1e1e1e;
  --color-bg: #121212;
  --color-text: #fff;
  --color-text-secondary: #999;
  --color-border: #333;
  --color-hover: #2a2a2a;
}

body {
  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto,
    'Helvetica Neue', Arial, sans-serif;
  color: var(--color-text);
  transition: background-color var(--transition-speed), color var(--transition-speed);
}

/* 隐藏滚动条 */
html {
  scrollbar-width: none;
  -ms-overflow-style: none;
}
html::-webkit-scrollbar {
  display: none;
}

</style>
