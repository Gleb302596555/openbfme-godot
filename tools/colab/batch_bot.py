#!/usr/bin/env python3
"""The batch bot (owner 2026-11-17: "сделай бота батча"): the mechanical half of a landing, on a Colab sandbox, so Claude only reviews the
result and runs tools/land.py --batch <name> on the PC.

    python3 tools/colab/batch_bot.py plan                       # the PRs a batch would take now, and why the others wait
    python3 tools/colab/batch_bot.py run [--name 2026-11-18a] [--prs 601,605] [--max 5] [--no-comment] [--rounds 3]

`run`:
 1. takes at most --max (default 5, owner 2026-11-18: a small batch needs at most 3 bisect points, each a long rebuild; 0 = no
    cap) open PRs, oldest first, whose newest checker report (tools/colab/pr_checker.py, `<!-- prcheck <head> -->`) is on its current head and
    has no ❌ (a pin whose cause the PR names is a ⚠️, not a ❌), not returned to its agent since, not a draft, not Claude's own branch;
 2. merges them in number order onto origin/main as batch/<name> in its own tree (workspace/wt/batch): the shared tables by the union of
    both sides' rows (docs/STOPS.md, engine/CMakeLists.txt, engine/cmake/*.cmake, RESERVED.tsv, the hand lists), README from main
    (land.py recounts it); a PR that conflicts in code with the PRs before it waits for the next batch (no comment: it is not its fault);
 3. renumbers a stop fragment whose id another PR of the batch or main already took (S-old -> the next free id, also in that PR's own
    files: code comments, tests, docs);
 4. builds, runs each PR's own tests (its new TEST_CASEs, else PORT-N* / LOCAL-N* of its title), the base set (SUITE), the tool tests
    and the matrix diff; a failure is checked on main (red there too = not the batch's);
 5. bisects every new failure over the batch's merge points (each point is one more PR) to the first red point: that PR broke it;
 6. a moved pin (PINS) whose cause the culprit PR names is re-measured on the final tree and committed with the cause; any other culprit
    leaves the batch, gets a `<!-- returned -->` comment with the failing test and its first error lines (the dashboard and the checker
    read that mark), and the batch is merged again without it (at most --rounds times);
 7. pushes the green batch to origin/batch/<name> (the checker skips PRs already in a batch/ branch) and writes the report on the Drive,
    openbfme-runs/batch/<name>/report.md: the PRs in and out and why, renumbered stops, re-measured pins, the suite, and every PR's
    "check in game" lines for TOP20.
Claude reads the report, adds what needs judgement (the pin's `// pin:` line, the TOP20 entries) and lands. Nothing here touches main.

`loop` (owner 2026-11-18: "сделать бота постоянным циклом на sb2"; autostart.d/batch.sh starts it, watchdog.py keeps it running, the
dashboard's Colab 2 card shows it): every 15 minutes, from its installed copy /content/sandbox/batch_bot.py, it
  - waits while a `run` is going (a hand one too); waits STACK_AFTER_S (7 min) for Claude to land its last green batch, then runs the
    next one on top of that batch (`run --base origin/batch/<last>`, owner 2026-11-18): the unlanded batch's PRs stay claimed, and
    landing the newest batch lands the ones under it;
  - otherwise runs a batch (`run --name <date><next letter>`, log on the Drive next to the reports) when the checker has --min (3) green
    PRs, or at least one and --idle-hours (2) passed since the last batch;
  - after a batch that did not come out green waits --idle-hours before the next (a red main must not rebuild every 15 minutes);
  - writes one line per round to /content/batch_loop.log (the watchdog's activity and the dashboard's log tail) and restarts on
    origin/main's newer copy of itself between rounds.
"""
from __future__ import annotations

import argparse
import concurrent.futures as cf
import datetime as dt
import json
import os
import re
import subprocess
import sys
import time
from pathlib import Path

REPO = Path(os.environ.get("BATCH_REPO", "/content/sandbox/openbfme-godot"))
WT = Path(os.environ.get("BATCH_TREE", str(REPO / "workspace" / "wt" / "batch")))
RUNS = Path(os.environ.get("BATCH_RUNS", "/content/drive/MyDrive/openbfme-runs/batch"))
GH_REPO = "Gleb302596555/openbfme-godot"
AUTHOR = ["-c", "user.name=Gleb302596555", "-c", "user.email=Gleb302596555@users.noreply.github.com"]
# what a batch is merged onto: origin/main, or the last green batch Claude has not landed yet (`run --base`, owner 2026-11-18: the loop
# waited an hour for a landing; after STACK_AFTER_S the next batch goes on top of the unlanded one, and landing the newest lands both)
BASE = "origin/main"
STACK_AFTER_S = 7 * 60

# the base set every batch runs (the hand batches 17a-18a): (doctest -tc pattern, short name)
SUITE = [("perf1*", "perf1"), ("uber1*", "uber1"), ("skirmish?ai?vs?ai*", "aivsai"), ("aiexpand1*", "aiexpand1"),
         ("save4a*", "save4a"), ("save4b*", "save4b"), ("save4c*", "save4c"), ("script3*", "script3"), ("script2?angmar*", "angmar"),
         ("module?coverage*", "modcov"), ("modgap1*", "modgap1"), ("modgap2*", "modgap2"), ("mod1*", "mod1"),
         ("facta?stops*", "factastops"), ("factb?spell?book*", "factbspell"), ("FACT-A?spell?book*", "factaspell"),
         ("factb?units*", "factbunits"), ("factc?units*", "factcunits"), ("ENNORATH?scenario*", "scenario"), ("mapobj*", "mapobj"),
         ("move?retail*", "move"), ("LOCAL-*", "locals"), ("PORT-1*", "ports1"), ("PORT-2*", "ports2"), ("PORT-3*", "ports3"),
         ("PORT-4*", "ports4"), ("PORT-5*", "ports5"), ("PORT-6*", "ports6"), ("PORT-7*", "ports7"), ("PORT-8*", "ports8"),
         ("PORT-9*", "ports9")]
PINS = {"perf1*": "OPENBFME_PERF1_REGEN", "uber1*": "OPENBFME_UBER1_REGEN"}
UNION = {"docs/STOPS.md", "engine/CMakeLists.txt", "docs/stops.d/RESERVED.tsv", "docs/ennorath/NIGHT_WATCH.md", "docs/ennorath/PROBLEMS.md",
         "docs/ennorath/TOP20.md", "docs/ennorath/PR_CANDIDATES.md", "docs/ennorath/MODULE_GAPS.md", "docs/ennorath/TEST_BASELINE.md"}
RETURNED = re.compile(r"<!-- returned -->|не принят", re.I)
# pr_checker.py's mark (2026-11-18): the base tests the PR moves (their values differ from main's); empty = a quiet PR
MOVES = re.compile(r"<!-- prcheck-moves ([^ ]*) ?-->")
FRAGMENT = re.compile(r"^docs/stops\.d/S-(\d+)\.md$")


def sh(cmd: list[str], cwd: Path = WT, check: bool = False, env: dict | None = None, timeout: int | None = None) -> subprocess.CompletedProcess:
    done = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, encoding="utf-8", errors="replace", env=env or agent_env(),
                          timeout=timeout)
    if check and done.returncode:
        raise RuntimeError(f"{' '.join(cmd)}: {(done.stdout + done.stderr)[-1500:]}")
    return done


def git(*args: str, check: bool = True) -> str:
    return sh(["git", *args], check=check).stdout


