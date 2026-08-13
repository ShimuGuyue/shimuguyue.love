<script setup lang="ts">
import { computed, onMounted, ref, watch } from 'vue'
import { useAuthStore } from '@/stores/auth'
import '@/assets/manage/button.css'
import '@/assets/manage/font.css'

interface ManageUser {
  id: number
  username: string | null
  key_enabled: boolean
  enabled: boolean
  has_password: boolean
  permissions: string[]
}

interface EditDraft {
  id: number
  username: string
  key_enabled: boolean
  enabled: boolean
  key: string
  password: string
  permissions: string[]
}

interface PageRow {
  user: ManageUser | null
  draft: EditDraft | null
}

const auth = useAuthStore()

const users = ref<ManageUser[]>([])
const allPermissions = ref<string[]>([])
const loading = ref(false)
const error = ref('')

const editing = ref(false)
const saving = ref(false)
const drafts = ref<EditDraft[]>([])

const showCreateDialog = ref(false)
const creating = ref(false)
const permDialogDraft = ref<EditDraft | null>(null)
const permDialogSelection = ref<string[]>([])
const createForm = ref({
  username: '',
  key: '',
  password: '',
  key_enabled: true,
  permissions: [] as string[],
})

/** 分页：每页固定 15 条 */
const PAGE_SIZE = 15
const page = ref(1)

const pageCount = computed(() =>
  Math.max(1, Math.ceil(users.value.length / PAGE_SIZE))
)

const pageNumbers = computed(() =>
  Array.from({ length: pageCount.value }, (_, i) => i + 1)
)

/** 当前页数据：不足 15 条时用空行补齐，并带上对应的编辑草稿。 */
const pageRows = computed<PageRow[]>(() => {
  const start = (page.value - 1) * PAGE_SIZE
  return Array.from({ length: PAGE_SIZE }, (_, i) => {
    const user = users.value[start + i] ?? null
    return { user, draft: drafts.value[start + i] ?? null }
  })
})

watch([users, pageCount], () => {
  if (page.value > pageCount.value) {
    page.value = pageCount.value
  }
})

async function loadUsers() {
  loading.value = true
  error.value = ''
  try {
    const resp = await fetch('/api/manage/users', {
      headers: { 'Authorization': 'Bearer ' + auth.token }
    })
    if (!resp.ok) {
      const data = await resp.json().catch(() => ({}))
      error.value = data.error ?? '加载失败'
      return
    }
    const data = await resp.json()
    users.value = data.users ?? []
    allPermissions.value = data.all_permissions ?? []
  } catch {
    error.value = '加载失败'
  } finally {
    loading.value = false
  }
}

onMounted(loadUsers)

/** 进入编辑模式：为所有用户生成草稿。 */
function startEdit() {
  drafts.value = users.value.map((user) => ({
    id: user.id,
    username: user.username ?? '',
    key_enabled: user.key_enabled,
    enabled: user.enabled,
    key: '',
    password: '',
    permissions: [...user.permissions],
  }))
  editing.value = true
}

/** 取消编辑：丢弃草稿。 */
function cancelEdit() {
  editing.value = false
  drafts.value = []
}

/** 打开新建用户弹窗。 */
function openCreateDialog() {
  createForm.value = {
    username: '',
    key: '',
    password: '',
    key_enabled: true,
    permissions: [],
  }
  showCreateDialog.value = true
}

/** 提交新建用户。 */
async function submitCreate() {
  if (!createForm.value.key.trim()) {
    window.alert('新用户必须设置密钥')
    return
  }
  creating.value = true
  try {
    const resp = await fetch('/api/manage/user/create', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'Authorization': 'Bearer ' + auth.token,
      },
      body: JSON.stringify({
        username: createForm.value.username,
        key: createForm.value.key,
        password: createForm.value.password,
        key_enabled: createForm.value.key_enabled,
        permissions: createForm.value.permissions,
      }),
    })
    const data = await resp.json().catch(() => ({}))
    if (!resp.ok) {
      window.alert(data.error ?? '新建用户失败')
      return
    }
    showCreateDialog.value = false
    editing.value = false
    drafts.value = []
    await loadUsers()
  } catch {
    window.alert('新建用户失败')
  } finally {
    creating.value = false
  }
}

