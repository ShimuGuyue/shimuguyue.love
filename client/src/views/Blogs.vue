<script setup lang="ts">
import { ref, watch, computed, onMounted } from 'vue'
import { useRouter, useRoute, onBeforeRouteUpdate } from 'vue-router'

import '@/assets/blog/selector.css'
import '@/assets/blog/card.css'
import '@/assets/button/function.css'

const router = useRouter()
const route = useRoute()

// ── 类型定义 ──

interface Category {
  id: number
  name: string
}

interface Tag {
  id: number
  name: string
  category_id: number
}

interface BlogItem {
  id: number
  title: string
  description: string | null
  update_time: string
  category: string | null
  file_path: string | null
  tags: string[]
}

// ── 数据 ──

const categories = ref<Category[]>([])
const tags = ref<Tag[]>([])
const blogs = ref<BlogItem[]>([])

// ── 分页 ──

/** 每页显示的博客条目数 */
const PAGE_SIZE = 15

const page = ref(1)

/** 总页数：至少 1 页，空结果时不出现 0 页 */
const pageCount = computed(() =>
  Math.max(1, Math.ceil(blogs.value.length / PAGE_SIZE))
)

const pageNumbers = computed(() =>
  Array.from({ length: pageCount.value }, (_, i) => i + 1)
)

/** 当前页要展示的博客条目 */
const pagedBlogs = computed(() => {
  const start = (page.value - 1) * PAGE_SIZE
  return blogs.value.slice(start, start + PAGE_SIZE)
})

/** 读取 URL 页码：非正整数（空值、字母、0、负数）一律按第 1 页处理 */
function readPage(raw: unknown): number {
  const num = Number(raw)
  return Number.isInteger(num) && num >= 1 ? num : 1
}

/** 解析 URL 页码：超出总页数的非法页回到第 1 页（需在数据加载后调用） */
function parsePage(raw: unknown): number {
  const num = readPage(raw)
  return num > pageCount.value ? 1 : num
}

/**
 * 应用 URL 中的页码：非法页回到第 1 页，并把 URL 归一化（去掉无效的 ?page=）。
 * @param raw URL query 中的 page 值
 */
function applyUrlPage(raw: unknown) {
  const requested = readPage(raw)
  page.value = parsePage(raw)
  if (requested > pageCount.value) {
    syncUrl()
  }
}

/** 判断两个 ID 集合是否相同（与顺序无关） */
function sameIds(a: number[], b: number[]): boolean {
  const key = (list: number[]) => [...list].sort((x, y) => x - y).join()
  return key(a) === key(b)
}

/** 翻页：越界或与当前页相同时忽略，页码写入 URL 并滚回列表顶部 */
function goToPage(num: number) {
  if (num < 1 || num > pageCount.value || num === page.value) return
  page.value = num
  // 点击翻页按钮时页码始终写入 URL，第 1 页也保留 ?page=1
  syncUrl(true)
  window.scrollTo({ top: 0, behavior: 'smooth' })
}

// 筛选后条目变少时，把页码收敛到最后一页
watch(pageCount, () => {
  if (page.value > pageCount.value) {
    page.value = pageCount.value
  }
})

// ── 筛选状态 ──

const selectedCategoryIds = ref<number[]>([])
const categoryMulti = ref(false)

const selectedTagIds = ref<number[]>([])
const tagMulti = ref(false)

const searchQuery = ref('')

let searchTimer: ReturnType<typeof setTimeout> | null = null

// ── 计算：当前可见的标签（按选中的分类过滤，同名标签合并为一个） ──

const visibleTags = computed<Tag[]>(() => {
  let list = tags.value
  if (selectedCategoryIds.value.length > 0) {
    // 多选时取所有选中分类的标签并集
    const idSet = new Set(selectedCategoryIds.value)
    list = tags.value.filter(t => idSet.has(t.category_id))
  }
  const seen = new Set<string>()
  return list.filter(t => {
    if (seen.has(t.name)) return false
    seen.add(t.name)
    return true
  })
})