def token() -> str:
    """The sandbox's GitHub token, wherever this sandbox keeps it (never printed): $GH_TOKEN, /root/.gh_token (the git credential helper's
    file), ~/.git-credentials (the PR checker's sandbox)."""
    if os.environ.get("GH_TOKEN"):
        return os.environ["GH_TOKEN"]
    f = Path("/root/.gh_token")
    if f.exists():
        return f.read_text().strip()
    cred = Path.home() / ".git-credentials"
    if cred.exists():
        for line in cred.read_text().splitlines():
            m = re.match(r"https://[^:]+:([^@]+)@github\.com", line)
            if m:
                return m.group(1)
    raise SystemExit("batch bot: no GitHub token on this sandbox (GH_TOKEN, /root/.gh_token, ~/.git-credentials)")


def gh(*args: str) -> str:
    return sh(["gh", *args, "-R", GH_REPO], cwd=REPO, check=True, env=dict(agent_env(), GH_TOKEN=token())).stdout


_ENV: dict | None = None


def agent_env() -> dict:
    global _ENV
    if _ENV is None:
        # the game paths of the tests live in /root/.agent_env; every command of the bot runs with it
        out = subprocess.run(["bash", "-c", "source /root/.agent_env 2>/dev/null; env -0"], capture_output=True, text=True).stdout
        _ENV = dict(kv.split("=", 1) for kv in out.split("\0") if "=" in kv) or dict(os.environ)
    return _ENV


STATUS_DIR = Path(os.environ.get("OPENBFME_STATUS_DIR", "/content/status"))


def status(key: str, text: str) -> None:
    """What this bot does right now, one line in /content/status/<key>.txt (owner 2026-11-18: the dashboard says what each bot is busy
    with); watchdog.py puts every such line into the sandbox's heartbeat. Never fails the bot."""
    try:
        STATUS_DIR.mkdir(parents=True, exist_ok=True)
        (STATUS_DIR / f"{key}.txt").write_text(text[:300] + "\n", encoding="utf-8")
    except OSError:
        pass


RUN_KEY = ""   # set by `run`: its log lines are its status


def log(msg: str) -> None:
    print(f"[{time.strftime('%H:%M:%S')}] {msg}", flush=True)
    if RUN_KEY:
        status(RUN_KEY, f"партия {RUN_KEY.removeprefix('batch-')}: {msg}")


# ---------------------------------------------------------------- 1. candidates

def verdict(p: dict) -> str:
    """'green' or why the PR waits."""
    if p["isDraft"]:
        return "draft"
    if p["headRefName"].startswith(("claude/", "agent/claude/", "up/", "batch/")):
        return "Claude's own / not an agent PR"
    if p["headRefName"].startswith("agent/local/"):   # owner 2026-11-19: a local station's PR carries much at once; Claude lands it into main directly
        return "Local: Claude переносит в main напрямую, без партии"
    head = p["headRefOid"]
    extra = json.loads(gh("pr", "view", str(p["number"]), "--json", "comments,commits"))
    last_commit = max((c.get("committedDate") or "" for c in extra.get("commits") or []), default="")
    checks = [c for c in extra.get("comments") or [] if (c.get("body") or "").startswith(f"<!-- prcheck {head}")]
    real = [c for c in checks if "⏸️" not in c["body"]]
    if any(RETURNED.search(c.get("body") or "") and not c["body"].startswith("<!-- prcheck") and (c.get("createdAt") or "") > last_commit
           for c in extra.get("comments") or []):
        return "returned to its agent (no commit since)"
    # A PR from another account's fork (a trusted person's agent, owner 2026-11-17) never runs on the owner's sandboxes before review: GitHub's
    # runner checks it (.github/workflows/fork-pr.yml, the job FORK_CHECK), the PR checker leaves it alone. A batch builds and runs its code
    # as root and landing makes it the owner's code, so it waits for that check's success on this head and for the owner's comment
    # "<!-- reviewed <head> -->" (the owner or Claude for them read the whole diff). foreign() fails closed.
    if foreign(p):
        owner = GH_REPO.split("/")[0]
        if p.get("files") is not None and any(f["path"].startswith((".github/", "tools/ci/")) for f in p["files"]):
            return "from another account's fork and it changes .github/ or tools/ci/ (its own check would not count)"
        if fork_check(head) != "success":
            return f"from another account's fork: GitHub's check '{FORK_CHECK}' is not green on this head ({fork_check(head) or 'not run'})"
        if not any((c.get("author") or {}).get("login") == owner and (c.get("body") or "").startswith(f"<!-- reviewed {head}")
                   for c in extra.get("comments") or []):
            return f"from another account's fork: its code is not reviewed yet (the owner's comment <!-- reviewed {head[:8]}… --> after reading the diff)"
        return "green"
    if not real:
        return "not checked on its current head yet"
    if "❌" in real[-1]["body"]:
        return "red in the checker's report"
    p["_report"] = real[-1]["body"]  # its named pins attribute a moved pin without a bisect (checker_pins)
    m = MOVES.search(real[-1]["body"])
    p["_moves"] = [t for t in m.group(1).split(",") if t] if m else None   # None: checked before the mark, treated as quiet
    return "green"


FORK_CHECK = "Fork PR: build and own tests"


def foreign(p: dict) -> bool:
    """True unless GitHub says the PR's branch lives in this repository (as pr_checker.foreign: a missing field counts as foreign)."""
    return p.get("isCrossRepository") is not False or (p.get("headRepositoryOwner") or {}).get("login") != GH_REPO.split("/")[0]


def fork_check(head: str) -> str:
    """The conclusion of GitHub's fork check on this commit ('success', 'failure', ...), 'running', or '' when it has not run."""
    name = FORK_CHECK.replace(" ", "%20").replace(":", "%3A")
    runs = json.loads(sh(["gh", "api", f"repos/{GH_REPO}/commits/{head}/check-runs?check_name={name}"], cwd=REPO, check=True,
                         env=dict(agent_env(), GH_TOKEN=token())).stdout)
    runs = runs.get("check_runs") or []
    if not runs:
        return ""
    last = max(runs, key=lambda r: r.get("started_at") or "")
    return last.get("conclusion") or "running"


MAX_PRS = 5   # owner 2026-11-18: "брать по 5 пр максимум", fewer merge points for a bisect to walk


CLAIM_HOURS = 12   # a claim of a run that never wrote its report (the runtime died) stops counting after this


def claimed(own: str = "") -> dict[int, str]:
    """PR number -> the batch that holds it (owner 2026-11-18: batches run on two sandboxes at once). Every run writes
    RUNS/<name>/claim.json with its PRs at its start; the claim holds while the run goes (no report.md yet, younger than CLAIM_HOURS) and
    while its green branch origin/batch/<name> is not in origin/main. A red run (report, no branch) or a landed one frees its PRs."""
    held: dict[int, str] = {}
    for f in RUNS.glob("*/claim.json") if RUNS.exists() else []:
        name = f.parent.name
        if name == own:
            continue
        try:
            prs = json.loads(f.read_text()).get("prs") or []
        except (OSError, ValueError):
            continue
        branch = f"refs/remotes/origin/batch/{name}"
        if sh(["git", "rev-parse", "-q", "--verify", branch], cwd=REPO).returncode == 0:
            hold = sh(["git", "merge-base", "--is-ancestor", branch, "refs/remotes/origin/main"], cwd=REPO).returncode != 0
        else:
            hold = not (f.parent / "report.md").exists() and time.time() - f.stat().st_mtime < CLAIM_HOURS * 3600
        if hold:
            held.update({int(n): name for n in prs})
    return held


