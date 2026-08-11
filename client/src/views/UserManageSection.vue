<script setup lang="ts">
import { onMounted, ref } from 'vue'
import { useAuthStore } from '@/stores/auth'

interface ManageUser {
  id: number
  username: string | null
  permissions: string[]
}

const auth = useAuthStore()

const users = ref<ManageUser[]>([])
const loading = ref(false)
const error = ref('')

onMounted(async () => {
  loading.value = true
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
  } catch {
    error.value = '加载失败'
  } finally {
    loading.value = false
  }
})
</script>

<template>
  <div>
    <h1 class="admin-content__title">用户管理</h1>

    <p v-if="loading" class="users-hint">加载中...</p>
    <p v-else-if="error" class="users-hint">{{ error }}</p>

    <table v-else-if="users.length" class="users-table">
      <thead>
        <tr>
          <th class="users-table__id">ID</th>
          <th>用户名</th>
          <th>权限列表</th>
        </tr>
      </thead>
      <tbody>
        <tr v-for="user in users" :key="user.id">
          <td class="users-table__id">{{ user.id }}</td>
          <td>{{ user.username ?? '—' }}</td>
          <td>
            <span v-if="user.permissions.length">
              {{ user.permissions.join('、') }}
            </span>
            <span v-else class="users-hint">无</span>
          </td>
        </tr>
      </tbody>
    </table>

    <p v-else class="users-hint">暂无用户</p>
  </div>
</template>

<style scoped>
.admin-content__title {
  margin: 0 0 24px;
  font-size: 1.5rem;
  color: var(--color-text);
}

.users-table {
  width: 100%;
  max-width: 720px;
  border-collapse: collapse;
  text-align: left;
}

.users-table th,
.users-table td {
  padding: 12px 16px;
  border-bottom: 1px solid var(--color-border);
  font-size: 0.95rem;
  color: var(--color-text);
}

.users-table th {
  font-weight: 600;
  color: var(--color-text-secondary);
}

.users-table__id {
  width: 80px;
}

.users-hint {
  margin: 0;
  font-size: 0.9rem;
  color: var(--color-text-secondary);
}
</style>
