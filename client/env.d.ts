/// <reference types="vite/client" />

/** 由 client/vite.config.ts 注入：《关于我》README.md 原文。 */
declare const __ABOUT_MARKDOWN__: string

/** 友链条目。 */
interface FriendLinkMeta {
  id: string
  title: string
  url: string
  description: string
  image: string
}

/** 由 client/vite.config.ts 注入：友链条目数组（构建期静态数据）。 */
declare const __FRIEND_LINKS__: FriendLinkMeta[]
