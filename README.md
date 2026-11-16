# Skyrim Modern UI

A Skyrim SE mod that replaces the character sheet with a modern web UI
(Prisma UI) fed by a small SKSE plugin. Full plan: [docs/plan.md](docs/plan.md).

## Status

| Phase | State |
|---|---|
| 0 — Prisma UI under Proton | **works with caveats** (needs a current VC++ redistributable in the prefix) — [docs/phase-0-prisma-proton.md](docs/phase-0-prisma-proton.md) |
| 1 — Build toolchain | Linux cross-compile works (clang-cl + xwin); self-built plugin loads and opens its view in game |
| 2 — Character rendering spike | **done**: real player with the world hidden and a black backdrop — [docs/phase-2-character-rendering.md](docs/phase-2-character-rendering.md) |
| 3 — Data bridge | schema + fixture + skeleton on both sides |
| 4 — Character screen UI | placeholder layout on mock data, builds |
| 5 — Outfits | not started |

Phases 0 to 2 are done; phase 3 (data bridge) is the next step.

## Layout

```
plugin/    C++ SKSE plugin (CommonLibSSE-NG, CMake presets, clang-cl cross-compile)
ui/        Svelte + Vite web app, builds to ui/dist
shared/    bridge JSON schema + fixture used by both sides
scripts/   deploy.sh, logs.sh, phase0-check.sh
docs/      plan, spike results, decisions
```

## Setup

```
git submodule update --init --recursive
cp .env.example .env        # then set SKYRIM_FOLDER
```

### UI

```
cd ui
npm install
npm run dev      # browser, mock bridge fed from shared/fixtures/character.json
npm run check    # types
npm run build    # ui/dist, one classic script (no ES modules) for Ultralight
```

Outside the game `window.smuiCommand` does not exist, so the bridge falls back
to the fixture and logs commands to the console.

### Plugin toolchain (phase 1, option A)

```
sudo pacman -S clang lld llvm cmake ninja zip
cargo install xwin
xwin --accept-license splat --output ~/.xwin   # accepts Microsoft's licence terms
cd plugin
./lib/vcpkg/bootstrap-vcpkg.sh -disableMetrics
cmake --preset release-linux
cmake --build --preset release-linux           # build/release-linux/SkyrimModernUI.dll
```

Fallback (option B): `.github/workflows/build.yml` builds the DLL on a Windows
runner; download the artifact and run `scripts/deploy.sh --dll <path>`.

### Deploy and debug

```
scripts/phase0-check.sh     # what is installed in the game folder (read-only)
scripts/deploy.sh           # build UI + plugin, copy into DEPLOY_DIR or Data/
scripts/deploy.sh --ui-only
scripts/logs.sh [-f]        # SKSE / Prisma / plugin logs from the Proton prefix
```

In game: **F4** toggles the screen (`TOGGLE_KEY` in
`plugin/src/CharacterMenu.cpp`). F4 is the only key that opens or closes it; Esc is not bound.

## Bridge

Defined in [shared/schema/bridge.schema.json](shared/schema/bridge.schema.json);
TypeScript mirror in `ui/src/bridge/types.ts`, C++ producer in
`plugin/src/Snapshot.cpp`.

- plugin → UI: `window.smuiReceive(json)` with a `Snapshot`
- UI → plugin: `window.smuiCommand(json)` with a `Command`
  (`ready`, `close`, `equip`, `unequip`, `switchOutfit`)

## Licences

- `plugin/CMakeLists.txt`, `plugin/CMakePresets.json` and `plugin/cmake/` are
  adapted from [codepuncher/CommonLibSSE-NG-template](https://github.com/codepuncher/CommonLibSSE-NG-template),
  which is GPL-3.0. Decide the project licence with that in mind, or rewrite
  those files.
- `plugin/src/PrismaUI_API.h` is the Prisma UI modder API header, copied as
  its own comment instructs. Prisma UI itself is a runtime dependency and is
  not redistributed.
