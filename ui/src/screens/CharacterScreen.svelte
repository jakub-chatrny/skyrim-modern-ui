<script lang="ts">
  // Figma node 18:12182 "Character / Iter 3", built at the design's own pixel
  // sizes on a 2560 x 1440 canvas and scaled to the game window.
  import fog from '../assets/design/fog.png'
  import type { Bridge, InventoryItem, ItemList, SlotId, Snapshot } from '../bridge/types'
  import EquipmentSlot from '../components/EquipmentSlot.svelte'
  import ItemPicker from '../components/ItemPicker.svelte'
  import StatsPanel from '../components/StatsPanel.svelte'
  import TopBar from '../components/TopBar.svelte'
  import { attachNavigation } from '../lib/navigation'

  let {
    snapshot,
    itemList,
    bridge,
    mock,
  }: { snapshot: Snapshot; itemList: ItemList | null; bridge: Bridge; mock: boolean } = $props()

  const CANVAS_WIDTH = 2560
  const CANVAS_HEIGHT = 1440

  // The design file labels several left-hand tiles "Hands"; these are the intended names.
  const left: [SlotId, string][] = [
    ['head', 'Head'],
    ['body', 'Body'],
    ['hands', 'Hands'],
    ['feet', 'Feet'],
    ['leftHand', 'Left hand'],
    ['rightHand', 'Right hand'],
  ]
  const right: [SlotId, string][] = [
    ['mask', 'Mask'],
    ['cloak', 'Cloak'],
    ['neck', 'Neck'],
    ['ring', 'Ring'],
    ['ranged', 'Ranged'],
    ['ammo', 'Ammo'],
  ]

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
  const labels = Object.fromEntries([...left, ...right]) as Record<SlotId, string>

  let picker = $state<{ slot: SlotId; x: number; y: number; tile: HTMLElement } | null>(null)
  let pickerElement = $state<HTMLElement>()
  // Only the answer for the slot that is open counts; an older one may still arrive.
  const pickerItems = $derived(picker && itemList && itemList.slot === picker.slot ? itemList.items : null)
  const isLeft = (slot: SlotId) => left.some(([id]) => id === slot)

  function openPicker(slot: SlotId, tile: HTMLElement) {
    if (!canvas) return
    console.log(`[ui] slot click: ${slot}`)
    // Tile position in canvas pixels; the popup opens beside the column, towards the character.
    const origin = canvas.getBoundingClientRect()
    const rect = tile.getBoundingClientRect()
    const tileLeft = (rect.left - origin.left) / scale
    const tileTop = (rect.top - origin.top) / scale
    const tileWidth = rect.width / scale
    picker = {
      slot,
      tile,
      x: isLeft(slot) ? tileLeft + tileWidth + PICKER_GAP : tileLeft - PICKER_GAP - PICKER_WIDTH,
      y: Math.max(40, Math.min(tileTop, CANVAS_HEIGHT - PICKER_HEIGHT - 40)),
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
  // the whole screen. LB / RB would switch the main tabs; only Character exists.
  let canvas = $state<HTMLElement>()
  $effect(() => {
    const root = pickerElement ?? canvas
    if (!root) return
    return attachNavigation(root, {
      // The screen opens with the first equipment tile selected, the popup with
      // the equipped item or else its first row.
      initial: pickerElement ? ['.row.equipped', '.row'] : ['.tile'],
      onShoulder: (step) => console.log(`[ui] shoulder button ${step > 0 ? 'RB' : 'LB'}: no other tab yet`),
    })
  })

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
    style="transform: translate({offsetX}px, {offsetY}px) scale({scale}); width: {CANVAS_WIDTH}px; height: {CANVAS_HEIGHT}px;"
  >
    <!-- The design's fog sits behind the character. The page is drawn over the
         game, so the fog is kept out of the column the character stands in. -->
    <div class="fog" aria-hidden="true">
      <div class="fog-layer" style="background-image: url({fog})"></div>
    </div>

    <TopBar player={snapshot.player} outfits={snapshot.outfits} onoutfit={onOutfit} />

    <main>
      <section class="equip">
        {#each left as [slot, label]}
          <EquipmentSlot {slot} {label} item={snapshot.equipment[slot]} onclick={(tile) => openPicker(slot, tile)} />
        {/each}
      </section>

      <!-- Transparent on purpose: the game-rendered character shows through. -->
      <section class="stage">
        {#if mock}<span>character renders here (mock data)</span>{/if}
        {#if snapshot.picturesPending > 0}
          <!-- The game is showing items here, one after another, to picture them. -->
          <span class="pictures">Preparing item pictures ({snapshot.picturesPending})</span>
        {/if}
      </section>

      <section class="equip">
        {#each right as [slot, label]}
          <EquipmentSlot {slot} {label} item={snapshot.equipment[slot]} onclick={(tile) => openPicker(slot, tile)} />
        {/each}
      </section>

      <StatsPanel {snapshot} />
    </main>

    {#if picker}
      <ItemPicker
        slot={picker.slot}
        label={labels[picker.slot]}
        current={snapshot.equipment[picker.slot]}
        items={pickerItems}
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

  .fog {
    position: absolute;
    top: 0;
    left: 0;
    width: 100%;
    height: 100%;
    overflow: hidden;
    pointer-events: none;
    opacity: 0.35;
    /* Clear between the two equipment columns (x 614 to 1558 of 2560), soft edges. */
    -webkit-mask-image: linear-gradient(
      90deg,
      #000 0%,
      #000 19%,
      transparent 27%,
      transparent 58%,
      #000 66%,
      #000 100%
    );
    mask-image: linear-gradient(90deg, #000 0%, #000 19%, transparent 27%, transparent 58%, #000 66%, #000 100%);
  }
  /* Static on purpose. Prisma UI draws the page on the CPU and the game waits
     for it every frame, so anything that animates continuously repaints the
     whole 2560 x 1440 page each frame. A drifting fog took the game down to
     about 20 fps while the screen was open; making it still fixed that. */
  .fog-layer {
    position: absolute;
    top: 0;
    left: 0;
    width: 100%;
    height: 100%;
    background-size: cover;
    background-position: center;
  }

  main {
    position: relative;
    display: flex;
    align-items: flex-start;
    justify-content: center;
    width: 100%;
    margin-top: 45px;
  }
  main > :global(* + *) {
    margin-left: 40px;
  }
  .equip {
    display: flex;
    flex-direction: column;
    align-items: flex-start;
    flex-shrink: 0;
    height: 1186px;
  }
  .equip > :global(* + *) {
    margin-top: 13px;
  }
  .pictures {
    position: absolute;
    bottom: 40px;
    font-family: var(--font-condensed);
    font-weight: 500;
  }
  /* Design: 90px of left padding, then the 943.72 x 1186 character area. */
  .stage {
    position: relative;
    display: flex;
    align-items: center;
    justify-content: center;
    flex-shrink: 0;
    width: 1033.72px;
    height: 1186px;
    padding-left: 90px;
    font-size: 24px;
    color: var(--text-60);
  }
</style>
