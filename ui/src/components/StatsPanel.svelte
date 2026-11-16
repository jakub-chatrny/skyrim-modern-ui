<script lang="ts">
  import type { Gauge, Snapshot } from '../bridge/types'

  let { snapshot }: { snapshot: Snapshot } = $props()

  const gauge = (g: Gauge) => `${Math.round(g.current)} / ${Math.round(g.max)}`
  // Whole numbers as they are, everything else to two decimals (weapon speeds).
  const number = (v: number) => (Number.isInteger(v) ? String(v) : v.toFixed(Math.abs(v) < 10 ? 2 : 0))

  const groups = $derived([
    {
      title: 'Basic',
      rows: Object.entries(snapshot.vitals).map(([k, g]) => [k, gauge(g)]),
    },
    ...(snapshot.survival
      ? [{ title: 'Survival', rows: Object.entries(snapshot.survival).map(([k, g]) => [k, gauge(g)]) }]
      : []),
    {
      title: 'Resistances',
      rows: Object.entries(snapshot.resistances).map(([k, v]) => [k, number(v)]),
    },
    {
      title: 'Damage',
      rows: Object.entries(snapshot.damage).map(([k, v]) => [k, number(v)]),
    },
    {
      title: 'Skills',
      rows: snapshot.skills.map((s) => [s.name, String(s.level)]),
    },
  ])
</script>

<aside>
  {#each groups as group}
    <h2>{group.title}</h2>
    <dl>
      {#each group.rows as [label, value]}
        <div><dt>{label}</dt><dd>{value}</dd></div>
      {/each}
    </dl>
  {/each}
</aside>

<style>
  aside {
    width: 20rem;
    padding: 1rem 1.5rem;
    overflow-y: auto;
    background: var(--panel);
    border-left: 1px solid var(--line);
  }
  h2 {
    margin: 1rem 0 0.25rem;
    font-size: 0.75rem;
    text-transform: uppercase;
    color: var(--muted);
  }
  dl {
    margin: 0;
  }
  dl div {
    display: flex;
    justify-content: space-between;
    padding: 0.125rem 0;
  }
  dd {
    margin: 0;
  }
</style>
