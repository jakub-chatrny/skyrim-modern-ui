# Technical notes

How the plugin and the page work together, and what was learned getting there. The README covers trying the
mod and the design; this is for working on the code.

## Bridge (plugin ↔ UI)

Defined in [shared/schema/bridge.schema.json](shared/schema/bridge.schema.json);
TypeScript mirror in `ui/src/bridge/types.ts`, C++ producer in
`plugin/src/Snapshot.cpp`.

- plugin → UI: `window.smuiReceive(json)` with a `Snapshot`, `window.smuiItems(json)` with an `ItemList`
- UI → plugin: `window.smuiCommand(json)` with a `Command`
  (`ready`, `close`, `equip`, `unequip`, `listItems`, `switchOutfit`)
- plugin → UI: `window.smuiKey(name)` with a key press. Prisma UI does not pass the keyboard to the page, so the
  plugin reads the screen's keys (WASD, arrows, Enter, Space, Q / E, 1–4) and hands them over
  (`plugin/src/CharacterMenu.cpp`, `ui/src/lib/navigation.ts`).

The plugin pushes a snapshot when the screen opens, when the UI says `ready`, and whenever the player's
equipment changes while the screen is open. Each snapshot is also written to `SkyrimModernUI.log`, and so is
everything the page writes to its console (`[ui]` lines), including script errors.

What the numbers mean (see `plugin/src/Snapshot.cpp`, `plugin/src/Equipment.cpp`):

- Item armor and damage are the values the game's own menus show (skill, perks, tempering applied).
- A bow or crossbow is reported in `ranged`, not `rightHand`; the game has no separate ranged slot.
- `cloak` and `mask` use the common mod slots (46/47 and 44/55); the base game has nothing for them.
- `damage.magicPower` is the Destruction power modifier for now; the design does not define it yet.
- Skill names are English constants.
- `effects`: active effects as the Magic menu lists them, one row per spell / potion / food, with the effect
  description's `<mag>` / `<dur>` filled in and the time left.
- `survival` is sent only with Survival Mode on, read from its `Survival_*` global variables (confirmed in game
  2026-11-22; every `Survival_` global is logged once). Survival Mode has no thirst.
- Equip / unequip ignore which exact copy of an item is meant (tempered vs plain); spells cannot be unequipped yet.


## Controls after loading a save

Sometimes, after loading a save, the player could not move or look around until the screen was opened and
closed. Cause (confirmed 2026-11-18: right after a load the log showed exactly Prisma's set disabled): the save
was made while the screen had focus, with Prisma UI's control block in place.
On every load the plugin now logs the enabled control groups and, if exactly Prisma's set is off (looking,
movement, jumping, activate, camera switch, zoom, VATS) while fighting is still on, re-enables them. Works.

## Choosing gear

Clicking a slot asks the plugin for everything in the inventory that fits it (`listItems`) and shows the
answer in a popup; picking a row sends `equip`, the "Nothing" row sends `unequip`.

- Left hand: shields, one-handed weapons, staves, torches. Right hand: any melee weapon or staff.
  Ranged: bows and crossbows. Spells are not listed yet.
- The list is per item type, not per copy: a tempered and a plain copy of the same item are one row.

## Item pictures

There are no item thumbnails in the game's data, so the plugin makes them (`plugin/src/Thumbnails.cpp`).
**Working** (confirmed in game 2026-11-22: all twelve items of the test character in under three seconds, no
failures). On by default; `"itemPictures": false` in `SkyrimModernUI.json` switches making new ones off.

How it works:

- While the screen is open, each item without a picture is loaded into the vanilla inventory preview
  (`Inventory3DManager`), the same one the inventory menu uses, with its lighting.
- A small invisible menu of ours does the capture in its `PostDisplay`, the game's menu drawing pass, which is
  where the inventory menu itself draws the preview: it clears a square of the frame to black, lets the engine
  paint the item there, copies it out and puts the frame back, so the item never shows. The square comes from
  the interface scene's own camera. The picture is scaled to 256 px and saved as
  `Data/PrismaUI/views/SkyrimModernUI/thumbs/<form id>.png` (kept by `deploy.sh`).
