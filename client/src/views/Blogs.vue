<script setup lang="ts">
import { ref, watch, computed, onMounted } from 'vue'
import { useRouter, useRoute, onBeforeRouteUpdate } from 'vue-router'

import '@/assets/blog.css'

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
  searchQuery.value   = (to.query.q as string) || ''
  categoryMulti.value = to.query.cm === '1'
  tagMulti.value      = to.query.tm === '1'

  selectedCategoryIds.value = urlCatNames
    .map(n => categories.value.find(c => c.name === n)?.id)
    .filter(Boolean) as number[]
  selectedTagIds.value = urlTagNames.flatMap(n => tagIdsByName(n))

  await fetchBlogs(true)
})

// ── URL 同步 ──

function syncUrl() {
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
  fetchBlogs()
}

function onSearchInput() {
  if (searchTimer) clearTimeout(searchTimer)
  searchTimer = setTimeout(fetchBlogs, 300)
}

// ── 生命周期 ──

onMounted(async () => {
  // 从 URL 读取 name 参数（等数据加载后转 ID）
  const urlCatNames = parseNames(route.query.categories as string | undefined)
  const urlTagNames = parseNames(route.query.tags as string | undefined)
  searchQuery.value   = (route.query.q as string) || ''
  categoryMulti.value = route.query.cm === '1'
  tagMulti.value      = route.query.tm === '1'

  initializing = true
  await Promise.all([fetchCategories(), fetchTags()])

  // name → ID 转换
  selectedCategoryIds.value = urlCatNames
    .map(n => categories.value.find(c => c.name === n)?.id).filter(Boolean) as number[]
  selectedTagIds.value = urlTagNames.flatMap(n => tagIdsByName(n))

  await fetchBlogs()
  initializing = false
})
</script>

<template>
  <main class="blogs-page">
    <div class="blogs-top">
      <button class="new-blog-btn" @click="router.push('/blog-edit/new')">新建博客</button>
    </div>
    <!-- ── 筛选器 ── -->
    <section class="filter-bar">
      <!-- 分类筛选 -->
      <div class="filter-row">
        <span class="filter-label">分类</span>
        <button class="filter-mode-btn" @click="categoryMulti = !categoryMulti">{{ categoryMulti ? '多选' : '单选' }}</button>
        <div class="filter-chips">
          <button
            v-for="cat in categories"
            :key="cat.id"
            class="chip"
            :class="{ 'chip--active': selectedCategoryIds.includes(cat.id) }"
            @click="toggleCategory(cat.id)"
          >
            {{ cat.name }}
          </button>
        </div>
      </div>

      <!-- 标签筛选 -->
      <div class="filter-row">
        <span class="filter-label">标签</span>
        <button class="filter-mode-btn" @click="tagMulti = !tagMulti">{{ tagMulti ? '多选' : '单选' }}</button>
        <div class="filter-chips">
          <button
            v-for="tag in visibleTags"
            :key="tag.name"
            class="chip"
            :class="{ 'chip--active': tagIdsByName(tag.name).some(id => selectedTagIds.includes(id)) }"
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
        v-for="blog in blogs"
        :key="blog.id"
        class="blog-card"
        :to="`/blogs/${(blog.file_path ?? '').replace(/^\/+/, '')}`"
      >
        <h3 class="blog-card__title">{{ blog.title }}</h3>
        <p class="blog-card__desc">{{ blog.description }}</p>
        <div class="blog-card__meta blog-tags">
          <span v-if="blog.category" class="blog-category">{{ blog.category }}</span>
          <span
            v-for="tag in blog.tags"
            :key="tag"
            class="blog-tag"
          >{{ tag }}</span>
        </div>
        <time class="blog-card__time">{{ blog.update_time }}</time>
      </RouterLink>
    </section>
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

.new-blog-btn {
  padding: 10px 20px;
  font-size: 0.9rem;
  color: #fff;
  background-color: #FF77CC;
  border: none;
  border-radius: 4px;
  cursor: pointer;
  white-space: nowrap;
  transition: opacity var(--transition-speed);
}

/* ── 筛选器 ── */
.filter-bar {
  background-color: var(--color-card-bg, var(--color-nav-bg));
  border: 1px solid var(--color-border);
  border-radius: 8px;
  padding: 20px 24px;
  margin-bottom: 28px;
}
.new-blog-btn:hover {
  opacity: 0.85;
}

.filter-row {
  display: flex;
  align-items: flex-start;
  gap: 12px;
  margin-bottom: 16px;
}

.filter-label {
  flex-shrink: 0;
  width: 48px;
  padding-top: 6px;
  font-size: 1.1rem;
  font-weight: 600;
  color: var(--color-text);
  text-align: right;
  transform: translateY(-7px);
}

.filter-chips {
  flex: 1;
  display: flex;
  flex-wrap: wrap;
  gap: 8px;
}

.chip {
  padding: 4px 14px;
  font-size: 0.85rem;
  color: var(--color-text-secondary);
  background: transparent;
  border: 1px solid var(--color-border);
  border-radius: 14px;
  cursor: pointer;
  transition: all var(--transition-speed);
}

.chip:hover {
  border-color: var(--color-text-secondary);
  color: var(--color-text);
}

.chip--active {
  color: #fff;
  background-color: var(--color-text);
  border-color: var(--color-text);
}

.filter-mode-btn {
  flex-shrink: 0;
  margin-top: 2px;
  padding: 2px 10px;
  font-size: 0.75rem;
  color: var(--color-text-secondary);
  background: transparent;
  border: 1px solid var(--color-border);
  border-radius: 4px;
  cursor: pointer;
  transition: all var(--transition-speed);
}

.filter-mode-btn:hover {
  color: var(--color-text);
  border-color: var(--color-text-secondary);
}

.filter-search {
  margin-top: 4px;
}

.search-input {
  width: 100%;
  box-sizing: border-box;
  padding: 8px 14px;
  font-size: 0.9rem;
  color: var(--color-text);
  background: transparent;
  border: 1px solid var(--color-border);
  border-radius: 4px;
  outline: none;
  transition: border-color var(--transition-speed);
}

.search-input::placeholder {
  color: var(--color-text-secondary);
  opacity: 0.5;
}

.search-input:focus {
  border-color: var(--color-text-secondary);
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


</style>
