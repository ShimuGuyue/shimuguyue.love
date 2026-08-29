<script setup lang="ts">
import { onMounted, ref } from 'vue'
import { useAuthStore } from '@/stores/auth'

import '@/assets/manage/font.css'
import '@/assets/manage/table.css'

/** 友链记录：来自 GET /api/friends，image 为空串表示未匹配到头像。 */
interface FriendLink {
  name: string
  url: string
  description: string
  image: string
}

const auth = useAuthStore()

const friends = ref<FriendLink[]>([])
const loading = ref(false)
const error = ref('')

async function loadFriends() {
  loading.value = true
  error.value = ''
  try {
    const resp = await fetch('/api/friends', {
      headers: { 'Authorization': 'Bearer ' + auth.token }
    })
    if (!resp.ok) {
      const data = await resp.json().catch(() => ({}))
      error.value = data.error ?? '加载失败'
      return
    }
    friends.value = await resp.json() as FriendLink[]
  } catch {
    error.value = '加载失败'
  } finally {
    loading.value = false
  }
}

onMounted(loadFriends)
</script>

<template>
  <div class="friends-section">
    <div class="friends-header">
      <h1 class="admin-content__title">友链管理</h1>
    </div>

    <div v-if="loading" class="page-loading">加载中...</div>
    <p v-else-if="error" class="friends-hint">{{ error }}</p>

    <template v-else-if="friends.length">
      <div class="blogs-table-wrap">
        <table class="blogs-table friends-table">
          <thead>
            <tr>
              <th class="friends-table__image">头像图片</th>
              <th class="friends-table__name">站点名</th>
              <th class="friends-table__url">站点链接</th>
              <th class="friends-table__desc">站点描述</th>
            </tr>
          </thead>
          <tbody>
            <tr v-for="friend in friends" :key="friend.name">
              <td class="friends-table__image">
                <img
                  v-if="friend.image"
                  class="friends-table__avatar"
                  :src="friend.image"
                  :alt="friend.name"
                  loading="lazy"
                />
                <div
                  v-else
                  class="friends-table__avatar friends-table__avatar--fallback"
                >
                  {{ friend.name.charAt(0) }}
                </div>
              </td>
              <td class="friends-table__name">
                <span :title="friend.name">{{ friend.name }}</span>
              </td>
              <td class="friends-table__url">
                <a
                  :href="friend.url"
                  target="_blank"
                  rel="noopener noreferrer"
                  class="friends-table__link"
                  :title="friend.url"
                >
                  {{ friend.url }}
                </a>
              </td>
              <td class="friends-table__desc">
                <span :title="friend.description">{{ friend.description }}</span>
              </td>
            </tr>
          </tbody>
        </table>
      </div>
    </template>

    <p v-else class="friends-hint">暂无友链</p>
  </div>
</template>

<style scoped>
.friends-section {
  display: flex;
  flex-direction: column;
  min-height: calc(100vh - 144px);
}

.friends-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  margin-bottom: 16px;
}

.admin-content__title {
  margin: 0;
  font-size: 1.5rem;
  color: var(--color-text);
}

.page-loading {
  display: flex;
  flex: 1;
  align-items: center;
  justify-content: center;
  margin: 0;
  font-size: 0.95rem;
  color: var(--color-text-secondary);
}

.friends-hint {
  margin: 0;
  font-size: 0.9rem;
  color: var(--color-text-secondary);
}

/* 复用博客管理表格外观；友链数量较少，行高自适应并容纳头像。 */
.blogs-table.friends-table {
  height: auto;
}

.friends-table td {
  height: 76px;
}

/* ── 列宽 ── */

/* 头像图片列：收紧左右内边距，宽度刚好包住缩略图 */
.friends-table__image {
  width: 6%;
  padding: 0 4px;
  text-align: center;
}

.friends-table__name {
  width: 14%;
}

.friends-table__url {
  width: 25%;
}

.friends-table__desc {
  width: 55%;
}

/* 站点链接：下划线可点击 */
.friends-table__link {
  color: var(--color-text);
  text-decoration: underline;
  text-underline-offset: 2px;
}

.friends-table__link:hover {
  color: var(--color-text-secondary);
}

/* 头像缩略图：圆形，居中裁切；无图时显示站点名首字符占位 */
.friends-table__avatar {
  display: inline-block;
  width: 56px;
  height: 56px;
  border-radius: 50%;
  object-fit: cover;
  object-position: center;
  overflow: hidden;
  vertical-align: middle;
}

.friends-table__avatar--fallback {
  display: flex;
  align-items: center;
  justify-content: center;
  color: #fff;
  font-size: 1.2rem;
  font-weight: 700;
  background-color: var(--pink-hot);
}
</style>
