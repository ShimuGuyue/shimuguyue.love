<script setup lang="ts">
import { computed, onMounted, ref } from 'vue'
import { useRouter } from 'vue-router'
import { useAuthStore } from '@/stores/auth'

/** 后台管理栏目定义。 */
interface AdminSection {
  key: string
  label: string
}

const router = useRouter()
const auth = useAuthStore()

/** 栏目列表：目前先提供「个人信息」，后续栏目在此追加。 */
const sections: AdminSection[] = [
  { key: 'profile', label: '个人信息' },
]

const activeSection = ref<string>('profile')
const permissions = ref<string[]>([])
const loading = ref(false)

/** 当前激活栏目的标题。 */
const activeLabel = computed(
  () => sections.find((section) => section.key === activeSection.value)?.label ?? ''
)

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

/** 点击左侧栏目时切换内容区。 */
function selectSection(key: string) {
  activeSection.value = key
}

function handleLogout() {
  auth.logout()
  router.push('/')
}
</script>

<template>
  <main class="admin">
    <!-- 最左侧栏目栏 -->
    <aside class="admin-sidebar">
      <h2 class="admin-sidebar__title">后台管理</h2>

      <nav class="admin-sidebar__nav">
        <button
          v-for="section in sections"
          :key="section.key"
          type="button"
          class="admin-sidebar__item"
          :class="{ 'admin-sidebar__item--active': activeSection === section.key }"
          @click="selectSection(section.key)"
        >
          {{ section.label }}
        </button>
      </nav>

      <button type="button" class="admin-sidebar__logout" @click="handleLogout">
        退出登录
      </button>
    </aside>

    <!-- 右侧内容区 -->
    <section class="admin-content">
      <h1 class="admin-content__title">{{ activeLabel }}</h1>

      <dl v-if="activeSection === 'profile'" class="info-list">
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
    </section>
  </main>
</template>

<style scoped>
/* 纯色背景铺满视口，覆盖全局渐变背景 */
.admin {
  display: flex;
  min-height: calc(100vh - 80px);
  background-color: var(--color-bg);
}

/* ── 最左侧栏目栏 ── */

.admin-sidebar {
  display: flex;
  flex-direction: column;
  flex-shrink: 0;
  width: 220px;
  padding: 24px 16px;
  border-right: 1px solid var(--color-border);
}

.admin-sidebar__title {
  margin: 0 0 24px;
  padding: 0 8px;
  font-size: 1rem;
  font-weight: 700;
  letter-spacing: 0.05em;
  color: var(--color-text-secondary);
}

.admin-sidebar__nav {
  display: flex;
  flex-direction: column;
  gap: 4px;
}

.admin-sidebar__item {
  padding: 10px 12px;
  border: none;
  background: transparent;
  font-size: 0.95rem;
  text-align: left;
  color: var(--color-text-secondary);
  cursor: pointer;
  transition:
    background-color var(--transition-speed),
    color var(--transition-speed);
}

.admin-sidebar__item:hover {
  background-color: var(--color-hover);
  color: var(--color-text);
}

.admin-sidebar__item--active {
  background-color: var(--color-hover);
  color: var(--color-text);
  font-weight: 600;
}

.admin-sidebar__logout {
  margin-top: auto;
  padding: 10px 12px;
  border: 1px solid var(--color-border);
  background: transparent;
  font-size: 0.9rem;
  color: #e53e3e;
  cursor: pointer;
  transition:
    background-color var(--transition-speed),
    border-color var(--transition-speed);
}

.admin-sidebar__logout:hover {
  background-color: rgba(229, 62, 62, 0.08);
  border-color: #e53e3e;
}

/* ── 右侧内容区 ── */

.admin-content {
  flex: 1;
  min-width: 0;
  padding: 32px 40px;
}

.admin-content__title {
  margin: 0 0 24px;
  font-size: 1.5rem;
  color: var(--color-text);
}

/* ── 信息列表 ── */

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

/* ── 窄屏：栏目栏移到顶部 ── */

@media (max-width: 720px) {
  .admin {
    flex-direction: column;
  }

  .admin-sidebar {
    width: 100%;
    border-right: none;
    border-bottom: 1px solid var(--color-border);
  }

  .admin-sidebar__nav {
    flex-direction: row;
    flex-wrap: wrap;
  }

  .admin-sidebar__logout {
    margin-top: 16px;
  }

  .admin-content {
    padding: 24px 20px;
  }
}
</style>
