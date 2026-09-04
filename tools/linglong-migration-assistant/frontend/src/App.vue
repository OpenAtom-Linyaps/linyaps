<template>
  <el-container>
    <el-header class="app-header">
      <div class="brand" @click="router.push('/')">
        <span class="logo">玲</span>
        <div>
          <div class="title">如意玲珑应用迁移助手</div>
          <div class="subtitle">deb / rpm / AppImage → Linyaps 一键迁移</div>
        </div>
      </div>
      <el-tag effect="dark" type="primary" round>Linyaps</el-tag>
    </el-header>

    <el-main>
      <div class="page-container">
        <el-steps :active="step" align-center finish-status="success" class="steps">
          <el-step title="上传安装包" description="deb / rpm / AppImage" />
          <el-step title="分析报告" description="依赖归属与检查清单" />
          <el-step title="生成与导出" description="linglong.yaml 工程包" />
        </el-steps>
        <router-view />
      </div>
    </el-main>

    <el-footer class="app-footer">
      面向开放原子开源大赛玲珑赛题 ·
      <a href="https://linyaps.org.cn/" target="_blank">如意玲珑官网</a>
    </el-footer>
  </el-container>
</template>

<script setup lang="ts">
import { computed } from 'vue'
import { useRoute, useRouter } from 'vue-router'

const route = useRoute()
const router = useRouter()
const step = computed(() => ({ home: 0, report: 1, export: 2 }[route.name as string] ?? 0))
</script>

<style scoped>
.app-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  background: #fff;
  border-bottom: 1px solid #e4e7ed;
  height: 64px;
}
.brand {
  display: flex;
  align-items: center;
  gap: 12px;
  cursor: pointer;
}
.logo {
  width: 40px;
  height: 40px;
  border-radius: 10px;
  background: linear-gradient(135deg, #4a5cff, #7b3ff2);
  color: #fff;
  font-size: 22px;
  display: flex;
  align-items: center;
  justify-content: center;
  font-weight: 700;
}
.title {
  font-size: 17px;
  font-weight: 600;
}
.subtitle {
  font-size: 12px;
  color: #909399;
}
.steps {
  margin-bottom: 24px;
}
.app-footer {
  text-align: center;
  color: #909399;
  font-size: 12px;
  height: 48px;
  line-height: 48px;
}
.app-footer a {
  color: #4a5cff;
  text-decoration: none;
}
</style>
