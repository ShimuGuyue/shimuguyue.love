<script setup lang="ts">
import { computed, onMounted, ref } from 'vue'
import { useAuthStore } from '@/stores/auth'
import '@/assets/manage/button.css'
import '@/assets/manage/font.css'
import '@/assets/manage/table.css'

interface BlogRow {
  id: number
  title: string
  category: string | null
  file_path: string | null
  tags: string[]
}

const auth = useAuthStore()

const blogs = ref<BlogRow[]>([])
const loading = ref(false)
const error = ref('')
const tagDialogBlog = ref<BlogRow | null>(null)

/** 分页：每页固定 15 条（与用户管理页保持一致） */
const PAGE_SIZE = 15
const page = ref(1)

const pageCount = computed(() =>
  Math.max(1, Math.ceil(blogs.value.length / PAGE_SIZE))
)

const pageNumbers = computed(() =>
  Array.from({ length: pageCount.value }, (_, i) => i + 1)
)

/** 当前页数据 */
const pageRows = computed(() => {
  const start = (page.value - 1) * PAGE_SIZE
  return blogs.value.slice(start, start + PAGE_SIZE)
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
    blogs.value = await resp.json()
  } catch {
    error.value = '加载失败'
  } finally {
    loading.value = false
  }
}

/** 打开指定博客的完整标签弹窗。 */
function openTagDialog(blog: BlogRow) {
  tagDialogBlog.value = blog
}

/** 关闭完整标签弹窗。 */
function closeTagDialog() {
  tagDialogBlog.value = null
}

onMounted(loadBlogs)
</script>

<template>
  <div class="blogs-section">
    <div class="blogs-header">
      <h1 class="admin-content__title">博客管理</h1>
    </div>

    <div v-if="loading" class="page-loading">加载中...</div>
    <p v-else-if="error" class="blogs-hint">{{ error }}</p>

    <template v-else-if="blogs.length">
      <div class="blogs-table-wrap">
        <table class="blogs-table">
          <thead>
            <tr>
              <th class="blogs-table__file-path">博客路径</th>
              <th class="blogs-table__title">标题</th>
              <th class="blogs-table__category">所属分类</th>
              <th class="blogs-table__tags">标签列表</th>
              <th class="blogs-table__content">博客详情</th>
            </tr>
          </thead>
          <tbody>
            <tr v-for="blog in pageRows" :key="blog.id">
              <td class="blogs-table__file-path">
                <span v-if="blog.file_path" :title="blog.file_path">
                  {{ blog.file_path }}
                </span>
                <span v-else class="blogs-hint">无</span>
              </td>
              <td class="blogs-table__title">
                <span v-if="blog.title" :title="blog.title">{{ blog.title }}</span>
                <span v-else class="blogs-hint">无</span>
              </td>
              <td class="blogs-table__category">
                <span v-if="blog.category">{{ blog.category }}</span>
                <span v-else class="blogs-hint">无</span>
              </td>
              <td class="blogs-table__tags">
                <div v-if="blog.tags.length" class="users-table__perms-view">
                  <span class="users-table__perms-text" :title="blog.tags.join('、')">
                    {{ blog.tags.join('、') }}
                  </span>
                  <button
                    type="button"
                    class="manage-btn users-table__perms-btn"
                    @click="openTagDialog(blog)"
                  >
                    完整标签
                  </button>
                </div>
                <span v-else class="blogs-hint">无</span>
              </td>
              <td class="blogs-table__content">
                <div v-if="blog.file_path" class="blogs-table__content-view">
                  <RouterLink
                    :to="{ name: 'blog-detail', params: { file_path: blog.file_path } }"
                    class="manage-btn manage-btn--primary blogs-table__link"
                  >
                    博客详情
                  </RouterLink>
                  <RouterLink
                    :to="{ name: 'blog-edit', params: { file_path: blog.file_path } }"
                    class="manage-btn manage-btn--primary blogs-table__link"
                  >
                    内容编辑
                  </RouterLink>
                </div>
                <span v-else class="blogs-hint">无</span>
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

    <div
      v-if="tagDialogBlog"
      class="create-mask"
      @click.self="closeTagDialog"
    >
      <div class="create-box perm-box" role="dialog" aria-modal="true">
        <h2 class="create-box__title">
          标签列表：{{ tagDialogBlog.title || tagDialogBlog.file_path || '博客' }}
        </h2>
        <p v-if="tagDialogBlog.tags.length" class="perm-item blogs-dialog__tags">
          {{ tagDialogBlog.tags.join('、') }}
        </p>
        <p v-else class="blogs-hint">无</p>
      </div>
    </div>
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
  min-height: calc(100vh - 144px);
}

.blogs-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  margin-bottom: 16px;
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
  gap: 6px;
}

.blogs-table__link {
  min-width: 0;
  padding: 2px 10px;
  font-size: 0.85rem;
}

/* ── 完整标签弹窗（与用户管理页弹窗同款） ── */

.create-mask {
  position: fixed;
  inset: 0;
  z-index: 200;
  display: flex;
  align-items: center;
  justify-content: center;
  background-color: rgba(0, 0, 0, 0.35);
}

.create-box {
  width: 380px;
  padding: 24px;
  background-color: var(--color-nav-bg);
  border: 1px solid var(--color-border);
  box-shadow: 0 0.25rem 1rem rgba(0, 0, 0, 0.15);
}

/* 完整标签弹窗：比新建/编辑弹窗略宽 */
.perm-box {
  width: 480px;
  max-width: calc(100vw - 48px);
}

.create-box__title {
  margin: 0 0 20px;
  font-size: 1.2rem;
  color: var(--color-text);
}

.blogs-dialog__tags {
  margin: 0;
  line-height: 1.7;
}
</style>
