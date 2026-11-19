#!/usr/bin/env bash
# Tests the Lua exporter (Brainstorm/Core/gamedata.lua) without the game: a
# mock of the unmodded game (tools/mock_game.lua) is built from the native
# library's built-in data, the exporter reads it back through the mock's
# game functions, and the result must equal the built-in data. Run for a
# fresh run and for one taken mid-run.
#
#   tools/test-exporter.sh path/to/gamedata_tool [luajit] [library folder]
#
# With the folder that holds the built library (Immolate.so / .dll /
# .dylib), the exported text is also sent through the mod's native bridge
# (Brainstorm/Core/native.lua): a search, a seed check and the start-of-run
# prediction, as the game would.
#
# (ctest runs this when LuaJIT is installed.)
set -euo pipefail
tool="$1"
lua="${2:-luajit}"
libdir="${3:-}"
repo="$(cd "$(dirname "$0")/.." && pwd)"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
"$tool" dump > "$tmp/builtin.txt"
status=0
for mode in fresh midrun; do
  extra=()
  if [[ -n "$libdir" ]]; then extra=("$repo/Brainstorm/Core/native.lua" "$libdir"); fi
  "$lua" "$repo/tools/mock_game.lua" "$tmp/builtin.txt" "$repo/Brainstorm/Core/gamedata.lua" "$mode" "${extra[@]}" > "$tmp/$mode.txt"
  printf '%-7s ' "$mode"
  "$tool" check "$tmp/$mode.txt" || status=1
done
# A game with modded items: nothing to compare the text with; the Lua side
# asserts that a filter on the new items is accepted, found and predicted.
if [[ -n "$libdir" ]]; then
  if "$lua" "$repo/tools/mock_game.lua" "$tmp/builtin.txt" "$repo/Brainstorm/Core/gamedata.lua" modded \
      "$repo/Brainstorm/Core/native.lua" "$libdir" > "$tmp/modded.txt"; then
    echo "modded  PASS  modded items are searched through the game's lists"
  else
    echo "modded  FAIL"; status=1
  fi
fi
exit $status
