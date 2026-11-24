// Keyboard and gamepad navigation: one focused control at a time, moved by
// direction to the nearest control that way, wherever the layout put it.
//
//   move      arrow keys, W A S D, D-pad
//   activate  Enter, Space, gamepad accept (A / Cross)
//   hotkeys   keys the caller claims (the Character screen: Q / E main tabs,
//             A / D and 1-4 outfits) win over moving; LB / RB are reported to the caller
//
// Keys come from the browser's keydown, or in game from the plugin (window.smuiKey).
//
// Opening and closing the screen is the plugin's hotkey only; nothing here
// closes it, and Esc and the gamepad cancel button are left alone.

type Direction = 'up' | 'down' | 'left' | 'right'

const KEY_DIRECTIONS: Record<string, Direction> = {
  arrowup: 'up',
  arrowdown: 'down',
  arrowleft: 'left',
  arrowright: 'right',
  w: 'up',
  s: 'down',
  a: 'left',
  d: 'right',
}

// W3C Standard Gamepad button indices, as Prisma UI reports them.
const PAD_DIRECTIONS: Record<number, Direction> = { 12: 'up', 13: 'down', 14: 'left', 15: 'right' }
const PAD_ACCEPT = 0

const SIDEWAYS_COST = 5
const PAD_LEFT_BUMPER = 4
const PAD_RIGHT_BUMPER = 5

export interface NavigationOptions {
  /** Called for LB (-1) and RB (+1). */
  onShoulder?: (step: -1 | 1) => void
  /** CSS selectors for the control that gets focus first, tried in order; default is the first control. */
  initial?: string[]
  /** Keys (lower case) the caller handles itself, taking them from movement. Returning false hands the key back. */
  hotkeys?: Record<string, () => void | boolean>
}

declare global {
  interface Window {
    /** Called by the plugin with a key name as `keyName` gives it ('a', 'arrowup', 'enter', '1'). */
    smuiKey?: (key: string) => void
  }
}

// Set by the first real keydown; from then on the plugin's copies are ignored.
let domKeys = false

const KEY_CODES: Record<number, string> = {
  13: 'enter',
  32: ' ',
  37: 'arrowleft',
  38: 'arrowup',
  39: 'arrowright',
  40: 'arrowdown',
}

/**
 * The key as lower-case `KeyboardEvent.key`. Ultralight can hand letter keys
 * over with `key` empty or as an identifier ("U+0041"), so the key code wins
 * for letters, arrows, Enter and Space.
 */
function keyName(event: KeyboardEvent): string {
  const code = event.keyCode
  if (code >= 65 && code <= 90) return String.fromCharCode(code + 32)
  if (code >= 48 && code <= 57) return String.fromCharCode(code)
  return KEY_CODES[code] ?? (event.key ?? '').toLowerCase()
}

function focusables(root: HTMLElement): HTMLElement[] {
  return Array.from(root.querySelectorAll<HTMLElement>('button:not(:disabled)'))
}

function centre(element: HTMLElement) {
  const rect = element.getBoundingClientRect()
  return { x: rect.left + rect.width / 2, y: rect.top + rect.height / 2 }
}

/** Nearest control in the given direction, preferring ones straight ahead. */
function neighbour(root: HTMLElement, from: HTMLElement, direction: Direction): HTMLElement | undefined {
  const origin = centre(from)
  let best: HTMLElement | undefined
  let bestScore = Infinity

  for (const candidate of focusables(root)) {
    if (candidate === from) continue
    const point = centre(candidate)
    const dx = point.x - origin.x
    const dy = point.y - origin.y
    const along = direction === 'left' ? -dx : direction === 'right' ? dx : direction === 'up' ? -dy : dy
    const across = direction === 'left' || direction === 'right' ? Math.abs(dy) : Math.abs(dx)
    // Must be ahead at all; being off to the side costs far more than distance,
    // so a control straight ahead wins even when a diagonal one is closer. No
    // hard cone: the layout has wide gaps (tabs above, columns far apart).
    if (along <= 1) continue
    const score = along + across * SIDEWAYS_COST
    if (score < bestScore) {
      bestScore = score
      best = candidate
    }
  }
  return best
}

/**
 * Wires navigation to `root`. Returns a function that removes it again.
 * Focus starts on the first control and follows the mouse, so there is never
 * a hovered control and a different focused one.
 */
export function attachNavigation(root: HTMLElement, options: NavigationOptions = {}): () => void {
  const current = (): HTMLElement | undefined => {
    const active = document.activeElement
    return active instanceof HTMLElement && root.contains(active) && active !== root ? active : undefined
  }

  const move = (direction: Direction) => {
    const from = current()
    const target = from ? neighbour(root, from, direction) : focusables(root)[0]
    target?.focus()
  }

  const activate = () => current()?.click()

  // One handler for both key sources; true when the key was used.
  const handleKey = (key: string): boolean => {
    const hotkey = options.hotkeys?.[key]
    const direction = KEY_DIRECTIONS[key]
    if (hotkey && hotkey() !== false) {
      // handled
    } else if (direction) {
      move(direction)
    } else if (key === 'enter' || key === ' ') {
      activate()
    } else {
      return false
    }
    return true
  }

  const onKeydown = (event: KeyboardEvent) => {
    domKeys = true
    if (handleKey(keyName(event))) event.preventDefault()
  }

  // In game Prisma UI does not pass the keyboard to the page; the plugin reads
  // the keys itself and calls this. Ignored once real key events have been seen,
  // so a key never counts twice.
  const onPluginKey = (key: string) => {
    if (!domKeys) handleKey(key)
  }

  const onMouseover = (event: MouseEvent) => {
    const target = event.target instanceof Element ? event.target.closest<HTMLElement>('button:not(:disabled)') : null
    if (target && root.contains(target) && target !== document.activeElement) {
      target.focus()
    }
  }

  const onPadDown = (event: Event) => {
    const detail = (event as CustomEvent<PrismaUiGamepadButtonEventDetail>).detail
    const direction = PAD_DIRECTIONS[detail.w3cButtonIndex]
    if (direction) {
      move(direction)
    } else if (detail.action === 'accept' || detail.w3cButtonIndex === PAD_ACCEPT) {
      activate()
    } else if (detail.w3cButtonIndex === PAD_LEFT_BUMPER) {
      options.onShoulder?.(-1)
    } else if (detail.w3cButtonIndex === PAD_RIGHT_BUMPER) {
      options.onShoulder?.(1)
    }
  }

  // window.prismaUi only exists inside the game.
  const pad: PrismaUiControls | undefined = window.prismaUi?.controls

  window.addEventListener('keydown', onKeydown)
  window.smuiKey = onPluginKey
  root.addEventListener('mouseover', onMouseover)
  pad?.addEventListener('gamepadbuttondown', onPadDown)

  if (!current()) {
    const first = (options.initial ?? [])
      .map((selector) => root.querySelector<HTMLElement>(selector))
      .find((element) => element !== null)
    ;(first ?? focusables(root)[0])?.focus()
  }

  return () => {
    window.removeEventListener('keydown', onKeydown)
    if (window.smuiKey === onPluginKey) delete window.smuiKey
    root.removeEventListener('mouseover', onMouseover)
    pad?.removeEventListener('gamepadbuttondown', onPadDown)
  }
}
