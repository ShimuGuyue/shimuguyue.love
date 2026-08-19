<script setup lang="ts">
import { ref, computed, onMounted, onUnmounted, nextTick } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { useAuthStore } from '@/stores/auth'
import MarkdownPreview from '@/components/MarkdownPreview.vue'
import type { HeadList } from 'md-editor-v3'
import { headingSlug } from '@/lib/md-editor-setup'

import '@/assets/blog-layout.css'
import '@/assets/blog/selector.css'
import '@/assets/normal/color.css'

const route = useRoute()
const router = useRouter()
const auth = useAuthStore()

const permissions = ref<string[]>([])
const canDrop = computed(() => permissions.value.includes('blog:delete'))

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

const headings = ref<TocItem[]>([])

/** 由 MdPreview 的 getCatalog 事件生成目录（仅取 h1~h4）。 */
function handleCatalog(list: HeadList[]) {
  const flat = list
    .filter(item => item.level <= 4)
    .map(item => ({ level: item.level, text: item.text, slug: headingSlug(item.text) }))
  headings.value = buildTree(flat)
}

/** 自定义平滑滚动，支持控制滚动时长 */
function smoothScrollTo(el: HTMLElement, duration = 1000) {
  // 减去 100px 偏移以匹配 scroll-margin-top，避免标题被 navbar 遮挡
  const offset = 100
  const targetTop = el.getBoundingClientRect().top + window.scrollY - offset
  const startTop = window.scrollY
  const distance = targetTop - startTop
  const startTime = performance.now()

  function easeInOutCubic(t: number): number {
    return t < 0.5 ? 4 * t * t * t : 1 - Math.pow(-2 * t + 2, 3) / 2
  }

  function step(currentTime: number) {
    const elapsed = currentTime - startTime
    const progress = Math.min(elapsed / duration, 1)
    window.scrollTo(0, startTop + distance * easeInOutCubic(progress))
    if (progress < 1) {
      requestAnimationFrame(step)
    }
  }

  requestAnimationFrame(step)
}

function scrollToHeading(slug: string) {
  const path = findPathToSlug(headings.value, slug)
  activeSlugs.value = new Set(path.map(item => item.slug))
  // 同步更新 URL hash
  router.replace({ hash: '#' + slug })
  const el = document.getElementById(slug)
  if (el) smoothScrollTo(el)
}

const activeSlugs = ref<Set<string>>(new Set())

function findPathToSlug(tree: TocItem[], target: string): TocItem[] {
  for (const node of tree) {
    if (node.slug === target) return [node]
    if (node.children.length) {
      const found = findPathToSlug(node.children, target)
      if (found.length) return [node, ...found]
    }
  }
  return []
}

function updateActiveHeading() {
  const contentEl = document.querySelector('.blog-detail__content')
  if (!contentEl) return
  // 获取所有标题元素（仅限 h1~h4，与目录层级保持一致）
  const headingEls = contentEl.querySelectorAll('h1, h2, h3, h4')
  if (!headingEls.length) {
    activeSlugs.value = new Set()
    return
  }

  const threshold = 120

  // 经典 scroll-spy：取最后一个顶部已越过阈值的标题
  let activeId = ''
  for (const el of headingEls) {
    if (el.getBoundingClientRect().top <= threshold) {
      activeId = el.id
    }
  }

  // 若没有任何标题越过阈值（页面顶部），回退到第一个标题
  if (!activeId && headingEls.length > 0) {
    activeId = headingEls[0]!.id || ''
  }

  if (activeId) {
    const path = findPathToSlug(headings.value, activeId)
    activeSlugs.value = new Set(path.map(item => item.slug))
  } else {
    activeSlugs.value = new Set()
  }
}

let scrollHandler: (() => void) | null = null

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

  // 初次更新（确保初始高亮正确）
  await nextTick()
  updateActiveHeading()

  // 监听滚动事件（使用节流优化）
  let ticking = false
  scrollHandler = () => {
    if (!ticking) {
      window.requestAnimationFrame(() => {
        updateActiveHeading()
        ticking = false
      })
      ticking = true
    }
  }
  window.addEventListener('scroll', scrollHandler)

  // 拦截标题点击，使用 scrollToHeading 统一滚动逻辑
  document.querySelector('.blog-detail__content')?.addEventListener('click', (e) => {
    const target = e.target as HTMLElement
    const heading = target.closest('h1, h2, h3, h4, h5, h6')
    if (heading && heading.id) {
      e.preventDefault()
      scrollToHeading(heading.id)
    }
  })

  // 获取当前用户权限
  if (auth.isLoggedIn && auth.id !== null) {
    try {
      const resp = await fetch('/api/user/permissions', {
        headers: { 'Authorization': 'Bearer ' + auth.token }
      })
      if (resp.ok) {
        const data = await resp.json()
        permissions.value = data.permissions || []
      }
    } catch { /* 权限获取失败静默 */ }
  }
})

onUnmounted(() => {
  if (scrollHandler) {
    window.removeEventListener('scroll', scrollHandler)
  }
})

/// 跳转到编辑页面
function editBlog() {
  const fp = route.params.file_path as string
  router.push({ name: 'blog-edit', params: { file_path: fp } })
}

