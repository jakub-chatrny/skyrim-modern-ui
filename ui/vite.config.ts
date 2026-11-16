import { svelte } from '@sveltejs/vite-plugin-svelte'
import { defineConfig, type Plugin } from 'vite'

// The built page is loaded by Prisma UI (Ultralight/WebKit) from a file on
// disk, so the bundle is one classic script: no ES modules, no absolute URLs.
// Whether Ultralight would accept <script type="module"> from disk is
// unverified; a classic script avoids the question.
function classicScript(): Plugin {
  return {
    name: 'classic-script',
    enforce: 'post',
    transformIndexHtml(html) {
      return html.replace(/<script type="module" crossorigin/g, '<script defer')
    },
  }
}

export default defineConfig({
  base: './',
  plugins: [svelte(), classicScript()],
  // The fixture lives in ../shared, outside the Vite root.
  server: { fs: { allow: ['..'] } },
  build: {
    target: 'es2019',
    modulePreload: false,
    rollupOptions: {
      output: { format: 'iife' },
    },
  },
})
