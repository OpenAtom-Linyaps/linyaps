<!--
SPDX-FileCopyrightText: 2026 Yanghanrui666
SPDX-License-Identifier: LGPL-3.0-or-later
-->
<template>
  <div v-if="store.analysis">
    <MetadataCard :metadata="store.analysis.metadata" :files="store.analysis.files" />

    <div class="section-title">检测到的应用文件</div>
    <el-card shadow="never">
      <el-row :gutter="16">
        <el-col :span="8">
          <div class="stat">
            <div class="stat-num">{{ store.analysis.files.binaries.length }}</div>
            <div class="stat-label">可执行文件</div>
            <div class="stat-detail">{{ firstOrDash(store.analysis.files.binaries) }}</div>
          </div>
        </el-col>
        <el-col :span="8">
          <div class="stat">
            <div class="stat-num">{{ store.analysis.files.desktop_entries.length }}</div>
            <div class="stat-label">desktop 文件</div>
            <div class="stat-detail">{{ firstOrDash(store.analysis.files.desktop_entries.map((d) => d.name || d.path)) }}</div>
          </div>
        </el-col>
        <el-col :span="8">
          <div class="stat">
            <div class="stat-num">{{ store.analysis.files.icons.length }}</div>
            <div class="stat-label">应用图标</div>
            <div class="stat-detail">{{ firstOrDash(store.analysis.files.icons) }}</div>
          </div>
        </el-col>
      </el-row>
      <el-alert
        v-for="(note, i) in store.analysis.files.notes"
        :key="i"
        :title="note"
        type="info"
        :closable="false"
        show-icon
        class="note"
      />
    </el-card>

    <div class="section-title">依赖分析</div>
    <DependencyTree :dependencies="store.analysis.dependencies" />

    <div class="section-title">迁移检查</div>
    <ChecklistPanel :checklist="store.analysis.checklist" />

    <div class="actions">
      <el-button @click="router.push('/')">重新上传</el-button>
      <el-button type="primary" @click="router.push('/export')">
        下一步：生成 linglong.yaml
      </el-button>
    </div>
  </div>
  <el-empty v-else description="请先上传并分析安装包">
    <el-button type="primary" @click="router.push('/')">去上传</el-button>
  </el-empty>
</template>

<script setup lang="ts">
import { useRouter } from 'vue-router'
import { store } from '../store'
import MetadataCard from '../components/MetadataCard.vue'
import DependencyTree from '../components/DependencyTree.vue'
import ChecklistPanel from '../components/ChecklistPanel.vue'

const router = useRouter()

function firstOrDash(list: string[]): string {
  return list.length > 0 ? list[0] : '-'
}
</script>

<style scoped>
.stat {
  text-align: center;
  padding: 8px 0;
}
.stat-num {
  font-size: 28px;
  font-weight: 700;
  color: #4a5cff;
}
.stat-label {
  font-size: 13px;
  color: #606266;
  margin-top: 4px;
}
.stat-detail {
  font-size: 12px;
  color: #909399;
  margin-top: 2px;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.note {
  margin-top: 10px;
}
.actions {
  margin-top: 24px;
  display: flex;
  justify-content: flex-end;
  gap: 12px;
}
</style>
