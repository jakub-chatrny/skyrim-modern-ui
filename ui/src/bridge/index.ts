import fixture from '../../../shared/fixtures/character.json'
import { createMockGame } from './mock'
import type { Bridge, Command, ItemList, Snapshot } from './types'

// Contract with the SKSE plugin (plugin/src/CharacterMenu.cpp):
//   plugin -> UI : window.smuiReceive(json)   Snapshot as a JSON string
//   plugin -> UI : window.smuiItems(json)     ItemList, the answer to `listItems`
//   UI -> plugin : window.smuiCommand(json)   Command as a JSON string
// Prisma UI injects smuiCommand (RegisterJSListener); outside the game it is
// missing and a small mock game answers from the fixture instead.
declare global {
  interface Window {
    smuiReceive?: (json: string) => void
    smuiItems?: (json: string) => void
    smuiCommand?: (json: string) => void
  }
}

const inGame = () => typeof window.smuiCommand === 'function'

/** How long to wait for the plugin before falling back to the fixture. */
const MOCK_FALLBACK_MS = 300

export interface BridgeHandlers {
  onSnapshot: (snapshot: Snapshot) => void
  onItems: (list: ItemList) => void
}

export function connect({ onSnapshot, onItems }: BridgeHandlers): Bridge {
  let received = false
  const mockGame = createMockGame(fixture as Snapshot, { onSnapshot, onItems })

  const bridge = {
    mock: false,
    send(command: Command) {
      if (inGame()) {
        window.smuiCommand!(JSON.stringify(command))
      } else {
        console.log('[mock bridge] command', command)
        if (bridge.mock) mockGame.handle(command)
      }
    },
  }

  window.smuiReceive = (json) => {
    received = true
    const snapshot = JSON.parse(json) as Snapshot
    console.log('[bridge] snapshot received for ' + snapshot.player.name)
    onSnapshot(snapshot)
  }
  window.smuiItems = (json) => {
    const list = JSON.parse(json) as ItemList
    console.log(`[bridge] ${list.items.length} item(s) for ${list.slot}`)
    onItems(list)
  }

  bridge.send({ type: 'ready' })

  setTimeout(() => {
    if (received || inGame()) return
    bridge.mock = true
    mockGame.handle({ type: 'ready' })
  }, MOCK_FALLBACK_MS)

  return bridge
}
