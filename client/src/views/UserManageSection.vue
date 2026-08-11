<script setup lang="ts">
import { computed, onMounted, ref, watch } from 'vue'
import { useAuthStore } from '@/stores/auth'

interface ManageUser {
  id: number
  username: string | null
  key_enabled: boolean
  has_password: boolean
  permissions: string[]
}

const auth = useAuthStore()

const users = ref<ManageUser[]>([])
const loading = ref(false)
const error = ref('')

/** 分页：每页固定 15 条 */
const PAGE_SIZE = 15
const page = ref(1)

const pageCount = computed(() =>
  Math.max(1, Math.ceil(users.value.length / PAGE_SIZE))
)

const pageNumbers = computed(() =>
  Array.from({ length: pageCount.value }, (_, i) => i + 1)
)

/** 当前页数据：不足 15 条时用空行补齐。 */
const pagedUsers = computed<(ManageUser | null)[]>(() => {
  const start = (page.value - 1) * PAGE_SIZE
  return Array.from(
    { length: PAGE_SIZE },
    (_, i) => users.value[start + i] ?? null
  )
})

watch([users, pageCount], () => {
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
      <div class="users-table-wrap">
        <table class="users-table">
          <thead>
            <tr>
              <th class="users-table__id">ID</th>
              <th>用户名</th>
              <th class="users-table__mask">密钥</th>
              <th class="users-table__status">密钥可用状态</th>
              <th class="users-table__mask">密码</th>
              <th>权限列表</th>
            </tr>
          </thead>
          <tbody>
            <tr v-for="(user, index) in pagedUsers" :key="index">
              <td class="users-table__id">{{ user?.id ?? '' }}</td>
              <td>
                <span v-if="user && user.username">{{ user.username }}</span>
                <span v-else-if="user" class="users-hint">无</span>
              </td>
              <td class="users-table__mask">{{ user ? '？？？？？？？？？' : '' }}</td>
              <td class="users-table__status">
                <input
                  v-if="user"
                  type="checkbox"
                  :checked="user.key_enabled"
                  disabled
                />
              </td>
              <td>
                <span v-if="user && user.has_password">？？？？？？？？？</span>
                <span v-else-if="user" class="users-hint">无</span>
              </td>
              <td>
                <span v-if="user && user.permissions.length">
                  {{ user.permissions.join('、') }}
                </span>
                <span v-else-if="user" class="users-hint">无</span>
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
  /* 固定高度：表头 + 15 行，每行 44px */
  height: calc(44px * 16);
  border-collapse: collapse;
  text-align: left;
}

.users-table th,
.users-table td {
  height: 44px;
  padding: 0 14px;
  border: 1px solid var(--color-border);
  font-family: 'FangSong', '仿宋', STFangsong, serif;
  font-size: 0.95rem;
  color: var(--color-text);
  white-space: nowrap;
}

.users-table th {
  font-weight: 600;
  color: var(--color-text-secondary);
  background-color: var(--color-hover);
}

.users-table__id {
  width: 80px;
}

.users-table__status {
  width: 60px;
  text-align: center;
}

.users-table__mask {
  width: 9em;
}

/* 自定义复选框：状态更清晰 */
.users-table {
  --checkbox-checked-color: #3366ff;
}

html.dark .users-table {
  --checkbox-checked-color: #598bff;
}

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

.users-hint {
  margin: 0;
  font-size: 0.9rem;
  color: var(--color-text-secondary);
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
</style>
