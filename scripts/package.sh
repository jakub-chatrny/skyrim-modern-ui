#!/usr/bin/env bash
# package.sh - build the UI and the plugin and zip them as a mod that MO2 or
# Vortex can install, for testers who do not build anything themselves.
#
# Usage: scripts/package.sh [--dll <path>]
#   --dll <path>  use a prebuilt DLL (e.g. the GitHub Actions artifact)
#                 instead of cross-compiling
#
# Writes release/SkyrimModernUI-<version>.zip with the Data folder layout:
#   SKSE/Plugins/SkyrimModernUI.dll
#   SKSE/Plugins/SkyrimModernUI.json
#   PrismaUI/views/SkyrimModernUI/index.html (+ assets)
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

dll=""
while [[ $# -gt 0 ]]; do
    case "$1" in
        --dll) dll="$2"; shift ;;
        *) echo "Unknown argument: $1" >&2; exit 1 ;;
    esac
    shift
done

echo "==> Building UI"
(cd "$ROOT/ui" && { [[ -d node_modules ]] || npm ci; } && npm run build)

if [[ -z "$dll" ]]; then
    echo "==> Building plugin (clang-cl cross-compile)"
    log="$ROOT/plugin/build/build.log"
    mkdir -p "$ROOT/plugin/build"
    if ! (cd "$ROOT/plugin" && cmake --preset release-linux && cmake --build --preset release-linux) >"$log" 2>&1; then
        grep -n -A8 -E "error( |:)|FAILED" "$log" | head -n 80 >&2 || true
        echo "Plugin build failed. Full log: $log" >&2
        exit 1
    fi
    dll="$ROOT/plugin/build/release-linux/SkyrimModernUI.dll"
fi

# Version: the latest tag if there is one, else the short commit, marked when
# the working tree has changes that are not committed.
version="$(git -C "$ROOT" describe --tags --always --dirty 2>/dev/null || date +%Y%m%d)"

stage="$(mktemp -d)"
trap 'rm -rf "$stage"' EXIT
mkdir -p "$stage/SKSE/Plugins" "$stage/PrismaUI/views/SkyrimModernUI"
cp "$dll" "$stage/SKSE/Plugins/SkyrimModernUI.dll"
cp "$ROOT/plugin/config/SkyrimModernUI.json" "$stage/SKSE/Plugins/"
cp -r "$ROOT/ui/dist/." "$stage/PrismaUI/views/SkyrimModernUI/"

mkdir -p "$ROOT/release"
zip_path="$ROOT/release/SkyrimModernUI-$version.zip"
rm -f "$zip_path"
(cd "$stage" && zip -qr "$zip_path" .)
echo "==> $zip_path"
