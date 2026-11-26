// Quests tab: groups, colours, categories and icons from the "Quests / Iter 1"
// design (Figma file Skyrim-Modern-UI, node 26:6553).
import type { Quest, QuestType } from '../bridge/types'

// Group headers and type stripes, in list order. The design shows the first
// seven; Dark Brotherhood, Civil War and the two DLCs take free palette colours.
export const GROUPS: { type: QuestType; label: string; colour: string }[] = [
  { type: 'main', label: 'Main Quest', colour: '#f2b65a' },
  { type: 'companions', label: 'Companions', colour: '#efa07e' },
  { type: 'college', label: 'College of Winterhold', colour: '#93b6ff' },
  { type: 'thieves', label: 'Thieves Guild', colour: '#8fd3b0' },
  { type: 'darkBrotherhood', label: 'Dark Brotherhood', colour: '#e2636e' },
  { type: 'civilWar', label: 'Civil War', colour: '#d8c38e' },
  { type: 'dawnguard', label: 'Dawnguard', colour: '#e89bc8' },
  { type: 'dragonborn', label: 'Dragonborn', colour: '#9ed37a' },
  { type: 'daedric', label: 'Daedric', colour: '#b98cff' },
  { type: 'side', label: 'Side Quest', colour: '#c9dce6' },
  { type: 'misc', label: 'Miscellaneous', colour: '#8fa9b8' },
]
export const groupOf = (type: QuestType) => GROUPS.find((g) => g.type === type)!

const FACTIONS: QuestType[] = ['companions', 'college', 'thieves', 'darkBrotherhood', 'civilWar', 'dawnguard', 'dragonborn']

// Icons on an 18-unit grid, as exported from the design.
export const ICON = {
  active: 'M2.25 9H4.5M13.5 9H15.75M9 2.25L11.25 9L9 15.75L6.75 9L9 2.25Z',
  main: 'M5.25 3C6.75 6.75 6.75 11.25 4.5 15M9 2.25C10.5 6.75 10.5 11.25 9 15.75M12.75 3C14.25 6.75 14.25 11.25 13.5 15',
  factions: 'M4.5 2.25V15.75M4.5 3H13.5L11.25 6L13.5 9H4.5',
  daedric: 'M9 2.25L10.125 7.875L15.75 9L10.125 10.125L9 15.75L7.875 10.125L2.25 9L7.875 7.875L9 2.25Z',
  side:
    'M5.25 3H12.75V13.5C12.75 13.8978 12.908 14.2794 13.1893 14.5607C13.4706 14.842 13.8522 15 14.25 15H6.75C6.35218 15 5.97064 14.842 5.68934 14.5607C5.40804 14.2794 5.25 13.8978 5.25 13.5V6M5.25 3V6M5.25 3C4.85218 3 4.47064 3.15804 4.18934 3.43934C3.90804 3.72064 3.75 4.10218 3.75 4.5V6H5.25M7.5 6H10.5M7.5 9H10.5',
  misc: 'M6.75 4.5H15M6.75 9H15M6.75 13.5H15M3.375 4.5H3.3825M3.375 9H3.3825M3.375 13.5H3.3825',
  completed:
    'M6 9.375L8.25 11.625L12.375 6.75M2.625 9C2.625 10.6908 3.29665 12.3123 4.49219 13.5078C5.68774 14.7034 7.30924 15.375 9 15.375C10.6908 15.375 12.3123 14.7034 13.5078 13.5078C14.7034 12.3123 15.375 10.6908 15.375 9C15.375 7.30924 14.7034 5.68774 13.5078 4.49219C12.3123 3.29665 10.6908 2.625 9 2.625C7.30924 2.625 5.68774 3.29665 4.49219 4.49219C3.29665 5.68774 2.625 7.30924 2.625 9Z',
  failed:
    'M6.75 6.75L11.25 11.25M11.25 6.75L6.75 11.25M2.625 9C2.625 10.6908 3.29665 12.3123 4.49219 13.5078C5.68774 14.7034 7.30924 15.375 9 15.375C10.6908 15.375 12.3123 14.7034 13.5078 13.5078C14.7034 12.3123 15.375 10.6908 15.375 9C15.375 7.30924 14.7034 5.68774 13.5078 4.49219C12.3123 3.29665 10.6908 2.625 9 2.625C7.30924 2.625 5.68774 3.29665 4.49219 4.49219C3.29665 5.68774 2.625 7.30924 2.625 9Z',
}

/** The icon a quest type gets beside its name in the details. */
export const typeIcon = (type: QuestType) =>
  type === 'main' ? ICON.main : type === 'daedric' ? ICON.daedric : type === 'side' ? ICON.side : type === 'misc' ? ICON.misc : ICON.factions

export type QuestCategory = 'active' | 'main' | 'factions' | 'daedric' | 'side' | 'misc' | 'completed'
export const CATEGORIES: { id: QuestCategory; label: string; icon: string }[] = [
  { id: 'active', label: 'Active', icon: ICON.active },
  { id: 'main', label: 'Main', icon: ICON.main },
  { id: 'factions', label: 'Factions', icon: ICON.factions },
  { id: 'daedric', label: 'Daedric', icon: ICON.daedric },
  { id: 'side', label: 'Side', icon: ICON.side },
  { id: 'misc', label: 'Misc', icon: ICON.misc },
  { id: 'completed', label: 'Completed', icon: ICON.completed },
]

export function inCategory(quest: Quest, category: QuestCategory): boolean {
  if (category === 'completed') return quest.completed
  if (quest.completed) return false
  switch (category) {
    case 'active':
      return true
    case 'factions':
      return FACTIONS.includes(quest.type)
    default:
      return quest.type === category
  }
}

/**
 * The list as the journal shows it: grouped by type, and all miscellaneous
 * objectives under one "Miscellaneous" entry, as the game's own journal does.
 */
export function shown(quests: Quest[]): Quest[] {
  const misc = quests.filter((q) => q.type === 'misc' && !q.completed)
  const rest = quests.filter((q) => q.type !== 'misc' || q.completed)
  const merged: Quest[] = misc.length
    ? [
        {
          id: 'misc',
          name: 'Miscellaneous',
          type: 'misc',
          tracked: misc.some((q) => q.tracked),
          completed: false,
          objectives: misc.flatMap((q) => q.objectives),
          journal: misc.flatMap((q) => q.journal),
        },
      ]
    : []
  const order = (q: Quest) => GROUPS.findIndex((g) => g.type === q.type)
  return [...rest, ...merged].sort((a, b) => order(a) - order(b) || a.name.localeCompare(b.name))
}

/** "1 objective", "2 objectives", "1 of 2 done", "Completed". */
export function progress(quest: Quest): string {
  if (quest.completed) return 'Completed'
  const done = quest.objectives.filter((o) => o.state === 'completed').length
  const total = quest.objectives.length
  if (done > 0) return `${done} of ${total} done`
  return `${total} objective${total === 1 ? '' : 's'}`
}

/** 2400 -> "2.4 km", 850 -> "850 m". */
export const distance = (metres: number) =>
  metres >= 1000 ? `${(metres / 1000).toFixed(metres >= 10000 ? 0 : 1)} km` : `${Math.round(metres)} m`