/** 打开某行的权限编辑弹窗。 */
function openPermDialog(draft: EditDraft) {
  permDialogSelection.value = [...draft.permissions]
  permDialogDraft.value = draft
}

/** 确认权限修改：写回对应行草稿。 */
function confirmPermDialog() {
  if (permDialogDraft.value) {
    permDialogDraft.value.permissions = [...permDialogSelection.value]
  }
  permDialogDraft.value = null
}

/** 取消权限修改。 */
function cancelPermDialog() {
  permDialogDraft.value = null
}

/** 保存：只提交有改动的行。 */
async function saveChanges() {
  saving.value = true
  try {
    for (const draft of drafts.value) {
      const original = users.value.find((user) => user.id === draft.id)
      if (!original) continue

      const payload: Record<string, unknown> = { id: draft.id }
      let changed = false

      if (draft.username !== (original.username ?? '')) {
        payload.username = draft.username
        changed = true
      }
      if (draft.key_enabled !== original.key_enabled) {
        payload.key_enabled = draft.key_enabled
        changed = true
      }
      if (draft.enabled !== original.enabled) {
        payload.enabled = draft.enabled
        changed = true
      }
      if (draft.key) {
        payload.key = draft.key
        changed = true
      }
      if (draft.password) {
        payload.password = draft.password
        changed = true
      }
      const permsChanged =
        original.permissions.length !== draft.permissions.length ||
        [...original.permissions].sort().join(',') !==
          [...draft.permissions].sort().join(',')
      if (permsChanged) {
        payload.permissions = draft.permissions
        changed = true
      }
      if (!changed) continue

      const resp = await fetch('/api/manage/user/update', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          'Authorization': 'Bearer ' + auth.token,
        },
        body: JSON.stringify(payload),
      })
      const data = await resp.json().catch(() => ({}))
      if (!resp.ok) {
        window.alert(data.error ?? '保存失败')
        return
      }
    }
    // 与个人信息页同款刷新逻辑：保存后从后端刷新当前用户名，导航栏即时同步
    await auth.refreshUsername()
    editing.value = false
    drafts.value = []
    await loadUsers()
  } catch {
    window.alert('保存失败')
  } finally {
    saving.value = false
  }
}
</script>

