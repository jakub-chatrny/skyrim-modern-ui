// Line icons from the "Active Effect Icons" design canvas (sheets "Active
// effect icons" and "Skills, resistances & damage types"): 24-unit grid,
// drawn with a 1.5 stroke, round caps and joins, no fill. Each is one SVG path;
// colour comes from the family (effects) or the tree / element (skills, stats).
import type { ActiveEffect } from '../bridge/types'

/** A circle as a path, as the canvas builds them. */
const C = (cx: number, cy: number, r: number) => `M${cx} ${cy - r}A${r} ${r} 0 1 1 ${cx - 0.01} ${cy - r}Z`

// ---- colours ------------------------------------------------------------------

export const FAMILY = {
  boon: '#A8E6FF', // timed, helpful
  passive: '#D8C38E', // worn, innate or earned
  dual: '#B9A3FF', // vampirism, lycanthropy
  curse: '#E2636E', // disease, poison, harm
  need: '#F2A65A', // Survival Mode
} as const
export type Family = keyof typeof FAMILY

export const TREE = { warrior: '#EFA07E', mage: '#93B6FF', thief: '#8FD3B0' } as const

export const ELEMENT = {
  physical: '#C9D6DE',
  fire: '#F2875A',
  frost: '#A8E6FF',
  shock: '#C7B2FF',
  magic: '#D8C38E',
  poison: '#9ED37A',
  disease: '#C2C98A',
  bleed: '#E2636E',
  absorb: '#E89BC8',
  sun: '#FFD27A',
} as const

// ---- shared parts -------------------------------------------------------------

const STONE = 'M6.5 21V8.5C6.5 5 9 3 12 3C15 3 17.5 5 17.5 8.5V21M4 21H20'
const AMULET = 'M7.5 3L12 8.5L16.5 3' + C(12, 14.5, 6)
const BONE = C(7.8, 5.2, 1.8) + C(5.2, 7.8, 1.8) + C(18.8, 16.2, 1.8) + C(16.2, 18.8, 1.8)
export const DROP = 'M12 3C12 3 6.5 9.5 6.5 14A5.5 5.5 0 0 0 17.5 14C17.5 9.5 12 3 12 3Z'
const FLAME_IN =
  'M12 7.5C12.9 9.4 15 10.4 15 13.1A3 3 0 0 1 9 13.1C9 11.9 9.6 11.1 10.2 10.6C10.4 11.6 10.9 12.1 11.4 12.3C11.1 10.7 11.3 9.1 12 7.5Z'
const SNOW =
  'M12 12L12.0 2.5M14.0 3.4L12.0 5.4L10.0 3.4M12 12L20.2 7.3M20.4 9.4L17.7 8.7L18.4 6.0M12 12L20.2 16.8M18.4 18.0L17.7 15.3L20.4 14.6M12 12L12.0 21.5M10.0 20.6L12.0 18.6L14.0 20.6M12 12L3.8 16.8M3.6 14.6L6.3 15.3L5.6 18.0M12 12L3.8 7.2M5.6 6.0L6.3 8.7L3.6 9.4'
const SHIELD = 'M12 3L19 5.5V11C19 15.5 16 19 12 21C8 19 5 15.5 5 11V5.5Z'

// ---- active effects -----------------------------------------------------------

