# Phase 2 — Character rendering spike (working notes)

Goal and the three approaches are in [plan.md](plan.md), phase 2. This file
records what was learned and, at the end, the decision.

## What existing mods do

"Show Player In Menus" (source: github.com/derickso/ShowPlayerInMenus, MIT)
confirms the plan's guess for approach 1. It does **not** render a separate
character. It shows the real in-world player:

- switches the camera to the third-person state (`PlayerCamera::SetState`),
- turns on `ThirdPersonState::toggleAnimCam` and `freeRotationEnabled` and sets
  `freeRotation.x` to about pi so the camera sits in front of the player,
- fixes distance and offsets through the `fVanityModeMinDist/MaxDist` and
  `fOverShoulder*` INI settings and `posOffsetExpected`,
- zeroes the tween-menu blur so the character is not blurred,
- restores everything when the menu closes.

The world stays visible behind the character. No source was found for "Show
Player In Inventory" (Nexus 178689); its description says it also adjusts the
camera.

## Approach 1 — real player, camera moved

Implemented in `plugin/src/CameraStage.cpp`, entered on open and left on close.
Framing is tunable without rebuilding: edit
`Data/SKSE/Plugins/SkyrimModernUI.json` in the game folder and press F4 twice.

| Key | Meaning |
|---|---|
| `enabled` | turn the camera move off entirely |
| `hideWorld` | approach 2: cull everything in the scene except the player |
| `hideHud` | hide the vanilla HUD while the screen is open |
| `pauseGame` | `true` freezes the world and the pose; `false` keeps the character animated but the world runs |
| `distance` | camera distance, game units |
| `offsetX` / `offsetZ` | sideways / height shift of the camera |
| `rotation` | radians added to "directly in front" |
| `pitch` | radians, camera looking down |
| `fov` | 0 leaves the game's FOV |

Status: **works in game** (confirmed by the user 2026-11-16: the character shows through the stage).

Known or expected limitations (to confirm in game):

- World, sky and other actors remain visible behind the character.
- The vanilla HUD (compass, crosshair, bars) is still drawn.
- With `pauseGame: false` enemies can attack while the screen is open.
- Camera states other than first/third person (mount, furniture, VATS,
  dragon, bleedout) are skipped.
- The UI's transparent stage is not at the screen centre, so the character
  needs `offsetX` to sit in the middle of it.

## Approach 2 — hide the world

Implemented on top of approach 1 in the same file (`hideWorld`, `hideHud`).

- World: starting at the player's third-person 3D, walk up the scene graph to
  the root and set the app-culled flag on every sibling along the way (except
  the branch holding the world camera), plus the sky roots. Everything culled
  is remembered and un-culled on close.
- HUD: the HUD menu's movie is set invisible and restored on close.

Status: **works in game** (user screenshots 2026-11-16).


Findings from the first in-game look:

- Culling the scene graph removes terrain, statics, trees, water and grass. Only the character is drawn.
- The character stays lit (brighter and warmer than in the world, since nothing shadows it).
- The background is not black: with the sky gone the frame shows the weather's fog colour (beige at Riverwood).
- A few animals appeared in the background: things attached to the scene while the screen is open.

