import fixture from '../../../shared/fixtures/character.json'
import type { Bridge, Command, Snapshot } from './types'

// Contract with the SKSE plugin (plugin/src/Plugin.cpp):
//   plugin -> UI : window.smuiReceive(json)   Snapshot as a JSON string
//   UI -> plugin : window.smuiCommand(json)   Command as a JSON string
// Prisma UI injects smuiCommand (RegisterJSListener); outside the game it is
// missing and the mock bridge feeds the fixture instead.
declare global {
  interface Window {
    smuiReceive?: (json: string) => void
    smuiCommand?: (json: string) => void
  }
}

const inGame = () => typeof window.smuiCommand === 'function'

/** How long to wait for the plugin before falling back to the fixture. */
const MOCK_FALLBACK_MS = 300

export function connect(onSnapshot: (snapshot: Snapshot) => void): Bridge {
  let received = false
  const bridge = {
    mock: false,
    send(command: Command) {
      if (inGame()) {
        window.smuiCommand!(JSON.stringify(command))
      } else {
        console.log('[mock bridge] command', command)
      }
    },
  }

  window.smuiReceive = (json) => {
    received = true
    onSnapshot(JSON.parse(json) as Snapshot)
  }

  bridge.send({ type: 'ready' })

  setTimeout(() => {
    if (received || inGame()) return
    bridge.mock = true
    onSnapshot(fixture as Snapshot)
  }, MOCK_FALLBACK_MS)

  return bridge
}
