<template>
  <div>
    <el-card shadow="never" class="upload-card">
      <el-upload
        drag
        :auto-upload="false"
        :show-file-list="false"
        :on-change="handleChange"
        accept=".deb,.rpm,.AppImage,.appimage"
      >
        <el-icon class="el-icon--upload" :size="48"><UploadFilled /></el-icon>
        <div class="el-upload__text">拖拽安装包到此处，或 <em>点击选择文件</em></div>
        <template #tip>
          <div class="el-upload__tip">
            支持 deb / rpm / AppImage，单文件上限 2GB。上传后仅在本地分析，不会分发。
          </div>
        </template>
      </el-upload>
    </el-card>

    <el-row :gutter="16" class="feature-row">
      <el-col :span="8" v-for="f in features" :key="f.title">
        <el-card shadow="hover" class="feature-card">
          <div class="feature-title">{{ f.title }}</div>
          <div class="feature-desc">{{ f.desc }}</div>
        </el-card>
      </el-col>
    </el-row>
  </div>
</template>

<script setup lang="ts">
import { ref } from 'vue'
import { useRouter } from 'vue-router'
import { UploadFilled } from '@element-plus/icons-vue'
import { ElMessage, type UploadFile } from 'element-plus'
import { analyzePackage } from '../api/client'
import { setAnalysis } from '../store'

const router = useRouter()
const loading = ref(false)

const features = [
  { title: '依赖归属分析', desc: '自动将依赖划分到 base / runtime / 需自带三档，可视化呈现迁移工作量。' },
  { title: 'linglong.yaml 生成', desc: '按官方 1.14.x 规范生成构建配置，启动命令自动改写为容器内路径。' },
  { title: '工程一键导出', desc: '导出可直接 ll-builder build 的完整工程：构建脚本、依赖收集、资源文件。' },
]

async function handleChange(file: UploadFile) {
  if (loading.value) return
  if (!file.raw) return
  loading.value = true
  try {
    const result = await analyzePackage(file.raw)
    setAnalysis(result)
    ElMessage.success(`分析完成：${result.metadata.name} ${result.metadata.version}`)
    router.push('/report')
  } catch (err: any) {
    const detail = err?.response?.data?.detail || err?.message || '未知错误'
    ElMessage.error(`分析失败：${detail}`)
  } finally {
    loading.value = false
  }
}
</script>

<style scoped>
.upload-card {
  margin-bottom: 20px;
}
.upload-card :deep(.el-upload-dragger) {
  padding: 48px 0;
}
.feature-row {
  margin-top: 4px;
}
.feature-card {
  min-height: 110px;
}
.feature-title {
  font-weight: 600;
  margin-bottom: 8px;
}
.feature-desc {
  font-size: 13px;
  color: #606266;
  line-height: 1.7;
}
</style>