<template>
  <div class="users-section">
    <div class="users-header">
      <h1 class="admin-content__title">用户管理</h1>
      <div v-if="users.length" class="users-toolbar">
        <template v-if="editing">
          <button
            type="button"
            class="manage-btn manage-btn--primary"
            :disabled="saving"
            @click="openCreateDialog"
          >
            新建用户
          </button>
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
        <button
          v-else
          type="button"
          class="manage-btn manage-btn--primary"
          @click="startEdit"
        >
          编辑用户
        </button>
      </div>
    </div>

    <div v-if="loading" class="page-loading">加载中...</div>
    <p v-else-if="error" class="users-hint">{{ error }}</p>

    <template v-else-if="users.length">
      <div class="users-table-wrap">
        <table class="users-table" :class="{ 'users-table--editing': editing }">
          <thead>
            <tr>
              <th class="users-table__id">ID</th>
              <th class="users-table__status">用户可用状态</th>
              <th class="users-table__username">用户名</th>
              <th class="users-table__mask">密钥</th>
              <th class="users-table__status">密钥可用状态</th>
              <th class="users-table__mask">密码</th>
              <th class="users-table__perms-col">权限列表</th>
            </tr>
          </thead>
          <tbody>
            <tr v-for="(row, index) in pageRows" :key="index">
              <td class="users-table__id">{{ row.user?.id ?? '' }}</td>
              <td class="users-table__status">
                <input
                  v-if="row.draft"
                  v-model="row.draft.enabled"
                  type="checkbox"
                />
                <input
                  v-else-if="row.user"
                  type="checkbox"
                  :checked="row.user.enabled"
                  disabled
                />
              </td>
              <td>
                <input
                  v-if="editing && row.draft"
                  v-model="row.draft.username"
                  class="users-table__input"
                  maxlength="10"
                />
                <span v-else-if="row.user && row.user.username">
                  {{ row.user.username }}
                </span>
                <span v-else-if="row.user" class="users-hint">无</span>
              </td>
              <td class="users-table__mask">
                <input
                  v-if="editing && row.draft"
                  v-model="row.draft.key"
                  type="password"
                  class="users-table__input"
                  placeholder="留空不修改"
                />
                <span v-else-if="row.user">？？？？？？？？？</span>
              </td>
              <td class="users-table__status">
                <input
                  v-if="row.draft"
                  v-model="row.draft.key_enabled"
                  type="checkbox"
                />
                <input
                  v-else-if="row.user"
                  type="checkbox"
                  :checked="row.user.key_enabled"
                  disabled
                />
              </td>
              <td>
                <input
                  v-if="editing && row.draft"
                  v-model="row.draft.password"
                  type="password"
                  class="users-table__input"
                  placeholder="留空不修改"
                />
                <span v-else-if="row.user && row.user.has_password">
                  ？？？？？？？？？
                </span>
                <span v-else-if="row.user" class="users-hint">无</span>
              </td>
              <td>
                <div v-if="editing && row.draft" class="users-table__perms-edit">
                  <span class="users-table__perms-summary">
                    {{
                      row.draft.permissions.length
                        ? row.draft.permissions.join('、')
                        : '无'
                    }}
                  </span>
                  <button
                    type="button"
                    class="manage-btn users-table__perms-btn"
                    @click="openPermDialog(row.draft)"
                  >
                    编辑权限
                  </button>
                </div>
                <template v-else-if="row.user">
                  <span v-if="row.user.permissions.length">
                    {{ row.user.permissions.join('、') }}
                  </span>
                  <span v-else class="users-hint">无</span>
                </template>
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

    <p v-else class="users-hint">暂无用户</p>

    <div
      v-if="showCreateDialog"
      class="create-mask"
    >
      <div class="create-box" role="dialog" aria-modal="true">
        <h2 class="create-box__title">新建用户</h2>

        <label class="create-box__field">
          <span>用户名</span>
          <input
            v-model="createForm.username"
            class="create-box__input"
            maxlength="10"
            placeholder="可选"
          />
        </label>

        <label class="create-box__field">
          <span>密钥</span>
          <input
            v-model="createForm.key"
            type="password"
            class="create-box__input"
          />
        </label>

        <label class="create-box__field">
          <span>密码</span>
          <input
            v-model="createForm.password"
            type="password"
            class="create-box__input"
            placeholder="可选"
          />
        </label>

        <label class="create-box__field create-box__field--inline">
          <span>密钥可用</span>
          <input
            v-model="createForm.key_enabled"
            type="checkbox"
            class="create-box__checkbox"
          />
        </label>

        <div class="create-box__field">
          <span>权限</span>
          <div class="create-box__perms">
            <label
              v-for="perm in allPermissions"
              :key="perm"
              class="create-box__perm"
            >
              <input
                type="checkbox"
                :value="perm"
                v-model="createForm.permissions"
                class="create-box__checkbox"
              />
              {{ perm }}
            </label>
          </div>
        </div>

        <div class="create-box__actions">
          <button
            type="button"
            class="manage-btn manage-btn--primary"
            :disabled="creating"
            @click="submitCreate"
          >
            {{ creating ? '创建中...' : '创建用户' }}
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

    <div v-if="permDialogDraft" class="create-mask">
      <div class="create-box" role="dialog" aria-modal="true">
        <h2 class="create-box__title">编辑权限</h2>

        <div class="create-box__perms">
          <label
            v-for="perm in allPermissions"
            :key="perm"
            class="create-box__perm"
          >
            <input
              type="checkbox"
              :value="perm"
              v-model="permDialogSelection"
              class="create-box__checkbox"
            />
            {{ perm }}
          </label>
        </div>

        <div class="create-box__actions">
          <button
            type="button"
            class="manage-btn manage-btn--primary"
            @click="confirmPermDialog"
          >
            确认
          </button>
          <button
            type="button"
            class="manage-btn"
            @click="cancelPermDialog"
          >
            取消
          </button>
        </div>
      </div>
    </div>
  </div>
</template>

<style scoped>
.users-table {
  width: 100%;
  /* 固定高度：表头 + 15 行，每行 44px */
  height: calc(44px * 16);
  table-layout: fixed;
  border-collapse: collapse;
  text-align: left;
  /* 自定义复选框颜色 */
  --checkbox-checked-color: #3366ff;
}

html.dark .users-table {
  --checkbox-checked-color: #598bff;
}

