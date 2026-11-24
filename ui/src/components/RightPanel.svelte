<script lang="ts">
  // Figma node 2019:6253 (Character / Iter 4), right column: defense, damage,
  // and skills split into Mage / Warrior / Thief tabs.
  import damageLine from '../assets/design/iter4/header-line-damage.svg'
  import defenseLine from '../assets/design/iter4/header-line-defense.svg'
  import skillsLine from '../assets/design/iter4/header-line-skills.svg'
  import skillFill from '../assets/design/iter4/skill-fill.svg'
  import skillSubFill from '../assets/design/iter4/skill-sub-fill.svg'
  import skillSubTrack from '../assets/design/iter4/skill-sub-track.svg'
  import skillTrack from '../assets/design/iter4/skill-track.svg'
  import tabUnderline from '../assets/design/iter4/tab-underline.svg'
  import type { Skill, Snapshot } from '../bridge/types'
  import { DAMAGE, ELEMENT, RESIST, SKILL, TREE } from '../lib/glyphs'
  import Glyph from './Glyph.svelte'
  import SectionHeader from './SectionHeader.svelte'

  let { snapshot }: { snapshot: Snapshot } = $props()

  const whole = (v: number) => String(Math.round(v))
  const percent = (v: number) => `${whole(v)}%`

  type Figure = { label: string; value: string; path: string; color: string }
  const defense = $derived<Figure[]>([
    { label: 'Armor', value: whole(snapshot.resistances.armor), ...RESIST.armor },
    { label: 'Magic', value: percent(snapshot.resistances.magic), ...RESIST.magic },
    { label: 'Fire', value: percent(snapshot.resistances.fire), ...RESIST.fire },
    { label: 'Frost', value: percent(snapshot.resistances.frost), ...RESIST.frost },
  ])
  // The design shows ranges (38–46); the game has one number per weapon.
  const damage = $derived<Figure[]>([
    { label: 'Melee', value: whole(snapshot.damage.rightHand), ...DAMAGE.physical },
    {
      label: 'Ranged',
      value: snapshot.damage.ranged > 0 ? whole(snapshot.damage.ranged) : '–',
      path: SKILL.Archery.path,
      color: ELEMENT.physical,
    },
    { label: 'Magic', value: percent(snapshot.damage.magicPower), ...DAMAGE.magic },
  ])

  // The three skill trees as the game groups them, in the design's order.
  const GROUPS = {
    Mage: ['Alteration', 'Conjuration', 'Destruction', 'Enchanting', 'Illusion', 'Restoration'],
    Warrior: ['Smithing', 'HeavyArmor', 'Block', 'TwoHanded', 'OneHanded', 'Archery'],
    Thief: ['LightArmor', 'Sneak', 'Lockpicking', 'Pickpocket', 'Speech', 'Alchemy'],
  } as const
  type Group = keyof typeof GROUPS
  const groupNames = Object.keys(GROUPS) as Group[]

  const skillsIn = (group: Group): Skill[] =>
    GROUPS[group].map((id) => snapshot.skills.find((skill) => skill.id === id)).filter((s): s is Skill => !!s)
  const total = (group: Group) => skillsIn(group).reduce((sum, skill) => sum + skill.level, 0)

  // Opens on the tree the character has put the most into.
  let chosen = $state<Group | null>(null)
  const group = $derived(chosen ?? groupNames.reduce((best, g) => (total(g) > total(best) ? g : best), 'Warrior'))
  const skills = $derived(skillsIn(group))

  const clamp = (v: number) => Math.max(0, Math.min(1, v))
</script>

