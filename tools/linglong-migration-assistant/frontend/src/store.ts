// SPDX-FileCopyrightText: 2026 Yanghanrui666
//
// SPDX-License-Identifier: LGPL-3.0-or-later
import { reactive } from 'vue'
import type { AnalysisResult } from './api/client'

interface AppState {
  analysis: AnalysisResult | null
  yaml: string
}

export const store = reactive<AppState>({
  analysis: null,
  yaml: '',
})

export function setAnalysis(result: AnalysisResult) {
  store.analysis = result
  store.yaml = result.suggested.yaml
}
