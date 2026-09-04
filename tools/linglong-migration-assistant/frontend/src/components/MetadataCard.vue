<template>
  <el-card shadow="never">
    <template #header>
      <div class="card-header">
        <span>软件包元信息</span>
        <el-tag :type="formatTag" effect="plain">{{ metadata.package_format.toUpperCase() }}</el-tag>
      </div>
    </template>
    <el-descriptions :column="2" border size="small">
      <el-descriptions-item label="包名">{{ metadata.name }}</el-descriptions-item>
      <el-descriptions-item label="版本">{{ metadata.version }}</el-descriptions-item>
      <el-descriptions-item label="架构">
        {{ metadata.architecture }}
        <span v-if="metadata.raw_arch && metadata.raw_arch !== metadata.architecture" class="raw-arch">
          （原始：{{ metadata.raw_arch }}）
        </span>
      </el-descriptions-item>
      <el-descriptions-item label="许可证">{{ metadata.license || '-' }}</el-descriptions-item>
      <el-descriptions-item label="维护者">{{ metadata.maintainer || '-' }}</el-descriptions-item>
      <el-descriptions-item label="主页">
        <a v-if="metadata.homepage" :href="metadata.homepage" target="_blank">{{ metadata.homepage }}</a>
        <span v-else>-</span>
      </el-descriptions-item>
      <el-descriptions-item label="描述" :span="2">{{ metadata.description || '-' }}</el-descriptions-item>
      <el-descriptions-item label="文件总数">{{ files.file_count }}</el-descriptions-item>
      <el-descriptions-item label="解压后大小">{{ formatSize(files.total_size) }}</el-descriptions-item>
    </el-descriptions>
  </el-card>
</template>

<script setup lang="ts">
import { computed } from 'vue'
import type { DetectedFiles, PackageMetadata } from '../api/client'
import { formatSize } from '../api/client'

const props = defineProps<{ metadata: PackageMetadata; files: DetectedFiles }>()

const formatTag = computed(() =>
  ({ deb: 'danger', rpm: 'warning', appimage: 'success' }[props.metadata.package_format] ?? 'info') as any,
)
</script>

<style scoped>
.card-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  font-weight: 600;
}
.raw-arch {
  color: #909399;
  font-size: 12px;
}
</style>
