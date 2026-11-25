// A tiny stand-in for the game, used when the page runs in a normal browser.
// It answers the same commands the plugin does, from the fixture plus a few
// invented inventory items, so the whole screen can be developed without Skyrim.
import type { BridgeHandlers } from './index'
import type { Command, InventoryEntry, InventoryItem, SlotId, SlotItem, Snapshot } from './types'

// structuredClone is not guaranteed in Ultralight's WebKit; keep one way of copying.
const clone = <T>(value: T): T => JSON.parse(JSON.stringify(value)) as T

const item = (
  formId: string,
  name: string,
  type: SlotItem['type'],
  icon: string,
  stats: [string, string][],
  enchantment?: string,
): SlotItem => ({
  formId,
  name,
  type,
  icon,
  stats: stats.map(([label, value]) => ({ label, value })),
  enchantment: enchantment ?? null,
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
  neck: [
    [
      item('0x000C8915', 'Amulet of Akatosh', 'jewelry', 'jewelry-neck', [['Weight', '1.0']],
        'Magicka regenerates 25% faster.'),
      1,
    ],
  ],
  ring: [
    [item('0x0001CF2B', 'Gold Ring', 'jewelry', 'jewelry-ring', [['Weight', '0.3']]), 2],
    [
      item('0x000877F1', 'Gold Ring of Major Wielding', 'jewelry', 'jewelry-ring', [['Weight', '0.3']],
        'One-handed weapons do 25% more damage.'),
      1,
    ],
    [item('0x0003B97C', 'Silver Ring', 'jewelry', 'jewelry-ring', [['Weight', '0.3']]), 1],
  ],
  ranged: [[item('0x0003B562', 'Long Bow', 'weapon', 'weapon-bow', [['Damage', '6'], ['Speed', '1.00'], ['Weight', '5.0']]), 1]],
  ammo: [[item('0x0001397D', 'Iron Arrow', 'ammo', 'ammo-arrow', [['Damage', '8'], ['Count', '120']]), 120]],
}

// Inventory tab extras: things that never go into a slot.
type Extra = Omit<InventoryEntry, 'worn' | 'favorite' | 'thumb' | 'enchantments' | 'rarity'> &
  Partial<Pick<InventoryEntry, 'enchantments' | 'rarity'>>
const EXTRAS: Extra[] = [
  // Showcases for the epic and legendary frames.
  { formId: '0x0001C4E6', name: 'Dawnbreaker', category: 'weapons', typeLabel: 'One-handed · Sword', count: 1,
    stats: { damage: 12, weight: 10, value: 740 }, rarity: 'legendary',
    facts: [{ label: 'Skill', value: 'One-Handed' }, { label: 'Speed', value: '1.00' }, { label: 'Reach', value: '1.00' },
      { label: 'Stagger', value: '0.75' }, { label: 'Critical damage', value: '6' }],
    enchantments: [{ name: 'Fire Damage', element: 'fire' }],
    description: 'Burns the target for 10 points. Undead targets may explode, burning and turning nearby undead.' },
  { formId: '0x0007C110', name: 'Ring of the Storm and Frost', category: 'armor', typeLabel: 'Jewelry · Ring', count: 1,
    stats: { weight: 0.3, value: 1203 }, rarity: 'epic',
    enchantments: [{ name: 'Resist Frost', element: 'frost' }, { name: 'Resist Shock', element: 'shock' }],
    description: 'Increases Frost Resistance by 30%. Increases Shock Resistance by 30%.' },
  { formId: '0x0003EADE', name: 'Potion of Minor Healing', category: 'potions', typeLabel: 'Potion', count: 4,
    stats: { weight: 0.5, value: 17 }, description: 'Restore 25 points of Health.' },
  { formId: '0x0003EAE3', name: 'Potion of Minor Magicka', category: 'potions', typeLabel: 'Potion', count: 2,
    stats: { weight: 0.5, value: 20 }, description: 'Restore 25 points of Magicka.' },
  { formId: '0x00065C99', name: 'Sweetroll', category: 'food', typeLabel: 'Food', count: 3,
    stats: { weight: 0.5, value: 2 }, description: 'Restores 2 points of Health.' },
  { formId: '0x000F4314', name: 'Vegetable Soup', category: 'food', typeLabel: 'Food', count: 1,
    stats: { weight: 0.5, value: 5 }, description: 'Restores 1 points of Health and Stamina per second for 720 seconds.' },
  { formId: '0x00077E1C', name: 'Blue Mountain Flower', category: 'ingredients', typeLabel: 'Ingredient', count: 6,
    stats: { weight: 0.1, value: 2 }, description: 'Restore Health. Fortify Conjuration.' },
  { formId: '0x00059B86', name: 'Nirnroot', category: 'ingredients', typeLabel: 'Ingredient', count: 1,
    stats: { weight: 0.2, value: 10 }, description: 'Effects not yet discovered.' },
  { formId: '0x000A26F1', name: 'Spell Tome: Oakflesh', category: 'books', typeLabel: 'Spell Tome', count: 1,
    stats: { weight: 1, value: 45 }, description: null,
    facts: [{ label: 'Teaches', value: 'Oakflesh' }, { label: 'Learned', value: 'No' }] },
  { formId: '0x0001B3D3', name: 'The Lusty Argonian Maid, v1', category: 'books', typeLabel: 'Book', count: 1,
    stats: { weight: 1, value: 15 }, description: null },
  { formId: '0x0005ACE4', name: 'Iron Ingot', category: 'misc', typeLabel: 'Miscellaneous', count: 5,
    stats: { weight: 1, value: 7 }, description: null },
  { formId: '0x0000000A', name: 'Lockpick', category: 'misc', typeLabel: 'Miscellaneous', count: 12,
    stats: { weight: 0, value: 3 }, description: null },
  { formId: '0x0002E4FF', name: 'Petty Soul Gem', category: 'misc', typeLabel: 'Soul Gem', count: 2,
    stats: { weight: 0.1, value: 10 }, description: null,
    facts: [{ label: 'Capacity', value: 'Petty' }, { label: 'Soul', value: 'Empty' }] },
]

// Slot items as the Inventory tab sees them.
const CATEGORY: Record<SlotItem['type'], InventoryEntry['category']> = {
  armor: 'armor', clothing: 'armor', jewelry: 'armor', shield: 'armor',
  weapon: 'weapons', ammo: 'weapons', spell: 'misc', torch: 'misc',
}
const LABEL: Partial<Record<SlotId, string>> = {
  head: 'Head', body: 'Body', hands: 'Hands', feet: 'Feet', neck: 'Jewelry · Neck', ring: 'Jewelry · Ring',
  leftHand: 'Shield', rightHand: 'One-handed', ranged: 'Bow', ammo: 'Arrow',
}
const statNumber = (entry: SlotItem, label: string) => {
  const stat = entry.stats.find((s) => s.label === label)
  return stat ? Number(stat.value) : undefined
}

export function createMockGame(fixture: Snapshot, { onSnapshot, onItems, onInventory }: BridgeHandlers) {
  const state = clone(fixture)

  const favourites = new Set<string>()

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

  const extras = EXTRAS.map((extra) => ({ ...extra }))
  function inventory(): InventoryEntry[] {
    const seen = new Map<string, InventoryEntry>()
    for (const [slot, entries] of owned) {
      for (const [entry, count] of entries.values()) {
        if (seen.has(entry.formId) || entry.type === 'spell') continue
        const enchanted = entry.enchantment ? [{ name: entry.enchantment, element: 'holy' as const }] : []
        seen.set(entry.formId, {
          formId: entry.formId,
          name: entry.name,
          category: CATEGORY[entry.type],
          typeLabel: entry.type === 'weapon' || entry.type === 'ammo' ? (LABEL[slot] ?? 'Weapon') :
            entry.type === 'jewelry' ? (LABEL[slot] ?? 'Jewelry') :
            `${entry.type === 'clothing' ? 'Clothing' : entry.type === 'shield' ? 'Heavy Armor' : 'Light Armor'} · ${entry.type === 'shield' ? 'Shield' : (LABEL[slot] ?? 'Apparel')}`,
          count,
          worn: Object.values(state.equipment).some((worn) => worn?.formId === entry.formId),
          favorite: favourites.has(entry.formId),
          stats: {
            damage: statNumber(entry, 'Damage'),
            armor: statNumber(entry, 'Armor'),
            weight: statNumber(entry, 'Weight') ?? 0,
            value: statNumber(entry, 'Value') ?? 25,
          },
          enchantments: enchanted,
          rarity: enchanted.length ? 'rare' : 'common',
          description: entry.enchantment ?? null,
          thumb: entry.thumb ?? null,
        })
      }
    }
    for (const extra of extras) {
      if (extra.count <= 0) continue
      seen.set(extra.formId, {
        enchantments: [], rarity: 'common', ...extra, worn: false, favorite: favourites.has(extra.formId), thumb: null,
      })
    }
    return [...seen.values()]
  }
  const pushInventory = () => setTimeout(() => onInventory({ items: clone(inventory()) }), 30)

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
            .map(([entry, count]) => ({
              ...entry,
              count,
              equipped: entry.formId === equippedId,
              favorite: favourites.has(entry.formId),
            }))
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
        case 'favorite':
          if (!favourites.delete(command.formId)) favourites.add(command.formId)
          push()
          break
        case 'drop': {
          // Queued, as in game: the drop happens when the screen closes.
          state.dropping = [...(state.dropping ?? []), command.formId]
          for (const slot of Object.keys(state.equipment) as SlotId[]) {
            if (state.equipment[slot]?.formId === command.formId) state.equipment[slot] = null
          }
          push()
          break
        }
        case 'listInventory':
          pushInventory()
          break
        case 'use': {
          const extra = extras.find((e) => e.formId === command.formId)
          if (extra) {
            if (extra.category === 'potions' || extra.category === 'food' || extra.category === 'ingredients') extra.count--
          } else {
            for (const [slot, entries] of owned) {
              const entry = entries.get(command.formId)
              if (!entry) continue
              state.equipment[slot] = state.equipment[slot]?.formId === command.formId ? null : entry[0]
              break
            }
          }
          push()
          pushInventory()
          break
        }
        case 'close':
          break
      }
    },
  }
}
