<script lang="ts">
/** 筛选器中的单个可选项。 */
export interface FilterOption {
  /** 选中判定的唯一键，点击时原样回传给使用方 */
  key: string
  /** 展示文本 */
  label: string
  /** 是否处于选中态（由使用方维护） */
  selected: boolean
}

/** 筛选器中的一个维度。 */
export interface FilterGroup {
  /** 维度唯一键，回调时原样回传给使用方 */
  key: string
  /** 维度名称，显示在行首 */
  label: string
  /** 单选 / 多选状态（由使用方维护，组件只负责发出切换事件） */
  multi: boolean
  /** 该维度下的可选项 */
  options: FilterOption[]
}
</script>

<script setup lang="ts">
import { nextTick, onBeforeUnmount, onMounted, reactive, ref, watch } from 'vue'

const props = withDefaults(
  defineProps<{
    /** 全部筛选维度（每项一行） */
    groups: FilterGroup[]
    /** 搜索框内容，配合 `update:search` 使用 */
    search?: string
    /** 搜索框占位提示 */
    searchPlaceholder?: string
    /** 搜索防抖时长（毫秒） */
    searchDebounce?: number
  }>(),
  {
    search: '',
    searchPlaceholder: '搜索...',
    searchDebounce: 300,
  }
)

const emit = defineEmits<{
  /** 点击某个可选项（切换其选中态由使用方负责） */
  (e: 'toggle', groupKey: string, optionKey: string): void
  /** 切换某个维度的单选 / 多选 */
  (e: 'update:multi', groupKey: string, multi: boolean): void
  /** 搜索框内容变化（已防抖） */
  (e: 'update:search', value: string): void
}>()

// ── 折叠：每个维度最多显示三行，超出时在三行末尾显示「更多...」 ──

/** 折叠状态下筛选器最多显示的行数 */
const MAX_FILTER_LINES = 3

/** 各维度是否已展开（仅本次组件存活内有效，重新渲染或重新进入页面后复位） */
const expanded = reactive<Record<string, boolean>>({})

/** 折叠状态下各维度显示的条目数（按容器宽度实测得出） */
const visibleCounts = reactive<Record<string, number>>({})

/** 各维度条目是否超过三行（超过才需要「更多...」） */
const overflows = reactive<Record<string, boolean>>({})

/** 各维度的条目容器（groupKey → 容器元素） */
const chipsEls = new Map<string, HTMLElement>()

/** 条目容器宽度变化（窗口缩放、字体加载完成等）时重新测量 */
let chipsObserver: ResizeObserver | null = null

/**
 * 注册维度条目容器（模板 ref 回调）。
 * @param groupKey 维度唯一键。
 * @param el       容器元素，卸载时为 null。
 */
function setChipsEl(groupKey: string, el: unknown): void {
  const prev = chipsEls.get(groupKey)
  if (prev && prev !== el) {
    chipsObserver?.unobserve(prev)
    chipsEls.delete(groupKey)
  }
  if (el instanceof HTMLElement) {
    chipsEls.set(groupKey, el)
    chipsObserver?.observe(el)
  }
}

/**
 * 按 flex 换行规则估算条目排布所需行数。
 * @param widths   按顺序排列的条目宽度（px）。
 * @param gap      条目之间的水平间距（px）。
 * @param maxWidth 容器可用宽度（px）。
 * @returns 排布所需行数。
 */
function countLines(widths: number[], gap: number, maxWidth: number): number {
  let lines = 0
  let lineWidth = 0
  for (const width of widths) {
    if (lineWidth === 0) {
      lineWidth = width
    } else if (lineWidth + gap + width <= maxWidth) {
      lineWidth += gap + width
    } else {
      lines += 1
      lineWidth = width
    }
  }
  return lineWidth > 0 ? lines + 1 : lines
}

/** 每个容器最近一次测量依据（容器宽度 + 条目数），避免重复测量与观察器抖动 */
const measuredKey = new WeakMap<HTMLElement, string>()

/**
 * 测量单个维度容器，得出折叠状态下可显示的条目数。
 *
 * 被折叠的条目依旧留在 DOM 中（绝对定位 + visibility: hidden），
 * 因此无论是否展开、容器宽度如何变化，都能取到真实宽度重新计算。
 *
 * @param groupKey  维度唯一键。
 * @param container 维度条目容器。
 */
function measureGroup(groupKey: string, container: HTMLElement): void {
  const more = container.querySelector<HTMLElement>('.filter-more')
  if (!more) return

  const style = getComputedStyle(container)
  const gap = Number.parseFloat(style.columnGap) || 0
  const maxWidth = container.clientWidth
    - (Number.parseFloat(style.paddingLeft) || 0)
    - (Number.parseFloat(style.paddingRight) || 0)
  if (maxWidth <= 0) return

  const widths = Array.from(container.querySelectorAll<HTMLElement>('.filter-chip'))
    .map(chip => chip.offsetWidth)

  const key = `${maxWidth}:${widths.length}`
  if (measuredKey.get(container) === key) return
  measuredKey.set(container, key)

  // 三行内放得下全部条目：无需「更多...」
  if (countLines(widths, gap, maxWidth) <= MAX_FILTER_LINES) {
    visibleCounts[groupKey] = widths.length
    overflows[groupKey] = false
    return
  }

  // 需要「更多...」：从后往前取能连按钮一起放进三行的最大条目数
  let count = 0
  for (let i = widths.length; i >= 0; i--) {
    if (countLines([...widths.slice(0, i), more.offsetWidth], gap, maxWidth)
        <= MAX_FILTER_LINES) {
      count = i
      break
    }
  }
  visibleCounts[groupKey] = count
  overflows[groupKey] = true
}

