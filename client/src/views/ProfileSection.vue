<script setup lang="ts">
import { onMounted, ref } from 'vue'
import { useAuthStore } from '@/stores/auth'
import '@/assets/manage/button.css'
import '@/assets/manage/font.css'

const auth = useAuthStore()

const permissions = ref<string[]>([])
const loading = ref(false)
const editingUsername = ref(false)
const savingUsername = ref(false)
const usernameDraft = ref('')

onMounted(async () => {
  if (!auth.id) return
  loading.value = true
  try {
    const resp = await fetch('/api/user/permissions', {
      headers: { 'Authorization': 'Bearer ' + auth.token }
    })
    if (resp.ok) {
      const data = await resp.json()
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
  editingUsername.value = true
}

/** 保存用户名：成功后导航栏立即更新。 */
async function saveUsername() {
  savingUsername.value = true
  try {
    const err = await auth.updateOwnUsername(usernameDraft.value)
    if (err) {
      window.alert(err)
      return
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
  <div>
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
        @click="startEditUsername"
      >
        编辑个人信息
      </button>
    </div>

    <dl class="info-list">
      <div class="info-list__row info-list__row--username">
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

      <div class="info-list__row">
        <dt>权限</dt>
        <dd>
          <span v-if="loading" class="info-list__hint">加载中...</span>
          <template v-else-if="permissions.length">
            <span
              v-for="perm in permissions"
              :key="perm"
              class="perm-item"
            >
              {{ perm }}
            </span>
          </template>
          <span v-else class="info-list__hint">暂无特殊权限</span>
        </dd>
      </div>
    </dl>
  </div>
</template>

<style scoped>
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

.info-list__row--username {
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

.perm-item {
  font-size: 0.85rem;
  color: var(--color-text);
}

.info-list__hint {
  font-size: 0.85rem;
  font-style: italic;
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

</style>
