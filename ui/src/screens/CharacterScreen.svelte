<script lang="ts">
  // Figma node 2007:3644 "Character / Iter 4", built at the design's own pixel
  // sizes on its 2507 x 1511 frame and scaled to the game window.
  import arch from '../assets/design/iter4/arch.png'
  import iconAmmo from '../assets/design/iter4/slot-ammo.svg'
  import iconBody from '../assets/design/iter4/slot-body.svg'
  import iconCloak from '../assets/design/iter4/slot-cloak.svg'
  import iconFeet from '../assets/design/iter4/slot-feet.svg'
  import iconHands from '../assets/design/iter4/slot-hands.svg'
  import iconHead from '../assets/design/iter4/slot-head.svg'
  import iconLeftHand from '../assets/design/iter4/slot-left-hand.svg'
  import iconMask from '../assets/design/iter4/slot-mask.svg'
  import iconNeck from '../assets/design/iter4/slot-neck.svg'
  import iconRanged from '../assets/design/iter4/slot-ranged.svg'
  import iconRightHand from '../assets/design/iter4/slot-right-hand.svg'
  import iconRing from '../assets/design/iter4/slot-ring.svg'
  import type { Bridge, InventoryItem, ItemList, SlotId, Snapshot } from '../bridge/types'
  import EquipmentSlot from '../components/EquipmentSlot.svelte'
  import Hotkey from '../components/Hotkey.svelte'
  import ItemPicker from '../components/ItemPicker.svelte'
  import LeftPanel from '../components/LeftPanel.svelte'
  import RightPanel from '../components/RightPanel.svelte'
  import TopBar from '../components/TopBar.svelte'
  import { itemIcon } from '../lib/icons'
  import { attachNavigation } from '../lib/navigation'

  let {
    snapshot,
    itemList,
    bridge,
    mock,
  }: { snapshot: Snapshot; itemList: ItemList | null; bridge: Bridge; mock: boolean } = $props()

  const CANVAS_WIDTH = 2507
  const CANVAS_HEIGHT = 1511

  // Tiles beside the arch (left, right) and under it (bottom), with the
  // design's line icons. The design has no labels; they are tooltips here.
  type Tile = [SlotId, string, string]
  const left: Tile[] = [
    ['head', 'Head', iconHead],
    ['body', 'Body', iconBody],
    ['hands', 'Hands', iconHands],
    ['feet', 'Feet', iconFeet],
  ]
  const right: Tile[] = [
    ['mask', 'Mask', iconMask],
    ['cloak', 'Cloak', iconCloak],
    ['neck', 'Neck', iconNeck],
    ['ring', 'Ring', iconRing],
  ]
  const bottom: Tile[] = [
    ['rightHand', 'Right hand', iconRightHand],
    ['leftHand', 'Left hand', iconLeftHand],
    ['ranged', 'Ranged', iconRanged],
    ['ammo', 'Ammo', iconAmmo],
  ]

  const slotIcons = Object.fromEntries([...left, ...right, ...bottom].map(([slot, , icon]) => [slot, icon])) as Record<
    SlotId,
    string
  >

  // Scale the canvas to fit the window and centre it.
  let viewport = $state({ width: window.innerWidth, height: window.innerHeight })
  const scale = $derived(Math.min(viewport.width / CANVAS_WIDTH, viewport.height / CANVAS_HEIGHT))
  const offsetX = $derived((viewport.width - CANVAS_WIDTH * scale) / 2)
  const offsetY = $derived((viewport.height - CANVAS_HEIGHT * scale) / 2)

  function onResize() {
    viewport = { width: window.innerWidth, height: window.innerHeight }
  }

  // Item picker: one slot at a time. The list comes from the plugin on request.
  const PICKER_WIDTH = 460
  const PICKER_HEIGHT = 640
  const PICKER_GAP = 16
  const labels = Object.fromEntries([...left, ...right, ...bottom].map(([id, label]) => [id, label])) as Record<
    SlotId,
    string
  >

  let picker = $state<{ slot: SlotId; x: number; y: number; tile: HTMLElement } | null>(null)

  // The slot whose tile has focus (keyboard or mouse); its item is described
  // in the footer. Not in the design: there is no other place that names the item.
  let focusedSlot = $state<SlotId | null>(null)
  function onFocusIn(event: FocusEvent) {
    const tile = event.target instanceof Element ? event.target.closest<HTMLElement>('[data-slot]') : null
    focusedSlot = (tile?.dataset.slot as SlotId | undefined) ?? null
  }
  const focusedItem = $derived(focusedSlot ? snapshot.equipment[focusedSlot] : null)
  let pickerElement = $state<HTMLElement>()
  // Only the answer for the slot that is open counts; an older one may still arrive.
  const pickerItems = $derived(picker && itemList && itemList.slot === picker.slot ? itemList.items : null)
  const isIn = (tiles: Tile[], slot: SlotId) => tiles.some(([id]) => id === slot)

  function openPicker(slot: SlotId, tile: HTMLElement) {
    if (!canvas) return
    console.log(`[ui] slot click: ${slot}`)
    // Tile position in canvas pixels; the popup opens beside the column, towards the character.
    const origin = canvas.getBoundingClientRect()
    const rect = tile.getBoundingClientRect()
    const tileLeft = (rect.left - origin.left) / scale
    const tileTop = (rect.top - origin.top) / scale
    const tileWidth = rect.width / scale
    // Beside a side column, towards the character; above the bottom row.
    let x: number
    let y: number
    if (isIn(bottom, slot)) {
      x = tileLeft + tileWidth / 2 - PICKER_WIDTH / 2
      y = tileTop - PICKER_GAP - PICKER_HEIGHT
    } else {
      x = isIn(left, slot) ? tileLeft + tileWidth + PICKER_GAP : tileLeft - PICKER_GAP - PICKER_WIDTH
      y = tileTop
    }
    picker = {
      slot,
      tile,
      x: Math.max(40, Math.min(x, CANVAS_WIDTH - PICKER_WIDTH - 40)),
      y: Math.max(40, Math.min(y, CANVAS_HEIGHT - PICKER_HEIGHT - 40)),
    }
    itemList = null
    bridge.send({ type: 'listItems', slot })
  }

  function closePicker() {
    const tile = picker?.tile
    picker = null
    // Give the selection back to the tile the popup came from.
    tile?.focus()
  }

  function pick(item: InventoryItem) {
    if (!picker) return
    if (!item.equipped) bridge.send({ type: 'equip', formId: item.formId, slot: picker.slot })
    closePicker()
  }

  function unequip() {
    if (!picker) return
    bridge.send({ type: 'unequip', slot: picker.slot })
    closePicker()
  }

  // When new item pictures arrive, the open popup's list is stale: it was
  // answered before they existed. Ask again.
  let lastPending = 0
  $effect(() => {
    const pending = snapshot.picturesPending
    if (pending < lastPending && picker) {
      bridge.send({ type: 'listItems', slot: picker.slot })
    }
    lastPending = pending
  })

  // Keyboard and gamepad focus: inside the popup while it is open, otherwise
  // the whole screen. Q / E and LB / RB would switch the main tabs; only
  // Character exists. A / D step through the outfits, 1 to 4 pick one.
  let canvas = $state<HTMLElement>()
  $effect(() => {
    const root = pickerElement ?? canvas
    if (!root) return
    return attachNavigation(root, {
      // The screen opens with the first equipment tile selected, the popup with
      // the equipped item or else its first row.
      initial: pickerElement ? ['.row.equipped', '.row'] : ['.tile'],
      onShoulder: (step) => console.log(`[ui] shoulder button ${step > 0 ? 'RB' : 'LB'}: no other tab yet`),
      hotkeys: pickerElement
        ? {
            // In the item list: the highlighted row.
            r: () => onRow('drop'),
            f: () => onRow('favorite'),
          }
        : {
            q: () => console.log('[ui] Q: no other tab yet'),
            // On a tile: E takes the item off, R drops it, F toggles its favourite mark.
            e: () => unequipFocused(),
            r: () => onTileItem('drop'),
            f: () => onTileItem('favorite'),
            a: () => stepOutfit(-1),
            d: () => stepOutfit(1),
            // A / D are the outfits, so W / S alone could not reach the bottom
            // row: on a tile they go round the arch instead.
            w: () => stepTile(-1),
            s: () => stepTile(1),

            '1': () => onOutfit(0),
            '2': () => onOutfit(1),
            '3': () => onOutfit(2),
            '4': () => onOutfit(3),
          },
    })
  })

  // Round the arch: down the left column, along the bottom, up the right.
  const RING: SlotId[] = [
    ...left.map(([slot]) => slot),
    ...bottom.map(([slot]) => slot),
    ...right.map(([slot]) => slot).reverse(),
  ]
  function stepTile(step: -1 | 1): boolean {
    const slot = (document.activeElement as HTMLElement | null)?.dataset?.slot as SlotId | undefined
    const index = slot ? RING.indexOf(slot) : -1
    if (index < 0) return false  // not on a tile: plain up / down
    const next = RING[(index + step + RING.length) % RING.length]
    canvas?.querySelector<HTMLElement>(`[data-slot="${next}"]`)?.focus()
    return true
  }

  function unequipFocused(): boolean {
    const slot = (document.activeElement as HTMLElement | null)?.dataset?.slot as SlotId | undefined
    if (!slot || !snapshot.equipment[slot]) return false
    bridge.send({ type: 'unequip', slot })
    return true
  }

  function onTileItem(type: 'drop' | 'favorite'): boolean {
    const slot = (document.activeElement as HTMLElement | null)?.dataset?.slot as SlotId | undefined
    const item = slot ? snapshot.equipment[slot] : null
    if (!item || item.type === 'spell') return false
    bridge.send({ type, formId: item.formId })
    return true
  }

  function onRow(type: 'drop' | 'favorite'): boolean {
    const formId = (document.activeElement as HTMLElement | null)?.dataset?.formId
    if (!formId || !picker) return false
    bridge.send({ type, formId })
    // The list shows counts and stars: ask for it again once the game has changed.
    const slot = picker.slot
    setTimeout(() => bridge.send({ type: 'listItems', slot }), 150)
    return true
  }

  function stepOutfit(step: -1 | 1) {
    const count = snapshot.outfits.defined.length
    onOutfit((snapshot.outfits.active + step + count) % count)
  }

  // The plugin owns the outfits; the highlight follows the snapshot it sends back.
  function onOutfit(index: number) {
    bridge.send({ type: 'switchOutfit', index })
  }
