#!/bin/bash
# Full openbfme_tests run (PROCESS 7, "Ночью") on a sandbox, one process per test file (JOBS at a time), on the maps tree's build of the same main:
# run it after a maps run has built (owner 2026-11-23: "запусти полный ночной прогон на 4 песочнице"). Results: Drive openbfme-runs/night-full/
# (summary.txt, failures.tsv: test, first error place, message; bad_files.tsv: a file whose process crashed or gave no summary). Per-file
# processes cannot check "skipped == doctest::skip count": the run is complete when bad_files.tsv is empty.
set -uo pipefail
source /root/.agent_env
T=/content/sandbox/openbfme-godot/workspace/wt/maps
OUT=/content/drive/MyDrive/openbfme-runs/night-full
LOCAL=/content/night-full
JOBS=${JOBS:-3}
mkdir -p "$OUT" "$LOCAL/logs" "$LOCAL/tmp"
cd "$T" || exit 1
rev=$(git rev-parse --short=8 HEAD)
echo "[$(date -u +%T)] start on $rev, jobs $JOBS" > "$OUT/run.log"
cp -f engine/build/openbfme_tests "$LOCAL/openbfme_tests"   # the maps run may rebuild its tree later: keep this main's binary
mapfile -t SCOPE < <(grep -v '^#' tools/fork_scope.txt | sed '/^\s*$/d')
excl=$(grep '^-sfe=' tools/fork_scope.txt | sed 's/^-sfe=//')
ls engine/tests/test_*.cpp | xargs -n1 basename | while read -r f; do
  case ",$excl," in *"*$f,"*) continue;; esac
  echo "$f"
done > "$LOCAL/files.txt"
echo "[$(date -u +%T)] $(wc -l < "$LOCAL/files.txt") files" >> "$OUT/run.log"
export LOCAL
one() {
  f=$1; log=$LOCAL/logs/$f.log
  [ -f "$log.done" ] && return 0
  mkdir -p "$LOCAL/tmp/$f"
  TMPDIR=$LOCAL/tmp/$f timeout 5400 nice -n 10 "$LOCAL/openbfme_tests" "-sf=*/$f" "${@:2}" > "$log" 2>&1
  echo $? > "$log.done"
}
export -f one
xargs -a "$LOCAL/files.txt" -P "$JOBS" -I{} bash -c 'one "$@"' _ {} "${SCOPE[@]}"
echo "[$(date -u +%T)] all files ran" >> "$OUT/run.log"
# summary: per file exit code and counts, failing test cases with their first error
python3 - "$LOCAL" "$OUT" "$rev" <<'EOF'
import sys, re, pathlib
local, out, rev = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2]), sys.argv[3]
tot = dict(passed=0, failed=0, skipped=0); bad = []; fails = []
for log in sorted((local / "logs").glob("*.log")):
    f = log.name[:-4]
    t = log.read_text(errors="replace")
    rc = (local / "logs" / (log.name + ".done")).read_text().strip() if (local / "logs" / (log.name + ".done")).exists() else "?"
    m = re.search(r"test cases:\s*(\d+)\s*\|\s*(\d+) passed\s*\|\s*(\d+) failed\s*\|\s*(\d+) skipped", t)
    if m:
        tot["passed"] += int(m[2]); tot["failed"] += int(m[3]); tot["skipped"] += int(m[4])
    if not m or rc not in ("0", "1") or "CRASHED" in t:
        bad.append(f"{f}\trc={rc}\t{'CRASHED' if 'CRASHED' in t else ('no summary' if not m else '')}")
    for blk in re.split(r"\n={20,}\n", t):
        tc = re.search(r"TEST CASE:\s+(.*)", blk)
        er = re.search(r"^(\S+:\d+): (?:ERROR|FATAL ERROR): (.*)$", blk, re.M)
        if tc and er:
            fails.append(f"{tc[1].strip()}\t{er[1].split('/')[-1]}\t{er[2].strip()[:200]}")
(out / "failures.tsv").write_text("test\tplace\tmessage\n" + "\n".join(fails) + "\n")
(out / "bad_files.tsv").write_text("\n".join(bad) + "\n")
(out / "summary.txt").write_text(f"main {rev}: passed {tot['passed']}, failed {tot['failed']}, skipped {tot['skipped']}; failing cases {len(fails)}; files without a clean end {len(bad)}\n")
print((out / "summary.txt").read_text())
EOF
echo "[$(date -u +%T)] done: $(cat "$OUT/summary.txt")" >> "$OUT/run.log"
