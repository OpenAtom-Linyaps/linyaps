<template>
  <el-card shadow="never">
    <template #header>
      <div class="card-header">
        <span>依赖归属分析</span>
        <span class="legend">
          <el-tag type="success" effect="plain" size="small">base 覆盖 {{ count('base') }}</el-tag>
          <el-tag type="primary" effect="plain" size="small">runtime 覆盖 {{ count('runtime') }}</el-tag>
          <el-tag type="warning" effect="plain" size="small">需自带 {{ count('bundled') }}</el-tag>
        </span>
      </div>
    </template>

    <el-empty v-if="dependencies.length === 0" description="未检测到依赖声明" :image-size="80" />

    <el-tree v-else :data="treeData" default-expand-all :expand-on-click-node="false">
      <template #default="{ data }">
        <span class="tree-node">
          <el-tag v-if="data.level" :type="levelType(data.level)" size="small" effect="plain" class="dep-tag">
            {{ levelLabel(data.level) }}
          </el-tag>
          <span :class="{ root: !data.level }">{{ data.name }}</span>
          <span v-if="data.constraint" class="constraint">{{ data.constraint }}</span>
        </span>
      </template>
    </el-tree>

    <el-alert
      v-if="count('bundled') > 0"
      type="warning"
      :closable="false"
      show-icon
      class="bundled-alert"
      :title="`${count('bundled')} 个依赖需要随应用自带，导出的工程已包含自动收集脚本`"
    />
  </el-card>
</template>

<script setup lang="ts">
import { computed } from 'vue'
import type { Dependency, DepLevel } from '../api/client'

const props = defineProps<{ dependencies: Dependency[] }>()

const GROUPS: { key: DepLevel; name: string }[] = [
  { key: 'base', name: 'base 覆盖（org.deepin.base 提供，无需处理）' },
  { key: 'runtime', name: 'runtime 覆盖（org.deepin.runtime.dtk 提供，已声明 runtime）' },
  { key: 'bundled', name: '需自带（构建时自动收集进应用包）' },
]

const LABELS: Record<DepLevel, string> = { base: 'base', runtime: 'runtime', bundled: '自带' }

const treeData = computed(() =>
  GROUPS.map((g) => ({
    name: `${g.name}（${props.dependencies.filter((d) => d.level === g.key).length}）`,
    children: props.dependencies
      .filter((d) => d.level === g.key)
      .map((d) => ({ name: d.name, constraint: d.constraint, level: d.level })),
  })).filter((g) => g.children.length > 0),
)

function count(level: DepLevel) {
  return props.dependencies.filter((d) => d.level === level).length
}
function levelType(level: DepLevel) {
  return { base: 'success', runtime: 'primary', bundled: 'warning' }[level]
}
function levelLabel(level: DepLevel) {
  return LABELS[level]
}
</script>

<style scoped>
.card-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  font-weight: 600;
}
.legend {
  display: flex;
  gap: 8px;
}
.tree-node {
  display: flex;
  align-items: center;
  gap: 8px;
  font-size: 13px;
}
.tree-node .root {
  font-weight: 600;
}
.dep-tag {
  width: 64px;
  justify-content: center;
}
.constraint {
  color: #909399;
  font-size: 12px;
}
.bundled-alert {
  margin-top: 12px;
}
</style>
