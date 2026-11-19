#!/bin/bash
# Sandbox 3, the maps sandbox (owner 2026-11-19), its own small loop, independent of the maps run (which waits for landings and can be paused):
# every 30 minutes every open Port / Local task's disassembly dossier (tools/mod/dossier_disasm.py on main's function map). A changed one is
# encrypted with the arena archive's key (/root/.arena_key, never printed) and pushed to refs/arena/dossier/<issue>, which the arena prompt
# fetches and decrypts (a trusted person's one-task grant re-encrypts it); the plain copy goes to the private Drive for the local agents.
# It runs from a copy of origin/main's tools and map in /content/dossier, so it never touches a tree the maps run or a station builds in.
# /content/sandbox/watchdog.off.dossier holds it off.
REPO=/content/sandbox/openbfme-godot
W=/content/dossier
INV=/content/drive/MyDrive/openbfme-audit/rw_function_blocks_full.json
DRIVE_OUT=/content/drive/MyDrive/openbfme-runs/dossiers
GAME="${ROTWK_GAME_DAT:-/content/game_assets/bfme_assets/RotWK/game.dat}"
mkdir -p $W/out $DRIVE_OUT
while true; do
  if [ -e /content/sandbox/watchdog.off.dossier ] || [ ! -s /root/.arena_key ]; then sleep 600; continue; fi
  export GH_TOKEN=$(sed -n "s#https://[^:]*:\([^@]*\)@github.com#\1#p" ~/.git-credentials | head -1)
  if git -C $REPO fetch -q origin; then
    for f in dossier_disasm.py citation_check.py; do git -C $REPO show origin/main:tools/mod/$f > $W/$f.new && mv $W/$f.new $W/$f; done
    git -C $REPO show origin/main:docs/ennorath/RW_FUNCTION_MAP.tsv > $W/RW_FUNCTION_MAP.tsv
    (cd $W && python3 dossier_disasm.py --game "$GAME" --inventory $INV --map $W/RW_FUNCTION_MAP.tsv --out $W/out) > /content/dossier.log 2>&1
    for n in $(sed -n 's/^WROTE //p' /content/dossier.log); do
      cp -f $W/out/dossier-$n.txt $DRIVE_OUT/
      openssl enc -aes-256-cbc -pbkdf2 -iter 200000 -salt -in $W/out/dossier-$n.txt -out $W/out/dossier-$n.txt.enc -pass file:/root/.arena_key || continue
      blob=$(git -C $REPO hash-object -w $W/out/dossier-$n.txt.enc)
      tree=$(printf '100644 blob %s\tdossier.txt.enc\n' "$blob" | git -C $REPO mktree)
      commit=$(git -C $REPO -c user.name=dossier -c user.email=dossier@localhost commit-tree $tree -m "dossier of issue $n")
      git -C $REPO push -q -f origin "$commit:refs/arena/dossier/$n" && echo "$(date -u '+%F %T') UTC dossier $n pushed" >> /content/dossier-history.log
    done
    mkdir -p /content/status && echo "досье задач: $(tail -1 /content/dossier.log)" > /content/status/dossier.txt
  fi
  sleep 1800
done