export const EFFECT = {
  oakflesh:
    'M12 21V9M12 3C9.5 4 9.5 6 10.5 6.8C8.5 6.5 7 8 8 9.8C6.5 10 6 12 7.5 13C6.5 14 7.5 16 9.5 15.5C9.5 17 11 18 12 17.5C13 18 14.5 17 14.5 15.5C16.5 16 17.5 14 16.5 13C18 12 17.5 10 16 9.8C17 8 15.5 6.5 13.5 6.8C14.5 6 14.5 4 12 3Z',
  muffle:
    'M8 4.5C9.9 4.5 11 6.3 11 8.6C11 11 10 12.4 10 14.2H6C6 12.4 5 11 5 8.6C5 6.3 6.1 4.5 8 4.5ZM6.2 16.5H9.8V18A1.8 1.8 0 0 1 6.2 18ZM15 9.5C16.2 10.7 16.2 13.3 15 14.5M17.8 7C20.2 9.4 20.2 14.6 17.8 17M13.5 18.5L20.5 5.5',
  flameCloak:
    'M12 5C13.2 7.6 16 9 16 12.6A4 4 0 0 1 8 12.6C8 11 8.8 9.9 9.6 9.2C9.9 10.6 10.5 11.3 11.3 11.6C10.9 9.4 11.2 7 12 5ZM4.5 15.5A8.5 8.5 0 0 1 4.8 8M19.5 15.5A8.5 8.5 0 0 0 19.2 8M7 20A8.5 8.5 0 0 0 17 20',
  candlelight:
    C(12, 11, 3.5) +
    'M12 3V4.8M12 17.2V19M4 11H5.8M18.2 11H20M6.3 5.3L7.6 6.6M16.4 15.4L17.7 16.7M17.7 5.3L16.4 6.6M7.6 15.4L6.3 16.7M10.5 21.5H13.5',
  smithing:
    'M10 3H14M10.5 3V8.2L6.3 15.5A3.3 3.3 0 0 0 9.2 20.5H14.8A3.3 3.3 0 0 0 17.7 15.5L13.5 8.2V3M7.8 13H16.2M9.5 18L12 15.5L14.5 18',
  stew: 'M4 12H20C20 16.4 16.4 19.5 12 19.5C7.6 19.5 4 16.4 4 12ZM9 21.5H15M9 9C8 7.8 10 6.7 9 5.5M12 9C11 7.6 13 6.3 12 4.5M15 9C14 7.8 16 6.7 15 5.5',
  fortHealth: 'M10 2.5H14M10.5 2.5V7.6A6.5 6.5 0 1 0 13.5 7.6V2.5M12 11.5V17.5M9 14.5H15',
  oneHanded: 'M10 2.5L11.5 5V14H8.5V5ZM6 14H14M10 14V19M8.8 21H11.2M18 3.5V8.5M15.5 6H20.5M17.5 13V16M16 14.5H19',
  resistFire: 'M5 4H19V11C19 15.5 16 18.8 12 21C8 18.8 5 15.5 5 11Z' + FLAME_IN,
  warrior: STONE + 'M12 8V15.5M10 13.5H14M12 15.5V17.5',
  lord: STONE + 'M9.3 9H14.7V12.5C14.7 14.3 13.5 15.6 12 16.5C10.5 15.6 9.3 14.3 9.3 12.5Z',
  lover: STONE + 'M12 16.2C10 14.8 8.8 13.4 8.8 11.8A1.7 1.7 0 0 1 12 10.9A1.7 1.7 0 0 1 15.2 11.8C15.2 13.4 14 14.8 12 16.2Z',
  stone: STONE,
  nord: 'M6 13C6 8.5 8.7 6 12 6C15.3 6 18 8.5 18 13V18H6ZM12 6V18M6 11C4 10 3 7.5 3.5 5M18 11C20 10 21 7.5 20.5 5M8.5 13.5H10.5M13.5 13.5H15.5',
  khajiit: 'M6.5 4C9 8 9.5 14 7.5 20M11.5 3.5C14 8 14.5 14 12.5 20.5M16.5 4C19 8 19.5 14 17.5 20',
  talos: AMULET + 'M9.3 16.6L8.9 12.8L10.8 14.2L12 11.8L13.2 14.2L15.1 12.8L14.7 16.6Z',
  akatosh: AMULET + 'M9.8 11.8H14.2M9.8 17.2H14.2M10.3 11.8L13.7 17.2M13.7 11.8L10.3 17.2',
  daedric: AMULET + 'M8.8 14.5C10.6 12.2 13.4 12.2 15.2 14.5C13.4 16.8 10.6 16.8 8.8 14.5ZM12 13.8V15.2',
  rested: 'M20 13.2A8 8 0 1 1 10.8 4A6.2 6.2 0 0 0 20 13.2Z',
  wellRested: 'M19 15.2A7 7 0 1 1 9.8 6.2A5.4 5.4 0 0 0 19 15.2ZM17 2.8V6.8M15 4.8H19',
  loversComfort:
    'M9.5 19C6 16.5 3.5 14 3.5 11A3 3 0 0 1 9.5 9.6A3 3 0 0 1 15.5 11C15.5 14 13 16.5 9.5 19ZM14.5 7.2A3 3 0 0 1 20.5 8.6C20.5 11.4 18.4 13.7 15.6 15.8',
  hearth: 'M4 11L12 4.5L20 11M6 9.5V20H18V9.5M10 20V15H14V20',
  rockjoint: BONE + 'M8.6 7.3L16.7 15.4M7.3 8.6L15.4 16.7M15.5 6.5H15.51M18.5 9H18.51M19 5.5H19.01',
  ataxia: 'M12 12A1.5 1.5 0 1 1 13.5 13.5A3 3 0 1 1 10.5 10.5A4.5 4.5 0 1 1 15 15M5 20L7 18.5M19 4L17 5.5',
  boneBreak: BONE + 'M8.6 7.3L11.6 10.3L10.9 11L10.3 11.6L7.3 8.6M16.7 15.4L13.7 12.4L13.1 13L12.4 13.7L15.4 16.7M18.5 6V8M4 17.5H6',
  sanguinare: DROP + 'M10.3 12.5L10.8 15M13.7 12.5L13.2 15',
  vampire: 'M4 8.5C7 10.5 17 10.5 20 8.5M8.5 10L9.5 15.5L10.5 10.3M13.5 10.3L14.5 15.5L15.5 10M6 15.5C9 17.8 15 17.8 18 15.5',
  lycan: C(12, 12, 7.5) + 'M8.5 8L10.8 15.5M11.5 7.2L13.8 14.8M14.5 7.5L16.2 13.5',
  mara: 'M12 20C9 17 8.5 12.5 12 7C15.5 12.5 15 17 12 20ZM12 20C8 19.5 4.5 17 4 12C7.5 12.2 10 14 12 20ZM12 20C16 19.5 19.5 17 20 12C16.5 12.2 14 14 12 20ZM12 3.5V4.5',
  ethereal: C(12, 6.6, 2.6) + 'M6 21V17C6 13.8 8.7 11.5 12 11.5C15.3 11.5 18 13.8 18 17V21',
  dragonhide:
    'M4 6A4 4 0 0 0 12 6A4 4 0 0 0 20 6M4 11A2.5 4 0 0 0 8 11A4 4 0 0 0 16 11A2.5 4 0 0 0 20 11M4 16A4 4 0 0 0 12 16A4 4 0 0 0 20 16',
  poison: 'M7 11A5 5 0 1 1 17 11V14.5H15V17H9V14.5H7ZM9.5 11.5H9.51M14.5 11.5H14.51M12 19.5V21.5',
  frostSlow: 'M9 4V16M4.5 6.5L13.5 13.5M13.5 6.5L4.5 13.5M17.5 6V19M15 16.5L17.5 19L20 16.5',
  paralysis: C(12, 5.5, 2) + 'M12 8V15M8.5 9.5H15.5M10 21L12 15L14 21M4.5 4V20M19.5 4V20',
  bleed: 'M12 17C7.5 14 5 11.5 5 8.6A3.3 3.3 0 0 1 12 7.2A3.3 3.3 0 0 1 19 8.6C19 11.5 16.5 14 12 17ZM8 20.5H8.01M12 21.5H12.01M16 20.5H16.01',
  hunger:
    'M14.5 3.5A5 5 0 0 1 18.5 12C17 13.5 14.5 13.5 13 13L9.5 16.5A2 2 0 1 1 6.5 18A2 2 0 1 1 8 15L11.5 11.5C11 10 11 7.5 12.5 6A5 5 0 0 1 14.5 3.5Z',
  fatigue:
    'M3.5 11C6 14.5 9 16 12 16C15 16 18 14.5 20.5 11M3.5 11C6 12.5 9 13 12 13C15 13 18 12.5 20.5 11M7 15L6 17M12 16V18.2M17 15L18 17',
  cold: SNOW,
} as const

