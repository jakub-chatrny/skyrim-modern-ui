// A tiny stand-in for the game, used when the page runs in a normal browser.
// It answers the same commands the plugin does, from the fixture plus a few
// invented inventory items, so the whole screen can be developed without Skyrim.
import type { BridgeHandlers } from './index'
import type { Command, InventoryItem, SlotId, SlotItem, Snapshot } from './types'

// structuredClone is not guaranteed in Ultralight's WebKit; keep one way of copying.
const clone = <T>(value: T): T => JSON.parse(JSON.stringify(value)) as T

const item = (formId: string, name: string, type: SlotItem['type'], icon: string, stats: [string, string][]): SlotItem => ({
  formId,
  name,
  type,
  icon,
  stats: stats.map(([label, value]) => ({ label, value })),
})

// Extra things "in the inventory", per slot, besides what the fixture has equipped.
const SPARE: Partial<Record<SlotId, [SlotItem, number][]>> = {
  head: [
    [item('0x00012E4D', 'Iron Helmet', 'armor', 'armor-head', [['Armor', '15'], ['Weight', '5.0']]), 1],
    [item('0x00013913', 'Leather Helmet', 'armor', 'armor-head', [['Armor', '12'], ['Weight', '2.0']]), 2],
    [item('0x00010594', 'Novice Hood', 'clothing', 'clothing-head', [['Weight', '1.0']]), 1],
  ],
  body: [[item('0x00012E49', 'Iron Armor', 'armor', 'armor-body', [['Armor', '25'], ['Weight', '30.0']]), 1]],
  hands: [[item('0x00012E46', 'Iron Gauntlets', 'armor', 'armor-hands', [['Armor', '10'], ['Weight', '5.0']]), 1]],
  feet: [[item('0x00012E4B', 'Iron Boots', 'armor', 'armor-feet', [['Armor', '10'], ['Weight', '6.0']]), 1]],
  leftHand: [
    [item('0x00012EB6', 'Iron Shield', 'shield', 'shield', [['Armor', '20'], ['Weight', '12.0']]), 1],
    [item('0x0001397E', 'Iron Dagger', 'weapon', 'weapon-dagger', [['Damage', '4'], ['Speed', '1.30'], ['Weight', '2.0']]), 3],
    [item('0x0001D4EC', 'Torch', 'torch', 'torch', [['Weight', '0.5']]), 5],
  ],
  rightHand: [
    [item('0x00013790', 'Iron War Axe', 'weapon', 'weapon-axe', [['Damage', '9'], ['Speed', '0.90'], ['Weight', '11.0']]), 1],
    [item('0x0001359D', 'Iron Greatsword', 'weapon', 'weapon-greatsword', [['Damage', '15'], ['Speed', '0.70'], ['Weight', '16.0']]), 1],
    [item('0x0001397E', 'Iron Dagger', 'weapon', 'weapon-dagger', [['Damage', '4'], ['Speed', '1.30'], ['Weight', '2.0']]), 3],
  ],
  neck: [[item('0x000C8915', 'Amulet of Akatosh', 'jewelry', 'jewelry-neck', [['Weight', '1.0']]), 1]],
  ring: [
    [item('0x0001CF2B', 'Gold Ring', 'jewelry', 'jewelry-ring', [['Weight', '0.3']]), 2],
    [item('0x0003B97C', 'Silver Ring', 'jewelry', 'jewelry-ring', [['Weight', '0.3']]), 1],
  ],
  ranged: [[item('0x0003B562', 'Long Bow', 'weapon', 'weapon-bow', [['Damage', '6'], ['Speed', '1.00'], ['Weight', '5.0']]), 1]],
  ammo: [[item('0x0001397D', 'Iron Arrow', 'ammo', 'ammo-arrow', [['Damage', '8'], ['Count', '120']]), 120]],
}

export function createMockGame(fixture: Snapshot, { onSnapshot, onItems }: BridgeHandlers) {
  const state = clone(fixture)

  // Everything the mock player owns, per slot: equipped fixture items plus the spares.
  const owned = new Map<SlotId, Map<string, [SlotItem, number]>>()
  const own = (slot: SlotId, entry: SlotItem, count: number) => {
    if (!owned.has(slot)) owned.set(slot, new Map())
    owned.get(slot)!.set(entry.formId, [entry, count])
  }
  for (const [slot, equipped] of Object.entries(state.equipment) as [SlotId, SlotItem | null][]) {
    if (equipped) own(slot, equipped, 1)
  }
  for (const [slot, spares] of Object.entries(SPARE) as [SlotId, [SlotItem, number][]][]) {
    for (const [entry, count] of spares) own(slot, entry, count)
  }

  // Outfits, like plugin/src/Outfits.cpp: the active one is what is worn.
  const outfits: (Snapshot['equipment'] | null)[] = [null, null, null, null]

  const push = () => onSnapshot(clone(state))

  return {
    handle(command: Command) {
      switch (command.type) {
        case 'ready':
          push()
          break
        case 'unequip':
          state.equipment[command.slot] = null
          push()
          break
        case 'equip': {
          const entry = command.slot ? owned.get(command.slot)?.get(command.formId) : undefined
          if (command.slot && entry) {
            state.equipment[command.slot] = entry[0]
            push()
          }
          break
        }
        case 'listItems': {
          const equippedId = state.equipment[command.slot]?.formId
          const items: InventoryItem[] = [...(owned.get(command.slot)?.values() ?? [])]
            .map(([entry, count]) => ({ ...entry, count, equipped: entry.formId === equippedId }))
            .sort((a, b) => a.name.localeCompare(b.name))
          // Answer a moment later, like the real round trip.
          setTimeout(() => onItems({ slot: command.slot, items: clone(items) }), 30)
          break
        }
        case 'switchOutfit': {
          const { active } = state.outfits
          if (command.index !== active && command.index >= 0 && command.index < outfits.length) {
            outfits[active] = clone(state.equipment)
            state.equipment = clone(outfits[command.index] ?? state.equipment)
            state.outfits = {
              active: command.index,
              defined: state.outfits.defined.map((was, i) => was || i === command.index),
              missing: 0,
            }
          }
          push()
          break
        }
        case 'close':
          break
      }
    },
  }
}
