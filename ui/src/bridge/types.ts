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
  stats: { label: string; value: string }[]
}

export interface Skill {
  id: string
  name: string
  level: number
  progress?: number
}

export interface Snapshot {
  schemaVersion: 1
  player: {
    name: string
    level: number
    xpProgress: number
    gold: number
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
}

export type Command =
  | { type: 'ready' }
  | { type: 'close' }
  | { type: 'equip'; formId: string; slot?: SlotId }
  | { type: 'unequip'; slot: SlotId }
  | { type: 'switchOutfit'; index: number }

export interface Bridge {
  /** True when no game is attached and data comes from the fixture. */
  readonly mock: boolean
  send(command: Command): void
}