// ---- skills -------------------------------------------------------------------

/** By actor value name, as the plugin sends skill ids. */
export const SKILL: Record<string, { path: string; tree: keyof typeof TREE }> = {
  OneHanded: { tree: 'warrior', path: 'M12 2.5L13.5 5V14H10.5V5ZM8 14H16M12 14V19M10.8 21H13.2' },
  TwoHanded: {
    tree: 'warrior',
    path: 'M12 3V21M12 5C8.5 4.5 5.5 6.5 5 10.5C6.5 9.5 9.5 9.5 12 11M12 5C15.5 4.5 18.5 6.5 19 10.5C17.5 9.5 14.5 9.5 12 11M10.5 21H13.5',
  },
  Archery: { tree: 'warrior', path: 'M6 3C12 6.5 12 17.5 6 21M6 3V21M3.5 12H20.5M18 9.5L20.5 12L18 14.5' },
  Block: {
    tree: 'warrior',
    path: 'M3.5 12A8.5 8.5 0 1 0 20.5 12A8.5 8.5 0 1 0 3.5 12M9.8 12A2.2 2.2 0 1 0 14.2 12A2.2 2.2 0 1 0 9.8 12M12 3.5V9.8M12 14.2V20.5M3.5 12H9.8M14.2 12H20.5',
  },
  HeavyArmor: { tree: 'warrior', path: 'M8 3.5L12 5.5L16 3.5L20 6V11L17.5 12V20H6.5V12L4 11V6ZM12 5.5V20M6.5 15H17.5' },
  Smithing: {
    tree: 'warrior',
    path: 'M3.5 9H17.5C17.5 11.5 15.5 13 13 13.3V16.5H16V19.5H7V16.5H10V13.3C7.5 13 5.5 11.8 5 10.5H3.5ZM15.5 2.5L19.5 6.5M17.5 4.5L21 1',
  },
  Alteration: { tree: 'mage', path: 'M12 3L19.8 7.5V16.5L12 21L4.2 16.5V7.5ZM12 8L16 12L12 16L8 12Z' },
  Conjuration: { tree: 'mage', path: 'M4 18A8 2.8 0 1 0 20 18A8 2.8 0 1 0 4 18ZM8 16.5V9A4 4 0 0 1 16 9V16.5M12 7V11M10 9H14' },
  Destruction: {
    tree: 'mage',
    path: 'M12 3L13.8 9L20 7.5L15.5 12L20 16.5L13.8 15L12 21L10.2 15L4 16.5L8.5 12L4 7.5L10.2 9Z',
  },
  Enchanting: { tree: 'mage', path: 'M12 2.5L17.5 8L12 21.5L6.5 8ZM6.5 8H17.5M9.5 8L12 21.5M14.5 8L12 21.5M9.5 8L12 2.5M14.5 8L12 2.5' },
  Illusion: {
    tree: 'mage',
    path: 'M2.5 12C5 7.5 8.3 5.5 12 5.5C15.7 5.5 19 7.5 21.5 12C19 16.5 15.7 18.5 12 18.5C8.3 18.5 5 16.5 2.5 12Z' + C(12, 12, 3) + 'M12 2V3.5M12 20.5V22',
  },
  Restoration: { tree: 'mage', path: C(12, 12, 5) + 'M12 9.5V14.5M9.5 12H14.5M12 2.5V4.5M12 19.5V21.5M2.5 12H4.5M19.5 12H21.5' },
  Alchemy: { tree: 'thief', path: 'M4 11H20C20 15.5 16.5 19 12 19C7.5 19 4 15.5 4 11ZM9 21.5H15M12 19V21.5M13 11L18.5 3.5' },
  LightArmor: { tree: 'thief', path: 'M8 3.5L11 7V20H5V6ZM16 3.5L13 7V20H19V6ZM11 10H13M11 14H13' },
  Lockpicking: {
    tree: 'thief',
    path: 'M4 10H15V20.5H4ZM6.5 10V7A3 3 0 0 1 12.5 7V10' + C(9.5, 14.2, 1.3) + 'M9.5 15.5V17.5M10.5 13.2L20.5 3.5M18.5 3.5H20.5V5.5',
  },
  Pickpocket: {
    tree: 'thief',
    path: 'M9 7L7.5 4H16.5L15 7ZM9 7C5 10 4 14 5 17C5.8 19.5 8.5 20.5 12 20.5C15.5 20.5 18.2 19.5 19 17C20 14 19 10 15 7M10 13H14M12 11V15',
  },
  Sneak: { tree: 'thief', path: 'M4 21C4 12 6.5 4 12 3C17.5 4 20 12 20 21M8 21C8 15 9.5 11 12 11C14.5 11 16 15 16 21M10.5 15H10.51M13.5 15H13.51' },
  Speech: { tree: 'thief', path: 'M4 5H20V15H11L6.5 19V15H4ZM8 9H16M8 12H13' },
}

