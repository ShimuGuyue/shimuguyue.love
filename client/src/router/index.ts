import { createRouter, createWebHistory } from 'vue-router'
import { useAuthStore } from '@/stores/auth'
import Home from '@/views/Home.vue'
import Blogs from '@/views/Blogs.vue'
import Projects from '@/views/Projects.vue'
import FavoritesMusic from '@/views/FavoritesMusic.vue'
import FavoritesPhoto from '@/views/FavoritesPhoto.vue'
import FavoritesWebsite from '@/views/FavoritesWebsite.vue'
import FavoritesOpensource from '@/views/FavoritesOpensource.vue'
import FavoritesAnime from '@/views/FavoritesAnime.vue'
import FavoritesGame from '@/views/FavoritesGame.vue'
import Friends from '@/views/Friends.vue'
import LoginKey from '@/views/LoginKey.vue'
import LoginPassword from '@/views/LoginPassword.vue'
import Manage from '@/views/Manage.vue'
import ProfileSection from '@/views/ProfileSection.vue'
import UserManageSection from '@/views/UserManageSection.vue'
import BlogManageSection from '@/views/BlogManageSection.vue'
import Acknowledgments from '@/views/Acknowledgments.vue'

const router = createRouter({
  history: createWebHistory(import.meta.env.BASE_URL),
  routes: [
    { path: '/', name: 'home', component: Home },
    { path: '/blogs', name: 'blogs', component: Blogs },
    { path: '/projects', name: 'projects', component: Projects },
    { path: '/favorites/music', name: 'favorites-music', component: FavoritesMusic },
    { path: '/favorites/photo', name: 'favorites-photo', component: FavoritesPhoto },
    { path: '/favorites/website', name: 'favorites-website', component: FavoritesWebsite },
    { path: '/favorites/opensource', name: 'favorites-opensource', component: FavoritesOpensource },
    { path: '/favorites/anime', name: 'favorites-anime', component: FavoritesAnime },
    { path: '/favorites/game', name: 'favorites-game', component: FavoritesGame },
    { path: '/friends', name: 'friends', component: Friends },
    { path: '/about', name: 'about', component: () => import('@/views/About.vue') },
    { path: '/thanks', name: 'acknowledgments', component: Acknowledgments },
    { path: '/login/key', name: 'login-key', component: LoginKey },
    { path: '/login/password', name: 'login-password', component: LoginPassword },
    {
      path: '/manage',
      name: 'manage',
      component: Manage,
      redirect: '/manage/profile',
      children: [
        {
          path: 'profile',
          name: 'manage-profile',
          component: ProfileSection,
        },
        {
          path: 'users',
          name: 'manage-users',
          component: UserManageSection,
          meta: { requiresPermission: 'manage:view' },
        },
        {
          path: 'blogs',
          name: 'manage-blogs',
          component: BlogManageSection,
          meta: { requiresPermission: 'manage:view' },
        },
      ],
    },
    { path: '/blog-edit/new', name: 'blog-edit-new', component: () => import('@/views/BlogEdit.vue') },
    { path: '/blog-edit/:file_path(.*)', name: 'blog-edit', component: () => import('@/views/BlogEdit.vue') },
    { path: '/blogs/:file_path(.*)', name: 'blog-detail', component: () => import('@/views/BlogDetail.vue') },
  ],
})

/** 拉取当前用户权限列表。 */
async function fetchPermissions(token: string | null): Promise<string[]> {
  if (!token) return []
  try {
    const resp = await fetch('/api/user/permissions', {
      headers: { 'Authorization': 'Bearer ' + token }
    })
    if (!resp.ok) return []
    const data = await resp.json()
    return data.permissions ?? []
  } catch {
    return []
  }
}

/** 后台管理页仅允许已登录用户访问，带权限要求的栏目再校验权限。 */
router.beforeEach(async (to) => {
  const auth = useAuthStore()
  if (to.path.startsWith('/manage') && !auth.isLoggedIn) {
    window.alert('该页面仅登录用户可访问，请先登录。')
    return { name: 'login-key' }
  }
  const required = to.meta.requiresPermission as string | undefined
  if (required) {
    const permissions = await fetchPermissions(auth.token)
    if (!permissions.includes(required)) {
      window.alert(`操作失败：进入该页面需要 ${required} 权限`)
      return { name: 'manage-profile' }
    }
  }
  return true
})

export default router
