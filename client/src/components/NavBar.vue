<script setup lang="ts">
import { computed } from 'vue'
import { useRouter } from 'vue-router'
import { storeToRefs } from 'pinia'
import { useThemeStore } from '@/stores/theme'
import { useAuthStore } from '@/stores/auth'

const router = useRouter()

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
      <RouterLink to="/favorites" class="header__nav-link">收藏夹</RouterLink>
      <RouterLink to="/about" class="header__nav-link">关于我</RouterLink>
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
