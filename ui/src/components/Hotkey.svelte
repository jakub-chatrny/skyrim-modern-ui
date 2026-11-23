<script lang="ts">
  // Figma "Hotokey": a key in a thin box, optionally followed by what it does.
  let { key, action }: { key: string; action?: string } = $props()
</script>

<span class="hotkey">
  <span class="key">{key}</span>
  {#if action}<span class="action">{action}</span>{/if}
</span>

<style>
  .hotkey {
    display: flex;
    align-items: center;
    flex-shrink: 0;
    font-family: var(--font-medium);
    font-weight: 500;
    font-size: 22px;
    line-height: normal;
    white-space: nowrap;
  }
  .key {
    display: flex;
    align-items: center;
    justify-content: center;
    padding: 4px 12px;
    color: var(--key-text);
    border: 1px solid var(--key-line);
    border-radius: 4px;
  }
  .action {
    margin-left: 10px;
    color: var(--muted);
  }
</style>
