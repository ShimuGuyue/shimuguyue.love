<script setup lang="ts">
import { computed, onMounted, ref, watch } from 'vue'
import { useAuthStore } from '@/stores/auth'

import '@/assets/button/manage.css'
import '@/assets/manage/font.css'
import '@/assets/manage/table.css'

/** 友链记录：来自 GET /api/friends，image 为空串表示未匹配到头像。 */
interface FriendLink {
  name: string
  url: string
  description: string
  image: string
}

/** 编辑模式下的行草稿：可编辑站点名、站点链接、站点描述；以原站点链接 old_url 定位记录；头像在独立的头像编辑模式中修改。 */
interface FriendEdit {
  old_url: string
  name: string
  url: string
  description: string
}

const auth = useAuthStore()

const friends = ref<FriendLink[]>([])
const loading = ref(false)
const error = ref('')

const editing = ref(false)
const avatarEditing = ref(false)
const showCreateDialog = ref(false)
const saving = ref(false)
const creating = ref(false)
const canEdit = ref(false)
const drafts = ref<FriendEdit[]>([])
const createForm = ref({ name: '', url: '', description: '' })
const fileInput = ref<HTMLInputElement | null>(null)
const uploadName = ref<string | null>(null)
const uploadingName = ref<string | null>(null)

/** 分页：每页固定 9 条 */
const PAGE_SIZE = 9
const page = ref(1)

const pageCount = computed(() =>
  Math.max(1, Math.ceil(friends.value.length / PAGE_SIZE))
)

const pageNumbers = computed(() =>
  Array.from({ length: pageCount.value }, (_, i) => i + 1)
)

/** 当前页行数据：按索引与完整草稿列表对齐。 */
const pageRows = computed(() => {
  const start = (page.value - 1) * PAGE_SIZE
  return friends.value
    .slice(start, start + PAGE_SIZE)
    .map((friend, i) => ({ friend, draft: drafts.value[start + i] ?? null }))
})

watch([friends, pageCount], () => {
  if (page.value > pageCount.value) {
    page.value = pageCount.value
  }
})

async function loadFriends() {
  loading.value = true
  error.value = ''
  try {
    const resp = await fetch('/api/friends', {
      headers: { 'Authorization': 'Bearer ' + auth.token }
    })
    if (!resp.ok) {
      const data = await resp.json().catch(() => ({}))
      error.value = data.error ?? '加载失败'
      return
    }
    friends.value = await resp.json() as FriendLink[]
  } catch {
    error.value = '加载失败'
  } finally {
    loading.value = false
  }
}

async function loadPermissions() {
  if (!auth.isLoggedIn || !auth.token) return
  try {
    const resp = await fetch('/api/user/permissions', {
      headers: { 'Authorization': 'Bearer ' + auth.token }
    })
    if (resp.ok) {
      const data = await resp.json()
      const permissions = data.permissions || []
      canEdit.value = permissions.includes('manage:edit')
    }
  } catch { /* 权限获取失败静默 */ }
}

onMounted(async () => {
  await loadFriends()
  await loadPermissions()
})

/** 进入编辑模式：为所有友链生成草稿。 */
function startEdit() {
  if (!canEdit.value) {
    window.alert('操作失败：该操作需要 manage:edit 权限')
    return
  }
  showCreateDialog.value = false
  avatarEditing.value = false
  drafts.value = friends.value.map((friend) => ({
    old_url: friend.url,
    name: friend.name,
    url: friend.url,
    description: friend.description,
  }))
  editing.value = true
}

/** 进入头像编辑模式：可点击各行头像上传 1:1 方形图片替换。 */
function startAvatarEdit() {
  if (!canEdit.value) {
    window.alert('操作失败：该操作需要 manage:edit 权限')
    return
  }
  showCreateDialog.value = false
  editing.value = false
  drafts.value = []
  avatarEditing.value = true
}

/** 退出头像编辑模式。 */
function exitAvatarEdit() {
  avatarEditing.value = false
}

/** 打开新建友链弹窗。 */
function openCreateDialog() {
  if (!canEdit.value) {
    window.alert('操作失败：该操作需要 manage:edit 权限')
    return
  }
  editing.value = false
  avatarEditing.value = false
  drafts.value = []
  createForm.value = { name: '', url: '', description: '' }
  showCreateDialog.value = true
}

