<script setup lang="ts">
import { ref } from 'vue'
import { useRouter } from 'vue-router'
import { useAuthStore } from '@/stores/auth'

const router = useRouter()
const auth = useAuthStore()

const username = ref('')
const pwd = ref('')
const error = ref('')
const submitting = ref(false)

/** 提交用户名密码登录 */
async function handleSubmit() {
  const name = username.value.trim()
  if (!name) {
    error.value = '请输入用户名'
    return
  }
  if (name.length > 10) {
    error.value = '用户名不能超过 10 个字符'
    return
  }
  if (!pwd.value) {
    error.value = '请输入密码'
    return
  }
  error.value = ''
  if (submitting.value) return
  submitting.value = true

  try {
    const resp = await fetch('/api/login/password', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ username: name, password: pwd.value }),
    })
    const data: { error?: string; id?: number; username?: string; token?: string } = await resp.json()
    if (!resp.ok) {
      error.value = data.error || '用户名或密码错误'
      return
    }
    auth.login(data.id!, data.username || name, data.token!)
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
      <h2 class="login-card__title">账号认证</h2>
      <p class="login-card__desc">使用帐号进行登录</p>

      <form class="login-card__form" @submit.prevent="handleSubmit">
        <div class="form-fields">
          <div class="form-field">
            <label class="form-field__label" for="login-username">用户名</label>
            <input
              id="login-username"
              v-model="username"
              type="text"
              class="form-field__input form-field__input--standalone"
              maxlength="10"
              autocomplete="username"
            />
          </div>

          <div class="form-field">
            <label class="form-field__label" for="login-pwd">密码</label>
            <div class="form-field__input-row">
              <input
                id="login-pwd"
                v-model="pwd"
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

      <!-- 跳转密钥登录 -->
      <p class="login-card__switch">
        还没有设置用户名和密码？使用
        <RouterLink to="/login/key">密钥登录</RouterLink>
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

/* 独立输入框（无切换按钮） */
.form-field__input--standalone {
  flex: 1;
  border: 1px solid var(--color-border);
  border-radius: 2px;
  padding: 10px 14px;
  font-size: 0.95rem;
  color: var(--color-text);
  background: transparent;
  transition: border-color var(--transition-speed);
}

.form-field__input--standalone:focus {
  border-color: var(--color-text-secondary);
  outline: none;
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
