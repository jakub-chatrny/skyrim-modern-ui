import { mount } from 'svelte'
import './app.css'
import App from './App.svelte'

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
