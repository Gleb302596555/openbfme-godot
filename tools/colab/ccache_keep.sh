#!/bin/bash
# Keeps a sandbox-local ccache across Colab restarts (owner, 2026-11-16): the PR checker's cache lives on the sandbox's own disk
# (/content/ccache-prcheck: Drive serves ccache's many small files too slowly), which a restart wipes. This script packs it into one
# archive on Drive and unpacks it at start.
#
#   bash tools/colab/ccache_keep.sh restore <dir>           # at start: unpack the Drive archive when <dir> is missing or empty
#   bash tools/colab/ccache_keep.sh loop <dir> [minutes]    # in the background: pack <dir> to Drive every N minutes (default 60) when it changed
#
# The archive: Drive openbfme-ccache-keep/<name of dir>-sb<N>.tar.zst, written to a .part file first and then renamed (a cut write
# never replaces a good archive).
set -uo pipefail
cmd=${1:?restore or loop}
dir=${2:?the cache directory}
mins=${3:-60}
keep=/content/drive/MyDrive/openbfme-ccache-keep
sb=$(cat /content/sandbox_id 2>/dev/null || echo 0)
arc="$keep/$(basename "$dir")-sb$sb.tar.zst"
mkdir -p "$keep"

case "$cmd" in
restore)
  if [ -d "$dir" ] && [ -n "$(ls -A "$dir" 2>/dev/null)" ]; then
    echo "ccache_keep: $dir already has a cache"
  elif [ -f "$arc" ]; then
    mkdir -p "$dir"
    t0=$(date +%s)
    zstd -dc -T0 "$arc" | tar -x -C "$dir" && echo "ccache_keep: restored $dir from $arc ($(du -sh "$dir" | cut -f1), $(( $(date +%s) - t0 )) s)"
  else
    echo "ccache_keep: no archive $arc yet"
  fi
  ;;
loop)
  mark="$dir/.ccache_keep_saved"
  while true; do
    sleep $(( mins * 60 ))
    [ -d "$dir" ] || continue
    if [ -f "$mark" ] && [ -z "$(find "$dir" -newer "$mark" -type f -not -name '*.lock' -print -quit 2>/dev/null)" ]; then
      continue
    fi
    touch "$mark.new"
    if tar -c -C "$dir" --exclude='./tmp' --exclude='*.lock' . 2>/dev/null | zstd -q -T0 -3 -o /content/ccache_keep.tar.zst -f \
       && cp /content/ccache_keep.tar.zst "$arc.part" && mv -f "$arc.part" "$arc"; then
      mv -f "$mark.new" "$mark"
      echo "ccache_keep: saved $dir to $arc ($(du -h "$arc" | cut -f1)) $(date -u +%H:%M)"
    fi
    rm -f /content/ccache_keep.tar.zst
  done
  ;;
*)
  echo "usage: ccache_keep.sh restore|loop <dir> [minutes]" >&2
  exit 2
  ;;
esac
