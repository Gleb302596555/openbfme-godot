#!/bin/bash
# Sandbox 2 (watchdog.py ROLES): the batch bot's loop (tools/colab/batch_bot.py loop) from today's main: a batch of at most 5 green PRs
# whenever the PR checker has enough, then it waits for Claude's landing. Logs: /content/batch_loop.log, the Drive openbfme-runs/batch/.
cd /content/sandbox/openbfme-godot && { git fetch -q origin || true; }
git show origin/main:tools/colab/batch_bot.py > /content/sandbox/batch_bot.py
exec python3 /content/sandbox/batch_bot.py loop
