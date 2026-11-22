<script lang="ts">
  // Figma "ItemTile" with its slot label. The art is a placeholder icon per item
  // type until there are real item thumbnails.
  import type { SlotId, SlotItem } from '../bridge/types'
  import { itemStatIcon } from '../lib/icons'
  import ItemArt from './ItemArt.svelte'

  let {
    slot,
    label,
    item,
    onclick,
  }: { slot: SlotId; label: string; item: SlotItem | null; onclick: (tile: HTMLElement) => void } = $props()

  // The tile has room for four stat rows.
  const stats = $derived(item ? item.stats.slice(0, 4) : [])
</script>

<div class="slot">
  <p class="label">{label}</p>
  <button
    class="tile"
    class:empty={!item}
    onclick={(event) => onclick(event.currentTarget)}
    title={item ? item.name : `Choose ${label.toLowerCase()}`}
  >
    <span class="art" class:photo={!!item?.thumb}><ItemArt {slot} {item} /></span>
    {#if stats.length}
      <span class="stats">
        {#each stats as stat}
          <span class="stat">
            {#if itemStatIcon(stat.label)}
              <img alt={stat.label} src={itemStatIcon(stat.label)} />
            {/if}
            <span>{stat.value}</span>
          </span>
        {/each}
      </span>
    {/if}
  </button>
</div>

<style>
  .slot {
    display: flex;
    flex-direction: column;
    align-items: flex-start;
    flex-shrink: 0;
  }
  .label {
    margin-bottom: 5px;
    font-size: 20px;
    line-height: normal;
    white-space: nowrap;
    color: var(--text-80);
  }
  .tile {
    position: relative;
    display: flex;
    align-items: center;
    justify-content: center;
    width: 157px;
    height: 157px;
    padding: 12px;
    overflow: hidden;
    background: var(--tile-bg);
    border: 1px solid var(--line-20);
    border-radius: 16px;
  }
  /* The design's selected tile: 2px amber border. The padding gives the pixel back.
     Focus follows the mouse (lib/navigation.ts), so there is no separate hover style. */
  .tile:focus {
    padding: 11px;
    border: 2px solid var(--focus);
  }
  /* Clicks should land on the button itself, not on what is inside it. */
  .tile > :global(*) {
    pointer-events: none;
  }
  .art {
    display: block;
    width: 100%;
    height: 100%;
  }
  /* A picture uses the whole tile; an icon keeps the design's 12px inset. */
  .art.photo {
    position: absolute;
    top: 0;
    left: 0;
  }
  .empty .art {
    opacity: 0.25;
  }
  .stats {
    position: absolute;
    left: -1px;
    bottom: -1px;
    display: flex;
    flex-direction: column;
    align-items: flex-start;
    width: 76px;
    padding: 10px;
    border-top: 1px solid var(--line-30);
    border-radius: 0 10px 10px 10px;
    background-image: linear-gradient(90deg, #000 40.385%, rgba(102, 102, 102, 0) 100%);
  }
  .stat {
    display: flex;
    align-items: center;
    font-family: var(--font-condensed);
    font-weight: 500;
    font-size: 14px;
    line-height: normal;
    white-space: nowrap;
    color: var(--text);
  }
  .stat img {
    display: block;
    width: 16px;
    height: 16px;
    margin-right: 4px;
  }
</style>
