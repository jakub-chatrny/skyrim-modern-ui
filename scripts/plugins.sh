#!/usr/bin/env bash
# plugins.sh - switch the SKSE plugins of this project on or off, to compare
# against the plain game. Usage: scripts/plugins.sh on|off|status
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
require_game

dir="$SKYRIM_FOLDER/Data/SKSE/Plugins"
names=(PrismaUI.dll SkyrimModernUI.dll)

case "${1:-status}" in
    off) for n in "${names[@]}"; do [[ -f "$dir/$n" ]] && mv "$dir/$n" "$dir/$n.disabled"; done ;;
    on)  for n in "${names[@]}"; do [[ -f "$dir/$n.disabled" ]] && mv "$dir/$n.disabled" "$dir/$n"; done ;;
    status) ;;
    *) echo "Usage: $0 on|off|status" >&2; exit 1 ;;
esac
for n in "${names[@]}"; do
    if [[ -f "$dir/$n" ]]; then echo "on   $n"; elif [[ -f "$dir/$n.disabled" ]]; then echo "off  $n"; else echo "none $n"; fi
done
