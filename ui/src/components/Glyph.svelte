<script lang="ts">
  // One line icon from lib/glyphs.ts, drawn the canvas's way: 24-unit grid,
  // 1.5 stroke, round caps and joins. `framed` puts it in the canvas's slot:
  // a dark square with a coloured border and a fainter inner line.
  let {
    path,
    color,
    size = 24,
    framed = false,
    dashed = false,
  }: { path: string; color: string; size?: number; framed?: boolean; dashed?: boolean } = $props()

  // "#RRGGBB" at an opacity, for the frame lines.
  function alpha(hex: string, a: number): string {
    const n = parseInt(hex.slice(1), 16)
    return `rgba(${(n >> 16) & 255}, ${(n >> 8) & 255}, ${n & 255}, ${a})`
  }
</script>

{#if framed}
  <span
    class="frame"
    style="width: {size}px; height: {size}px; border-color: {alpha(color, 0.6)}; outline-color: {alpha(color, 0.22)}"
  >
    <svg width={size * 0.56} height={size * 0.56} viewBox="0 0 24 24" fill="none" stroke={color} stroke-width="1.5"
      stroke-linecap="round" stroke-linejoin="round" stroke-dasharray={dashed ? '2 2.2' : 'none'} aria-hidden="true">
      <path d={path} />
    </svg>
  </span>
{:else}
  <svg class="bare" width={size} height={size} viewBox="0 0 24 24" fill="none" stroke={color} stroke-width="1.5"
    stroke-linecap="round" stroke-linejoin="round" stroke-dasharray={dashed ? '2 2.2' : 'none'} aria-hidden="true">
    <path d={path} />
  </svg>
{/if}

<style>
  .frame {
    display: flex;
    align-items: center;
    justify-content: center;
    flex-shrink: 0;
    box-sizing: border-box;
    background: rgba(10, 22, 32, 0.72);
    border: 1px solid;
    outline: 1px solid;
    outline-offset: -5px;
  }
  svg {
    display: block;
    flex-shrink: 0;
  }
</style>
