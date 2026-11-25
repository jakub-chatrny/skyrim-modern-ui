<script lang="ts">
  // Figma node 13:3635 "Inventory / Iter 3" (file Skyrim-Modern-UI,
  // UNyXAYouKVcPGIP56lT4UV), built at the design's own pixel sizes on its
  // 2507 x 1480 frame. The canvas is the Character screen's 2507 x 1511, so
  // both tabs scale the same and the shared header does not move; the extra
  // 31 px go to the item grid.
  import sortIcon from '../assets/design/inventory/sort-desc.svg'
  import type { Bridge, Inventory, InventoryEntry, Snapshot } from '../bridge/types'
  import Glyph from '../components/Glyph.svelte'
  import Hotkey from '../components/Hotkey.svelte'
  import ItemTile from '../components/ItemTile.svelte'
  import StatPill from '../components/StatPill.svelte'
  import TopBar, { type MainTab } from '../components/TopBar.svelte'
  import { CATEGORIES, type CategoryId, categoryGlyph, SORTS, type SortKey, sortItems } from '../lib/inventory'
  import { attachNavigation } from '../lib/navigation'

  let {
    snapshot,
    inventory,
    bridge,
    mock,
    ontab,
  }: { snapshot: Snapshot; inventory: Inventory | null; bridge: Bridge; mock: boolean; ontab: (tab: MainTab) => void } =
    $props()

  const CANVAS_WIDTH = 2507
  const CANVAS_HEIGHT = 1511

  let viewport = $state({ width: window.innerWidth, height: window.innerHeight })
  const scale = $derived(Math.min(viewport.width / CANVAS_WIDTH, viewport.height / CANVAS_HEIGHT))
  const offsetX = $derived((viewport.width - CANVAS_WIDTH * scale) / 2)
  const offsetY = $derived((viewport.height - CANVAS_HEIGHT * scale) / 2)

  // The list comes from the plugin on request: when the tab opens and after
  // every fresh snapshot (equipping, drops, favourites and new item pictures
  // all send one).
  $effect(() => {
    void snapshot
    bridge.send({ type: 'listInventory' })
  })

  let category = $state<CategoryId>('all')
  let sort = $state<SortKey>('value')
  let descending = $state(true)

  const items = $derived(inventory?.items ?? [])
  const counts = $derived(
    Object.fromEntries(
      CATEGORIES.map((c) => [c.id, c.id === 'all' ? items.length : items.filter((i) => i.category === c.id).length]),
    ) as Record<CategoryId, number>,
  )
  const visible = $derived(
    sortItems(category === 'all' ? items : items.filter((i) => i.category === category), sort, descending),
  )
  const categoryLabel = $derived(CATEGORIES.find((c) => c.id === category)!.label)

  // The item the details panel shows: the focused tile, else the first one.
  let focusedId = $state<string | null>(null)
  const selected = $derived(visible.find((i) => i.formId === focusedId) ?? visible[0] ?? null)

  const dropping = $derived(snapshot.dropping ?? [])
  const droppingCount = (formId: string) => dropping.filter((id) => id === formId).length

  const CONSUMABLE = new Set(['potions', 'food', 'ingredients'])
  const GEAR = new Set(['weapons', 'armor'])
  const useLabel = (item: InventoryEntry | null) =>
    !item ? null : item.worn ? 'Unequip' : GEAR.has(item.category) ? 'Equip' : CONSUMABLE.has(item.category) ? 'Use' : null

  function use(item: InventoryEntry) {
    focusedId = item.formId
    if (!useLabel(item)) return
    bridge.send({ type: 'use', formId: item.formId })
  }

  function stepCategory(step: -1 | 1) {
    const index = CATEGORIES.findIndex((c) => c.id === category)
    category = CATEGORIES[(index + step + CATEGORIES.length) % CATEGORIES.length].id
    focusedId = null
    // The first tile of the new list takes the selection.
    setTimeout(() => canvas?.querySelector<HTMLElement>(".grid .tile")?.focus(), 0)
  }

  function onSort(key: SortKey) {
    if (key === sort) descending = !descending
    else sort = key
  }

  function onItem(type: 'drop' | 'favorite'): boolean {
    const formId = (document.activeElement as HTMLElement | null)?.dataset?.formId
    if (!formId) return false
    bridge.send({ type, formId })
    return true
  }

  let canvas = $state<HTMLElement>()
  $effect(() => {
    if (!canvas) return
    return attachNavigation(canvas, {
      initial: ['.grid .tile'],
      onShoulder: () => ontab('Character'),
      hotkeys: {
        q: () => ontab('Character'),
        e: () => ontab('Character'),
        a: () => stepCategory(-1),
        d: () => stepCategory(1),
        r: () => onItem('drop'),
        f: () => onItem('favorite'),
        z: () => turn(-30),
        c: () => turn(30),
      },
    })
  })

  const weightText = (w: number) => (Number.isInteger(w) ? String(w) : w.toFixed(1))

  // The list arrives after the tab opens, so the navigation's first focus
  // found no tile. Select the first one once there is one.
  let placed = false
  $effect(() => {
    if (placed || !canvas || visible.length === 0) return
    placed = true
    if (!(document.activeElement as HTMLElement | null)?.classList.contains('tile')) {
      canvas.querySelector<HTMLElement>('.grid .tile')?.focus()
    }
  })

  // In game the details box shows the item in 3D: the plugin draws the game's
  // own inventory preview into the frame, inside this box, and the page
  // leaves the box transparent (the rest of the screen is covered by four
  // shades around it). The character is hidden meanwhile. Pictures still
  // being made come first; until then the box shows the picture.
  const live = $derived(!mock)
  let previewBox = $state<HTMLElement>()
  const drawing3D = $derived(live && !!selected?.thumb && snapshot.picturesPending === 0)

  $effect(() => {
    if (!live || !previewBox) return
    // Again after every snapshot: the screen may have been closed and reopened.
    void snapshot
    void viewport
    const box = previewBox.getBoundingClientRect()
    bridge.send({
      type: 'preview',
      open: true,
      formId: selected?.formId ?? null,
      rect: {
        left: box.left / window.innerWidth,
        top: box.top / window.innerHeight,
        width: box.width / window.innerWidth,
        height: box.height / window.innerHeight,
      },
    })
  })
  // Leaving the tab: no preview, the character back.
  $effect(() => {
    if (!live) return
    return () => bridge.send({ type: 'preview', open: false })
  })

  // Turning: drag across the box, or Z / C.
  let dragX: number | null = null
  function onDragStart(event: MouseEvent) {
    dragX = event.clientX
  }
  function onDragMove(event: MouseEvent) {
    if (dragX === null) return
    const step = (event.clientX - dragX) / scale
    dragX = event.clientX
    if (step) bridge.send({ type: 'turn', degrees: step * 0.5 })
  }
  function turn(degrees: number) {
    bridge.send({ type: 'turn', degrees })
    return true
  }
