#!/bin/bash
# Maps (owner 2026-11-16; 2026-11-17: one run of all three tools on one main, after every landing; 2026-11-23: "все карты и свипы и аудиты
# крутил один скрипт на 3 песочнице"): the script map, the button matrix and its grey explanation (MATRIX_GREY), the field audit, the map
# sweep (every Ennorath map starts and plays, MAP_SWEEP), the RotWK function map, the whole-image map (RW_BINARY_MAP), the modifier map and their joins (MATRIX_GAPS,
# FIELD_GAPS), pushed to the branch auto/maps; GitHub Actions checks the commit and lands it on main (.github/workflows/maps-land.yml, owner
# 2026-11-20; never into main from here), with one summary on the Drive (openbfme-runs/maps/<date>.md, and latest.md: the PR checker and the agents read it).
#
#   bash tools/colab/nightly_maps.sh          # every 15 min: when origin/main moved since the last run and has been still for 15 min, run
#   bash tools/colab/nightly_maps.sh --now    # one run now
#   bash tools/colab/nightly_maps.sh --full   # one run of every step (a run otherwise does only the steps whose inputs changed, see want())
#   bash tools/colab/nightly_maps.sh --only binary   # only the named steps: script matrix audit sweep funcmap gaps binary drafts
# The result goes to the branch auto/maps; without push rights in the sandbox it waits on the Drive as openbfme-runs/maps/<date>.patch.
#
# Runs in Claude's station-3 tree of this sandbox (workspace/wt/local-sb<N>-st3, made from origin/main when missing). Inputs: the game files
# of /root/.agent_env, the Ennorath INI archive and the function inventory on the Drive (for-arena/, openbfme-audit/), the newest field audit
# there. angr is installed into /content/py219 on demand (only a changed game.dat needs it: the inventory is cached by its SHA256).
# The map's decomp column (2026-11-17, Port 240) also needs BFME2's game.dat ($BFME2_INSTALL) and the Open-BFME-2 clone ($OPENBFME_DECOMP2,
# default /content/ref/open-bfme-2, cloned or pulled here each night).
set -uo pipefail
[ -f /root/.agent_env ] && source /root/.agent_env
export RW_GAME_DAT="${RW_GAME_DAT:-${OPENBFME_GAME_DAT:-}}"
SB=$(cat /content/sandbox_id 2>/dev/null || echo 3)
REPO=/content/sandbox/openbfme-godot
# its own tree (2026-11-17): a run after a landing comes in the day, while Claude works in station 3
T=${MAPS_TREE:-$REPO/workspace/wt/maps}
DRIVE=/content/drive/MyDrive
INI=$DRIVE/for-arena/ennorath-ini-1.9.8.7.zip   # the mod's INI view: Ennorath 1.9.8.7 since 2026-11-23 (1.9.8.3 before)
INV=$DRIVE/openbfme-audit/rw_function_blocks.json
INV_FULL=$DRIVE/openbfme-audit/rw_function_blocks_full.json   # the whole-image inventory (Port 241): more readers for FIELD_GAPS
AUDIT=$DRIVE/openbfme-audit/field-audit-auto                  # this run's field audit (the numbered vN stay hand releases)
RUNS=$DRIVE/openbfme-runs/maps
mkdir -p "$RUNS"
# Self-update (owner 2026-11-17): between iterations the loop compares its installed copy with origin/main's and restarts on the new one
# (after `bash -n`), so a landed change to the loop needs no hand swap. Only an installed copy outside a git tree updates itself (a run from
# a tree is that tree's version); SELF_UPDATE=0 keeps the running copy (a branch version under test).
self_update() {
  [ "${SELF_UPDATE:-1}" = 1 ] || return 0
  git -C "$(dirname "$0")" rev-parse --git-dir >/dev/null 2>&1 && return 0
  local new="$0.new"
  { git -C "$REPO" show "origin/main:tools/colab/$1" > "$new" 2>/dev/null && [ -s "$new" ]; } || { rm -f "$new"; return 0; }
  if cmp -s "$new" "$0"; then rm -f "$new"; return 0; fi
  bash -n "$new" || { echo "self-update: origin/main's $1 does not parse, keeping the running copy"; rm -f "$new"; return 0; }
  echo "== $(date -u '+%F %T') UTC self-update: restarting on origin/main's $1"
  mv -f "$new" "$0" && exec bash "$0"
}
LOG=$RUNS/nightly_maps.log   # every log on the Drive: /content dies with the runtime (owner 2026-11-16)