def candidates(only: list[int] | None, cap: int = MAX_PRS, own: str = "") -> tuple[list[dict], list[tuple[dict, str]]]:
    prs = json.loads(gh("pr", "list", "--state", "open", "--limit", "100", "--json",
                        "number,title,body,headRefOid,headRefName,isDraft,author,isCrossRepository,headRepositoryOwner,files"))
    prs = sorted((p for p in prs if not only or p["number"] in only), key=lambda p: p["number"])
    held = claimed(own)
    take, wait = [], []
    for p in prs:
        why = f"уже в партии {held[p['number']]}" if p["number"] in held else verdict(p)
        (take.append(p) if why == "green" else wait.append((p, why)))
    take, held_back = split_movers(take)
    wait = held_back + wait
    if cap > 0 and len(take) > cap:   # the oldest green PRs go first; the rest wait for the next batch, with no comment on them
        wait = [(p, f"следующая партия (в партии не больше {cap} PR)") for p in take[cap:]] + wait
        take = take[:cap]
    return take, wait


def split_movers(green: list[dict]) -> tuple[list[dict], list[tuple[dict, str]]]:
    """Owner 2026-11-18: the bisects of moved hash tests made the batches slow (18b: 6.5 hours for 15 PRs). A PR the checker saw move a base
    test (pr_checker.py's prcheck-moves mark) goes alone: no bisect, its pins are re-measured on it; the quiet ones go together. The oldest
    green PR decides which kind of batch comes next, so neither kind waits for ever."""
    if not green:
        return [], []
    if green[0].get("_moves"):
        return [green[0]], [(p, f"следующая партия: #{green[0]['number']} сдвигает хеш-тесты и идёт один") for p in green[1:]]
    quiet = [p for p in green if not p.get("_moves")]
    return quiet, [(p, f"сдвигает хеш-тесты ({', '.join(p['_moves'])}): пойдёт отдельной партией") for p in green if p.get("_moves")]


def solo_mover(take: list[dict]) -> bool:
    return len(take) == 1 and bool(take[0].get("_moves"))


# ---------------------------------------------------------------- 2-3. merging, stops

def ensure_tree() -> None:
    RUNS.mkdir(parents=True, exist_ok=True)
    if not (WT / ".git").exists():
        sh(["git", "worktree", "prune"], cwd=REPO)
        sh(["git", "worktree", "add", "--detach", str(WT), BASE], cwd=REPO, check=True)
    # docs/STOPS.md merges by stop id (tools/stops_merge.py, .gitattributes merge=stops; owner 2026-11-19): a conflict left after it is real
    sh(["git", "config", "merge.stops.driver", "python3 tools/stops_merge.py %O %A %B"], cwd=REPO)


def pr_files(n: int) -> list[str]:
    return git("diff", "--name-only", f"{BASE}...refs/batch/{n}").split()


def next_free_id() -> int:
    out = sh([sys.executable, "tools/fragments.py", "next"], check=True).stdout.strip()
    return int(out.removeprefix("S-"))


def pr_added_lines(n: int, f: str) -> list[str]:
    """The lines PR n adds to file f against the batch's base (the `+` side of its diff, without the header)."""
    out = []
    for line in git("diff", f"{BASE}...refs/batch/{n}", "--", f).split("\n"):
        if line.startswith("+") and not line.startswith("+++"):
            out.append(line[1:].rstrip("\r"))
    return out


def renumber(n: int, old: int, content: str, why: str, renumbered: list[str]) -> None:
    """Writes PR n's stop fragment under the next free id and moves its mentions in the PR's own files."""
    new = next_free_id()
    (WT / f"docs/stops.d/S-{new}.md").write_text(content.replace(f"S-{old}", f"S-{new}"), encoding="utf-8")
    git("add", f"docs/stops.d/S-{new}.md")
    for f in pr_files(n):
        p = WT / f
        if FRAGMENT.match(f) or not p.is_file():
            continue
        try:
            text = p.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            continue
        if not re.search(rf"\bS-{old}\b", text):
            continue
        if f in UNION:
            # a shared table: only the lines this PR added, never another stop's row (2026-11-18h / i: main already had a row S-7493, #654's
            # fragment S-7493 went to S-7507, and the global replace renamed main's row too: "S-7507 is already a row")
            added = set(pr_added_lines(n, f))
            lines = text.split("\n")
            for i, line in enumerate(lines):
                if line in added and not line.startswith(f"| S-{old} |"):
                    lines[i] = re.sub(rf"\bS-{old}\b", f"S-{new}", line)
            new_text = "\n".join(lines)
        else:
            new_text = re.sub(rf"\bS-{old}\b", f"S-{new}", text)
        if new_text != text:
            p.write_text(new_text, encoding="utf-8")
            git("add", f)
    renumbered.append(f"#{n}: S-{old} -> S-{new} ({why})")


STOP_ROW = re.compile(r"^\| S-(\d+) \|")


def dedupe_rows(merged: str, base: str) -> str | None:
    """A union merge keeps both versions of a stop row one side edited (2026-11-19e: Port 271 narrowed S-705, main's old row stayed beside it
    and tools/fragments.py collect refused the landing). Of two rows with one id, the one equal to the base's is the old one and goes; when
    neither equals it (both sides edited the row) it is a real conflict: None."""
    old = {m.group(1): line for line in base.splitlines() if (m := STOP_ROW.match(line))}
    lines = merged.splitlines(keepends=True)
    seen: dict[str, list[int]] = {}
    for i, line in enumerate(lines):
        if m := STOP_ROW.match(line):
            seen.setdefault(m.group(1), []).append(i)
    drop = set()
    for sid, idx in seen.items():
        if len(idx) < 2:
            continue
        stale = [i for i in idx if lines[i].rstrip("\r\n") == old.get(sid)]
        if len(idx) != 2 or len(stale) != 1:
            return None
        drop.add(stale[0])
    return "".join(line for i, line in enumerate(lines) if i not in drop)


def resolve(n: int, pending: list[tuple[int, str]]) -> list[str]:
    """Resolves a merge's conflicts in shared tables; returns the files it could not resolve (code)."""
    left = []
    for f in git("diff", "--name-only", "--diff-filter=U").split():
        m = FRAGMENT.match(f)
        if f == "README.md":
            git("checkout", "-q", "--ours", "--", f)
        elif m:  # add/add of the same stop id: the earlier PR keeps it, this PR's fragment is renumbered after the merge commit
            pending.append((int(m.group(1)), sh(["git", "show", f":3:{f}"]).stdout))
            git("checkout", "-q", "--ours", "--", f)
        elif f == "docs/STOPS.md":  # tools/stops_merge.py (the merge driver) already merged it by stop id: what is left is a real conflict
            left.append(f)
            continue
        elif f in UNION or (f.startswith("engine/cmake/") and f.endswith(".cmake")):
            parts = [sh(["git", "show", f":{k}:{f}"]).stdout for k in (2, 1, 3)]
            tmp = [WT.parent / f".batch-merge{k}" for k in range(3)]
            for t, text in zip(tmp, parts):
                t.write_text(text, encoding="utf-8")
            sh(["git", "merge-file", "--union", *map(str, tmp)])
            merged = tmp[0].read_text(encoding="utf-8")
            if f == "docs/STOPS.md":
                merged = dedupe_rows(merged, parts[1])
            if merged is None:  # both sides edited one stop row: a real conflict
                for t in tmp:
                    t.unlink()
                left.append(f)
                continue
            (WT / f).write_text(merged, encoding="utf-8")
            for t in tmp:
                t.unlink()
        else:
            left.append(f)
            continue
        git("add", f)
    return left


