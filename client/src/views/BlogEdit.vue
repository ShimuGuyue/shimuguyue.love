<script setup lang="ts">
import { ref, computed, onMounted, onUnmounted } from 'vue'
import { onBeforeRouteLeave, useRoute, useRouter } from 'vue-router'
import { useAuthStore } from '@/stores/auth'
import { useThemeStore } from '@/stores/theme'
import { MdEditor } from 'md-editor-v3'
import 'md-editor-v3/lib/style.css'
import '@/assets/markdown/headings.css'
import '@/assets/markdown/divider.css'
import '@/assets/markdown/text.css'
import '@/assets/markdown/blockquote.css'
import '@/assets/markdown/lists.css'
import '@/assets/markdown/code.css'
import '@/assets/markdown/tables.css'
import '@/assets/markdown/images.css'
import '@/assets/markdown/tasks.css'
import '@/assets/markdown/alerts.css'

import '@/assets/blog-layout.css'
import '@/assets/normal/color.css'
import '@/assets/glass.css'
const auth = useAuthStore()
const theme = useThemeStore()
const route = useRoute()
const router = useRouter()

/// 是否为编辑模式（路由含 file_path 参数）
const isEditing = computed(() => !!route.params.file_path)

const title             = ref('')
const description       = ref('')
const category          = ref('')
const tags              = ref('')
const pathCategory      = ref('')
const pathName          = ref('')
/// 导入文件 frontmatter 中的更新时间（YYYY-MM-DD，未导入则为空）
const updateTime        = ref('')
/// 编辑模式下的原始 file_path（新建时为 null）
const editFilePath      = ref<string | null>(isEditing.value ? String(route.params.file_path) : null)
/// 博客 Markdown 正文
const content           = ref('')
const savedSuccessfully = ref(false)
const permissions       = ref<string[]>([])

/// 判断是否有任何字段非空（有内容就拦截离开）
function hasContent(): boolean {
  if (savedSuccessfully.value) return false
  if (title.value || description.value || category.value || tags.value ||
      pathCategory.value || pathName.value) return true
  if (content.value.trim()) return true
  return false
}

/// 浏览器级拦截：刷新 / 关闭标签页 / 外部导航
function onBeforeUnload(e: BeforeUnloadEvent) {
  if (hasContent()) {
    e.preventDefault()
  }
}

