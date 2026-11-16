#!/usr/bin/env bash
# phase0-check.sh - read-only check of what plan phase 0 needs in the game
# folder. Installs nothing. Usage: scripts/phase0-check.sh
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
require_game

data="$SKYRIM_FOLDER/Data"
plugins="$data/SKSE/Plugins"

check() { # label, glob...
    local label="$1" found=""
    shift
    for pattern in "$@"; do
        # shellcheck disable=SC2086
        found="$(compgen -G "$pattern" | head -n 3 || true)"
        [[ -n "$found" ]] && break
    done
    if [[ -n "$found" ]]; then
        printf '  ok       %s\n' "$label"
        while IFS= read -r line; do printf '           %s\n' "${line#"$SKYRIM_FOLDER"/}"; done <<<"$found"
    else
        printf '  MISSING  %s\n' "$label"
    fi
}

echo "Game folder: $SKYRIM_FOLDER"
echo "Prefix:      $SKYRIM_PREFIX"
echo
# The SKSE runtime DLL name carries the game version it was built for
# (skse64_1_5_97.dll = SE, skse64_1_6_*.dll = AE); it must match SkyrimSE.exe.
check "SKSE loader"                    "$SKYRIM_FOLDER/skse64_loader.exe"
check "SKSE runtime DLL"               "$SKYRIM_FOLDER/skse64_1_*.dll"
check "Address Library"                "$plugins/versionlib-*.bin" "$plugins/version-*.bin"
check "Prisma UI"                      "$plugins/PrismaUI.dll"
check "Prisma UI views folder"         "$data/PrismaUI/views/*"
check "Media Keys Fix (Prisma req.)"   "$plugins/*[Mm]edia*[Kk]ey*.dll"
check "CrashLoggerSSE (recommended)"   "$plugins/CrashLogger*.dll"
check "This mod's plugin"              "$plugins/SkyrimModernUI.dll"
check "This mod's view"                "$data/PrismaUI/views/SkyrimModernUI/index.html"
echo
if [[ -f "$SKYRIM_PREFIX/../version" ]]; then
    echo "Proton version of the prefix: $(cat "$SKYRIM_PREFIX/../version")"
fi
if [[ -d "$SKSE_LOG_DIR" ]]; then
    echo "SKSE logs: $SKSE_LOG_DIR  (scripts/logs.sh)"
else
    echo "No SKSE logs yet - the game has not been started through SKSE."
fi
