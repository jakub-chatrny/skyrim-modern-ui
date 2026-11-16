#!/usr/bin/env bash
# logs.sh - show the SKSE logs from the Proton prefix (there is no debugger).
# Usage: scripts/logs.sh [-f]     -f follows SkyrimModernUI.log
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

if [[ ! -d "$SKSE_LOG_DIR" ]]; then
    echo "No SKSE log folder at: $SKSE_LOG_DIR" >&2
    echo "Set SKYRIM_FOLDER or SKYRIM_PREFIX in .env, and run the game through SKSE once." >&2
    exit 1
fi

if [[ "${1:-}" == "-f" ]]; then
    exec tail -F "$SKSE_LOG_DIR/SkyrimModernUI.log"
fi

for log in skse64.log PrismaUI.log SkyrimModernUI.log; do
    echo "===== $log"
    if [[ -f "$SKSE_LOG_DIR/$log" ]]; then tail -n 40 "$SKSE_LOG_DIR/$log"; else echo "(missing)"; fi
done
echo "===== other logs in $SKSE_LOG_DIR"
ls -1 "$SKSE_LOG_DIR"