/** 重新测量全部维度的条目容器（条目或容器宽度变化后调用）。 */
async function measureFilters(): Promise<void> {
  await nextTick()
  for (const [groupKey, container] of chipsEls) {
    measureGroup(groupKey, container)
  }
}

/** 折叠状态下该条目是否应隐藏（未测量到时先全部显示）。 */
function isChipHidden(groupKey: string, index: number): boolean {
  if (expanded[groupKey]) return false
  const visible = visibleCounts[groupKey]
  return visible !== undefined && index >= visible
}

/** 「更多...」按钮是否应隐藏（已展开或未超过三行时隐藏）。 */
function isMoreHidden(groupKey: string): boolean {
  return !!expanded[groupKey] || !overflows[groupKey]
}

function selectOption(group: FilterGroup, option: FilterOption): void {
  emit('toggle', group.key, option.key)
}

function toggleMulti(group: FilterGroup): void {
  emit('update:multi', group.key, !group.multi)
}

// ── 搜索：输入即时回显，对外按防抖后的内容通知 ──

const searchText = ref(props.search)

let searchTimer: ReturnType<typeof setTimeout> | null = null

// 使用方（如路由前进后退）改写搜索词时同步到输入框
watch(() => props.search, value => {
  if (value !== searchText.value) searchText.value = value
})

function onSearchInput(): void {
  if (searchTimer) clearTimeout(searchTimer)
  searchTimer = setTimeout(() => {
    searchTimer = null
    emit('update:search', searchText.value)
  }, props.searchDebounce)
}

// ── 生命周期 ──

// 条目变化（分类 / 标签异步加载完成等）后重新测量折叠行数
watch(
  () => props.groups.map(group => `${group.key}:${group.options.length}`).join('|'),
  () => { measureFilters() },
  { flush: 'post' }
)

onMounted(async () => {
  chipsObserver = new ResizeObserver(() => { measureFilters() })
  for (const container of chipsEls.values()) {
    chipsObserver.observe(container)
  }
  await measureFilters()
})

onBeforeUnmount(() => {
  if (searchTimer) clearTimeout(searchTimer)
  searchTimer = null
  chipsObserver?.disconnect()
  chipsObserver = null
  chipsEls.clear()
})
</script>

<template>
  <section class="filter-bar">
    <!-- 各筛选维度：一行一个（分类 / 标签 / ...） -->
    <div v-for="group in groups" :key="group.key" class="filter-row">
      <span class="filter-label">{{ group.label }}</span>
      <button class="filter-mode-btn tag-pink" @click="toggleMulti(group)">{{ group.multi ? '多选' : '单选' }}</button>
      <div :ref="el => setChipsEl(group.key, el)" class="filter-chips">
        <button
          v-for="(option, index) in group.options"
          :key="option.key"
          class="tag-normal filter-chip"
          :class="{
            'tag--active': option.selected,
            'filter-chip--hidden': isChipHidden(group.key, index),
          }"
          @click="selectOption(group, option)"
        >
          {{ option.label }}
        </button>
        <button
          class="filter-more tag-pink"
          :class="{ 'filter-chip--hidden': isMoreHidden(group.key) }"
          :aria-expanded="!!expanded[group.key]"
          @click="expanded[group.key] = true"
        >
          更多...
        </button>
      </div>
    </div>

    <!-- 搜索 -->
    <div class="filter-search">
      <input
        v-model="searchText"
        type="text"
        class="search-input"
        :placeholder="searchPlaceholder"
        @input="onSearchInput"
      />
    </div>
  </section>
</template>

<!-- 共享外观：卡片底色（block）与标签样式（tag）为全局样式，不参与 scoped 作用域 -->
<style>
@import "@/assets/background/block.css";
@import "@/assets/normal/tag.css";
</style>

<style scoped>
/* ── 筛选器专属变量（亮色）：
   --selector-bar-border       筛选栏边框色
   --selector-input-bg         搜索框背景色
   --selector-input-border     搜索框边框色
   --selector-input-focus-ring 搜索框聚焦光圈色 */
.filter-bar {
  --selector-bar-border: #f0dce8;
  --selector-input-bg: var(--pink-04);
  --selector-input-border: #f0c8e2;
  --selector-input-focus-ring: var(--pink-25);
}

/* 筛选器专属变量（暗色）：与上方一一对应，仅调整色值适配深色背景 */
html.dark .filter-bar {
  --selector-bar-border: #3d2c38;
  --selector-input-bg: var(--pink-06);
  --selector-input-border: #55374c;
  --selector-input-focus-ring: var(--pink-30);
}

