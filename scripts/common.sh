# Sourced by the other scripts. Loads .env and resolves paths.
# shellcheck shell=bash

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [[ -f "$ROOT/.env" ]]; then
    # shellcheck source=/dev/null
    source "$ROOT/.env"
fi

SKYRIM_FOLDER="${SKYRIM_FOLDER:-}"
DEPLOY_DIR="${DEPLOY_DIR:-}"
SKYRIM_PREFIX="${SKYRIM_PREFIX:-}"

if [[ -z "$SKYRIM_PREFIX" && -n "$SKYRIM_FOLDER" ]]; then
    # <library>/steamapps/common/<game>  ->  <library>/steamapps/compatdata/489830/pfx
    SKYRIM_PREFIX="$(dirname "$(dirname "$SKYRIM_FOLDER")")/compatdata/489830/pfx"
fi

SKSE_LOG_DIR="$SKYRIM_PREFIX/drive_c/users/steamuser/Documents/My Games/Skyrim Special Edition/SKSE"

require_game() {
    if [[ -z "$SKYRIM_FOLDER" ]]; then
        echo "SKYRIM_FOLDER is not set. Copy .env.example to .env and fill it in." >&2
        exit 1
    fi
    if [[ ! -f "$SKYRIM_FOLDER/SkyrimSE.exe" ]]; then
        echo "No SkyrimSE.exe in: $SKYRIM_FOLDER (is the drive mounted?)" >&2
        exit 1
    fi
}
