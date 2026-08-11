<script setup lang="ts">
import { useRouter } from 'vue-router'
import { useAuthStore } from '@/stores/auth'

/** 后台管理栏目定义。 */
interface AdminSection {
  key: string
  label: string
  path: string
}

const router = useRouter()
const auth = useAuthStore()

/** 栏目列表：每个栏目对应一个子路由，后续栏目在此追加。 */
const sections: AdminSection[] = [
  { key: 'profile', label: '个人信息', path: '/manage/profile' },
  { key: 'users', label: '用户管理', path: '/manage/users' },
]

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
        <RouterLink
          v-for="section in sections"
          :key="section.key"
          :to="section.path"
          class="admin-sidebar__item"
        >
          {{ section.label }}
        </RouterLink>
      </nav>

      <button type="button" class="admin-sidebar__logout" @click="handleLogout">
        退出登录
      </button>
    </aside>

    <!-- 右侧内容区 -->
    <section class="admin-content">
      <RouterView />
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
  display: block;
  padding: 10px 12px;
  border: none;
  background: transparent;
  font-size: 0.95rem;
  text-align: left;
  text-decoration: none;
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

.admin-sidebar__item.router-link-exact-active {
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
