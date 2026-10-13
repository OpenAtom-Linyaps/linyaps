// SPDX-FileCopyrightText: 2026 Yanghanrui666
//
// SPDX-License-Identifier: LGPL-3.0-or-later
import { createApp } from 'vue'
import ElementPlus from 'element-plus'
import 'element-plus/dist/index.css'
import App from './App.vue'
import router from './router'
import './style.css'

const app = createApp(App)
app.use(ElementPlus)
app.use(router)
app.mount('#app')
