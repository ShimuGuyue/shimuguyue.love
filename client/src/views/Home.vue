<script setup lang="ts">
import { ref, computed, onMounted, onUnmounted, watch, nextTick } from 'vue'
import { useAuthStore } from '@/stores/auth'
import { useThemeStore } from '@/stores/theme'

import '@/assets/glass.css'
import '@/assets/pink-theme.css'

const auth = useAuthStore()
const theme = useThemeStore()

interface ImageItem {
  id: number
  path: string
  description: string
  scale: number
  rotation: number
  pos_x: number
  pos_y: number
  z: number
  w?: number
  h?: number
}

const images = ref<ImageItem[]>([])
const permissions = ref<string[]>([])
const profile = ref({ title: '', subtitle: '', bio: '' })
const profileEditMode = ref(false)
const profileDraft = ref({ title: '', subtitle: '', bio: '' })

const previewId = ref<number | null>(null)
const previewImage = computed(() =>
  previewId.value !== null
    ? images.value.find(i => i.id === previewId.value) ?? null
    : null
)

const previewSrcRect = ref<DOMRect | null>(null)

watch(previewImage, async (img) => {
  if (!img || !previewSrcRect.value) return
  await nextTick()
  await new Promise(r => requestAnimationFrame(r))
  const el = document.querySelector('.home__preview-img') as HTMLElement | null
  if (!el) return
  const start = previewSrcRect.value
  const end = el.getBoundingClientRect()
  // 计算从墙上位置到屏幕中央的 delta（用 transform 偏移）
  const dx = (start.left + start.width / 2) - (end.left + end.width / 2)
  const dy = (start.top + start.height / 2) - (end.top + end.height / 2)
  const sx = start.width / end.width
  const sy = start.height / end.height
  // 旋转取最短路径（-180° ~ 180°）
  let sr = img.rotation % 360
  if (sr > 180) sr -= 360
  if (sr < -180) sr += 360
  el.animate([
    { transform: `translate(${dx}px, ${dy}px) scale(${sx}, ${sy}) rotate(${sr}deg)`, opacity: 0 },
    { transform: 'translate(0, 0) scale(1, 1) rotate(0deg)', opacity: 1 },
  ], {
    duration: 450,
    easing: 'cubic-bezier(0.25, 0.46, 0.45, 0.94)',
    fill: 'backwards',
  })
})

onMounted(async () => {
  // 先加载个人介绍
  try {
    const r = await fetch('/api/profile')
    if (r.ok) profile.value = await r.json()
  } catch { /* 静默 */ }

  if (auth.isLoggedIn && auth.id !== null) {
    try {
      const resp = await fetch('/api/user/permissions', {
        headers: { 'Authorization': 'Bearer ' + auth.token }
      })
      if (resp.ok) {
        const data = await resp.json()
        permissions.value = data.permissions || []
      }
    } catch { /* 静默 */ }
  }

  await loadImages()
})

onUnmounted(() => {
  if (revealTimer !== null) {
    clearTimeout(revealTimer)
    revealTimer = null
  }
})

/// 单张图片的显示动画时长（毫秒），比原来 500ms 放慢
const REVEAL_MS = 1200

/// 下一张图片的触发间隔：当前图片显示到 1/3 时开始显示下一张（毫秒）
const REVEAL_INTERVAL_MS = REVEAL_MS / 3

/// 图片逐个渲染的定时器
let revealTimer: ReturnType<typeof setTimeout> | null = null

async function loadImages() {
  // 清除上一次未完成的定时器
  if (revealTimer !== null) {
    clearTimeout(revealTimer)
    revealTimer = null
  }

  try {
    const resp = await fetch('/api/images')
    if (!resp.ok) return
    const all: ImageItem[] = await resp.json()

    // 按层次从低到高排列（z 值低的先渲染）
    all.sort((a, b) => a.z - b.z)

    // 清空后逐张渲染
    images.value = []
    await revealImages(all)
    // 初始化 z 计数器为当前最大 z 值 + 1
    const maxZ = all.reduce((m, i) => Math.max(m, i.z), 0)
    zCounter = maxZ + 1
  } catch { /* 静默 */ }
}

