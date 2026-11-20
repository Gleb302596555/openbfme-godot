#!/bin/bash
# Sandbox 4 (owner 2026-11-19): every 30 minutes the open tasks that port the same RotWK functions (tools/mod/task_overlap.py scan) ->
# Drive openbfme-runs/upstream/task-overlap.json, which the dashboard shows at each task and refuses to give out. (The dossiers moved to
# sandbox 3's own loop, autostart.d/dossier.sh.) It runs from a copy of origin/main's tools and engine/src in /content/overlap, so it never
# touches a tree another loop builds in.
REPO=/content/sandbox/openbfme-godot
W=/content/overlap
OUT=/content/drive/MyDrive/openbfme-runs/upstream/task-overlap.json
INV=/content/drive/MyDrive/openbfme-audit/rw_function_blocks_full.json
mkdir -p $W
while true; do
  [ -e /content/sandbox/watchdog.off.overlap ] && { sleep 600; continue; }
  export GH_TOKEN=$(sed -n "s#https://[^:]*:\([^@]*\)@github.com#\1#p" ~/.git-credentials | head -1)
  if git -C $REPO fetch -q origin; then
    for f in task_overlap.py citation_check.py; do git -C $REPO show origin/main:tools/mod/$f > $W/$f.new && mv $W/$f.new $W/$f; done
    rm -rf $W/src.new && mkdir -p $W/src.new && git -C $REPO archive origin/main engine/src | tar -x -C $W/src.new && rm -rf $W/src && mv $W/src.new $W/src
    (cd $W && python3 task_overlap.py scan --src $W/src/engine/src --out $OUT --inventory $INV) > /content/overlap.log 2>&1
    echo "$(date -u '+%F %T') UTC $(head -1 /content/overlap.log)" >> /content/overlap-history.log
    mkdir -p /content/status && echo "пересечения задач: $(head -1 /content/overlap.log)" > /content/status/overlap.txt
  fi
  sleep 1800
done
