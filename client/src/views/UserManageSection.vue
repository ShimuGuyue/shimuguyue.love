<script setup lang="ts">
import { computed, onMounted, ref, watch } from 'vue'
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

/** 分页状态 */
const perPageOptions = [5, 10, 20, 50]
const perPage = ref(10)
const page = ref(1)

const pageCount = computed(() =>
  Math.max(1, Math.ceil(users.value.length / perPage.value))
)

const pageNumbers = computed(() =>
  Array.from({ length: pageCount.value }, (_, i) => i + 1)
)

const pagedUsers = computed(() => {
  const start = (page.value - 1) * perPage.value
  return users.value.slice(start, start + perPage.value)
})

watch([perPage, users], () => {
  if (page.value > pageCount.value) {
    page.value = pageCount.value
  }
})

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

    <template v-else-if="users.length">
      <table class="users-table">
        <thead>
          <tr>
            <th class="users-table__id">ID</th>
            <th>用户名</th>
            <th>权限列表</th>
          </tr>
        </thead>
        <tbody>
          <tr v-for="user in pagedUsers" :key="user.id">
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

      <div class="users-pager">
        <label class="users-pager__per-page">
          每页
          <select
            v-model.number="perPage"
            class="users-pager__select"
          >
            <option
              v-for="size in perPageOptions"
              :key="size"
              :value="size"
            >
              {{ size }}
            </option>
          </select>
          条
        </label>

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
  border-collapse: collapse;
  text-align: left;
}

.users-table th,
.users-table td {
  padding: 10px 14px;
  border: 1px solid var(--color-border);
  font-size: 0.95rem;
  color: var(--color-text);
}

.users-table th {
  font-weight: 600;
  color: var(--color-text-secondary);
  background-color: var(--color-hover);
}

.users-table__id {
  width: 80px;
}

.users-hint {
  margin: 0;
  font-size: 0.9rem;
  color: var(--color-text-secondary);
}

/* ── 分页 ── */

.users-pager {
  display: flex;
  align-items: center;
  justify-content: space-between;
  width: 100%;
  margin-top: 16px;
  font-size: 0.9rem;
  color: var(--color-text-secondary);
}

.users-pager__select {
  margin: 0 4px;
  padding: 4px 8px;
  border: 1px solid var(--color-border);
  background-color: var(--color-nav-bg);
  color: var(--color-text);
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
</style>
