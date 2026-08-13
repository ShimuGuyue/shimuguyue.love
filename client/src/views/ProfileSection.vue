<script setup lang="ts">
import { computed, onMounted, ref } from 'vue'
import { useAuthStore } from '@/stores/auth'
import '@/assets/manage/button.css'
import '@/assets/manage/font.css'
import '@/assets/manage/table.css'

const auth = useAuthStore()

const permissions = ref<string[]>([])
const loading = ref(false)
const editingUsername = ref(false)
const savingUsername = ref(false)
const usernameDraft = ref('')
const keyEnabled = ref(false)
const hasPassword = ref(false)
const keyEnabledDraft = ref(true)
const passwordDraft = ref('')

/** 权限大类型分组：每种大类型在权限表格中占一行。 */
const PERMISSION_GROUPS = [
  { label: '用户管理', pattern: /^manage:/ },
  { label: '博客', pattern: /^blog:/ },
  { label: '照片墙', pattern: /^photo_wall:/ },
  { label: '个人简介', pattern: /^introduction:/ },
]

/** 按大类型归组后的权限（仅含当前用户拥有的权限）。 */
const permissionGroups = computed(() =>
  PERMISSION_GROUPS.map(group => ({
    label: group.label,
    perms: permissions.value.filter(perm => group.pattern.test(perm)),
  }))
)

onMounted(async () => {
  if (!auth.id) return
  loading.value = true
  try {
    const [infoResp, permResp] = await Promise.all([
      fetch('/api/user/info', {
        headers: { 'Authorization': 'Bearer ' + auth.token }
      }),
      fetch('/api/user/permissions', {
        headers: { 'Authorization': 'Bearer ' + auth.token }
      })
    ])
    if (infoResp.ok) {
      const data = await infoResp.json()
      keyEnabled.value = data.key_enabled ?? false
      hasPassword.value = data.has_password ?? false
    }
    if (permResp.ok) {
      const data = await permResp.json()
      permissions.value = data.permissions ?? []
    }
  } catch {
    // 忽略
  } finally {
    loading.value = false
  }
})

/** 进入修改用户名模式。 */
function startEditUsername() {
  usernameDraft.value = auth.username ?? ''
  keyEnabledDraft.value = keyEnabled.value
  passwordDraft.value = ''
  editingUsername.value = true
}

/** 保存个人信息：用户名 / 密钥可用状态 / 密码一次提交，成功后导航栏立即更新。 */
async function saveUsername() {
  savingUsername.value = true
  try {
    const payload: Record<string, unknown> = {}
    if (usernameDraft.value !== (auth.username ?? '')) {
      payload.username = usernameDraft.value
    }
    if (keyEnabledDraft.value !== keyEnabled.value) {
      payload.key_enabled = keyEnabledDraft.value
    }
    if (passwordDraft.value) {
      payload.password = passwordDraft.value
    }
    const err = await auth.updateOwnProfile(payload)
    if (err) {
      window.alert(err)
      return
    }
    keyEnabled.value = keyEnabledDraft.value
    if (passwordDraft.value) {
      hasPassword.value = true
    }
    editingUsername.value = false
  } catch {
    window.alert('保存失败')
  } finally {
    savingUsername.value = false
  }
}
</script>

