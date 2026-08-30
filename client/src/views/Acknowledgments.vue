<script setup lang="ts">
import '@/assets/normal/color.css'
import '@/assets/background/block.css'
</script>

<template>
  <main class="thanks">
    <div class="thanks__card">
      <h1>致谢</h1>
      <h2>资源提供</h2>
      <p>
        标签页网站图标使用
        <a href="https://www.flaticon.com" 
           title="study icons" 
           target="_blank" 
           rel="noopener noreferrer">
          Flaticon
        </a>
        网站提供的免费第三方资源：
        <a href="https://www.flaticon.com/free-icons/study" 
           title="study icons" 
           target="_blank" 
           rel="noopener noreferrer">
          Study icons created by Magnific - Flaticon
        </a>
      </p>
      <p>
        照片墙便签图标使用
        <a href="https://www.flaticon.com" 
           title="study icons" 
           target="_blank" 
           rel="noopener noreferrer">
          Flaticon
        </a>
        网站提供的免费第三方资源：
        <a href="https://www.flaticon.com/free-stickers/sticky-note"
           title="sticky note stickers"
           target="_blank"
           rel="noopener noreferrer">
          Sticky note stickers created by jocularityart - Flaticon
        </a>
      </p>
      <h2>样式参考</h2>
      <p>
        首页照片墙样式参考自
        <a href="https://github.com/lvy010"
           title="lvy010"
           target="_blank"
           rel="noopener noreferrer">
          lvy
        </a>
        个人网站中使用的渲染方案：
        <a href="https://lvyovo-wiki.tech/pictures"
           title="lvy-neko"
           target="_blank"
           rel="noopener noreferrer">
          lvy-neko
        </a>
      </p>
      <p>
        首页标语样式参考自
        <a href="https://www.mihoyo.com"
           title="mihoyo"
           target="_blank"
           rel="noopener noreferrer">
          米哈游
        </a>
        官方网站中使用的展示样式：
        <a href="https://www.mihoyo.com/?page=product"
           title="米哈游-TECH OTAKUS SAVE THE WORLD"
           target="_blank"
           rel="noopener noreferrer">
          米哈游-TECH OTAKUS SAVE THE WORLD
        </a>
      </p>
      <h2>协助开发</h2>
      <p>
        感谢
        <a href="https://github.com/CERN-8373558"
           title="CERN-8373558"
           target="_blank"
           rel="noopener noreferrer">
          CERN-8373558
        </a>
        协助进行服务器部署测试和服务器渗透测试
      </p>
      <p>
        感谢
        <a href="https://github.com/bob1145"
           title="yan wenyu"
           target="_blank"
           rel="noopener noreferrer">
          yan wenyu
        </a>
        协助进行服务器渗透测试
      </p>
    </div>
  </main>
</template>

<style scoped>
.thanks {
  padding: 48px 24px;
  display: flex;
  justify-content: center;
}

.thanks__card {
  max-width: 1000px;
  width: 100%;
  text-align: center;
  background-color: var(--blog-surface-bg);
  border: 1px solid var(--color-border);
  border-radius: var(--blog-surface-radius);
  padding: var(--blog-surface-padding);
  box-shadow: var(--blog-surface-shadow);
}

.thanks__card h1 {
  margin: 0 0 24px;
  color: var(--color-text);
}

.thanks__card p {
  margin: 0;
  font-size: 1rem;
  color: var(--color-text-secondary);
  line-height: 1.8;
}

.thanks__card a {
  color: var(--pink-hot);
  text-decoration: underline solid;
  margin: 2px;
}
.thanks__card a:hover {
  color: var(--purple-herrscher);
}
</style>
