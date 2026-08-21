import { computed } from 'vue'
import { config } from 'md-editor-v3'
import type { MdHeadingId, Themes } from 'md-editor-v3'
import hljs from 'highlight.js'
import 'highlight.js/styles/github.css'
import katex from 'katex'
import 'katex/dist/katex.min.css'
import MarkdownItGitHubAlerts from 'markdown-it-github-alerts'

// md-editor-v3 组件样式（预览 + 编辑器）
import 'md-editor-v3/lib/preview.css'
import 'md-editor-v3/lib/style.css'
// 站点 Markdown 渲染样式（PinkFairy 主题），按类型拆分
import '@/assets/markdown/font.css'
import '@/assets/markdown/headings.css'
import '@/assets/markdown/divider.css'
import '@/assets/markdown/text.css'
import '@/assets/markdown/blockquote.css'
import '@/assets/markdown/lists.css'
import '@/assets/markdown/code.css'
import '@/assets/markdown/tables.css'
import '@/assets/markdown/images.css'
import '@/assets/markdown/tasks.css'
import '@/assets/markdown/alerts.css'

import { useThemeStore } from '@/stores/theme'

/**
 * 标题 slug 生成规则。
 *
 * 沿用旧渲染规则：非字母数字中文字符统一转为 `-`，去除首尾 `-` 后转小写。
 * 详情页目录、点击标题滚动与预览标题 id 共用此函数，保证 id 一致。
 */
export function headingSlug(text: string): string {
  return text.replace(/[^a-zA-Z0-9\u4e00-\u9fff]+/g, '-').replace(/^-|-$/g, '').toLowerCase()
}

/**
 * 站点统一的 Markdown 组件属性：详情页预览（MdPreview）与编辑页（MdEditor）共用。
 *
 * md-editor-v3 的全局 config() 不支持组件 prop 默认值，因此需要显式传给每个组件；
 * 这里集中定义，避免同一配置散落在多个页面。
 */
export const sharedMdProps = {
  /** 代码块默认不折叠，完整展开显示。 */
  codeFoldable: false,
  /** 不启用 mermaid / echarts / 图片点击放大（与站点现有渲染能力保持一致）。 */
  noMermaid: true,
  noEcharts: true,
  noImgZoomIn: true,
} as const

/**
 * 预览标题 id 与目录 slug 共用同一规则。
 */
export const mdHeadingId: MdHeadingId = (options) => headingSlug(options.text)

/**
 * Markdown 组件统一属性：主题（跟随站点深浅色、预览风格固定 github）+ 站点公共配置。
 * @return 可直接 v-bind 到 MdPreview / MdEditor 的完整属性对象。
 */
export function useMdProps() {
  const themeStore = useThemeStore()
  return computed<{ theme: Themes; 'preview-theme': string } & typeof sharedMdProps>(() => ({
    theme: themeStore.isDark ? 'dark' : 'light',
    'preview-theme': 'github',
    ...sharedMdProps,
  }))
}

/**
 * md-editor-v3 全局配置（幂等合并，可被多页共享导入）。
 *
 * 注入本地 highlight.js / katex 实例，运行期不再请求 unpkg CDN；
 * 保持旧渲染的 typographer / breaks 行为，避免博客渲染回归。
 */
config({
  editorExtensions: {
    highlight: { instance: hljs },
    katex: { instance: katex },
  },
  markdownItConfig: (md) => {
    md.set({ typographer: true, breaks: false })
    md.use(MarkdownItGitHubAlerts)
  },
  markdownItPlugins: (plugins) => {
    // 图片插件默认 figcaption: true 时只认 title 属性；改为 alt，
    // 让 `![描述](图片)` 的 alt 文本渲染为图片下方的说明
    return plugins.map((plugin) => {
      if (plugin.type === 'image') {
        return { ...plugin, options: { ...plugin.options, figcaption: 'alt' } }
      }
      return plugin
    })
  },
})
