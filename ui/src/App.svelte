<script lang="ts">
  import { connect } from './bridge'
  import type { ItemList, Snapshot } from './bridge/types'
  import CharacterScreen from './screens/CharacterScreen.svelte'

  let snapshot = $state<Snapshot | null>(null)
  let itemList = $state<ItemList | null>(null)
  let mock = $state(false)

  const bridge = connect({
    onSnapshot(next) {
      snapshot = next
      mock = bridge.mock
    },
    onItems(list) {
      itemList = list
    },
  })

  // No key handling here on purpose: opening and closing is the plugin's F4
  // hotkey only. Esc is left alone for now.
</script>

{#if snapshot}
  <CharacterScreen {snapshot} {itemList} {bridge} {mock} />
{/if}
