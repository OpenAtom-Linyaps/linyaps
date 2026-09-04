<template>
  <div v-if="store.analysis">
    <el-alert
      type="success"
      :closable="false"
      show-icon
      class="summary"
      :title="`已按官方规范生成 linglong.yaml：${store.analysis.suggested.id} ${store.analysis.suggested.version}`"
      :description="`启动命令 ${store.analysis.suggested.command || '（未检测到，请手动补充 command 字段）'} · base ${store.analysis.suggested.base}${store.analysis.suggested.runtime ? ' · runtime ' + store.analysis.suggested.runtime : ''}`"
    />

    <div class="section-title">linglong.yaml（可编辑）</div>
    <el-card shadow="never" class="editor-card">
      <Codemirror
        v-model="store.yaml"
        :extensions="extensions"
        :style="{ height: '420px' }"
        placeholder="linglong.yaml"
      />
    </el-card>

    <div class="section-title">导出内容</div>
    <el-card shadow="never">
      <el-check-tag checked class="include-item">linglong.yaml</el-check-tag>
      <el-check-tag checked class="include-item">README.md（构建指引）</el-check-tag>
      <el-check-tag checked class="include-item">deps.txt</el-check-tag>
      <el-check-tag checked class="include-item">srcs/（放置原始包）</el-check-tag>
      <el-check-tag v-if="resourceCount > 0" checked class="include-item">
        resources/（{{ resourceCount }} 个 desktop/icon）
      </el-check-tag>
    </el-card>

    <div class="actions">
      <el-button @click="router.push('/report')">上一步</el-button>
      <el-button type="primary" size="large" :loading="exporting" @click="handleExport">
        <el-icon class="btn-icon"><Download /></el-icon>
        下载迁移工程 zip
      </el-button>
    </div>
  </div>
  <el-empty v-else description="请先上传并分析安装包">
    <el-button type="primary" @click="router.push('/')">去上传</el-button>
  </el-empty>
</template>

<script setup lang="ts">
import { computed, ref } from 'vue'
import { useRouter } from 'vue-router'
import { Codemirror } from 'vue-codemirror'
import { yaml as yamlLang } from '@codemirror/lang-yaml'
import { oneDark } from '@codemirror/theme-one-dark'
import { ElMessage } from 'element-plus'
import { Download } from '@element-plus/icons-vue'
import { exportProject } from '../api/client'
import { store } from '../store'

const router = useRouter()
const exporting = ref(false)
const extensions = [yamlLang(), oneDark]

const resourceCount = computed(
  () =>
    store.analysis!.files.desktop_entries.length + store.analysis!.files.icons.length,
)

async function handleExport() {
  exporting.value = true
  try {
    const blob = await exportProject(store.analysis!.suggested.token, store.yaml)
    const url = URL.createObjectURL(blob)
    const a = document.createElement('a')
    a.href = url
    a.download = `${store.analysis!.suggested.id}-linglong-project.zip`
    a.click()
    URL.revokeObjectURL(url)
    ElMessage.success('工程导出成功，将原包放入 srcs/ 后即可 ll-builder build')
  } catch (err: any) {
    const detail = err?.response?.data?.detail || err?.message || '未知错误'
    ElMessage.error(`导出失败：${detail}`)
  } finally {
    exporting.value = false
  }
}
</script>

<style scoped>
.summary {
  margin-bottom: 8px;
}
.editor-card :deep(.cm-editor) {
  border-radius: 6px;
  font-size: 13px;
}
.include-item {
  margin: 0 8px 8px 0;
}
.btn-icon {
  margin-right: 6px;
}
.actions {
  margin-top: 24px;
  display: flex;
  justify-content: flex-end;
  gap: 12px;
}
</style>
