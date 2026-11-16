# Phase 0 — Prisma UI under Proton (result record)

Fill this in when phase 0 is run. Steps are in [plan.md](plan.md), phase 0.
`scripts/phase0-check.sh` shows what is installed in the game folder.

| | |
|---|---|
| Date | 2026-11-15 |
| Game version | 1.7.104.0 (Steam build 24914197), fresh vanilla install at `/mnt/games/SteamLibrary` |
| SKSE64 build | 2.3.1 (`skse64_1_7_104.dll`), installed 2026-11-15; Address Library v13 (`versionlib-1-7-104-0.bin`) |
| Proton version | GE-Proton10-29 (prefix already created) |
| Prisma UI version | 1.5.1 + its example plugin, installed into `Data/` on 2026-11-15 (not yet run) |
| Launch option used | `bash -c 'exec "${@/SkyrimSELauncher.exe/skse64_loader.exe}"' -- %command%` (works) |

## What to install for 1.7.104

| What | Version | Source |
|---|---|---|
| SKSE64 | 2.3.1 | Nexus mod 30379 (login needed) |
| Address Library for SKSE Plugins (All in One) | v13 or newer, must contain `versionlib-1-7-104-0.bin` | Nexus mod 32444 (login needed) |
| Prisma UI | 1.5.1 | github.com/PrismaUI-SKSE/framework/releases |
| Prisma UI example plugin | 1.5.1 | same release, `PrismaUI-ExamplePlugin.zip` (F3 focuses the view) |
| Media Keys Fix SKSE | must support 1.7.104 — unverified | Nexus (login needed) |

## Result

- [x] A Prisma UI view renders in game (our own Character screen view, with live player data)
- [x] Mouse input works (button clicks reached the plugin as commands)
- [x] Keyboard input works (hotkey opens the view)
- [x] Open/close several times without getting stuck (plugin log shows clean F4 toggles; the blocked controls
      seen earlier were the game's gamepad mode, see notes)

Verdict: **works with caveats.** Prisma UI 1.5.1 runs under GE-Proton10-29 on Skyrim 1.7.104, provided the
prefix has a current VC++ x64 redistributable (`protontricks 489830 vcrun2022`). Without it Ultralight fails
to initialise and the game crashes. Our plugin links its CRT statically and does not need it; Ultralight does.

## Notes

### 2026-11-15, first run (GE-Proton10-29, launch option as above)

- `skse64_loader.log` / `skse64.log`: SKSE 2.3.1 starts and scans plugins. The launch option works.
- `PrismaUI.dll` "loaded correctly"; `PrismaUI.log` shows it running on 1.7.104 and all four Ultralight DLLs loaded.
- `PrismaUI-Example-Plugin.dll` (from the 1.5.1 release): "disabled, fatal error occurred while loading plugin" and the
  SKSE error dialog. It died inside its load function before creating its own log file. Cause not determined; no
  matching report in the Prisma issue trackers. Renamed to `.dll.disabled`.
- Not yet tested: whether a view actually renders and takes input (needs a working plugin that creates a view).


What was tried, log excerpts, anything Proton-specific.
- Own plugin built on Linux the same day (clang-cl 23 + xwin 0.10, vcpkg) and deployed with `scripts/deploy.sh`
  as the test view instead of the example. In game: F4 toggles it.
- Second run: our plugin failed exactly like the example ("fatal error occurred while loading plugin", no log).
  Cause: both import `MSVCP140.dll`, and the prefix only has the 2016 redistributable (14.0.24215) that the game
  installs. `PrismaUI.dll` has no such import (static CRT) and loads. Fix: the plugin now links the CRT
  statically (toolchain + vcpkg triplet). The stock example would need a current VC++ redistributable in the prefix.
- Third run (static-CRT plugin): our plugin "loaded correctly" and wrote its log — the CRT diagnosis was right.
  On `kDataLoaded` it requested the Prisma API; Prisma logged "Unknown exception during Ultralight
  Platform/Renderer init on UI thread" and "Core initialization failed: Renderer not created", then the game
  crashed. The four Ultralight DLLs in `Data/PrismaUI/libs` import `MSVCP140.dll` / `VCRUNTIME140*.dll`, so they
  hit the same 2016 runtime. Next step: install the current VC++ x64 redistributable into the prefix.
- Fourth run, after installing the VC++ redistributable in the prefix: view renders, shows live data, takes
  mouse clicks. Closing it left movement and menus blocked. Likely cause: the hotkey toggled more than once per
  press, and a close that follows an open too quickly leaves Prisma's FocusMenu (modal cursor menu) open.
  Plugin now toggles once per key press, with a 400 ms cooldown, and logs every open/close.
- Fifth run: the plugin log shows clean, single open/close pairs per F4 press, so toggling is fixed. The "cannot
  move or open menus" state exists right after `coc riverwood` loads, before the view is ever opened, with the
  Survival Mode popup on screen. Prisma's input capture is inactive at that point. Open question: plain-game
  behaviour of `coc` from the main menu (new-game state / the popup) versus something our plugins do. To separate
  them: load a real save, or run once with `scripts/plugins.sh off`.
- Esc handling removed from the UI on request; F4 is the only toggle.
- Resolved (found by the user): the blocked movement and menus were not caused by Prisma UI or the plugin. The
  game was in gamepad mode (`bGamepadEnable=1` in `SkyrimPrefs.ini`, a controller device visible to it), so
  keyboard gameplay controls were ignored; with the controller connected everything works, including a new game.
  For keyboard-only sessions set `bGamepadEnable=0`.