/** 当前图片显示到 1/3 时推入下一张图片 */
/** 逐张预加载尺寸后推入，避免定位跳动 */
async function revealImages(all: ImageItem[]) {
  // 预加载所有图片获取原始尺寸
  const sized: (ImageItem & { w: number; h: number })[] = []
  for (const img of all) {
    const size = await loadImageSize(img.path)
    sized.push({ ...img, w: size.w, h: size.h })
  }

  return new Promise<void>((resolve) => {
    let i = 0
    function next() {
      if (i >= sized.length) { resolve(); return }
      images.value.push(sized[i]!)
      i++
      revealTimer = setTimeout(next, REVEAL_INTERVAL_MS)
    }
    next()
  })
}

function loadImageSize(path: string): Promise<{ w: number; h: number }> {
  return new Promise((resolve) => {
    const img = new Image()
    img.onload = () => {
      const MAX = 300
      let w = img.naturalWidth
      let h = img.naturalHeight
      if (w > MAX || h > MAX) {
        const ratio = Math.min(MAX / w, MAX / h)
        w = Math.round(w * ratio)
        h = Math.round(h * ratio)
      }
      resolve({ w, h })
    }
    img.onerror = () => resolve({ w: 200, h: 200 })
    img.src = `/image/${path}`
  })
}

const editMode = ref(false)
const draggingId = ref<number | null>(null)
const dragStart = ref({ x: 0, y: 0 })
/// 进入编辑模式时保存的快照
const editSnapshot = ref<string>('')
/// 待删除的图片 id 集合（完成编辑时统一删除）
const pendingDeletes = ref<Set<number>>(new Set())
/// 本次编辑期间上传的图片 id 集合（取消编辑时统一删除）
const pendingUploads = ref<Set<number>>(new Set())
/// z-index 计数器，每次交互递增
let zCounter = 0

function bringToFront(imgId: number) {
  const img = images.value.find(i => i.id === imgId)
  if (img) img.z = ++zCounter
}

/// 点击照片墙内进入编辑模式，点击外部退出编辑模式
function profileHasChanges(): boolean {
  return profileDraft.value.title !== profile.value.title
    || profileDraft.value.subtitle !== profile.value.subtitle
    || profileDraft.value.bio !== profile.value.bio
}

function onWallClick(e: MouseEvent) {
  // 点击预览遮罩时不处理
  if ((e.target as HTMLElement).closest('.home__preview')) return
  const inPhoto = !!(e.target as HTMLElement).closest('.home__photo')
  const onImg = !!(e.target as HTMLElement).closest('.home__img')
  const inProfileEdit = !!(e.target as HTMLElement).closest('.home__profile-edit-box')

  if (profileEditMode.value) {
    // 个人介绍编辑：点击外部且无修改时退出
    if (!inProfileEdit && !profileHasChanges()) cancelProfileEdit()
    return
  }

  if (editMode.value) {
    // 编辑模式下：仅点击外部且无修改时退出
    if (!inPhoto && !hasChanges() && pendingDeletes.value.size === 0) cancelEdit()
    return
  }

  // 非编辑模式下：点击照片墙内部（非图片上）进入编辑
  if (inPhoto && !onImg) {
    editMode.value = true
    pendingUploads.value = new Set()
    editSnapshot.value = JSON.stringify(images.value.map(i => ({
      id: i.id, pos_x: i.pos_x, pos_y: i.pos_y, scale: i.scale, rotation: i.rotation, description: i.description,
    })))
  }
}

async function exitEdit() {
  const changed = hasChanges() || pendingDeletes.value.size > 0
  if (changed && !permissions.value.includes('photo_wall:edit')) {
    alert('操作失败：该操作需要 photo_wall:edit 权限')
    pendingDeletes.value = new Set()
    pendingUploads.value = new Set()
    revertChanges()
    editMode.value = false
    return
  }

  for (const id of pendingDeletes.value) {
    const img = images.value.find(i => i.id === id)
    if (img) {
      await fetch('/api/image/delete', {
        method: 'DELETE',
        headers: {
          'Content-Type': 'application/json',
          'Authorization': `Bearer ${auth.token}`,
        },
        body: JSON.stringify({ path: img.path }),
      })
    }
  }
  images.value = images.value.filter(i => !pendingDeletes.value.has(i.id))
  pendingDeletes.value = new Set()
  pendingUploads.value = new Set()

  if (changed) {
    for (const img of images.value) {
      await saveMeta(img)
    }
  }
  editMode.value = false
}

