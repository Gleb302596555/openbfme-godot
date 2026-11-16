#!/bin/bash
# Nightly maps (owner 2026-11-16): on today's main, the button matrix, the RotWK function map and their joins, pushed to the branch
# auto/maps for Claude to land (never into main from here), with a summary on the Drive (openbfme-runs/maps/<date>.md).
#
#   bash tools/colab/nightly_maps.sh          # wait for 01:00 UTC, run, repeat every night (autostart.d)
#   bash tools/colab/nightly_maps.sh --now    # one run now
# The result goes to the branch auto/maps; without push rights in the sandbox it waits on the Drive as openbfme-runs/maps/<date>.patch.
#
# Runs in Claude's station-3 tree of this sandbox (workspace/wt/local-sb<N>-st3, made from origin/main when missing). Inputs: the game files
# of /root/.agent_env, the Ennorath INI archive and the function inventory on the Drive (for-arena/, openbfme-audit/), the newest field audit
# there. angr is installed into /content/py219 on demand (only a changed game.dat needs it: the inventory is cached by its SHA256).
set -uo pipefail
[ -f /root/.agent_env ] && source /root/.agent_env
export RW_GAME_DAT="${RW_GAME_DAT:-${OPENBFME_GAME_DAT:-}}"
SB=$(cat /content/sandbox_id 2>/dev/null || echo 3)
REPO=/content/sandbox/openbfme-godot
T=${MAPS_TREE:-$REPO/workspace/wt/local-sb$SB-st3}
DRIVE=/content/drive/MyDrive
INI=$DRIVE/for-arena/ennorath-ini-1.9.8.3.zip
INV=$DRIVE/openbfme-audit/rw_function_blocks.json
RUNS=$DRIVE/openbfme-runs/maps
mkdir -p "$RUNS"
LOG=$RUNS/nightly_maps.log   # every log on the Drive: /content dies with the runtime (owner 2026-11-16)

run() {
  local day; day=$(date -u +%F)
  echo "== $day $(date -u +%T) start" >> "$LOG"
  [ -d "$T" ] || git -C "$REPO" worktree add -q --detach "$T" origin/main || return 1
  cd "$T" || return 1
  git fetch -q origin && git checkout -q -f --detach origin/main || return 1
  local rev; rev=$(git rev-parse --short=8 HEAD)
  bash tools/colab/build_tests.sh > "$RUNS/nightly_build.log" 2>&1 || { echo "build failed at $rev" >> "$LOG"; return 1; }
  python3 tools/matrix_run.py all --jobs 5 > "$RUNS/nightly_matrix.log" 2>&1 || { echo "matrix failed at $rev" >> "$LOG"; return 1; }
  [ -d /content/py219/angr ] || python3 -m pip install -q --target /content/py219 angr==9.2.213 bitstring==4.4.0 pycparser==3.0 > /dev/null 2>&1
  export PYTHONPATH=/content/py219
  cp -f "$INV" workspace/rebuild_inventory.json 2>/dev/null
  python3 tools/mod/rw_function_map.py --game "$RW_GAME_DAT" --ini "$INI" --inventory workspace/rebuild_inventory.json \
      --out docs/ennorath/RW_FUNCTION_MAP > "$RUNS/nightly_map.log" 2>&1 || { echo "map failed at $rev" >> "$LOG"; return 1; }
  cmp -s workspace/rebuild_inventory.json "$INV" || cp -f workspace/rebuild_inventory.json "$INV"   # a new game.dat: share the new cache
  python3 tools/mod/matrix_gaps.py --base "$ROTWK_INSTALL" --overlay "$OPENBFME_ENNORATH_INSTALL" --inventory workspace/rebuild_inventory.json \
      --game "$RW_GAME_DAT" > "$RUNS/nightly_gaps.log" 2>&1 || { echo "matrix_gaps failed at $rev" >> "$LOG"; return 1; }
  local audit; audit=$(ls -d "$DRIVE"/openbfme-audit/field-audit-v* 2>/dev/null | sort -V | tail -1)/field-audit.tsv
  python3 tools/mod/field_gaps.py --audit "$audit" --inventory workspace/rebuild_inventory.json --game "$RW_GAME_DAT" \
      > "$RUNS/nightly_fgaps.log" 2>&1 || { echo "field_gaps failed at $rev" >> "$LOG"; return 1; }
  rm -f workspace/rebuild_inventory.json
  git checkout -q -B auto/maps
  git add docs/ennorath/MATRIX.tsv docs/ennorath/README.md docs/ennorath/RW_FUNCTION_MAP.tsv docs/ennorath/RW_FUNCTION_MAP.md \
      docs/ennorath/MATRIX_GAPS.tsv docs/ennorath/MATRIX_GAPS.md docs/ennorath/FIELD_GAPS.tsv
  local red; red=$(python3 -c "import sys; sys.path.insert(0,'tools/mod'); import progress_snapshot as p; print(*p.matrix_counts(None))")
  if git diff --cached --quiet; then
    echo "$day: nothing changed at $rev" >> "$LOG"
  else
    git commit -q -m "Nightly maps $day on main $rev: matrix, function map, MATRIX_GAPS, FIELD_GAPS (red buttons / specific gaps: $red)" \
        -m "tools/colab/nightly_maps.sh (sandbox $SB); land with the next batch." || { echo "commit refused at $rev" >> "$LOG"; return 1; }
    # the sandbox shell may have no GitHub credentials: the commit then waits on the Drive as a patch (Claude applies it on the PC, 2026-11-16)
    git format-patch -1 --stdout HEAD > "$RUNS/$day.patch"
    git push -q -f origin auto/maps 2>/dev/null && rm -f "$RUNS/$day.patch" || echo "push failed at $rev: the commit is $RUNS/$day.patch" >> "$LOG"
  fi
  { echo "# Nightly maps $day (main $rev, sandbox $SB)"; echo; echo "red buttons / specific gaps: $red"; echo
    echo '## Function map'; tail -3 "$RUNS/nightly_map.log"; echo; echo '## MATRIX_GAPS'; tail -2 "$RUNS/nightly_gaps.log"
    echo; echo '## FIELD_GAPS'; tail -2 "$RUNS/nightly_fgaps.log"; echo; echo '## Matrix'; tail -5 "$RUNS/nightly_matrix.log"; } > "$RUNS/$day.md"
  echo "== $day $(date -u +%T) done ($red)" >> "$LOG"
}

if [ "${1:-}" = "--now" ]; then
  run
  exit $?
fi
while true; do
  now=$(date -u +%s); next=$(date -u -d "tomorrow 01:00" +%s)
  [ "$(date -u +%H)" -lt 1 ] && next=$(date -u -d "today 01:00" +%s)
  sleep $((next - now))
  run
done
