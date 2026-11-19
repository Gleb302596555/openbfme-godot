#!/usr/bin/env python3
"""The sandbox watchdog (owner 2026-11-17: "сторож песочниц"): every 5 minutes on each Colab sandbox it
  - checks that the sandbox's own loops run (ROLES: sb1 the PR checker, sb2 the batch bot's loop, sb3 the maps run, sb4 the upstream loop) and starts a missing one
    again from origin/main's copy, as autostart.d does after a restart (a loop missing at two checks in a row, at most 3 starts an hour;
    /content/sandbox/watchdog.off or a file /content/sandbox/watchdog.off.<loop> holds it off while Claude swaps a loop by hand);
  - writes a heartbeat to the Drive, openbfme-runs/upstream/openbfme-health-sb<N>.json (the folder the dashboard's Drive key reads; the
    dashboard's /local page shows it): each loop's state, when it last did something (its log), the restarts, what else runs (builds,
    tests, the batch bot), load, memory, disk, the uptime. A stale heartbeat on the dashboard = the runtime itself is gone.
Like the loops it restarts on origin/main's newer copy of itself between rounds.

    python3 tools/colab/watchdog.py            # the loop (autostart.d/watchdog.sh starts it)
    python3 tools/colab/watchdog.py --once     # one round, the heartbeat printed
"""
from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path

REPO = Path("/content/sandbox/openbfme-godot")
INSTALL = Path("/content/sandbox")
DRIVE = Path("/content/drive/MyDrive/openbfme-runs")
STATE = Path("/content/watchdog-state.json")
SB = Path("/content/sandbox_id").read_text().strip() if Path("/content/sandbox_id").exists() else "?"

# loop name -> (the installed copy it runs, the command, its log, the files whose age says when it last did something)
LOOPS = {
    "prcheck": ("pr_checker.py", ["python3", "/content/sandbox/pr_checker.py"], "/content/prcheck.log", ["/content/prcheck.log"],
                {"PRCHECK_STATE": "/content/sandbox/prcheck-state.json"}),
    "maps": ("nightly_maps.sh", ["bash", "/content/sandbox/nightly_maps.sh"], "/content/nightly_maps.log",
             [str(DRIVE / "maps/nightly_maps.log"), str(DRIVE / "maps/last_rev")], {}),
    # a running batch writes its own log on the Drive and its status line, not the loop's log (2026-11-18: "активность 2.3 ч назад" in mid-batch)
    "batch": ("batch_bot.py", ["python3", "/content/sandbox/batch_bot.py", "loop"], "/content/batch_loop.log",
              ["/content/batch_loop.log", "/content/status/batch*.txt", str(DRIVE / "batch" / "2*.log")], {}),
    "upstream": ("upstream_loop.sh", ["bash", "/content/sandbox/upstream_loop.sh"], "/content/upstream.log",
                 [str(DRIVE / "upstream/loop.log")], {}),
}
ROLES = {"1": ["prcheck"], "2": ["batch"], "3": ["maps"], "4": ["upstream"]}


def procs() -> list[list[str]]:
    out = []
    for d in Path("/proc").iterdir():
        if d.name.isdigit() and int(d.name) != os.getpid():
            try:
                argv = (d / "cmdline").read_bytes().split(b"\0")
            except OSError:
                continue
            out.append([a.decode("utf-8", "replace") for a in argv if a])
    return out


def running(loop: str, ps: list[list[str]]) -> bool:
    path = LOOPS[loop][1][1]
    extra = LOOPS[loop][1][2:]   # the batch loop is `batch_bot.py loop`: a hand `batch_bot.py run` of the same copy is not the loop
    return any(len(a) >= 2 and a[1] == path and a[2:2 + len(extra)] == extra and a[0].endswith(("python3", "bash", "python")) for a in ps)


def start(loop: str) -> str:
    name, cmd, logf, _, env = LOOPS[loop]
    subprocess.run(["git", "-C", str(REPO), "fetch", "-q", "origin"], capture_output=True)
    new = subprocess.run(["git", "-C", str(REPO), "show", f"origin/main:tools/colab/{name}"], capture_output=True).stdout
    if new:
        tmp = INSTALL / (name + ".new")
        tmp.write_bytes(new)
        os.replace(tmp, INSTALL / name)
    with open(logf, "ab") as log:
        subprocess.Popen(cmd, cwd=str(INSTALL), stdout=log, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL,
                         env=dict(os.environ, **env), start_new_session=True)
    return f"started {loop} ({'origin/main copy' if new else 'installed copy'})"


def age(paths: list[str]) -> int | None:
    """Seconds since the newest of the files (a path may be a glob pattern)."""
    import glob
    found = [f for p in paths for f in (glob.glob(p) if any(c in p for c in "*?[") else [p])]
    ages = [time.time() - Path(f).stat().st_mtime for f in found if Path(f).exists()]
    return int(min(ages)) if ages else None


def tail(path: str, n: int = 2) -> list[str]:
    try:
        with open(path, "rb") as f:
            f.seek(0, 2)
            f.seek(max(0, f.tell() - 4000))
            return [l.decode("utf-8", "replace")[:200] for l in f.read().splitlines()[-n:]]
    except OSError:
        return []


