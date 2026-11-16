#!/bin/bash
# Sandbox 3 (or any one sandbox, never two): the nightly maps (tools/colab/nightly_maps.sh) from today's main, every night at 01:00 UTC.
# MAPS_TREE: the built tree to use (default workspace/wt/local-sb<N>-st3).
cd /content/sandbox/openbfme-godot && git fetch -q origin
git show origin/main:tools/colab/nightly_maps.sh > /content/sandbox/nightly_maps.sh
exec bash /content/sandbox/nightly_maps.sh
