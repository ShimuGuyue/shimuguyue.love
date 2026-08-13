import { createRouter, createWebHistory } from 'vue-router'
import { useAuthStore } from '@/stores/auth'
import Home from '@/views/Home.vue'
import Blogs from '@/views/Blogs.vue'
import Projects from '@/views/Projects.vue'
import Favorites from '@/views/Favorites.vue'
import About from '@/views/About.vue'
import LoginKey from '@/views/LoginKey.vue'
import LoginPassword from '@/views/LoginPassword.vue'
import Manage from '@/views/Manage.vue'
import ProfileSection from '@/views/ProfileSection.vue'
import UserManageSection from '@/views/UserManageSection.vue'
import BlogDetail from '@/views/BlogDetail.vue'
import BlogEdit from '@/views/BlogEdit.vue'
import Acknowledgments from '@/views/Acknowledgments.vue'

const router = createRouter({
  history: createWebHistory(import.meta.env.BASE_URL),
  routes: [
    { path: '/', name: 'home', component: Home },
    { path: '/blogs', name: 'blogs', component: Blogs },
    { path: '/projects', name: 'projects', component: Projects },
    { path: '/favorites', name: 'favorites', component: Favorites },
    { path: '/about', name: 'about', component: About },
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
          meta: { requiresPermission: 'manage_view' },
        },
      ],
    },
    { path: '/blog-edit/new', name: 'blog-edit-new', component: BlogEdit },
    { path: '/blog-edit/:file_path(.*)', name: 'blog-edit', component: BlogEdit },
    { path: '/blogs/:file_path(.*)', name: 'blog-detail', component: BlogDetail },
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
