// Hand-written mirror of shared/schema/bridge.schema.json. Keep both in sync.

export interface Gauge {
  current: number
  max: number
}

export type SlotId =
  | 'head'
  | 'body'
  | 'hands'
  | 'feet'
  | 'leftHand'
  | 'rightHand'
  | 'mask'
  | 'cloak'
  | 'neck'
  | 'ring'
  | 'ranged'
  | 'ammo'

export type ItemType =
  | 'armor'
  | 'clothing'
  | 'jewelry'
  | 'weapon'
  | 'shield'
  | 'spell'
  | 'ammo'
  | 'torch'

export interface SlotItem {
  formId: string
  name: string
  type: ItemType
  icon: string
  /** URL of the item's picture, made in game and cached; absent or null while there is none. */
  thumb?: string | null
  stats: { label: string; value: string }[]
}

/** An item in the inventory that could go into a slot. */
export interface InventoryItem extends SlotItem {
  count: number
  equipped: boolean
}

/** The plugin's answer to a `listItems` command. */
export interface ItemList {
  slot: SlotId
  items: InventoryItem[]
}

/** Four equipment sets; the active one is whatever is worn now. */
export interface Outfits {
  active: number
  /** False for an outfit that was never used; switching to it keeps the current gear. */
  defined: boolean[]
  /** Items the last switch wanted but the player no longer had. */
  missing: number
}

export interface Skill {
  id: string
  name: string
  level: number
  progress?: number
}

export interface ActiveEffect {
  name: string
  description: string
  /** Seconds left; -1 when the effect does not run out. */
  remaining: number
  positive: boolean
  kind: 'cold' | 'food' | 'other'
}

export interface Snapshot {
  schemaVersion: 1
  player: {
    name: string
    level: number
    xpProgress: number
    gold: number
    dragonSouls: number
    carryWeight: Gauge
  }
  vitals: { health: Gauge; stamina: Gauge; magicka: Gauge }
  /** Only present with Survival Mode or a needs mod. */
  survival?: { energy?: Gauge; hunger?: Gauge; thirst?: Gauge; warmth?: Gauge }
  resistances: {
    armor: number
    magic: number
    fire: number
    frost: number
    shock: number
    poison: number
    disease: number
  }
  damage: {
    rightHand: number
    leftHand: number
    ranged: number
    meleeSpeed: number
    rangedSpeed: number
    magicPower: number
  }
  skills: Skill[]
  equipment: Record<SlotId, SlotItem | null>
  outfits: Outfits
  /** Item pictures still being made; the character is off screen while this is above 0. */
  picturesPending: number
  /** Older plugins do not send it. */
  effects?: ActiveEffect[]
}

export type Command =
  | { type: 'ready' }
  | { type: 'close' }
  | { type: 'equip'; formId: string; slot?: SlotId }
  | { type: 'unequip'; slot: SlotId }
  | { type: 'listItems'; slot: SlotId }
  | { type: 'switchOutfit'; index: number }

export interface Bridge {
  /** True when no game is attached and data comes from the fixture. */
  readonly mock: boolean
  send(command: Command): void
}
