import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'
import { viteSingleFile } from 'vite-plugin-singlefile'

// The dashboard is loaded as a file:// page inside an embedded WebKitWebView.
// ES modules with crossorigin are blocked over file:// (opaque origin → CORS
// failure), which leaves the panel blank. viteSingleFile inlines the JS and CSS
// into one self-contained index.html that loads correctly from file://.
export default defineConfig({
  plugins: [react(), viteSingleFile()],
  base: './',
  build: {
    outDir: 'dist',
    assetsDir: 'assets',
    cssCodeSplit: false,
    assetsInlineLimit: 100000000,
    rollupOptions: {
      output: {
        manualChunks: undefined,
        inlineDynamicImports: true,
      },
    },
  },
  server: {
    port: 5179,
    strictPort: true,
  },
})
