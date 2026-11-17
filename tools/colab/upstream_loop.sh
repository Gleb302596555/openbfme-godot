#!/bin/bash
# Sandbox 4 (owner 2026-11-16): the upstream PR loop of tools/upstream_pr.py, every 3 hours. rebase squashes the fork's up/* branches onto
# upstream/main (list-file conflicts and "#N" fixed), check runs the text and merge checks of each, and (owner 2026-11-17) open --ready opens
# an Open-BFME PR for every branch the check found ready that has no upstream PR yet; visual and failing branches stay for the owner's
# captures / the agent's rework. Logs and the report: Drive openbfme-runs/upstream (the dashboard's /local "PR в апстрим" reads it).
# Install: cp tools/colab/upstream_loop.sh /content/sandbox/ ; setsid nohup bash /content/sandbox/upstream_loop.sh > /content/upstream.log 2>&1 &
REPO=/content/sandbox/openbfme-godot
OUT=/content/drive/MyDrive/openbfme-runs/upstream
mkdir -p "$OUT"
while true; do
  {
    echo "== $(date -u '+%F %T') UTC"
    cd "$REPO" && git fetch -q origin && git fetch -q upstream && git checkout -q main && git reset -q --hard origin/main
    [ -d workspace/wt/upstream-pr/.git ] || [ -f workspace/wt/upstream-pr/.git ] || git worktree add -q --detach workspace/wt/upstream-pr upstream/main
    python3 tools/upstream_pr.py rebase
    python3 tools/upstream_pr.py check
    python3 tools/upstream_pr.py open --ready
    python3 tools/upstream_pr.py check  # the report and the dashboard's PR list see the PRs just opened
    cp -f workspace/upstream-pr/report.md workspace/upstream-pr/report.json "$OUT/" 2>/dev/null
    # the dashboard (/local, "PR в апстрим") finds the report on Drive by this name
    cp -f workspace/upstream-pr/report.json "$OUT/openbfme-upstream-report.json" 2>/dev/null
  } >> "$OUT/loop.log" 2>&1
  sleep 10800
done
