<script lang="ts">
  import type { Snapshot } from '../bridge/types'

  let { player }: { player: Snapshot['player'] } = $props()

  // Only Character exists in the first version; the rest are placeholders.
  const tabs = ['Map', 'Quests', 'Character', 'Magic', 'Inventory', 'Skills']
</script>

<header>
  <div class="who">
    <strong>{player.name}</strong>
    <span>Level {player.level}</span>
    <div class="xp"><div style="width: {player.xpProgress * 100}%"></div></div>
  </div>
  <nav>
    {#each tabs as tab}
      <span class:active={tab === 'Character'}>{tab}</span>
    {/each}
  </nav>
  <div class="purse">
    <span>{Math.round(player.carryWeight.current)} / {Math.round(player.carryWeight.max)} wt</span>
    <span>{player.gold} gold</span>
  </div>
</header>

<style>
  header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 0.75rem 1.5rem;
    background: var(--panel);
    border-bottom: 1px solid var(--line);
  }
  .who,
  .purse,
  nav {
    display: flex;
    align-items: center;
    gap: 1rem;
  }
  .xp {
    width: 8rem;
    height: 0.25rem;
    background: var(--line);
  }
  .xp div {
    height: 100%;
    background: var(--accent);
  }
  nav span {
    color: var(--muted);
  }
  nav span.active {
    color: var(--text);
    border-bottom: 2px solid var(--accent);
  }
</style>
