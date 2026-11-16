<script lang="ts">
  import type { SlotId, SlotItem } from '../bridge/types'

  let { slot, item, onunequip }: { slot: SlotId; item: SlotItem | null; onunequip: () => void } = $props()
</script>

<button class="slot" class:empty={!item} disabled={!item} onclick={onunequip} title={item ? 'Unequip' : ''}>
  <span class="label">{slot}</span>
  {#if item}
    <span class="name">{item.name}</span>
    <span class="stats">
      {#each item.stats as stat}
        <span>{stat.label} {stat.value}</span>
      {/each}
    </span>
  {:else}
    <span class="name">Empty</span>
  {/if}
</button>

<style>
  .slot {
    display: flex;
    flex-direction: column;
    align-items: flex-start;
    gap: 0.125rem;
    padding: 0.5rem 0.75rem;
    text-align: left;
  }
  .label {
    font-size: 0.7rem;
    text-transform: uppercase;
    color: var(--muted);
  }
  .stats {
    display: flex;
    gap: 0.75rem;
    font-size: 0.8rem;
    color: var(--muted);
  }
  .empty .name {
    color: var(--muted);
  }
</style>