async function cancelEdit() {
  // 取消编辑：删除本次编辑期间上传的图片（服务器记录与文件）
  for (const id of pendingUploads.value) {
    const img = images.value.find(i => i.id === id)
    if (img) {
      await fetch('/api/image/delete', {
        method: 'DELETE',
        headers: {
          'Content-Type': 'application/json',
          'Authorization': `Bearer ${auth.token}`,
        },
        body: JSON.stringify({ path: img.path }),
      })
    }
  }
  images.value = images.value.filter(i => !pendingUploads.value.has(i.id))
  pendingUploads.value = new Set()
  pendingDeletes.value = new Set()
  revertChanges()
  editMode.value = false
}

function hasChanges(): boolean {
  try {
    const snap = JSON.parse(editSnapshot.value) as Pick<ImageItem, 'id' | 'pos_x' | 'pos_y' | 'scale' | 'rotation' | 'description'>[]
    for (const img of images.value) {
      const s = snap.find(s => s.id === img.id)
      if (!s) continue
      if (s.pos_x !== img.pos_x || s.pos_y !== img.pos_y || s.scale !== img.scale || s.rotation !== img.rotation || s.description !== img.description) return true
    }
    return false
  } catch { return false }
}

function revertChanges() {
  try {
    const snap = JSON.parse(editSnapshot.value) as Pick<ImageItem, 'id' | 'pos_x' | 'pos_y' | 'scale' | 'rotation' | 'description'>[]
    for (const s of snap) {
      const img = images.value.find(i => i.id === s.id)
      if (img) {
        img.pos_x = s.pos_x
        img.pos_y = s.pos_y
        img.scale = s.scale
        img.rotation = s.rotation
        img.description = s.description ?? ''
      }
    }
  } catch { /* 静默 */ }
}

async function saveMeta(img: ImageItem) {
  await fetch('/api/image/save', {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
      'Authorization': `Bearer ${auth.token}`,
    },
    body: JSON.stringify({
      path: img.path,
      description: img.description,
      scale: img.scale,
      rotation: img.rotation,
      pos_x: img.pos_x,
      pos_y: img.pos_y,
      z: img.z,
    }),
  })
}

// ── 拖拽 ──
function onImgMouseDown(e: MouseEvent, imgId: number) {
  if (!editMode.value) return
  bringToFront(imgId)
  draggingId.value = imgId
  dragStart.value = { x: e.clientX, y: e.clientY }
  e.preventDefault()
  e.stopPropagation()
}

let wasDragged = false

function onWallMouseMove(e: MouseEvent) {
  if (draggingId.value === null) return
  wasDragged = true
  const wall = (e.currentTarget as HTMLElement).getBoundingClientRect()
  const img = images.value.find(i => i.id === draggingId.value)
  if (!img) return
  const dx = ((e.clientX - dragStart.value.x) / wall.width) * 100
  const dy = ((e.clientY - dragStart.value.y) / wall.height) * 100
  img.pos_x = Math.max(0, Math.min(100, img.pos_x + dx))
  img.pos_y = Math.max(0, Math.min(100, img.pos_y + dy))
  dragStart.value = { x: e.clientX, y: e.clientY }
}

function onWallMouseUp() {
  draggingId.value = null
}

function onWallWheel(e: WheelEvent) {
  if (e.ctrlKey) e.preventDefault()
}

function markDelete(img: ImageItem) {
  if (pendingDeletes.value.has(img.id)) {
    pendingDeletes.value.delete(img.id)
  } else {
    if (!permissions.value.includes('photo_wall:delete')) {
      alert('操作失败：该操作需要 photo_wall:delete 权限')
      return
    }
    pendingDeletes.value.add(img.id)
  }
  pendingDeletes.value = new Set(pendingDeletes.value)
}

async function uploadImage() {
  if (!permissions.value.includes('photo_wall:upload')) {
    alert('操作失败：该操作需要 photo_wall:upload 权限')
    return
  }
  const input = document.createElement('input')
  input.type = 'file'
  input.accept = 'image/*'
  input.onchange = async () => {
    const file = input.files?.[0]
    if (!file) return
    const form = new FormData()
    form.append('file', file)
    const resp = await fetch('/api/image/upload', {
      method: 'POST',
      headers: { 'Authorization': `Bearer ${auth.token}` },
      body: form,
    })
    if (!resp.ok) {
      const err = await resp.json().catch(() => ({ error: '上传失败' }))
      alert(err.error || '上传失败')
      return
    }
    const uploaded = await resp.json() as { id: number; path: string }
    const size = await loadImageSize(uploaded.path)
    zCounter = Math.max(zCounter, images.value.reduce((m, i) => Math.max(m, i.z), 0)) + 1
    images.value.push({
      id: uploaded.id,
      path: uploaded.path,
      description: '',
      scale: 1.0,
      rotation: 0,
      pos_x: 50,
      pos_y: 50,
      z: zCounter,
      w: size.w,
      h: size.h,
    })
    pendingUploads.value = new Set(pendingUploads.value).add(uploaded.id)
  }
  input.click()
}

