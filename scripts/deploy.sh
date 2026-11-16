#!/usr/bin/env bash
# deploy.sh - the edit -> build -> run loop: build the UI and the plugin, copy
# both into the game (or a mod manager folder).
#
# Usage: scripts/deploy.sh [--ui-only] [--dll <path>]
#   --ui-only     skip the plugin (UI changes need no rebuild of the DLL)
#   --dll <path>  use a prebuilt DLL (e.g. the GitHub Actions artifact)
#                 instead of cross-compiling
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

ui_only=false
dll=""
while [[ $# -gt 0 ]]; do
    case "$1" in
        --ui-only) ui_only=true ;;
        --dll) dll="$2"; shift ;;
        *) echo "Unknown argument: $1" >&2; exit 1 ;;
    esac
    shift
done

if [[ -z "$DEPLOY_DIR" ]]; then
    require_game
    DEPLOY_DIR="$SKYRIM_FOLDER/Data"
fi

# Overwriting a DLL the running game has mapped can corrupt that process.
if pgrep -x SkyrimSE.exe >/dev/null || ps -eo args | grep -q '[S]kyrimSE\.exe'; then
    echo "Skyrim is running. Close it first (the plugin is only loaded at startup anyway)." >&2
    exit 1
fi

echo "==> Building UI"
(cd "$ROOT/ui" && { [[ -d node_modules ]] || npm ci; } && npm run build)

if ! $ui_only && [[ -z "$dll" ]]; then
    echo "==> Building plugin (clang-cl cross-compile)"
    if [[ ! -d "$HOME/.xwin" ]]; then
        echo "~/.xwin is missing. See README.md, 'Plugin toolchain'." >&2
        exit 1
    fi
    (cd "$ROOT/plugin" && cmake --preset release-linux && cmake --build --preset release-linux)
    dll="$ROOT/plugin/build/release-linux/SkyrimModernUI.dll"
fi

echo "==> Deploying to $DEPLOY_DIR"
view_dir="$DEPLOY_DIR/PrismaUI/views/SkyrimModernUI"
mkdir -p "$view_dir" "$DEPLOY_DIR/SKSE/Plugins"
# Replace the old view: bundle file names carry a content hash.
find "$view_dir" -mindepth 1 -delete
cp -r "$ROOT/ui/dist/." "$view_dir/"
if [[ -n "$dll" ]]; then
    cp "$dll" "$DEPLOY_DIR/SKSE/Plugins/SkyrimModernUI.dll"
    [[ -f "${dll%.dll}.pdb" ]] && cp "${dll%.dll}.pdb" "$DEPLOY_DIR/SKSE/Plugins/"
fi

echo "Done. Start the game through SKSE; log: scripts/logs.sh"
