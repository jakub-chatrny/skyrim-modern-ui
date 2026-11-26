<script lang="ts">
  // Figma node 26:6553 "Quests / Iter 1" (file Skyrim-Modern-UI), at the
  // design's pixel sizes on the shared 2507 x 1511 canvas, scaled to the
  // window like the other tabs. The header is the shared one (Inventory's
  // counters); this design's own counters differ slightly.
  import targetIcon from '../assets/design/quests/target.svg'
  import trackedChip from '../assets/design/quests/tracked-chip.svg'
  import trackedIcon from '../assets/design/quests/tracked.svg'
  import type { Bridge, Quest, QuestLog, Snapshot } from '../bridge/types'
  import Glyph from '../components/Glyph.svelte'
  import Hotkey from '../components/Hotkey.svelte'
  import TopBar, { type MainTab, nextTab } from '../components/TopBar.svelte'
  import { attachNavigation } from '../lib/navigation'
  import {
    CATEGORIES,
    distance,
    groupOf,
    ICON,
    inCategory,
    progress,
    type QuestCategory,
    shown,
    typeIcon,
  } from '../lib/quests'

  let {
    snapshot,
    log,
    bridge,
    mock,
    ontab,
  }: { snapshot: Snapshot; log: QuestLog | null; bridge: Bridge; mock: boolean; ontab: (tab: MainTab) => void } = $props()

  const CANVAS_WIDTH = 2507
  const CANVAS_HEIGHT = 1511

  let viewport = $state({ width: window.innerWidth, height: window.innerHeight })
  const scale = $derived(Math.min(viewport.width / CANVAS_WIDTH, viewport.height / CANVAS_HEIGHT))
  const offsetX = $derived((viewport.width - CANVAS_WIDTH * scale) / 2)
  const offsetY = $derived((viewport.height - CANVAS_HEIGHT * scale) / 2)

  // The journal comes on request: when the tab opens and after each snapshot.
  $effect(() => {
    void snapshot
    bridge.send({ type: 'listQuests' })
  })

  let category = $state<QuestCategory>('active')
  const quests = $derived(log?.quests ?? [])
  const counts = $derived(
    Object.fromEntries(CATEGORIES.map((c) => [c.id, shown(quests.filter((q) => inCategory(q, c.id))).length])) as Record<
      QuestCategory,
      number
    >,
  )
  const visible = $derived(shown(quests.filter((q) => inCategory(q, category))))
  const trackedCount = $derived(quests.filter((q) => q.tracked && !q.completed).length)
  const activeCount = $derived(quests.filter((q) => !q.completed).length)

  // The selected quest: the focused row, else a tracked one, else the first.
  let selectedId = $state<string | null>(null)
  const selected = $derived(
    visible.find((q) => q.id === selectedId) ?? visible.find((q) => q.tracked) ?? visible[0] ?? null,
  )
  const target = $derived(selected?.objectives.find((o) => o.state === 'active' && o.target)?.target ?? null)

  function stepCategory(step: -1 | 1) {
    const index = CATEGORIES.findIndex((c) => c.id === category)
    category = CATEGORIES[(index + step + CATEGORIES.length) % CATEGORIES.length].id
    selectedId = null
    setTimeout(() => canvas?.querySelector<HTMLElement>('.row')?.focus(), 0)
  }

  function track(): boolean {
    if (!selected || selected.id === 'misc' || selected.completed) return false
    bridge.send({ type: 'trackQuest', questId: selected.id })
    // Tracking goes through the quest's script; ask again once it has run.
    setTimeout(() => bridge.send({ type: 'listQuests' }), 300)
    return true
  }

  function showMap(): boolean {
    bridge.send({ type: 'showMap' })
    return true
  }

  let canvas = $state<HTMLElement>()
  $effect(() => {
    if (!canvas) return
    return attachNavigation(canvas, {
      initial: ['.row.selected', '.row'],
      onShoulder: (step) => ontab(nextTab('Quests', step)),
      hotkeys: {
        q: () => ontab(nextTab('Quests', -1)),
        e: () => ontab(nextTab('Quests', 1)),
        a: () => stepCategory(-1),
        d: () => stepCategory(1),
        t: () => track(),
        m: () => showMap(),
      },
    })
  })

  // The journal arrives after the tab opens; select a row then.
  let placed = false
  $effect(() => {
    if (placed || !canvas || visible.length === 0) return
    placed = true
    if (!(document.activeElement as HTMLElement | null)?.classList.contains('row')) {
      canvas.querySelector<HTMLElement>('.row.selected, .row')?.focus()
    }
  })

  // Group headers: shown before the first row of each type.
  const startsGroup = (list: Quest[], index: number) => index === 0 || list[index - 1].type !== list[index].type
