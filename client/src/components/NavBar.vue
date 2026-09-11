<script setup lang="ts">
import { computed } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { storeToRefs } from 'pinia'
import { useThemeStore } from '@/stores/theme'
import { useAuthStore } from '@/stores/auth'

const router = useRouter()
const route = useRoute()

const theme = useThemeStore()
const { isDark } = storeToRefs(theme)
const { toggle } = theme

const auth = useAuthStore()
const { isLoggedIn, username } = storeToRefs(auth)

/** 用户按钮跳转目标：未登录 → /login/key，已登录 → /manage */
const userLink = computed(() =>
  isLoggedIn.value ? '/manage' : '/login/key'
)

/** 用户按钮显示文本 */
const userLabel = computed(() => {
  if (!isLoggedIn.value) return '权限认证'
  return username.value ?? '匿名用户'
})

/** 收藏夹下拉菜单当前是否处于激活状态（含其子页面） */
const favoritesActive = computed(() => route.path.startsWith('/favorites'))

/**
 * 鼠标点击下拉菜单触发器/链接后移除焦点，避免菜单因 :focus-within 一直展开。
 * 键盘激活（detail 为 0）不移除焦点，保持键盘可达性。
 */
function onDropdownClick(event: MouseEvent): void {
  if (event.detail > 0) {
    (event.currentTarget as HTMLElement | null)?.blur()
  }
}
</script>

<template>
  <nav class="navbar">
    <!-- Logo -->
    <RouterLink to="/" class="navbar-logo">石木古月</RouterLink>
    <!-- 主题切换按钮 -->
    <button
      class="header__theme-btn"
      @click="toggle"
      :style="{
        maskImage: `url(https://cdn.jsdelivr.net/npm/heroicons@2.1.1/24/solid/${isDark ? 'moon' : 'sun'}.svg)`,
      }"
    ></button>

    <!-- 用户按钮 -->
    <button class="header__user-btn" @click="router.push(userLink)">
      {{ userLabel }}
    </button>

    <div class="navbar-spacer"></div>

    <!-- 导航栏目 -->
    <div class="header__nav">
      <RouterLink to="/" class="header__nav-link">首页</RouterLink>
      <RouterLink to="/blogs" class="header__nav-link">博客</RouterLink>
      <RouterLink to="/projects" class="header__nav-link">项目</RouterLink>
      <!-- 收藏夹下拉菜单 -->
      <div
        class="header__nav-dropdown"
        :class="{ 'header__nav-dropdown--active': favoritesActive }"
      >
        <button
          type="button"
          class="header__nav-link header__nav-dropdown-trigger"
          aria-haspopup="true"
          @click="onDropdownClick"
        >
          收藏夹
        </button>
      </div>
      <RouterLink to="/about" class="header__nav-link">关于我</RouterLink>
      <RouterLink to="/friends" class="header__nav-link">友链推广</RouterLink>
    </div>
    <!-- 分割线 -->
    <div class="header__divider"></div>
    <!-- 个人链接 -->
    <div class="header__social">
      <a
        href="https://github.com/ShimuGuyue"
        target="_blank"
        class="header__social-icon"
        style="mask-image: url(https://cdn.jsdelivr.net/npm/simple-icons@11.15.0/icons/github.svg)"
      ></a>
    </div>
  </nav>
</template>

<style scoped>
.navbar {
  display: flex;
  align-items: center;
  height: 80px;
  padding: 0 32px;
  background-color: var(--color-nav-bg);
  border-bottom: 1px solid var(--color-border);
  box-shadow: 0 1px 4px rgba(0, 0, 0, 0.06);
  transition: all var(--transition-speed);
  position: sticky;
  top: 0;
  z-index: 100;
}

.navbar-logo {
  font-size: 1.8rem;
  font-weight: 700;
  color: var(--color-text);
  text-decoration: none;
  transition: color var(--transition-speed);
}

