<script lang="ts">
  // Figma node 18:12186 "Menu": a 3 x 2 grid. Row 1: name, tabs, counters.
  // Row 2: level + XP bar, outfit tabs.
  import coinA from '../assets/design/icon-coin-a.svg'
  import coinB from '../assets/design/icon-coin-b.svg'
  import dragonHead from '../assets/design/icon-dragon-head.svg'
  import weight from '../assets/design/icon-weight.svg'
  import type { Snapshot } from '../bridge/types'

  let {
    player,
    outfits,
    onoutfit,
  }: { player: Snapshot['player']; outfits: Snapshot['outfits']; onoutfit: (index: number) => void } = $props()

  // Only Character exists in the first version; the other tabs are inert.
  const tabs = ['Map', 'Quests', 'Character', 'Magic', 'Inventory', 'Skills']

  // 13848 -> "13 848", as in the design.
  const grouped = (value: number) => String(Math.round(value)).replace(/\B(?=(\d{3})+(?!\d))/g, ' ')
</script>

<header class="menu">
  <p class="name">{player.name}</p>

  <nav class="tabs">
    <span class="key">L1</span>
    <div class="items">
      {#each tabs as tab}
        <span class="tab" class:active={tab === 'Character'}>{tab}</span>
      {/each}
    </div>
    <span class="key">R1</span>
  </nav>

  <div class="counters">
    <div class="counter">
      <img alt="Dragon souls" src={dragonHead} width="24" height="24" />
      <p>{player.dragonSouls}</p>
    </div>
    <div class="counter">
      <img alt="Carry weight" src={weight} width="28" height="28" />
      <p>{Math.round(player.carryWeight.current)}/{Math.round(player.carryWeight.max)}</p>
    </div>
    <div class="counter">
      <span class="coin" aria-label="Gold">
        <img alt="" class="coin-a" src={coinA} />
        <img alt="" class="coin-b" src={coinB} />
      </span>
      <p>{grouped(player.gold)}</p>
    </div>
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
    {#each outfits.defined as _, index}
      <button class="outfit" class:active={index === outfits.active} onclick={() => onoutfit(index)}>
        Outfit {index + 1}
      </button>
    {/each}
    {#if outfits.missing > 0}
      <p class="missing">
        {outfits.missing === 1 ? '1 item is' : `${outfits.missing} items are`} no longer in the inventory
      </p>
    {/if}
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
  .tabs > * + * {
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
    font-size: 36px;
    line-height: normal;
    text-transform: uppercase;
    white-space: nowrap;
    color: var(--tab-idle);
  }
  .tab.active {
    width: 221px;
    height: auto;
    font-size: 40px;
    color: var(--text);
    border-bottom: 3px solid var(--text);
  }
  .key {
    padding: 2px 12px;
    font-family: var(--font-condensed);
    font-weight: 500;
    font-size: 14px;
    line-height: normal;
    text-transform: uppercase;
    color: var(--text-80);
    background: rgba(255, 255, 255, 0.1);
    border: 1px solid #000;
    border-radius: 2px 2px 14px 14px;
    box-shadow: 0 0 6px 0 rgba(255, 255, 255, 0.55);
  }

  .counters {
    grid-column: 3;
    grid-row: 1;
    align-self: start;
    display: flex;
    align-items: center;
    justify-content: flex-end;
  }
  .counters > * + * {
    margin-left: 49px;
  }
  .counter {
    display: flex;
    align-items: center;
    font-family: var(--font-condensed);
    font-weight: 700;
    font-size: 24px;
    line-height: normal;
    white-space: nowrap;
  }
  .counter > * + * {
    margin-left: 4px;
  }
  .counter img {
    display: block;
  }
  /* mingcute:coin-fill, two vector parts placed inside a square, as in the design. */
  .coin {
    position: relative;
    width: 29px;
    height: 29px;
    overflow: hidden;
  }
  .coin img {
    position: absolute;
  }
  .coin-a {
    top: 96.43%;
    left: 50.32%;
    width: 3.48%;
    height: 2.79%;
  }
  .coin-b {
    top: 20.83%;
    left: 8.33%;
    width: 83.34%;
    height: 58.34%;
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
  .level-label {
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

  .submenu {
    position: relative;
    grid-column: 2;
    grid-row: 2;
    align-self: center;
    justify-self: center;
    display: flex;
    align-items: center;
    justify-content: center;
  }
  .submenu > .outfit + .outfit {
    margin-left: 47px;
  }
  .outfit {
    display: flex;
    align-items: center;
    justify-content: center;
    height: 34px;
    font-size: 24px;
    line-height: normal;
    text-transform: uppercase;
    white-space: nowrap;
    border-bottom: 1px solid transparent;
  }
  .outfit.active {
    height: auto;
    font-size: 28px;
    border-bottom-color: var(--text);
  }
  /* Not in the design: shown when an outfit wanted something that is gone. */
  .missing {
    position: absolute;
    top: 100%;
    left: 0;
    width: 100%;
    margin: 6px 0 0;
    text-align: center;
    font-family: var(--font-condensed);
    font-weight: 500;
    font-size: 18px;
    line-height: normal;
    white-space: nowrap;
    color: var(--focus);
  }
  .outfit:focus {
    border-bottom-color: var(--focus);
  }
</style>