// ---- resistances and damage ---------------------------------------------------

export const RESIST = {
  armor: { color: ELEMENT.physical, path: SHIELD + 'M12 6.5L16.5 8V11.5C16.5 14.3 14.6 16.6 12 18C9.4 16.6 7.5 14.3 7.5 11.5V8Z' },
  fire: { color: ELEMENT.fire, path: SHIELD + FLAME_IN },
  frost: { color: ELEMENT.frost, path: SHIELD + 'M12 8V16M8.5 10L15.5 14M15.5 10L8.5 14' },
  shock: { color: ELEMENT.shock, path: SHIELD + 'M12.8 7.5L9.5 12.5H12.5L11.2 16.5L14.5 11.5H11.5Z' },
  magic: { color: ELEMENT.magic, path: SHIELD + 'M12 7.5L13 11L16.5 12L13 13L12 16.5L11 13L7.5 12L11 11Z' },
  poison: { color: ELEMENT.poison, path: SHIELD + 'M12 8C12 8 9.5 11 9.5 13A2.5 2.5 0 0 0 14.5 13C14.5 11 12 8 12 8Z' },
  disease: {
    color: ELEMENT.disease,
    path: SHIELD + C(12, 12.5, 2.2) + 'M12 8.5V10.3M12 14.7V16.5M8 12.5H9.8M14.2 12.5H16M9.2 9.7L10.4 10.9M13.6 14.1L14.8 15.3M14.8 9.7L13.6 10.9M10.4 14.1L9.2 15.3',
  },
} as const

