<script setup lang="ts">
import { computed, onMounted, ref } from 'vue'
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

/** 编辑模式下的行草稿：可编辑站点名、站点链接、站点描述，图片暂不开放修改。 */
interface FriendEdit {
  old_name: string
  name: string
  url: string
  description: string
}

const auth = useAuthStore()

const friends = ref<FriendLink[]>([])
const loading = ref(false)
const error = ref('')

const editing = ref(false)
const saving = ref(false)
const canEdit = ref(false)
const drafts = ref<FriendEdit[]>([])

/** 行数据：编辑态绑定对应草稿（按索引对齐）。 */
const editRows = computed(() =>
  friends.value.map((friend, i) => ({ friend, draft: drafts.value[i] ?? null }))
)

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
  drafts.value = friends.value.map((friend) => ({
    old_name: friend.name,
    name: friend.name,
    url: friend.url,
    description: friend.description,
  }))
  editing.value = true
}

/** 取消编辑：丢弃草稿。 */
function cancelEdit() {
  editing.value = false
  drafts.value = []
}

/** 保存编辑：仅提交有改动的行，站点名与站点链接必填，且站点名不允许重复。 */
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

  const nameSeen = new Set<string>()
  for (const draft of drafts.value) {
    const name = draft.name.trim()
    if (nameSeen.has(name)) {
      window.alert(`存在重复的站点名：${name}`)
      return
    }
    nameSeen.add(name)
  }

  saving.value = true
  try {
    for (const draft of drafts.value) {
      const original = friends.value.find((friend) => friend.name === draft.old_name)
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
          old_name: draft.old_name,
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
      <button
        v-else
        type="button"
        class="manage-btn manage-btn--primary"
        :disabled="saving"
        @click="startEdit"
      >
        编辑友链
      </button>
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
            <tr v-for="{ friend, draft } in editRows" :key="friend.name">
              <td class="friends-table__image">
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
    </template>

    <p v-else class="friends-hint">暂无友链</p>
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
.friends-table__avatar {
  display: inline-block;
  width: 56px;
  height: 56px;
  border-radius: 50%;
  object-fit: cover;
  object-position: center;
  overflow: hidden;
  vertical-align: middle;
}

.friends-table__avatar--fallback {
  /* inline-flex 使其参与行内排版，能被单元格 text-align:center 居中 */
  display: inline-flex;
  align-items: center;
  justify-content: center;
  color: #fff;
  font-size: 1.2rem;
  font-weight: 700;
  background-color: var(--pink-hot);
}
</style>
