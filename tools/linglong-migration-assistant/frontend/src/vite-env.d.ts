// SPDX-FileCopyrightText: 2026 Yanghanrui666
//
// SPDX-License-Identifier: LGPL-3.0-or-later
/// <reference types="vite/client" />

declare module '*.vue' {
  import type { DefineComponent } from 'vue'
  const component: DefineComponent<{}, {}, any>
  export default component
}
