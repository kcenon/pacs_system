import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'
import path from 'path'

// Where the /api and /dicomweb requests of the page go. `vite preview` uses the same proxy.
const apiTarget = process.env.PACS_API_URL ?? 'http://localhost:8080'

// https://vite.dev/config/
export default defineConfig({
  plugins: [react()],
  resolve: {
    alias: {
      '@': path.resolve(__dirname, './src'),
    },
  },
  server: {
    port: 3000,
    proxy: {
      '/api': {
        target: apiTarget,
        changeOrigin: true,
      },
    },
  },
})
