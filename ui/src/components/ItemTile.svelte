<script lang="ts">
  // Figma "ItemTile" (Inventory / Iter 3): a 200 px square with the item's
  // picture, favourite star and Worn mark top left, count and enchantment
  // markers top right, stat pills along the bottom. The frame colour is the
  // item's rarity; common items keep the plain tile border.
  import favoriteStar from '../assets/design/inventory/favorite.svg'
  import type { InventoryEntry } from '../bridge/types'
  import { categoryGlyph, ELEMENT_COLOUR, ELEMENT_GLYPH, RARITY_COLOUR } from '../lib/inventory'
  import Glyph from './Glyph.svelte'
  import StatPill from './StatPill.svelte'

  let {
    item,
    dropping = 0,
    onactivate,
    onfocus,
  }: { item: InventoryEntry; dropping?: number; onactivate: () => void; onfocus: () => void } = $props()

  const colour = $derived(RARITY_COLOUR[item.rarity])
  const glow = $derived(item.rarity === 'common' ? 'none' : `inset 0 0 14px 0 ${colour}40`)
  const primary = $derived(
    item.stats.damage !== undefined
      ? { kind: 'damage' as const, text: String(item.stats.damage) }
      : item.stats.armor !== undefined
        ? { kind: 'armor' as const, text: String(item.stats.armor) }
        : null,
  )
  const weightText = (w: number) => (Number.isInteger(w) ? String(w) : w.toFixed(1))

  let failed = $state<string | null>(null)
  const thumb = $derived(item.thumb && item.thumb !== failed ? item.thumb : null)

  const rgba = ([r, g, b]: [number, number, number], a: number) => `rgba(${r}, ${g}, ${b}, ${a})`
</script>

<button
  class="tile"
  data-form-id={item.formId}
  style="border-color: {colour}; box-shadow: {glow}"
  onclick={onactivate}
  onfocus={(event) => {
    onfocus()
    // Keep the selected tile in view as the selection moves down the grid.
    event.currentTarget.scrollIntoView?.({ block: 'nearest' })
  }}
  onmouseenter={(event) => event.currentTarget.focus()}
>
  <span class="slot">
    {#if thumb}
      <img class="photo" alt="" src={thumb} onerror={() => (failed = thumb)} />
    {:else}
      <span class="fallback"><Glyph path={categoryGlyph(item.category)} color="#4a6a80" size={88} /></span>
    {/if}
  </span>

  <span class="stats">
    {#if primary}<StatPill kind={primary.kind} text={primary.text} />{/if}
    <StatPill kind="weight" text={weightText(item.stats.weight)} />
    <StatPill kind="value" text={String(item.stats.value)} />
  </span>

  <span class="top-left">
    {#if item.favorite}<img class="star" alt="Favourite" src={favoriteStar} width="24" height="24" />{/if}
    {#if item.worn}
      <span class="worn"><span class="dot"></span>Worn</span>
    {/if}
    {#if dropping}
      <span class="worn dropping"><span class="dot"></span>Dropping{dropping > 1 ? ` ×${dropping}` : ''}</span>
    {/if}
  </span>

  <span class="top-right">
    {#if item.count > 1}<span class="count">×{item.count}</span>{/if}
    {#each item.enchantments.slice(0, 2) as enchantment}
      {@const ring = ELEMENT_COLOUR[enchantment.element]}
      <span class="enchant" title={enchantment.name} style="border-color: {rgba(ring, 0.6)}">
        <span class="inner" style="border-color: {rgba(ring, 0.22)}"></span>
        <span class="enchant-icon"><Glyph path={ELEMENT_GLYPH[enchantment.element]} color={rgba(ring, 1)} size={16} /></span>
      </span>
    {/each}
  </span>
</button>

<style>
  .tile {
    position: relative;
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    width: 200px;
    height: 200px;
    padding: 12px;
    overflow: hidden;
    background: var(--tile-bg);
    border: 1px solid;
    border-radius: 16px;
    font: inherit;
    color: inherit;
    text-align: left;
    cursor: pointer;
  }
  /* Not in the design: the selected tile, as the Character screen marks its tiles. */
  .tile:focus {
    outline: 2px solid var(--accent);
    outline-offset: -2px;
  }
  .slot {
    position: relative;
    display: flex;
    align-items: center;
    justify-content: center;
    flex: 1 0 0;
    width: 100%;
    min-height: 1px;
  }
  .photo {
    display: block;
    width: 153px;
    height: 153px;
    object-fit: contain;
  }
  .fallback {
    display: flex;
    opacity: 0.8;
  }
  .stats {
    display: flex;
    align-items: flex-start;
    justify-content: space-between;
    flex-shrink: 0;
    width: 100%;
  }
  .top-left {
    position: absolute;
    top: 9px;
    left: 9px;
    display: flex;
    align-items: center;
  }
  .top-left > * + * {
    margin-left: 10px;
  }
  .star {
    display: block;
  }
  .worn {
    display: flex;
    align-items: center;
    font-family: var(--font-number);
    font-weight: 500;
    font-variant: small-caps;
    font-size: 12px;
    line-height: 1;
    letter-spacing: 0.48px;
    white-space: nowrap;
    color: var(--accent);
  }
  .dot {
    display: block;
    width: 6px;
    height: 6px;
    margin: 0 7px 0 1px;
    background: var(--accent);
    -webkit-transform: rotate(45deg);
    transform: rotate(45deg);
  }
  .dropping {
    color: var(--negative);
  }
  .dropping .dot {
    background: var(--negative);
  }
  .top-right {
    position: absolute;
    top: 9px;
    right: 9px;
    display: flex;
    flex-direction: column;
    align-items: flex-end;
    width: 60px;
  }
  .top-right > * + * {
    margin-top: 4px;
  }
  .count {
    font-family: var(--font-number);
    font-weight: 400;
    font-size: 16px;
    line-height: 1.25;
    white-space: nowrap;
    color: var(--ink);
  }
  .enchant {
    position: relative;
    display: block;
    width: 26px;
    height: 26px;
    background: rgba(10, 22, 32, 0.78);
    border: 1px solid;
  }
  /* Design offsets are from the outer edge; these are inside the 1 px border. */
  .inner {
    position: absolute;
    top: 2px;
    left: 2px;
    width: 18px;
    height: 18px;
    border: 1px solid;
  }
  .enchant-icon {
    position: absolute;
    top: 3px;
    left: 3px;
  }
</style>