// ── 滚轮缩放/旋转 ──
function onImgWheel(e: WheelEvent, imgId: number) {
  if (!editMode.value) return
  bringToFront(imgId)
  e.preventDefault()
  e.stopPropagation()
  const img = images.value.find(i => i.id === imgId)
  if (!img) return
  const speed = e.ctrlKey ? 10 : 1
  if (e.shiftKey) {
    img.rotation = (img.rotation + e.deltaY * 0.01 * speed + 360) % 360
  } else {
    img.scale = Math.max(0.1, Math.min(5, img.scale - e.deltaY * 0.0001 * speed))
  }
}

function handleImgClick(imgId: number, event: MouseEvent) {
  if (wasDragged) { wasDragged = false; return }
  if (editMode.value) return  // 编辑模式下单击不放大，改为双击
  openPreview(imgId, event)
}

function handleImgDblClick(imgId: number, event: MouseEvent) {
  if (wasDragged) { wasDragged = false; return }
  openPreview(imgId, event)
}

function openPreview(id: number, event: MouseEvent) {
  const el = event.currentTarget as HTMLElement
  previewSrcRect.value = el.getBoundingClientRect()
  previewId.value = id
  if (editMode.value) {
    const img = images.value.find(i => i.id === id)
    if (img) editDesc.value = img.description
  }
}

/// 编辑中的描述文本
const editDesc = ref('')
function closePreview() {
  if (editMode.value && previewId.value !== null) {
    const img = images.value.find(i => i.id === previewId.value)
    if (img) img.description = editDesc.value
  }
  previewId.value = null
  previewSrcRect.value = null
}

// ── 个人介绍编辑 ──

function enterProfileEdit() {
  profileDraft.value = { ...profile.value }
  profileEditMode.value = true
}

function cancelProfileEdit() {
  profileEditMode.value = false
}

async function saveProfile() {
  if (!permissions.value.includes('introduction:edit')) {
    alert('操作失败：该操作需要 introduction:edit 权限')
    return
  }
  const resp = await fetch('/api/profile/save', {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
      'Authorization': `Bearer ${auth.token}`,
    },
    body: JSON.stringify(profileDraft.value),
  })
  if (!resp.ok) {
    const err = await resp.json().catch(() => ({ error: '保存失败' }))
    alert(err.error || '保存失败')
    return
  }
  profile.value = { ...profileDraft.value }
  profileEditMode.value = false
}

function imgStyle(img: ImageItem) {
  return {
    left: `${img.pos_x}%`,
    top: `${img.pos_y}%`,
    transform: `translate(-50%, -50%) scale(${img.scale}) rotate(${img.rotation}deg)`,
  }
}
</script>

