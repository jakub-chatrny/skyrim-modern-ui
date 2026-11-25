// Inventory tab: categories, rarity and enchantment colours, and the line
// icons, from the Inventory design (Figma "Inventory / Iter 3") and its UI kit
// (figma/build-ui-kit.js, which made the file's icons and colour variables).
import type { InventoryCategory, InventoryEntry, Rarity } from '../bridge/types'

export type CategoryId = 'all' | InventoryCategory

// Item categories icons, 24-unit grid, as in the UI kit. The design's
// submenu shows the "All" icon on every entry as a placeholder.
export const CATEGORIES: { id: CategoryId; label: string; glyph: string }[] = [
  { id: 'all', label: 'All', glyph: 'M4 4H10V10H4ZM14 4H20V10H14ZM4 14H10V20H4ZM14 14H20V20H14Z' },
  { id: 'weapons', label: 'Weapons', glyph: 'M12 2.5L13.5 5V14H10.5V5ZM8 14H16M12 14V19M10.8 21H13.2' },
  { id: 'armor', label: 'Armor', glyph: 'M8 3.5L12 5.5L16 3.5L20 6V11L17.5 12V20H6.5V12L4 11V6ZM12 5.5V20M6.5 15H17.5' },
  { id: 'potions', label: 'Potions', glyph: 'M10 2.5H14M10.5 2.5V7.6A6.5 6.5 0 1 0 13.5 7.6V2.5M12 11.5V17.5M9 14.5H15' },
  {
    id: 'food',
    label: 'Food',
    glyph: 'M4 12H20C20 16.4 16.4 19.5 12 19.5C7.6 19.5 4 16.4 4 12ZM9 21.5H15M9 9C8 7.8 10 6.7 9 5.5M12 9C11 7.6 13 6.3 12 4.5M15 9C14 7.8 16 6.7 15 5.5',
  },
  {
    id: 'ingredients',
    label: 'Ingredients',
    glyph: 'M12 21V12M12 12C9.5 12 8 10 8.5 7.5C10.5 7.5 12 9 12 12ZM12 12C14.5 12 16 10 15.5 7.5C13.5 7.5 12 9 12 12ZM12 12C11 9.5 11 6 12 3.5C13 6 13 9.5 12 12ZM12 17C10 17 8.5 15.8 8 14M12 18C14 18 15.5 16.8 16 15',
  },
  {
    id: 'books',
    label: 'Books',
    glyph: 'M5 4.5H17.5A1.5 1.5 0 0 1 19 6V20.5H6.5A1.5 1.5 0 0 1 5 19ZM5 17.5H19M12 8L13 10.5L15.5 11.5L13 12.5L12 15L11 12.5L8.5 11.5L11 10.5Z',
  },
  { id: 'misc', label: 'Misc', glyph: 'M3 16L6.5 10H17.5L21 16ZM6.5 10L8 13H16L17.5 10M8 13L3 16M16 13L21 16' },
]

export const categoryGlyph = (id: CategoryId) => CATEGORIES.find((c) => c.id === id)!.glyph

// Tile frame per rarity: the design's common frame is the plain tile border;
// the others are the UI kit's rarity colours, with an inner glow.
export const RARITY_COLOUR: Record<Rarity, string> = {
  common: '#1f3d52',
  rare: '#6fa8ff',
  epic: '#b98cff',
  legendary: '#f2b65a',
}

// Enchant Slot: ring colour by element, and its glyph.
export const ELEMENT_COLOUR: Record<InventoryEntry['enchantments'][number]['element'], [number, number, number]> = {
  frost: [168, 230, 255],
  fire: [242, 135, 90],
  shock: [199, 178, 255],
  holy: [168, 230, 255],
}
export const ELEMENT_GLYPH: Record<InventoryEntry['enchantments'][number]['element'], string> = {
  frost: 'M12 5V19M5.9 8.5L18.1 15.5M18.1 8.5L5.9 15.5',
  fire: 'M12 5C13.2 7.6 16 9 16 12.6A4 4 0 0 1 8 12.6C8 11 8.8 9.9 9.6 9.2C9.9 10.6 10.5 11.3 11.3 11.6C10.9 9.4 11.2 7 12 5Z',
  shock: 'M13.5 4L7.5 13H12L10.5 20L16.5 11H12Z',
  holy: 'M7.5 3L12 8.5L16.5 3M12 8.5A6 6 0 1 1 11.99 8.5ZM9.3 16.6L8.9 12.8L10.8 14.2L12 11.8L13.2 14.2L15.1 12.8L14.7 16.6Z',
}

export type SortKey = 'name' | 'type' | 'weight' | 'value' | 'valuePerWeight'
export const SORTS: { id: SortKey; label: string }[] = [
  { id: 'name', label: 'Name' },
  { id: 'type', label: 'Type' },
  { id: 'weight', label: 'Weight' },
  { id: 'value', label: 'Value' },
  { id: 'valuePerWeight', label: 'Value / Wt' },
]

const sortValue = (item: InventoryEntry, key: SortKey): number | string => {
  switch (key) {
    case 'name':
      return item.name
    case 'type':
      return item.typeLabel
    case 'weight':
      return item.stats.weight
    case 'value':
      return item.stats.value
    case 'valuePerWeight':
      return item.stats.weight > 0 ? item.stats.value / item.stats.weight : Infinity
  }
}

/** Sorted copy. "High to low" puts big numbers and Z first; names break ties A to Z. */
export function sortItems(items: InventoryEntry[], key: SortKey, descending: boolean): InventoryEntry[] {
  return [...items].sort((a, b) => {
    const x = sortValue(a, key)
    const y = sortValue(b, key)
    let order =
      typeof x === 'string' || typeof y === 'string'
        ? String(x).localeCompare(String(y))
        : x === y
          ? 0
          : x < y
            ? -1
            : 1
    if (descending) order = -order
    return order || a.name.localeCompare(b.name)
  })
}