def merge_batch(name: str, prs: list[dict], out: list[int]) -> tuple[list[dict], list[tuple[dict, str]], list[str]]:
    """batch/<name> = origin/main + the PRs not in `out`; returns (merged, waiting for a conflict, renumbered stops)."""
    git("fetch", "-q", "origin")
    for p in prs:
        git("fetch", "-q", "origin", f"+pull/{p['number']}/head:refs/batch/{p['number']}")
    git("checkout", "-q", "-f", "-B", f"batch/{name}", BASE)
    git("clean", "-fdq", "docs", "engine/tests")
    merged, conflicted, renumbered = [], [], []
    for p in prs:
        n = p["number"]
        if n in out:
            continue
        msg = f"Merge PR #{n}: {p['title'][:100]}"
        done = sh(["git", *AUTHOR, "merge", "-q", "--no-ff", "--no-edit", "--no-verify", "-m", msg, f"refs/batch/{n}"])
        pending: list[tuple[int, str]] = []
        if done.returncode:
            left = resolve(n, pending)
            if left:
                git("merge", "--abort", check=False)
                conflicted.append((p, "conflicts in " + ", ".join(left[:6]) + " with the PRs merged before it"))
                continue
            git(*AUTHOR, "commit", "-q", "--no-verify", "--no-edit")
        merged.append(p)
        for old, content in pending:
            renumber(n, old, content, "the id an earlier PR of the batch took", renumbered)
        if pending:
            git(*AUTHOR, "commit", "-q", "--no-verify", "-m", f"Batch {name}: #{n}'s stop renumbered (the id an earlier PR took)")
    # A PR that conflicts with the base itself, not with an earlier PR of the batch, would be taken by every next batch and leave again
    # (2026-11-19a .. d: #579 #615 #629 #658, checked on an old main, filled the slots for 8 hours and the green PRs behind them waited):
    # it is marked BASE and goes back to its agent (left_batch).
    if conflicted:
        here = git("rev-parse", "HEAD").strip()
        for i, (p, why) in enumerate(conflicted):
            git("checkout", "-q", "-f", "--detach", BASE)
            tried = sh(["git", *AUTHOR, "merge", "-q", "--no-ff", "--no-commit", "--no-verify", f"refs/batch/{p['number']}"])
            if tried.returncode and resolve(p["number"], []):
                conflicted[i] = (p, BASE_CONFLICT + why)
            git("merge", "--abort", check=False)
        git("checkout", "-q", "-f", f"batch/{name}")
        git("reset", "-q", "--hard", here)
    # a fragment whose id main's table already has (taken since the PR started)
    owner = {f: p["number"] for p in merged for f in pr_files(p["number"]) if FRAGMENT.match(f)}
    taken = set(int(i) for i in re.findall(r"^\| S-(\d+) \|", (WT / "docs/STOPS.md").read_text(encoding="utf-8"), re.M))
    moved = False
    for f, n in sorted(owner.items()):
        sid = int(FRAGMENT.match(f).group(1))
        if sid in taken and (WT / f).exists():
            content = (WT / f).read_text(encoding="utf-8")
            git("rm", "-q", f)
            renumber(n, sid, content, "already a row of docs/STOPS.md", renumbered)
            moved = True
    if moved:
        git(*AUTHOR, "commit", "-q", "--no-verify", "-m", f"Batch {name}: stops renumbered whose id main already has")
    check = sh([sys.executable, "tools/fragments.py", "check"])
    if check.returncode:
        raise RuntimeError(f"fragments still broken after the merges:\n{check.stdout}")
    return merged, conflicted, renumbered


# ---------------------------------------------------------------- 4. build and tests

def build(tag: str, out_dir: Path) -> tuple[bool, str]:
    if RUN_KEY:
        status(RUN_KEY, f"партия {RUN_KEY.removeprefix('batch-')}: сборка {tag}")
    done = sh(["bash", "tools/colab/build_tests.sh"], timeout=4 * 3600)
    text = done.stdout + done.stderr
    (out_dir / f"build-{tag}.log").write_text(text, encoding="utf-8")
    if done.returncode:
        return False, "\n".join([l for l in text.splitlines() if re.search(r"\berror\b", l)][:12]) or text[-2000:]
    return True, ""


def first_errors(out: str) -> str:
    lines = out.splitlines()
    for i, l in enumerate(lines):
        if re.search(r"(\.cpp|\.h):\d+: (ERROR|FATAL)|SIGSEGV|CRASH|timeout", l):
            return "\n".join(x[:240] for x in lines[i:i + 6])
    return "\n".join(l[:240] for l in lines if re.search(r"ERROR|FATAL", l))[:1200]


def run_test(tc: str, env: dict | None = None) -> tuple[str, str]:
    """('pass' | 'fail' | 'none', first error lines)."""
    try:
        done = sh([str(WT / "engine/build/openbfme_tests"), f"-tc={tc}"], env=env or agent_env(), timeout=3600)
    except subprocess.TimeoutExpired:
        return "fail", "timeout 60 min"
    out = done.stdout + done.stderr
    m = re.search(r"test cases:\s*(\d+)\s*\|\s*(\d+) passed\s*\|\s*(\d+) failed", out)
    if not m or m.group(1) == "0":
        return "none", ""
    return ("fail" if done.returncode or m.group(3) != "0" else "pass"), first_errors(out)


def run_tests(patterns: list[str]) -> dict[str, tuple[str, str]]:
    with cf.ThreadPoolExecutor(max(2, min(8, (os.cpu_count() or 4)))) as pool:
        return dict(zip(patterns, pool.map(run_test, patterns)))


def own_tests(p: dict) -> list[str]:
    diff = git("diff", f"{BASE}...refs/batch/{p['number']}", "--", "engine/tests")
    pats = {re.split(r"[:\s]", name, maxsplit=1)[0] + "*" for name in re.findall(r'^\+\s*TEST_CASE\("([^"]+)"', diff, re.M)}
    m = re.match(r"(Port|Local)\s+(\d+)", p["title"], re.I)
    if m and not pats:
        pats.add(f"{m.group(1).upper()}-{m.group(2)}*")
    return sorted(pats)


def suite(merged: list[dict]) -> tuple[list[str], dict[str, int]]:
    """The patterns to run and whose own test each is."""
    own: dict[str, int] = {}
    for p in merged:
        for t in own_tests(p):
            own.setdefault(t, p["number"])
    return sorted(set(own) | {t for t, _ in SUITE}), own


def tool_checks(out_dir: Path) -> dict[str, tuple[str, str]]:
    res = {}
    py = sh([sys.executable, "-m", "pytest", "-q", "tools/re", "tools/mod", "tools/test_fragments.py"], timeout=1800)
    (out_dir / "pytest.log").write_text(py.stdout + py.stderr, encoding="utf-8")
    res["pytest tools"] = ("pass" if py.returncode == 0 else "fail", (py.stdout.strip().splitlines() or [""])[-1])
    md = sh([sys.executable, "tools/matrix_run.py", "diff", "--against", BASE], timeout=600)
    (out_dir / "matrix_diff.log").write_text(md.stdout + md.stderr, encoding="utf-8")
    res["matrix diff"] = ("pass" if md.returncode == 0 else "fail", (md.stdout + md.stderr).strip()[-600:])
    return res


# ---------------------------------------------------------------- 5. bisect

