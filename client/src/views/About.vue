<script setup lang="ts">
import MarkdownPreview from '@/components/MarkdownPreview.vue'

import '@/assets/blog-layout.css'
import '@/assets/normal/color.css'
import '@/assets/background/block.css'

/**
 * 《关于我》正文。
 *
 * 构建期静态注入：client/vite.config.ts 读取
 * $FILE_PATH/doc/README/README.md（pull-readme.sh 拉取），
 * 经 __ABOUT_MARKDOWN__ 直接打包进页面。
 */
const aboutContent = __ABOUT_MARKDOWN__
</script>

<template>
  <main class="about-page">
    <article v-if="aboutContent" class="blog-detail__content">
      <MarkdownPreview :model-value="aboutContent" />
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
  background-color: var(--blog-surface-bg);
  border: 1px solid var(--color-border);
  border-radius: var(--blog-surface-radius);
  padding: var(--blog-surface-padding);
  box-shadow: var(--blog-surface-shadow);
}
.about-page__status {
  text-align: center;
  padding: 64px 0;
  font-size: 1rem;
  color: var(--color-text-secondary);
}
</style>
