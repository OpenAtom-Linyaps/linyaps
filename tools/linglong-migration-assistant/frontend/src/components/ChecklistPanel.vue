<!--
SPDX-FileCopyrightText: 2026 Yanghanrui666
SPDX-License-Identifier: LGPL-3.0-or-later
-->
<template>
  <el-card shadow="never">
    <template #header><span class="header">迁移检查清单</span></template>
    <div v-for="item in checklist" :key="item.key" class="check-item">
      <el-icon :size="20" :color="STATUS_COLOR[item.status]">
        <CircleCheckFilled v-if="item.status === 'pass'" />
        <WarningFilled v-else-if="item.status === 'warn'" />
        <CircleCloseFilled v-else-if="item.status === 'fail'" />
        <InfoFilled v-else />
      </el-icon>
      <div class="content">
        <div class="title">
          {{ item.title }}
          <el-tag size="small" :type="STATUS_TAG[item.status]" effect="plain">
            {{ STATUS_LABEL[item.status] }}
          </el-tag>
        </div>
        <div class="detail">{{ item.detail }}</div>
      </div>
    </div>
  </el-card>
</template>

<script setup lang="ts">
import type { CheckItem } from '../api/client'
import { CircleCheckFilled, CircleCloseFilled, InfoFilled, WarningFilled } from '@element-plus/icons-vue'

defineProps<{ checklist: CheckItem[] }>()

const STATUS_COLOR: Record<string, string> = {
  pass: '#67c23a',
  warn: '#e6a23c',
  fail: '#f56c6c',
  info: '#409eff',
}
const STATUS_TAG: Record<string, any> = { pass: 'success', warn: 'warning', fail: 'danger', info: 'info' }
const STATUS_LABEL: Record<string, string> = { pass: '通过', warn: '注意', fail: '待处理', info: '说明' }
</script>

<style scoped>
.header {
  font-weight: 600;
}
.check-item {
  display: flex;
  gap: 12px;
  padding: 10px 4px;
  border-bottom: 1px dashed #ebeef5;
}
.check-item:last-child {
  border-bottom: none;
}
.title {
  font-weight: 600;
  font-size: 14px;
  display: flex;
  align-items: center;
  gap: 8px;
}
.detail {
  font-size: 13px;
  color: #606266;
  margin-top: 4px;
  line-height: 1.6;
}
</style>