<template>
  <div class="profile-section">
    <div class="profile-header">
      <h1 class="admin-content__title">个人信息</h1>
      <template v-if="editingUsername">
        <div class="profile-header__actions">
          <button
            type="button"
            class="manage-btn manage-btn--primary"
            :disabled="savingUsername"
            @click="saveUsername"
          >
            {{ savingUsername ? '保存中...' : '保存编辑' }}
          </button>
          <button
            type="button"
            class="manage-btn"
            :disabled="savingUsername"
            @click="editingUsername = false"
          >
            取消编辑
          </button>
        </div>
      </template>
      <button
        v-else
        type="button"
        class="manage-btn manage-btn--primary"
        :disabled="loading"
        @click="startEditUsername"
      >
        编辑个人信息
      </button>
    </div>

    <div v-if="loading" class="page-loading">加载中...</div>

    <dl v-else class="info-list">
      <div class="info-list__row info-list__row--fixed">
        <dt>用户名</dt>
        <dd>
          <template v-if="editingUsername">
            <input
              v-model="usernameDraft"
              class="info-list__input"
              maxlength="10"
            />
          </template>
          <template v-else>
            <span>{{ auth.username ?? '匿名用户' }}</span>
          </template>
        </dd>
      </div>

      <div class="info-list__row info-list__row--fixed">
        <dt>密钥可用状态</dt>
        <dd>
          <input
            v-if="editingUsername"
            v-model="keyEnabledDraft"
            type="checkbox"
            class="profile-checkbox"
          />
          <input
            v-else
            type="checkbox"
            :checked="keyEnabled"
            disabled
            class="profile-checkbox"
          />
        </dd>
      </div>

      <div class="info-list__row info-list__row--fixed">
        <dt>密码</dt>
        <dd>
          <input
            v-if="editingUsername"
            v-model="passwordDraft"
            type="text"
            class="info-list__input"
            placeholder="留空不修改"
          />
          <span v-else-if="hasPassword">？？？？？？？？？</span>
          <span v-else class="info-list__hint">无</span>
        </dd>
      </div>

      <div class="info-list__row">
        <dt>权限</dt>
        <dd>
          <table class="perm-table">
            <tbody>
              <tr
                v-for="group in permissionGroups"
                :key="group.label"
              >
                <th>{{ group.label }}</th>
                <td>
                  <span v-if="group.perms.length" class="perm-item">
                    {{ group.perms.join('、') }}
                  </span>
                  <span v-else class="info-list__hint">无</span>
                </td>
              </tr>
            </tbody>
          </table>
        </dd>
      </div>
    </dl>
  </div>
</template>

<style scoped>
.profile-section {
  display: flex;
  flex-direction: column;
  min-height: calc(100vh - 144px);
  --checkbox-checked-color: #3366ff;
}

html.dark .profile-section {
  --checkbox-checked-color: #598bff;
}

.admin-content__title {
  margin: 0;
  font-size: 1.5rem;
  color: var(--color-text);
}

.profile-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  margin-bottom: 24px;
}

.profile-header__actions {
  display: flex;
  gap: 8px;
}

.info-list {
  margin: 0;
  max-width: 640px;
}

.info-list__row {
  display: flex;
  align-items: flex-start;
  padding: 14px 0;
  border-bottom: 1px solid var(--color-border);
}

.info-list__row--fixed {
  height: 44px;
  align-items: center;
  padding: 0;
}

.info-list__row:last-child {
  border-bottom: none;
}

.info-list__row dt {
  flex-shrink: 0;
  width: 96px;
  font-size: 0.95rem;
  color: var(--color-text-secondary);
}

.info-list__row dd {
  display: flex;
  flex-wrap: wrap;
  gap: 8px;
  margin: 0;
  font-size: 1rem;
  color: var(--color-text);
}

.info-list__hint {
  font-size: 0.85rem;
  font-style: italic;
  color: var(--color-text-secondary);
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

.info-list__input {
  height: 28px;
  padding: 0 8px;
  border: 1px solid var(--color-border);
  background-color: var(--color-nav-bg);
  font-size: 0.9rem;
  color: var(--color-text);
}

.info-list__input:focus {
  outline: none;
  border-color: var(--color-text-secondary);
}

.profile-checkbox {
  width: 18px;
  height: 18px;
  margin: 0;
  vertical-align: middle;
  appearance: none;
  -webkit-appearance: none;
  border: 2px solid var(--color-text-secondary);
  background-color: var(--color-nav-bg);
  cursor: pointer;
  position: relative;
}

.profile-checkbox:checked {
  border-color: var(--checkbox-checked-color);
  background-color: var(--checkbox-checked-color);
}

.profile-checkbox:checked::after {
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

</style>
