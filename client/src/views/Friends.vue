<script setup lang="ts">
import { ref, onMounted } from 'vue'

import '@/assets/background/block.css'

interface FriendLink {
  name: string
  url: string
  description: string
  image: string
  status?: 'online' | 'offline'
}

const friends = ref<FriendLink[]>([])
const loading = ref(true)

async function fetchFriends() {
  try {
    const resp = await fetch('/api/friends')
    if (!resp.ok) {
      throw new Error(`HTTP ${resp.status}`)
    }
    friends.value = await resp.json()
    friends.value.forEach(checkFriendStatus)
  } catch (e) {
    console.error('获取友链失败:', e)
    friends.value = []
  } finally {
    loading.value = false
  }
}

onMounted(fetchFriends)

/** 探测友链站点是否可访问：no-cors 请求能建立连接即视为 ONLINE，超时或失败为 OFFLINE。 */
async function checkFriendStatus(friend: FriendLink) {
  try {
    await fetch(friend.url, {
      mode: 'no-cors',
      signal: AbortSignal.timeout(8000),
    })
    friend.status = 'online'
  } catch {
    friend.status = 'offline'
  }
}

</script>

<template>
  <main class="friends-page">
    <!-- 页面标题 -->
    <header class="friends-header">
      <h1 class="friends-header__title">友情链接</h1>
      <p class="friends-header__subtitle">
        分享更多朋友们的优秀链接，欢迎在
        <a href="https://github.com/ShimuGuyue/shimuguyue.love/issues/new?template=add_friend_link.yml" target="_blank" rel="noopener noreferrer">此处</a>
        创建友链添加申请。
      </p>
    </header>

    <!-- 友链卡片网格 -->
    <p v-if="loading" class="friends-status">加载中...</p>
    <p v-else-if="!friends.length" class="friends-status">暂无友链，期待与有趣的人互链。</p>
    <section v-else class="blog-grid">
      <article
        v-for="friend in friends"
        :key="friend.url"
        class="friend-card"
      >
        <div class="friend-card__head">
          <!-- 左侧圆形头像：非 1:1 时取中间部分 -->
          <img
            v-if="friend.image"
            class="friend-card__avatar"
            :src="friend.image"
            :alt="friend.name"
            loading="lazy"
          />
          <div v-else class="friend-card__avatar friend-card__avatar--fallback">
            {{ friend.name.charAt(0) }}
          </div>
          <!-- 右侧两行：站点名称 + 站点链接 -->
          <div class="friend-card__info">
            <div class="friend-card__name">{{ friend.name }}</div>
            <a
              class="friend-card__url"
              :href="friend.url"
              :title="friend.url"
              target="_blank"
              rel="noopener noreferrer"
            >
              {{ friend.url }}
            </a>
            <p
              class="friend-card__status"
              :class="`friend-card__status--${friend.status === 'offline' ? 'offline' : 'online'}`"
            >
              {{ friend.status === 'offline' ? 'OFFLINE' : 'ONLINE' }}
            </p>
          </div>
        </div>
        <!-- 描述：固定两行位置 -->
        <p class="friend-card__desc">{{ friend.description }}</p>
      </article>
    </section>
  </main>
</template>

<style scoped>
.friends-page {
  max-width: 1200px;
  margin: 0 auto;
  padding: 24px 32px 48px;
}

.friends-header {
  margin-bottom: 28px;
  text-align: center;
}

.friends-header__title {
  margin: 0;
  font-size: 1.8rem;
  font-weight: 700;
  color: var(--color-text);
}

.friends-header__subtitle {
  margin: 8px 0 0;
  font-size: 0.95rem;
  color: var(--color-text-secondary);
}

.friends-status {
  text-align: center;
  padding: 48px 0;
  font-size: 0.95rem;
  color: var(--color-text-secondary);
}

.blog-grid {
  display: grid;
  grid-template-columns: repeat(3, 1fr);
  gap: 20px;
}

/* ── 友链卡片 ── */
.friend-card {
  display: flex;
  flex-direction: column;
  padding: var(--blog-surface-padding);
  background-color: var(--blog-surface-bg);
  border: 1px solid var(--color-border);
  border-radius: 30px;
  box-shadow: var(--blog-surface-shadow);
}

.friend-card__head {
  display: flex;
  align-items: center;
  gap: 14px;
  margin-bottom: 12px;
}

/* 头像尺寸等于右侧两行（名称 + 链接）的总高度 */
.friend-card__avatar {
  flex-shrink: 0;
  width: 60px;
  height: 60px;
  border-radius: 50%;
  object-fit: cover;
  object-position: center;
  overflow: hidden;
}

.friend-card__avatar--fallback {
  display: flex;
  align-items: center;
  justify-content: center;
  color: #fff;
  font-size: 1.2rem;
  font-weight: 700;
  background-color: var(--pink-hot);
}

.friend-card__info {
  display: flex;
  flex-direction: column;
  min-width: 0;
}

.friend-card__name {
  font-size: 1.05rem;
  font-weight: 600;
  color: var(--color-text);
  line-height: 1.6;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.friend-card__url {
  font-size: 0.85rem;
  line-height: 1.6;
  overflow-wrap: anywhere;
}

/* 网站状态：可访问绿色 ONLINE，不可访问红色 OFFLINE */
.friend-card__status {
  margin: 0;
  font-size: 0.75rem;
  font-weight: 600;
  letter-spacing: 0.5px;
}

.friend-card__status--online {
  color: #22c55e;
}

.friend-card__status--offline {
  color: #ef4444;
}

/* 描述：固定两行，超出裁剪，不足时仍占两行位置 */
.friend-card__desc {
  margin: 0;
  font-size: 0.85rem;
  color: var(--color-text-secondary);
  line-height: 1.5;
  display: -webkit-box;
  -webkit-line-clamp: 2;
  line-clamp: 2;
  -webkit-box-orient: vertical;
  overflow: hidden;
  min-height: calc(1.5em * 2);
}

@media (max-width: 768px) {
  .blog-grid {
    grid-template-columns: 1fr;
  }
}
</style>