More from the same build (user, 2026-11-16): no crashes over repeated open/close; indoors it works too, with a
green background there (the interior's fog colour), which fits the fog-colour explanation.

Second round (user, 2026-11-16):

- Black fog did **not** give a black background: outdoors the colour only shifted from beige to brown, indoors
  it stayed green. So the clear colour is not simply the weather/cell fog colour. Parked at the user's request;
  the code is kept behind `blackFog` (default off).
- Repeating the cull pass every frame removed the stray animals, but NPCs and chickens stayed visible. Likely
  cause: the game resets the culled flag on actor root nodes each frame.
- F4 no longer opens the screen over a vanilla menu. No crashes.

Third round (user screenshot, 2026-11-16): culling the children of each actor's root works, NPCs and chickens
are gone. First crash seen: alt-tabbing away while the screen was open. No trace in any log; cause unknown
(candidates: Prisma UI's focus handling, our per-frame pass, Proton). CrashLoggerSSE is not installed.


Requirement clarified by the user: the background **must be black**; the "fog" in the design is a white fog
animation drawn by the UI over that black, not the game's fog.

Fourth round, deployed but **not yet seen in game** — three things together, each switchable in the JSON:

- The sky's root nodes are culled together with their children. The faint cloud-like texture in the third
  screenshot suggests the sky dome was still drawing, the same way actor roots were.
- `blackFog`: weather / cell fog colours set to black (as before).
- `blackClearColor`: the renderer's clear colour set to black every frame. Its memory offset is only documented
  up to game 1.6.1130+, so the plugin logs the value it reads and refuses to write unless it looks like a colour.

Fifth round (user's idea), deployed together with the fourth and also **not yet seen in game**: a black
backdrop. The game's own `meshes\markers\blackplane01.nif` (a 512-unit black square, lying flat, single-sided)
is loaded with `BSModelDB::Demand`, cloned twice back to back, stood upright and attached to the player's 3D
root, 150 units behind the character at scale 10. Nothing is created as a game object, so nothing reaches the
save. Tunable in the JSON: `backdrop`, `backdropDistance`, `backdropHeight`, `backdropScale`.

Result of rounds four and five (user screenshots, 2026-11-16):


- **Indoors: pitch black.** The backdrop works.
- **Outdoors:** black for about a frame, then dark brown, with the character's shadow visible on the backdrop
  and the character itself darker than before. So the plane reacts to sunlight, and black fog dims the character.
- The renderer's clear colour read as 0/0/0/0, so it is not where the background colour comes from; that
  experiment is removed.
- Alt-tab with the screen open did not crash this time. The earlier crash is unexplained and not reproduced.

Sixth round — **works** (user, 2026-11-16: "works perfectly", black outdoors too):



- `noFog`: outdoors the weather's fog is switched off while the screen is open (maximum opacity 0), instead of
  only being coloured black. Should stop both the backdrop tint and the dimming of the character.
- The backdrop's material has its specular response zeroed, so sunlight should no longer show on it.

## Approach 3 — clone in the interface 3D scene

Not needed; not started.

## Decision (2026-11-16)

**Approach 2, with a black backdrop.** The real player is shown in the world with the camera swung in front
(approach 1), everything else in the scene is hidden, and a black plane stands behind the character. The UI
draws its white fog animation over that black. The plan's rule was to stop at the first acceptable result;
this one matches the design, so approach 3 was not attempted.

What the plugin does while the screen is open (`plugin/src/CameraStage.cpp`), all undone on close:

1. Forces third person and puts the camera in front of the player at a fixed distance.
2. Culls every scene-graph branch that does not lead to the player, repeated every frame.
3. Culls the children of every other loaded actor's root node and of the sky's root nodes (the game un-culls
   those roots itself each frame).
4. Attaches two copies of the game's `markers\blackplane01.nif` behind the character, with specular zeroed.
5. Outdoors, switches the weather's fog off and sets its colours to black.
6. Hides the vanilla HUD.

Known limitations and loose ends:

- **The world keeps running** (`pauseGame: false`) so that the character stays animated. Enemies can attack
  while the screen is open. Pausing freezes the pose. Not decided which the mod should ship with.
- Only first- and third-person camera states are handled; on a mount, in furniture, in VATS, as a werewolf or
  dragon rider the camera move is skipped.
- Which of `blackFog` / `noFog` are both needed was not isolated; they are separate switches in the JSON.
- The backdrop's material is shared with the game's own black planes, which lose their specular for the rest
  of the session.
- The character is centred on the screen, not on the UI's stage, which sits left of centre. Use `offsetX`,
  or centre the stage in the phase 4 layout.
- One crash on alt-tab with the screen open, seen once, not reproduced, no log. CrashLoggerSSE is not installed.
- All of this was only run on game 1.7.104 under GE-Proton10-29, with no other mods. Camera mods (SmoothCam,
  True Directional Movement) and ENB are untested.
