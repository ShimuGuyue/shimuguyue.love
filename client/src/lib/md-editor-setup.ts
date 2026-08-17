import { config } from 'md-editor-v3'
import hljs from 'highlight.js'
import 'highlight.js/styles/github.css'
import katex from 'katex'
import 'katex/dist/katex.min.css'

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
  },
})
