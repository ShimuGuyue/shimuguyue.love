<script setup lang="ts">
import { ref, computed, onMounted, nextTick, watch } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { useAuthStore } from '@/stores/auth'
import MarkdownIt from 'markdown-it'
import taskLists from 'markdown-it-task-lists'
import markdownItGitHubAlerts from 'markdown-it-github-alerts'
import hljs from 'highlight.js'
import 'highlight.js/styles/github.css'
import katex from 'katex'
import 'katex/dist/katex.min.css'

const route = useRoute()
const router = useRouter()
const auth = useAuthStore()

const permissions = ref<string[]>([])
const canDrop = computed(() => permissions.value.includes('drop'))

const md = new MarkdownIt({
  html: true,
  linkify: true,
  typographer: true,
})
  .use(taskLists, { enabled: true, label: true, labelAfter: true })
  .use(markdownItGitHubAlerts)

md.enable('strikethrough')

/** 自定义 fence 渲染：在代码块上显示语言标签 */
const rawFence = md.renderer.rules.fence!
md.renderer.rules.fence = (tokens, idx, options, env, self): string => {
  const token = tokens[idx]!
  const lang = token.info.trim().split(/\s+/)[0] ?? ''
  const body = rawFence(tokens, idx, options, env, self)
  if (!lang) return body
  return `<div class="code-block"><div class="code-block__lang">${md.utils.escapeHtml(lang)}</div>${body}</div>`
}

/** 自定义 heading_open：添加锚点 id 和 # 链接 */
const fallbackOpen = (tokens: any, idx: any, options: any, env: any, self: any) => self.renderToken(tokens, idx, options)
const rawHeadingOpen = md.renderer.rules.heading_open || fallbackOpen
md.renderer.rules.heading_open = (tokens, idx, options, env, self): string => {
  const token = tokens[idx]!
  const nextToken = tokens[idx + 1]
  let slug = ''
  if (nextToken && nextToken.type === 'inline') {
    slug = nextToken.content.replace(/[^a-zA-Z0-9\u4e00-\u9fff]+/g, '-').replace(/^-|-$/g, '').toLowerCase()
    token.attrSet('id', slug)
  }
  const html = rawHeadingOpen(tokens, idx, options, env, self)
  return slug ? html + '<a class="heading-anchor" href="#' + slug + '">#</a>' : html
}

/** 自定义 image 渲染：alt 文字作为图片下方说明 */
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

interface BlogDetail {
  id: number
  title: string
  description: string | null
  content: string | null
  update_time: string
  category: string | null
  tags: string[]
}

const blog = ref<BlogDetail | null>(null)
const loading = ref(true)