def busy(ps: list[list[str]]) -> dict:
    names = [os.path.basename(a[0]) for a in ps if a]
    joined = [" ".join(a) for a in ps]
    return {"tests": sum(1 for n in names if n == "openbfme_tests"), "compilers": sum(1 for n in names if n in ("cc1plus", "cc1")),
            "batch_bot": any(len(a) >= 3 and a[0].endswith(("python3", "python")) and a[1].endswith("batch_bot.py") and a[2] == "run"
                             for a in ps),   # a batch being built, not the loop that waits
            "matrix": any("matrix_run.py" in j and j.startswith("python") for j in joined)}


def machine() -> dict:
    load = os.getloadavg()[0]
    mem = {}
    for line in Path("/proc/meminfo").read_text().splitlines():
        k, v = line.split(":", 1)
        mem[k] = int(v.split()[0]) // 1024
    disk = shutil.disk_usage("/content")
    up = float(Path("/proc/uptime").read_text().split()[0])
    return {"load": round(load, 2), "cpus": os.cpu_count(), "mem_free_mb": mem.get("MemAvailable"), "mem_total_mb": mem.get("MemTotal"),
            "disk_free_gb": round(disk.free / 2**30, 1), "uptime_h": round(up / 3600, 1)}


def round_once(state: dict) -> dict:
    ps = procs()
    now = time.time()
    loops = []
    for loop in ROLES.get(SB, []):
        st = state.setdefault(loop, {"missing": 0, "starts": []})
        st["starts"] = [t for t in st["starts"] if now - t < 3600]
        ok = running(loop, ps)
        action = ""
        held = (INSTALL / "watchdog.off").exists() or (INSTALL / f"watchdog.off.{loop}").exists()
        if ok:
            st["missing"] = 0
        else:
            st["missing"] += 1
            if held:
                action = "held off (watchdog.off)"
            elif st["missing"] < 2:
                action = "missing: start at the next check if still missing"
            elif len(st["starts"]) >= 3:
                action = "missing: 3 starts this hour, left for Claude"
            else:
                action = start(loop)
                st["starts"].append(now)
                st["missing"] = 0
                st["last_start"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
        loops.append({"loop": loop, "running": ok, "action": action, "activity_age_s": age(LOOPS[loop][3]),
                      "last_start": st.get("last_start"), "starts_last_hour": len(st["starts"]), "log_tail": tail(LOOPS[loop][2])})
    head = subprocess.run(["git", "-C", str(REPO), "log", "-1", "--format=%h %cI", "origin/main"], capture_output=True, text=True).stdout.strip()
    return {"sandbox": SB, "at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()), "loops": loops, "busy": busy(ps), "doing": doing(), "machine": machine(),
            "origin_main": head, "watchdog_pid": os.getpid()}


STATUS_DIR = Path("/content/status")
STATUS_MAX_AGE = 6 * 3600


def doing() -> list[dict]:
    """The bots' own one-line statuses (/content/status/<key>.txt, written by pr_checker.py, batch_bot.py, nightly_maps.sh,
    upstream_loop.sh), newest first; a line older than STATUS_MAX_AGE is dropped (its bot died or forgot it)."""
    out = []
    for f in STATUS_DIR.glob("*.txt") if STATUS_DIR.exists() else []:
        try:
            age_s = int(time.time() - f.stat().st_mtime)
            text = f.read_text(encoding="utf-8", errors="replace").strip()
        except OSError:
            continue
        if text and age_s < STATUS_MAX_AGE:
            out.append({"who": f.stem, "text": text[:300], "age_s": age_s})
    return sorted(out, key=lambda d: d["age_s"])


def publish(beat: dict) -> None:
    out = DRIVE / "upstream" / f"openbfme-health-sb{SB}.json"
    out.parent.mkdir(parents=True, exist_ok=True)
    tmp = out.with_suffix(".tmp")
    tmp.write_text(json.dumps(beat, ensure_ascii=False, indent=1), encoding="utf-8")
    os.replace(tmp, out)


def self_update() -> None:
    me = Path(__file__).resolve()
    if subprocess.run(["git", "-C", str(me.parent), "rev-parse", "--git-dir"], capture_output=True).returncode == 0:
        return  # a run from a tree is that tree's version
    new = subprocess.run(["git", "-C", str(REPO), "show", "origin/main:tools/colab/watchdog.py"], capture_output=True).stdout
    if not new or new == me.read_bytes():
        return
    try:
        compile(new, str(me), "exec")
    except SyntaxError as e:
        print(f"self-update: origin/main's watchdog.py does not compile ({e})", flush=True)
        return
    tmp = me.with_name(me.name + ".new")
    tmp.write_bytes(new)
    os.replace(tmp, me)
    print("self-update: restarting on origin/main's watchdog.py", flush=True)
    os.execv(sys.executable, [sys.executable, str(me)] + sys.argv[1:])


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--once", action="store_true")
    ap.add_argument("--every", type=int, default=300)
    a = ap.parse_args()
    while True:
        try:
            state = json.loads(STATE.read_text()) if STATE.exists() else {}
        except ValueError:
            state = {}
        beat = round_once(state)
        STATE.write_text(json.dumps(state))
        try:
            publish(beat)
        except OSError as e:  # the Drive mount can hiccup: the next round writes again
            print(f"heartbeat not written: {e}", flush=True)
        for l in beat["loops"]:
            if l["action"].startswith("started"):
                print(f"{beat['at']} {l['loop']}: {l['action']}", flush=True)
        if a.once:
            print(json.dumps(beat, ensure_ascii=False, indent=1))
            return 0
        time.sleep(a.every)
        subprocess.run(["git", "-C", str(REPO), "fetch", "-q", "origin"], capture_output=True)
        self_update()


if __name__ == "__main__":
    sys.exit(main())