</script>

<svelte:window
  onresize={() => (viewport = { width: window.innerWidth, height: window.innerHeight })}
  onmousemove={onDragMove}
  onmouseup={() => (dragX = null)}
/>

<!-- The design has no character on this tab: the page covers the screen,
     except, in game, the 3D preview box (see the shades). -->
<div class="viewport" class:opaque={!live}>
  <div
    class="canvas"
    bind:this={canvas}
    style="transform: translate({offsetX}px, {offsetY}px) scale({scale}); width: {CANVAS_WIDTH}px; height: {CANVAS_HEIGHT}px;"
  >
    <TopBar player={snapshot.player} active="Inventory" {ontab}>
      {#snippet submenu()}
        <Hotkey key="A" />
        <div class="categories">
          {#each CATEGORIES as c}
            <button
              class="category"
              class:active={c.id === category}
              onclick={() => {
                category = c.id
                focusedId = null
              }}
            >
              <Glyph path={c.glyph} color={c.id === category ? '#e4f3fa' : '#8fa9b8'} size={24} />
              <span class="category-label">{c.label}</span>
              <span class="category-count">{counts[c.id]}</span>
            </button>
          {/each}
        </div>
        <Hotkey key="D" />
      {/snippet}
    </TopBar>

    <main>
      <section class="list">
        <div class="toolbar">
          <p class="summary">{categoryLabel} · {visible.length} items</p>
          <div class="sorts">
            <span class="sort-label">Sort by</span>
            {#each SORTS as s}
              <button class="sort" class:active={s.id === sort} onclick={() => onSort(s.id)}>{s.label}</button>
            {/each}
            <button class="sort direction" onclick={() => (descending = !descending)}>
              <span class="sort-icon" class:flipped={!descending}><img alt="" src={sortIcon} /></span>
              {descending ? 'High to Low' : 'Low to High'}
            </button>
          </div>
        </div>

        <div class="grid">
          {#if !inventory}
            <p class="empty">Reading the inventory…</p>
          {:else if visible.length === 0}
            <p class="empty">Nothing here.</p>
          {/if}
          {#each visible as item (item.formId)}
            <ItemTile
              {item}
              dropping={droppingCount(item.formId)}
              onactivate={() => use(item)}
              onfocus={() => (focusedId = item.formId)}
            />
          {/each}
        </div>
      </section>

      <section class="details">
        {#if selected}
          <div class="heading">
            <div class="titles">
              <p class="item-name">{selected.name}</p>
              <p class="item-type">{selected.typeLabel}</p>
            </div>
            <div class="detail-stats">
              {#if selected.stats.armor !== undefined}<StatPill kind="armor" text={String(selected.stats.armor)} variant="detail" />{/if}
              {#if selected.stats.damage !== undefined}<StatPill kind="damage" text={String(selected.stats.damage)} variant="detail" />{/if}
              <StatPill kind="weight" text={weightText(selected.stats.weight)} variant="detail" />
              <StatPill kind="value" text={String(selected.stats.value)} variant="detail" />
            </div>
          </div>
        {/if}
        <!-- svelte-ignore a11y_no_static_element_interactions -->
        <div class="preview" bind:this={previewBox} onmousedown={onDragStart}>
          {#if live}
            <span class="shade shade-top"></span>
            <span class="shade shade-bottom"></span>
            <span class="shade shade-left"></span>
            <span class="shade shade-right"></span>
          {/if}
          {#if selected && !drawing3D}
            {#if selected.thumb}
              <img alt="" src={selected.thumb} />
            {:else}
              <Glyph path={categoryGlyph(selected.category)} color="#2f4e62" size={320} />
            {/if}
          {/if}
        </div>
        {#if selected}
          <div class="description">
            {#if selected.facts?.length}
              <dl class="facts">
                {#each selected.facts as fact}
                  <dt>{fact.label}</dt>
                  <dd>{fact.value}</dd>
                {/each}
              </dl>
            {/if}
            {#if selected.enchantments.length}
              <p class="enchantments">{selected.enchantments.map((e) => e.name).join(' · ')}</p>
            {/if}
            {#if selected.description}<p>{selected.description}</p>{/if}
          </div>
        {/if}
      </section>
    </main>

    <!-- The design's "esc Close": the screen is closed with its own key (Tab by default), Esc stays the game's. -->
    <footer>
      {#if selected && useLabel(selected)}<Hotkey key="enter" action={useLabel(selected) ?? ''} />{/if}
      {#if drawing3D}
        <Hotkey key="Z" />
        <Hotkey key="C" action="Turn" />
      {/if}
      {#if selected}
        <Hotkey key="R" action="Drop" />
        <Hotkey key="F" action="Favourite" />
      {/if}
      <Hotkey key={snapshot.toggleKey ?? 'F4'} action="Close" />
    </footer>
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
  .viewport.opaque {
    background: #070b10;
  }
  /* In game: the screen's background, everywhere but the preview box. Four
     slabs around the box, reaching far past the canvas so they also cover
     the letterbox bands; z-index -1 puts them under all of the canvas's
     content (the canvas's transform makes it the stacking context). */
  .shade {
    position: absolute;
    z-index: -1;
    background: #070b10;
    pointer-events: none;
  }
  .shade-top {
    bottom: 100%;
    left: -10000px;
    right: -10000px;
    height: 10000px;
  }
  .shade-bottom {
    top: 100%;
    left: -10000px;
    right: -10000px;
    height: 10000px;
  }
  .shade-left {
    top: 0;
    bottom: 0;
    right: 100%;
    width: 10000px;
  }
  .shade-right {
    top: 0;
    bottom: 0;
    left: 100%;
    width: 10000px;
  }
  .canvas {
    position: absolute;
    top: 0;
    left: 0;
    transform-origin: 0 0;
    display: flex;
    flex-direction: column;
    padding: 32px 40px;
  }

  /* Top bar submenu: item categories. */
  .categories {
    display: flex;
    align-items: center;
    margin: 0 10px;
    padding: 0 24px;
  }
  .category + .category {
    margin-left: 24px;
  }
  .category {
    display: flex;
    align-items: center;
    height: 34px;
    border-bottom: 1px solid transparent;
  }
  .category > :global(* + *) {
    margin-left: 10px;
  }
  .category-label {
    font-family: var(--font-medium);
    font-weight: 500;
    font-size: 24px;
    line-height: normal;
    white-space: nowrap;
    color: var(--muted);
  }
  .category-count {
    font-family: var(--font-medium);
    font-weight: 500;
    font-size: 20px;
    line-height: normal;
    color: #698ca0;
  }
  /* Not in the design (it shows no selected category): as the selected outfit. */
  .category.active {
    border-bottom-color: var(--text);
  }
  .category.active .category-label {
    color: var(--text);
  }
  .category:focus {
    border-bottom-color: var(--negative);
  }

  main {
    display: flex;
    align-items: flex-start;
    height: 1228px;
    margin-top: 32px;
  }

  .list {
    display: flex;
    flex-direction: column;
    flex-shrink: 0;
    width: 1280px;
    height: 100%;
  }
  .toolbar {
    display: flex;
    align-items: center;
    justify-content: space-between;
    height: 34px;
    flex-shrink: 0;
  }
  .summary {
    font-family: var(--font-medium);
    font-weight: 500;
    font-style: italic;
    font-size: 18px;
    line-height: normal;
    white-space: nowrap;
    color: #a9c3d1;
  }
  .sorts {
    display: flex;
    align-items: center;
  }
  .sorts > * + * {
    margin-left: 9px;
  }
  .sort-label {
    font-family: var(--font-medium);
    font-weight: 500;
    font-size: 18px;
    line-height: normal;
    color: #a9c3d1;
  }
  .sort {
    display: flex;
    align-items: flex-end;
    justify-content: center;
    height: 34px;
    padding: 5px 13px;
    background: #121417;
    border: 1px solid #2f4e62;
    border-radius: 2px;
    font-family: var(--font-display);
    font-weight: 700;
    font-size: 18px;
    line-height: normal;
    white-space: nowrap;
    color: #a9c3d1;
  }
  .sort.active {
    background: #313a3d;
    border-color: var(--accent);
    color: var(--ink);
  }
  .sort:focus {
    border-color: var(--negative);
  }
  .sort-icon {
    position: relative;
    width: 16px;
    height: 16px;
    margin-right: 10px;
    align-self: center;
  }
  .sort-icon img {
    position: absolute;
    left: 1.74px;
    top: 2.07px;
    width: 12.867px;
    height: 11.867px;
  }
  .sort-icon.flipped {
    -webkit-transform: scaleY(-1);
    transform: scaleY(-1);
  }

  /* Six 200 px tiles a row, 16 px apart; scrolls when there are more. */
  .grid {
    display: flex;
    flex-wrap: wrap;
    align-content: flex-start;
    flex: 1 1 0;
    min-height: 0;
    margin-top: 16px;
    overflow-y: auto;
    overflow-x: hidden;
  }
  .grid > :global(.tile) {
    margin: 0 16px 17px 0;
  }
  .grid > :global(.tile:nth-child(6n)) {
    margin-right: 0;
  }
  .empty {
    width: 100%;
    font-family: var(--font-medium);
    font-style: italic;
    font-size: 22px;
    color: var(--muted);
  }
  .grid::-webkit-scrollbar {
    width: 0;
  }

  .details {
    display: flex;
    flex-direction: column;
    flex: 1 1 0;
    min-width: 0;
    height: 100%;
    margin-left: 40px;
  }
  .heading {
    flex-shrink: 0;
  }
  .titles {
    width: 100%;
  }
  .item-name {
    font-family: var(--font-display);
    font-weight: 700;
    font-size: 40px;
    line-height: normal;
    color: var(--text);
  }
  .item-type {
    margin-top: 2px;
    font-family: var(--font-condensed-medium);
    font-weight: 500;
    font-size: 20px;
    line-height: normal;
    color: #b3b3b3;
  }
  .detail-stats {
    display: flex;
    margin-top: 12px;
  }
  .detail-stats > :global(* + *) {
    margin-left: 12px;
  }
  .preview {
    position: relative;
    display: flex;
    align-items: center;
    justify-content: center;
    flex-shrink: 0;
    height: 662px;
    margin-top: 10px;
    padding: 10px;
  }
  .preview img {
    display: block;
    width: 642px;
    height: 642px;
    object-fit: contain;
  }
  .description {
    flex: 1 1 0;
    min-height: 0;
    margin-top: 10px;
    padding: 20px;
    overflow: hidden;
    font-family: var(--font-medium);
    font-weight: 500;
    font-size: 20px;
    line-height: normal;
    color: var(--text);
  }
  .description p {
    white-space: pre-line;
  }
  .description p + p {
    margin-top: 10px;
  }
  .enchantments {
    color: var(--accent);
  }
  /* Not in the design (its description is free text): what the game knows
     about the item, as label / value pairs in two columns. */
  .facts {
    display: grid;
    grid-template-columns: max-content 240px max-content minmax(0, 1fr);
    column-gap: 16px;
    row-gap: 8px;
    margin: 0 0 16px;
    font-size: 20px;
  }
  .facts dt {
    color: var(--muted);
    white-space: nowrap;
  }
  .facts dd {
    margin: 0;
    color: var(--ink);
  }

  footer {
    display: flex;
    align-items: center;
    justify-content: flex-end;
    height: 36px;
    margin-top: 32px;
  }
  footer > :global(* + *) {
    margin-left: 10px;
  }
</style>
