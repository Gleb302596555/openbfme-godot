#!/bin/bash
# Colab build: the engine, openbfme_tests AND the GDExtension (godot/bin/openbfme.linux.*.so).
# The GDExtension is built on purpose: with OPENBFME_BUILD_GODOT=OFF a GodotDevice/ error (Port 158, 2026-11-14) only showed on the
# owner's Windows game build. Run from the tree to build: bash tools/colab/build_tests.sh [jobs]
set -euo pipefail
[ -f /root/.agent_env ] && source /root/.agent_env
# /root/.agent_env is rebuilt on every Colab restart without it
export RW_GAME_DAT="${RW_GAME_DAT:-${OPENBFME_GAME_DAT:-}}"
# The compiler cache lives on the sandbox's own disk (Drive serves ccache's many small files slowly), the Drive one (agent_env's CCACHE_DIR)
# read-only behind it. Every tree of a sandbox shares /content/ccache-local (owner 2026-11-16: "кеш всех деревьев"); the PR checker keeps its
# own (OPENBFME_CCACHE_DIR=/content/ccache-prcheck). tools/colab/ccache_keep.sh packs each to Drive hourly and unpacks it after a restart
# (autostart.d/ccache.sh, autostart.d/prcheck.sh).
[ -z "${OPENBFME_CCACHE_DIR:-}" ] && [ -f /content/sandbox_id ] && OPENBFME_CCACHE_DIR=/content/ccache-local
if [ -n "${OPENBFME_CCACHE_DIR:-}" ]; then
  [ -n "${CCACHE_DIR:-}" ] && [ "$CCACHE_DIR" != "$OPENBFME_CCACHE_DIR" ] && export CCACHE_REMOTE_STORAGE="file:$CCACHE_DIR|read-only"
  export CCACHE_DIR="$OPENBFME_CCACHE_DIR" CCACHE_MAXSIZE="${CCACHE_MAXSIZE:-8G}"  # 8G: the hourly Drive archive stays small enough
fi
# Paths under /content are hashed relative to the build directory (2026-11-16): every tree (prcheck, local-sbN-stK) compiles with
# absolute -I paths of its own, so without this no tree could hit another tree's cache entries (the PR checker had 0 hits from Drive's)
export CCACHE_BASEDIR="${CCACHE_BASEDIR:-/content}"
cd "$(git rev-parse --show-toplevel)"
git submodule update --init --depth 1 engine/thirdparty/godot-cpp
cmake -S engine -B engine/build -G Ninja -DCMAKE_BUILD_TYPE=Release -DOPENBFME_BUILD_GODOT=ON \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
cmake --build engine/build -j "${1:-$(nproc)}"
echo "BUILD DONE: engine/build/openbfme_tests and godot/bin/openbfme.linux.*.so"
