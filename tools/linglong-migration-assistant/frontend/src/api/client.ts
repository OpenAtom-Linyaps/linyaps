// SPDX-FileCopyrightText: 2026 Yanghanrui666
//
// SPDX-License-Identifier: LGPL-3.0-or-later
import axios from 'axios'

export interface PackageMetadata {
  package_format: string
  name: string
  version: string
  description: string
  architecture: string
  raw_arch: string
  maintainer: string
  homepage: string
  license: string
  raw: Record<string, string>
}

export interface DesktopEntry {
  path: string
  name: string
  exec: string
  icon: string
  wm_class: string
}

export interface DetectedFiles {
  binaries: string[]
  desktop_entries: DesktopEntry[]
  icons: string[]
  libraries: string[]
  file_count: number
  total_size: number
  notes: string[]
}

export type DepLevel = 'base' | 'runtime' | 'bundled'

export interface Dependency {
  name: string
  level: DepLevel
  constraint: string
}

export interface CheckItem {
  key: string
  title: string
  status: 'pass' | 'warn' | 'fail' | 'info'
  detail: string
}

export interface Suggested {
  id: string
  name: string
  version: string
  command: string
  base: string
  runtime: string
  bundled_deps: string[]
  yaml: string
  token: string
}

export interface AnalysisResult {
  metadata: PackageMetadata
  files: DetectedFiles
  dependencies: Dependency[]
  checklist: CheckItem[]
  suggested: Suggested
}

const http = axios.create({ baseURL: '/api', timeout: 300000 })

export async function analyzePackage(file: File): Promise<AnalysisResult> {
  const form = new FormData()
  form.append('file', file)
  const { data } = await http.post<AnalysisResult>('/analyze', form)
  return data
}

export async function exportProject(token: string, yaml: string): Promise<Blob> {
  const { data } = await http.post('/export', { token, yaml }, { responseType: 'blob' })
  return data
}

export function formatSize(bytes: number): string {
  if (!bytes) return '0 B'
  const units = ['B', 'KB', 'MB', 'GB']
  let i = 0
  let v = bytes
  while (v >= 1024 && i < units.length - 1) {
    v /= 1024
    i++
  }
  return `${v.toFixed(i === 0 ? 0 : 1)} ${units[i]}`
}