/* ── 筛选栏容器 ─────────────────────────────────────────
   整块筛选区域的容器：
     · 背景复用共享变量 --blog-surface-bg（半透明）
     · 12px 圆角 + 细边框 + 轻微阴影，形成独立卡片感
     · 主题切换时背景与边框平滑过渡 */

.filter-bar {
  background-color: var(--blog-surface-bg);
  border: 1px solid var(--selector-bar-border);
  border-radius: var(--blog-surface-radius);
  padding: var(--blog-surface-padding);
  margin-bottom: 28px;
  box-shadow: var(--blog-surface-shadow);
  transition:
    background-color var(--transition-speed),
    border-color var(--transition-speed);
}

/* 筛选行：一行一个筛选维度（分类 / 标签），flex 左对齐排布 */
.filter-row {
  display: flex;
  align-items: flex-start;
  gap: 12px;
  margin-bottom: 16px;
}

/* 左侧维度名（分类 / 标签）：
   与右侧按钮同高（22px）并用 flex 垂直居中，
   使文字与按钮内容处于同一水平线，无需位移 hack */
.filter-label {
  flex-shrink: 0;
  width: 48px;
  display: flex;
  align-items: center;
  justify-content: flex-end;
  height: 22px;
  font-size: 1.1rem;
  font-weight: 600;
  color: var(--color-text);
  text-align: right;
}

/* ── 选择按钮（分类 / 标签） ──────────────────────────────
   按钮本体复用全局 .tag-normal（灰色标签外观），
   选中时叠加 .tag--active（粉色实心）以示区分。*/

/* 选择按钮容器：占满剩余宽度，可换行，按钮间距 8px。
   position: relative 作为折叠条目与「更多...」按钮的定位基准 */
.filter-chips {
  flex: 1;
  display: flex;
  flex-wrap: wrap;
  gap: 8px;
  position: relative;
}

/* ── 筛选器折叠（最多三行 + 「更多...」） ─────────────────
   条目超过三行时只显示前三行，并在三行末尾显示「更多...」。
   被折叠的条目改为绝对定位：脱离文档流（不参与换行、不撑高容器），
   同时保留布局尺寸，visibility: hidden 之下仍可被脚本量取宽度。 */

.filter-chip--hidden {
  position: absolute;
  top: 0;
  left: 0;
  visibility: hidden;
  pointer-events: none;
}

/* 「更多...」按钮：外观复用 .tag-pink，作为筛选器内的展开控件 */
.filter-more {
  white-space: nowrap;
}

/* 选中态：粉色实心 + 白字 + 外发光，明确标识当前已选中的筛选项 */
.tag--active {
  color: #fff;
  background-color: var(--pink-hot, #FF77CC);
  border-color: var(--pink-hot, #FF77CC);
  box-shadow: 0 2px 8px var(--pink-35);
}

/* hover：两类标签按钮在筛选器内悬停时粉底略微加深，提供交互反馈 */
.filter-bar .tag-pink:hover,
.filter-bar .tag-normal:hover {
  background-color: var(--pink-20);
}

/* 按钮统一度量：
   选择按钮（tag-normal）、多选/单选（tag-pink + filter-mode-btn）
   统一为 22px 高，inline-flex 垂直水平居中文本。 */
.filter-bar .tag-pink,
.filter-bar .tag-normal,
.filter-bar .filter-mode-btn {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  height: 22px;
  box-sizing: border-box;
  cursor: pointer;
}

/* ── 多选 / 单选切换按钮 ─────────────────────────────────
   外观完全复用 .tag-pink（粉色标签样式），
   这里只补充布局属性：不收缩、保持可点击。 */

.filter-mode-btn {
  flex-shrink: 0;
  cursor: pointer;
}

/* ── 搜索框 ─────────────────────────────────────────────
   关键字搜索输入框：
     · 淡粉半透明底 + 粉色细边框，与筛选器整体风格一致
     · 聚焦时边框变艳粉并带一圈柔光光圈，提示当前编辑位置 */

/* 搜索区与上方筛选行的间距 */
.filter-search {
  margin-top: 4px;
}

/* 搜索输入框本体：占满整行、圆角 8px、无默认聚焦描边 */
.search-input {
  width: 100%;
  box-sizing: border-box;
  padding: 9px 14px;
  font-size: 0.9rem;
  color: var(--color-text);
  background-color: var(--selector-input-bg);
  border: 1px solid var(--selector-input-border);
  border-radius: 8px;
  outline: none;
  transition:
    background-color var(--transition-speed),
    border-color var(--transition-speed),
    box-shadow var(--transition-speed);
}

/* 占位提示文字：次要色、半透明，弱化存在感 */
.search-input::placeholder {
  color: var(--color-text-secondary);
  opacity: 0.6;
}

/* 聚焦态：粉色边框 + 3px 光圈，光圈颜色来自主题变量 */
.search-input:focus {
  border-color: var(--pink-hot, #FF77CC);
  box-shadow: 0 0 0 3px var(--selector-input-focus-ring);
}
</style>
