<script setup lang="ts">
import { MdPreview } from 'md-editor-v3'
import type { HeadList } from 'md-editor-v3'

import { mdHeadingId, useMdProps } from '@/lib/md-editor-setup'

defineProps<{ modelValue: string }>()
const emit = defineEmits<{ (e: 'getCatalog', list: HeadList[]): void }>()

const mdProps = useMdProps()

function handleGetCatalog(list: HeadList[]) {
  emit('getCatalog', list)
}
</script>

<template>
  <MdPreview
    :model-value="modelValue"
    v-bind="mdProps"
    :md-heading-id="mdHeadingId"
    @get-catalog="handleGetCatalog"
  />
</template>

<style>
.md-editor.md-editor-previewOnly {
  background-color: transparent;
}
</style>
