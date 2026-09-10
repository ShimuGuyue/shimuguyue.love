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

/**
 * 由 client/vite.config.ts 注入的虚拟模块：构建期友链条目数组，
 * 其中 image 为 Vite 资源管线生成的头像 URL。
 */
declare module 'virtual:friend-links' {
  const friendLinks: FriendLinkMeta[]
  export default friendLinks
}