/** 获取指定名称标签在全部分类下的所有 ID（同名标签一并选中） */
function tagIdsByName(name: string): number[] {
  return tags.value.filter(t => t.name === name).map(t => t.id)
}

// 分类切换后清除已不存在的标签选中
let initializing = false
watch(selectedCategoryIds, () => {
  if (initializing) return
  const visibleNames = new Set(visibleTags.value.map(t => t.name))
  selectedTagIds.value = selectedTagIds.value.filter(id => {
    const name = tags.value.find(t => t.id === id)?.name
    return name !== undefined && visibleNames.has(name)
  })
})

// ── 远程获取 ──

const loading = ref(true)

async function fetchCategories() {
  try {
    const resp = await fetch('/api/categories')
    if (!resp.ok) throw new Error(`HTTP ${resp.status}`)
    categories.value = await resp.json()
  } catch (e) { console.error('获取分类失败:', e) }
}

async function fetchTags() {
  try {
    const resp = await fetch('/api/tags')
    if (!resp.ok) throw new Error(`HTTP ${resp.status}`)
    tags.value = await resp.json()
  } catch (e) { console.error('获取标签失败:', e) }
}

async function fetchBlogs(skipSync = false) {
  const params = new URLSearchParams()

  if (selectedCategoryIds.value.length > 0) {
    params.set('category_ids', selectedCategoryIds.value.join(','))
  }

  if (selectedTagIds.value.length > 0) {
    params.set('tag_ids', selectedTagIds.value.join(','))
  }

  const q = searchQuery.value.trim()
  if (q) {
    params.set('q', q)
  }

  loading.value = true
  try {
    const resp = await fetch('/api/blogs?' + params.toString())
    if (!resp.ok) throw new Error(`HTTP ${resp.status}`)
    blogs.value = await resp.json()
    if (!skipSync) {
      syncUrl()
    }
  } catch (e) {
    console.error('获取博客失败:', e)
    blogs.value = []
  } finally {
    loading.value = false
  }
}

onBeforeRouteUpdate(async (to, from) => {
  const urlCatNames = parseNames(to.query.categories as string | undefined)
  const urlTagNames = parseNames(to.query.tags as string | undefined)
  const nextSearch      = (to.query.q as string) || ''
  const nextCategoryIds = urlCatNames
    .map(n => categories.value.find(c => c.name === n)?.id)
    .filter(Boolean) as number[]
  const nextTagIds = urlTagNames.flatMap(n => tagIdsByName(n))

  // 只有页码变化（翻页写 URL、前进后退）时筛选结果不变，无需重新请求
  const pageOnly =
    nextSearch === searchQuery.value &&
    (to.query.cm === '1') === categoryMulti.value &&
    (to.query.tm === '1') === tagMulti.value &&
    sameIds(nextCategoryIds, selectedCategoryIds.value) &&
    sameIds(nextTagIds, selectedTagIds.value)

  searchQuery.value   = nextSearch
  categoryMulti.value = to.query.cm === '1'
  tagMulti.value      = to.query.tm === '1'
  selectedCategoryIds.value = nextCategoryIds
  selectedTagIds.value = nextTagIds

  if (!pageOnly) {
    await fetchBlogs(true)
  }
  applyUrlPage(to.query.page)
})

// ── URL 同步 ──

/**
 * 把当前筛选条件与页码同步到 URL。
 * @param withPage 为 true 时页码为第 1 页也写入 URL（点击翻页按钮用）
 */
function syncUrl(withPage = false) {
  const q: Record<string, string> = {}
  // ID → name 转换
  const catNames = selectedCategoryIds.value
    .map(id => categories.value.find(c => c.id === id)?.name).filter(Boolean)
  const tagNames = selectedTagIds.value
    .map(id => tags.value.find(t => t.id === id)?.name).filter(Boolean)
  const uniqueTagNames = [...new Set(tagNames)]
  if (catNames.length) q.categories = catNames.join(',')
  if (uniqueTagNames.length) q.tags    = uniqueTagNames.join(',')
  if (searchQuery.value.trim())     q.q = searchQuery.value.trim()
  if (categoryMulti.value)          q.cm = '1'
  if (tagMulti.value)               q.tm = '1'
  if (page.value > 1 || withPage)   q.page = String(page.value)
  router.replace({ query: Object.keys(q).length ? q : {} })
}