# The RotWK -> BFME2 pairing (tools/re/README.md, Port 240: no Ghidra): RotWK entries from the angr inventory, BFME2's from the decomp's own
# list, then match_rotwk.py; rw_function_map.py reads it as the map's decomp column (the BFME2 C++ a lane reads instead of disassembly).
# Generated, never committed: it stays in the tree's ignored workspace/rebuild/xmap (match_rotwk's feature cache in work/ makes reruns fast).
DECOMP2=${OPENBFME_DECOMP2:-/content/ref/open-bfme-2}
decomp_map() {
  XMAP=$PWD/workspace/rebuild/xmap; mkdir -p "$XMAP"; rm -f "$XMAP/rw_map.csv"
  local log=$RUNS/nightly_decomp.log
  { [ -d "$DECOMP2/.git" ] && git -C "$DECOMP2" pull -q --ff-only || git clone -q --depth 1 https://github.com/Open-BFME/Open-BFME-2 "$DECOMP2"; } > "$log" 2>&1
  python3 tools/re/inventory.py --bin "$RW_GAME_DAT" --angr workspace/rebuild_inventory.json --out "$XMAP/rotwk_functions.csv" >> "$log" 2>&1 &&
  python3 tools/re/inventory.py --bin "$BFME2_INSTALL/game.dat" --csv "$DECOMP2/reverse/ghidra_functions.csv" --out "$XMAP/bfme2_functions.csv" >> "$log" 2>&1 &&
  python3 tools/re/ledger_lanes.py --clone "$DECOMP2" --out "$XMAP/bfme2_ledger.csv" >> "$log" 2>&1 &&
  python3 tools/re/match_rotwk.py --rotwk "$RW_GAME_DAT" --rotwk-inv "$XMAP/rotwk_functions.csv" --bfme2 "$BFME2_INSTALL/game.dat" \
      --bfme2-inv "$XMAP/bfme2_functions.csv" --bfme2-ledger "$XMAP/bfme2_ledger.csv" --decomp "$DECOMP2" --work "$XMAP/work" \
      --out-csv "$XMAP/rw_map.csv" --out-json "$XMAP/rw_summary.json" >> "$log" 2>&1 ||
    { rm -f "$XMAP/rw_map.csv"; echo "decomp map failed (column left empty): $log" >> "$LOG"; }
}

# The whole-image map (owner 2026-11-23, tools/rw_object_model/): every function of game.dat with its class, slot, callers, BFME2 pair
# and port status -> docs/ennorath/RW_BINARY_MAP.*, RW_CLASSES.tsv, RW_VCALLS.tsv. The inventory and the per-function facts take hours:
# they are cached on the Drive keyed by game.dat's SHA256 and the two tools' blob ids, so a night reruns only the BFME2 pairing over the
# inventory (minutes) and the report (port status from this main's `RW 0x...` citations); a changed tool rebuilds the cache once.
BINMAP=$DRIVE/openbfme-audit/binary-map
binary_map() {
  local key log=$RUNS/nightly_binmap.log
  key="$(sha256sum "$RW_GAME_DAT" | cut -c1-16) $(git rev-parse HEAD:tools/rw_object_model/binary_map.py HEAD:tools/rw_object_model/class_map.py | tr '\n' ' ')"
  mkdir -p workspace/rebuild "$BINMAP"
  if [ "$(cat "$BINMAP/key" 2>/dev/null)" = "$key" ]; then
    cp -f "$BINMAP/rw_binary_functions.json" "$BINMAP/rw_binary_facts.pickle" workspace/rebuild/ || return 1
    echo "cached inventory and facts ($key)" > "$log"
  else
    echo "rebuilding the inventory and facts ($key)" > "$log"
    { python3 tools/rw_object_model/binary_map.py functions && python3 tools/rw_object_model/class_map.py facts; } >> "$log" 2>&1 || return 1
    cp -f workspace/rebuild/rw_binary_functions.json workspace/rebuild/rw_binary_facts.pickle "$BINMAP/" && echo "$key" > "$BINMAP/key"
  fi
  local xf=$PWD/workspace/rebuild/xmap-full; mkdir -p "$xf"; rm -f "$xf/rw_map.csv"
  { python3 tools/rw_object_model/binary_map.py export-blocks --out workspace/rebuild/rw_binary_blocks.json &&
    python3 tools/re/inventory.py --bin "$RW_GAME_DAT" --angr workspace/rebuild/rw_binary_blocks.json --out "$xf/rotwk_functions.csv"; } >> "$log" 2>&1 || return 1
  cp -f workspace/rebuild/rw_binary_blocks.json "$DRIVE/openbfme-audit/rw_binary_blocks.json"   # the dossier loop's exact bounds (autostart.d/dossier.sh)
  # BFME2's side is decomp_map's (the same clone, the same night)
  if [ -f "$XMAP/bfme2_functions.csv" ] && [ -f "$XMAP/bfme2_ledger.csv" ]; then
    python3 tools/re/match_rotwk.py --rotwk "$RW_GAME_DAT" --rotwk-inv "$xf/rotwk_functions.csv" --bfme2 "$BFME2_INSTALL/game.dat" \
        --bfme2-inv "$XMAP/bfme2_functions.csv" --bfme2-ledger "$XMAP/bfme2_ledger.csv" --decomp "$DECOMP2" --work "$xf/work" \
        --out-csv "$xf/rw_map.csv" --out-json "$xf/rw_summary.json" >> "$log" 2>&1 || rm -f "$xf/rw_map.csv"
  fi
  # no pairing, no report (2026-11-24): a table without the subsystem column would zero the dashboard's binary counts; main's tables stay
  [ -f "$xf/rw_map.csv" ] || { echo "no BFME2 pairing ($xf/rw_map.csv): the tables are left as main has them" >> "$log"; return 1; }
  python3 tools/rw_object_model/class_map.py report "--xmap=$xf/rw_map.csv" >> "$log" 2>&1
}

# The arena's audit archive (owner 2026-11-17: "сам заменял на гугл диске ennorath-audit.zip новым"): this run's audit + the newest hand
# verdicts + rotwk-triage/ (unchanged, taken from the current archive) -> Drive for-arena/ennorath-audit.zip, the previous one kept as
# ennorath-audit-prev.zip; only when the audit table changed. The arena gets it through the encrypted refs/arena/files, which Claude
# re-encrypts on the PC (DashBoard scripts/arena_refresh.sh, run by tools/land.py): the key never comes to a sandbox.
audit_zip() {
  local rev=$1 manual=$2
  python3 - "$DRIVE/for-arena" "$AUDIT" "$manual" "$rev" <<'PY' >> "$LOG" 2>&1
import io, re, shutil, sys, zipfile
from datetime import date
from pathlib import Path
folder, audit, manual, rev = Path(sys.argv[1]), Path(sys.argv[2]), Path(sys.argv[3]), sys.argv[4]
cur = folder / "ennorath-audit.zip"
old = zipfile.ZipFile(cur) if cur.exists() else None
new_tsv = (audit / "field-audit.tsv").read_bytes()
if old and any(n.endswith("/field-audit.tsv") and old.read(n) == new_tsv for n in old.namelist()):
    print("audit zip: unchanged"); sys.exit(0)
totals = next((l.strip() for l in (audit / "field-audit.md").read_text(encoding="utf-8").splitlines() if "fields of the mod" in l), "")
readme = f"""ennorath-audit.zip, {date.today()} (main {rev}; rebuilt by tools/colab/nightly_maps.sh after each landing)

field-audit/  the mod field audit (tools/mod/field_audit.py on main {rev}; the mod = Ennorath 1.9.8.3 over RotWK 2.01 vanilla).
  field-audit.tsv      one row per field the mod writes: status in the PORT (used, parsed-unused, not-read, raw, missing-module, ...),
                       where, values; the tool column is the tool's own status
  field-audit.md       the same as a report, plus the hand checks to look at again
  field-audit-diff.md  the fields whose status changed against the previous run
  module-coverage.tsv  the engine's module table (ported / parsed-only / missing)
  module-queue.tsv     classes ordered by the number of mod objects
  manual-check.tsv     the hand-checked verdicts every run applies with --manual
  {totals}

script-map/SCRIPT_MAP.tsv  every script of the sides' script libraries (Libraries\<name>\<name>.map) and .scb files of the mod's mount:
                       conditions, actions with parameters, the condition / action types the port has no case for (tools/mod/script_map.py;
                       the repository has its names-only half: docs/ennorath/SCRIPT_GRANTS.tsv, SCRIPT_MAP.md)

rotwk-triage/  what RotWK 2.01 itself does with 741 of those fields (cloud-agent triage, all 25 batches), reviewed; unchanged since 2026-11-10.
"""
buf = io.BytesIO()
with zipfile.ZipFile(buf, "w", zipfile.ZIP_DEFLATED) as z:
    z.writestr("README.txt", readme)
    for f in ("field-audit.tsv", "field-audit.md", "field-audit-diff.md", "module-coverage.tsv", "module-queue.tsv"):
        if (audit / f).exists():
            z.write(audit / f, f"field-audit/{f}")
    z.write(manual, "field-audit/manual-check.tsv")
    if (audit / "script-map" / "SCRIPT_MAP.tsv").exists():
        z.write(audit / "script-map" / "SCRIPT_MAP.tsv", "script-map/SCRIPT_MAP.tsv")
    for n in (old.namelist() if old else []):
        if n.startswith("rotwk-triage/"):
            z.writestr(n, old.read(n))
if old:
    old.close()
    shutil.copyfile(cur, folder / "ennorath-audit-prev.zip")
tmp = folder / "ennorath-audit.zip.tmp"
tmp.write_bytes(buf.getvalue())
tmp.replace(cur)
print(f"audit zip: replaced ({len(buf.getvalue())} bytes, main {rev})")
PY
}

st() { mkdir -p /content/status && echo "$*" > /content/status/maps.txt; }   # the dashboard's "what it does" line (watchdog.py)

# Only what changed (owner 2026-11-24: "чтобы прогон гонял только то что обновилось а не все"): a step runs when its inputs moved since the
# main of the previous run ($RUNS/last_rev): its own tools, engine/ for what runs the engine (matrix, audit, sweep), engine/src's citations
# for the function maps. The inputs outside git (game.dat, BFME2's game.dat, the mod's INI archive, the newest hand verdicts) are one key
# ($RUNS/inputs_key): a new key runs everything. A skipped step leaves main's table as it is (the commit then has no change for it).
#   --full           every step
#   --only "<steps>" only these steps (names below), e.g. --only binary; last_rev is then left alone, so the next run still sees every change
ONLY=""; FULL=0
STEPS_ALL="script matrix audit sweep funcmap gaps binary drafts"
want() {   # want <step> <paths...>
  local step=$1; shift
  if [ -n "$ONLY" ]; then [[ " $ONLY " == *" $step "* ]]; return; fi
  [ "$FULL" = 1 ] && return 0
  [ -n "$BASE" ] && git cat-file -e "$BASE^{commit}" 2>/dev/null || return 0
  ! git diff --quiet "$BASE" HEAD -- "$@"
}
RAN=""; SKIPPED=""
note_step() { if [ "$2" = 1 ]; then RAN="$RAN $1"; else SKIPPED="$SKIPPED $1"; fi; }

run() {
  local day; day=$(date -u +%F)
  echo "== $day $(date -u +%T) start${ONLY:+ (only: $ONLY)}" >> "$LOG"
  [ -d "$T" ] || git -C "$REPO" worktree add -q --detach "$T" origin/main || return 1
  cd "$T" || return 1
  git fetch -q origin && git checkout -q -f --detach origin/main || return 1
  local rev; rev=$(git rev-parse --short=8 HEAD)
  XMAP=$PWD/workspace/rebuild/xmap
  BASE=$(cat "$RUNS/last_rev" 2>/dev/null)
  local manual; manual=$(ls -d "$DRIVE"/openbfme-audit/field-audit-v* 2>/dev/null | sort -V | tail -1)/manual-check.tsv
  local key; key="$(cat "$RW_GAME_DAT" "$BFME2_INSTALL/game.dat" "$INI" "$manual" 2>/dev/null | md5sum | cut -c1-16)"
  if [ -z "$ONLY" ] && [ "$key" != "$(cat "$RUNS/inputs_key" 2>/dev/null)" ]; then
    echo "inputs outside git changed (game.dat / INI archive / hand verdicts): every step" >> "$LOG"; FULL=1
  fi
  RAN=""; SKIPPED=""
  local s_script=0 s_matrix=0 s_audit=0 s_sweep=0 s_funcmap=0 s_gaps=0 s_binary=0 s_drafts=0
  want script  tools/mod/script_map.py && s_script=1
  want matrix  engine tools/matrix_run.py 'tools/mod/matrix_*' && s_matrix=1
  want audit   engine tools/mod/field_audit.py && s_audit=1
  want sweep   engine tools/maps && s_sweep=1
  want funcmap engine/src tools/mod/rw_function_map.py tools/re && s_funcmap=1
  want binary  engine/src tools/rw_object_model tools/mod/rw_function_map.py 'tools/mod/rw_library_functions*' docs/ennorath/rw_verdicts.d tools/re && s_binary=1
  # the joins follow their inputs: the matrix, the audit, the function map
  { want gaps tools/mod/matrix_gaps.py tools/mod/field_gaps.py tools/mod/modifier_map.py || { [ -z "$ONLY" ] && [ $((s_matrix + s_audit + s_funcmap)) -gt 0 ]; }; } && s_gaps=1
  { want drafts tools/mod/matrix_drafts.py || { [ -z "$ONLY" ] && [ "$s_matrix" = 1 ]; }; } && s_drafts=1
  for s in $STEPS_ALL; do v="s_$s"; note_step "$s" "${!v}"; done
  echo "steps: ran${RAN:- none}; skipped${SKIPPED:- none} (since ${BASE:-no previous run})" >> "$LOG"
  if [ $((s_matrix + s_audit + s_sweep)) -gt 0 ]; then
    st "карты на main $rev: сборка"
    OPENBFME_BUILD_TARGETS=openbfme_tests bash tools/colab/build_tests.sh > "$RUNS/nightly_build.log" 2>&1   # the tests only: no GDExtension here || { echo "build failed at $rev" >> "$LOG"; return 1; }
  fi
  mkdir -p "$AUDIT"
  # the script map (owner 2026-11-19: mechanics the sides' script libraries give, like the Manwe science): before the matrix (matrix_run.py collect
  # notes the script on a row naming what a script gives); its names-only half goes into the
  # repository with the maps, the full transcription into the audit folder and its zip
  if [ "$s_script" = 1 ]; then
  st "карты на main $rev: карта скриптов"
  python3 tools/mod/script_map.py "$ROTWK_INSTALL" "$OPENBFME_ENNORATH_INSTALL" --out docs/ennorath --full "$AUDIT/script-map"       > "$RUNS/nightly_scripts.log" 2>&1 || echo "script map failed at $rev (the run goes on)" >> "$LOG"
  fi
  if [ "$s_matrix" = 1 ]; then
  st "карты на main $rev: матрица кнопок (все фракции)"
  python3 tools/matrix_run.py all --jobs 5 > "$RUNS/nightly_matrix.log" 2>&1 || { echo "matrix failed at $rev" >> "$LOG"; return 1; }
  # the matrix review (owner 2026-11-17): layer 1 ran inside `all` (recheck: every red object swept again alone, ORDER when fine alone);
  # layers 2 and 3 here: the lint against the mod's INI, and every button whose verdict moved since the main the previous run measured
  python3 tools/mod/matrix_review.py --ini "$INI" --before "$(cat "$RUNS/last_rev" 2>/dev/null)" > "$RUNS/nightly_review.log" 2>&1       || echo "matrix review failed at $rev (the run goes on)" >> "$LOG"
  # why the DISABLED buttons are grey (MATRIX_GREY, Port 220), from this run's matrix: the next run's sweep reads it (2026-11-23: it was
  # last made by hand on 2026-11-18)
  st "карты на main $rev: серые кнопки (MATRIX_GREY)"
  python3 tools/mod/matrix_grey.py --ini "$INI" --matrix docs/ennorath/MATRIX.tsv --out docs/ennorath/MATRIX_GREY.tsv \
      --md docs/ennorath/MATRIX_GREY.md > "$RUNS/nightly_grey.log" 2>&1 || echo "matrix grey failed at $rev (the run goes on)" >> "$LOG"
  fi
  # the field audit on this main (2026-11-17): the engine's module coverage table, then field_audit.py with the newest hand verdicts
  if [ "$s_audit" = 1 ]; then
  mkdir -p "$AUDIT"
  st "карты на main $rev: аудит полей"
  OPENBFME_MODULE_COVERAGE_OUT="$AUDIT/module-coverage.tsv" engine/build/openbfme_tests "-tc=module coverage: every registry class*" \
      > "$RUNS/nightly_audit.log" 2>&1
  python3 tools/mod/field_audit.py --base "$ROTWK_INSTALL" --overlay "$OPENBFME_ENNORATH_INSTALL" --coverage "$AUDIT/module-coverage.tsv" \
      --manual "$manual" --out "$AUDIT" >> "$RUNS/nightly_audit.log" 2>&1 || { echo "field audit failed at $rev" >> "$LOG"; return 1; }
  # field-audit-auto is the one place every reader takes the audit from (owner 2026-11-18: "чтобы ночной сам везде обновлял"): the hand
  # verdicts this run applied sit next to it, and README.txt says which main and which vN they are; the numbered vN stay hand releases
  # (Claude adds new verdicts there), so no prompt or document names a version or a date that goes stale
  cp -f "$manual" "$AUDIT/manual-check.tsv"
  printf 'field-audit-auto: main %s, %s UTC (tools/colab/nightly_maps.sh, sandbox %s); hand verdicts from %s
%s
' "$rev" "$(date -u '+%F %T')" "$SB"       "$(basename "$(dirname "$manual")")" "$(grep -m1 'fields of the mod' "$AUDIT/field-audit.md" 2>/dev/null)" > "$AUDIT/README.txt"
  audit_zip "$rev" "$manual" || echo "audit zip failed at $rev" >> "$LOG"   # never stops the run
  fi
  # the map sweep (owner 2026-11-23: last run by hand on 2026-11-20, and two Local tasks were written from it after their fixes had
  # landed): every map of the Ennorath install starts a skirmish and plays 60 s, one process per map; the per-map logs stay on the Drive
  if [ "$s_sweep" = 1 ]; then
  st "карты на main $rev: свип карт Эннората"
  local sw="$RUNS/map-sweep"; rm -rf "$sw"; mkdir -p "$sw"
  local jobs; jobs=$(( $(nproc) > 4 ? $(nproc) - 2 : 2 ))
  if python3 tools/maps/map_sweep.py --tests engine/build/openbfme_tests --out "$sw" --jobs "$jobs" --seconds 60 --timeout 600 \
        > "$RUNS/nightly_sweep.log" 2>&1; then
    python3 tools/maps/map_sweep.py --summary "$sw/map-sweep.tsv" --docs docs/ennorath >> "$RUNS/nightly_sweep.log" 2>&1 \
        || echo "map sweep summary failed at $rev (the run goes on)" >> "$LOG"
  else
    echo "map sweep failed at $rev (the run goes on): $RUNS/nightly_sweep.log" >> "$LOG"
  fi
  fi
  [ -d /content/py219/angr ] || python3 -m pip install -q --target /content/py219 angr==9.2.213 bitstring==4.4.0 pycparser==3.0 > /dev/null 2>&1
  export PYTHONPATH=/content/py219
  mkdir -p workspace/rebuild   # a fresh tree (MAPS_TREE) has no workspace/: without it the copy failed silently and the pairing had no inventory
  cp -f "$INV" workspace/rebuild_inventory.json || echo "no function inventory on the Drive ($INV)" >> "$LOG"
  # the decomp pairing feeds both function maps: rerun it with the function map, or when the whole-image map runs without its BFME2 files
  if [ "$s_funcmap" = 1 ] || { [ "$s_binary" = 1 ] && [ ! -f "$XMAP/bfme2_functions.csv" ]; }; then
    st "карты на main $rev: карта функций RotWK"
    decomp_map   # fills the map's decomp column (Port 240); a failure leaves the column empty, it never stops the night
  fi
  if [ "$s_binary" = 1 ]; then
    st "карты на main $rev: карта всего бинарника (RW_BINARY_MAP)"
    binary_map || echo "binary map failed at $rev (the run goes on): $RUNS/nightly_binmap.log" >> "$LOG"
  fi
  if [ "$s_funcmap" = 1 ]; then
  python3 tools/mod/rw_function_map.py --game "$RW_GAME_DAT" --ini "$INI" --inventory workspace/rebuild_inventory.json \
      --decomp "$XMAP/rw_map.csv" --out docs/ennorath/RW_FUNCTION_MAP > "$RUNS/nightly_map.log" 2>&1 || { echo "map failed at $rev" >> "$LOG"; return 1; }
  cmp -s workspace/rebuild_inventory.json "$INV" || cp -f workspace/rebuild_inventory.json "$INV"   # a new game.dat: share the new cache
  fi
  if [ "$s_gaps" = 1 ]; then
  python3 tools/mod/matrix_gaps.py --base "$ROTWK_INSTALL" --overlay "$OPENBFME_ENNORATH_INSTALL" --inventory workspace/rebuild_inventory.json \
      --game "$RW_GAME_DAT" > "$RUNS/nightly_gaps.log" 2>&1 || { echo "matrix_gaps failed at $rev" >> "$LOG"; return 1; }
  # FIELD_GAPS on this run's audit, with the template getters of the decomp map and the whole-image inventory when they exist
  local finv=workspace/rebuild_inventory.json xm=()
  [ -f "$INV_FULL" ] && finv=$INV_FULL
  [ -f "$XMAP/rw_map.csv" ] && xm=(--xmap "$XMAP/rw_map.csv")
  python3 tools/mod/field_gaps.py --audit "$AUDIT/field-audit.tsv" --inventory "$finv" --game "$RW_GAME_DAT" "${xm[@]}" \
      > "$RUNS/nightly_fgaps.log" 2>&1 || { echo "field_gaps failed at $rev" >> "$LOG"; return 1; }
  # the attribute modifier map (owner 2026-11-19) on this run's MATRIX_GAPS: docs/ennorath/MODIFIER_*; the per-list table is the mod's data: Drive only
  if [ -f "$INV_FULL" ]; then
    local mm="$RUNS/modifier-map"; mkdir -p "$mm"
    if python3 tools/mod/modifier_map.py --base "$ROTWK_INSTALL" --overlay "$OPENBFME_ENNORATH_INSTALL" --game "$RW_GAME_DAT" --inventory "$INV_FULL"         --out "$mm" > "$RUNS/nightly_modmap.log" 2>&1; then
      cp -f "$mm/modifier-map.md" docs/ennorath/MODIFIER_MAP.md
      cp -f "$mm/modifier-readers.tsv" docs/ennorath/MODIFIER_READERS.tsv
      cp -f "$mm/modifier-port-reads.tsv" docs/ennorath/MODIFIER_PORT_READS.tsv
      cp -f "$mm/modifier-unresolved.tsv" docs/ennorath/MODIFIER_UNRESOLVED.tsv
      cp -f "$mm/modifier-matrix.tsv" docs/ennorath/MODIFIER_MATRIX.tsv
    else
      echo "modifier map failed at $rev (the run goes on)" >> "$LOG"
    fi
  fi
  fi
  rm -f workspace/rebuild_inventory.json
  # task drafts from the red buttons, grouped by module class, after MATRIX_GAPS (owner 2026-11-17): Drive openbfme-runs/maps/drafts, never the repository
  if [ "$s_drafts" = 1 ]; then
  python3 tools/mod/matrix_drafts.py --ini "$INI" --out "$RUNS/drafts" > "$RUNS/nightly_drafts.log" 2>&1           || echo "matrix drafts failed at $rev (the run goes on)" >> "$LOG"
  fi
  # the day's progress row from this run (owner 2026-11-17: the dashboard's /stats and the home page bar read it without Claude):
  # this run's audit, matrix and gaps, and the stop curve against the previous row
  python3 tools/mod/progress_snapshot.py --audit "$AUDIT/field-audit.tsv" --audit-name auto --write >> "$LOG" 2>&1 || echo "progress row failed at $rev" >> "$LOG"
  local stops; stops=$(tail -1 docs/ennorath/PROGRESS.tsv | awk -F'\t' '{print "open " $8 ", new " $12 ", closed " $13}')
  st "карты на main $rev: коммит auto/maps"
  git checkout -q -B auto/maps
  git add docs/ennorath/MATRIX.tsv docs/ennorath/README.md docs/ennorath/RW_FUNCTION_MAP.tsv docs/ennorath/RW_FUNCTION_MAP.md \
      docs/ennorath/MATRIX_GAPS.tsv docs/ennorath/MATRIX_GAPS.md docs/ennorath/FIELD_GAPS.tsv docs/ennorath/PROGRESS.tsv docs/ennorath/MATRIX_FACTIONS.tsv docs/ennorath/MATRIX_REVIEW.md       docs/ennorath/SCRIPT_GRANTS.tsv docs/ennorath/SCRIPT_MAP.md docs/ennorath/GRANTS_INI.tsv \
      docs/ennorath/MODIFIER_MAP.md docs/ennorath/MODIFIER_READERS.tsv docs/ennorath/MODIFIER_PORT_READS.tsv docs/ennorath/MODIFIER_UNRESOLVED.tsv docs/ennorath/MODIFIER_MATRIX.tsv \
      docs/ennorath/MATRIX_GREY.tsv docs/ennorath/MATRIX_GREY.md docs/ennorath/MAP_SWEEP.tsv docs/ennorath/MAP_SWEEP.md \
      docs/ennorath/RW_BINARY_MAP.tsv docs/ennorath/RW_BINARY_MAP.md docs/ennorath/RW_CLASSES.tsv docs/ennorath/RW_VCALLS.tsv \
      || { echo "$day: git add of the maps failed at $rev (nothing committed)" >> "$LOG"; return 1; }
  local red; red=$(python3 -c "import sys; sys.path.insert(0,'tools/mod'); import progress_snapshot as p; print(*p.matrix_counts(None))")
  if git diff --cached --quiet; then
    echo "$day: nothing changed at $rev" >> "$LOG"
  else
    git commit -q -m "Nightly maps $day on main $rev: matrix, MATRIX_GREY, map sweep, function map, binary map, MATRIX_GAPS, FIELD_GAPS (red buttons / specific gaps: $red)" \
        -m "tools/colab/nightly_maps.sh (sandbox $SB); GitHub Actions lands it (maps-land.yml)." || { echo "commit refused at $rev" >> "$LOG"; return 1; }
    # the sandbox shell may have no GitHub credentials: the commit then waits on the Drive as a patch (Claude applies it on the PC, 2026-11-16)
    git format-patch -1 --stdout HEAD > "$RUNS/$day.patch"
    git push -q -f origin auto/maps 2>/dev/null && rm -f "$RUNS/$day.patch" || echo "push failed at $rev: the commit is $RUNS/$day.patch" >> "$LOG"
    # owner 2026-11-20: GitHub Actions lands the commit on main (.github/workflows/maps-land.yml, tools/ci/maps_land.py: only the maps
    # files, whole tables, PROGRESS by date, README three-way); a red check leaves auto/maps for Claude
  fi
  { echo "# Nightly maps $day (main $rev, sandbox $SB)"; echo; echo "red buttons / specific gaps: $red"; echo
    echo "steps run:${RAN:- none}; skipped (inputs unchanged since ${BASE:-?}, their sections below are from an earlier run):${SKIPPED:- none}"; echo
    echo "stops since the previous progress row: $stops"; echo
    echo '## Function map'; tail -3 "$RUNS/nightly_map.log"; echo; echo '## Whole-image map'; tail -2 "$RUNS/nightly_binmap.log"; echo; echo '## MATRIX_GAPS'; tail -2 "$RUNS/nightly_gaps.log"
    echo; echo '## FIELD_GAPS'; tail -2 "$RUNS/nightly_fgaps.log"; echo; echo '## Matrix'; tail -5 "$RUNS/nightly_matrix.log"
    echo; echo '## Map sweep'; grep -m1 '^Maps:' docs/ennorath/MAP_SWEEP.md 2>/dev/null; echo; echo '## MATRIX_GREY'; tail -2 "$RUNS/nightly_grey.log"
    echo; echo '## Field audit (changes against the previous run)'; head -40 "$AUDIT/field-audit-diff.md" 2>/dev/null
    echo; echo '## Task drafts (Drive openbfme-runs/maps/drafts/index.md)'; tail -1 "$RUNS/nightly_drafts.log"
    echo; sed -n '/^## Lint/,$p' docs/ennorath/MATRIX_REVIEW.md 2>/dev/null | head -60; } > "$RUNS/$day.md"
  cp -f "$RUNS/$day.md" "$RUNS/latest.md"
  # a partial run (--only) does not move the baseline: the next run still compares against the last full view of main
  if [ -z "$ONLY" ]; then echo "$rev" > "$RUNS/last_rev"; echo "$key" > "$RUNS/inputs_key"; fi
  echo "== $day $(date -u +%T) done ($red; ran:${RAN:- none})" >> "$LOG"
  st "жду следующего переноса в main (последний прогон: main $rev, красных кнопок / пробелов $red)"
}

case "${1:-}" in
  --now) run; exit $? ;;
  --full) FULL=1; run; exit $? ;;
  --only) ONLY="${2:?--only needs the step names: $STEPS_ALL}"; run; exit $? ;;