def merge_points() -> list[tuple[str, int | None]]:
    """The batch's first-parent commits after origin/main, oldest first, each with the PR it brings (renumber commits: their PR)."""
    out = []
    for line in git("log", "--first-parent", "--reverse", "--format=%H %s", f"{BASE}..HEAD").splitlines():
        sha, subject = line.split(" ", 1)
        m = re.search(r"#(\d+)", subject)
        out.append((sha, int(m.group(1)) if m else None))
    return out


def evaluate(sha: str, tests: list[str], out_dir: Path, cache: dict) -> dict[str, str]:
    if sha not in cache:
        git("checkout", "-q", "-f", "--detach", sha)
        ok, _ = build(sha[:8], out_dir)
        if ok:
            cache[sha] = {t: s for t, (s, _) in run_tests(tests).items()}
        else:  # a point that does not build counts as red for every test
            cache[sha] = {t: "build" for t in tests}
        log(f"point {sha[:8]}: " + ", ".join(f"{t}={s}" for t, s in cache[sha].items()))
    return cache[sha]


def bisect(failing: list[str], out_dir: Path, branch: str) -> dict[str, int | None]:
    """test -> the PR whose merge point first fails it (None: no single point)."""
    points = merge_points()
    cache: dict[str, dict[str, str]] = {}
    culprit: dict[str, int | None] = {}
    for t in failing:
        lo, hi = -1, len(points) - 1  # lo: the last point known green (-1 = origin/main), hi: the first known red (the tip)
        while hi - lo > 1:
            mid = (lo + hi) // 2
            st = evaluate(points[mid][0], failing, out_dir, cache)[t]
            if st in ("fail", "build"):
                hi = mid
            else:
                lo = mid
        culprit[t] = points[hi][1]
    git("checkout", "-q", "-f", branch)
    return culprit


NAMED_PIN = re.compile(r"⚠️ `([^`]+)` сдвигает пин, причина в PR названа")


def checker_pins(merged: list[dict]) -> dict[str, list[dict]]:
    """pin test -> the batch's PRs whose checker report says they move it with a named cause (pr_checker.py ran each PR alone on main with the
    batch's own set): such a pin needs no bisect, it is re-measured on the tip and credited to them (owner 2026-11-17, batch 18b)."""
    out: dict[str, list[dict]] = {}
    for p in merged:
        for t in NAMED_PIN.findall(p.get("_report") or ""):
            if t in PINS:
                out.setdefault(t, []).append(p)
    return out


def names_cause(p: dict, test: str) -> bool:
    """The checker's rule: the PR text or diff names the pin's test and a pin / cause word."""
    text = ((p.get("body") or "") + "\n" + git("diff", f"{BASE}...refs/batch/{p['number']}")).lower()
    return (re.search(r"\bpins?\b|пин|причин|cause|re-?measure|перемер", text) is not None
            and re.split(r"[*?]", test)[0].lower() in text)


def cause_line(p: dict, test: str) -> str:
    key = re.split(r"[*?]", test)[0].lower()
    for line in (p.get("body") or "").splitlines():
        if key in line.lower():
            return line.strip(" -*")[:300]
    return ""


def repin(test: str, ps: dict | list[dict], name: str) -> str:
    ps = ps if isinstance(ps, list) else [ps]
    p = ps[0]
    env = dict(agent_env(), **{PINS[test]: "1"})
    st, err = run_test(test, env)
    after, err2 = run_test(test)
    changed = [l[3:] for l in git("status", "--porcelain", "--", "engine/tests").splitlines()]
    if after != "pass" or not changed:
        return f"`{test}`: re-measure failed (regen {st}, after {after}: {err2[:200] or 'no data changed'})"
    git("add", "--", *changed)
    by = ", ".join(f"#{q['number']}" for q in ps)
    cause = "; ".join(c for c in (cause_line(q, test) for q in ps) if c)
    git(*AUTHOR, "commit", "-q", "--no-verify", "-m",
        f"Batch {name}: {test.rstrip('*')} re-measured, moved by {by}" + (f": {cause}" if cause else "") +
        "\n\nCo-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>")
    return f"`{test}` re-measured ({len(changed)} files), moved by {by}" + (f": {cause}" if cause else "")


# ---------------------------------------------------------------- 6-7. returns, report

BATCH_MARK = "<!-- batch {name} {head} -->"
LEFT_MARK = "<!-- batch-left {name} {head} -->"
BASE_CONFLICT = "BASE: "   # merge_batch's mark of a conflict with the batch's base (main), not with an earlier PR
EMPTY_RC = 3   # cmd_run's exit code for a batch whose PRs all left before the build


def taken_comment(name: str, p: dict, sandbox: str) -> str:
    """The PR is in batch `name`: the dashboard shows it "в батче" until a later check, return or this batch's left mark."""
    return (BATCH_MARK.format(name=name, head=p["headRefOid"]) + "\n"
            f"📦 **PR #{p['number']} взят в партию `{name}`** (песочница {sandbox}): тесты всей партии идут там. Зелёная партия переносится "
            "в main, и PR закроется сам; при поломке придёт комментарий с причиной.")


def left_batch(take: list[dict], rows: dict, out: dict) -> list[tuple[dict, str]]:
    """PRs that leave batch `name` without a return comment: a conflict with the PRs before them, or a batch that is not green."""
    conflicted = {p["number"]: why for p, why in rows.get("conflicted") or []}
    green = rows["verdict"].startswith("зелёный")
    res = []
    for p in take:
        if p["number"] in out:
            continue  # its return comment says why
        if p["number"] in conflicted and conflicted[p["number"]].startswith(BASE_CONFLICT):
            files = conflicted[p["number"]].split("conflicts in ", 1)[-1].split(" with ")[0]
            res.append((p, f"конфликтует с самим `main` ({files}), не с другими PR партии.\n\n<!-- returned --> Возвращён агенту: влей свежий "
                           "`origin/main` в ветку, разреши конфликт, прогони свои тесты и запушь — проверяльщик перепроверит, и PR снова "
                           "пойдёт в партию. Пока нового коммита нет, бот партии его не берёт"))
        elif p["number"] in conflicted:
            res.append((p, "конфликтует с PR, влитыми в партию раньше: пойдёт в следующую партию"))
        elif not green:
            res.append((p, f"партия не зелёная ({rows['verdict']}), но не из-за этого PR: снова ждёт партии"))
    return res


def left_comment(name: str, p: dict, why: str) -> str:
    return LEFT_MARK.format(name=name, head=p["headRefOid"]) + f"\n↩️ PR #{p['number']} вышел из партии `{name}`: {why}."


def return_comment(name: str, p: dict, tests: list[tuple[str, str]]) -> str:
    lines = [f"<!-- returned --> Не принят в батч {name}: бисект по точкам слияния (точка до этого PR зелёная, точка с ним красная, остальное "
             "то же) показал, что этот PR ломает то, что на main зелёное (`tools/colab/batch_bot.py`).", ""]
    for t, err in tests:
        pin = " — **сдвигает пин**, причина в PR не названа" if t in PINS else ""
        lines.append(f"❌ `{t}`{pin}" + (f"\n```\n{err[:1500]}\n```" if err else ""))
    lines += ["", "Что сделать: влей свежий `origin/main`, найди причину по RotWK (`rw.py dis`), исправь или, если тест ошибается, покажи "
              "адреса в PR (PROCESS 14); у пина назови причину в описании PR. Запушь в ту же ветку — проверяльщик перепроверит сам."]
    return "\n".join(lines)


