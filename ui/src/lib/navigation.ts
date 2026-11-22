// Keyboard and gamepad navigation: one focused control at a time, moved by
// direction to the nearest control that way, wherever the layout put it.
//
//   move      arrow keys, W A S D, D-pad
//   activate  Enter, Space (native on a focused button), E, gamepad accept (A / Cross)
//   tabs      Q / E are NOT used; LB / RB are reported to the caller
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

  const onKeydown = (event: KeyboardEvent) => {
    const key = event.key.toLowerCase()
    const direction = KEY_DIRECTIONS[key]
    if (direction) {
      event.preventDefault()
      move(direction)
    } else if (key === 'e') {
      event.preventDefault()
      activate()
    }
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
    root.removeEventListener('mouseover', onMouseover)
    pad?.removeEventListener('gamepadbuttondown', onPadDown)
  }
}