const renderedContent = computed(() => {
  if (!blog.value?.content) return ''
  let text = blog.value.content
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

interface TocItem {
  level: number
  text: string
  slug: string
  children: TocItem[]
}

function buildTree(flat: { level: number; text: string; slug: string }[]): TocItem[] {
  const root: TocItem[] = []
  const stack: TocItem[] = []
  for (const item of flat) {
    const node: TocItem = { ...item, children: [] }
    while (stack.length && stack[stack.length - 1]!.level >= node.level) stack.pop()
    if (stack.length) {
      stack[stack.length - 1]!.children.push(node)
    } else {
      root.push(node)
    }
    stack.push(node)
  }
  return root
}

const headings = computed<TocItem[]>(() => {
  if (!blog.value?.content) return []
  const flat: { level: number; text: string; slug: string }[] = []
  const re = /^(#{1,6})\s+(.+)$/gm
  let m: RegExpExecArray | null
  while ((m = re.exec(blog.value.content)) !== null) {
    const text = m[2]!.trim()
    const slug = text.replace(/[^a-zA-Z0-9\u4e00-\u9fff]+/g, '-').replace(/^-|-$/g, '').toLowerCase()
    const level = m[1]!.length
    if (level <= 4) flat.push({ level, text, slug })
  }
  return buildTree(flat)
})

function scrollToHeading(slug: string) {
  document.getElementById(slug)?.scrollIntoView({ behavior: 'smooth', block: 'start' })
}

onMounted(async () => {
  try {
    const fp = route.params.file_path as string
    const resp = await fetch('/api/blog?file_path=' + encodeURIComponent(fp))
    if (!resp.ok) throw new Error(`HTTP ${resp.status}`)
    blog.value = await resp.json()
  } catch (e) {
    console.error('获取博客失败:', e)
  } finally {
    loading.value = false
  }

  // 获取当前用户权限
  if (auth.isLoggedIn && auth.id !== null) {
    try {
      const resp = await fetch(`/api/user/permissions?user_id=${auth.id}`)
      if (resp.ok) {
        const data = await resp.json()
        permissions.value = data.permissions || []
      }
    } catch { /* 权限获取失败静默 */ }
  }
})

/// 删除当前博客
async function deleteBlog() {
  if (!canDrop.value) {
    alert('当前用户无 drop 权限，无法删除博客')
    return
  }
  if (!window.confirm('确定要删除这篇博客吗？此操作不可撤销。')) return
  const fp = route.params.file_path as string
  const resp = await fetch('/api/blog/delete', {
    method: 'DELETE',
    headers: {
      'Content-Type': 'application/json',
      'Authorization': `Bearer ${auth.token}`,
    },
    body: JSON.stringify({ file_path: fp }),
  })
  if (!resp.ok) {
    const err = await resp.json().catch(() => ({ error: '删除失败' }))
    alert(err.error || '删除失败')
    return
  }
  router.push({ name: 'blogs' })
}

watch(renderedContent, async () => {
  await nextTick()
  hljs.highlightAll()
})
</script>

<template>
  <main class="blog-detail">
    <p v-if="loading" class="blog-detail__status">加载中...</p>
    <p v-else-if="!blog" class="blog-detail__status">博客不存在</p>
    <div v-else class="blog-detail__layout">
      <!-- 左侧：元信息 -->
      <div class="blog-detail__left">
        <aside class="blog-detail__side glass">
          <h1 class="blog-detail__title">{{ blog.title }}</h1>
          <p v-if="blog.description" class="blog-detail__desc">{{ blog.description }}</p>
          <p v-if="blog.category" class="blog-category">{{ blog.category }}</p>
          <div class="blog-tags">
            <span v-for="tag in blog.tags" :key="tag" class="blog-tag">{{ tag }}</span>
          </div>
          <time class="blog-detail__time">{{ blog.update_time }}</time>
        </aside>
        <div class="blog-detail__actions">
          <button class="blog-detail__delete-btn" @click="deleteBlog">删除博客</button>
        </div>
      </div>

      <!-- 中间：正文 -->
      <article v-if="blog.content" class="blog-detail__content glass" v-html="renderedContent"></article>

      <!-- 右侧：目录 -->
      <nav v-if="headings.length" class="blog-detail__toc glass">
        <h4 class="toc-title">目录</h4>
        <ul class="toc-list">
          <template v-for="h in headings" :key="h.slug">
            <li class="toc-item">
              <span class="toc-text" @click.stop="scrollToHeading(h.slug)">{{ h.text }}</span>
              <ul v-if="h.children.length" class="toc-sublist">
                <li v-for="c2 in h.children" :key="c2.slug" class="toc-item">
                  <span class="toc-text" @click.stop="scrollToHeading(c2.slug)">{{ c2.text }}</span>
                  <ul v-if="c2.children.length" class="toc-sublist">
                    <li v-for="c3 in c2.children" :key="c3.slug" class="toc-item">
                      <span class="toc-text" @click.stop="scrollToHeading(c3.slug)">{{ c3.text }}</span>
                      <ul v-if="c3.children.length" class="toc-sublist">
                        <li v-for="c4 in c3.children" :key="c4.slug" class="toc-item">
                          <span class="toc-text" @click.stop="scrollToHeading(c4.slug)">{{ c4.text }}</span>
                        </li>
                      </ul>
                    </li>
                  </ul>
                </li>
              </ul>
            </li>
          </template>
        </ul>
      </nav>
      <div v-else class="blog-detail__right-placeholder"></div>
    </div>
  </main>
</template>

<style scoped>
.blog-detail {
  padding: 32px 24px 32px;
}

.blog-detail__title,
.blog-detail__desc {
  word-break: break-word;
  overflow-wrap: break-word;
  white-space: normal;
}

.blog-detail__status {
  text-align: center;
  padding: 64px 0;
  font-size: 1rem;
  color: var(--color-text-secondary);
}

.blog-detail__layout {
  max-width: 1400px;
  margin: 0 auto;
  display: grid;
  grid-template-columns: 300px 1fr 300px;;
  gap: 40px;
}

/* ── 左侧 ── */
.blog-detail__left {
  position: sticky;
  top: 120px;
  align-self: start;
}

.blog-detail__side {
  gap: 16px;
  margin-top: 8px;
}

.blog-detail__title {
  margin: 0;
  font-size: 1.6rem;
  color: var(--color-text);
  line-height: 1.4;
}

.blog-detail__desc {
  margin: 0;
  font-size: 0.95rem;
  color: var(--color-text-secondary);
  line-height: 1.6;
}

.blog-detail__time {
  font-size: 0.8rem;
  color: var(--color-text-secondary);
}

.blog-detail__actions {
  display: flex;
  flex-direction: column;
  gap: 8px;
  margin-top: 16px;
}

.blog-detail__delete-btn {
  padding: 8px 0;
  width: 100%;
  border: none;
  border-radius: 4px;
  background: #d44;
  color: #fff;
  font-size: 0.85rem;
  cursor: pointer;
  transition: opacity 0.15s;
}
.blog-detail__delete-btn:hover {
  opacity: 0.85;
}

/* ── 中间正文 ── */
.blog-detail__content {
  min-height: calc(100vh - 177px);
}

/* ── 右侧目录 ── */
.blog-detail__toc {
  position: sticky;
  top: 120px;
  align-self: start;
  max-height: calc(100vh - 120px);
  overflow-y: auto;
  margin-top: 8px;
}

.toc-title {
  margin: 0 0 12px;
  font-size: 0.95rem;
  font-weight: 600;
  color: var(--color-text);
}

.toc-list,
.toc-sublist {
  list-style: none;
  padding: 0 !important;
  margin: 0 !important;
}

.toc-sublist {
  padding-left: 1rem !important; /* 子级层级缩进 */
}

.toc-item {
  position: relative;
  display: flex;
  flex-direction: column;
  margin: 0 !important;
  padding: 0 !important;
}

/* 左侧指示线条：使用 ::before 贴合父元素真实高度 (top: 0; bottom: 0) */
.toc-item::before {
  content: '';
  position: absolute;
  left: 2px;
  top: 0;
  bottom: 0;
  width: 2px;
  background-color: rgba(255, 255, 255, 0.15); /* 默认淡色底线 */
  transition: background-color var(--transition-speed, 0.2s);
  pointer-events: none;
}

/* 当鼠标 hover 到任意层级时，高亮当前层及所有上级父线条 */
.toc-item:hover > ::before,
.toc-item:hover::before {
  background-color: var(--pink-hot, #FF77CC);
}

.toc-text {
  display: inline-block;
  padding: 4px 0 4px 12px;
  font-size: 0.95rem;
  line-height: 1.4;
  color: var(--color-text-secondary);
  cursor: pointer;
  transition: color var(--transition-speed, 0.2s);
}

.toc-item:hover > .toc-text {
  color: var(--pink-hot, #FF77CC);
}

/* 全局覆盖防样式干扰 */
.blog-detail__toc li,
.blog-detail__toc ul {
  margin-top: 0 !important;
  margin-bottom: 0 !important;
}
</style>

<style>
@import "@/assets/blog-layout.css";
@import "@/assets/blog.css";
@import "@/assets/glass.css";
@import "@/assets/markdown.css";
@import "katex/dist/katex.min.css";
</style>