def in_game(p: dict) -> str:
    body = (p.get("body") or "").splitlines()
    for i, l in enumerate(body):
        if re.search(r"видно в игре|in game|в игре:", l, re.I):
            out = [l.strip()]
            for x in body[i + 1:i + 8]:
                if re.match(r"\s*(\d+\.|#|\*\*[^*]+\*\*\s*$)", x) or "правка местная" in x.lower():
                    break
                if x.strip():
                    out.append(x.strip())
            return " ".join(out)[:700]
    return "(в PR нет строки «видно в игре»)"


def report(name: str, out_dir: Path, rows: dict) -> Path:
    r = rows
    lines = [f"# Батч {name}", "", f"Ветка `batch/{name}` на `{BASE}` {r['main'][:8]}, голова {r.get('head', '?')[:8]} "
             f"(`tools/colab/batch_bot.py`, {dt.datetime.utcnow():%Y-%m-%d %H:%M} UTC). Итог: **{r['verdict']}**.", ""]
    lines += [f"## Вошли ({len(r['merged'])})", ""] + [f"- #{p['number']} {p['title'][:110]}" for p in r["merged"]]
    if r["out"]:
        lines += ["", f"## Исключены и возвращены агентам ({len(r['out'])})", ""] + [f"- #{n}: {why}" for n, why in r["out"].items()]
    if r["conflicted"]:
        lines += ["", "## Ждут следующего батча (конфликт с PR батча, агенту ничего не пишется)", ""] + \
                 [f"- #{p['number']} {p['title'][:80]}: {why}" for p, why in r["conflicted"]]
    if r["wait"]:
        lines += ["", "## Не взяты", ""] + [f"- #{p['number']} {p['title'][:80]}: {why}" for p, why in r["wait"]]
    if r["renumbered"]:
        lines += ["", "## Перенумерованные стопы", ""] + [f"- {x}" for x in r["renumbered"]]
    if r["repinned"]:
        lines += ["", "## Перемеренные пины (Claude: строка `// pin:` с причиной в тесте пина)", ""] + [f"- {x}" for x in r["repinned"]]
    lines += ["", "## Тесты на голове батча", "", "| тест | итог | первая ошибка |", "|---|---|---|"]
    for t, (s, e) in sorted(r["results"].items()):
        mark = {"pass": "✅", "fail": "❌", "none": "—", "build": "❌ сборка"}.get(s, s)
        lines.append(f"| `{t}` | {mark}{' (красный и на main)' if t in r['main_red'] else ''} | {e.splitlines()[0][:120] if e else ''} |")
    lines += ["", "## Проверить в игре (для TOP20)", ""] + [f"- #{p['number']} {p['title'][:70]}: {in_game(p)}" for p in r["merged"]]
    if r["verdict"].startswith("зелёный"):
        lines += ["", "## Влить", "", f"На ПК в `workspace/wt/claude-station-0`: `python tools/land.py --batch {name}` (после записей TOP20 и строк `// pin:`)."]
    path = out_dir / "report.md"
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return path


# ---------------------------------------------------------------- loop

# The loop's state lives on the Drive beside the runs (owner 2026-11-19: sandbox 1 runs batch rounds when it has nothing to check, so both
# sandboxes must see the same unlanded green batch, the same red one and the same idle time); the old per-sandbox file seeds it once.
LOOP_STATE = Path(os.environ.get("BATCH_LOOP_STATE", str(RUNS / "loop.json")))
OLD_LOOP_STATE = Path("/content/sandbox/batch-loop.json")
# Owner 2026-11-19: "если на 2 песочнице нету готовых пр на сборку 30 мин то включался проверяльщик как на 1 песочнице" — after HELP_AFTER_S
# with no green PR and no batch, a loop round runs one PR checker cycle (pr_checker.py --once --no-helper); claims on the Drive keep the
# two checkers off the same PR (pr_checker.py claim_check), the batch claims keep the two batchers off the same PRs (claimed above).
HELP_AFTER_S = 30 * 60
INSTALL = Path("/content/sandbox")


def say(msg: str) -> None:
    print(f"{time.strftime('%F %T', time.gmtime())} UTC {msg}", flush=True)
    status("batch-loop", f"цикл: {msg}")


def is_run(argv: list[str]) -> bool:
    """`python3 <any>/batch_bot.py run ...` (a hand run too); the loop itself is `... loop` and never matches."""
    return len(argv) >= 3 and argv[0].endswith(("python3", "python")) and argv[1].endswith("batch_bot.py") and argv[2] == "run"


def run_going() -> bool:
    for d in Path("/proc").iterdir():
        if d.name.isdigit() and int(d.name) != os.getpid():
            try:
                argv = [a.decode("utf-8", "replace") for a in (d / "cmdline").read_bytes().split(b"\0") if a]
            except OSError:
                continue
            if is_run(argv):
                return True
    return False


def unlanded(name: str) -> bool:
    """The batch's green branch is on origin and not in origin/main yet (deleted by Claude = given up: no longer waited for)."""
    ref = f"refs/remotes/origin/batch/{name}"
    if sh(["git", "rev-parse", "-q", "--verify", ref], cwd=REPO).returncode:
        return False
    return sh(["git", "merge-base", "--is-ancestor", ref, "refs/remotes/origin/main"], cwd=REPO).returncode != 0


def next_name(day: str, used: set[str]) -> str:
    for c in "abcdefghijklmnopqrstuvwxyz":
        if day + c not in used:
            return day + c
    return day + "-" + time.strftime("%H%M", time.gmtime())


def should_start(green: int, since_s: float, minimum: int, idle_hours: float) -> bool:
    return green > 0 and (green >= minimum or since_s >= idle_hours * 3600)


def stack_base(last: str | None, pending: bool, waited_s: float) -> tuple[str | None, float]:
    """What the next batch is merged onto, or (None, seconds to wait): the last green batch still unlanded is waited for STACK_AFTER_S
    (Claude may land it at once), then the next batch goes on top of it (its PRs stay claimed, so they are not taken twice; landing the
    newest batch lands every batch under it). A landed or given-up batch (its branch deleted) is not pending: origin/main."""
    if not (last and pending):
        return "origin/main", 0.0
    if waited_s < STACK_AFTER_S:
        return None, STACK_AFTER_S - waited_s
    return f"origin/batch/{last}", 0.0


def helper_due(st: dict, now: float) -> bool:
    """No green PR and no batch for HELP_AFTER_S: time to check a PR instead (the round marks ready_at whenever it sees a green PR)."""
    return now - max(st.get("finished", 0), st.get("ready_at", 0)) >= HELP_AFTER_S   # shared state: a restart does not reset the wait


def install(name: str) -> Path:
    """The other bot's script, as origin/main has it, beside this one (the watchdog installs the same way)."""
    new = sh(["git", "show", f"origin/main:tools/colab/{name}"], cwd=REPO).stdout
    path = INSTALL / name
    if new and (not path.exists() or path.read_text(encoding="utf-8", errors="replace") != new):
        tmp = path.with_name(name + ".new")
        tmp.write_text(new, encoding="utf-8")
        os.replace(tmp, path)
    return path


def help_check() -> int:
    """One PR checker cycle on this sandbox (it checks at most one PR, then returns)."""
    env = dict(os.environ, PRCHECK_STATE=str(INSTALL / "prcheck-state.json"))
    with open("/content/prcheck.log", "ab") as log:
        return subprocess.run([sys.executable, str(install("pr_checker.py")), "--once", "--no-helper"], cwd=str(INSTALL), env=env,
                              stdout=log, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL).returncode


def load_loop_state() -> dict:
    for f in (LOOP_STATE, OLD_LOOP_STATE):
        try:
            if f.exists():
                return json.loads(f.read_text())
        except (OSError, ValueError):
            continue
    return {}


