<script setup lang="ts">
import { ref, onMounted, computed } from 'vue'
import MarkdownIt from 'markdown-it'
import taskLists from 'markdown-it-task-lists'
import markdownItGitHubAlerts from 'markdown-it-github-alerts'
import hljs from 'highlight.js'
import 'highlight.js/styles/github.css'
import katex from 'katex'
import 'katex/dist/katex.min.css'

const md = new MarkdownIt({ html: true, linkify: true, typographer: true })
  .use(taskLists, { enabled: true, label: true, labelAfter: true })
  .use(markdownItGitHubAlerts)
md.enable('strikethrough')

const rawFence = md.renderer.rules.fence!
md.renderer.rules.fence = (tokens, idx, options, env, self): string => {
  const token = tokens[idx]!
  const lang = token.info.trim().split(/\s+/)[0] ?? ''
  const body = rawFence(tokens, idx, options, env, self)
  if (!lang) return body
  return `<div class="code-block"><div class="code-block__lang">${md.utils.escapeHtml(lang)}</div>${body}</div>`
}

const fallbackOpen = (tokens: any, idx: any, options: any, env: any, self: any) => self.renderToken(tokens, idx, options)
const rawHeadingOpen = md.renderer.rules.heading_open || fallbackOpen
md.renderer.rules.heading_open = (tokens, idx, options, env, self): string => {
  const token = tokens[idx]!
  const nextToken = tokens[idx + 1]
  let slug = ''
  if (nextToken && nextToken.type === 'inline') {
    slug = nextToken.content.replace(/[^a-zA-Z0-9\u4e00-\u9fff]+/g, '-').replace(/^-|-$/g, '').toLowerCase()
    token!.attrSet('id', slug)
  }
  const html = rawHeadingOpen(tokens, idx, options, env, self)
  return slug ? html + '<a class="heading-anchor" href="#' + slug + '">#</a>' : html
}

const rawImage = md.renderer.rules.image!
md.renderer.rules.image = (tokens, idx, options, env, self): string => {
  const token = tokens[idx]!
  const src = token.attrGet('src') ?? ''
  const alt = token.content
  const img = rawImage(tokens, idx, options, env, self)
  if (!alt) return img
  const escapedAlt = md.utils.escapeHtml(alt)
  return `<figure><img src="${md.utils.escapeHtml(src)}" alt="${escapedAlt}" loading="lazy"><figcaption>${escapedAlt}</figcaption></figure>`
}

const content = ref('')
const loading = ref(true)

const renderedContent = computed(() => {
  if (!content.value) return ''
  let text = content.value
  text = text.replace(/\$\$([^$]+)\$\$/g, (_, f) => renderKatex(f, true))
  text = text.replace(/\$([^$]+)\$/g, (_, f) => renderKatex(f, false))
  return md.render(text)
})

function renderKatex(formula: string, display: boolean): string {
  try {
    return katex.renderToString(formula, { displayMode: display, throwOnError: false })
  } catch {
    return display ? `$${formula}$` : `$${formula}$$`
  }
}

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
    <article
      v-else-if="content"
      class="blog-detail__content glass"
      v-html="renderedContent"
    ></article>
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

<style>
@import "@/assets/blog-layout.css";
@import "@/assets/markdown.css";
@import "@/assets/glass.css";
</style>