<template>
  <main class="home" :style="{ '--reveal-duration': REVEAL_MS + 'ms', '--img-border': theme.isDark ? '#000' : '#fff', '--btn-text-color': '#000', '--title-color': theme.isDark ? '#8899ff' : '#3451b2' }" @click="onWallClick">
    <div class="home__layout">
      <div
        :class="{ 'home__photo': true, 'home__photo--edit': editMode }"
        @mousemove="onWallMouseMove"
        @mouseup="onWallMouseUp"
        @mouseleave="onWallMouseUp"
        @wheel="onWallWheel"
      >
        <button
          v-if="editMode" class="home__edit-done"
          :style="{ background: theme.isDark ? 'rgba(0,0,0,0.1)' : 'rgba(255,255,255,0.2)' }"
          @click.stop="exitEdit"
        >完成编辑</button>
        <button
          v-if="editMode" class="home__edit-upload"
          :style="{ background: theme.isDark ? 'rgba(0,0,0,0.1)' : 'rgba(255,255,255,0.2)' }"
          @click.stop="uploadImage"
        >上传图片</button>
        <button
          v-if="editMode" class="home__edit-cancel"
          :style="{ background: theme.isDark ? 'rgba(0,0,0,0.1)' : 'rgba(255,255,255,0.2)' }"
          @click.stop="cancelEdit"
        >取消编辑</button>
        <div
          v-if="editMode" class="home__hint"
          :style="{ background: theme.isDark ? 'rgba(0,0,0,0.1)' : 'rgba(255,255,255,0.2)' }"
        >
          <p>拖拽：按住图片拖动</p>
          <p>缩放：滚轮</p>
          <p>旋转：Shift + 滚轮</p>
          <p>放大：双击图片</p>
          <p>加速：按住 Ctrl 使缩放和旋转速度加快</p>
        </div>
        <div
          v-for="img in images"
          :key="img.id"
          :class="{ 'home__img': true, 'home__img--enter': true, 'home__img--pending': editMode && pendingDeletes.has(img.id) }"
          :style="{ left: img.pos_x + '%', top: img.pos_y + '%', zIndex: img.z || 0 }"
        >
          <div
            class="home__img-wrap"
            :class="{ 'home__img--edit': editMode, 'home__img-wrap--pending': editMode && pendingDeletes.has(img.id) }"
            :style="{ width: img.w + 'px', height: img.h + 'px', transform: `scale(${img.scale}) rotate(${img.rotation}deg)` }"
            @mousedown="e => onImgMouseDown(e, img.id)"
            @click.stop="handleImgClick(img.id, $event)"
            @dblclick.stop="handleImgDblClick(img.id, $event)"
            @wheel.prevent="e => onImgWheel(e, img.id)"
          >
            <img
              :src="`/image/${img.path}`"
              :alt="img.description"
              draggable="false"
              :style="{ border: `5px solid ${theme.isDark ? '#000' : '#fff'}` }"
            />
            <button
              v-if="editMode"
              class="home__img-del"
              :style="{ transform: `scale(${1 / img.scale})` }"
              @click.stop="markDelete(img)"
            >✕</button>
          </div>
        </div>
      </div>
      <div class="home__info">
        <div class="home__profile-edit-box" :class="{ 'home__profile-edit-box--active': profileEditMode }">
          <div class="home__profile-field home__profile-field--title-row">
            <div class="home__profile-actions" :class="{ 'home__profile-actions--hidden': !profileEditMode }">
              <button class="home__profile-btn home__profile-btn--save" :style="{ background: theme.isDark ? 'rgba(0,0,0,0.1)' : 'rgba(255,255,255,0.2)' }" @click="saveProfile">完成编辑</button>
              <button class="home__profile-btn home__profile-btn--cancel" :style="{ background: theme.isDark ? 'rgba(0,0,0,0.1)' : 'rgba(255,255,255,0.2)' }" @click="cancelProfileEdit">取消编辑</button>
            </div>
            <div class="home__profile-field" :class="{ 'home__profile-field--dashed': profileEditMode }" style="flex:1; margin-right: 4rem">
              <input :value="profileEditMode ? profileDraft.title : profile.title" @input="profileEditMode && (profileDraft.title = ($event.target as HTMLInputElement).value)" :readonly="!profileEditMode" class="home__profile-input home__profile-input--title" @click="!profileEditMode && enterProfileEdit()" />
            </div>
          </div>
          <div class="home__profile-field" :class="{ 'home__profile-field--dashed': profileEditMode }">
            <input :value="profileEditMode ? profileDraft.subtitle : profile.subtitle" @input="profileEditMode && (profileDraft.subtitle = ($event.target as HTMLInputElement).value)" :readonly="!profileEditMode" class="home__profile-input home__profile-input--subtitle" />
          </div>
          <div class="home__profile-field" :class="{ 'home__profile-field--dashed': profileEditMode }">
            <textarea v-if="profileEditMode" v-model="profileDraft.bio" class="home__profile-input home__profile-input--bio" rows="6" />
            <textarea v-else :value="profile.bio" readonly class="home__profile-input home__profile-input--bio" rows="6" />
          </div>
        </div>
        <RouterLink to="/thanks" class="home__info-link">致谢</RouterLink>
      </div>
    </div>

    <Transition name="preview">
      <div
        v-if="previewImage"
        class="home__preview"
        :style="{ background: theme.isDark ? 'rgba(0,0,0,0.01)' : 'rgba(255,255,255,0.01)' }"
        @click="closePreview"
      >
        <img
          :src="`/image/${previewImage.path}`"
          class="home__preview-img"
        />
      </div>
    </Transition>
    <Transition name="preview">
      <div
        v-if="previewImage"
        class="home__preview-desc"
        :style="{ backgroundImage: 'url(/assets/note-background.png)' }"
      >
        <textarea
          v-if="editMode && permissions.includes('photo_wall:edit')"
          v-model="editDesc"
          class="home__preview-textarea"
          @click.stop
        />
        <span v-else>{{ previewImage.description }}</span>
      </div>
    </Transition>
  </main>
