<script lang="ts">
  // Figma node 2019:6476 (Character / Iter 4), left column: active effects at
  // the top; condition (survival needs) and the three vitals at the bottom.
  import condLine from '../assets/design/iter4/header-line-condition.svg'
  import effectsLine from '../assets/design/iter4/header-line-effects.svg'
  import diamondHealth from '../assets/design/iter4/diamond-health.svg'
  import diamondMana from '../assets/design/iter4/diamond-mana.svg'
  import diamondStamina from '../assets/design/iter4/diamond-stamina.svg'
  import iconOther from '../assets/design/iter4/icon-bow-small.svg'
  import iconCold from '../assets/design/iter4/icon-cold.svg'
  import iconFood from '../assets/design/iter4/icon-food.svg'
  import iconSleep from '../assets/design/iter4/icon-sleep.svg'
  import iconThirst from '../assets/design/iter4/icon-thirst.svg'
  import pipEmpty from '../assets/design/iter4/pip-empty.svg'
  import pipFilled from '../assets/design/iter4/pip-filled.svg'
  import pipWarm from '../assets/design/iter4/pip-warm.svg'
  import type { ActiveEffect, Gauge, Snapshot } from '../bridge/types'
  import SectionHeader from './SectionHeader.svelte'

  let { snapshot }: { snapshot: Snapshot } = $props()

  // The column has room for this many effect rows above the condition block.
  const MAX_EFFECTS = 8

  const effectIcon: Record<ActiveEffect['kind'], string> = { cold: iconCold, food: iconFood, other: iconOther }

  // Cold lasts until the player warms up; other effects without an end are abilities.
  function duration(seconds: number, kind: ActiveEffect['kind']): string {
    if (seconds < 0) return kind === 'cold' ? 'until warm' : ''
    if (seconds < 60) return `${Math.max(1, Math.round(seconds))} s`
    if (seconds < 3600) return `${Math.round(seconds / 60)} min`
    return `${Math.round(seconds / 3600)} h`
  }

  const effects = $derived((snapshot.effects ?? []).slice(0, MAX_EFFECTS))

  // A need as five pips, full when the player is fine, and a word for it.
  type Need = { icon: string; label: string; pips: number; state: string; warm: boolean }
  const PIPS = 5
  const pipsOf = (gauge: Gauge) => Math.round((gauge.max > 0 ? gauge.current / gauge.max : 0) * PIPS)
  // States by pip count, 0 .. 5.
  const states = {
    energy: ['Exhausted', 'Drained', 'Weary', 'Tired', 'Rested', 'Well rested'],
    hunger: ['Starving', 'Ravenous', 'Hungry', 'Peckish', 'Fed', 'Satiated'],
    thirst: ['Dehydrated', 'Parched', 'Thirsty', 'Dry', 'Quenched', 'Quenched'],
    warmth: ['Freezing', 'Numb', 'Chilled', 'Cool', 'Warm', 'Warm'],
  }

  const needs = $derived.by((): Need[] => {
    const survival = snapshot.survival
    if (!survival) return []
    const list: Need[] = []
    const add = (key: keyof typeof states, icon: string, label: string) => {
      const gauge = survival[key]
      if (!gauge) return
      const pips = pipsOf(gauge)
      list.push({ icon, label, pips, state: states[key][pips], warm: key === 'warmth' })
    }
    add('energy', iconSleep, 'Sleep')
    add('hunger', iconFood, 'Hunger')
    add('thirst', iconThirst, 'Thirst')
    add('warmth', iconCold, 'Warmth')
    return list
  })

  const whole = (v: number) => String(Math.round(v))
  const fraction = (g: Gauge) => (g.max > 0 ? Math.max(0, Math.min(1, g.current / g.max)) : 0)
  const vitals = $derived([
    { label: 'Mana', gauge: snapshot.vitals.magicka, diamond: diamondMana, colour: 'var(--mana)' },
    { label: 'Stamina', gauge: snapshot.vitals.stamina, diamond: diamondStamina, colour: 'var(--stamina)' },
    { label: 'Health', gauge: snapshot.vitals.health, diamond: diamondHealth, colour: 'var(--health)' },
  ])
</script>

