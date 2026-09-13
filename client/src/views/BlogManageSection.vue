<script setup lang="ts">
import { computed, onMounted, ref, watch } from 'vue'
import { useAuthStore } from '@/stores/auth'
import '@/assets/button/manage.css'
import '@/assets/manage/font.css'
import '@/assets/manage/table.css'

interface BlogRow {
  id: number
  title: string
  categories: string[]
  file_path: string | null
  tags: string[]
  update_time: string
}

/** 编辑模式下的行草稿：可编辑博客路径、标题、分类、标签 */
interface BlogDraft {
  id: number
  old_file_path: string
  file_path_category: string
  file_path_name: string
  title: string
  categories: string
  tags: string
}

/** 表格行：非编辑态显示博客数据，编辑态绑定额外草稿 */
interface PageRow {
  blog: BlogRow | null
  draft: BlogDraft | null
}

const auth = useAuthStore()

const blogs = ref<BlogRow[]>([])
const loading = ref(false)
const error = ref('')

const editing = ref(false)
const saving = ref(false)
const canEdit = ref(false)
const canDownload = ref(false)
const drafts = ref<BlogDraft[]>([])

/** 分页：每页固定 15 条（与用户管理页保持一致） */
const PAGE_SIZE = 15
const page = ref(1)

const pageCount = computed(() =>
  Math.max(1, Math.ceil(blogs.value.length / PAGE_SIZE))
)

const pageNumbers = computed(() =>
  Array.from({ length: pageCount.value }, (_, i) => i + 1)
)

/** 当前页数据：不足 15 条时空行补齐，并带上对应草稿 */
const pageRows = computed<PageRow[]>(() => {
  const start = (page.value - 1) * PAGE_SIZE
  return Array.from({ length: PAGE_SIZE }, (_, i) => {
    const blog = blogs.value[start + i] ?? null
    return { blog, draft: drafts.value[start + i] ?? null }
  })
})

watch([blogs, pageCount], () => {
  if (page.value > pageCount.value) {
    page.value = pageCount.value
  }
})

async function loadBlogs() {
  loading.value = true
  error.value = ''
  try {
    const resp = await fetch('/api/blogs', {
      headers: { 'Authorization': 'Bearer ' + auth.token }
    })
    if (!resp.ok) {
      const data = await resp.json().catch(() => ({}))
      error.value = data.error ?? '加载失败'
      return
    }
    const data = await resp.json() as BlogRow[]
    // 博客条目按“关联分类 → 标题”排序，分类/标题为空时排在最后
    blogs.value = data.sort((a, b) => {
      const catA = a.categories.join('、') || '\uffff'
      const catB = b.categories.join('、') || '\uffff'
      const catDiff = catA.localeCompare(catB, 'zh')
      if (catDiff !== 0) return catDiff
      const titleA = a.title || '\uffff'
      const titleB = b.title || '\uffff'
      return titleA.localeCompare(titleB, 'zh')
    })
  } catch {
    error.value = '加载失败'
  } finally {
    loading.value = false
  }
}

onMounted(async () => {
  await loadBlogs()
  // 编辑保存需要 manage:edit 权限，数据下载需要 manage:download 权限
  if (auth.isLoggedIn && auth.token) {
    try {
      const resp = await fetch('/api/user/permissions', {
        headers: { 'Authorization': 'Bearer ' + auth.token }
      })
      if (resp.ok) {
        const data = await resp.json()
        const permissions = data.permissions || []
        canEdit.value = permissions.includes('manage:edit')
        canDownload.value = permissions.includes('manage:download')
      }
    } catch { /* 权限获取失败静默 */ }
  }
})

