<script lang="ts">
  // Figma "ItemTile" (Character / Iter 4): a 120 x 120 tile. Empty, it shows the
  // slot's line icon from the design; with an item, the item's picture (or its
  // type icon while there is no picture yet).
  import type { SlotId, SlotItem } from '../bridge/types'
  import ItemArt from './ItemArt.svelte'

  let {
    slot,
    label,
    icon,
    item,
    onclick,
  }: {
    slot: SlotId
    label: string
    icon: string
    item: SlotItem | null
    onclick: (tile: HTMLElement) => void
  } = $props()
</script>

<button
  class="tile"
  class:photo={!!item?.thumb}
  data-slot={slot}
  onclick={(event) => onclick(event.currentTarget)}
  title={item ? `${label}: ${item.name}` : label}
>
  {#if item}
    <span class="art"><ItemArt {slot} {item} /></span>
  {:else}
    <span class="slot-icon"><img alt="" src={icon} /></span>
  {/if}
</button>

<style>
  .tile {
    position: relative;
    display: flex;
    align-items: center;
    justify-content: center;
    flex-shrink: 0;
    width: 120px;
    height: 120px;
    padding: 12px;
    overflow: hidden;
    background: var(--tile-bg);
    border-radius: 16px;
  }
  /* Not in the design, which has no selected tile: a 2px ring in the design's
     accent colour. Focus follows the mouse (lib/navigation.ts). */
  .tile:focus::after {
    content: '';
    position: absolute;
    top: 0;
    left: 0;
    right: 0;
    bottom: 0;
    border: 2px solid var(--accent);
    border-radius: 16px;
  }
  /* Clicks should land on the button itself, not on what is inside it. */
  .tile > :global(*) {
    pointer-events: none;
  }
  /* The design's 64 x 64 icon slot; each line icon sits centred at its own size. */
  .slot-icon {
    display: flex;
    align-items: center;
    justify-content: center;
    width: 64px;
    height: 64px;
  }
  .slot-icon img {
    display: block;
  }
  .art {
    display: block;
    width: 64px;
    height: 64px;
  }
  /* A picture uses the whole tile. */
  .photo .art {
    position: absolute;
    top: 0;
    left: 0;
    width: 100%;
    height: 100%;
  }
</style>