/// 删除当前博客
async function deleteBlog() {
  if (!canDrop.value) {
    alert('操作失败：该操作需要 blog:delete 权限')
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

</script>

<template>
  <main class="blog-detail">
    <p v-if="loading" class="blog-detail__status">加载中...</p>
    <p v-else-if="!blog" class="blog-detail__status">博客不存在</p>
    <div v-else class="blog-detail__layout">
      <!-- 左侧：元信息 -->
      <div class="blog-detail__left">
        <aside class="blog-detail__side">
          <h1 class="blog-detail__title">{{ blog.title }}</h1>
          <p v-if="blog.description" class="blog-detail__desc">{{ blog.description }}</p>
          <p v-if="blog.category" class="blog-detail__category tag-pink">{{ blog.category }}</p>
          <div class="blog-tags">
            <span v-for="tag in blog.tags" :key="tag" class="tag-normal">{{ tag }}</span>
          </div>
          <time class="blog-detail__time">{{ blog.update_time }}</time>
        </aside>
        <div class="blog-detail__actions">
          <button class="blog-detail__edit-btn" @click="editBlog">编辑博客</button>
          <button class="blog-detail__delete-btn" @click="deleteBlog">删除博客</button>
        </div>
      </div>

      <!-- 中间：正文 -->
      <article v-if="blog.content" class="blog-detail__content">
        <MarkdownPreview :model-value="blog.content" @get-catalog="handleCatalog" />
      </article>

      <!-- 右侧：目录 -->
      <nav class="blog-detail__toc">
        <h4 class="toc-title">目录</h4>
        <div v-if="headings.length">
          <ul class="toc-list">
            <template v-for="h in headings" :key="h.slug">
              <li class="toc-item" :class="{ active: activeSlugs.has(h.slug) }">
                <span class="toc-text" @click.stop="scrollToHeading(h.slug)">{{ h.text }}</span>
                <ul v-if="h.children.length" class="toc-sublist">
                  <li v-for="c2 in h.children" :key="c2.slug" class="toc-item" :class="{ active: activeSlugs.has(c2.slug) }">
                    <span class="toc-text" @click.stop="scrollToHeading(c2.slug)">{{ c2.text }}</span>
                    <ul v-if="c2.children.length" class="toc-sublist">
                      <li v-for="c3 in c2.children" :key="c3.slug" class="toc-item" :class="{ active: activeSlugs.has(c3.slug) }">
                        <span class="toc-text" @click.stop="scrollToHeading(c3.slug)">{{ c3.text }}</span>
                        <ul v-if="c3.children.length" class="toc-sublist">
                          <li v-for="c4 in c3.children" :key="c4.slug" class="toc-item" :class="{ active: activeSlugs.has(c4.slug) }">
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
        </div>
        <div v-else class="toc-empty">暂无章节</div>
      </nav>
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
  color: var(--color-text);
}

.blog-detail__layout {
  max-width: 1400px;
  margin: 0 auto;
  display: grid;
  grid-template-columns: 1fr 2fr 1fr;
  gap: 40px;
}

/* ── 块级背景：元信息 / 正文 / 目录共用 ──
   复用 --blog-surface-bg（半透明卡片底色，见 background/block.css），
   与博客卡片、筛选栏保持一致的块级组件外观。 */
.blog-detail__side,
.blog-detail__content,
.blog-detail__toc {
  background-color: var(--blog-surface-bg);
  border: 1px solid var(--color-border);
  border-radius: var(--blog-surface-radius);
  padding: var(--blog-surface-padding);
  box-shadow: var(--blog-surface-shadow);
  transition:
    background-color var(--transition-speed),
    border-color var(--transition-speed);
}

/* ── 左侧 ── */
.blog-detail__left {
  position: sticky;
  top: 112px;
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
  color: var(--color-text);
  line-height: 1.6;
}

.blog-detail__category {
  width: fit-content;
}

.blog-detail__time {
  font-size: 0.8rem;
  color: var(--color-text);
}

.blog-detail__actions {
  display: flex;
  flex-direction: column;
  gap: 8px;
  margin-top: 16px;
}

.blog-detail__edit-btn,
.blog-detail__delete-btn {
  padding: 8px 0;
  width: 100%;
  border: none;
  border-radius: 4px;
  font-size: 0.85rem;
  cursor: pointer;
  transition: opacity 0.15s;
}
.blog-detail__edit-btn {
  background: var(--pink-soft);
  color: #fff;
}
.blog-detail__delete-btn {
  background: #d44;
  color: #fff;
}
.blog-detail__edit-btn:hover,
.blog-detail__delete-btn:hover {
  opacity: 0.85;
}

/* ── 中间正文 ── */
.blog-detail__content {
  overflow-wrap: break-word;
  word-wrap: break-word;
  min-width: 0;
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
  min-height: 300px;
}

.toc-empty {
  color: var(--color-text);
  font-size: 0.9rem;
  text-align: center;
  padding: 20px 0;
  flex: 1;
  display: flex;
  align-items: center;
  justify-content: center;
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

.toc-item.active::before,
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

.toc-item.active > .toc-text,
.toc-item:hover > .toc-text {
  color: var(--pink-hot, #FF77CC);
  font-weight: 500;
}

/* 全局覆盖防样式干扰 */
.blog-detail__toc li,
.blog-detail__toc ul {
  margin-top: 0 !important;
  margin-bottom: 0 !important;
}
</style>
