<script setup lang="ts">
import { ref, computed, onMounted } from 'vue'
import { useRouter, useRoute, onBeforeRouteUpdate } from 'vue-router'

import FilterBar, { type FilterGroup } from '@/components/FilterBar.vue'

import '@/assets/normal/tag.css'
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
}

interface BlogItem {
  id: number
  title: string
  description: string | null
  update_time: string
  categories: string[]
  file_path: string | null
  tags: string[]
}

// ── 数据 ──

const categories = ref<Category[]>([])
const tags = ref<Tag[]>([])
/** 当前页的博客条目（由服务端按需查询，每页 PAGE_SIZE 条） */
const blogs = ref<BlogItem[]>([])

// ── 分页 ──

/** 每页显示的博客条目数（构建期由 conf/page_size.yml 注入，与服务端一致） */
const PAGE_SIZE = __BLOG_PAGE_SIZE__

const page = ref(1)

/** 符合条件的博客总数（服务端返回，用于计算总页数） */
const total = ref(0)

/** 总页数：至少 1 页，空结果时不出现 0 页 */
const pageCount = computed(() =>
  Math.max(1, Math.ceil(total.value / PAGE_SIZE))
)

const pageNumbers = computed(() =>
  Array.from({ length: pageCount.value }, (_, i) => i + 1)
)

/** 读取 URL 页码：非正整数（空值、字母、0、负数）一律按第 1 页处理 */
function readPage(raw: unknown): number {
  const num = Number(raw)
  return Number.isInteger(num) && num >= 1 ? num : 1
}

/**
 * 按需加载当前页。
 * 页码超出总页数（非法页）时回到第 1 页重新加载，
 * 并把 URL 归一化（去掉无效的 ?page=）。
 */
async function loadCurrentPage() {
  await fetchBlogs()
  if (page.value > pageCount.value) {
    page.value = 1
    await fetchBlogs()
    syncUrl()
  }
}

/** 判断两个 ID 集合是否相同（与顺序无关） */
function sameIds(a: number[], b: number[]): boolean {
  const key = (list: number[]) => [...list].sort((x, y) => x - y).join()
  return key(a) === key(b)
}

/** 翻页：越界或与当前页相同时忽略，按需查询该页、写入 URL 并滚回列表顶部 */
function goToPage(num: number) {
  if (num < 1 || num > pageCount.value || num === page.value) return
  page.value = num
  fetchBlogs()
  // 点击翻页按钮时页码始终写入 URL，第 1 页也保留 ?page=1
  syncUrl(true)
  window.scrollTo({ top: 0, behavior: 'smooth' })
}

// ── 筛选状态 ──

const selectedCategoryIds = ref<number[]>([])
const categoryMulti = ref(false)

const selectedTagIds = ref<number[]>([])
const tagMulti = ref(false)

const searchQuery = ref('')

// ── 计算：当前可见的标签（标签与分类相互独立，直接展示全部标签） ──

const visibleTags = computed<Tag[]>(() => tags.value)

/** 按名称获取标签 ID（标签名全局唯一） */
function tagIdsByName(name: string): number[] {
  return tags.value.filter(t => t.name === name).map(t => t.id)
}

// ── 公共筛选器（FilterBar）：分类 / 标签两行维度 ──

/** 筛选器维度的唯一键，与 FilterBar 回调中的 groupKey 一一对应 */
const FILTER_GROUP_CATEGORIES = 'categories'
const FILTER_GROUP_TAGS = 'tags'

/** 拼装公共筛选器所需的维度数据（选中态与单选 / 多选均由本页维护） */
const filterGroups = computed<FilterGroup[]>(() => [
  {
    key: FILTER_GROUP_CATEGORIES,
    label: '分类',
    multi: categoryMulti.value,
    options: categories.value.map(cat => ({
      key: String(cat.id),
      label: cat.name,
      selected: selectedCategoryIds.value.includes(cat.id),
    })),
  },
  {
    key: FILTER_GROUP_TAGS,
    label: '标签',
    multi: tagMulti.value,
    options: visibleTags.value.map(tag => ({
      key: tag.name,
      label: tag.name,
      selected: tagIdsByName(tag.name).some(id => selectedTagIds.value.includes(id)),
    })),
  },
])

/** 筛选器点击某个筛选项：按维度分发到分类 / 标签的切换逻辑 */
function onFilterToggle(groupKey: string, optionKey: string) {
  if (groupKey === FILTER_GROUP_CATEGORIES) {
    toggleCategory(Number(optionKey))
    return
  }
  if (groupKey === FILTER_GROUP_TAGS) {
    toggleTag(optionKey)
  }
}