/** 下载博客数据 zip */
async function downloadData() {
  if (!canDownload.value) {
    window.alert('操作失败：该操作需要 manage:download 权限')
    return
  }
  try {
    const resp = await fetch('/api/manage/download?scope=blogs', {
      headers: { 'Authorization': 'Bearer ' + auth.token }
    })
    if (!resp.ok) {
      const data = await resp.json().catch(() => ({}))
      window.alert(data.error ?? '下载失败')
      return
    }
    const blob = await resp.blob()
    const url = URL.createObjectURL(blob)
    const link = document.createElement('a')
    link.href = url
    link.download = 'data-blogs.zip'
    document.body.appendChild(link)
    link.click()
    link.remove()
    URL.revokeObjectURL(url)
  } catch {
    window.alert('下载失败')
  }
}

/** 进入编辑模式：为所有博客生成草稿。 */
function startEdit() {
  drafts.value = blogs.value.map((blog) => {
    const slash = blog.file_path ? blog.file_path.lastIndexOf('/') : -1
    return {
      id: blog.id,
      old_file_path: blog.file_path ?? '',
      file_path_category: slash >= 0 ? blog.file_path!.slice(0, slash) : '',
      file_path_name: slash >= 0 ? blog.file_path!.slice(slash + 1) : (blog.file_path ?? ''),
      title: blog.title,
      categories: blog.categories.join(', '),
      tags: blog.tags.join(', '),
    }
  })
  editing.value = true
}

/** 取消编辑：丢弃草稿。 */
function cancelEdit() {
  editing.value = false
  drafts.value = []
}

/** 逗号分隔的文本 → 非空条目数组（分类与标签共用）。 */
function listToArray(text: string): string[] {
  return text.split(',').map(s => s.trim()).filter(Boolean)
}

