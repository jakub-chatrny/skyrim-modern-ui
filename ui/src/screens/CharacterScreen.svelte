<script lang="ts">
  // Placeholder layout only: the regions from the plan, fed by real bridge
  // data. The Figma design is implemented in phase 4.
  import type { Bridge, SlotId, Snapshot } from '../bridge/types'
  import EquipmentSlot from '../components/EquipmentSlot.svelte'
  import StatsPanel from '../components/StatsPanel.svelte'
  import TopBar from '../components/TopBar.svelte'

  let { snapshot, bridge, mock }: { snapshot: Snapshot; bridge: Bridge; mock: boolean } = $props()

  const left: SlotId[] = ['head', 'body', 'hands', 'feet', 'leftHand', 'rightHand']
  const right: SlotId[] = ['mask', 'cloak', 'neck', 'ring', 'ranged', 'ammo']
  const outfits = [0, 1, 2, 3]
</script>

<div class="screen">
  <TopBar player={snapshot.player} />

  <div class="outfits">
    {#each outfits as index}
      <button onclick={() => bridge.send({ type: 'switchOutfit', index })}>Outfit {index + 1}</button>
    {/each}
  </div>

  <main>
    <section class="slots">
      {#each left as slot}
        <EquipmentSlot {slot} item={snapshot.equipment[slot]} onunequip={() => bridge.send({ type: 'unequip', slot })} />
      {/each}
    </section>

    <!-- Transparent on purpose: Prisma UI is an overlay, the game-rendered
         character shows through here (phase 2). -->
    <section class="stage">
      {#if mock}<span>character renders here (mock data)</span>{/if}
    </section>

    <section class="slots">
      {#each right as slot}
        <EquipmentSlot {slot} item={snapshot.equipment[slot]} onunequip={() => bridge.send({ type: 'unequip', slot })} />
      {/each}
    </section>

    <StatsPanel {snapshot} />
  </main>
</div>

<style>
  .screen {
    display: flex;
    flex-direction: column;
    height: 100vh;
  }
  .outfits {
    display: flex;
    gap: 0.5rem;
    padding: 0.5rem 1.5rem;
    background: var(--panel);
  }
  main {
    flex: 1;
    min-height: 0;
    display: flex;
  }
  .slots {
    width: 17rem;
    padding: 1rem;
    display: flex;
    flex-direction: column;
    gap: 0.5rem;
    background: var(--panel);
  }
  .stage {
    flex: 1;
    display: flex;
    align-items: center;
    justify-content: center;
    color: var(--muted);
  }
</style>
