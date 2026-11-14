#!/bin/bash
# Colab build: the engine, openbfme_tests AND the GDExtension (godot/bin/openbfme.linux.*.so).
# The GDExtension is built on purpose: with OPENBFME_BUILD_GODOT=OFF a GodotDevice/ error (Port 158, 2026-11-14) only showed on the
# owner's Windows game build. Run from the tree to build: bash tools/colab/build_tests.sh [jobs]
set -euo pipefail
[ -f /root/.agent_env ] && source /root/.agent_env
# /root/.agent_env is rebuilt on every Colab restart without it
export RW_GAME_DAT="${RW_GAME_DAT:-${OPENBFME_GAME_DAT:-}}"
cd "$(git rev-parse --show-toplevel)"
git submodule update --init --depth 1 engine/thirdparty/godot-cpp
cmake -S engine -B engine/build -G Ninja -DCMAKE_BUILD_TYPE=Release -DOPENBFME_BUILD_GODOT=ON \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
cmake --build engine/build -j "${1:-$(nproc)}"
echo "BUILD DONE: engine/build/openbfme_tests and godot/bin/openbfme.linux.*.so"
