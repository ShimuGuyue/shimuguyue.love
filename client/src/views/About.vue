<script setup lang="ts">
import { ref, onMounted } from 'vue'
import MarkdownPreview from '@/components/MarkdownPreview.vue'

import '@/assets/blog-layout.css'
import '@/assets/glass.css'
import '@/assets/pink-theme.css'

const content = ref('')
const loading = ref(true)

onMounted(async () => {
  try {
    const resp = await fetch('/api/about')
    if (resp.ok) {
      const data = await resp.json()
      content.value = data.content || ''
    }
  } catch { /* 静默 */ }
  loading.value = false
})
</script>

<template>
  <main class="about-page">
    <p v-if="loading" class="about-page__status">加载中...</p>
    <article v-else-if="content" class="blog-detail__content glass">
      <MarkdownPreview :model-value="content" />
    </article>
    <p v-else class="about-page__status">暂无内容</p>
  </main>
</template>

<style scoped>
.about-page {
  padding: 32px 24px 64px;
}
.about-page .blog-detail__content {
  max-width: 900px;
  margin: 0 auto;
  min-height: calc(100vh - 177px);
}
.about-page__status {
  text-align: center;
  padding: 64px 0;
  font-size: 1rem;
  color: var(--color-text-secondary);
}
</style>
