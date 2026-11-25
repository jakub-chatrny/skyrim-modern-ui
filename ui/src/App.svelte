<script lang="ts">
  import { connect } from './bridge'
  import type { Inventory, ItemList, Snapshot } from './bridge/types'
  import type { MainTab } from './components/TopBar.svelte'
  import CharacterScreen from './screens/CharacterScreen.svelte'
  import InventoryScreen from './screens/InventoryScreen.svelte'

  let snapshot = $state<Snapshot | null>(null)
  let itemList = $state<ItemList | null>(null)
  let inventory = $state<Inventory | null>(null)
  let mock = $state(false)
  // Kept while the screen is closed: it opens on the tab it was left on.
  // In a browser, index.html#inventory opens on the Inventory tab.
  let tab = $state<MainTab>(location.hash === '#inventory' ? 'Inventory' : 'Character')

  // The plugin sends a snapshot when a save loads, while the page is still
  // hidden. Lay the page out and decode its pictures then, so the first open
  // has less to do.
  let warmed = false
  function warmUp() {
    if (warmed) return
    warmed = true
    setTimeout(() => {
      const started = performance.now()
      document.body.getBoundingClientRect()
      for (const image of Array.from(document.images)) image.decode?.().catch(() => {})
      console.log(`[ui] page laid out ahead of time in ${Math.round(performance.now() - started)} ms`)
    }, 0)
  }

  const bridge = connect({
    onSnapshot(next) {
      snapshot = next
      mock = bridge.mock
      warmUp()
    },
    onItems(list) {
      itemList = list
    },
    onInventory(next) {
      inventory = next
    },
  })

  function ontab(next: MainTab) {
    console.log(`[ui] tab: ${next}`)
    tab = next
  }

  // No key handling here on purpose: opening and closing is the plugin's
  // toggle key only (Tab by default). Esc is left alone for now.
</script>

{#if snapshot}
  {#if tab === 'Inventory'}
    <InventoryScreen {snapshot} {inventory} {bridge} {mock} {ontab} />
  {:else}
    <CharacterScreen {snapshot} {itemList} {bridge} {mock} {ontab} />
  {/if}
{/if}
