<script module lang="ts">
  export type MainTab = 'Character' | 'Inventory'
</script>

<script lang="ts">
  // Figma "Menu" as in Inventory / Iter 3, shared by every tab: a 3 x 2 grid.
  // Row 1: name, Q / tabs / E, counters (dragon souls, gold, carry weight).
  // Row 2: level + XP bar, A / submenu / D. Each screen passes its own
  // submenu (outfits, item categories). Hotkeys are handled by the screens.
  import type { Snippet } from 'svelte'
  import dragonHead from '../assets/design/inventory/dragon-head.svg'
  import weightIcon from '../assets/design/inventory/stat-weight.svg'
  import type { Snapshot } from '../bridge/types'
  import Hotkey from './Hotkey.svelte'
  import StatPill from './StatPill.svelte'

  let {
    player,
    active,
    ontab,
    submenu,
  }: {
    player: Snapshot['player']
    active: MainTab
    ontab?: (tab: MainTab) => void
    submenu: Snippet
  } = $props()

  const carry = $derived(player.carryWeight)
  const carryFraction = $derived(carry.max > 0 ? Math.max(0, Math.min(1, carry.current / carry.max)) : 0)

  // Order from the Inventory design. Only Character and Inventory exist yet;
  // the other tabs are inert.
  const tabs = ['Map', 'Quests', 'Character', 'Inventory', 'Magic', 'Skills']
  const working = (tab: string): tab is MainTab => tab === 'Character' || tab === 'Inventory'

</script>

<header class="menu">
  <p class="name">{player.name}</p>

  <nav class="tabs">
    <Hotkey key="Q" />
    <div class="items">
      {#each tabs as tab}
        <span
          class="tab"
          class:active={tab === active}
          role="button"
          tabindex="-1"
          onclick={() => working(tab) && ontab?.(tab)}
          onkeydown={() => {}}
        >
          {tab}
        </span>
      {/each}
    </div>
    <Hotkey key="E" />
  </nav>

  <div class="counters">
    <span class="counter">
      <img alt="Dragon souls" src={dragonHead} width="20" height="20" />
      <span class="souls">{player.dragonSouls}</span>
    </span>
    <StatPill kind="value" text={String(player.gold)} variant="counter" />
    <span class="carry">
      <span class="carry-label">
        <span class="carry-icon"><img alt="" src={weightIcon} /></span>
        Carry weight
      </span>
      <span class="carry-bar"><span class="carry-fill" style="width: {carryFraction * 118}px"></span></span>
      <span class="carry-value">{Math.round(carry.current)} / {Math.round(carry.max)}</span>
    </span>
  </div>

  <div class="level">
    <div class="level-text">
      <p class="level-number">{player.level}</p>
      <p class="level-label">Level</p>
    </div>
    <div class="progress">
      <div class="fill" style="width: {Math.max(0, Math.min(1, player.xpProgress)) * 100}%"></div>
    </div>
  </div>

  <div class="submenu">
    {@render submenu()}
  </div>
</header>

<style>
  .menu {
    display: grid;
    grid-template-columns: 1fr auto 1fr;
    grid-template-rows: 1fr 1fr;
    row-gap: 23px;
    width: 100%;
    height: 119px;
    flex-shrink: 0;
  }

  .name {
    grid-column: 1;
    grid-row: 1;
    align-self: start;
    font-family: var(--font-display);
    font-weight: 700;
    font-size: 40px;
    line-height: normal;
    white-space: nowrap;
  }

  .tabs {
    grid-column: 2;
    grid-row: 1;
    align-self: start;
    display: flex;
    align-items: center;
  }
  .tabs > :global(* + *) {
    margin-left: 30px;
  }
  .items {
    display: flex;
    align-items: center;
  }
  .items > * + * {
    margin-left: 50px;
  }
  .tab {
    display: flex;
    align-items: center;
    justify-content: center;
    height: 54px;
    font-family: var(--font-medium);
    font-weight: 500;
    font-size: 36px;
    line-height: normal;
    text-transform: uppercase;
    white-space: nowrap;
    color: var(--muted);
  }
  .tab.active {
    height: auto;
    font-size: 40px;
    color: var(--text);
    border-bottom: 3px solid var(--text);
  }
  .counters {
    grid-column: 3;
    grid-row: 1;
    align-self: start;
    display: flex;
    align-items: center;
    justify-content: flex-end;
  }
  .counters > :global(* + *) {
    margin-left: 16px;
  }
  .counter {
    display: flex;
    align-items: center;
  }
  .counter img {
    display: block;
    margin-right: 4px;
  }
  .souls {
    font-family: var(--font-condensed);
    font-weight: 700;
    font-size: 22px;
    line-height: normal;
    color: #a9c3d1;
  }
  .carry {
    display: flex;
    align-items: center;
  }
  .carry > * + * {
    margin-left: 17px;
  }
  .carry-label {
    display: flex;
    align-items: center;
    font-family: var(--font-medium);
    font-weight: 500;
    font-size: 22px;
    line-height: normal;
    white-space: nowrap;
    color: #a9c3d1;
  }
  .carry-icon {
    position: relative;
    width: 14px;
    height: 14px;
    margin-right: 4px;
  }
  .carry-icon img {
    position: absolute;
    left: 1.82px;
    top: 1.82px;
    width: 10.363px;
    height: 10.363px;
  }
  .carry-bar {
    position: relative;
    display: block;
    width: 120px;
    height: 6px;
    background: rgba(7, 11, 16, 0.6);
    border: 1px solid rgba(143, 169, 184, 0.35);
  }
  .carry-fill {
    position: absolute;
    top: 0;
    left: 0;
    height: 4px;
    background: #7fd6c2;
  }
  .carry-value {
    font-family: var(--font-condensed-medium);
    font-weight: 500;
    font-size: 22px;
    line-height: normal;
    white-space: nowrap;
    color: var(--ink);
  }

  .level {
    grid-column: 1;
    grid-row: 2;
    align-self: start;
    display: flex;
    align-items: center;
  }
  .level > * + * {
    margin-left: 16px;
  }
  .level-text {
    display: flex;
    align-items: center;
    font-size: 24px;
    line-height: normal;
    white-space: nowrap;
  }
  .level-number {
    font-weight: 700;
  }
  .level-number {
    font-family: var(--font-display);
  }
  .level-label {
    font-family: var(--font-medium);
    font-weight: 500;
    margin-left: 7px;
    color: var(--text-80);
  }
  .progress {
    width: 240px;
    height: 24px;
    padding: 3px;
    border: 1.5px solid var(--line-30);
    border-radius: 10px;
    overflow: hidden;
  }
  .fill {
    height: 100%;
    border-radius: 8px;
    background-image: linear-gradient(
      90deg,
      rgba(184, 222, 241, 0.85) 0%,
      rgba(120, 201, 228, 0.85) 24.519%,
      rgba(116, 141, 198, 0.85) 53.365%,
      rgba(83, 94, 148, 0.85) 100%
    );
  }

  /* Across the whole bar, centred: the submenu's width (outfits or item
     categories) must not move the main tabs above it. */
  .submenu {
    position: relative;
    grid-column: 1 / -1;
    grid-row: 2;
    align-self: end;
    justify-self: center;
    display: flex;
    align-items: center;
    justify-content: center;
    padding: 10px;
  }
</style>
