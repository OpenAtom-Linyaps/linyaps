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