/** 提交新建友链：站点名与站点链接必填，且站点链接不允许重复。 */
async function submitCreate() {
  if (!canEdit.value) {
    window.alert('操作失败：该操作需要 manage:edit 权限')
    return
  }

  const name = createForm.value.name.trim()
  const url = createForm.value.url.trim()
  const description = createForm.value.description.trim()
  if (!name) {
    window.alert('站点名 不能为空')
    return
  }
  if (!url) {
    window.alert('站点链接 不能为空')
    return
  }
  if (friends.value.some((friend) => friend.url === url)) {
    window.alert(`站点链接已存在：${url}`)
    return
  }

  creating.value = true
  try {
    const resp = await fetch('/api/friends', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'Authorization': 'Bearer ' + auth.token,
      },
      body: JSON.stringify({ name, url, description }),
    })
    const data = await resp.json().catch(() => ({}))
    if (!resp.ok) {
      window.alert(data.error ?? '创建失败')
      return
    }
    showCreateDialog.value = false
    await loadFriends()
    page.value = pageCount.value
    window.alert('创建成功')
  } catch {
    window.alert('创建失败')
  } finally {
    creating.value = false
  }
}

/** 取消编辑：丢弃草稿。 */
function cancelEdit() {
  editing.value = false
  drafts.value = []
}

