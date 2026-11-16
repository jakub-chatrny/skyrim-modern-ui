# Skyrim modern UI mod — project plan

This file is self-contained. It can be followed by hand or handed to a coding
agent (e.g. Claude Code) on the Linux gaming machine: "read
skyrim-ui-mod-plan.md and start with phase 0".

Written 2026-11-15. Anything marked **UNVERIFIED** was not confirmed during
research and must be checked before relying on it.

## 1. Goal

A Skyrim Special Edition mod that replaces the character sheet (later also the
inventory) with a modern UI.

- Design: https://www.figma.com/design/XysgSJjTgysHlN2y6StFND/Skyrim-UI?node-id=18-12182
- First version = the **Character screen only**, opened with a hotkey,
  living alongside the vanilla menus.

What the Character screen contains (from the design):

- Top bar: character name, level + XP bar, tabs (Map / Quests / Character /
  Magic / Inventory / Skills), carry weight, gold.
- Outfit tabs (Outfit 1–4).
- Large animated 3D character in the centre, over a black background with fog.
- ~10 equipment slots left and right (head, body, hands, feet, left hand,
  right hand, mask, cloak, neck, ring, ranged, ammo) with item thumbnails and
  small stat lines.
- Stats panel: basic (health/stamina/magicka), survival (energy/hunger/
  thirst/warmth), resistances, damage statistics, skills.

## 2. Decisions already made

| Topic | Decision | Why |
|---|---|---|
| UI technology | **Prisma UI** (HTML/CSS/JS rendered in game via Ultralight/WebKit, DirectX 11 overlay) | Author is a web UI developer; ImGui was tried and was too painful |
| UI framework | React, Vue or Svelte — author's choice | Prisma UI supports all of them |
| Native side | Small C++ SKSE plugin on **CommonLibSSE-NG** | Only a data bridge: game state out as JSON, commands in |
| Item thumbnails | **Pre-rendered icons**, not live 3D | The engine previews one 3D item at a time; a grid of live 3D items is hard |
| Live 3D item preview | Optional, later, one selected item via the vanilla `Inventory3DManager` | Vanilla already does this |
| Scope | Character screen first; other tabs later | Replacing all six menus is too large |
| Survival stats | Optional | Hunger/thirst/warmth only exist with Survival Mode or a needs mod |

## 3. Environment

- Gaming + development machine: Linux (CachyOS, Arch-based), Skyrim SE on
  Steam via Proton. **No Windows machine is available.**
- A MacBook is available for UI work in a browser only (no game).
- Skyrim SE Steam app id: `489830`. Proton prefix:
  `~/.steam/steam/steamapps/compatdata/489830/pfx/`
- SKSE logs under Proton:
  `<prefix>/drive_c/users/steamuser/Documents/My Games/Skyrim Special Edition/SKSE/`

## 4. Known risks (ordered)

1. **Prisma UI under Proton — UNVERIFIED.** No reports found either way.
   Phase 0 tests this. If it fails, the fallback is Scaleform (Flash), which
   loses the web-UI advantage; reconsider the project at that point.
2. **Black background behind an animated character — UNVERIFIED.** Existing
   mods ("Show Player In Menus", "Show Player In Inventory") most likely show
   the real in-world player with the camera moved, with the world still
   visible behind. A true black/fog background needs extra work. Phase 2 is a
   spike for exactly this.
3. **Cross-compiling on Linux** is reported to work but is not officially
   supported by CommonLibSSE-NG. Fallback: build on a GitHub Actions Windows
   runner.
4. **No Visual Studio debugger.** Debug with log files (spdlog) and a crash
   logger mod (CrashLoggerSSE).
5. **Ultralight is not Chromium.** Some CSS/JS features may be missing; test
   in game early, not only in a desktop browser.

## 5. Phases

Each phase has a "done when". Do not start a phase before the previous one is
done — the early phases exist to kill the project cheaply if a risk is real.

### Phase 0 — Does Prisma UI run under Proton? (an evening)

1. Check the game version (SE 1.5.97 vs AE 1.6.x) and install the matching
   SKSE64 build into the game folder.
2. Make Steam start the SKSE loader. A commonly used launch option
   (**verify**):
   `bash -c 'exec "${@/SkyrimSELauncher.exe/skse64_loader.exe}"' -- %command%`
3. Install: Address Library for SKSE Plugins, Prisma UI and its listed
   requirements (the Nexus page lists "Media Keys Fix SKSE").
4. Install the prebuilt example from
   https://github.com/PrismaUI-SKSE/example-skse-plugin (or any released mod
   that uses Prisma UI).
5. Start the game, load a save, open the example view.

**Done when:** a Prisma UI view renders in game, accepts mouse/keyboard
input, and the game does not crash. Note the Proton version used.
**If it fails:** try Proton-GE and a newer/older Proton, read the SKSE and
Prisma logs, check the Prisma UI issue tracker. If still failing, stop and
re-evaluate.

### Phase 1 — Build toolchain (a weekend)

Goal: compile an SKSE plugin DLL without Windows and load it in game.

Option A — cross-compile on Linux (try first):

1. Install LLVM tooling: `clang` (provides `clang-cl`), `lld`, `llvm`,
   `cmake`, `ninja`, plus `xwin` (via `cargo install xwin` or a package).
2. Fetch the Windows SDK and MSVC CRT headers once:
   `xwin --accept-license splat --output ~/.xwin`
3. Start from a CommonLibSSE-NG template that has a Linux preset. One was
   found during research (**UNVERIFIED**, check it exists and is maintained):
   `codepuncher/CommonLibSSE-NG-template`, preset `release-linux`, toolchain
   file `cmake/toolchains/clang-cl-cross.cmake`.