export const DAMAGE = {
  physical: { color: ELEMENT.physical, path: 'M19.5 4.5L9.5 14.5M19.5 4.5H16.5M19.5 4.5V7.5M7 12L12 17M9.5 14.5L4.5 19.5' },
  fire: {
    color: ELEMENT.fire,
    path: 'M12 2.5C13.6 6.4 18.5 8.6 18.5 14A6.5 6.5 0 0 1 5.5 14C5.5 11.3 6.8 9.6 8.1 8.5C8.5 10.8 9.5 12 10.8 12.4C10.1 8.8 10.7 5.5 12 2.5Z',
  },
  frost: { color: ELEMENT.frost, path: SNOW },
  shock: { color: ELEMENT.shock, path: 'M13.5 2.5L5.5 13.5H11.5L10.5 21.5L18.5 10.5H12.5Z' },
  magic: { color: ELEMENT.magic, path: 'M12 3L13.8 9L20 7.5L15.5 12L20 16.5L13.8 15L12 21L10.2 15L4 16.5L8.5 12L4 7.5L10.2 9Z' },
} as const

// ---- choosing an effect's icon ------------------------------------------------

export interface Glyph {
  path: string
  color: string
  /** The canvas draws Become Ethereal dashed. */
  dashed?: boolean
}

const has = (text: string, ...words: string[]) => words.some((word) => text.includes(word))

/**
 * Icon and colour for an active effect, from what the plugin says it is.
 * Names are English: the plugin reports the game's own names.
 */