/** 保存编辑：仅提交有改动的行，站点名与站点链接必填，且站点链接不允许重复。 */
async function saveChanges() {
  if (!canEdit.value) {
    window.alert('操作失败：该操作需要 manage:edit 权限')
    return
  }

  for (const draft of drafts.value) {
    if (!draft.name.trim()) {
      window.alert('站点名 不能为空')
      return
    }
    if (!draft.url.trim()) {
      window.alert('站点链接 不能为空')
      return
    }
  }

  const urlSeen = new Set<string>()
  for (const draft of drafts.value) {
    const url = draft.url.trim()
    if (urlSeen.has(url)) {
      window.alert(`存在重复的站点链接：${url}`)
      return
    }
    urlSeen.add(url)
  }

  saving.value = true
  try {
    for (const draft of drafts.value) {
      const original = friends.value.find((friend) => friend.url === draft.old_url)
      if (!original) continue

      const name = draft.name.trim()
      const url = draft.url.trim()
      const description = draft.description.trim()
      const changed =
        name !== original.name ||
        url !== original.url ||
        description !== original.description
      if (!changed) continue

      const resp = await fetch('/api/friends/update', {
        method: 'PUT',
        headers: {
          'Content-Type': 'application/json',
          'Authorization': 'Bearer ' + auth.token,
        },
        body: JSON.stringify({
          old_url: draft.old_url,
          name,
          url,
          description,
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
    await loadFriends()
    window.alert('保存成功')
  } catch {
    window.alert('保存失败')
  } finally {
    saving.value = false
  }
}

/** 点击头像上的上传按钮：记录目标友链并触发文件选择。 */
function openUpload(friend: FriendLink) {
  uploadName.value = friend.url
  fileInput.value?.click()
}

/** 校验图片宽高比例是否为 1:1（正方形）。 */
function isSquare(file: File): Promise<boolean> {
  return new Promise((resolve) => {
    const url = URL.createObjectURL(file)
    const img = new Image()
    img.onload = () => {
      URL.revokeObjectURL(url)
      resolve(img.naturalWidth === img.naturalHeight)
    }
    img.onerror = () => {
      URL.revokeObjectURL(url)
      resolve(false)
    }
    img.src = url
  })
}

/** 上传选中文件到目标友链；成功后即时替换该行头像。 */
async function uploadAvatar(url: string, file: File) {
  uploadingName.value = url
  try {
    const form = new FormData()
    form.append('url', url)
    form.append('file', file)
    const resp = await fetch('/api/friend/avatar/upload', {
      method: 'POST',
      headers: { 'Authorization': 'Bearer ' + auth.token },
      body: form,
    })
    const data = await resp.json().catch(() => ({}))
    if (!resp.ok) {
      window.alert(data.error ?? '上传失败')
      return
    }

    const image = data.image as string | undefined
    const friend = friends.value.find((f) => f.url === url)
    if (friend && image) {
      friend.image = image
    }
    window.alert('上传成功')
  } catch {
    window.alert('上传失败')
  } finally {
    uploadingName.value = null
  }
}

/** 文件选择变化：先校验 1:1，再上传。 */
async function onFileChange(event: Event) {
  const input = event.target as HTMLInputElement
  const file = input.files?.[0]
  input.value = ''
  if (!file) return

  const url = uploadName.value
  if (!url) return

  const ok = await isSquare(file)
  if (!ok) {
    window.alert('图片宽高比例必须为 1:1')
    return
  }
  await uploadAvatar(url, file)
}
</script>

<template>
  <div class="friends-section">
    <div class="friends-header">
      <h1 class="admin-content__title">友链管理</h1>
      <template v-if="editing">
        <div class="friends-toolbar">
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
            class="manage-btn"
            :disabled="saving"
            @click="cancelEdit"
          >
            取消编辑
          </button>
        </div>
      </template>
      <template v-else-if="avatarEditing">
        <div class="friends-toolbar">
          <button
            type="button"
            class="manage-btn manage-btn--primary"
            @click="exitAvatarEdit"
          >
            结束修改
          </button>
        </div>
      </template>
      <div v-else class="friends-toolbar">
        <button
          type="button"
          class="manage-btn manage-btn--primary"
          :disabled="saving"
          @click="openCreateDialog"
        >
          新建友链
        </button>
        <button
          type="button"
          class="manage-btn"
          :disabled="saving"
          @click="startAvatarEdit"
        >
          编辑头像
        </button>
        <button
          type="button"
          class="manage-btn"
          :disabled="saving"
          @click="startEdit"
        >
          编辑友链
        </button>
      </div>
    </div>

    <div v-if="loading" class="page-loading">加载中...</div>
    <p v-else-if="error" class="friends-hint">{{ error }}</p>

    <template v-else-if="friends.length">
      <div class="blogs-table-wrap">
        <table class="blogs-table friends-table">
          <thead>
            <tr>
              <th class="friends-table__image">头像图片</th>
              <th class="friends-table__name">站点名</th>
              <th class="friends-table__url">站点链接</th>
              <th class="friends-table__desc">站点描述</th>
            </tr>
          </thead>
          <tbody>
            <tr v-for="{ friend, draft } in pageRows" :key="friend.url">
              <td class="friends-table__image">
                <div class="friends-table__avatar-wrap">
                  <img
                    v-if="friend.image"
                    class="friends-table__avatar"
                    :src="friend.image"
                    :alt="friend.name"
                    loading="lazy"
                  />
                  <div
                    v-else
                    class="friends-table__avatar friends-table__avatar--fallback"
                  >
                    {{ friend.name.charAt(0) }}
                  </div>
                  <button
                    v-if="avatarEditing"
                    type="button"
                    class="friends-table__upload"
                    :disabled="uploadingName === friend.url"
                    @click="openUpload(friend)"
                  >
                    {{ uploadingName === friend.url ? '上传中' : '上传' }}
                  </button>
                </div>
              </td>
              <td class="friends-table__name">
                <input
                  v-if="editing && draft"
                  v-model="draft.name"
                  class="blogs-table__input"
                  placeholder="站点名"
                />
                <span v-else :title="friend.name">{{ friend.name }}</span>
              </td>
              <td class="friends-table__url">
                <input
                  v-if="editing && draft"
                  v-model="draft.url"
                  class="blogs-table__input"
                  placeholder="https://..."
                />
                <a
                  v-else
                  :href="friend.url"
                  target="_blank"
                  rel="noopener noreferrer"
                  class="friends-table__link"
                  :title="friend.url"
                >
                  {{ friend.url }}
                </a>
              </td>
              <td class="friends-table__desc">
                <input
                  v-if="editing && draft"
                  v-model="draft.description"
                  class="blogs-table__input"
                  placeholder="站点描述"
                />
                <span v-else :title="friend.description">{{ friend.description }}</span>
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

    <p v-else class="friends-hint">暂无友链</p>

    <input
      ref="fileInput"
      type="file"
      class="friends-upload-input"
      accept="image/png,image/jpeg,image/webp"
      @change="onFileChange"
    />

    <div
      v-if="showCreateDialog"
      class="create-mask"
    >
      <div class="create-box" role="dialog" aria-modal="true">
        <h2 class="create-box__title">新建友链</h2>

        <label class="create-box__field">
          <span>站点名</span>
          <input
            v-model="createForm.name"
            class="create-box__input"
            placeholder="必填"
          />
        </label>

        <label class="create-box__field">
          <span>站点链接</span>
          <input
            v-model="createForm.url"
            class="create-box__input"
            placeholder="https://... 必填"
          />
        </label>

        <label class="create-box__field">
          <span>站点描述</span>
          <input
            v-model="createForm.description"
            class="create-box__input"
            placeholder="可选"
          />
        </label>

        <div class="create-box__actions">
          <button
            type="button"
            class="manage-btn manage-btn--primary"
            :disabled="creating"
            @click="submitCreate"
          >
            {{ creating ? '创建中...' : '创建友链' }}
          </button>
          <button
            type="button"
            class="manage-btn"
            :disabled="creating"
            @click="showCreateDialog = false"
          >
            取消创建
          </button>
        </div>
      </div>
    </div>
  </div>
</template>

<style scoped>
.friends-section {
  display: flex;
  flex-direction: column;
  min-height: calc(100vh - 144px);
}

.friends-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  margin-bottom: 16px;
}

.friends-toolbar {
  display: flex;
  gap: 8px;
}

.admin-content__title {
  margin: 0;
  font-size: 1.5rem;
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

.friends-hint {
  margin: 0;
  font-size: 0.9rem;
  color: var(--color-text-secondary);
}

/* ── 新建友链弹窗（与用户管理页弹窗同款） ── */
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
  max-width: calc(100vw - 48px);
  padding: 24px;
  background-color: var(--color-nav-bg);
  border: 1px solid var(--color-border);
  box-shadow: 0 0.25rem 1rem rgba(0, 0, 0, 0.15);
}

.create-box__title {
  margin: 0 0 20px;
  font-size: 1.2rem;
  color: var(--color-text);
}

.create-box__field {
  display: flex;
  align-items: center;
  gap: 12px;
  margin-bottom: 14px;
  font-size: 0.9rem;
  color: var(--color-text);
}

.create-box__field > span {
  flex-shrink: 0;
  width: 64px;
  color: var(--manage-label-color);
}

.create-box__input {
  flex: 1;
  height: 30px;
  padding: 0 8px;
  border: 1px solid var(--color-border);
  background-color: var(--color-bg);
  font-size: 0.9rem;
  color: var(--color-text);
}

.create-box__input:focus {
  outline: none;
  border-color: var(--color-text-secondary);
}

.create-box__actions {
  display: flex;
  justify-content: flex-end;
  gap: 8px;
  margin-top: 20px;
}

/* 复用博客管理表格外观；友链数量较少，行高自适应并容纳头像。 */
.blogs-table.friends-table {
  height: auto;
}

.friends-table td {
  height: 76px;
}

/* ── 列宽 ── */

/* 头像图片列：收紧左右内边距，宽度刚好包住缩略图 */
.friends-table__image {
  width: 10%;
  text-align: center;
}

.friends-table__name {
  width: 15%;
}

.friends-table__url {
  width: 20%;
}

.friends-table__desc {
  width: 55%;
}

/* 站点链接：下划线可点击 */
.friends-table__link {
  color: var(--color-text);
  text-decoration: underline;
  text-underline-offset: 2px;
}

.friends-table__link:hover {
  color: var(--color-text-secondary);
}

/* 头像缩略图：圆形，居中裁切；无图时显示站点名首字符占位 */
.friends-table__avatar-wrap {
  position: relative;
  display: inline-block;
  width: 56px;
  height: 56px;
  border-radius: 50%;
  overflow: hidden;
  vertical-align: middle;
}

.friends-table__avatar {
  display: block;
  width: 100%;
  height: 100%;
  border-radius: 50%;
  object-fit: cover;
  object-position: center;
}

.friends-table__avatar--fallback {
  display: flex;
  align-items: center;
  justify-content: center;
  color: #fff;
  font-size: 1.2rem;
  font-weight: 700;
  background-color: var(--pink-hot);
}

/* 头像编辑模式下头像上的上传覆盖按钮 */
.friends-table__upload {
  position: absolute;
  inset: 0;
  display: flex;
  align-items: center;
  justify-content: center;
  border: none;
  border-radius: 50%;
  background-color: rgba(0, 0, 0, 0.45);
  color: #fff;
  font-size: 0.8rem;
  cursor: pointer;
}

.friends-table__upload:hover:not(:disabled) {
  background-color: rgba(0, 0, 0, 0.62);
}

.friends-table__upload:disabled {
  cursor: default;
  opacity: 0.7;
}

/* 隐藏的图片选择输入框 */
.friends-upload-input {
  display: none;
}
</style>
