<script lang="ts">
  import { connect } from './bridge'
  import type { Snapshot } from './bridge/types'
  import CharacterScreen from './screens/CharacterScreen.svelte'

  let snapshot = $state<Snapshot | null>(null)
  let mock = $state(false)

  const bridge = connect((next) => {
    snapshot = next
    mock = bridge.mock
  })

  // No key handling here on purpose: opening and closing is the plugin's F4
  // hotkey only. Esc is left alone for now.
</script>

{#if snapshot}
  <CharacterScreen {snapshot} {bridge} {mock} />
{/if}