4. Build, copy the DLL to `Data/SKSE/Plugins/`, confirm in the SKSE log that
   it loaded.

Option B — GitHub Actions (fallback, reliable):

1. Push the plugin repo to GitHub.
2. Workflow on `windows-latest`: checkout, vcpkg, CMake with the MSVC preset,
   upload the DLL as an artifact.
3. Download the artifact and copy it into the game.

Then: build the Prisma UI example plugin from source with whichever option
works, and change something visible to prove the loop.

**Done when:** a self-built plugin opens a self-modified Prisma view in game,
and the edit → build → run loop is written down as a script (`deploy.sh`).

### Phase 2 — Character rendering spike (the risky part, time-box 1–2 weeks)

Goal: animated player character visible inside the custom screen. Try in
this order and stop at the first acceptable result:

1. **Real player, camera moved.** On menu open: force third person, place the
   camera in front of the player, freeze/pause the game, let the UI have a
   transparent area in the middle. World stays visible (dim/blur it).
   Proven possible by existing mods; not a true black background.
2. **Same, with the world hidden.** Hide everything except the player from
   rendering (cull the world/sky/other actors, or mask by depth), and let the
   UI draw the black + fog background around the character. **UNVERIFIED.**
3. **Clone in the UI 3D scene.** Clone the player's 3D into the engine's
   interface 3D scene (the mechanism behind the loading-screen model and
   `Inventory3DManager`). Static pose is plausible; animation is uncertain.
   **UNVERIFIED.**

Study before coding: source of "Show Player In Inventory" (its Nexus page
says source is on GitHub), "Show Player In Menus", and CommonLibSSE-NG
headers for `Inventory3DManager`, `PlayerCamera`, `UI`, `IMenu`.

Note on draw order: Prisma UI is an overlay drawn on top of everything, so
the character must show through a transparent region of the page.

**Done when:** a decision is recorded — which approach is used, a
screenshot, and known limitations. If only approach 1 works, decide whether a
dimmed/blurred world instead of black fog is acceptable.

### Phase 3 — Data bridge

Plugin → UI (JSON, pushed on open and on change):

- Player: name, level, XP progress, gold, carry weight current/max.
- Health / stamina / magicka current and max.
- Resistances (armor rating, magic, fire, frost, shock, poison, disease).
- Damage stats (melee/ranged damage and speed, magic power).
- Skills with levels.
- Equipped item per slot: form id, name, type, key stats, icon key.

UI → plugin (commands): equip, unequip, close menu, later switch outfit.

Reference for stat calculation: the open-source "Skyrim Character Sheet"
mod by mlthelama.

Define the JSON schema in one shared file and generate/hand-write matching
TypeScript types.

**Done when:** opening the screen in game logs a full, correct JSON snapshot
in the UI, and an equip command from the UI changes equipment in game.

### Phase 4 — Character screen UI

1. Web project (Vite + chosen framework) that builds into the Prisma views
   folder.
2. **Mock bridge**: the same API as the real bridge but fed from a JSON
   fixture, so the whole screen can be developed in a normal browser (also on
   the Mac).
3. Implement the Figma design: top bar, slots, stats panel, fog background.
4. Item icons: start with a generic icon per item type; a per-item
   pre-rendered icon pipeline is a later improvement.
5. Keyboard and gamepad navigation (focus management, LB/RB for tabs).
6. Open/close with a hotkey; pause the game while open.
7. Test in game regularly because of Ultralight differences.

**Done when:** the Character screen matches the design in game at 1080p and
1440p, usable with mouse, keyboard and gamepad.

### Phase 5 — Outfits

Saved equipment sets (Outfit 1–4): store form ids per slot in the SKSE
co-save, switch with one command, handle missing items gracefully.

**Done when:** outfits survive save/load and switching equips the full set.

### Phase 6 — Later

- Inventory screen (item lists, sorting, drop/favourite, containers).
- One live 3D item preview for the selected item.
- Decide whether to replace vanilla menus or stay alongside them.
- Compatibility with other mods that hook the vanilla inventory menu.
- Packaging and release on Nexus (respect the Prisma UI licence: use it as a
  dependency, do not redistribute a modified framework).

## 6. Suggested repository layout

```
skyrim-modern-ui/
  plugin/          C++ SKSE plugin (CommonLibSSE-NG, CMake presets)
  ui/              web app (Vite), builds to dist/
  shared/          JSON schema + fixtures used by both sides
  scripts/         deploy.sh (build + copy into the game's Data folder)
  .github/workflows/build.yml   Windows CI build (fallback / releases)
  docs/            spike results, decisions
```

## 7. References

- Prisma UI docs: https://www.prismaui.dev
- Prisma UI framework: https://github.com/PrismaUI-SKSE/framework
- Prisma UI example plugin: https://github.com/PrismaUI-SKSE/example-skse-plugin
- Prisma UI on Nexus: https://www.nexusmods.com/skyrimspecialedition/mods/148718
- CommonLibSSE-NG: https://github.com/alandtse/CommonLibSSE-NG
- Cross-compile write-up (clang-cl + xwin): https://halgari.github.io/mora/cross-compile-windows/
- Show Player In Menus: https://www.nexusmods.com/skyrimspecialedition/mods/81291
- Show Player In Inventory: https://www.nexusmods.com/skyrimspecialedition/mods/178689
- CrashLoggerSSE: https://github.com/alandtse/CrashLoggerSSE
