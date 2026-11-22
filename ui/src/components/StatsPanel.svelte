<script lang="ts">
  // Figma node 18:12224: the statistics panel on the right.
  import type { Gauge, Snapshot } from '../bridge/types'
  import { statIcons } from '../lib/icons'
  import Ornament from './Ornament.svelte'

  let { snapshot }: { snapshot: Snapshot } = $props()

  type Row = { icon: string; label: string; value: string; strong?: boolean }
  type Group = { title: string; rows: Row[] }

  const whole = (v: number) => String(Math.round(v))
  const ratio = (g: Gauge) => `${whole(g.current)}/${whole(g.max)}`
  const percent = (v: number) => `${whole(v)}%`
  const speed = (v: number) => (v > 0 ? v.toFixed(2) : '–')

  // The panel is as tall as the stage. Without the survival block there is
  // room for five more skill rows than the design shows.
  const SKILL_ROWS = 11
  const SURVIVAL_ROWS = 5

  const groups = $derived.by((): Group[] => {
    const { vitals, survival, resistances: r, damage: d, skills } = snapshot
    const result: Group[] = [
      {
        title: 'Basic statistics',
        rows: [
          { icon: statIcons.health, label: 'Health', value: ratio(vitals.health), strong: true },
          { icon: statIcons.stamina, label: 'Stamina', value: ratio(vitals.stamina), strong: true },
          { icon: statIcons.magicka, label: 'Magicka', value: ratio(vitals.magicka), strong: true },
        ],
      },
    ]

    if (survival) {
      const rows: Row[] = []
      if (survival.energy) rows.push({ icon: statIcons.energy, label: 'Energy', value: whole(survival.energy.current) })
      if (survival.hunger) rows.push({ icon: statIcons.hunger, label: 'Hunger', value: whole(survival.hunger.current) })
      if (survival.thirst) rows.push({ icon: statIcons.thirst, label: 'Thirst', value: whole(survival.thirst.current) })
      if (survival.warmth) rows.push({ icon: statIcons.warmth, label: 'Warmth', value: whole(survival.warmth.current) })
      if (rows.length) result.push({ title: 'Survival statistics', rows })
    }

    result.push(
      {
        title: 'Resistances',
        rows: [
          { icon: statIcons.armor, label: 'Armor', value: whole(r.armor) },
          { icon: statIcons.magic, label: 'Magic resist', value: percent(r.magic) },
          { icon: statIcons.fire, label: 'Fire resist', value: percent(r.fire) },
          { icon: statIcons.frost, label: 'Frost resist', value: percent(r.frost) },
          { icon: statIcons.shock, label: 'Shock resist', value: percent(r.shock) },
          { icon: statIcons.poison, label: 'Poison resist', value: percent(r.poison) },
          { icon: statIcons.disease, label: 'Disease resist', value: percent(r.disease) },
        ],
      },
      {
        title: 'Damage statistics',
        rows: [
          { icon: statIcons.damage, label: 'Melee damage', value: whole(d.rightHand) },
          { icon: statIcons.speed, label: 'Melee speed', value: speed(d.meleeSpeed) },
          { icon: statIcons.ranged, label: 'Ranged damage', value: whole(d.ranged) },
          { icon: statIcons.speed, label: 'Ranged speed', value: speed(d.rangedSpeed) },
          { icon: statIcons.magic, label: 'Magic power', value: percent(d.magicPower) },
        ],
      },
      {
        title: 'Skills',
        rows: [...skills]
          .sort((a, b) => b.level - a.level)
          .slice(0, SKILL_ROWS + (survival ? 0 : SURVIVAL_ROWS))
          .map((skill) => ({ icon: statIcons.skill, label: skill.name, value: String(skill.level) })),
      },
    )
    return result
  })
</script>

<aside class="panel">
  {#each groups as group}
    <section>
      <p class="title">{group.title}</p>
      {#each group.rows as row}
        <div class="row">
          <div class="name">
            <span class="icon"><img alt="" src={row.icon} /></span>
            <p>{row.label}</p>
          </div>
          <p class="value" class:strong={row.strong}>{row.value}</p>
        </div>
      {/each}
    </section>
  {/each}
  <Ornament />
  <Ornament mirrored />
</aside>

<style>
  .panel {
    position: relative;
    display: flex;
    flex-direction: column;
    align-items: flex-start;
    flex-shrink: 0;
    width: 399px;
    padding: 24px 40px;
    border: 1px solid var(--line-30);
    border-radius: 40px;
    background-image: linear-gradient(
      180deg,
      rgba(0, 0, 0, 0.2) 51.923%,
      rgba(51, 51, 51, 0.1) 83.654%,
      rgba(102, 102, 102, 0) 100%
    );
    font-family: var(--font-condensed);
    font-weight: 500;
    font-size: 24px;
    line-height: 29px;
  }
  section {
    width: 100%;
  }
  section + section {
    margin-top: 10px;
  }
  .title {
    color: var(--text-60);
    white-space: nowrap;
  }
  .row {
    display: flex;
    align-items: center;
    justify-content: space-between;
    width: 100%;
    margin-top: 2px;
    white-space: nowrap;
  }
  .name {
    display: flex;
    align-items: center;
  }
  .icon {
    display: flex;
    align-items: center;
    justify-content: center;
    width: 16px;
    height: 16px;
    margin-right: 8px;
  }
  .icon img {
    display: block;
    max-width: 16px;
    max-height: 16px;
  }
  .value.strong {
    font-weight: 700;
  }
</style>