function parseNames(raw: string | undefined): string[] {
  if (!raw) return []
  return raw.split(',').map(decodeURIComponent).filter(Boolean)
}

// ── 筛选操作 ──

function toggleCategory(id: number) {
  if (categoryMulti.value) {
    const idx = selectedCategoryIds.value.indexOf(id)
    if (idx >= 0) {
      selectedCategoryIds.value.splice(idx, 1)
    } else {
      selectedCategoryIds.value.push(id)
    }
  } else {
    selectedCategoryIds.value =
      selectedCategoryIds.value[0] === id ? [] : [id]
  }
  page.value = 1
  fetchBlogs()
}

function toggleTag(name: string) {
  const ids = tagIdsByName(name)
  if (ids.length === 0) return
  const anySelected = ids.some(id => selectedTagIds.value.includes(id))
  if (tagMulti.value) {
    if (anySelected) {
      selectedTagIds.value = selectedTagIds.value.filter(id => !ids.includes(id))
    } else {
      selectedTagIds.value = [...selectedTagIds.value, ...ids]
    }
  } else {
    selectedTagIds.value = anySelected ? [] : ids
  }
  page.value = 1
  fetchBlogs()
}

function onSearchInput() {
  if (searchTimer) clearTimeout(searchTimer)
  searchTimer = setTimeout(() => {
    page.value = 1
    fetchBlogs()
  }, 300)
}

// ── 生命周期 ──

onMounted(async () => {
  // 从 URL 读取 name 参数（等数据加载后转 ID）
  const urlCatNames = parseNames(route.query.categories as string | undefined)
  const urlTagNames = parseNames(route.query.tags as string | undefined)
  searchQuery.value   = (route.query.q as string) || ''
  categoryMulti.value = route.query.cm === '1'
  tagMulti.value      = route.query.tm === '1'
  // 先按 URL 设定页码，fetchBlogs 内的 syncUrl 才不会把 ?page= 丢掉
  page.value          = readPage(route.query.page)

  initializing = true
  await Promise.all([fetchCategories(), fetchTags()])

  // name → ID 转换
  selectedCategoryIds.value = urlCatNames
    .map(n => categories.value.find(c => c.name === n)?.id).filter(Boolean) as number[]
  selectedTagIds.value = urlTagNames.flatMap(n => tagIdsByName(n))

  await fetchBlogs()
  applyUrlPage(route.query.page)
  initializing = false
})
</script>

