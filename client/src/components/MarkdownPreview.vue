<script setup lang="ts">
import { MdPreview } from 'md-editor-v3'
import type { HeadList, MdHeadingId } from 'md-editor-v3'
import 'md-editor-v3/lib/preview.css'
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
import { headingSlug } from '@/lib/md-editor-setup'

defineProps<{ modelValue: string }>()
const emit = defineEmits<{ (e: 'getCatalog', list: HeadList[]): void }>()

const theme = useThemeStore()

/** 预览标题 id 与目录 slug 共用同一规则。 */
const headingId: MdHeadingId = (options) => headingSlug(options.text)

function handleGetCatalog(list: HeadList[]) {
  emit('getCatalog', list)
}
</script>

<template>
  <MdPreview
    :model-value="modelValue"
    :theme="theme.isDark ? 'dark' : 'light'"
    :md-heading-id="headingId"
    no-mermaid
    no-echarts
    no-img-zoom-in
    @get-catalog="handleGetCatalog"
  />
</template>

<style>
.md-editor.md-editor-previewOnly {
  background-color: transparent;
}
</style>