onMounted(async () => {
  window.addEventListener('beforeunload', onBeforeUnload)

  // 新建博客：更新时间默认显示今天（保存时后端也会默认取当天）
  if (!isEditing.value) {
    updateTime.value = todayString()
  }

  // 获取当前用户权限（用于保存按钮的权限拦截）
  if (auth.isLoggedIn && auth.token) {
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

  // 编辑模式：加载已有博客数据
  if (isEditing.value && editFilePath.value) {
    try {
      const resp = await fetch('/api/blog?file_path=' + encodeURIComponent(editFilePath.value))
      if (resp.ok) {
        const data = await resp.json()
        title.value = data.title || ''
        description.value = data.description || ''
        category.value = data.category || ''
        tags.value = Array.isArray(data.tags) ? data.tags.join(', ') : ''
        pathCategory.value = data.file_path?.split('/').slice(0, -1).join('/') || ''
        pathName.value = data.file_path?.split('/').pop() || ''
        updateTime.value = isValidDateString(data.update_time) ? data.update_time : ''
        content.value = data.content || ''
      }
    } catch (e) {
      console.error('加载博客失败:', e)
    }
  }
})

onUnmounted(() => {
  window.removeEventListener('beforeunload', onBeforeUnload)
})

/// Vue Router 组件内导航守卫：SPA 页面跳转
onBeforeRouteLeave((_to, _from, next) => {
  if (!hasContent()) {
    next()
    return
  }
  const discard = window.confirm('丢弃所有更改？\n\n您填写的内容将不会被保存。')
  if (discard) {
    next()
  } else {
    next(false)
  }
})

async function importFile() {
  const input = document.createElement('input')
  input.type = 'file'
  input.accept = '.md,.txt'
  input.onchange = async () => {
    const file = input.files?.[0]
    if (!file) return
    const text = await file.text()
    try {
      const resp = await fetch('/api/blog/parse', { method: 'POST', body: text })
      if (!resp.ok) throw new Error(`${resp.status}`)

      const data = await resp.json()
      title.value = data.title || ''
      description.value = data.description || ''
      category.value = data.category || ''
      tags.value = Array.isArray(data.tags) ? data.tags.join(', ') : (data.tags || '')
      const importedUpdateTime = data.update_time || ''
      updateTime.value = isValidDateString(importedUpdateTime) ? importedUpdateTime : todayString()
      pathCategory.value = data.file_path_category || ''
      pathName.value = data.file_path_name || ''

      content.value = data.content || ''
    } catch (e) {
      // 后端不可用时本地解析
      alert('导入失败: ' + e)
    }
  }
  input.click()
}

/// 校验 YYYY-MM-DD 格式且为真实存在的日期
function isValidDateString(value: string): boolean {
  const m = /^(\d{4})-(\d{2})-(\d{2})$/.exec(value)
  if (!m)
    return false
  const year = Number(m[1])
  const month = Number(m[2])
  const day = Number(m[3])
  if (month < 1 || month > 12 || day < 1)
    return false
  const isLeap = (year % 4 === 0 && year % 100 !== 0) || year % 400 === 0
  const daysInMonth = [31, isLeap ? 29 : 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31]
  const maxDay = daysInMonth[month - 1]
  return maxDay !== undefined && day <= maxDay
}

/// 当前本地日期（YYYY-MM-DD）
function todayString(): string {
  const now = new Date()
  const year = now.getFullYear()
  const month = String(now.getMonth() + 1).padStart(2, '0')
  const day = String(now.getDate()).padStart(2, '0')
  return `${year}-${month}-${day}`
}

async function saveBlog() {
  if (!auth.isLoggedIn) {
    alert('请先登录')
    return
  }

  // 权限拦截：新建页需 blog:create，编辑页需 blog:edit
  if (isEditing.value) {
    if (!permissions.value.includes('blog:edit')) {
      alert('操作失败：该操作需要 blog:edit 权限')
      return
    }
  } else if (!permissions.value.includes('blog:create')) {
    alert('操作失败：该操作需要 blog:create 权限')
    return
  }

  // 每次保存时重置标志，确保本轮保存触发新的跳转提示
  savedSuccessfully.value = false

  const tagList = tags.value.split(',').map(s => s.trim()).filter(Boolean)
  const contentText = content.value

  if (!title.value || !description.value || !category.value ||
      !pathCategory.value || !pathName.value || !contentText) {
    alert('请填写所有字段后再保存')
    return
  }

  // 元信息特殊字符校验：禁止空格、标点、路径分隔符等
  const META_RE = /[<>"'\\|*?\/ .!@#$%^&()+=[]{};:'"`,.<>?~\-]/

  if (META_RE.test(title.value)) {
    alert('标题 含有特殊字符');
    return;
  }
  if (META_RE.test(description.value)) {
    alert('描述 含有特殊字符');
    return;
  }
  if (META_RE.test(category.value)) {
    alert('分类 含有特殊字符');
    return;
  }
  if (META_RE.test(pathCategory.value) || META_RE.test(pathName.value)) {
    alert('文件路径 含有特殊字符');
    return;
  }
  if (!tagList.every(tag => {
    if (META_RE.test(tag)) {
      alert(`标签 含有特殊字符`);
      return false;
    }
    return true;
  })) return;
  const isEdit = isEditing.value && editFilePath.value != null
  const newFilePath = `${pathCategory.value}/${pathName.value}`

  const resp = await fetch(isEdit ? '/api/blog/update' : '/api/blog/save', {
    method: isEdit ? 'PUT' : 'POST',
    headers: {
      'Content-Type': 'application/json',
      'Authorization': `Bearer ${auth.token}`
    },
    body: JSON.stringify(isEdit ? {
      title: title.value,
      description: description.value,
      category: category.value,
      tags: tagList,
      update_time: updateTime.value,
      file_path_category: pathCategory.value,
      file_path_name: pathName.value,
      old_file_path: editFilePath.value,
      content: contentText,
    } : {
      title: title.value,
      description: description.value,
      category: category.value,
      tags: tagList,
      update_time: updateTime.value,
      file_path_category: pathCategory.value,
      file_path_name: pathName.value,
      content: contentText,
    }),
  })

  if (!resp.ok) {
    const err = await resp.json().catch(() => ({ error: '保存失败' }))
    alert(err.error || '保存失败')
    return
  }
  savedSuccessfully.value = true
  if (window.confirm('保存成功！是否立即跳转到博客页面？')) {
    router.push({ name: 'blog-detail', params: { file_path: newFilePath } })
  }
}
</script>

<template>
  <main class="blog-edit">
    <div class="blog-edit__layout">
      <aside class="blog-edit__left">
        <div class="blog-edit__meta glass">
          <div class="blog-edit__field">
            <label class="blog-edit__label">标题</label>
            <input v-model="title" class="blog-edit__input" placeholder="博客标题" />
          </div>
          <div class="blog-edit__field">
            <label class="blog-edit__label">描述</label>
            <input v-model="description" class="blog-edit__input" placeholder="简短描述" />
          </div>
          <div class="blog-edit__field">
            <label class="blog-edit__label">分类</label>
            <input v-model="category" class="blog-edit__input" placeholder="分类名称" />
          </div>
          <div class="blog-edit__field">
            <label class="blog-edit__label">标签</label>
            <input v-model="tags" class="blog-edit__input" placeholder="用英文逗号分隔" />
          </div>
          <div class="blog-edit__field">
            <label class="blog-edit__label">文件路径</label>
            <div class="blog-edit__path-row">
              <input v-model="pathCategory" class="blog-edit__input blog-edit__path-input"
                placeholder="分类目录" />
              <span class="blog-edit__path-sep">/</span>
              <input v-model="pathName" class="blog-edit__input blog-edit__path-input"
                placeholder="文件名" />
            </div>
          </div>
          <div class="blog-edit__field">
            <label class="blog-edit__label">更新时间</label>
            <input :value="updateTime" readonly class="blog-edit__input blog-edit__input--readonly" />
          </div>
        </div>
        <div class="blog-edit__actions">
          <button class="blog-edit__btn blog-edit__btn--import" @click="importFile">导入文件</button>
          <button class="blog-edit__btn blog-edit__btn--primary" @click="saveBlog">保存博客</button>
        </div>
      </aside>
      <section class="blog-edit__main glass">
        <MdEditor
          v-model="content"
          :theme="theme.isDark ? 'dark' : 'light'"
          preview-theme="github"
          :style="{ height: 'calc(100vh - 226px)' }"
          placeholder="在此编辑博客 Markdown 文本..."
          :preview="true"
          no-mermaid
          no-echarts
          no-upload-img
          no-prettier
          no-img-zoom-in
          :toolbars="[]"
          @on-save="saveBlog"
        />
      </section>
    </div>
  </main>
</template>

<style scoped>
.blog-edit {
  padding: 32px 24px 64px;
  overflow: visible;
}

.blog-edit__layout {
  max-width: 1400px;
  margin: 0 auto;
  display: grid;
  grid-template-columns: 300px 1fr;
  gap: 40px;
  align-items: start;
  overflow: visible;
}

/* ── 左侧 ── */
.blog-edit__left {
  position: sticky;
  top: 118px;
  align-self: start;
}
.blog-edit__field {
  margin-bottom: 16px;
}
.blog-edit__label {
  display: block;
  margin-bottom: 4px;
  font-size: 0.85rem;
  color: var(--color-text-secondary);
}
.blog-edit__input {
  width: 100%;
  padding: 6px 10px;
  font-size: 0.9rem;
  color: var(--color-text);
  background: rgba(0, 0, 0, 0.04);
  border: 1px solid var(--color-border);
  border-radius: 4px;
  outline: none;
  transition: border-color var(--transition-speed);
}
.blog-edit__input:focus {
  border-color: var(--pink-soft);
}
.blog-edit__input--readonly {
  cursor: not-allowed;
  background: rgba(0, 0, 0, 0.08);
  color: var(--color-text-secondary);
}
.blog-edit__input--readonly:focus {
  border-color: var(--color-border);
}

.blog-edit__path-row {
  display: flex;
  align-items: center;
  gap: 6px;
}
.blog-edit__path-input {
  flex: 1;
  min-width: 0;
}
.blog-edit__path-sep {
  color: var(--color-text-secondary);
  font-size: 0.9rem;
}

.blog-edit__actions {
  display: flex;
  flex-direction: column;
  gap: 8px;
  margin-top: auto;
  padding-top: 16px;
}
.blog-edit__btn {
  padding: 8px 0;
  font-size: 0.85rem;
  border: none;
  border-radius: 4px;
  cursor: pointer;
  transition: opacity var(--transition-speed);
}
.blog-edit__btn--primary {
  color: #fff;
  background-color: var(--pink-hot);
}
.blog-edit__btn--import {
  color: #fff;
  background: var(--pink-soft);
  border: 1px solid var(--color-border);
}
.blog-edit__btn:hover {
  opacity: 0.85;
}

/* ── 中间 ── */
.blog-edit__main {
  min-width: 0;
  overflow: hidden;
}
.blog-edit__main :deep(.md-editor) {
  background-color: transparent;
}
/* 固定 50/50 分屏：隐藏拖拽分隔条，编辑区与预览区宽度各占一半 */
.blog-edit__main :deep(.md-editor-resize-operate) {
  display: none !important;
}
.blog-edit__hint {
  margin: 4px 0 0;
  font-size: 0.75rem;
  color: var(--color-text-secondary);
  opacity: 0.7;
}

</style>
