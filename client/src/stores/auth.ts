import { ref } from 'vue'
import { defineStore } from 'pinia'

const STORAGE_KEY = 'auth'

function save(isLoggedIn: boolean, id: number | null, username: string | null, token: string | null, expiresAt: string | null) {
    localStorage.setItem(
        STORAGE_KEY,
        JSON.stringify({ isLoggedIn, id, username, token, expiresAt }),
    )
}

function load() {
    const raw = localStorage.getItem(STORAGE_KEY)
    if (!raw) return { isLoggedIn: false, id: null as number | null, username: null as string | null, token: null as string | null, expiresAt: null as string | null }
    try {
        const { isLoggedIn, id, username, token, expiresAt } = JSON.parse(raw)
        return {
            isLoggedIn: !!isLoggedIn,
            id: typeof id === 'number' ? id : null,
            username: username ?? null,
            token: typeof token === 'string' ? token : null,
            expiresAt: typeof expiresAt === 'string' ? expiresAt : null,
        }
    } catch {
        return { isLoggedIn: false, id: null as number | null, username: null as string | null, token: null as string | null, expiresAt: null as string | null }
    }
}

/**
 * 用户认证状态管理。
 *
 * 登录状态持久化到 localStorage，保存用户 ID 和 session token。
 * 权限鉴别全部在后端完成。
 */
export const useAuthStore = defineStore('auth', () => {
    const stored = load()
    const isLoggedIn = ref(stored.isLoggedIn)
    const id = ref<number | null>(stored.id)
    const username = ref<string | null>(stored.username)
    const token = ref<string | null>(stored.token)
    const expiresAt = ref<string | null>(stored.expiresAt)

    /** 设置登录状态。 */
    function login(uid: number, name: string | null, tok: string, exp: string | null) {
        id.value = uid
        username.value = name
        token.value = tok
        expiresAt.value = exp
        isLoggedIn.value = true
        save(true, uid, username.value, tok, exp)
    }

    /**
     * 退出登录，清除认证状态。
     */
    function logout() {
        id.value = null
        username.value = null
        token.value = null
        expiresAt.value = null
        isLoggedIn.value = false
        localStorage.removeItem(STORAGE_KEY)
    }

    return { isLoggedIn, id, username, token, expiresAt, login, logout }
})
