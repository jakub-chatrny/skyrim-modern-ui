<script lang="ts">
  // Section title followed by the design's double rule, which fills the rest of the row.
  let { title, line, lineHeight = 5, gap = 23 }: { title: string; line: string; lineHeight?: number; gap?: number } =
    $props()
</script>

<div class="header">
  <p class="title">{title}</p>
  <span class="rule" style="margin-left: {gap}px; height: {lineHeight}px">
    <img alt="" src={line} />
  </span>
</div>

<style>
  .header {
    display: flex;
    align-items: center;
    width: 100%;
  }
  .title {
    flex-shrink: 0;
    font-family: var(--font-display);
    font-weight: 700;
    font-size: 24px;
    line-height: 28px;
    white-space: nowrap;
    color: var(--ink);
  }
  .rule {
    position: relative;
    flex: 1 1 0;
    min-width: 1px;
  }
  /* The rule's strokes stick out half a pixel above and below its box. */
  .rule img {
    position: absolute;
    top: -0.5px;
    left: 0;
    display: block;
    width: 100%;
    height: calc(100% + 1px);
  }
</style>
