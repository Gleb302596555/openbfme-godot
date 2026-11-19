#!/bin/bash
# Sandbox 4 (owner 2026-11-19): every 30 minutes
#  * the open tasks that port the same RotWK functions (tools/mod/task_overlap.py scan) -> Drive openbfme-runs/upstream/task-overlap.json, which
#    the dashboard shows at each task and refuses to give out;
#  * every open task's disassembly dossier (tools/mod/dossier_disasm.py): a changed one is encrypted with the arena archive's key
#    (/root/.arena_key, never printed) and pushed to refs/arena/dossier/<issue>, which the agent's prompt fetches and decrypts.
# It runs from a copy of origin/main's tools, maps and engine/src in /content/overlap, so it never touches a tree another loop builds in.
REPO=/content/sandbox/openbfme-godot
W=/content/overlap
OUT=/content/drive/MyDrive/openbfme-runs/upstream/task-overlap.json
INV=/content/drive/MyDrive/openbfme-audit/rw_function_blocks_full.json
mkdir -p $W
while true; do
  [ -e /content/sandbox/watchdog.off.overlap ] && { sleep 600; continue; }
  export GH_TOKEN=$(sed -n "s#https://[^:]*:\([^@]*\)@github.com#\1#p" ~/.git-credentials | head -1)
  if git -C $REPO fetch -q origin; then
    for f in task_overlap.py citation_check.py dossier_disasm.py; do git -C $REPO show origin/main:tools/mod/$f > $W/$f.new && mv $W/$f.new $W/$f; done
    git -C $REPO show origin/main:docs/ennorath/RW_FUNCTION_MAP.tsv > $W/RW_FUNCTION_MAP.tsv
    rm -rf $W/src.new && mkdir -p $W/src.new && git -C $REPO archive origin/main engine/src | tar -x -C $W/src.new && rm -rf $W/src && mv $W/src.new $W/src
    (cd $W && python3 task_overlap.py scan --src $W/src/engine/src --out $OUT --inventory $INV) > /content/overlap.log 2>&1
    echo "$(date -u '+%F %T') UTC $(head -1 /content/overlap.log)" >> /content/overlap-history.log
    mkdir -p /content/status && echo "пересечения задач: $(head -1 /content/overlap.log)" > /content/status/overlap.txt
    if [ -s /root/.arena_key ]; then
      mkdir -p $W/dossiers
      GAME="${ROTWK_GAME_DAT:-/content/game_assets/bfme_assets/RotWK/game.dat}"
      (cd $W && python3 dossier_disasm.py --game "$GAME" --inventory $INV --map $W/RW_FUNCTION_MAP.tsv --out $W/dossiers) > /content/dossier.log 2>&1
      mkdir -p /content/drive/MyDrive/openbfme-runs/dossiers
      for n in $(sed -n 's/^WROTE //p' /content/dossier.log); do
        cp -f $W/dossiers/dossier-$n.txt /content/drive/MyDrive/openbfme-runs/dossiers/   # the local agents' copy (private Drive)
        openssl enc -aes-256-cbc -pbkdf2 -iter 200000 -salt -in $W/dossiers/dossier-$n.txt -out $W/dossiers/dossier-$n.txt.enc -pass file:/root/.arena_key || continue
        blob=$(git -C $REPO hash-object -w $W/dossiers/dossier-$n.txt.enc)
        tree=$(printf '100644 blob %s\tdossier.txt.enc\n' "$blob" | git -C $REPO mktree)
        commit=$(git -C $REPO -c user.name=dossier -c user.email=dossier@localhost commit-tree $tree -m "dossier of issue $n")
        git -C $REPO push -q -f origin "$commit:refs/arena/dossier/$n" && echo "$(date -u '+%F %T') UTC dossier $n pushed" >> /content/overlap-history.log
      done
    fi
  fi
  sleep 1800
done