.header__theme-btn {
  width: 24px;
  height: 24px;
  margin-left: 16px;
  border: none;
  background-color: var(--color-text-secondary);
  mask-size: contain;
  mask-repeat: no-repeat;
  mask-position: center;
  -webkit-mask-size: contain;
  -webkit-mask-repeat: no-repeat;
  -webkit-mask-position: center;
  cursor: pointer;
  transition: background-color var(--transition-speed);
}

.header__theme-btn:hover {
  background-color: var(--color-text);
}

.header__user-btn {
  margin-left: 12px;
  min-width: 10ch;
  padding: 4px 8px;
  font-family: 'FangSong', '仿宋', STFangsong, serif;
  font-size: 0.875rem;
  color: var(--color-text-secondary);
  text-align: center;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
  background: none;
  cursor: pointer;
  border: 1px solid var(--color-border);
  border-radius: 6px;
  transition: color var(--transition-speed), border-color var(--transition-speed);
}

.header__user-btn:hover {
  color: var(--color-text);
  border-color: var(--color-text-secondary);
}

.navbar-spacer {
  flex: 1;
}

.header__nav {
  align-self: stretch;
  display: flex;
  align-items: center;
  gap: 80px;
}

.header__nav-link {
  font-size: 1.125rem;
  color: var(--color-text-secondary);
  text-decoration: none;
  transition: color var(--transition-speed);
}

.header__nav-link:hover {
  color: var(--color-text);
}

.header__nav-link.router-link-active {
  font-weight: 700;
  color: var(--color-text);
}

/* 收藏夹下拉菜单：容器撑满导航栏高度，菜单紧贴导航栏底部，悬停时无空隙 */
.header__nav-dropdown {
  position: relative;
  display: flex;
  align-items: center;
  align-self: stretch;
}

.header__nav-dropdown-trigger {
  display: flex;
  align-items: center;
  height: 100%;
  padding: 0;
  border: none;
  background: none;
  font-family: inherit;
  cursor: pointer;
}

.header__nav-dropdown--active .header__nav-link {
  font-weight: 700;
  color: var(--color-text);
}

.header__nav-dropdown-menu {
  position: absolute;
  top: 100%;
  left: 50%;
  display: flex;
  flex-direction: column;
  min-width: 120px;
  padding: 6px 0;
  background-color: var(--color-nav-bg);
  border: 1px solid var(--color-border);
  /* 去掉上边框，菜单与导航栏连成一体，不留横向接缝 */
  border-top: none;
  border-radius: 0;
  opacity: 0;
  visibility: hidden;
  transform: translateX(-50%);
}

.header__nav-dropdown:hover .header__nav-dropdown-menu,
.header__nav-dropdown:focus-within .header__nav-dropdown-menu {
  opacity: 1;
  visibility: visible;
}

.header__nav-dropdown-item {
  padding: 8px 20px;
  font-size: 1.125rem;
  color: var(--color-text-secondary);
  text-align: center;
  white-space: nowrap;
  text-decoration: none;
}

.header__nav-dropdown-item:hover {
  color: var(--color-text);
}

.header__nav-dropdown-item.router-link-active {
  font-weight: 700;
  color: var(--color-text);
}

.header__divider {
  width: 1px;
  height: 24px;
  margin: 0 40px;
  background-color: var(--color-border);
  transition: background-color var(--transition-speed);
}

.header__social {
  display: flex;
  align-items: center;
  gap: 12px;
}

.header__social-icon {
  display: block;
  width: 24px;
  height: 24px;
  background-color: var(--color-text-secondary);
  mask-size: contain;
  mask-repeat: no-repeat;
  mask-position: center;
  -webkit-mask-size: contain;
  -webkit-mask-repeat: no-repeat;
  -webkit-mask-position: center;
  transition: background-color var(--transition-speed);
}

.header__social-icon:hover {
  background-color: var(--color-text);
}
</style>