<aside class="column">
  <section>
    <SectionHeader title="Defense" line={defenseLine} />
    <dl class="numbers four">
      {#each defense as figure}<dt><Glyph path={figure.path} color={figure.color} size={24} />{figure.label}</dt>{/each}
      {#each defense as figure}<dd>{figure.value}</dd>{/each}
    </dl>
  </section>

  <section>
    <SectionHeader title="Damage" line={damageLine} />
    <dl class="numbers three">
      {#each damage as figure}<dt><Glyph path={figure.path} color={figure.color} size={24} />{figure.label}</dt>{/each}
      {#each damage as figure}<dd>{figure.value}</dd>{/each}
    </dl>
  </section>

  <section class="skills">
    <SectionHeader title="Skills" line={skillsLine} />
    <div class="tabs">
      {#each groupNames as name}
        <button class="tab" class:active={name === group} onclick={() => (chosen = name)}>
          <span>{name}</span>
          {#if name === group}<img class="underline" alt="" src={tabUnderline} />{/if}
        </button>
      {/each}
    </div>
    <ul>
      {#each skills as skill}
        <!-- Not in the design: the skill level, muted, after the name. -->
        <li>
          <p class="skill-name">
            {#if SKILL[skill.id]}<Glyph path={SKILL[skill.id].path} color={TREE[SKILL[skill.id].tree]} size={26} />{/if}
            {skill.name} <span class="level">{skill.level}</span>
          </p>
          <span class="skill-bar" title="Level {skill.level}">
            <span class="track">
              <img alt="" class="part" src={skillFill} style="width: {clamp(skill.level / 100) * 100}%" />
              <img alt="" class="part rest" src={skillTrack} />
            </span>
            <span class="sub">
              <img alt="" class="part" src={skillSubFill} style="width: {clamp(skill.progress ?? 0) * 100}%" />
              <img alt="" class="part rest" src={skillSubTrack} />
            </span>
          </span>
        </li>
      {/each}
    </ul>
  </section>
</aside>

<style>
  .column {
    display: flex;
    flex-direction: column;
    align-items: center;
    flex-shrink: 0;
    width: 651px;
    height: 1186px;
    line-height: normal;
    color: var(--ink);
  }
  section {
    width: 100%;
  }
  section + section {
    margin-top: 96px;
  }

  .numbers {
    display: grid;
    grid-template-rows: auto auto;
    row-gap: 6px;
    margin: 15px 0 0;
    white-space: nowrap;
  }
  .numbers.four {
    grid-template-columns: repeat(4, minmax(0, 1fr));
    column-gap: 16px;
  }
  .numbers.three {
    grid-template-columns: repeat(3, minmax(0, 1fr));
    column-gap: 50px;
  }
  dt {
    display: flex;
    align-items: center;
    font-family: var(--font-label);
    font-size: 22px;
    color: var(--label);
  }
  dt > :global(svg) {
    margin-right: 8px;
  }
  dd {
    margin: 0;
    font-family: var(--font-number);
    font-size: 36px;
    color: var(--ink);
  }

  .tabs {
    display: flex;
    align-items: flex-start;
    margin-top: 24px;
  }
  .tab + .tab {
    margin-left: 13px;
  }
  .tab {
    display: flex;
    flex-direction: column;
    align-items: center;
    font-family: var(--font-medium);
    font-weight: 500;
    font-size: 22px;
    line-height: 26px;
    white-space: nowrap;
    color: var(--muted);
  }
  .tab.active {
    width: 96px;
    color: var(--ink);
  }
  .underline {
    display: block;
    width: 99px;
    height: 3px;
    margin-top: 4.5px;
  }
  .tab:focus span {
    color: var(--accent);
  }

  ul {
    margin: 24px 0 0;
    padding: 0;
    list-style: none;
  }
  li {
    display: flex;
    align-items: center;
    justify-content: space-between;
    width: 100%;
  }
  li + li {
    margin-top: 24px;
  }
  .skill-name {
    display: flex;
    align-items: center;
    font-family: var(--font-medium);
    font-weight: 500;
    font-size: 24px;
    white-space: nowrap;
  }
  .skill-name > :global(svg) {
    margin-right: 10px;
  }
  .level {
    margin-left: 6px;
    font-size: 22px;
    color: var(--muted);
  }
  .skill-bar {
    display: flex;
    flex-direction: column;
    flex-shrink: 0;
    width: 400px;
  }
  .track {
    display: flex;
    height: 10px;
    overflow: hidden;
    border-radius: 3px;
  }
  .sub {
    display: flex;
    height: 2px;
    padding: 0 2px;
    overflow: hidden;
  }
  .part {
    display: block;
    flex-shrink: 0;
    height: 100%;
  }
  .sub .part {
    height: 4px;
  }
  .rest {
    flex: 1 1 0;
    min-width: 1px;
  }
</style>