.users-table th,
.users-table td {
  height: 44px;
  padding: 0 14px;
  border: 1px solid var(--color-border);
  font-size: 0.95rem;
  color: var(--color-text);
  vertical-align: middle;
  white-space: nowrap;
}

/* 编辑模式：行高保持 44px，超宽内容裁剪 */
.users-table--editing th,
.users-table--editing td {
  overflow: hidden;
}

.users-table th {
  font-weight: 600;
  color: var(--color-text-secondary);
  background-color: var(--color-hover);
}

.users-table__id {
  width: 80px;
}

.users-table__username {
  width: 18%;
}

.users-table__status {
  width: 8em;
  text-align: center;
}

.users-table__mask {
  width: 12em;
}

/* ── 自定义复选框 ── */

.users-table input[type='checkbox'] {
  width: 18px;
  height: 18px;
  margin: 0;
  vertical-align: middle;
  appearance: none;
  -webkit-appearance: none;
  border: 2px solid var(--color-text-secondary);
  background-color: var(--color-nav-bg);
  cursor: default;
  position: relative;
}

.users-table input[type='checkbox']:checked {
  border-color: var(--checkbox-checked-color);
  background-color: var(--checkbox-checked-color);
}

.users-table input[type='checkbox']:checked::after {
  content: '';
  position: absolute;
  left: 4px;
  top: 1px;
  width: 6px;
  height: 10px;
  border: solid #fff;
  border-width: 0 2px 2px 0;
  transform: rotate(45deg);
}

/* ── 编辑输入控件 ── */

.users-table__input {
  width: 100%;
  height: 28px;
  padding: 0 8px;
  border: 1px solid var(--color-border);
  background-color: var(--color-nav-bg);
  font-size: 0.9rem;
  color: var(--color-text);
}

.users-table__input:focus {
  outline: none;
  border-color: var(--color-text-secondary);
}

.users-table__perms-edit {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 8px;
  max-width: 100%;
}

.users-table__perms-summary {
  flex: 1;
  min-width: 0;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.users-table__perms-btn {
  flex-shrink: 0;
  min-width: auto;
  padding: 3px 10px;
  font-size: 0.85rem;
}

.users-table__perm input {
  cursor: pointer;
}

.users-hint {
  margin: 0;
  font-size: 0.9rem;
  color: var(--color-text-secondary);
}

.users-section {
  display: flex;
  flex-direction: column;
  min-height: calc(100vh - 144px);
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

/* ── 标题栏与工具栏 ── */

.users-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  margin-bottom: 16px;
}

.admin-content__title {
  margin: 0;
  font-size: 1.5rem;
  color: var(--color-text);
}

.users-toolbar {
  display: flex;
  gap: 8px;
}

/* ── 分页 ── */

.users-table-wrap {
  width: 100%;
  overflow-x: auto;
}

.users-pager {
  display: flex;
  align-items: center;
  justify-content: flex-end;
  width: 100%;
  margin-top: 16px;
  font-size: 0.9rem;
  color: var(--color-text-secondary);
}

.users-pager__pages {
  display: flex;
  gap: 4px;
}

.users-pager__btn {
  min-width: 32px;
  padding: 4px 10px;
  border: 1px solid var(--color-border);
  background: transparent;
  font-size: 0.875rem;
  color: var(--color-text);
  cursor: pointer;
}

.users-pager__btn:hover:not(:disabled) {
  background-color: var(--color-hover);
}

.users-pager__btn:disabled {
  opacity: 0.4;
  cursor: default;
}

.users-pager__btn--active {
  background-color: var(--color-hover);
  font-weight: 600;
}

/* ── 新建用户弹窗 ── */

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

.create-box__field--inline > span {
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

.create-box__checkbox {
  width: 18px;
  height: 18px;
  accent-color: #3366ff;
}

.create-box__perms {
  display: flex;
  flex-wrap: wrap;
  gap: 4px 16px;
}

.create-box__perm {
  display: inline-flex;
  align-items: center;
  gap: 6px;
  font-size: 0.85rem;
  white-space: nowrap;
  cursor: pointer;
  color: var(--manage-info-color);
}

.create-box__actions {
  display: flex;
  justify-content: flex-end;
  gap: 8px;
  margin-top: 20px;
}

</style>
