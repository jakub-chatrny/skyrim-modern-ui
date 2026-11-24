<script lang="ts">
  // An item's picture: the cached in-game render when there is one, otherwise
  // (or if it fails to load) the generic icon for its type.
  import type { SlotId, SlotItem } from '../bridge/types'
  import { itemIcon } from '../lib/icons'

  let { slot, item }: { slot: SlotId; item: SlotItem | null } = $props()

  let failed = $state<string | null>(null)
  const thumb = $derived(item?.thumb && item.thumb !== failed ? item.thumb : null)

  // Logged so a picture that will not load shows up in SkyrimModernUI.log
  // instead of quietly turning back into the icon.
  function onError(url: string) {
    console.warn(`[ui] item picture did not load: ${url}`)
    failed = url
  }
</script>

{#if thumb}
  <img class="photo" alt="" src={thumb} onerror={() => onError(thumb)} />
{:else}
  <img class="icon" alt="" src={itemIcon(slot, item)} />
{/if}

<style>
  img {
    display: block;
    width: 100%;
    height: 100%;
  }
  /* Pictures are square renders with a transparent background; the tile shows behind them. */
  .photo {
    object-fit: cover;
    border-radius: inherit;
  }
</style>
