<script setup lang="ts">
import { ref } from 'vue'
import { useRouter } from 'vue-router'
import { useAuthStore } from '@/stores/auth'

const router = useRouter()
const auth = useAuthStore()

const key = ref('')
const error = ref('')

const submitting = ref(false)

/** 提交 Key 登录 */
async function handleSubmit() {
  const trimmed = key.value.trim()
  if (!trimmed) {
    error.value = '请输入密钥'
    return
  }
  error.value = ''
  if (submitting.value) return
  submitting.value = true

  try {
    const resp = await fetch('/api/login/key', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ key: trimmed }),
    })
    const data: { error?: string; id?: number; username?: string | null; token?: string } = await resp.json()
    if (!resp.ok) {
      error.value = data.error || '不存在或已失效的密钥'
      return
    }
    auth.login(data.id!, data.username ?? null, data.token!)
    router.push('/')
  } catch {
    error.value = '网络错误，请稍后重试'
  } finally {
    submitting.value = false
  }
}
</script>

<template>
  <main class="login-page">
    <div class="login-card">
      <h2 class="login-card__title">密钥认证</h2>
      <p class="login-card__desc">使用密钥进行权限认证</p>

      <form class="login-card__form" @submit.prevent="handleSubmit">
        <div class="form-fields">
          <div class="form-field form-field--ghost" aria-hidden="true">
            <span class="form-field__label">&nbsp;</span>
            <div class="form-field__input-row">&nbsp;</div>
          </div>

          <div class="form-field">
            <label class="form-field__label" for="login-key-input">密钥</label>
            <div class="form-field__input-row">
              <input
                id="login-key-input"
                v-model="key"
                class="form-field__input"
              />
            </div>
          </div>
        </div>

        <p class="form-error" v-show="error">{{ error }}</p>

        <button type="submit" class="form-submit" :disabled="submitting">
          {{ submitting ? '登录中...' : '登 录' }}
        </button>
      </form>

      <!-- 跳转账号登录 -->
      <p class="login-card__switch">
        已拥有用户名和密码？使用
        <RouterLink to="/login/password">账号认证</RouterLink>
      </p>
    </div>
  </main>
</template>

<style scoped>
.login-page {
  display: flex;
  align-items: center;
  justify-content: center;
  min-height: calc(100vh - 80px);
  padding: 40px 20px;
}

.login-card {
  display: flex;
  flex-direction: column;
  width: 520px;
  max-width: calc(100vw - 40px);
  min-height: 440px;
  box-sizing: border-box;
  padding: 36px 32px;
  background-color: var(--color-card-bg, var(--color-nav-bg));
  border: 1px solid var(--color-border);
  border-radius: 8px;
}

.login-card__title {
  margin: 0 0 8px;
  font-size: 1.5rem;
  color: var(--color-text);
}

.login-card__desc {
  margin: 0 0 28px;
  font-size: 0.9rem;
  color: var(--color-text-secondary);
}

.login-card__desc--ghost {
  visibility: hidden;
}

.login-card__form {
  flex: 1;
  display: flex;
  flex-direction: column;
  gap: 20px;
}

.form-fields {
  display: flex;
  flex-direction: column;
  justify-content: flex-end;
  gap: 24px;
  height: 108px;
}

.form-field {
  display: flex;
  align-items: center;
  gap: 16px;
}

.form-field--ghost {
  visibility: hidden;
}

.form-field__label {
  flex-shrink: 0;
  width: 84px;
  font-size: 0.95rem;
  font-weight: 600;
  color: var(--color-text);
  text-align: right;
}

.form-field__input-row {
  flex: 1;
  display: flex;
  align-items: center;
  border: 1px solid var(--color-border);
  border-radius: 2px;
  overflow: hidden;
  transition: border-color var(--transition-speed);
}

.form-field__input-row:focus-within {
  border-color: var(--color-text-secondary);
}

.form-field__input {
  flex: 1;
  padding: 10px 14px;
  border: none;
  outline: none;
  font-size: 0.95rem;
  color: var(--color-text);
  background: transparent;
}

.form-field__input::placeholder {
  color: var(--color-text-secondary);
  opacity: 0.6;
}

.form-error {
  min-height: 1.5em;
  line-height: 1.4;
  font-size: 0.85rem;
  color: #e53e3e;
}

.form-submit {
  margin-top: auto;
  align-self: center;
  width: 160px;
  padding: 12px 0;
  border: none;
  border-radius: 2px;
  font-size: 1rem;
  font-weight: 600;
  color: var(--color-card-bg, var(--color-nav-bg));
  background-color: var(--color-text);
  cursor: pointer;
  transition: opacity var(--transition-speed);
}

.form-submit:hover {
  opacity: 0.85;
}

.login-card__switch {
  margin-top: auto;
  padding-top: 24px;
  font-size: 0.875rem;
  text-align: center;
  color: var(--color-text-secondary);
}

.login-card__switch a {
  color: var(--color-text);
  font-weight: 600;
}
</style>