- The preview holds at most 7 models and unloading does not free a slot, so the scene is reset at 5; it is
  never torn down while a load is in flight (the crash Grid Inventory documents).
- The capture menu **pauses the game** while it runs (about 0.1 s per item): the engine's preview draws nothing
  while the game runs. Closing the screen closes it at once; a session that cannot be ended safely is reused.
- `Render()` is called **every frame** while an item is pending, with the whole frame saved and put back around
  it: the engine finishes loading the preview inside `Render()`. Without this only models already in memory
  loaded (4 of 12).
- Armor with a ground model for one sex only has the empty slot filled first (Grid Inventory's rule).
- Items whose picture cannot be made are given up after 1.5 s and listed in `thumbs/failed.txt`.
- On open, every equippable item in the inventory without a picture is queued; `picturesPending` tells the UI.
  Delete the folder to have everything pictured again.

The technique (preview manager plus save / clear / render / copy / restore) is taken from Modex,
`src/ui/components/Item3DPreview.cpp` in [ModExplorerMenu](https://github.com/patchulidev/ModExplorerMenu), which
[Grid Inventory](https://github.com/skypia0147-dev/grid-inventory) also builds on. Grid Inventory's notes cover
improvements not done here: rendering over black and over white to get real transparency, giving slow-loading
models a later second chance, and per-category orientation.

History: a first version placed each item's world model where the character stands and captured the world
camera's view; in game the pictures were black, then plain background. A second used the preview manager from a
frame hook instead of a menu: the engine painted nothing there, and loads failed once the 7-slot array was full.
A third ran from a menu as described above, and the engine still painted nothing; preview loads stayed
pending. Ruled out: the world-hiding cull pass (the interface scene is not in the world's graph). Menu mode
(`kUsesMenuContext`, as Modex's and Grid Inventory's menus set) did not help either: loads still stayed pending,
and once the scene could not be torn down safely.

Reference test (2026-11-18): Grid Inventory 1.6.6 installed in the game folder. After its icon cache reset it
captured fresh icons in game within ~0.1 s each, including items that time out in ours, so the technique works
under Proton and the fault is in this port. Its code states the missing piece: `kPausesGame` is required,
`Inventory3DManager::Render()` draws nothing while the game runs. With the capture menu pausing the game and
beginning the session on show, as Grid Inventory and Modex do, the engine painted the items. Grid Inventory was
removed again afterwards.
Item lighting partly follows the sky where the player stands (Grid Inventory measured this): pictures made in a
dark interior come out darker.

Known limits: texture variants that share one model look alike; enchantment glow may be missing on the first
capture of a model (noted by Grid Inventory); items created in game (player-enchanted) are skipped because their
ids are not stable; spells have no item model.

## Outfits

Four equipment sets, kept in the SKSE co-save, so they belong to a save game (`plugin/src/Outfits.cpp`).

- The active outfit is whatever is being worn; there is nothing to "save".
- Switching stores the current gear in the outfit being left and puts on the one being entered.
- An outfit that was never used starts as a copy of the current gear.
- Items an outfit wants but the player no longer has are skipped and counted; the UI shows the count.

## Camera and walls (open bug)

Standing with a wall behind the camera, the third-person camera stops at the wall even though the world is
hidden, so the character is shown far too close. Tried so far (2026-11-23), neither helped:

1. Clearing the camera sphere's collision layer (`COL_LAYER::kCameraSphere`) in `bhkCollisionFilter` while the
   screen is open. It ran (logged), but the camera still stopped at the wall: the game's check does not go
   through that layer.
2. Replacing `ThirdPersonState::GetTranslation` (vtable slot 5) to answer with a learned offset from the
   player. The hook installed, but the game never called it while the camera moved: the camera position does
   not come through that function. Still in `CameraStage.cpp` behind `"ignoreWalls"` (off by default).

`ThirdPersonState::UpdateCameraCollision` is the function to replace, but CommonLibSSE-NG has its address
only for SE (ID 49980), not for AE 1.6+/1.7. Next ideas: find its AE address, or hook `ThirdPersonState::Update`
(slot 3) and write the camera node's position after it.