</script>

<svelte:window onresize={onResize} />

<!-- In a browser there is no game behind the page; paint the black the game provides. -->
<div class="viewport" class:mock>
  <div
    class="canvas"
    bind:this={canvas}
    onfocusin={onFocusIn}
    style="transform: translate({offsetX}px, {offsetY}px) scale({scale}); width: {CANVAS_WIDTH}px; height: {CANVAS_HEIGHT}px;"
  >
    <TopBar player={snapshot.player} outfits={snapshot.outfits} onoutfit={onOutfit} />

    <main>
      <!-- The arch frames the character; its middle is clear, so the
           game-rendered character shows through. -->
      <img class="arch" alt="" src={arch} />

      <LeftPanel {snapshot} />

      <section class="centre">
        <div class="upper">
          <div class="equip">
            {#each left as [slot, label, icon]}
              <EquipmentSlot {slot} {label} {icon} item={snapshot.equipment[slot]} onclick={(tile) => openPicker(slot, tile)} />
            {/each}
          </div>

          <div class="stage">
            {#if mock}<span>character renders here (mock data)</span>{/if}
            {#if snapshot.picturesPending > 0}
              <!-- The game is showing items here, one after another, to picture them. -->
              <span class="pictures">Preparing item pictures ({snapshot.picturesPending})</span>
            {/if}
          </div>

          <div class="equip">
            {#each right as [slot, label, icon]}
              <EquipmentSlot {slot} {label} {icon} item={snapshot.equipment[slot]} onclick={(tile) => openPicker(slot, tile)} />
            {/each}
          </div>
        </div>

        <div class="lower">
          {#each bottom as [slot, label, icon]}
            <EquipmentSlot {slot} {label} {icon} item={snapshot.equipment[slot]} onclick={(tile) => openPicker(slot, tile)} />
          {/each}
        </div>
      </section>

      <RightPanel {snapshot} />
    </main>

    <!-- The design's "esc Close": the screen is closed with F4 only, Esc stays the game's. -->
    <footer>
      <div class="details">
        {#if focusedSlot}
          <p class="details-name">
            <img class="details-icon" alt="" src={focusedItem ? itemIcon(focusedSlot, focusedItem) : slotIcons[focusedSlot]} />
            {focusedItem ? focusedItem.name : labels[focusedSlot]}
            {#if focusedItem}
              <span class="details-stats">
                {focusedItem.stats.map((stat) => `${stat.label} ${stat.value}`).join(' · ')}
              </span>
            {:else}
              <span class="details-stats">Empty</span>
            {/if}
          </p>
          {#if focusedItem?.enchantment}<p class="details-enchantment">{focusedItem.enchantment}</p>{/if}
        {/if}
      </div>
      <Hotkey key="enter" action="Equip" />
      {#if focusedItem}
        <Hotkey key="E" action="Unequip" />
        {#if focusedItem.type !== 'spell'}
          <Hotkey key="R" action="Drop" />
          <Hotkey key="F" action="Favourite" />
        {/if}
      {/if}
      <Hotkey key="F4" action="Close" />
    </footer>

    {#if picker}
      <ItemPicker
        slot={picker.slot}
        label={labels[picker.slot]}
        icon={slotIcons[picker.slot]}
        current={snapshot.equipment[picker.slot]}
        items={pickerItems}
        dropping={snapshot.dropping ?? []}
        x={picker.x}
        y={picker.y}
        bind:element={pickerElement}
        onpick={pick}
        onunequip={unequip}
        onclose={closePicker}
      />
    {/if}
  </div>
</div>

<style>
  .viewport {
    position: fixed;
    top: 0;
    left: 0;
    width: 100%;
    height: 100%;
    overflow: hidden;
  }
  .viewport.mock {
    background: #000;
  }
  .canvas {
    position: absolute;
    top: 0;
    left: 0;
    transform-origin: 0 0;
    display: flex;
    flex-direction: column;
    align-items: flex-start;
    padding: 40px;
  }

  main {
    position: relative;
    display: flex;
    align-items: flex-start;
    justify-content: space-between;
    width: 100%;
    height: 1186px;
    margin-top: 45px;
  }
  /* Design: 1095 x 1248 frame at x 666 of the main row, the drawing inset by
     about a pixel; it runs past the bottom of the row, behind the lower tiles. */
  .arch {
    position: absolute;
    top: 1.6px;
    left: 667px;
    width: 1094px;
    height: 1212px;
    pointer-events: none;
  }

  .centre {
    position: relative;
    display: flex;
    flex-direction: column;
    align-items: center;
    flex-shrink: 0;
    width: 996.88px;
    height: 1186px;
  }
  .upper {
    display: flex;
    align-items: stretch;
    height: 1018px;
  }
  .upper > * + * {
    margin-left: 40px;
  }
  .equip {
    display: flex;
    flex-direction: column;
    justify-content: flex-end;
    flex-shrink: 0;
  }
  .equip > :global(* + *) {
    margin-top: 96px;
  }
  .lower {
    display: flex;
    margin-top: 48px;
  }
  .lower > :global(* + *) {
    margin-left: 48px;
  }
  /* Transparent on purpose: the game-rendered character shows through. */
  .stage {
    position: relative;
    display: flex;
    align-items: center;
    justify-content: center;
    flex-shrink: 0;
    width: 676.88px;
    font-size: 24px;
    color: var(--muted);
  }
  .pictures {
    position: absolute;
    bottom: 24px;
    font-family: var(--font-medium);
    font-weight: 500;
  }

  footer {
    display: flex;
    align-items: flex-end;
    justify-content: flex-end;
    width: 100%;
    margin-top: 45px;
  }
  footer > :global(* + *) {
    margin-left: 10px;
  }
  .details {
    flex: 1 1 0;
    min-width: 0;
    margin-right: 24px;
    line-height: normal;
  }
  .details-name {
    overflow: hidden;
    white-space: nowrap;
    text-overflow: ellipsis;
  }
  .details-icon {
    display: inline-block;
    width: 28px;
    height: 28px;
    margin-right: 10px;
    vertical-align: -5px;
  }
  .details-name {
    font-family: var(--font-medium);
    font-weight: 500;
    font-size: 24px;
    color: var(--ink);
  }
  .details-stats {
    margin-left: 16px;
    font-family: var(--font-label);
    font-weight: 400;
    font-size: 20px;
    color: var(--muted);
  }
  .details-enchantment {
    margin-top: 4px;
    font-family: var(--font-medium);
    font-weight: 500;
    font-style: italic;
    font-size: 20px;
    color: var(--accent);
  }
</style>
