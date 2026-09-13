<script setup lang="ts">
import { ref, watch, computed, onMounted, onBeforeUnmount, nextTick, type Ref } from 'vue'
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

let searchTimer: ReturnType<typeof setTimeout> | null = null

// ── 计算：当前可见的标签（标签与分类相互独立，直接展示全部标签） ──

const visibleTags = computed<Tag[]>(() => tags.value)

/** 按名称获取标签 ID（标签名全局唯一） */
function tagIdsByName(name: string): number[] {
  return tags.value.filter(t => t.name === name).map(t => t.id)
}

// ── 筛选器折叠：分类 / 标签最多显示三行，超出则在三行末尾显示「更多...」 ──

/** 折叠状态下筛选器最多显示的行数 */
const MAX_FILTER_LINES = 3

/** 分类 / 标签筛选是否已展开（仅本次页面停留内有效，刷新或重新进入页面后复位） */
const categoriesExpanded = ref(false)
const tagsExpanded = ref(false)

/** 折叠状态下显示的条目数（按容器宽度实测得出） */
const categoriesVisibleCount = ref(Number.POSITIVE_INFINITY)
const tagsVisibleCount = ref(Number.POSITIVE_INFINITY)

/** 条目是否超过三行（超过才需要「更多...」） */
const categoriesOverflow = ref(false)
const tagsOverflow = ref(false)

const categoryChipsEl = ref<HTMLElement | null>(null)
const tagChipsEl = ref<HTMLElement | null>(null)

/**
 * 按 flex 换行规则估算条目排布所需行数。
 * @param widths   按顺序排列的条目宽度（px）。
 * @param gap      条目之间的水平间距（px）。
 * @param maxWidth 容器可用宽度（px）。
 * @returns 排布所需行数。
 */
function countLines(widths: number[], gap: number, maxWidth: number): number {
  let lines = 0
  let lineWidth = 0
  for (const width of widths) {
    if (lineWidth === 0) {
      lineWidth = width
    } else if (lineWidth + gap + width <= maxWidth) {
      lineWidth += gap + width
    } else {
      lines += 1
      lineWidth = width
    }
  }
  return lineWidth > 0 ? lines + 1 : lines
}

/** 每个容器最近一次测量依据（容器宽度 + 条目数），避免重复测量与观察器抖动 */
const measuredKey = new WeakMap<HTMLElement, string>()

/**
 * 测量筛选器容器，得出折叠状态下可显示的条目数。
 *
 * 被折叠的条目依旧留在 DOM 中（绝对定位 + visibility: hidden），
 * 因此无论是否展开、容器宽度如何变化，都能取到真实宽度重新计算。
 *
 * @param containerRef 筛选器条目容器。
 * @param visibleCount 折叠时显示的条目数（输出）。
 * @param overflow     是否超过三行（输出）。
 */
async function measureChips(
  containerRef: Ref<HTMLElement | null>,
  visibleCount: Ref<number>,
  overflow: Ref<boolean>
) {
  await nextTick()
  const container = containerRef.value
  const more = container?.querySelector<HTMLElement>('.filter-more')
  if (!container || !more) return

  const style = getComputedStyle(container)
  const gap = Number.parseFloat(style.columnGap) || 0
  const maxWidth = container.clientWidth
    - (Number.parseFloat(style.paddingLeft) || 0)
    - (Number.parseFloat(style.paddingRight) || 0)
  if (maxWidth <= 0) return

  const widths = Array.from(container.querySelectorAll<HTMLElement>('.filter-chip'))
    .map(chip => chip.offsetWidth)

  const key = `${maxWidth}:${widths.length}`
  if (measuredKey.get(container) === key) return
  measuredKey.set(container, key)

  // 三行内放得下全部条目：无需「......」
  if (countLines(widths, gap, maxWidth) <= MAX_FILTER_LINES) {
    visibleCount.value = widths.length
    overflow.value = false
    return
  }

  // 需要「......」：从后往前取能连按钮一起放进三行的最大条目数
  let count = 0
  for (let i = widths.length; i >= 0; i--) {
    if (countLines([...widths.slice(0, i), more.offsetWidth], gap, maxWidth)
        <= MAX_FILTER_LINES) {
      count = i
      break
    }
  }
  visibleCount.value = count
  overflow.value = true
}

/** 重新测量分类与标签筛选器（条目或容器宽度变化后调用）。 */
function measureFilters() {
  measureChips(categoryChipsEl, categoriesVisibleCount, categoriesOverflow)
  measureChips(tagChipsEl, tagsVisibleCount, tagsOverflow)
}

/** 容器宽度变化（窗口缩放、字体加载完成等）时重新测量 */
let chipsObserver: ResizeObserver | null = null

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

function onSearchInput() {
  if (searchTimer) clearTimeout(searchTimer)
  searchTimer = setTimeout(() => {
    page.value = 1
    fetchBlogs()
    syncUrl()
  }, 300)
}

// ── 生命周期 ──

// 分类 / 标签条目变化后重新测量折叠行数
watch(categories, measureFilters, { flush: 'post' })
watch(visibleTags, measureFilters, { flush: 'post' })

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

  // 测量筛选器：超过三行的分类 / 标签折叠，并在三行末尾显示「更多...」
  await measureFilters()
  chipsObserver = new ResizeObserver(() => measureFilters())
  if (categoryChipsEl.value) chipsObserver.observe(categoryChipsEl.value)
  if (tagChipsEl.value) chipsObserver.observe(tagChipsEl.value)
})

onBeforeUnmount(() => {
  chipsObserver?.disconnect()
  chipsObserver = null
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
        <div ref="categoryChipsEl" class="filter-chips">
          <button
            v-for="(cat, index) in categories"
            :key="cat.id"
            class="tag-normal filter-chip"
            :class="{
              'tag--active': selectedCategoryIds.includes(cat.id),
              'filter-chip--hidden': !categoriesExpanded && index >= categoriesVisibleCount,
            }"
            @click="toggleCategory(cat.id)"
          >
            {{ cat.name }}
          </button>
          <button
            class="filter-more tag-pink"
            :class="{ 'filter-chip--hidden': categoriesExpanded || !categoriesOverflow }"
            :aria-expanded="categoriesExpanded"
            @click="categoriesExpanded = true"
          >
            更多...
          </button>
        </div>
      </div>

      <!-- 标签筛选 -->
      <div class="filter-row">
        <span class="filter-label">标签</span>
        <button class="filter-mode-btn tag-pink" @click="tagMulti = !tagMulti">{{ tagMulti ? '多选' : '单选' }}</button>
        <div ref="tagChipsEl" class="filter-chips">
          <button
            v-for="(tag, index) in visibleTags"
            :key="tag.name"
            class="tag-normal filter-chip"
            :class="{
              'tag--active': tagIdsByName(tag.name).some(id => selectedTagIds.includes(id)),
              'filter-chip--hidden': !tagsExpanded && index >= tagsVisibleCount,
            }"
            @click="toggleTag(tag.name)"
          >
            {{ tag.name }}
          </button>
          <button
            class="filter-more tag-pink"
            :class="{ 'filter-chip--hidden': tagsExpanded || !tagsOverflow }"
            :aria-expanded="tagsExpanded"
            @click="tagsExpanded = true"
          >
            ......
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
        v-for="blog in blogs"
        :key="blog.id"
        class="blog-card"
        :to="`/blogs/${(blog.file_path ?? '').replace(/^\/+/, '')}`"
      >
        <h3 class="blog-card__title">{{ blog.title }}</h3>
        <p class="blog-card__desc">{{ blog.description }}</p>
        <div class="blog-card__meta blog-tags">
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
