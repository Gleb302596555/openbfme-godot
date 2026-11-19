#!/bin/bash
# Sandbox 3 (watchdog.py ROLES; any one sandbox, never two): the maps run (tools/colab/nightly_maps.sh) from today's main: matrix, field
# audit, function map and their joins after every landing (and at 01:00 UTC when main moved). MAPS_TREE: its tree (default workspace/wt/maps).
# paused by the owner (2026-11-19): /content/sandbox/watchdog.off.maps holds it off after a restart too, as it holds the watchdog
[ -e /content/sandbox/watchdog.off.maps ] && { echo "maps paused: /content/sandbox/watchdog.off.maps"; exit 0; }
cd /content/sandbox/openbfme-godot && git fetch -q origin
git show origin/main:tools/colab/nightly_maps.sh > /content/sandbox/nightly_maps.sh
exec bash /content/sandbox/nightly_maps.sh