esac
# after every landing (owner 2026-11-17): a "Landing:" commit (tools/land.py) or an upstream merge since the last run, and main still for
# 15 minutes; otherwise once a day at 01:00 UTC when main moved at all (tool and doc commits alone do not take the sandbox for an hour)
while true; do
  git -C "$REPO" fetch -q origin 2>/dev/null
  self_update nightly_maps.sh
  head=$(git -C "$REPO" rev-parse --short=8 origin/main 2>/dev/null)
  last=$(cat "$RUNS/last_rev" 2>/dev/null)
  age=$(( $(date +%s) - $(git -C "$REPO" log -1 --format=%ct origin/main 2>/dev/null || echo 0) ))
  landed=0
  if [ -n "$last" ] && git -C "$REPO" cat-file -e "$last^{commit}" 2>/dev/null; then
    # the 15 still minutes count from the newest landing, not from main's tip (2026-11-19: Claude's doc and tool commits every 10-20 minutes
    # after the 08:50 landing kept the run from starting for 1.5 hours)
    land_ct=$(git -C "$REPO" log -1 --format=%ct -E --grep='^(Landing|Merge upstream)' "$last..origin/main" 2>/dev/null)
    [ -n "$land_ct" ] && landed=1 && age=$(( $(date +%s) - land_ct ))
  else
    landed=1   # no previous run known: run once
  fi
  night=0; [ "$(date -u +%H)" = "01" ] && night=1
  if [ -n "$head" ] && [ "$head" != "$last" ] && [ "$age" -ge 900 ] && [ $((landed + night)) -gt 0 ]; then
    run || echo "$head" > "$RUNS/last_rev"   # a failed run is not retried on the same main (its log says why)
  fi
  sleep 900
done