</script>

<svelte:window onresize={() => (viewport = { width: window.innerWidth, height: window.innerHeight })} />

<div class="viewport">
  <div
    class="canvas"
    bind:this={canvas}
    style="transform: translate({offsetX}px, {offsetY}px) scale({scale}); width: {CANVAS_WIDTH}px; height: {CANVAS_HEIGHT}px;"
  >
    <TopBar player={snapshot.player} active="Quests" {ontab}>
      {#snippet submenu()}
        <Hotkey key="A" />
        <div class="categories">
          {#each CATEGORIES as c}
            <button
              class="category"
              class:active={c.id === category}
              onclick={() => {
                category = c.id
                selectedId = null
              }}
            >
              <Glyph path={c.icon} grid={18} size={18} color={c.id === category ? '#e4f3fa' : '#8fa9b8'} />
              <span class="category-label">{c.label}</span>
              <span class="category-count">{counts[c.id]}</span>
            </button>
          {/each}
        </div>
        <Hotkey key="D" />
      {/snippet}
      {#snippet status()}
        <p class="counts">
          <img alt="" src={trackedIcon} width="24" height="24" />
          <span class="tracked-count">{trackedCount} tracked</span>
          <span class="active-count">· {activeCount} active</span>
        </p>
      {/snippet}
    </TopBar>

    <main>
      <nav class="list">
        {#if !log}
          <p class="empty">Reading the journal…</p>
        {:else if visible.length === 0}
          <p class="empty">No quests here.</p>
        {/if}
        {#each visible as quest, index (quest.id)}
          {@const group = groupOf(quest.type)}
          {#if startsGroup(visible, index)}
            <p class="group" class:first={index === 0} style="color: {group.colour}">
              <span class="mark" style="background: {group.colour}"></span>{group.label}
            </p>
          {/if}
          <button
            class="row"
            class:selected={quest.id === selected?.id}
            onfocus={(event) => {
              selectedId = quest.id
              event.currentTarget.scrollIntoView?.({ block: 'nearest' })
            }}
            onclick={() => (selectedId = quest.id)}
          >
            <span class="stripe" style="background: {group.colour}"></span>
            <span class="info">
              <span class="name">{quest.name}</span>
              <span class="progress">{progress(quest)}</span>
            </span>
            {#if quest.tracked && !quest.completed}<img class="tracked" alt="Tracked" src={trackedIcon} width="16" height="16" />{/if}
          </button>
        {/each}
      </nav>

      {#if selected}
        {@const group = groupOf(selected.type)}
        <section class="detail">
          <header>
            <div class="title-block">
              <p class="type" style="color: {group.colour}">
                <Glyph path={typeIcon(selected.type)} grid={18} size={16} color={group.colour} />
                {group.label}
              </p>
              <p class="title">{selected.name}</p>
              <div class="chips">
                {#if selected.tracked && !selected.completed}
                  <span class="chip chip-tracked"><img alt="" src={trackedChip} width="14" height="14" />Tracked</span>
                {/if}
                <span class="chip">{selected.completed ? 'Completed' : 'In progress'}</span>
              </div>
            </div>
            {#if selected.id !== 'misc' && !selected.completed}
              <div class="actions">
                <button class="key-button" onclick={track}>
                  <span class="key">T</span>{selected.tracked ? 'Untrack' : 'Track'}
                </button>
                <button class="key-button primary" onclick={showMap}>
                  <span class="key">M</span>Show on map
                </button>
              </div>
            {/if}
          </header>

          <div class="columns">
            <div class="journal">
              <p class="label">Journal</p>
              {#if selected.journal.length}
                <p class="current">{selected.journal[0]}</p>
                {#if selected.journal.length > 1}
                  <div class="divider"></div>
                  <p class="label">Earlier entries</p>
                  {#each selected.journal.slice(1) as entry}
                    <p class="entry">{entry}</p>
                  {/each}
                {/if}
              {:else}
                <p class="entry none">No journal entries.</p>
              {/if}
            </div>

            <div class="objectives">
              <p class="label">Objectives</p>
              {#each selected.objectives as objective}
                <div class="objective {objective.state}">
                  {#if objective.state === 'active'}
                    <Glyph path={ICON.active} grid={18} size={20} color="#a8e6ff" />
                  {:else if objective.state === 'completed'}
                    <Glyph path={ICON.completed} grid={18} size={20} color="#8fd3b0" />
                  {:else}
                    <Glyph path={ICON.failed} grid={18} size={20} color="#e2636e" />
                  {/if}
                  <p class="objective-text">{objective.text}</p>
                  {#if objective.target?.metres !== undefined}
                    <p class="objective-distance">{distance(objective.target.metres)}</p>
                  {/if}
                </div>
              {/each}

              {#if target}
                <p class="label target-label">Target</p>
                <div class="target">
                  <span class="target-icon"><img alt="" src={targetIcon} /></span>
                  <div class="place">
                    <p class="place-name">{target.place ?? target.name}</p>
                    {#if target.region}<p class="region">{target.region}</p>{/if}
                  </div>
                  {#if target.metres !== undefined}
                    <div class="how-far">
                      <p class="how-far-distance">{distance(target.metres)}</p>
                      {#if target.direction}<p class="direction">{target.direction}</p>{/if}
                    </div>
                  {/if}
                </div>
                {#if target.place}<p class="target-who">{target.name}  ·  quest target</p>{/if}
              {/if}
            </div>
          </div>
        </section>
      {/if}
    </main>

    <!-- The design's footer: T Track, M Map, Tab Close (the screen's own key). -->
    <footer>
      {#if selected && selected.id !== 'misc' && !selected.completed}
        <Hotkey key="T" action={selected.tracked ? 'Untrack' : 'Track'} />
        <Hotkey key="M" action="Map" />
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
    background: #070b10;
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
  p {
    margin: 0;
  }

  /* Top bar: categories and the tracked / active count. */
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
  /* Not in the design (no category is shown selected): as the selected outfit. */
  .category.active {
    border-bottom-color: var(--text);
  }
  .category.active .category-label {
    color: var(--text);
  }
  .category:focus {
    border-bottom-color: var(--negative);
  }
  .counts {
    display: flex;
    align-items: center;
    font-family: var(--font-medium);
    font-weight: 500;
    font-size: 24px;
    line-height: 1.25;
    white-space: nowrap;
  }
  .counts img {
    display: block;
    margin-right: 8px;
  }
  .tracked-count {
    color: var(--accent);
  }
  .active-count {
    margin-left: 8px;
    color: #a9c3d1;
  }

  main {
    display: flex;
    align-items: flex-start;
    flex: 1 1 0;
    min-height: 0;
    margin-top: 32px;
  }

  /* Quest list */
  .list {
    display: flex;
    flex-direction: column;
    flex-shrink: 0;
    width: 504px;
    max-height: 100%;
    padding: 18px 16px;
    overflow-y: auto;
  }
  .list::-webkit-scrollbar {
    width: 0;
  }
  .list > * + * {
    margin-top: 8px;
  }
  .empty {
    font-family: var(--font-medium);
    font-style: italic;
    font-size: 22px;
    color: var(--muted);
  }
  .group {
    display: flex;
    align-items: center;
    padding: 10px 2px 2px;
    font-family: var(--font-display);
    font-weight: 700;
    font-size: 21px;
    line-height: 1.15;
    letter-spacing: 1.68px;
    text-transform: uppercase;
    white-space: nowrap;
  }
  .group.first {
    padding-top: 0;
  }
  .mark {
    display: block;
    flex-shrink: 0;
    width: 6px;
    height: 6px;
    margin-right: 8px;
  }
  .row {
    position: relative;
    display: flex;
    align-items: center;
    flex-shrink: 0;
    width: 472px;
    padding-right: 16px;
    background: rgba(10, 22, 32, 0.4);
    border: 1px solid #1e3444;
    border-left: 0;
    border-radius: 8px;
    text-align: left;
  }
  .row.selected,
  .row:focus {
    background: rgba(28, 48, 66, 0.9);
    border-color: var(--accent);
    box-shadow:
      0 0 11px rgba(168, 230, 255, 0.25),
      0 0 0 1px rgba(168, 230, 255, 0.4);
  }
  .stripe {
    align-self: stretch;
    flex-shrink: 0;
    width: 3px;
    border-radius: 8px 0 0 8px;
  }
  .info {
    display: flex;
    flex-direction: column;
    flex: 1 1 0;
    min-width: 0;
    margin-left: 14px;
    padding: 5px 0;
  }
  .name {
    font-family: var(--font-medium);
    font-weight: 500;
    font-size: 24px;
    line-height: 1.25;
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
    color: var(--ink);
  }
  .progress {
    margin-top: 4px;
    font-family: var(--font-condensed-medium);
    font-weight: 500;
    font-size: 21px;
    line-height: 1;
    color: var(--muted);
  }
  .tracked {
    display: block;
    flex-shrink: 0;
    margin-left: 14px;
  }

  /* Quest detail */
  .detail {
    display: flex;
    flex-direction: column;
    flex: 1 1 0;
    min-width: 0;
    max-height: 100%;
    margin-left: 48px;
    overflow: hidden;
  }
  header {
    display: flex;
    align-items: flex-end;
    padding: 28px 32px 24px;
    border-bottom: 1px solid #1e3444;
  }
  .title-block {
    display: flex;
    flex-direction: column;
    flex: 1 1 0;
    min-width: 0;
  }
  .title-block > * + * {
    margin-top: 10px;
  }
  .type {
    display: flex;
    align-items: center;
    font-family: var(--font-condensed-medium);
    font-weight: 500;
    font-size: 21px;
    line-height: normal;
    letter-spacing: 0.84px;
    text-transform: uppercase;
  }
  .type > :global(svg) {
    margin-right: 8px;
  }
  .title {
    font-family: var(--font-display);
    font-weight: 700;
    font-size: 49px;
    line-height: 1.15;
    letter-spacing: 1.96px;
    color: var(--ink);
  }
  .chips {
    display: flex;
  }
  .chip + .chip {
    margin-left: 8px;
  }
  .chip {
    display: flex;
    align-items: center;
    padding: 4px 10px 4px 8px;
    background: rgba(7, 11, 16, 0.5);
    border: 1px solid rgba(201, 220, 230, 0.5);
    font-family: var(--font-condensed-medium);
    font-weight: 500;
    font-size: 18px;
    line-height: 1;
    letter-spacing: 0.72px;
    white-space: nowrap;
    color: #c9dce6;
  }
  .chip-tracked {
    border-color: rgba(168, 230, 255, 0.5);
    color: var(--accent);
  }
  .chip-tracked img {
    display: block;
    margin-right: 6px;
  }
  .actions {
    display: flex;
    flex-shrink: 0;
    margin-left: 24px;
  }
  .key-button + .key-button {
    margin-left: 8px;
  }
  .key-button {
    display: flex;
    align-items: center;
    height: 36px;
    padding: 0 16px;
    background: rgba(7, 11, 16, 0.5);
    border: 1px solid #2f4e62;
    border-radius: 2px;
    font-family: var(--font-display);
    font-weight: 700;
    font-size: 19px;
    line-height: normal;
    letter-spacing: 1.9px;
    text-transform: uppercase;
    white-space: nowrap;
    color: var(--ink);
  }
  .key {
    display: flex;
    align-items: center;
    justify-content: center;
    height: 20px;
    margin-right: 8px;
    padding: 0 6px;
    border: 1px solid #4a6a80;
    font-family: var(--font-condensed-medium);
    font-weight: 500;
    font-size: 18px;
    letter-spacing: 0;
    color: #a9c3d1;
  }
  .key-button.primary {
    background: rgba(168, 230, 255, 0.14);
    border-color: var(--accent);
  }
  .key-button.primary .key {
    border-color: var(--accent);
    color: var(--accent);
  }
  .key-button:focus {
    border-color: var(--negative);
  }

  .columns {
    display: flex;
    align-items: flex-start;
    min-height: 0;
    padding: 28px 32px;
    overflow-y: auto;
  }
  .columns::-webkit-scrollbar {
    width: 0;
  }
  .label {
    font-family: var(--font-condensed-medium);
    font-weight: 500;
    font-size: 21px;
    line-height: normal;
    letter-spacing: 0.84px;
    text-transform: uppercase;
    white-space: nowrap;
    color: #a9c3d1;
  }
  .journal {
    display: flex;
    flex-direction: column;
    flex: 1 1 0;
    min-width: 0;
  }
  .journal > * + * {
    margin-top: 16px;
  }
  .current {
    width: 100%;
    font-family: var(--font-medium);
    font-weight: 500;
    font-size: 24px;
    line-height: 1.25;
    color: var(--ink);
  }
  .divider {
    width: 632px;
    height: 1px;
    background: #1e3444;
  }
  .entry {
    width: 100%;
    padding-left: 12px;
    border-left: 2px solid #2f4e62;
    font-family: var(--font-medium);
    font-weight: 500;
    font-size: 24px;
    line-height: 1.25;
    color: var(--muted);
  }
  .entry.none {
    border-left: 0;
    padding-left: 0;
    font-style: italic;
  }

  .objectives {
    display: flex;
    flex-direction: column;
    flex-shrink: 0;
    width: 780px;
    margin-left: 56px;
  }
  .objectives > * + * {
    margin-top: 10px;
  }
  .objective {
    display: flex;
    align-items: flex-start;
    padding: 12px 14px 12px 12px;
    background: rgba(7, 11, 16, 0.3);
    border: 1px solid #1e3444;
    border-radius: 4px;
  }
  .objective.active {
    background: rgba(7, 11, 16, 0.5);
    border-color: rgba(168, 230, 255, 0.35);
  }
  .objective > :global(svg) {
    margin-top: 3px;
  }
  .objective-text {
    flex: 1 1 0;
    min-width: 0;
    margin-left: 12px;
    font-family: var(--font-medium);
    font-weight: 500;
    font-size: 24px;
    line-height: 1.25;
    color: #a9c3d1;
  }
  .objective.active .objective-text {
    color: var(--ink);
  }
  .objective.failed .objective-text {
    text-decoration: line-through;
  }
  .objective-distance {
    margin-left: 12px;
    font-family: var(--font-condensed-medium);
    font-weight: 500;
    font-size: 19px;
    line-height: 1;
    white-space: nowrap;
    color: #a9c3d1;
  }
  .target-label {
    margin-top: 24px !important;
  }
  .target {
    display: flex;
    align-items: center;
    padding: 16px;
    background: rgba(7, 11, 16, 0.5);
    border: 1px solid #1e3444;
  }
  .target-icon {
    position: relative;
    flex-shrink: 0;
    width: 30px;
    height: 30px;
  }
  .target-icon img {
    position: absolute;
    top: -26.04%;
    right: -15.63%;
    bottom: -23.96%;
    left: -15.63%;
    width: 131.26%;
    height: 150%;
  }
  .place {
    flex: 1 1 0;
    min-width: 0;
    margin-left: 16px;
  }
  .place-name {
    font-family: var(--font-display);
    font-weight: 700;
    font-size: 26px;
    line-height: 1.15;
    letter-spacing: 1.04px;
    color: var(--ink);
  }
  .region {
    margin-top: 5px;
    font-family: var(--font-condensed-medium);
    font-weight: 500;
    font-size: 21px;
    line-height: 1;
    white-space: pre;
    color: #a9c3d1;
  }
  .how-far {
    display: flex;
    flex-direction: column;
    align-items: flex-end;
    margin-left: 16px;
    line-height: 1;
    white-space: nowrap;
  }
  .how-far-distance {
    font-family: var(--font-display);
    font-weight: 700;
    font-size: 29px;
    letter-spacing: 0.58px;
    color: var(--ink);
  }
  .direction {
    margin-top: 4px;
    font-family: var(--font-condensed-medium);
    font-weight: 500;
    font-size: 18px;
    letter-spacing: 0.72px;
    color: #a9c3d1;
  }
  .target-who {
    font-family: var(--font-condensed-medium);
    font-weight: 500;
    font-size: 21px;
    line-height: 1;
    white-space: pre;
    color: var(--muted);
  }

  footer {
    display: flex;
    align-items: flex-end;
    justify-content: flex-end;
    height: 36px;
    margin-top: 32px;
  }
  footer > :global(* + *) {
    margin-left: 10px;
  }
</style>
