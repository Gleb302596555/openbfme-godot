#!/bin/bash
# Every sandbox: the watchdog (tools/colab/watchdog.py) from today's main: keeps this sandbox's loops running and writes its heartbeat for the
# dashboard (Drive openbfme-runs/upstream/openbfme-health-sb<N>.json).
cd /content/sandbox/openbfme-godot && git fetch -q origin
git show origin/main:tools/colab/watchdog.py > /content/sandbox/watchdog.py
exec python3 /content/sandbox/watchdog.py
