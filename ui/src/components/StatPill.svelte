<script lang="ts">
  // Figma "ItemStat": a dark pill with a small line icon and a number. On the
  // item tiles 16 px condensed, in the details 22 px bold, in the top bar's
  // gold counter 22 px bold condensed.
  // The icons are the design's exported vectors, drawn inside a 14 px square
  // at the offsets the design gives them.
  import armor from '../assets/design/inventory/stat-armor.svg'
  import damage from '../assets/design/inventory/stat-damage.svg'
  import value from '../assets/design/inventory/stat-value.svg'
  import weight from '../assets/design/inventory/stat-weight.svg'

  type StatKind = 'armor' | 'damage' | 'weight' | 'value'

  let {
    kind,
    text,
    variant = 'tile',
  }: { kind: StatKind; text: string; variant?: 'tile' | 'detail' | 'counter' } = $props()

  // [icon, width, height, left, top] in design pixels.
  const ICONS: Record<StatKind, [string, number, number, number, number]> = {
    armor: [armor, 9.167, 11.5, 2.42, 1.25],
    damage: [damage, 10.8, 12.083, 1.6, 0.96],
    weight: [weight, 10.363, 10.363, 1.82, 1.82],
    value: [value, 10.333, 9.75, 1.83, 2.71],
  }
  const icon = $derived(ICONS[kind])
</script>

<span class="pill pill-{variant}" class:gold={kind === 'value'}>
  <span class="icon">
    <img alt="" src={icon[0]} style="width: {icon[1]}px; height: {icon[2]}px; left: {icon[3]}px; top: {icon[4]}px" />
  </span>
  <span class="text">{text}</span>
</span>

<style>
  .pill {
    display: inline-flex;
    align-items: center;
    flex-shrink: 0;
    padding: 2px 10px 2px 6px;
    background: #0f1215;
    border: 1px solid #2e3133;
    border-radius: 999px;
  }
  .icon {
    position: relative;
    display: block;
    flex-shrink: 0;
    width: 14px;
    height: 14px;
    margin-right: 4px;
  }
  .icon img {
    position: absolute;
    display: block;
    max-width: none;
  }
  .text {
    font-family: var(--font-condensed-medium);
    font-weight: 500;
    font-size: 16px;
    line-height: normal;
    white-space: nowrap;
    color: #c9dce6;
  }
  .pill-detail .text {
    font-family: var(--font-display);
    font-weight: 700;
    font-size: 22px;
  }
  .pill-counter .text {
    font-family: var(--font-condensed);
    font-weight: 700;
    font-size: 22px;
  }
  .gold .text {
    color: #f2e2b8;
  }
</style>
