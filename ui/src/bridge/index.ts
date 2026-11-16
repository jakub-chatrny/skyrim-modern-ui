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

// structuredClone is not guaranteed in Ultralight's WebKit.
const structuredCloneSafe = <T>(value: T): T => JSON.parse(JSON.stringify(value)) as T

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
        mockApply(command)
      }
    },
  }

  // Mock only: apply equip / unequip to a copy of the fixture, like the game would.
  const mockState = structuredCloneSafe(fixture) as Snapshot
  const mockItems = new Map(
    Object.values(mockState.equipment).flatMap((item) => (item ? [[item.formId, item] as const] : [])),
  )
  function mockApply(command: Command) {
    if (!bridge.mock) return
    if (command.type === 'unequip') {
      mockState.equipment[command.slot] = null
    } else if (command.type === 'equip' && command.slot && mockItems.has(command.formId)) {
      mockState.equipment[command.slot] = mockItems.get(command.formId)!
    } else {
      return
    }
    onSnapshot(structuredCloneSafe(mockState))
  }

  window.smuiReceive = (json) => {
    received = true
    const snapshot = JSON.parse(json) as Snapshot
    console.log('[bridge] snapshot received for ' + snapshot.player.name)
    onSnapshot(snapshot)
  }

  bridge.send({ type: 'ready' })

  setTimeout(() => {
    if (received || inGame()) return
    bridge.mock = true
    onSnapshot(structuredCloneSafe(mockState))
  }, MOCK_FALLBACK_MS)

  return bridge
}
