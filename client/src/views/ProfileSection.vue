<script setup lang="ts">
import { onMounted, ref } from 'vue'
import { useAuthStore } from '@/stores/auth'

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
</script>

<template>
  <div>
    <h1 class="admin-content__title">个人信息</h1>

    <dl class="info-list">
      <div class="info-list__row">
        <dt>用户名</dt>
        <dd>{{ auth.username ?? '匿名用户' }}</dd>
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
  margin: 0 0 24px;
  font-size: 1.5rem;
  color: var(--color-text);
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
</style>
