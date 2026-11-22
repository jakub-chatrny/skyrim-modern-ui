// Writes the handful of icons the UI uses, from the Iconify data packages, as
// plain SVG files in src/assets/icons/. Run after changing the lists:
//   node scripts/extract-icons.mjs
// game-icons: CC BY 3.0 (game-icons.net). mdi: Apache 2.0 (Pictogrammers).
import { createRequire } from 'node:module'
import { mkdirSync, writeFileSync } from 'node:fs'

const require = createRequire(import.meta.url)
const sets = {
  'game-icons': require('@iconify-json/game-icons/icons.json'),
  mdi: require('@iconify-json/mdi/icons.json'),
}

// file name -> [set, icon, fill]
const WHITE = '#ffffff'
const ITEM = '#8c8c8c' // item placeholders, like the design's empty-art tiles
const icons = {
  // item placeholders, by the bridge's `icon` key
  'item-armor-head': ['game-icons', 'visored-helm', ITEM],
  'item-armor-body': ['game-icons', 'chest-armor', ITEM],
  'item-armor-hands': ['game-icons', 'gauntlet', ITEM],
  'item-armor-feet': ['game-icons', 'leg-armor', ITEM],
  'item-clothing-head': ['game-icons', 'hood', ITEM],
  'item-clothing-body': ['game-icons', 'shirt', ITEM],
  'item-clothing-hands': ['game-icons', 'gloves', ITEM],
  'item-clothing-feet': ['game-icons', 'boots', ITEM],
  'item-shield': ['game-icons', 'round-shield', ITEM],
  'item-jewelry-neck': ['game-icons', 'gem-pendant', ITEM],
  'item-jewelry-ring': ['game-icons', 'ring', ITEM],
  'item-cloak': ['game-icons', 'cloak', ITEM],
  'item-mask': ['game-icons', 'domino-mask', ITEM],
  'item-weapon': ['game-icons', 'broadsword', ITEM],
  'item-weapon-sword': ['game-icons', 'broadsword', ITEM],
  'item-weapon-dagger': ['game-icons', 'plain-dagger', ITEM],
  'item-weapon-axe': ['game-icons', 'wood-axe', ITEM],
  'item-weapon-mace': ['game-icons', 'flanged-mace', ITEM],
  'item-weapon-greatsword': ['game-icons', 'two-handed-sword', ITEM],
  'item-weapon-battleaxe': ['game-icons', 'battle-axe', ITEM],
  'item-weapon-bow': ['game-icons', 'pocket-bow', ITEM],
  'item-weapon-crossbow': ['game-icons', 'crossbow', ITEM],
  'item-weapon-staff': ['game-icons', 'wizard-staff', ITEM],
  'item-ammo-arrow': ['game-icons', 'quiver', ITEM],
  'item-spell': ['game-icons', 'spell-book', ITEM],
  'item-torch': ['game-icons', 'torch', ITEM],
  // stat and row icons the design leaves as placeholders
  'stat-damage': ['mdi', 'sword', WHITE],
  'stat-speed': ['mdi', 'speedometer', WHITE],
  'stat-count': ['mdi', 'pound', WHITE],
  'stat-cost': ['mdi', 'water', WHITE],
  'stat-magicka': ['mdi', 'star-four-points', WHITE],
  'stat-magic': ['mdi', 'auto-fix', WHITE],
  'stat-fire': ['mdi', 'fire', WHITE],
  'stat-frost': ['mdi', 'snowflake', WHITE],
  'stat-shock': ['mdi', 'flash', WHITE],
  'stat-poison': ['mdi', 'bottle-tonic-skull', WHITE],
  'stat-disease': ['mdi', 'virus', WHITE],
  'stat-ranged': ['mdi', 'bow-arrow', WHITE],
  'stat-skill': ['mdi', 'rhombus-medium', WHITE],
}

const out = new URL('../src/assets/icons/', import.meta.url)
mkdirSync(out, { recursive: true })
for (const [file, [set, name, fill]] of Object.entries(icons)) {
  const data = sets[set]
  const icon = data.icons[name]
  if (!icon) throw new Error(`${set}:${name} not found`)
  const w = icon.width ?? data.width ?? 24
  const h = icon.height ?? data.height ?? 24
  const body = icon.body.replaceAll('currentColor', fill)
  writeFileSync(
    new URL(`${file}.svg`, out),
    `<svg xmlns="http://www.w3.org/2000/svg" width="${w}" height="${h}" viewBox="0 0 ${w} ${h}">${body}</svg>\n`,
  )
}
console.log(`wrote ${Object.keys(icons).length} icons`)
