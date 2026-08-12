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
     * 更新当前登录用户的用户名并持久化（修改后导航栏立即生效）。
     */
    function setUsername(name: string | null) {
        username.value = name
        save(isLoggedIn.value, id.value, name, token.value, expiresAt.value)
    }

    /**
     * 修改当前登录用户的用户名。
     *
     * 调用后端接口更新，成功后立即更新本地用户名并持久化。
     *
     * @param name 新用户名；空字符串表示清空用户名。
     * @return 成功返回 null，失败返回错误消息。
     */
    async function updateOwnUsername(name: string): Promise<string | null> {
        const resp = await fetch('/api/manage/user/update', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
                'Authorization': 'Bearer ' + token.value,
            },
            body: JSON.stringify({ id: id.value, username: name }),
        })
        const data = await resp.json().catch(() => ({}))
        if (!resp.ok) {
            return data.error ?? '保存失败'
        }
        setUsername(name === '' ? null : name)
        return null
    }

    /**
     * 从后端一次性刷新当前登录用户的用户名。
     *
     * 与本地不一致时更新并持久化；由保存操作后调用，不做定时轮询。
     */
    async function refreshUsername(): Promise<void> {
        if (!isLoggedIn.value || !token.value) return
        try {
            const resp = await fetch('/api/user/info', {
                headers: { 'Authorization': 'Bearer ' + token.value },
            })
            if (!resp.ok) return
            const data = await resp.json()
            const name: string | null =
                typeof data.username === 'string' ? data.username : null
            if (name !== username.value) {
                setUsername(name)
            }
        } catch {
            // 静默忽略
        }
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

    return { isLoggedIn, id, username, token, expiresAt, login, logout, setUsername, updateOwnUsername, refreshUsername }
})