</template>

<style scoped>
.home {
  padding: 24px;
  height: calc(100vh - 80px);
  overflow: hidden;
}

.home__layout {
  height: 100%;
  display: grid;
  grid-template-columns: 3fr 2fr;
  gap: 24px;
}

.home__photo {
  position: relative;
  min-height: 100%;
  user-select: none;
}

.home__photo--edit {
  outline: 2px dashed #000;
  outline-offset: -1px;
}

/* 右侧信息栏 */
.home__info {
  display: flex;
  flex-direction: column;
  align-items: flex-end;
  gap: 12px;
  padding: 0 0 20px 0;
  padding-right: 24px;
}

/* 个人介绍编辑模式 */
.home__profile-edit-box {
  display: flex;
  flex-direction: column;
  padding: 12px;
}

.home__profile-edit-box--active {
  outline: 2px dashed #000;
  outline-offset: -1px;
}

.home__profile-actions {
  display: flex;
  flex-direction: column;
  gap: 4px;
  flex-shrink: 0;
}

.home__profile-field--title-row {
  display: flex;
  gap: 8px;
  align-items: center;
}

.home__profile-actions--hidden {
  visibility: hidden;
}

.home__profile-btn {
  padding: 4px 12px;
  border: 1px solid var(--color-border);
  border-radius: 4px;
  font-size: 0.8rem;
  cursor: pointer;
  background: none;
  color: var(--btn-text-color, #333);
}

.home__profile-btn--cancel {
  color: var(--btn-text-color, #666);
}

.home__profile-btn--save:disabled {
  opacity: 0.4;
  cursor: not-allowed;
}

.home__profile-field--dashed {
  outline: 2px dashed #000;
  outline-offset: -1px;
}

.home__profile-input {
  width: 100%;
  border: none;
  background: transparent;
  color: var(--btn-text-color, #333);
  outline: none;
  padding: 6px 8px;
  font-family: inherit;
  box-sizing: border-box;
  text-align: right;
}

.home__profile-input--title {
  font-size: 5rem;
  font-weight: 700;
  color: var(--title-color, #3451b2);
  text-shadow: 3px 3px 6px rgba(0,0,0,0.3);
}

.home__profile-input--subtitle {
  font-size: 18px;
  font-weight: 700;
  color: var(--title-color, #3451b2);
  
  display: inline-block;

  /* 米哈游 https://www.mihoyo.com/?page=product 同款科技蓝与霓虹粉斜向渐变 */
  background-image: linear-gradient(
    -135deg, 
    rgb(55, 120, 229), 
    rgb(233, 139, 192), 
    rgb(55, 120, 229), 
    rgb(233, 139, 192), 
    rgb(55, 120, 229)
  );
  background-size: 200% auto;
  -webkit-background-clip: text;
  background-clip: text;
  color: transparent !important;
  -webkit-text-fill-color: transparent !important;
  animation: mihoyo-gradient-flow 10s linear infinite;
}
/* 定义渐变流动动画：改变 background-position */
@keyframes mihoyo-gradient-flow {
  0% {
    background-position: 0% 50%;
  }
  100% {
    background-position: -200% 50%;
  }
}

.home__profile-input--bio {
  font-size: 1.1rem;
  line-height: 1.8;
  resize: none;
  border-bottom: 1px solid #000;
  padding-bottom: 8px;
  font-weight: 700;
  font-family: FangSong, STFangsong, serif;
  color: var(--title-color, #3451b2);
}

.home__profile-input[readonly] {
  opacity: 1;
}

.home__profile-input--title[readonly] {
  cursor: pointer;
}

.home__info-link {
  display: inline-block;
  padding: 4px 12px;
  border-radius: 4px;
  color: #000;
  font-size: 1.1rem;
  text-decoration: none;
  transition: color 0.15s;
  font-weight: 700;
}
.home__info-link:hover {
  color: var(--pink-hot);
}

.home__img {
  position: absolute;
  cursor: pointer;
  transform: translate(-50%, -50%);
}

.home__img--enter {
  animation: img-pop-in var(--reveal-duration, 0.5s) ease-out both;
}

.home__img--pending {
  pointer-events: none;
}

@keyframes img-pop-in {
  from {
    transform: translate(-50%, -50%) scale(0);
    opacity: 0;
  }
}
.home__img img {
  display: block;
  max-width: 300px;
  max-height: 300px;
  object-fit: contain;
  pointer-events: none;
  user-select: none;
}

.home__img-wrap {
  position: relative;
  cursor: pointer;
}

.home__img--edit {
  cursor: grab;
}

.home__img-del {
  position: absolute;
  top: -8px;
  right: -8px;
  z-index: 10;
  width: 16px;
  height: 16px;
  border: none;
  border-radius: 50%;
  background: #d44;
  color: #fff;
  font-size: 0.7rem;
  cursor: pointer;
  display: flex;
  align-items: center;
  justify-content: center;
}

.home__img-wrap--pending {
  opacity: 0;
  pointer-events: none;
}
.home__img-wrap--pending .home__img-del {
  background: #666;
}

.home__edit-done {
  position: absolute;
  top: 12px;
  right: 12px;
  z-index: 9999;
  padding: 6px 16px;
  border: 1px solid var(--color-border);
  border-radius: 4px;
  color: var(--btn-text-color, #333);
  font-size: 0.8rem;
  cursor: pointer;
}

.home__edit-upload {
  position: absolute;
  top: 12px;
  right: 200px;
  z-index: 9999;
  padding: 6px 16px;
  border: 1px solid var(--color-border);
  border-radius: 4px;
  color: var(--btn-text-color, #333);
  font-size: 0.8rem;
  cursor: pointer;
}

.home__edit-cancel {
  position: absolute;
  top: 12px;
  right: 100px;
  z-index: 9999;
  padding: 6px 16px;
  border: 1px solid var(--color-border);
  border-radius: 4px;
  color: var(--btn-text-color, #666);
  font-size: 0.8rem;
  cursor: pointer;
}

.home__hint {
  position: absolute;
  bottom: 12px;
  right: 12px;
  z-index: 9999;
  padding: 8px 14px;
  border-radius: 4px;
  color: var(--btn-text-color, #666);
  font-size: 0.75rem;
  line-height: 1.7;
}
.home__hint p {
  margin: 0;
}

.home__preview {
  position: fixed;
  inset: 0;
  z-index: 2000;
  backdrop-filter: blur(5px);
  display: flex;
  align-items: center;
  justify-content: center;
  cursor: pointer;
}

.home__preview-img {
  max-height: 90vh;
  max-width: 65vw;
  object-fit: contain;
  border-radius: 8px;
}

/* Vue Transition 类 */
.preview-enter-active {
  transition: opacity 1s ease;
}
.preview-leave-active {
  transition: opacity 0.5s ease;
}
.preview-enter-from,
.preview-leave-to {
  opacity: 0;
}

.home__preview-desc {
  position: fixed;
  right: 32px;
  top: 50%;
  transform: translateY(-50%);
  z-index: 2001;
  padding: 24px;
  width: 256px;
  height: 256px;
  font-size: 1rem;
  color: var(--color-text);
  line-height: 1.8;
  background-size: contain;
  background-position: center;
  display: flex;
  justify-content: center; /* 水平居中 */
  align-items: center;     /* 垂直居中 */
}

.home__preview-textarea {
  width: 80%;
  height: 80%;
  transform: translateY(20px);
  background: transparent;
  border: none;
  color: #000000;
  font-size: 1rem;
  line-height: 1.8;
  padding: 8px;
  resize: none;
  font-weight: 600;
  outline: none;
  font-family: "仿宋", FangSong, serif;
  font-weight: bold;
  box-sizing: border-box;
}

.home__preview-desc span {
  display: block;
  width: 80%;
  height: 80%;
  transform: translateY(20px);
  font-family: "仿宋", FangSong, serif;
  font-weight: bold;
  font-size: 1rem;
  line-height: 1.8;
  color: #000000;
  padding: 8px;
  box-sizing: border-box;
  word-break: break-word;
  white-space: pre-wrap;
}
</style>
