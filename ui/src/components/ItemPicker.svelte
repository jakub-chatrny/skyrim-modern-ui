<script lang="ts">
  // Popup listing what the player owns for one slot. There is no design for it
  // in the Figma file yet; it reuses the tile and panel styles of the screen.
  import type { InventoryItem, SlotId, SlotItem } from '../bridge/types'
  import { itemStatIcon } from '../lib/icons'
  import ItemArt from './ItemArt.svelte'

  let {
    slot,
    label,
    icon,
    current,
    items,
    x,
    y,
    element = $bindable(),
    onpick,
    onunequip,
    onclose,
  }: {
    slot: SlotId
    label: string
    /** The slot's line icon from the design, shown on the "Nothing" row. */
    icon: string
    /** What the slot holds now. */
    current: SlotItem | null
    /** null while the plugin's answer is on its way. */
    items: InventoryItem[] | null
    /** Top-left corner, in canvas pixels. */
    x: number
    y: number
    element?: HTMLElement
    onpick: (item: InventoryItem) => void
    onunequip: () => void
    onclose: () => void
  } = $props()
</script>

<!-- Anything outside the popup closes it. -->
<div class="backdrop" onclick={onclose} role="presentation"></div>

<div class="picker" bind:this={element} style="left: {x}px; top: {y}px;" role="dialog" aria-label="{label} items">
  <header>
    <p class="title">{label}</p>
    <button class="close" onclick={onclose} title="Close">Close</button>
  </header>

  <div class="list">
    {#if current}
      <button class="row" onclick={onunequip}>
        <span class="art empty"><img alt="" src={icon} /></span>
        <span class="text">
          <span class="name">Nothing</span>
          <span class="stats">Take off {current.name}</span>
        </span>
      </button>
    {/if}

    {#if items === null}
      <p class="note">Looking through the inventory…</p>
    {:else if items.length === 0}
      <p class="note">Nothing in the inventory fits here.</p>
    {:else}
      {#each items as item (item.formId)}
        <button class="row" class:equipped={item.equipped} onclick={() => onpick(item)}>
          <span class="art"><ItemArt {slot} {item} /></span>
          <span class="text">
            <span class="name">{item.name}</span>
            <span class="stats">
              {#each item.stats.slice(0, 4) as stat}
                <span class="stat">
                  {#if itemStatIcon(stat.label)}<img alt={stat.label} src={itemStatIcon(stat.label)} />{/if}
                  {stat.value}
                </span>
              {/each}
            </span>
          </span>
          <span class="side">
            {#if item.equipped}<span class="tag">Equipped</span>{/if}
            {#if item.count > 1}<span class="count">×{item.count}</span>{/if}
          </span>
        </button>
      {/each}
    {/if}
  </div>
</div>

<style>
  .backdrop {
    position: absolute;
    top: 0;
    left: 0;
    width: 100%;
    height: 100%;
    z-index: 10;
  }
  .picker {
    position: absolute;
    z-index: 11;
    display: flex;
    flex-direction: column;
    width: 460px;
    max-height: 640px;
    padding: 16px;
    background: #08121a;
    border: 1px solid var(--key-line);
    border-radius: 16px;
    box-shadow: 0 0 40px 0 rgba(0, 0, 0, 0.8);
  }
  header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    flex-shrink: 0;
    margin-bottom: 12px;
  }
  .title {
    font-family: var(--font-display);
    font-weight: 700;
    font-size: 24px;
    color: var(--ink);
    line-height: normal;
    text-transform: uppercase;
  }
  .close {
    padding: 2px 12px;
    font-family: var(--font-condensed);
    font-weight: 500;
    font-size: 18px;
    line-height: normal;
    color: var(--key-text);
    border: 1px solid var(--key-line);
    border-radius: 4px;
  }
  .close:focus {
    color: var(--text);
    border-color: var(--accent);
  }
  .list {
    display: flex;
    flex-direction: column;
    min-height: 0;
    overflow-y: auto;
  }
  .list > * + * {
    margin-top: 8px;
  }
  .row {
    display: flex;
    align-items: center;
    flex-shrink: 0;
    width: 100%;
    padding: 8px;
    text-align: left;
    background: var(--tile-bg);
    border: 1px solid transparent;
    border-radius: 12px;
  }
  .row:focus {
    padding: 7px;
    border: 2px solid var(--accent);
  }
  .row.equipped {
    background: #10293a;
  }
  /* Clicks land on the row itself. */
  .row > :global(*) {
    pointer-events: none;
  }
  .art {
    flex-shrink: 0;
    width: 56px;
    height: 56px;
    margin-right: 12px;
    border-radius: 8px;
    overflow: hidden;
  }
  .art img {
    display: block;
    width: 100%;
    height: 100%;
  }
  .art.empty {
    display: flex;
    align-items: center;
    justify-content: center;
    background: var(--tile-bg);
  }
  /* The line icon at its own size, as on an empty tile. */
  .art.empty img {
    width: auto;
    height: auto;
    max-width: 40px;
    max-height: 40px;
  }
  .text {
    display: flex;
    flex-direction: column;
    flex: 1;
    min-width: 0;
  }
  .name {
    font-family: var(--font-medium);
    font-weight: 500;
    color: var(--ink);
    font-size: 22px;
    line-height: normal;
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
  }
  .stats {
    display: flex;
    align-items: center;
    font-family: var(--font-condensed);
    font-weight: 500;
    font-size: 18px;
    line-height: normal;
    color: var(--muted);
    white-space: nowrap;
  }
  .stat {
    display: flex;
    align-items: center;
    margin-right: 14px;
  }
  .stat img {
    display: block;
    width: 16px;
    height: 16px;
    margin-right: 4px;
  }
  .side {
    display: flex;
    flex-direction: column;
    align-items: flex-end;
    flex-shrink: 0;
    margin-left: 12px;
    font-family: var(--font-condensed);
    font-weight: 500;
    font-size: 18px;
    line-height: normal;
  }
  .tag {
    color: var(--accent);
    text-transform: uppercase;
  }
  .count {
    color: var(--muted);
  }
  .note {
    padding: 12px 8px;
    font-family: var(--font-condensed);
    font-weight: 500;
    font-size: 20px;
    color: var(--muted);
  }
</style>
