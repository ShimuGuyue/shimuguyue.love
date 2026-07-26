<script setup lang="ts">
import NavBar from '@/components/NavBar.vue'
import '@/assets/background.css'
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

