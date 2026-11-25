<script lang="ts">
  // Popup listing what the player owns for one slot. There is no design for it
  // in the Figma file yet; it reuses the tile and panel styles of the screen.
  import type { InventoryItem, SlotId, SlotItem } from '../bridge/types'
  import { itemStatIcon } from '../lib/icons'
  import Hotkey from './Hotkey.svelte'
  import ItemArt from './ItemArt.svelte'

  let {
    slot,
    label,
    icon,
    current,
    items,
    dropping = [],
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
    /** Form ids queued to drop when the screen closes, one per copy. */
    dropping?: string[]
    /** Top-left corner, in canvas pixels. */
    x: number
    y: number
    element?: HTMLElement
    onpick: (item: InventoryItem) => void
    onunequip: () => void
    onclose: () => void
  } = $props()

  // Favourites first, then the best by the slot's main stat (Armor, else
  // Damage), then by name; spells stay after the items.
  const mainStat = $derived(slot === 'leftHand' || slot === 'rightHand' || slot === 'ranged' || slot === 'ammo' ? 'Damage' : 'Armor')
  const sorted = $derived(
    items === null
      ? null
      : [...items].sort((a, b) => {
          const spell = Number(a.type === 'spell') - Number(b.type === 'spell')
          if (spell) return spell
          const favourite = Number(!!b.favorite) - Number(!!a.favorite)
          if (favourite) return favourite
          const stat = statValue(b.stats, mainStat) - statValue(a.stats, mainStat)
          return stat || a.name.localeCompare(b.name)
        }),
  )

  // Armor and damage compared with what the slot holds now: "+4", "−2", or
  // nothing when equal or this is the equipped item. An item without the stat
  // counts as 0 (a hood in place of a helmet loses all its armor).
  const COMPARED = ['Armor', 'Damage']
  const statValue = (stats: InventoryItem['stats'], label: string) =>
    parseFloat(stats.find((stat) => stat.label === label)?.value ?? '0') || 0
  function delta(item: InventoryItem, label: string): { text: string; better: boolean } | null {
    if (item.equipped || !current) return null
    const difference = Math.round(statValue(item.stats, label) - statValue(current.stats, label))
    if (difference === 0) return null
    return { text: difference > 0 ? `+${difference}` : `−${-difference}`, better: difference > 0 }
  }
  // Compared stats the row lacks but the equipped item has, shown as a loss.
  const missing = (item: InventoryItem) =>
    item.equipped || !current
      ? []
      : COMPARED.filter((label) => !item.stats.some((stat) => stat.label === label) && statValue(current.stats, label) > 0)
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
      {#each sorted ?? [] as item (item.formId)}
        {@const leaving = dropping.filter((id) => id === item.formId).length}
        <button class="row" class:equipped={item.equipped} data-form-id={item.type === 'spell' ? undefined : item.formId} onclick={() => onpick(item)}>
          <span class="art"><ItemArt {slot} {item} /></span>
          <span class="text">
            <span class="name">{#if item.favorite}<svg class="favourite" width="16" height="16" viewBox="0 0 24 24" aria-label="Favourite"><path d="M12 2.5L14.9 8.6L21.5 9.4L16.6 13.9L17.9 20.5L12 17.2L6.1 20.5L7.4 13.9L2.5 9.4L9.1 8.6Z" fill="#D8C38E" /></svg>{/if}{item.name}</span>
            <span class="stats">
              {#each item.stats.slice(0, 4) as stat}
                {@const change = COMPARED.includes(stat.label) ? delta(item, stat.label) : null}
                <span class="stat">
                  {#if itemStatIcon(stat.label)}<img alt={stat.label} src={itemStatIcon(stat.label)} />{/if}
                  {stat.value}
                  {#if change}<span class="delta" class:better={change.better} class:worse={!change.better}>{change.text}</span>{/if}
                </span>
              {/each}
              {#each missing(item) as label}
                {@const change = delta(item, label)}
                <span class="stat">
                  {#if itemStatIcon(label)}<img alt={label} src={itemStatIcon(label)} />{/if}
                  0
                  {#if change}<span class="delta worse">{change.text}</span>{/if}
                </span>
              {/each}
            </span>
            {#if item.enchantment}<span class="enchantment">{item.enchantment}</span>{/if}
          </span>
          <span class="side">
            {#if item.equipped}<span class="tag">Equipped</span>{/if}
            {#if leaving}<span class="tag dropping">Dropping{leaving > 1 ? ` ×${leaving}` : ''}</span>{/if}
            {#if item.count > 1}<span class="count">×{item.count}</span>{/if}
          </span>
        </button>
      {/each}
    {/if}
  </div>
  <div class="keys">
    <Hotkey key="R" action="Drop" />
    <Hotkey key="F" action="Favourite" />
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
    background: var(--tile-bg);
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
  .delta {
    margin-left: 4px;
    font-size: 15px;
  }
  .delta.better {
    color: var(--positive);
  }
  .delta.worse {
    color: var(--negative);
  }
  .enchantment {
    display: block;
    margin-top: 2px;
    font-family: var(--font-medium);
    font-weight: 500;
    font-style: italic;
    font-size: 15px;
    line-height: normal;
    color: var(--accent);
  }
  .favourite {
    display: inline-block;
    margin-right: 6px;
    vertical-align: -1px;
  }
  .keys {
    display: flex;
    justify-content: flex-end;
    flex-shrink: 0;
    margin-top: 12px;
  }
  .keys > :global(* + *) {
    margin-left: 10px;
  }
  .tag.dropping {
    color: var(--negative);
  }
</style>