/** 筛选器切换单选 / 多选 */
function onFilterMulti(groupKey: string, multi: boolean) {
  if (groupKey === FILTER_GROUP_CATEGORIES) {
    categoryMulti.value = multi
  } else if (groupKey === FILTER_GROUP_TAGS) {
    tagMulti.value = multi
  }
}

/** 搜索框内容变化（已防抖）：回到第 1 页重新查询并同步 URL */
function onSearch(value: string) {
  searchQuery.value = value
  page.value = 1
  fetchBlogs()
  syncUrl()
}

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

/** 按当前筛选条件与页码请求服务端（服务端按需查询该页）。 */
async function fetchBlogs() {
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
  params.set('page', String(page.value))
  params.set('page_size', String(PAGE_SIZE))

  loading.value = true
  try {
    const resp = await fetch('/api/blogs?' + params.toString())
    if (!resp.ok) throw new Error(`HTTP ${resp.status}`)
    const data = await resp.json() as { items: BlogItem[]; total: number }
    blogs.value = data.items
    total.value = data.total
  } catch (e) {
    console.error('获取博客失败:', e)
    blogs.value = []
    total.value = 0
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

  // 与当前状态一致说明只是 URL 写法变化，无需重新查询
  const filtersSame =
    nextSearch === searchQuery.value &&
    (to.query.cm === '1') === categoryMulti.value &&
    (to.query.tm === '1') === tagMulti.value &&
    sameIds(nextCategoryIds, selectedCategoryIds.value) &&
    sameIds(nextTagIds, selectedTagIds.value)

  const loadedPage = page.value
  const requested  = readPage(to.query.page)

  searchQuery.value   = nextSearch
  categoryMulti.value = to.query.cm === '1'
  tagMulti.value      = to.query.tm === '1'
  selectedCategoryIds.value = nextCategoryIds
  selectedTagIds.value = nextTagIds
  page.value = requested

  // 筛选变化或页码变化（前进后退）时按需查询该页
  if (!filtersSame || requested !== loadedPage) {
    await loadCurrentPage()
  }
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
  syncUrl()
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
  syncUrl()
}

// ── 生命周期 ──

onMounted(async () => {
  // 从 URL 读取 name 参数（等数据加载后转 ID）
  const urlCatNames = parseNames(route.query.categories as string | undefined)
  const urlTagNames = parseNames(route.query.tags as string | undefined)
  searchQuery.value   = (route.query.q as string) || ''
  categoryMulti.value = route.query.cm === '1'
  tagMulti.value      = route.query.tm === '1'
  // 先按 URL 设定页码，随后按需查询该页
  page.value          = readPage(route.query.page)

  await Promise.all([fetchCategories(), fetchTags()])

  // name → ID 转换
  selectedCategoryIds.value = urlCatNames
    .map(n => categories.value.find(c => c.name === n)?.id).filter(Boolean) as number[]
  selectedTagIds.value = urlTagNames.flatMap(n => tagIdsByName(n))

  await loadCurrentPage()
})
</script>

<template>
  <main class="blogs-page">
    <div class="blogs-top">
      <button class="func-btn" @click="router.push('/blog-edit/new')">新建博客</button>
    </div>
    <!-- ── 筛选器（公共组件） ── -->
    <FilterBar
      :groups="filterGroups"
      :search="searchQuery"
      search-placeholder="搜索标题、描述、分类和标签中的内容..."
      @toggle="onFilterToggle"
      @update:multi="onFilterMulti"
      @update:search="onSearch"
    />

    <!-- ── 博客卡片网格 ── -->
    <p v-if="loading" class="blog-status">加载中...</p>
    <p v-else-if="!blogs.length" class="blog-status">未检索到对应博客</p>
    <section v-else class="blog-grid">
      <RouterLink
        v-for="blog in blogs"
        :key="blog.id"
        class="blog-card"
        :to="`/blogs/${(blog.file_path ?? '').replace(/^\/+/, '')}`"
      >
        <h3 class="blog-card__title">{{ blog.title }}</h3>
        <p class="blog-card__desc">{{ blog.description }}</p>
        <div class="blog-card__meta tag-list">
          <span
            v-for="category in blog.categories"
            :key="category"
            class="tag-pink"
          >{{ category }}</span>
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
