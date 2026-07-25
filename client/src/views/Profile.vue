<script setup lang="ts">
import { ref, onMounted } from 'vue'
import { useRouter } from 'vue-router'
import { useAuthStore } from '@/stores/auth'

const router = useRouter()
const auth = useAuthStore()

const permissions = ref<string[]>([])
const loading = ref(false)

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

function handleLogout() {
  auth.logout()
  router.push('/')
}
</script>

<template>
  <main class="profile">
    <h1>个人中心</h1>

    <div class="profile-card">
      <p v-if="auth.username" class="profile-card__name">
        {{ auth.username }}
      </p>
      <p v-else class="profile-card__name profile-card__name--anonymous">
        匿名用户
      </p>

      <!-- 权限列表 -->
      <p v-if="loading" class="profile-card__no-perm">加载中...</p>
      <ul v-else-if="permissions.length" class="profile-card__permissions">
        <li
          v-for="perm in permissions"
          :key="perm"
          class="profile-card__perm-item"
        >
          {{ perm }}
        </li>
      </ul>
      <p v-else class="profile-card__no-perm">暂无特殊权限</p>

      <button class="profile-card__logout" @click="handleLogout">
        退出登录
      </button>
    </div>
  </main>
</template>

<style scoped>
.profile {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  min-height: calc(100vh - 80px);
  padding: 40px 20px;
}

.profile h1 {
  margin: 0 0 28px;
  font-size: 1.5rem;
  color: var(--color-text);
}

.profile-card {
  width: 100%;
  max-width: 360px;
  padding: 32px;
  background-color: var(--color-card-bg, var(--color-nav-bg));
  border: 1px solid var(--color-border);
  border-radius: 12px;
  text-align: center;
}

.profile-card__name {
  margin: 0 0 8px;
  font-family: 'FangSong', '仿宋', STFangsong, serif;
  font-size: 1.25rem;
  color: var(--color-text);
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.profile-card__name--anonymous {
  color: var(--color-text-secondary);
}

/* ── 权限列表 ── */

.profile-card__permissions {
  margin: 16px 0 0;
  padding: 0;
  list-style: none;
  border-top: 1px solid var(--color-border);
  padding-top: 12px;
}

.profile-card__perm-item {
  padding: 6px 0;
  font-size: 0.9rem;
  color: var(--color-text-secondary);
}

.profile-card__no-perm {
  margin: 12px 0 0;
  font-size: 0.85rem;
  color: var(--color-text-secondary);
  font-style: italic;
}

/* ── 退出 ── */

.profile-card__logout {
  margin-top: 20px;
  padding: 10px 0;
  width: 100%;
  border: 1px solid var(--color-border);
  border-radius: 8px;
  font-size: 0.95rem;
  color: #e53e3e;
  background: transparent;
  cursor: pointer;
  transition: background-color var(--transition-speed), border-color var(--transition-speed);
}

.profile-card__logout:hover {
  background-color: rgba(229, 62, 62, 0.08);
  border-color: #e53e3e;
}
</style>