export function effectGlyph(effect: ActiveEffect): Glyph {
  const name = effect.name.toLowerCase()
  const av = effect.av ?? ''
  const archetype = effect.archetype ?? ''
  const source = effect.source ?? ''
  const lasting = effect.remaining < 0
  const glyph = (path: string, family: Family): Glyph => ({ path, color: FAMILY[family] })

  // Survival Mode needs, by the stage names the game uses.
  if (effect.kind === 'cold' && !effect.racial && source === 'ability' && !av.startsWith('Resist'))
    return { path: EFFECT.cold, color: effect.positive ? FAMILY.need : FAMILY.curse }
  if (has(name, 'warm', 'chilly', 'cold', 'freezing', 'numb', 'comfortable') && source === 'ability')
    return { path: EFFECT.cold, color: effect.positive ? FAMILY.need : FAMILY.curse }
  if (has(name, 'peckish', 'hungry', 'starving', 'ravenous', 'satiated', 'fed') && source === 'ability')
    return glyph(EFFECT.hunger, 'need')
  if (has(name, 'drained', 'tired', 'weary', 'debilitated', 'exhausted') && source === 'ability')
    return glyph(EFFECT.fatigue, 'need')

  // Rest and home bonuses.
  if (name.includes('well rested')) return glyph(EFFECT.wellRested, 'boon')
  if (name.includes('rested')) return glyph(EFFECT.rested, 'boon')
  if (name.includes("lover's comfort") || name.includes('lover’s comfort')) return glyph(EFFECT.loversComfort, 'boon')

  // Standing stones and shrine blessings.
  if (name.includes('stone')) {
    if (name.includes('warrior')) return glyph(EFFECT.warrior, 'passive')
    if (name.includes('lord')) return glyph(EFFECT.lord, 'passive')
    if (name.includes('lover')) return glyph(EFFECT.lover, 'passive')
    return glyph(EFFECT.stone, 'passive')
  }
  if (name.includes('blessing')) {
    if (name.includes('talos')) return glyph(EFFECT.talos, 'boon')
    if (name.includes('akatosh')) return glyph(EFFECT.akatosh, 'boon')
    if (has(name, 'azura', 'boethiah', 'mephala', 'namira', 'nocturnal', 'daedric')) return glyph(EFFECT.daedric, 'boon')
    return glyph(EFFECT.akatosh, 'boon')
  }
  if (name.includes('agent of mara') || name.includes('mara')) return glyph(EFFECT.mara, 'passive')

  // Vampirism and lycanthropy.
  if (name.includes('sanguinare')) return glyph(EFFECT.sanguinare, 'curse')
  if (has(name, 'vampire', 'vampiric')) return glyph(EFFECT.vampire, 'dual')
  if (has(name, 'lycanthropy', 'beast blood', 'werewolf')) return glyph(EFFECT.lycan, 'dual')

  // Diseases.
  if (source === 'disease') {
    if (name.includes('rockjoint')) return glyph(EFFECT.rockjoint, 'curse')
    if (name.includes('ataxia')) return glyph(EFFECT.ataxia, 'curse')
    if (name.includes('bone break')) return glyph(EFFECT.boneBreak, 'curse')
    return glyph(EFFECT.rockjoint, 'curse')
  }

  // Racial abilities.
  if (effect.racial) {
    if (av === 'ResistFrost' || name.includes('nord')) return glyph(EFFECT.nord, 'passive')
    if (name.includes('claw')) return glyph(EFFECT.khajiit, 'passive')
  }

  // Food and potions.
  if (effect.kind === 'food') return glyph(EFFECT.stew, 'boon')
  if (source === 'poison' || (!effect.positive && av === 'Health' && archetype === 'ValueModifier' && source !== 'potion'))
    return glyph(source === 'poison' ? EFFECT.poison : EFFECT.bleed, 'curse')

  // Hostile effects by kind.
  if (!effect.positive) {
    if (archetype === 'Paralysis') return glyph(EFFECT.paralysis, 'curse')
    if (av === 'SpeedMult') return glyph(EFFECT.frostSlow, 'curse')
    if (has(av, 'Health', 'Stamina', 'Magicka')) return glyph(EFFECT.bleed, 'curse')
  }

  // Shouts.
  if (source === 'shout' || archetype === 'Etherealize') {
    if (archetype === 'Etherealize') return { ...glyph(EFFECT.ethereal, 'boon'), dashed: true }
    return glyph(EFFECT.dragonhide, 'boon')
  }

  // Spells on self, by what they do.
  if (archetype === 'Cloak') return glyph(EFFECT.flameCloak, 'boon')
  if (archetype === 'Light') return glyph(EFFECT.candlelight, 'boon')
  if (av === 'MovementNoiseMult' || name.includes('muffle')) return glyph(EFFECT.muffle, 'boon')
  if (av === 'DamageResist' && source === 'spell') return glyph(EFFECT.oakflesh, 'boon')

  // Resistances: the shield icons, coloured by element.
  const resist: Record<string, keyof typeof RESIST> = {
    ResistFire: 'fire',
    ResistFrost: 'frost',
    ResistShock: 'shock',
    ResistMagic: 'magic',
    PoisonResist: 'poison',
    ResistDisease: 'disease',
    DamageResist: 'armor',
  }
  const family: Family = effect.positive ? (lasting ? 'passive' : 'boon') : 'curse'
  if (resist[av]) return { path: RESIST[resist[av]].path, color: FAMILY[family] }

  // A skill (Fortify One-Handed, Fortify Illusion): that skill's icon. Skill
  // modifiers come as "<Skill>Mod", "<Skill>Modifier" or "<Skill>PowerModifier".
  const skill = Object.keys(SKILL).find((id) => av === id || av.startsWith(id))
  if (skill) return { path: SKILL[skill].path, color: FAMILY[family] }

  // Potions: a flask (Fortify Smithing has its own).
  if (source === 'potion') return glyph(av.startsWith('Smithing') ? EFFECT.smithing : EFFECT.fortHealth, 'boon')
  // Enchanted gear without a skill: the worn-ring look.
  if (source === 'enchantment') return glyph(EFFECT.oneHanded, 'passive')
  // A spell's own school, else the magic star.
  if (effect.school && SKILL[effect.school]) return { path: SKILL[effect.school].path, color: FAMILY[family] }
  return { path: DAMAGE.magic.path, color: FAMILY[family] }
}