def loop_round(a, st: dict) -> str:
    """One round; returns the line for the log."""
    sh(["git", "fetch", "-q", "--prune", "origin"], cwd=REPO)
    if run_going():
        return "a batch run is going: waiting"
    last = st.get("green")
    base, wait_s = stack_base(last, bool(last) and unlanded(last), time.time() - st.get("finished", 0))
    if base is None:
        st["sleep"] = wait_s   # wake when the wait ends, not a whole round later
        return (f"batch/{last} is green and waits for Claude's landing (tools/land.py --batch {last}): "
                f"{int(wait_s / 60) + 1} min more, then the next batch goes on top of it")
    since = time.time() - st.get("finished", 0)
    if st.get("red") and since < a.idle_hours * 3600:
        return f"the last batch ({st.get('name')}) did not come out green: next try in {int((a.idle_hours * 3600 - since) / 60)} min"
    take, _ = candidates(None, a.max)
    if take:
        st["ready_at"] = time.time()
    if not solo_mover(take) and not should_start(len(take), since, a.min, a.idle_hours):   # a PR that moves pins goes at once
        return f"{len(take)} green PR(s): a batch starts at {a.min}, or at 1 after {a.idle_hours:g} h since the last batch"
    used = {q.name.split(".")[0] for q in RUNS.glob("2*")} if RUNS.exists() else set()
    # the batch branches on origin too: a run folder can be gone from Drive while its branch lives (2026-11-18: the loop named its batch
    # 2026-11-18a over the morning's batch/2026-11-18a)
    used |= {r.rsplit("/", 1)[-1] for r in sh(["git", "ls-remote", "--heads", "origin", "refs/heads/batch/*"], cwd=REPO).stdout.split()
             if r.startswith("refs/heads/batch/")}
    name = next_name(time.strftime("%Y-%m-%d", time.gmtime()), used)
    RUNS.mkdir(parents=True, exist_ok=True)
    say(f"batch {name}{'' if base == 'origin/main' else ' on top of ' + base}: {' '.join('#%d' % p['number'] for p in take)}")
    me = Path(__file__).resolve()
    with open(RUNS / f"{name}.log", "w") as out:
        rc = subprocess.run([sys.executable, str(me), "run", "--name", name, "--max", str(a.max), "--base", base], cwd=str(REPO),
                            stdout=out, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL).returncode
    # an empty batch (every PR left: conflicts, returns) tested nothing, so it does not hold the next one for idle_hours (2026-11-19d held
    # #681 #682 #683 for 2 h behind four PRs that conflicted with main)
    st.update(name=name, finished=time.time(), red=rc not in (0, EMPTY_RC))
    if rc == 0:
        st["green"] = name
        return f"batch {name} green: batch/{name} pushed, waits for Claude's landing"
    return f"batch {name} not green: {RUNS / name / 'report.md'}"


def self_update() -> None:
    """As pr_checker.py: the installed copy restarts on origin/main's newer one between rounds; a copy inside a git tree stays."""
    me = Path(__file__).resolve()
    if subprocess.run(["git", "-C", str(me.parent), "rev-parse", "--git-dir"], capture_output=True).returncode == 0:
        return
    new = subprocess.run(["git", "show", "origin/main:tools/colab/batch_bot.py"], cwd=REPO, capture_output=True).stdout
    if not new or new == me.read_bytes():
        return
    try:
        compile(new, str(me), "exec")
    except SyntaxError as e:
        say(f"self-update: origin/main's batch_bot.py does not compile ({e}); keeping the running copy")
        return
    tmp = me.with_name(me.name + ".new")
    tmp.write_bytes(new)
    os.replace(tmp, me)
    say("self-update: restarting on origin/main's batch_bot.py")
    os.execv(sys.executable, [sys.executable, str(me)] + sys.argv[1:])


def cmd_loop(a) -> int:
    while True:
        st = load_loop_state()
        try:
            line = loop_round(a, st)
        except Exception as e:  # a GitHub or Drive hiccup: the next round tries again
            line = f"round failed: {e}"[:300]
        RUNS.mkdir(parents=True, exist_ok=True)
        LOOP_STATE.write_text(json.dumps({k: v for k, v in st.items() if k != "sleep"}))
        say(line)
        if a.once:
            return 0
        if not a.no_helper and not run_going() and helper_due(st, time.time()):
            say(f"{HELP_AFTER_S // 60} мин без зелёных PR и партий: один цикл проверщика PR (pr_checker.py --once)")
            rc = help_check()
            say(f"цикл проверщика закончен (код {rc}); дальше партии")
            continue
        time.sleep(max(30.0, min(float(a.every), float(st.pop("sleep", a.every)))))
        sh(["git", "fetch", "-q", "origin"], cwd=REPO)
        self_update()


# ---------------------------------------------------------------- main

def cmd_plan(a) -> int:
    take, wait = candidates(a.prs, a.max)
    for p in take:
        print(f"take  #{p['number']} {p['title'][:90]}")
    for p, why in wait:
        print(f"wait  #{p['number']} {p['title'][:70]}: {why}")
    return 0


def cmd_run(a) -> int:
    global RUN_KEY, BASE
    BASE = a.base
    ensure_tree()
    name = a.name or dt.datetime.utcnow().strftime("%Y-%m-%d-%H%M")
    RUN_KEY = f"batch-{name}"
    out_dir = RUNS / name
    out_dir.mkdir(parents=True, exist_ok=True)
    take, wait = candidates(a.prs, a.max, own=name)
    (out_dir / "claim.json").write_text(json.dumps({"prs": [p["number"] for p in take], "sandbox": Path("/content/sandbox_id").read_text().strip()
                                                    if Path("/content/sandbox_id").exists() else "?", "started": time.time()}))
    log(f"batch {name}: {len(take)} green PRs: {' '.join('#%d' % p['number'] for p in take)}")
    sandbox = Path("/content/sandbox_id").read_text().strip() if Path("/content/sandbox_id").exists() else "?"
    if not a.no_comment:
        # the dashboard learns from GitHub only: without this mark a PR in a running batch stayed "✅ можно брать в батч" (owner 2026-11-18)
        for p in take:
            gh("pr", "comment", str(p["number"]), "--body", taken_comment(name, p, sandbox))
    try:
        return run_batch(a, name, take, wait, out_dir)
    except Exception as e:
        # a crash (2026-11-18a: its branch was checked out in another tree) left its PRs "в батче" and claimed for 12 hours: the report
        # releases the claim, the left marks put the PRs back among the green ones
        (out_dir / "report.md").write_text(f"# Партия {name}: упала\n\n```\n{e}\n```\n", encoding="utf-8")
        if not a.no_comment:
            for p in take:
                gh("pr", "comment", str(p["number"]), "--body", left_comment(name, p, f"партия упала ({str(e)[:200]}), не из-за этого PR: снова ждёт партии"))
        (STATUS_DIR / f"{RUN_KEY}.txt").unlink(missing_ok=True)
        raise


def load_main_results(path: Path) -> dict[str, str]:
    """A test's status on one origin/main ("pass" / "fail" / "build" / "none"), from a batch that ran it before; {} when none did."""
    try:
        return {str(k): str(v) for k, v in json.loads(path.read_text(encoding="utf-8")).items()}
    except (OSError, ValueError, AttributeError):
        return {}


def save_main_results(path: Path, results: dict[str, str]) -> None:
    merged = {**load_main_results(path), **results}   # another sandbox may have added its tests meanwhile
    tmp = path.with_suffix(f".{os.getpid()}.tmp")
    tmp.write_text(json.dumps(merged, sort_keys=True), encoding="utf-8")
    tmp.replace(path)


