import { createRouter, createWebHistory } from 'vue-router'

const router = createRouter({
  history: createWebHistory(),
  routes: [
    { path: '/', name: 'home', component: () => import('../views/HomeView.vue') },
    { path: '/report', name: 'report', component: () => import('../views/ReportView.vue') },
    { path: '/export', name: 'export', component: () => import('../views/ExportView.vue') },
  ],
})

export default router
