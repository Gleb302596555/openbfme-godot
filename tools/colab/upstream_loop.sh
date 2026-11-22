#!/bin/bash
# Sandbox 4 (owner 2026-11-16): the upstream PR loop of tools/upstream_pr.py, every 3 hours. rebase squashes the fork's up/* branches onto
# upstream/main (list-file conflicts and "#N" fixed), check runs the text and merge checks of each, and (owner 2026-11-17) open --ready opens
# an Open-BFME PR for every branch the check found ready that has no upstream PR yet; visual and failing branches stay for the owner's
# captures / the agent's rework. Logs and the report: Drive openbfme-runs/upstream (the dashboard's /local "PR в апстрим" reads it).
# Install: cp tools/colab/upstream_loop.sh /content/sandbox/ ; setsid nohup bash /content/sandbox/upstream_loop.sh > /content/upstream.log 2>&1 &
REPO=/content/sandbox/openbfme-godot
OUT=/content/drive/MyDrive/openbfme-runs/upstream
mkdir -p "$OUT"
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
while true; do
  git -C "$REPO" fetch -q origin 2>/dev/null
  self_update upstream_loop.sh
  {
    echo "== $(date -u '+%F %T') UTC"
    cd "$REPO" && git fetch -q origin && git fetch -q upstream && git checkout -q main && git reset -q --hard origin/main
    [ -d workspace/wt/upstream-pr/.git ] || [ -f workspace/wt/upstream-pr/.git ] || git worktree add -q --detach workspace/wt/upstream-pr upstream/main
    st() { mkdir -p /content/status && echo "$*" > /content/status/upstream.txt; }   # the dashboard's "what it does" line
    st "ветки up/*: перенос на свежий upstream/main"
    python3 tools/upstream_pr.py rebase
    st "ветки up/*: проверка (сборка, тесты, текст)"
    python3 tools/upstream_pr.py check
    st "ветки up/*: открываю PR готовых"
    python3 tools/upstream_pr.py open --ready
    python3 tools/upstream_pr.py check  # the report and the dashboard's PR list see the PRs just opened
    cp -f workspace/upstream-pr/report.md workspace/upstream-pr/report.json "$OUT/" 2>/dev/null
    # the dashboard (/local, "PR в апстрим") finds the report on Drive by this name
    cp -f workspace/upstream-pr/report.json "$OUT/openbfme-upstream-report.json" 2>/dev/null
    st "сплю до следующего круга ($(date -u -d '+3 hours' '+%H:%M') UTC)"
  } >> "$OUT/loop.log" 2>&1
  sleep 10800
done