def run_batch(a, name: str, take: list[dict], wait: list, out_dir: Path) -> int:
    rows: dict = {"main": git("rev-parse", BASE).strip(), "wait": wait, "out": {}, "repinned": [], "results": {}, "main_red": set(),
                  "merged": [], "conflicted": [], "renumbered": [], "verdict": "нет PR"}
    if not take:
        report(name, out_dir, rows)
        return 0
    out: dict[int, str] = {}
    # the results of origin/main's own run, shared by every batch on the same main (2026-11-18: 18a on sb2 and 18d on sb3 each spent ~50
    # minutes building the same main 1910e252 for the same red pins)
    main_file = RUNS / f"main-{rows['main'][:12]}.json"
    main_cache: dict[str, str] = load_main_results(main_file)
    for rnd in range(1, a.rounds + 1):
        rows["verdict"] = ""
        merged, conflicted, renumbered = merge_batch(name, take, list(out))
        rows.update(merged=merged, conflicted=conflicted, renumbered=renumbered, head=git("rev-parse", "HEAD").strip())
        by_num = {p["number"]: p for p in merged}
        if not merged:
            rows["verdict"] = "пустой: все PR исключены"
            break
        patterns, own = suite(merged)
        ok, err = build("tip", out_dir)
        if not ok:
            results = {"build": ("build", err)}
        else:
            results = run_tests(patterns)
            results.update(tool_checks(out_dir))
        rows["results"] = results
        failing = [t for t, (s, _) in results.items() if s in ("fail", "build") or (s == "none" and t in own)]
        log(f"round {rnd}: failing {failing}")
        if not failing:
            rows["verdict"] = "зелёный"
            break
        # red on main too = not the batch's (the tool checks and own tests have no main twin worth a build)
        doctests = [t for t in failing if t in dict(SUITE)]
        unknown = [t for t in doctests if t not in main_cache]
        if unknown:
            git("checkout", "-q", "-f", "--detach", BASE)
            ok_main, _ = build("main", out_dir)
            main_cache.update({t: s for t, (s, _) in run_tests(unknown).items()} if ok_main else {t: "build" for t in unknown})
            save_main_results(main_file, main_cache)
            git("checkout", "-q", "-f", f"batch/{name}")
        rows["main_red"] = {t for t in doctests if main_cache.get(t) in ("fail", "build")}
        excluded_now: dict[int, list[tuple[str, str]]] = {}
        # a PR's own test that runs nothing at the tip is not wired into openbfme_tests: that PR, no bisect needed
        for t in failing:
            if results[t][0] == "none" and t in own:
                excluded_now.setdefault(own[t], []).append((t, "ни одного теста по шаблону: тестовый файл не подключён к openbfme_tests "
                                                               "(engine/CMakeLists.txt / engine/cmake/*.cmake)?"))
        new = [t for t in failing if t not in rows["main_red"] and t not in ("pytest tools", "matrix diff", "build")
               and results[t][0] != "none"]
        tool_red = [t for t in failing if t in ("pytest tools", "matrix diff")]
        broken_build = "build" in failing
        if broken_build:
            new = [SUITE[0][0]]  # the tip does not build: bisect on one test, a point that does not build counts red
        if not new and not tool_red and not excluded_now:
            rows["verdict"] = "зелёный (красное только то, что красное и на main)"
            break
        if not new and not excluded_now:
            rows["verdict"] = "красный: инструменты (pytest / матрица) — смотреть руками"
            break
        # a pin the checker already saw moved by named PRs (each alone on main): re-measured on the tip, no bisect for it
        known = {t: ps for t, ps in checker_pins(merged).items() if t in new and not broken_build}
        if known:
            log("pins from the checker's reports: " + ", ".join(f"{t} <- {' '.join('#%d' % q['number'] for q in ps)}" for t, ps in known.items()))
        new = [t for t in new if t not in known]
        culprit = bisect(new, out_dir, f"batch/{name}") if new else {}
        log(f"bisect: {culprit}")
        repins: list[tuple[str, dict | list[dict]]] = list(known.items())
        for t, n in culprit.items():
            if n is None or n not in by_num:
                rows["verdict"] = f"красный: `{t}` не сводится к одному PR — смотреть руками"
                continue
            if broken_build:
                excluded_now.setdefault(n, []).append(("сборка", results["build"][1]))
            elif t in PINS and names_cause(by_num[n], t):
                repins.append((t, by_num[n]))
            else:
                excluded_now.setdefault(n, []).append((t, results.get(t, ("", ""))[1]))
        if excluded_now:
            for n, tests in excluded_now.items():
                out[n] = "ломает " + ", ".join(f"`{t}`" for t, _ in tests)
                if not a.no_comment:
                    gh("pr", "comment", str(n), "--body", return_comment(name, by_num[n], tests))
            rows["out"] = out
            continue  # merge again without them
        if rows["verdict"].startswith("красный"):
            break
        build("tip-repin", out_dir)  # the bisect left the build of its last point: the tip again before re-measuring
        for t, p in repins:
            rows["repinned"].append(repin(t, p, name))
        rerun = run_tests([t for t, _ in repins])
        rows["results"].update(rerun)
        rows["verdict"] = "зелёный" if all(s == "pass" for s, _ in rerun.values()) else "красный после перемера — смотреть руками"
        break
    else:
        rows["verdict"] = f"красный после {a.rounds} кругов — смотреть руками"
    rows["head"] = git("rev-parse", "HEAD").strip()
    if rows["verdict"].startswith("зелёный"):
        git("push", "-q", "-f", "origin", f"HEAD:refs/heads/batch/{name}")
    path = report(name, out_dir, rows)
    log(f"{rows['verdict']}: {path}")
    if not a.no_comment:
        for p, why in left_batch(take, rows, out):
            gh("pr", "comment", str(p["number"]), "--body", left_comment(name, p, why))
    (STATUS_DIR / f"{RUN_KEY}.txt").unlink(missing_ok=True)   # done: the loop's line (or nothing) says the rest
    if rows["verdict"].startswith("пустой"):
        return EMPTY_RC
    return 0 if rows["verdict"].startswith("зелёный") else 1


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    lp = sub.add_parser("loop")
    lp.add_argument("--max", type=int, default=MAX_PRS)
    lp.add_argument("--min", type=int, default=3, help="green PRs that start a batch at once")
    lp.add_argument("--idle-hours", type=float, default=2.0, help="after this long since the last batch, one green PR is enough")
    lp.add_argument("--every", type=int, default=900)
    lp.add_argument("--once", action="store_true")
    lp.add_argument("--no-helper", action="store_true", help="never run a PR checker cycle (the round pr_checker.py runs itself)")
    for c in ("plan", "run"):
        s = sub.add_parser(c)
        s.add_argument("--prs", type=lambda v: [int(x) for x in v.split(",") if x], default=None)
        s.add_argument("--max", type=int, default=MAX_PRS, help=f"at most this many green PRs per batch, oldest first (default {MAX_PRS}; 0 = all)")
        if c == "run":
            s.add_argument("--name")
            s.add_argument("--base", default="origin/main", help="merge onto this ref (the loop: an unlanded green batch, see stack_base)")
            s.add_argument("--rounds", type=int, default=3)
            s.add_argument("--no-comment", action="store_true", help="do not comment on the excluded PRs (a dry run)")
    a = ap.parse_args()
    return cmd_plan(a) if a.cmd == "plan" else cmd_run(a) if a.cmd == "run" else cmd_loop(a)


if __name__ == "__main__":
    sys.exit(main())