/** 保存编辑：仅提交有改动的行（与博客编辑页同款校验规则）。 */
async function saveChanges() {
  if (!canEdit.value) {
    window.alert('操作失败：该操作需要 manage:edit 权限')
    return
  }

  const META_RE = /[<>"'\\|*?\/ .!@#$%^&()+=[]{};:'"`,.<>?~\-]/
  for (const draft of drafts.value) {
    if (META_RE.test(draft.title)) {
      window.alert('标题 含有特殊字符')
      return
    }
    for (const category of listToArray(draft.categories)) {
      if (META_RE.test(category)) {
        window.alert('分类 含有特殊字符')
        return
      }
    }
    if (META_RE.test(draft.file_path_category) || META_RE.test(draft.file_path_name)) {
      window.alert('博客路径 含有特殊字符')
      return
    }
    for (const tag of listToArray(draft.tags)) {
      if (META_RE.test(tag)) {
        window.alert('标签 含有特殊字符')
        return
      }
    }
  }

  // 保存前先校验：编辑后的博客路径不允许重复，避免“先改第一个、其余报错”
  const pathSeen = new Set<string>()
  for (const draft of drafts.value) {
    const targetPath = `${draft.file_path_category}/${draft.file_path_name}`
    if (pathSeen.has(targetPath)) {
      window.alert(`存在重复的博客路径：${targetPath}`)
      return
    }
    pathSeen.add(targetPath)
  }

  saving.value = true
  try {
    for (const draft of drafts.value) {
      const original = blogs.value.find((blog) => blog.id === draft.id)
      if (!original) continue

      const categoryList = listToArray(draft.categories)
      const tagList = listToArray(draft.tags)
      const newFilePath = `${draft.file_path_category}/${draft.file_path_name}`
      const changed =
        draft.title !== original.title ||
        categoryList.join(',') !== original.categories.join(',') ||
        tagList.join(',') !== original.tags.join(',') ||
        newFilePath !== (original.file_path ?? '')
      if (!changed) continue

      // 更新接口需要 description 与 content，先从详情接口取回
      const getResp = await fetch('/api/blog?file_path=' + encodeURIComponent(draft.old_file_path))
      const existing = await getResp.json().catch(() => ({}))
      if (!getResp.ok) {
        window.alert(existing.error ?? '获取博客内容失败')
        return
      }

      const resp = await fetch('/api/blog/update', {
        method: 'PUT',
        headers: {
          'Content-Type': 'application/json',
          'Authorization': 'Bearer ' + auth.token,
        },
        body: JSON.stringify({
          title: draft.title,
          description: existing.description ?? '',
          categories: categoryList,
          tags: tagList,
          update_time: existing.update_time ?? '',
          file_path_category: draft.file_path_category,
          file_path_name: draft.file_path_name,
          old_file_path: draft.old_file_path,
          content: existing.content ?? '',
          from_manage: true,
        }),
      })
      const data = await resp.json().catch(() => ({}))
      if (!resp.ok) {
        window.alert(data.error ?? '保存失败')
        return
      }
    }

    editing.value = false
    drafts.value = []
    await loadBlogs()
    window.alert('保存成功')
  } catch {
    window.alert('保存失败')
  } finally {
    saving.value = false
  }
}
</script>

<template>
  <div class="blogs-section">
    <div class="blogs-header">
      <h1 class="admin-content__title">博客管理</h1>
      <div class="blogs-toolbar">
        <button
          type="button"
          class="manage-btn manage-btn--primary"
          :disabled="saving"
          @click="downloadData"
        >
          导出数据
        </button>
        <template v-if="editing">
          <button
            type="button"
            class="manage-btn manage-btn--primary"
            :disabled="saving"
            @click="saveChanges"
          >
            {{ saving ? '保存中...' : '保存编辑' }}
          </button>
          <button
            type="button"
            class="manage-btn manage-btn--primary"
            :disabled="saving"
            @click="cancelEdit"
          >
            取消编辑
          </button>
        </template>
        <template v-else>
          <RouterLink
            to="/blog-edit/new"
            target="_blank"
            rel="noopener"
            class="manage-btn manage-btn--primary blogs-toolbar__link"
          >
            新建博客
          </RouterLink>
          <button
            type="button"
            class="manage-btn manage-btn--primary"
            @click="startEdit"
          >
            编辑博客
          </button>
        </template>
      </div>
    </div>

    <div v-if="loading" class="page-loading">加载中...</div>
    <p v-else-if="error" class="blogs-hint">{{ error }}</p>

    <template v-else-if="blogs.length">
      <div class="blogs-table-wrap">
        <table class="blogs-table">
          <thead>
            <tr>
              <th class="blogs-table__title">标题</th>
              <th class="blogs-table__category">关联分类</th>
              <th class="blogs-table__tags">关联标签</th>
              <th class="blogs-table__file-path">博客路径</th>
              <th class="blogs-table__content">博客详情</th>
              <th class="blogs-table__update-time">更新时间</th>
            </tr>
          </thead>
          <tbody>
            <tr v-for="(row, index) in pageRows" :key="row.blog?.id ?? index">
              <td class="blogs-table__title">
                <input
                  v-if="editing && row.draft"
                  v-model="row.draft.title"
                  class="blogs-table__input"
                />
                <span v-else-if="row.blog?.title" :title="row.blog.title">
                  {{ row.blog.title }}
                </span>
                <span v-else-if="row.blog" class="blogs-hint">无</span>
              </td>
              <td class="blogs-table__category">
                <input
                  v-if="editing && row.draft"
                  v-model="row.draft.categories"
                  class="blogs-table__input"
                  placeholder="用英文逗号分隔"
                />
                <template v-else-if="row.blog">
                  <div class="blogs-table__scroll">
                    <span v-if="row.blog.categories.length">{{ row.blog.categories.join('、') }}</span>
                    <span v-else class="blogs-hint">无</span>
                  </div>
                </template>
              </td>
              <td class="blogs-table__tags">
                <input
                  v-if="editing && row.draft"
                  v-model="row.draft.tags"
                  class="blogs-table__input"
                  placeholder="用英文逗号分隔"
                />
                <template v-else-if="row.blog">
                  <div class="blogs-table__scroll">
                    <span v-if="row.blog.tags.length">{{ row.blog.tags.join('、') }}</span>
                    <span v-else class="blogs-hint">无</span>
                  </div>
                </template>
              </td>
              <td class="blogs-table__file-path">
                <div v-if="editing && row.draft" class="blogs-table__path-row">
                  <input
                    v-model="row.draft.file_path_category"
                    class="blogs-table__input"
                    placeholder="分类目录"
                  />
                  <span class="blogs-table__path-sep">/</span>
                  <input
                    v-model="row.draft.file_path_name"
                    class="blogs-table__input"
                    placeholder="文件名"
                  />
                </div>
                <span v-else-if="row.blog?.file_path" :title="row.blog.file_path">
                  {{ row.blog.file_path }}
                </span>
                <span v-else-if="row.blog" class="blogs-hint">无</span>
              </td>
              <td class="blogs-table__content">
                <div v-if="row.blog?.file_path" class="blogs-table__content-view">
                  <RouterLink
                    :to="{ name: 'blog-detail', params: { file_path: row.blog.file_path } }"
                    target="_blank"
                    rel="noopener"
                    class="manage-btn manage-btn--primary blogs-table__link"
                  >
                    博客详情
                  </RouterLink>
                  <RouterLink
                    :to="{ name: 'blog-edit', params: { file_path: row.blog.file_path } }"
                    target="_blank"
                    rel="noopener"
                    class="manage-btn manage-btn--primary blogs-table__link"
                  >
                    内容编辑
                  </RouterLink>
                </div>
                <span v-else-if="row.blog" class="blogs-hint">无</span>
              </td>
              <td class="blogs-table__update-time">
                <span v-if="row.blog?.update_time">{{ row.blog.update_time }}</span>
                <span v-else-if="row.blog" class="blogs-hint">无</span>
              </td>
            </tr>
          </tbody>
        </table>
      </div>

      <div class="users-pager">
        <div class="users-pager__pages">
          <button
            type="button"
            class="users-pager__btn"
            :disabled="page <= 1"
            @click="page -= 1"
          >
            上一页
          </button>
          <button
            v-for="num in pageNumbers"
            :key="num"
            type="button"
            class="users-pager__btn"
            :class="{ 'users-pager__btn--active': num === page }"
            @click="page = num"
          >
            {{ num }}
          </button>
          <button
            type="button"
            class="users-pager__btn"
            :disabled="page >= pageCount"
            @click="page += 1"
          >
            下一页
          </button>
        </div>
      </div>
    </template>

    <p v-else class="blogs-hint">暂无博客</p>
  </div>
</template>

<style scoped>
.blogs-hint {
  margin: 0;
  font-size: 0.9rem;
  color: var(--color-text-secondary);
}

.blogs-section {
  display: flex;
  flex-direction: column;
  /* 视口 − 导航栏 − .admin-content 上下内边距（32px × 2） */
  min-height: calc(var(--page-height) - 64px);
}

.blogs-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  margin-bottom: 16px;
}

.blogs-toolbar {
  display: flex;
  gap: 8px;
}

.blogs-toolbar__link {
  text-decoration: none;
}

.admin-content__title {
  margin: 0;
  font-size: 1.5rem;
  font-weight: 700;
  color: var(--color-text);
}

.page-loading {
  display: flex;
  flex: 1;
  align-items: center;
  justify-content: center;
  margin: 0;
  font-size: 0.95rem;
  color: var(--color-text-secondary);
}

/* ── 博客内容 / 编辑博客 列内按钮 ── */

.blogs-table__content-view {
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 12px;
}

.blogs-table__link {
  min-width: 0;
  padding: 2px 10px;
  font-size: 0.85rem;
}

/* ── 编辑模式：路径拆分输入 ── */

.blogs-table__path-row {
  display: flex;
  align-items: center;
  gap: 6px;
}

.blogs-table__path-sep {
  color: var(--color-text-secondary);
}

/* ── 关联分类 / 标签：内容过长时可在单元格内横向滚动（隐藏滚动条） ── */

.blogs-table__scroll {
  max-width: 100%;
  overflow-x: auto;
  white-space: nowrap;
  /* Firefox */
  scrollbar-width: none;
  /* 旧版 Edge / IE */
  -ms-overflow-style: none;
}

/* Chrome / Edge / Safari */
.blogs-table__scroll::-webkit-scrollbar {
  display: none;
}
</style>
