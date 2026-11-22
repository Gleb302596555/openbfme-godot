#!/bin/bash
# Run by the Colab setup cell after the sandbox restore (the owner's two-line addition at the cell's end, 2026-11-15):
# starts every /content/sandbox/autostart.d/*.sh of this sandbox in the background, log /content/<name>.log.
# The folder is in /content/sandbox, so it survives restarts through the Drive backup; which jobs a sandbox runs is its own choice.
#
# 2026-11-18: one `git fetch` here, before any job starts. The jobs each fetched at once, and git's ref lock let only one win: on sb1 the
# PR checker's script (set -e) died on "cannot lock ref 'refs/remotes/origin/main'". Then every installed job script (and this script)
# is refreshed from origin/main by name, so a landed fix reaches the sandboxes at their next start; a sandbox-only job (ccache.sh) stays
# as it is. The installed copies used to stay at the version first copied by hand.
[ -f /root/.agent_env ] && source /root/.agent_env
mkdir -p /root/.vscode-server   # save_sandbox.sh tars it; a sandbox without VS Code had no backup at all (2026-11-15)
REPO=/content/sandbox/openbfme-godot
for i in 1 2 3; do git -C "$REPO" fetch -q origin && break; sleep 10; done
refresh() {   # $1 = repository path, $2 = installed copy; only a copy that parses replaces the old one
  local new="$2.new"
  git -C "$REPO" show "origin/main:$1" > "$new" 2>/dev/null && [ -s "$new" ] && bash -n "$new" && ! cmp -s "$new" "$2" \
    && mv -f "$new" "$2" && echo "autostart: refreshed $2 from origin/main"
  rm -f "$new"
}
if [ -z "${AUTOSTART_REFRESHED:-}" ]; then
  refresh tools/colab/autostart.sh /content/sandbox/autostart.sh
  export AUTOSTART_REFRESHED=1
  exec bash /content/sandbox/autostart.sh   # the refreshed copy does the rest (same copy when nothing changed)
fi
for f in /content/sandbox/autostart.d/*.sh; do
  [ -f "$f" ] || continue
  refresh "tools/colab/autostart.d/$(basename "$f")" "$f"
done
for f in /content/sandbox/autostart.d/*.sh; do
  [ -f "$f" ] || continue
  nohup bash "$f" > "/content/$(basename "$f" .sh).log" 2>&1 < /dev/null &
  echo "autostart: $f (pid $!)"
done