<template>
  <main class="blogs-page">
    <div class="blogs-top">
      <button class="func-btn" @click="router.push('/blog-edit/new')">新建博客</button>
    </div>
    <!-- ── 筛选器 ── -->
    <section class="filter-bar">
      <!-- 分类筛选 -->
      <div class="filter-row">
        <span class="filter-label">分类</span>
        <button class="filter-mode-btn tag-pink" @click="categoryMulti = !categoryMulti">{{ categoryMulti ? '多选' : '单选' }}</button>
        <div class="filter-chips">
          <button
            v-for="cat in categories"
            :key="cat.id"
            class="tag-normal"
            :class="{ 'tag--active': selectedCategoryIds.includes(cat.id) }"
            @click="toggleCategory(cat.id)"
          >
            {{ cat.name }}
          </button>
        </div>
      </div>

      <!-- 标签筛选 -->
      <div class="filter-row">
        <span class="filter-label">标签</span>
        <button class="filter-mode-btn tag-pink" @click="tagMulti = !tagMulti">{{ tagMulti ? '多选' : '单选' }}</button>
        <div class="filter-chips">
          <button
            v-for="tag in visibleTags"
            :key="tag.name"
            class="tag-normal"
            :class="{ 'tag--active': tagIdsByName(tag.name).some(id => selectedTagIds.includes(id)) }"
            @click="toggleTag(tag.name)"
          >
            {{ tag.name }}
          </button>
        </div>
      </div>

      <!-- 搜索 -->
      <div class="filter-search">
        <input
          v-model="searchQuery"
          type="text"
          class="search-input"
          placeholder="搜索标题、描述、分类和标签中的内容..."
          @input="onSearchInput"
        />
      </div>
    </section>

    <!-- ── 博客卡片网格 ── -->
    <p v-if="loading" class="blog-status">加载中...</p>
    <p v-else-if="!blogs.length" class="blog-status">未检索到对应博客</p>
    <section v-else class="blog-grid">
      <RouterLink
        v-for="blog in pagedBlogs"
        :key="blog.id"
        class="blog-card"
        :to="`/blogs/${(blog.file_path ?? '').replace(/^\/+/, '')}`"
      >
        <h3 class="blog-card__title">{{ blog.title }}</h3>
        <p class="blog-card__desc">{{ blog.description }}</p>
        <div class="blog-card__meta blog-tags">
          <span v-if="blog.category" class="tag-pink">{{ blog.category }}</span>
          <span
            v-for="tag in blog.tags"
            :key="tag"
            class="tag-normal"
          >{{ tag }}</span>
        </div>
        <time class="blog-card__time">{{ blog.update_time }}</time>
      </RouterLink>
    </section>

    <!-- ── 分页 ── -->
    <nav v-if="!loading && pageCount > 1" class="blog-pager">
      <div class="blog-pager__pages">
        <button
          type="button"
          class="blog-pager__btn"
          :disabled="page <= 1"
          @click="goToPage(page - 1)"
        >
          上一页
        </button>
        <button
          v-for="num in pageNumbers"
          :key="num"
          type="button"
          class="blog-pager__btn"
          :class="{ 'blog-pager__btn--active': num === page }"
          @click="goToPage(num)"
        >
          {{ num }}
        </button>
        <button
          type="button"
          class="blog-pager__btn"
          :disabled="page >= pageCount"
          @click="goToPage(page + 1)"
        >
          下一页
        </button>
      </div>
    </nav>
  </main>
</template>

<style scoped>
/* ── 页面 ── */

.blogs-page {
  max-width: 1200px;
  margin: 0 auto;
  padding: 24px 32px 48px;
}

/* ── 新建按钮 ── */
.blogs-top {
  display: flex;
  justify-content: flex-end;
  margin-bottom: 12px;
}

.blogs-top .func-btn {
  padding: 10px 20px;
  font-size: 0.9rem;
  color: var(--pink-hot);
  border-color: var(--pink-hot);
}

/* ── 状态提示 ── */

.blog-status {
  text-align: center;
  padding: 48px 0;
  font-size: 0.95rem;
  color: var(--color-text-secondary);
}

/* ── 博客网格 ── */

.blog-grid {
  display: grid;
  grid-template-columns: repeat(3, 1fr);
  gap: 20px;
}

/* ── 分页 ── */

.blog-pager {
  display: flex;
  justify-content: center;
  margin-top: 28px;
}

.blog-pager__pages {
  display: flex;
  flex-wrap: wrap;
  justify-content: center;
  gap: 6px;
}

/* 页码按钮：与卡片同一底色，悬停/选中用主题粉强调 */
.blog-pager__btn {
  min-width: 34px;
  height: 34px;
  padding: 0 12px;
  font-family: inherit;
  font-size: 0.85rem;
  color: var(--color-text);
  background-color: var(--blog-surface-bg);
  border: 1px solid var(--color-border);
  border-radius: 6px;
  cursor: pointer;
  transition:
    color var(--transition-speed),
    background-color var(--transition-speed),
    border-color var(--transition-speed);
}

.blog-pager__btn:hover:not(:disabled):not(.blog-pager__btn--active) {
  color: var(--pink-hot);
  border-color: var(--pink-hot);
}

.blog-pager__btn:disabled {
  opacity: 0.4;
  cursor: not-allowed;
}

/* 当前页：粉色实心 + 白字，与筛选器的选中态保持一致 */
.blog-pager__btn--active {
  color: #fff;
  background-color: var(--pink-hot);
  border-color: var(--pink-hot);
}

</style>
