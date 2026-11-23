import { mount } from 'svelte'
// Stand-ins for the design's Futura faces (see app.css).
import '@fontsource/jost/400.css'
import '@fontsource/jost/500.css'
import '@fontsource/jost/500-italic.css'
import '@fontsource/jost/700.css'
import '@fontsource/barlow-condensed/500.css'
import '@fontsource/barlow-condensed/700.css'
import '@fontsource/alegreya/400.css'
import '@fontsource/inter/400.css'
import './app.css'
import App from './App.svelte'

// Ultralight's WebKit has no HTMLMediaElement. Svelte tests `instanceof
// HTMLMediaElement` when it attaches load / error listeners (as on <img>), and
// that throws without the class. An empty stand-in makes the test just false.
if (typeof window.HTMLMediaElement === 'undefined') {
  ;(window as unknown as { HTMLMediaElement: unknown }).HTMLMediaElement = class {}
}

// In game the console is forwarded to SkyrimModernUI.log; make sure script
// errors reach it with a location.
window.addEventListener('error', (event) => {
  console.error(`uncaught: ${event.message} at ${event.filename}:${event.lineno}:${event.colno}`)
})
window.addEventListener('unhandledrejection', (event) => {
  console.error(`unhandled rejection: ${String(event.reason)}`)
})

const app = mount(App, {
  target: document.getElementById('app')!,
})

export default app