<aside class="column">
  <section class="effects">
    <SectionHeader title="Active effects" line={effectsLine} />
    {#if effects.length}
      <div class="effect-grid">
        {#each effects as effect}
          <span class="effect-icon"><img alt="" src={effectIcon[effect.kind]} /></span>
          <div class="effect-text">
            <p class="effect-name">{effect.name}</p>
            {#if effect.description}
              <p class:positive={effect.positive} class:negative={!effect.positive}>{effect.description}</p>
            {/if}
          </div>
          <p class="note">{duration(effect.remaining, effect.kind)}</p>
        {/each}
      </div>
    {:else}
      <p class="none">None</p>
    {/if}
  </section>

  <section class="condition">
    {#if needs.length}
      <SectionHeader title="Condition" line={condLine} lineHeight={10} gap={19} />
      <div class="need-grid">
        {#each needs as need}
          <span class="need-icon"><img alt="" src={need.icon} /></span>
          <p class="need-label">{need.label}</p>
          <span class="pips">
            {#each Array(PIPS) as _, index}
              <span class="pip"><img alt="" src={index < need.pips ? (need.warm ? pipWarm : pipFilled) : pipEmpty} /></span>
            {/each}
          </span>
          <p class="note" class:negative={need.pips <= 2}>{need.state}</p>
        {/each}
      </div>
    {/if}

    <div class="vital-grid">
      {#each vitals as vital}
        <div class="vital-bar">
          <span class="diamond"><img alt="" src={vital.diamond} /></span>
          <span class="bar">
            <span class="fill" style="width: {fraction(vital.gauge) * 100}%; background: {vital.colour}"></span>
          </span>
        </div>
        <p class="vital-label">{vital.label}</p>
        <p class="vital-value">{whole(vital.gauge.current)} / {whole(vital.gauge.max)}</p>
      {/each}
    </div>
  </section>
</aside>

<style>
  .column {
    display: flex;
    flex-direction: column;
    justify-content: space-between;
    flex-shrink: 0;
    width: 651px;
    height: 1186px;
    font-family: var(--font-medium);
    font-weight: 500;
    line-height: normal;
    color: var(--ink);
  }
  p {
    white-space: nowrap;
  }

  .effect-grid {
    display: grid;
    grid-template-columns: auto minmax(0, 1fr) auto;
    column-gap: 16px;
    row-gap: 16px;
    margin-top: 16px;
    font-size: 22px;
  }
  .effect-icon,
  .need-icon {
    display: flex;
    align-items: center;
    justify-content: center;
    width: 32px;
    height: 32px;
  }
  .effect-icon img,
  .need-icon img {
    display: block;
  }
  .effect-text {
    min-width: 0;
  }
  .effect-text p {
    overflow: hidden;
    text-overflow: ellipsis;
  }
  .positive {
    color: var(--positive);
  }
  .negative {
    color: var(--negative);
  }
  .note {
    justify-self: end;
    font-style: italic;
    font-size: 22px;
    color: var(--muted);
  }
  .note.negative {
    color: var(--negative);
  }
  .none {
    margin-top: 16px;
    font-style: italic;
    font-size: 22px;
    color: var(--muted);
  }

  .condition {
    display: flex;
    flex-direction: column;
    width: 100%;
  }
  /* Spacing set per block: a scoped `* + *` would not see SectionHeader's root. */
  .need-grid {
    margin-top: 16px;
    margin-bottom: 24px;
    display: grid;
    grid-template-columns: minmax(0, 1fr) minmax(0, 3fr) minmax(0, 5fr) minmax(0, 2fr);
    column-gap: 16px;
    row-gap: 16px;
  }
  .need-label {
    font-size: 24px;
  }
  .pips {
    display: flex;
    align-items: center;
    align-self: center;
  }
  .pip {
    display: flex;
    align-items: center;
    justify-content: center;
    width: 18px;
    height: 18px;
  }
  .pip + .pip {
    margin-left: 5px;
  }
  .pip img {
    display: block;
    -webkit-transform: rotate(-45deg);
    transform: rotate(-45deg);
  }

  .vital-grid {
    display: grid;
    grid-template-columns: minmax(0, 4fr) minmax(0, 1fr) minmax(0, 1fr);
    column-gap: 16px;
    row-gap: 4px;
    font-family: var(--font-number);
    font-weight: 400;
  }
  .vital-bar {
    display: flex;
    align-items: center;
  }
  .diamond {
    display: flex;
    align-items: center;
    justify-content: center;
    flex-shrink: 0;
    width: 14px;
    height: 14px;
    margin-right: 6px;
  }
  .diamond img {
    display: block;
    -webkit-transform: rotate(-45deg);
    transform: rotate(-45deg);
  }
  .bar {
    display: flex;
    flex: 1 1 0;
    min-width: 1px;
    height: 18px;
    padding: 3px;
    overflow: hidden;
    border: 1.5px solid var(--line-30);
    border-radius: 8px;
  }
  .fill {
    display: block;
    height: 100%;
    border-radius: 5px;
  }
  .vital-label {
    font-size: 24px;
  }
  .vital-value {
    font-size: 22px;
    color: var(--muted);
  }
</style>
