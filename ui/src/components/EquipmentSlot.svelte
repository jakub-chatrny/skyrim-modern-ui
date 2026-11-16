<script lang="ts">
  import type { SlotId, SlotItem } from '../bridge/types'

  let {
    slot,
    item,
    previous,
    onclick,
  }: { slot: SlotId; item: SlotItem | null; previous?: SlotItem; onclick: () => void } = $props()
</script>

<button
  class="slot"
  class:empty={!item}
  disabled={!item && !previous}
  {onclick}
  title={item ? 'Unequip' : previous ? 'Equip again' : ''}
>
  <span class="label">{slot}</span>
  {#if item}
    <span class="name">{item.name}</span>
    <span class="stats">
      {#each item.stats as stat}
        <span>{stat.label} {stat.value}</span>
      {/each}
    </span>
  {:else if previous}
    <span class="name">Empty</span>
    <span class="stats"><span>Equip {previous.name} again</span></span>
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
  /* Clicks should land on the button itself, like on the plain-text outfit
     buttons, not on the spans inside it. */
  .slot > :global(*) {
    pointer-events: none;
  }
  .label {
    font-size: 0.7rem;
    text-transform: uppercase;
    color: var(--muted);
  }
  .stats {
    display: flex;
    flex-wrap: wrap;
    gap: 0 0.75rem;
    font-size: 0.8rem;
    color: var(--muted);
  }
  .empty .name {
    color: var(--muted);
  }
</style>
