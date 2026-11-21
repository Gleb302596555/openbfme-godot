#!/usr/bin/env python3
"""Per-PR checker on a Colab sandbox (owner, 2026-11-14): every open fork PR is built alone on today's main and tested,
before any batch, so a red test or a moved pin is pinned to its PR at once (PROCESS 7a).

    python3 tools/colab/pr_checker.py [--once] [--pr N] [--no-comment]

Runs from a clone on the sandbox (default /content/sandbox/openbfme-godot). For each open PR whose head was not
checked yet (state in /content/prcheck/state.json):
 1. tree /content/prcheck/wt (one reused worktree, its build dir stays, so builds are incremental + ccache):
    origin/main, then `git merge` of the PR head; a conflict is reported with its files;
 2. tools/fragments.py check, a stop id the PR adds that main already has, a STOPS / NIGHT_WATCH / README edit
    (should be fragments since 2026-11-14);
 3. tools/colab/build_tests.sh (all cores); a compile error ends the check;
 4. the PR's own tests (the prefixes of the TEST_CASEs it adds, e.g. `PORT-206*`) and the base set
    (BASE below) in parallel; a base test is compared with main's result for the same main commit (cached), so
    only what the PR changes is reported; perf1 / uber1 red where main is green = the PR moves a pin;
 5. open PRs that change the same files;
 6. a comment on the PR (marker <!-- prcheck -->) and /content/prcheck/reports/<pr>-<sha>.md.
 7. (2026-11-17) the field audit of the PR's tree beside its tests, against main's (once per main commit): fields the PR makes read,
    fields it stops reading, fields the task / PR names that stay unused; and the function map's view (map_section). Notes and
    warnings only: the verdict stays the tests'.
The GitHub token is the sandbox's git credential (~/.git-credentials), never printed.

A PR from another account's fork (a trusted person's agent, owner 2026-11-17) is never built or run on the owner's machines: GitHub's
runner checks it (.github/workflows/fork-pr.yml), the checker only notes that once per head (<!-- prcheck-foreign <head> -->, not a verdict),
and its retail tests run in the owner's batch after the code was read (batch_bot.py). foreign() fails closed: only a PR whose branch GitHub
says lives in this repository is the owner's.

Order and skips (owner 2026-11-17: the checker ran hours behind, re-checking what Claude had already taken):
 - reworked PRs (checked before) and never-checked ones in turn, each queue oldest head commit first (queue_order, 2026-11-19);
 - a PR whose head is already in a batch Claude is testing or landing (origin batch/*, land/*, agent/claude/station-0) is skipped;
 - a PR returned to its agent (a comment with <!-- returned --> or "не принят", newer than the PR's last commit) is skipped until a new
   commit arrives;
 - the base set (BASE) runs only when the PR changes the engine (engine/src, thirdparty, cmake, test data, godot/) or a base test's file:
   otherwise its results are main's by construction, and only the PR's own tests run.
The compiler cache works (direct-mode hits for every unchanged translation unit): a PR that edits a core header (Object.h, Player.h,
GameLogic.h) recompiles ~700 units because their content is new, which no cache avoids. Each report gives the build and test times.
"""
from __future__ import annotations

import argparse
import concurrent.futures as cf
import hashlib
import json
import os
import re
import subprocess
import sys
import time
from pathlib import Path

GH = "Gleb302596555/openbfme-godot"
REPO = Path(os.environ.get("PRCHECK_REPO", "/content/sandbox/openbfme-godot"))
HOME = Path(os.environ.get("PRCHECK_HOME", "/content/prcheck"))
WT = HOME / "wt"
STATE = Path(os.environ.get("PRCHECK_STATE", str(HOME / "state.json")))  # autostart keeps it in /content/sandbox (saved to Drive)
REPORTS = HOME / "reports"
# The batch's whole set (batch_bot.py SUITE, owner 2026-11-17: batch 18b spent three hours bisecting skirmish ai vs ai, aiexpand1 and script2
# angmar, which this checker did not run, down to PRs it had passed) plus the PlayerTemplate check; test_batch_bot.py keeps them equal.
# The tests run one per core in parallel: the longest one sets the time.
BASE = ["perf1*", "uber1*", "skirmish?ai?vs?ai*", "aiexpand1*", "save4a*", "save4b*", "save4c*", "script3*", "script2?angmar*",
        "module?coverage*", "modgap1*", "modgap2*", "mod1*", "facta?stops*", "factb?spell?book*", "FACT-A?spell?book*", "factb?units*",
        "factc?units*", "ENNORATH?scenario*", "mapobj*", "move?retail*", "LOCAL-*", "PORT-1*", "PORT-2*", "PORT-3*", "PORT-4*", "PORT-5*",
        "PORT-6*", "PORT-7*", "PORT-8*", "PORT-9*", "logic retail: the PlayerTemplate*"]
PINS = {"perf1*", "uber1*"}
SHARED = ("docs/STOPS.md", "docs/ennorath/NIGHT_WATCH.md", "README.md")
DOC_UNION = {"docs/STOPS.md", "docs/ennorath/NIGHT_WATCH.md", "docs/ennorath/PROBLEMS.md", "docs/ennorath/TOP20.md",
             "docs/ennorath/PR_CANDIDATES.md", "docs/ennorath/MODULE_GAPS.md", "docs/ennorath/TEST_BASELINE.md", "engine/CMakeLists.txt"}


SANDBOX = (Path("/content/sandbox_id").read_text().strip() if Path("/content/sandbox_id").exists() else "?")
# Owner 2026-11-19: the checker and the batcher help each other ("они должны быть синхронизированы, чтобы знали друг о друге и не делали
# одно и то же"). Sandbox 2's batch loop runs a checker cycle after 30 minutes without green PRs (batch_bot.py helper_due), and this
# checker runs a batch round when it has nothing to check (help_batch). A check claims its PR on the Drive first: the other sandbox's
# checker leaves a PR whose current head is claimed there, until the claim is CLAIM_S old.
CLAIMS = Path(os.environ.get("PRCHECK_CLAIMS", "/content/drive/MyDrive/openbfme-runs/prcheck/claims"))
CLAIM_S = 4 * 3600
CLAIM_SETTLE_S = 90   # the Drive carries a write to the other sandbox within about a minute
INSTALL = Path("/content/sandbox")


def sh(cmd: list[str] | str, cwd: Path = WT, timeout: int | None = None, env: dict | None = None) -> subprocess.CompletedProcess:
    return subprocess.run(cmd, cwd=cwd, shell=isinstance(cmd, str), capture_output=True, text=True,
                          encoding="utf-8", errors="replace", timeout=timeout, env=env)


def token() -> str:
    for line in Path.home().joinpath(".git-credentials").read_text().splitlines():
        m = re.match(r"https://[^:]+:([^@]+)@github\.com", line)
        if m:
            return m.group(1)
    raise SystemExit("prcheck: no github.com credential in ~/.git-credentials")


def gh(*args: str) -> str:
    env = dict(os.environ, GH_TOKEN=token())
    done = subprocess.run(["gh", *args, "-R", GH], capture_output=True, text=True, encoding="utf-8", errors="replace", env=env)
    if done.returncode:
        raise RuntimeError(f"gh {' '.join(args[:2])}: {done.stderr.strip()[:300]}")
    return done.stdout


STATUS_DIR = Path(os.environ.get("OPENBFME_STATUS_DIR", "/content/status"))


def status(key: str, text: str) -> None:
    """What this bot does right now, one line in /content/status/<key>.txt (owner 2026-11-18: the dashboard says what each bot is busy
    with); watchdog.py puts every such line into the sandbox's heartbeat. Never fails the bot."""
    try:
        STATUS_DIR.mkdir(parents=True, exist_ok=True)
        (STATUS_DIR / f"{key}.txt").write_text(text[:300] + "\n", encoding="utf-8")
    except OSError:
        pass


def load_state() -> dict:
    return json.loads(STATE.read_text()) if STATE.exists() else {"checked": {}, "main": {}}


def save_state(s: dict) -> None:
    STATE.write_text(json.dumps(s, indent=1))


def ensure_tree() -> None:
    HOME.mkdir(parents=True, exist_ok=True)
    REPORTS.mkdir(exist_ok=True)
    if not (WT / ".git").exists():
        sh(["git", "worktree", "prune"], cwd=REPO)  # a Colab restart leaves the old tree's registration behind
        sh(["git", "worktree", "add", "--detach", str(WT), "origin/main"], cwd=REPO).check_returncode()
    # docs/STOPS.md merges by stop id (tools/stops_merge.py, .gitattributes merge=stops; owner 2026-11-19: a line union doubled 67 rows)
    sh(["git", "config", "merge.stops.driver", "python3 tools/stops_merge.py %O %A %B"], cwd=REPO)


def build() -> tuple[bool, str]:
    # the compiler cache on the sandbox's disk, Drive's read-only behind it (build_tests.sh, OPENBFME_CCACHE_DIR)
    done = sh(["bash", "tools/colab/build_tests.sh"], timeout=3 * 3600, env=dict(os.environ, OPENBFME_CCACHE_DIR="/content/ccache-prcheck"))
    log = done.stdout + done.stderr
    (HOME / "build.log").write_text(log)
    if done.returncode:
        errs = [l for l in log.splitlines() if re.search(r"\berror\b", l)][:12]
        return False, "\n".join(errs) or log[-2000:]
    return True, ""


# What a test measured, not only its colour (owner 2026-11-18): a hash test red on main (aiexpand1, skirmish AI vs AI, save4b ...) that a PR
# moves stays red, so pass / fail alone cannot see the move. The signature is the failures' own lines (the assertion and its values, file
# paths and line numbers dropped): equal on main and on the PR = the PR does not move the test.
SIG_LINE = re.compile(r"ERROR:|values:|TEST CASE:|logged:|FATAL|SIGSEGV|CRASH")
SIG_PATH = re.compile(r"^\S*?\.(?:cpp|h|hpp)(?::\d+|\(\d+\))?:?\s*")


def signature(status: str, out: str) -> str:
    """'pass' / 'none', or a short hash of the failure lines of `out` (doctest's own output)."""
    if status != "fail":
        return status
    keep = [SIG_PATH.sub("", l.strip()) for l in out.splitlines() if SIG_LINE.search(l)]
    return "fail:" + hashlib.sha1("\n".join(keep).encode("utf-8", "replace")).hexdigest()[:12]


SIGS: dict[str, str] = {}   # pattern -> signature of its last run (run_test fills it; each pattern runs in one thread)


def moved_tests(res: dict[str, tuple[str, str]], base: dict[str, str], main_sigs: dict[str, str]) -> list[str]:
    """The base tests whose result on the PR differs from main's: by signature where both have one, else by status."""
    return [tc for tc in BASE if SIGS.get(tc, res[tc][0]) != main_sigs.get(tc, base.get(tc))]


MOVES = re.compile(r"<!-- prcheck-moves ([^ ]*) ?-->")


def moves_mark(moved: list[str]) -> str:
    """The comment's lines for batch_bot.py (the hidden mark) and the reader."""
    mark = f"<!-- prcheck-moves {','.join(moved)} -->"
    if moved:
        return mark + f"\n🧷 Сдвигает хеш-тесты {', '.join(f'`{t}`' for t in moved)} (их значения не как на main): в партию пойдёт один."
    return mark + "\n🔕 Базовый набор не сдвинут (значения как на main): пойдёт в общую партию."


def run_test(tc: str) -> tuple[str, str, str]:
    """(pattern, 'pass'|'fail'|'none', first error lines); its signature goes to SIGS[pattern]."""
    exe = WT / "engine/build/openbfme_tests"
    try:
        done = sh([str(exe), f"-tc={tc}"], timeout=2400, env=agent_env())
        out = done.stdout + done.stderr
    except subprocess.TimeoutExpired:
        SIGS[tc] = "fail:timeout"
        return tc, "fail", "timeout 40 min"
    m = re.search(r"test cases:\s*(\d+)\s*\|\s*(\d+) passed\s*\|\s*(\d+) failed", out)
    if not m or m.group(1) == "0":
        SIGS[tc] = "none"
        return tc, "none", ""
    errs = "\n".join([l[:220] for l in out.splitlines() if re.search(r"ERROR|FATAL|SIGSEGV|CRASH", l)][:4])
    st = "fail" if (done.returncode or m.group(3) != "0") else "pass"
    SIGS[tc] = signature(st, out)
    return tc, st, errs


_ENV: dict | None = None


def agent_env() -> dict:
    global _ENV
    if _ENV is None:
        out = sh(["bash", "-c", "source /root/.agent_env 2>/dev/null; env -0"], cwd=HOME).stdout
        _ENV = dict(kv.split("=", 1) for kv in out.split("\0") if "=" in kv)
    return _ENV


def run_set(patterns: list[str]) -> dict[str, tuple[str, str]]:
    workers = max(1, os.cpu_count() or 2)  # one process per core: each test is single-threaded, memory allows it (3-5 GB each, 50 GB)
    with cf.ThreadPoolExecutor(workers) as pool:
        return {tc: (st, err) for tc, st, err in pool.map(run_test, patterns)}


# Main's base results on demand (owner 2026-11-19): the checker used to build main and run the whole base set and the field audit after every
# landing that touched the engine, on each sandbox: 35-80 of a check's 70-80 minutes. Now the PR goes first. A base test whose PR result
# equals main's last known one (status and failure signature; a pinned test that passes is at its pin) is as on main; only a test that
# differs (a PR red where main was green, green where it was red, another failure) needs this main's own result, and main is built and only
# those tests run (the audit of main beside them). Results are kept per main commit here and on the Drive, so the other sandbox reuses them.
MAIN_DRIVE = Path(os.environ.get("PRCHECK_MAIN_DRIVE", "/content/drive/MyDrive/openbfme-runs/prcheck/main"))


def engine_moved(old: str, new: str) -> bool:
    moved = sh(["git", "diff", "--name-only", f"{old}..{new}"]).stdout.split()
    return not moved or any(f.startswith(("engine/", "godot/")) or f == "tools/mod/field_audit.py" for f in moved)


def main_known(state: dict, main_sha: str) -> dict[str, list[str]]:
    """{test: [status, signature]} measured on main_sha (by this sandbox or, through the Drive, the other one). A main that moved by docs /
    tools only keeps the previous commit's results."""
    cur = state.get("main") or {}
    if "results" in cur and "tests" not in cur:  # the state of the whole-baseline checker: its results become the last known ones
        state["main_last"] = {k: [v, (cur.get("sigs") or {}).get(k, v)] for k, v in cur["results"].items()}
        if cur.get("audit"):
            state["main_audit"] = [cur["audit"], cur.get("sha", "")]
        cur = {"sha": cur.get("sha"), "tests": {k: list(v) for k, v in state["main_last"].items()}}
    if cur.get("sha") != main_sha:
        old = cur.get("sha")
        keep = (cur.get("tests") or {}) if old and not engine_moved(old, main_sha) else {}
        cur = {"sha": main_sha, "tests": dict(keep)}
    try:
        shared = json.loads((MAIN_DRIVE / f"{main_sha}.json").read_text())
    except (OSError, ValueError):
        shared = {}
    for k, v in shared.items():
        cur["tests"].setdefault(k, v)
    state["main"] = cur
    return cur["tests"]


def remember_main(state: dict, main_sha: str, got: dict[str, list[str]]) -> None:
    state["main"]["tests"].update(got)
    state.setdefault("main_last", {}).update(got)
    save_state(state)
    try:  # shared with the other sandbox; merged with what it wrote meanwhile
        MAIN_DRIVE.mkdir(parents=True, exist_ok=True)
        path = MAIN_DRIVE / f"{main_sha}.json"
        try:
            have = json.loads(path.read_text())
        except (OSError, ValueError):
            have = {}
        have.update(got)
        tmp = path.with_suffix(f".{SANDBOX}.tmp")
        tmp.write_text(json.dumps(have, indent=1))
        os.replace(tmp, path)
    except OSError:
        pass


def main_needed(pr_res: dict[str, list[str]], known: dict, last: dict) -> tuple[list[str], dict[str, list[str]]]:
    """(the tests main must run, main's view of the rest): known on this main -> that; equal to main's last known result -> as on main;
    otherwise main runs it."""
    need, view = [], {}
    for tc, (st, sig) in pr_res.items():
        if tc in known:
            view[tc] = list(known[tc])
        elif tc in last and list(last[tc]) == [st, sig]:
            view[tc] = [st, sig]
        else:
            need.append(tc)
    return need, view


def measure_main(state: dict, main_sha: str, tests: list[str]) -> None:
    """Build main and run only `tests` on it, the field audit of main beside them."""
    sh(["git", "checkout", "-q", "-f", "--detach", main_sha]).check_returncode()
    status("prcheck", f"main {main_sha[:8]}: сборка и {len(tests)} наборов, которые у PR не как на main")
    ok, err = build()
    if not ok:
        raise RuntimeError(f"main {main_sha[:8]} does not build:\n{err}")
    with cf.ThreadPoolExecutor(1) as side:
        audit_job = side.submit(run_audit, HOME / "audit-main")
        res = run_set(tests)
        try:
            audit = audit_job.result()
        except Exception:
            audit = None
    if audit:
        state["main_audit"] = [str(audit), main_sha]
    remember_main(state, main_sha, {tc: [res[tc][0], SIGS.get(tc, res[tc][0])] for tc in tests})


# The field audit (owner 2026-11-17: the checker sees the nightly maps' tools too): tools/mod/field_audit.py on the built tree, the engine's
# module coverage from the test binary, the newest hand verdicts from the Drive. About 8 minutes; for a PR it runs beside the tests.
AUDIT_DRIVE = Path("/content/drive/MyDrive/openbfme-audit")
USED = {"used", "used-raw", "used-outside", "own-reader"}
UNUSED = {"parsed-unused", "not-read"}


def run_audit(out: Path) -> Path | None:
    if not (WT / "tools/mod/field_audit.py").exists():
        return None
    out.mkdir(parents=True, exist_ok=True)
    for old in out.glob("*"):
        old.unlink()
    env = agent_env()
    cov = out / "module-coverage.tsv"
    sh([str(WT / "engine/build/openbfme_tests"), "-tc=module coverage: every registry class*"], timeout=1200,
       env=dict(env, OPENBFME_MODULE_COVERAGE_OUT=str(cov)))
    manuals = sorted(AUDIT_DRIVE.glob("field-audit-v*/manual-check.tsv"), key=lambda p: int(re.sub(r"\D", "", p.parent.name) or 0))
    cmd = [sys.executable, "tools/mod/field_audit.py", "--base", env.get("ROTWK_INSTALL", ""), "--overlay",
           env.get("OPENBFME_ENNORATH_INSTALL", ""), "--coverage", str(cov), "--out", str(out)]
    if manuals:
        cmd += ["--manual", str(manuals[-1])]
    done = sh(cmd, timeout=1800)
    tsv = out / "field-audit.tsv"
    return tsv if done.returncode == 0 and tsv.exists() else None


def audit_rows(tsv: Path) -> dict[tuple[str, str, str], str]:
    import csv
    with tsv.open(encoding="utf-8", newline="") as s:
        return {(r["block"], r["owner"], r["field"]): r["status"] for r in csv.DictReader(s, delimiter="\t", quoting=csv.QUOTE_NONE)}


def audit_section(base_tsv: str, pr_tsv: Path | None, claim_text: str) -> list[str]:
    """What the PR does to the mod's fields: unused -> used (the port's evidence), used -> unused (a reader lost?), and fields the task or
    PR names that the audit still sees unused. Warnings and notes only: the verdict stays the tests' (a red line is never the audit's)."""
    if not pr_tsv or not base_tsv or not Path(base_tsv).exists():
        return ["ℹ️ Аудит полей не посчитан (нет аудита main или PR) — статусы полей не сверены."]
    was, now = audit_rows(Path(base_tsv)), audit_rows(pr_tsv)
    name = lambda k: f"`{k[0]}{'/' + k[1] if k[1] else ''} {k[2]}`"
    gained = sorted(k for k, st in now.items() if st in USED and was.get(k) in UNUSED)
    lost = sorted(k for k, st in now.items() if st in UNUSED and was.get(k) in USED)
    out = []
    if gained:
        out.append(f"📋 Аудит полей: PR переводит в «используется» {len(gained)}: " + ", ".join(map(name, gained[:15]))
                   + (" …" if len(gained) > 15 else ""))
    else:
        out.append("📋 Аудит полей: PR не переводит ни одно поле мода в «используется».")
    if lost:
        out.append(f"⚠️ Поля, которые на main читаются, а с PR — нет ({len(lost)}; потерян читатель?): " + ", ".join(map(name, lost[:15])))
    text = claim_text.lower()
    named = sorted(k for k, st in now.items() if st in UNUSED and len(k[2]) >= 6 and re.search(rf"\b{re.escape(k[2].lower())}\b", text)
                   and ((k[1].lower() in text) if k[1] else (k[0].lower() in text)))  # a module field needs its class named, not just "object"
    if named:
        out.append(f"⚠️ Названы в задаче или PR, но аудит по-прежнему видит их неиспользуемыми ({len(named)}): " + ", ".join(map(name, named[:15]))
                   + ". Если правка читает их — проверь, что аудит находит читателя; если задача не про них — всё в порядке.")
    return out


def own_tests(diff: str, title: str) -> list[str]:
    pats = set()
    for name in re.findall(r'^\+\s*TEST_CASE\("([^"]+)"', diff, re.M):
        pats.add(re.split(r"[:\s]", name, maxsplit=1)[0] + "*")
    m = re.match(r"(Port|Local)\s+(\d+)", title)
    if m and not pats:
        pats.add(f"{m.group(1).upper()}-{m.group(2)}*")
    return sorted(pats)


def own_test_missing(pr: dict, diff: str) -> list[str]:
    """tools/ci/own_test_check.py's rule on this PR (opened after the rule): the added test prefixes no text shows passing."""
    for d in (Path(__file__).resolve().parents[1] / "ci", WT / "tools" / "ci"):  # a checkout's own, else the checker tree's
        if (d / "own_test_check.py").exists() and str(d) not in sys.path:
            sys.path.insert(0, str(d))
    try:
        import own_test_check as ot
    except ImportError:
        return []
    if (pr.get("createdAt") or "") < ot.RULE_SINCE:
        return []
    texts = [pr.get("body") or ""]
    try:
        texts += [c.get("body") or "" for c in json.loads(gh("pr", "view", str(pr["number"]), "--json", "comments")).get("comments") or []]
        m = re.search(r"(?:closes|fixes|resolves)\s+#(\d+)", pr.get("body") or "", re.I)
        if m:
            texts += [c.get("body") or "" for c in json.loads(gh("issue", "view", m.group(1), "--json", "comments")).get("comments") or []]
    except Exception:
        pass
    return ot.missing(diff, [t for t in texts if not ot.BOT.match(t)])


def check_pr(pr: dict, state: dict, main_sha: str, open_prs: list[dict], comment: bool) -> str:
    n, sha = pr["number"], pr["headRefOid"]
    sh(["git", "fetch", "-q", "origin", f"+pull/{n}/head:refs/prcheck/{n}"]).check_returncode()
    sh(["git", "checkout", "-q", "-f", "--detach", main_sha]).check_returncode()
    status("prcheck", f"PR #{n} ({pr['title'][:60]}): вливаю в main {main_sha[:8]}")
    lines = [f"<!-- prcheck {sha} -->", f"**Проверка PR #{n}** на `main` {main_sha[:8]}, голова {sha[:8]} (песочница {SANDBOX}, `tools/colab/pr_checker.py`)", ""]
    merge = sh(["git", "-c", "user.name=prcheck", "-c", "user.email=prcheck@localhost", "merge", "--no-edit", "--no-verify", "-q", f"refs/prcheck/{n}"])
    if merge.returncode:
        files = sh(["git", "diff", "--name-only", "--diff-filter=U"]).stdout.split()
        code = [f for f in files if f not in DOC_UNION and f != "README.md"]
        if code or not files:
            sh(["git", "merge", "--abort"])
            lines += [f"❌ **Не вливается в main**: конфликт в {', '.join(f'`{f}`' for f in code) or merge.stderr[:300]}. Влей `origin/main` в свою ветку."]
            return finish(n, sha, lines, comment, state)
        for f in files:  # the shared tables: both sides' rows kept (as the batch merge does), README taken from main
            if f == "README.md":
                sh(["git", "checkout", "-q", "--ours", f])
            else:
                parts = [sh(["git", "show", f":{k}:{f}"]).stdout for k in (2, 1, 3)]
                tmp = [HOME / f"merge{k}" for k in range(3)]
                for t, text in zip(tmp, parts):
                    t.write_text(text, encoding="utf-8")
                sh(["git", "merge-file", "--union", *map(str, tmp)])
                (WT / f).write_text(tmp[0].read_text(encoding="utf-8"), encoding="utf-8")
            sh(["git", "add", f])
        sh(["git", "-c", "user.name=prcheck", "-c", "user.email=prcheck@localhost", "commit", "-q", "--no-verify", "--no-edit"]).check_returncode()
        lines.append(f"⚠️ Конфликт только в общих таблицах ({', '.join(f'`{f}`' for f in files)}): для проверки сведён объединением строк. "
                     "Новые строки — файлами `docs/stops.d/`, `docs/ennorath/night_watch.d/` (PROCESS 5.7).")
    changed = sh(["git", "diff", "--name-only", f"{main_sha}..HEAD"]).stdout.split()
    diff = sh(["git", "diff", f"{main_sha}..HEAD"]).stdout
    # the PR's issue: its stop ids must not be another issue's reservation (Port 212 took #578's S-7423, 2026-11-17)
    issue_m = re.search(r"(?:closes|fixes|resolves|issue)\s+#(\d+)", pr.get("body") or "", re.I)
    frag = sh([sys.executable, "tools/fragments.py", "check"] + (["--issue", issue_m.group(1)] if issue_m else [])) \
        if (WT / "tools/fragments.py").exists() else None
    if frag and frag.returncode:
        # a stop id another branch took first (PR 523's S-7387, taken by Port 228) is renumbered by the owner when landing, as every batch
        # does: a warning; an id reserved for another issue and any other broken fragment stay red
        problems = [l for l in frag.stdout.splitlines() if l.strip().startswith("- ") or ": " in l]
        taken = [l for l in problems if "is already a row of docs/STOPS.md" in l]
        foreign = [l for l in problems if "is reserved for issue" in l]
        if problems and len(taken) == len(problems):
            lines.append("⚠️ Номер стопа уже занят (владелец перенумерует при вливании; в следующий раз бери номер из своего диапазона "
                         f"`docs/stops.d/RESERVED.tsv`): {'; '.join(t.strip(' -') for t in taken)[:400]}")
        elif foreign and len(foreign) + len(taken) == len(problems):
            lines.append("❌ **Номер стопа зарезервирован за другой задачей**: переименуй фрагмент на номер из диапазона своей issue "
                         "(`python tools/fragments.py check --issue <номер>` покажет его; нет диапазона — `python tools/fragments.py next`): "
                         + "; ".join(t.strip(' -') for t in foreign + taken)[:600])
        else:
            lines.append(f"❌ Фрагменты: {frag.stdout.strip()[:600]}")
    shared = [f for f in changed if f in SHARED]
    if shared:
        lines.append(f"⚠️ Правит общие таблицы {', '.join(f'`{f}`' for f in shared)}: новые строки — файлами `docs/stops.d/S-<id>.md` и `docs/ennorath/night_watch.d/…`, README не трогать (PROCESS 5.7). Допустимо только закрыть или сузить существующий стоп.")
    # a new matrix may not hide buttons that passed on main (tools/matrix_run.py diff, owner 2026-11-16 after PR 587)
    if "docs/ennorath/MATRIX.tsv" in changed and (WT / "tools/matrix_run.py").exists():
        md = sh([sys.executable, "tools/matrix_run.py", "diff", "--against", main_sha])
        out = (md.stdout + md.stderr).strip()
        head = "✅ Матрица: ни одна кнопка, проходившая на main, не ухудшилась без объяснения" if md.returncode == 0 else \
            "❌ **Матрица ухудшает кнопки, проходившие на main**, без объяснения в `docs/ennorath/matrix_explain.d/<задача>.md` " \
            "(строка `` `<объект> <кнопка>`: почему ``; ошибка теста — чинить тест, а не объяснять)"
        lines += [head, "```", out[:3500], "```"]
    # owner 2026-11-19: a PR that adds tests shows them passing (tools/ci/own_test_check.py, the same rule as the Actions check "Own test
    # shown passing"): without the line it goes back at once, no build spent on it (PR 721's test had never compiled)
    miss = own_test_missing(pr, diff)
    if miss:
        lines.append("❌ **Свой тест не показан пройденным**: для " + ", ".join(f"`{m}*`" for m in miss) + " нет строки итога doctest "
                     "(`[doctest] test cases: N | N passed | 0 failed`) в описании PR, его комментариях или отчёте в issue. Собери, прогони свой "
                     "тест и вставь итог в описание PR. Сборка не запускалась.")
        return finish(n, sha, lines, comment, state)
    t_build = time.time()
    status("prcheck", f"PR #{n} ({pr['title'][:60]}): сборка")
    ok, err = build()
    t_build = time.time() - t_build
    if not ok:
        lines += ["❌ **Не собирается**:", "```", err[:3000], "```"]
        return finish(n, sha, lines, comment, state)
    lines.append("✅ Собирается (openbfme_tests и GDExtension, GCC).")
    own = own_tests(diff, pr["title"])
    base_needed = touches_engine(changed)
    t_tests = time.time()
    status("prcheck", f"PR #{n} ({pr['title'][:60]}): тесты, {len(own) + (len(BASE) if base_needed else 0)} наборов"
           + (" и аудит полей" if base_needed else ""))
    with cf.ThreadPoolExecutor(1) as side:  # the field audit of the PR's tree beside its tests (about 8 minutes, one core)
        audit_job = side.submit(run_audit, HOME / "audit-pr") if base_needed else None
        res = run_set(own + (BASE if base_needed else []))
        try:
            pr_audit = audit_job.result() if audit_job else None
        except Exception:  # the audit never decides the verdict
            pr_audit = None
    t_tests = time.time() - t_tests
    known = main_known(state, main_sha)
    last = state.get("main_last") or {}
    if base_needed:
        pr_res = {tc: [res[tc][0], SIGS.get(tc, res[tc][0])] for tc in BASE}  # before a main run overwrites SIGS
        need, view = main_needed(pr_res, known, last)
        if need:
            t_main = time.time()
            measure_main(state, main_sha, need)
            t_main = time.time() - t_main
            view.update({tc: list(state["main"]["tests"][tc]) for tc in need})
            lines.append(f"ℹ️ Main `{main_sha[:8]}` собран для {len(need)} наборов, где PR не как прежний main: "
                         f"{', '.join(f'`{t}`' for t in need)} ({t_main / 60:.0f} мин).")
        for tc, (st, sig) in pr_res.items():
            SIGS[tc] = sig
    else:
        view = {tc: list(known.get(tc) or last.get(tc) or ["none", "none"]) for tc in BASE}
    base = {tc: v[0] for tc, v in view.items()}
    main_sigs = {tc: v[1] for tc, v in view.items()}
    if not base_needed:
        res.update({tc: (base.get(tc, "none"), "") for tc in BASE})
        lines.append("ℹ️ Базовый набор не гонялся: PR не меняет движок (код, cmake, тестовые данные, godot/) и файлы базовых тестов — результат как на main.")
    for tc in own:
        st, e = res[tc]
        mark = {"pass": "✅", "fail": "❌", "none": "❌ ни одного теста по шаблону (тестовый файл не подключён к openbfme_tests в engine/CMakeLists.txt?)"}[st]
        lines.append(f"{mark} свои тесты `{tc}`" + (f"\n```\n{e}\n```" if st == "fail" and e else ""))
    if not own:
        lines.append("⚠️ PR не добавляет TEST_CASE: своего теста нет.")
    worse, better = [], []
    for tc in BASE:
        st, e = res[tc]
        was = base.get(tc)
        if st == "fail" and was == "pass":
            worse.append((tc, e))
        elif st == "pass" and was == "fail":
            better.append(tc)
    # a pin the PR names a cause for (its text or diff mentions the test and a pin / cause word: Port 225's "## Pins" section) is no failure:
    # the owner re-measures it when landing; a PR that moves a pin and says nothing, or breaks any other test, stays red (2026-11-17: PRs 582
    # and 517 sat "for rework" on the dashboard although 582 had named its uber1 / perf1 cause)
    text = ((pr.get("body") or "") + "\n" + diff).lower()
    cause_word = re.search(r"\bpins?\b|пин|причин|cause|re-?measure|перемер", text) is not None

    def named(tc):
        return tc in PINS and cause_word and re.split(r"[*?]", tc)[0].lower() in text

    real = [(tc, e) for tc, e in worse if not named(tc)]
    if real:
        lines.append("❌ **Ломает то, что на main зелёное:**")
        for tc, e in real:
            pin = " — **сдвигает пин**: назови причину в PR (владелец перемеряет)" if tc in PINS else ""
            lines.append(f"- `{tc}`{pin}" + (f"\n```\n{e}\n```" if e else ""))
    for tc, e in worse:
        if named(tc):
            lines.append(f"⚠️ `{tc}` сдвигает пин, причина в PR названа: владелец перемерит при вливании." + (f"\n```\n{e}\n```" if e else ""))
    # the batch's split (owner 2026-11-18): a PR that moves any base test (its signature differs from main's, red on main or not) goes into
    # a batch alone, the others together; batch_bot.py reads this mark. Without the base set run (no engine change) nothing moves.
    lines.append(moves_mark(moved_tests(res, base, main_sigs) if base_needed else []))
    if not worse:
        reds = [tc for tc in BASE if base.get(tc) == "fail"]
        lines.append("✅ Базовый набор как на main" + (f" (на main и так красные: {', '.join(f'`{t}`' for t in reds)})" if reds else "") + ".")
    if better:
        lines.append(f"ℹ️ Чинит красное на main: {', '.join(f'`{t}`' for t in better)}.")
    mine = set(changed)
    overlap = []
    for other in open_prs:
        if other["number"] == n:
            continue
        common = sorted(mine & {f["path"] for f in other.get("files", [])} - set(SHARED) - {"engine/CMakeLists.txt"})
        if common:
            overlap.append(f"- #{other['number']} {other['title'][:80]}: {', '.join(f'`{c}`' for c in common[:6])}")
    if overlap:
        lines += ["⚠️ Те же файлы правят открытые PR (при слиянии сверить функции):"] + overlap
    if base_needed:
        claim = (pr.get("title") or "") + "\n" + (pr.get("body") or "")
        m = re.search(r"(?:closes|fixes|resolves|issue)\s+#(\d+)", pr.get("body") or "", re.I)
        if m:
            try:
                claim += "\n" + json.loads(gh("issue", "view", m.group(1), "--json", "title,body")).get("body", "")
            except Exception:
                pass
        try:
            lines += audit_section((state.get("main_audit") or ["", ""])[0], pr_audit, claim)
        except (OSError, ValueError, KeyError) as exc:
            lines.append(f"ℹ️ Аудит полей не сверен: {exc}")
    try:
        lines += map_section(diff, main_sha)
    except (OSError, ValueError, KeyError) as exc:
        lines.append(f"ℹ️ Карта функций не посчитана: {exc}")
    lines.append(f"⏱️ Сборка {t_build / 60:.0f} мин, тесты {t_tests / 60:.0f} мин.")
    return finish(n, sha, lines, comment, state)


ENGINE_PREFIXES = ("engine/src/", "engine/thirdparty/", "engine/cmake/", "engine/tests/data/", "godot/")


def touches_engine(changed: list[str]) -> bool:
    """True when the PR can change a base test's result: engine code / build / data, or the source file of a base test."""
    if any(f.startswith(ENGINE_PREFIXES) or f == "engine/CMakeLists.txt" for f in changed):
        return True
    prefixes = [b.rstrip("*").split("?")[0].split(":")[0].strip() for b in BASE]
    for f in changed:
        if f.startswith("engine/tests/") and f.endswith(".cpp") and (WT / f).exists():
            text = (WT / f).read_text(encoding="utf-8", errors="replace")
            if any(f'TEST_CASE("{p}' in text for p in prefixes):
                return True
    return False


RETURNED = re.compile(r"<!-- returned -->|не принят")
TAKEN_REFS = ("refs/remotes/origin/batch/", "refs/remotes/origin/land/", "refs/remotes/origin/agent/claude/station-0")


def foreign(p: dict) -> bool:
    """True unless GitHub says the PR's branch lives in this repository (only the owner and the agents working through the owner's account
    push here). GitHub sets these fields, not the PR; a missing field or a deleted fork counts as foreign."""
    return p.get("isCrossRepository") is not False or (p.get("headRepositoryOwner") or {}).get("login") != GH.split("/")[0]


FOREIGN_NOTE = ("🔒 **PR из чужого форка**: на песочницах владельца его код не собирается и не запускается. Сборку и свои тесты "
                "проверяет GitHub Actions — проверка «Fork PR: build and own tests» у этого PR (тесты, которым нужны файлы игры, там "
                "пропускаются). Тесты на данных игры пройдут в батче владельца после того, как владелец прочитает код.")


FOREIGN_CONFLICT = ("⛔ **PR конфликтует с main** ({main}) в файлах {files}. На PR с конфликтом GitHub не запускает проверку "
                    "«Fork PR: build and own tests», и батч владельца его не возьмёт. Влейте свежий `origin/main` в ветку PR, разрешите "
                    "конфликты (обе стороны, ничего из main не теряя), соберите и прогоните свои тесты, запушьте.")


def base_conflicts(n: str) -> list[str]:
    """Files PR n's head (refs/prcheck/<n>, fetched by cycle) conflicts in with origin/main; [] when it merges cleanly or git cannot say."""
    r = sh(["git", "merge-tree", "--write-tree", "--name-only", "--no-messages", "origin/main", f"refs/prcheck/{n}"], cwd=REPO)
    if r.returncode != 1:
        return []
    return [line for line in r.stdout.splitlines()[1:] if line.strip()]


def skip_reason(p: dict) -> str:
    """Why the checker leaves this PR alone now ('' = check it)."""
    head = p["headRefOid"]
    # comments and commits per PR: asking for them in the list query exceeds GitHub's GraphQL node limit
    extra = json.loads(gh("pr", "view", str(p["number"]), "--json", "comments,commits"))
    last = max((c.get("committedDate") or "" for c in extra.get("commits") or []), default="")
    for c in extra.get("comments") or []:
        if RETURNED.search(c.get("body") or "") and not (c.get("body") or "").startswith("<!-- prcheck") and (c.get("createdAt") or "") > last:
            return "returned to its agent (no commit since)"
    if sh(["git", "cat-file", "-e", f"{head}^{{commit}}"], cwd=REPO).returncode == 0:
        refs = sh(["git", "for-each-ref", "--format=%(refname)", *TAKEN_REFS], cwd=REPO).stdout.split()
        for ref in refs:
            if sh(["git", "merge-base", "--is-ancestor", head, ref], cwd=REPO).returncode == 0:
                return f"already in {ref.removeprefix('refs/remotes/origin/')}"
    return ""


def map_section(diff: str, main_sha: str) -> list[str]:
    """The function map's view of the PR (owner 2026-11-16): the RotWK functions its citations port for the first time, their module classes,
    the failing matrix buttons linked to them (docs/ennorath/MATRIX_GAPS.tsv: the rows to recheck), and citations inside an uncited function
    (tools/mod/citation_check.py: the map would keep that function `none`)."""
    import csv
    sys.path.insert(0, str(WT / "tools" / "mod"))
    try:
        import citation_check as cc
    except ImportError:
        return []
    cites = cc.added_citations(diff)
    if not cites:
        return []
    inv_path = cc.find_inventory(WT)
    if not inv_path:
        return ["ℹ️ Карта функций: нет списка функций RotWK (`openbfme-audit/rw_function_blocks.json` на Диске) — цитаты не проверены."]
    inv = cc.Inventory.load(inv_path)
    before = {int(a, 16) for a in re.findall(r"RW\s+0x([0-9A-Fa-f]{5,8})", sh(["git", "grep", "-h", "-E", r"RW\s+0x", main_sha, "--", "engine/src"], cwd=WT).stdout)}
    new = cc.newly_ported(cites, inv, before)
    out = []
    if new:
        classes: dict[int, set] = {}
        fmap = WT / "docs/ennorath/RW_FUNCTION_MAP.tsv"
        if fmap.exists():
            with fmap.open(encoding="utf-8", newline="") as s:
                for r in csv.DictReader(s, delimiter="\t"):
                    a = int(r["function"], 16)
                    if a in new:
                        classes.setdefault(a, set()).add(r["module"])
        buttons = []
        gaps = WT / "docs/ennorath/MATRIX_GAPS.tsv"
        if gaps.exists():
            with gaps.open(encoding="utf-8", newline="") as s:
                for r in csv.DictReader(s, delimiter="\t"):
                    fns = {int(x.split("@")[0], 16) for x in r[list(r)[12]].split() if x.startswith("0x")}
                    if fns & new and (r["faction"], r["object"], r["button"]) not in [b[:3] for b in buttons]:
                        buttons.append((r["faction"], r["object"], r["button"], r["result"]))
        listed = ", ".join(f"`RW 0x{a:X}`" + (f" ({', '.join(sorted(classes[a])[:3])})" if a in classes else "") for a in sorted(new)[:12])
        out.append(f"🗺️ Карта функций: PR впервые цитирует {len(new)} функций RotWK — станут `ported`: {listed}.")
        if buttons:
            out.append(f"🎯 Красные кнопки матрицы, связанные с ними ({len(buttons)}; после переноса перепроверить, должны позеленеть): "
                       + "; ".join(f"{f} {o} `{b}` ({res})" for f, o, b, res in buttons[:10]) + (" …" if len(buttons) > 10 else ""))
    warn = cc.interior_warnings(cites, inv, cc.cited(WT / "engine" / "src"))
    if warn:
        out.append("⚠️ Цитаты внутри функции, начало которой нигде не процитировано (карта оставит её `none`; процитируй и начало):")
        out += [f"- {w}" for w in warn[:10]]
    return out


def finish(n: int, sha: str, lines: list[str], comment: bool, state: dict) -> str:
    body = "\n".join(lines)
    (REPORTS / f"{n}-{sha[:8]}.md").write_text(body)
    if comment:
        gh("pr", "comment", str(n), "--body", body)
    state["checked"][str(n)] = sha
    save_state(state)
    return body


def last_checked_head(comments: list[dict]) -> str:
    """The head of the newest real check among the PR's comments ('' = none): `<!-- prcheck <sha> -->` reports, without the ⏸️ notes
    (a skip is not a check)."""
    heads = [m.group(1) for c in comments for m in [re.match(r"<!-- prcheck ([0-9a-f]{40}) -->", c.get("body") or "")]
             if m and "⏸️" not in (c.get("body") or "")]
    return heads[-1] if heads else ""


def sync_from_comments(p: dict, state: dict) -> None:
    """Once per PR head: when the local state does not know this head as checked, read the PR's comments. A report on this head = checked
    (no second run); a report on an older head = the PR was checked before, so a new head is a rework and goes first."""
    n, head = str(p["number"]), p["headRefOid"]
    synced = state.setdefault("synced", {})
    if state["checked"].get(n) == head or synced.get(n) == head or p["isDraft"]:
        return
    comments = json.loads(gh("pr", "view", n, "--json", "comments")).get("comments") or []
    last = last_checked_head(comments)
    if last:
        state["checked"][n] = last
    synced[n] = head
    save_state(state)


# Owner 2026-11-20: GitHub Actions builds every PR and runs its own tests first (.github/workflows/fork-pr.yml, the job of this name, for this
# repository's PRs too); the sandbox checks a PR only when that job is green on its head, so a build failure or a red own test costs a runner,
# not an hour of the sandbox. A head pushed before the rule (no run of the job) is checked as before.
ACTIONS_CHECK = "Fork PR: build and own tests"
ACTIONS_FROM = 1795208400   # 2026-11-20 21:00 UTC: heads committed earlier have no run of the job for all PRs


RULES_CHECK = "PR rules: description, test sources, citations"   # owner 2026-11-20, .github/workflows/pr-rules.yml (no build, seconds)


def actions_check(head: str) -> str:
    """The build job and the rules job on this commit, together: a red one is red ('failure', ...), a running one 'running', '' when the build
    job has not run; a head without a rules run (before the rule) counts as the build job alone."""
    build = check_conclusion(head, ACTIONS_CHECK)
    rules = check_conclusion(head, RULES_CHECK)
    for c in (build, rules):
        if c not in ("", "running", "success", "neutral", "skipped"):
            return c
    if "running" in (build, rules):
        return "running"
    return build


def check_conclusion(head: str, check: str) -> str:
    """One check's conclusion on this commit ('success', 'failure', ...), 'running', or '' when it has not run."""
    name = check.replace(" ", "%20").replace(":", "%3A")
    done = subprocess.run(["gh", "api", f"repos/{GH}/commits/{head}/check-runs?check_name={name}"], capture_output=True, text=True,
                          encoding="utf-8", errors="replace", env=dict(os.environ, GH_TOKEN=token()))
    if done.returncode:
        raise RuntimeError(f"gh api check-runs: {done.stderr.strip()[:300]}")
    runs = json.loads(done.stdout).get("check_runs") or []
    if not runs:
        return ""
    last = max(runs, key=lambda r: r.get("started_at") or "")
    return last.get("conclusion") or "running"


def head_time(n: int | str) -> int:
    """The committer time of the PR's head (refs/prcheck/N, fetched each cycle): when its agent last pushed. Not updatedAt, which every
    comment moves, the checker's own report too."""
    out = sh(["git", "log", "-1", "--format=%ct", f"refs/prcheck/{n}"], cwd=REPO).stdout.strip()
    return int(out) if out.isdigit() else 0


def queue_order(todo: list[dict], checked: dict, when, last_reworked: bool) -> list[dict]:
    """Two queues, each oldest head first, taken in turn (owner 2026-11-19: in a day only reworked PRs were checked; PRs 653..671, never
    checked, waited behind them since 2026-11-17, because reworked went first and the rest by updatedAt, which the checker's own comments
    move). Reworked = checked before at another head (its agent waits for the answer, PR 523); new = never checked. The queue that did not
    give the last check gives the next one."""
    rework = sorted([p for p in todo if str(p["number"]) in checked], key=when)
    new = sorted([p for p in todo if str(p["number"]) not in checked], key=when)
    first, second = (new, rework) if last_reworked else (rework, new)
    out = []
    for i in range(max(len(first), len(second))):
        out += first[i:i + 1] + second[i:i + 1]
    return out


def cycle(args) -> int:
    state = load_state()
    sh(["git", "fetch", "-q", "origin"], cwd=REPO).check_returncode()
    main_sha = sh(["git", "rev-parse", "origin/main"], cwd=REPO).stdout.strip()
    prs = json.loads(gh("pr", "list", "--state", "open", "--limit", "100", "--json",
                        "number,title,body,headRefOid,headRefName,files,isDraft,updatedAt,createdAt,isCrossRepository,headRepositoryOwner"))
    sh(["git", "fetch", "-q", "origin", "+refs/pull/*/head:refs/prcheck/*"], cwd=REPO)  # the heads, for the batch test below
    # A red PR whose description changed is checked again without a new commit: an agent that only names a pin's cause in the PR body
    # (the named-pin rule reads the body) gets its new verdict (PR 583, 2026-11-17).
    bodies = state.setdefault("body", {})

    def body_hash(p) -> str:
        return hashlib.sha1((p.get("body") or "").encode("utf-8")).hexdigest()

    def body_changed(p) -> bool:
        n, h = str(p["number"]), body_hash(p)
        if n not in bodies:
            bodies[n] = h
            return False
        report = REPORTS / f"{n}-{p['headRefOid'][:8]}.md"
        return bodies[n] != h and report.exists() and "❌" in report.read_text(encoding="utf-8", errors="replace")

    # GitHub is the record of what was checked, not the local state: a sandbox restored from an older backup (sb1 2026-11-18, a state from
    # 2026-11-15) took every PR for new, re-checked the whole list and left reworked PR 523 behind the queue for hours.
    for p in prs:
        sync_from_comments(p, state)
    todo = [p for p in prs if (args.pr is None or p["number"] == args.pr) and not p["isDraft"]
            and (state["checked"].get(str(p["number"])) != p["headRefOid"] or body_changed(p))
            and state.get("errors", {}).get(str(p["number"])) != p["headRefOid"]
            and not p["headRefName"].startswith(("claude/", "agent/claude/", "agent/local/"))]   # local stations: Claude tests them on the PC and lands them (owner 2026-11-19)
    skipped = state.setdefault("skipped", {})
    keep = []
    for p in todo:
        if foreign(p):  # GitHub's runner checks it; one note per head, never a build here
            n, head = str(p["number"]), p["headRefOid"]
            if state.setdefault("foreign", {}).get(n) != head:
                print(f"#{n}: from another account's fork, left to GitHub Actions", flush=True)
                if not args.no_comment:
                    gh("pr", "comment", n, "--body", f"<!-- prcheck-foreign {head} -->\n{FOREIGN_NOTE}")
                state["foreign"][n] = head
                save_state(state)
            # A fork's PR that conflicts with main never gets GitHub's check (pull_request runs on the merge commit, which a conflict
            # does not have) and the batch bot waits for that check forever (PR 774, 2026-11-21): say so to its agent, once per head
            files = base_conflicts(n)
            if files and state.setdefault("foreign_conflict", {}).get(n) != head:
                print(f"#{n}: from another account's fork, conflicts with main in {', '.join(files[:6])}", flush=True)
                if not args.no_comment:
                    gh("pr", "comment", n, "--body", f"<!-- returned --><!-- prcheck-foreign-conflict {head} -->\n" + FOREIGN_CONFLICT.format(
                        main=main_sha[:8], files=", ".join(f"`{f}`" for f in files[:12])))
                state["foreign_conflict"][n] = head
                save_state(state)
            continue
        why = skip_reason(p) if args.pr is None else ""
        if why:
            if skipped.get(str(p["number"])) != [p["headRefOid"], why]:
                print(f"#{p['number']}: skipped, {why}", flush=True)
                skipped[str(p["number"])] = [p["headRefOid"], why]
                what = ("возвращён агенту; проверю, когда в PR появится новый коммит" if why.startswith("returned")
                        else f"уже в `{why.removeprefix('already in ')}`: его тесты гоняются там, при новом коммите проверю заново")
                finish(p["number"], p["headRefOid"], [f"<!-- prcheck {p['headRefOid']} -->",
                       f"⏸️ **PR #{p['number']} не проверялся заново** (песочница {SANDBOX}, голова {p['headRefOid'][:8]}): {what}."],
                       not args.no_comment, state)
            continue
        if args.pr is None:
            n, head = str(p["number"]), p["headRefOid"]
            ci = actions_check(head)
            if ci in ("", "running") and (ci == "running" or head_time(n) >= ACTIONS_FROM):
                if state.setdefault("actions_wait", {}).get(n) != head:
                    print(f"#{n}: waits for GitHub Actions ({ci or 'not started'})", flush=True)
                    state["actions_wait"][n] = head
                    save_state(state)
                continue
            if ci not in ("", "success", "neutral", "skipped"):
                if state.setdefault("actions_red", {}).get(n) != head:
                    print(f"#{n}: GitHub Actions {ci}: not checked here", flush=True)
                    if not args.no_comment:
                        gh("pr", "comment", n, "--body", f"<!-- prcheck {head} -->\n❌ **PR #{n} не проверялся в песочнице** (голова {head[:8]}): "
                           f"в GitHub Actions красная проверка «{ACTIONS_CHECK}» или «{RULES_CHECK}» ({ci}) — сборка, свои тесты или правила PR (разделы описания, источник ожиданий тестов). Ошибки — в аннотациях проверки "
                           "на вкладке Checks этого PR. Исправь и запушь: Actions проверит заново, песочница возьмёт PR после зелёного результата.")
                    state["actions_red"][n] = head
                    save_state(state)
                continue
        keep.append(p)
    todo = keep
    held = check_claims()
    for p in [p for p in todo if held_elsewhere(p, held)]:
        print(f"#{p['number']}: checked on sandbox {held_elsewhere(p, held)} now", flush=True)
    todo = [p for p in todo if not held_elsewhere(p, held)]
    todo = queue_order(todo, state["checked"], lambda p: head_time(p["number"]), state.get("last_reworked", False))
    # one PR per cycle, then the list again: a PR reworked while another is being checked waits for that one check only, not the whole queue
    for p in todo[:1]:
        state["last_reworked"] = str(p["number"]) in state["checked"]
        if not claim_check(p):
            print(f"#{p['number']}: the other sandbox claimed it first", flush=True)
            continue
        bodies[str(p["number"])] = body_hash(p)
        t0 = time.time()
        try:
            body = check_pr(p, state, main_sha, prs, not args.no_comment)
            print(f"#{p['number']}: {body.count('❌')} red, {time.time() - t0:.0f}s", flush=True)
        except Exception as e:  # one PR's failure does not stop the others: that head is left until a new commit
            print(f"#{p['number']}: checker error: {e}", flush=True)
            state.setdefault("errors", {})[str(p["number"])] = p["headRefOid"]
            save_state(state)
        finally:
            release_check(p)
    return len(todo)


def self_update() -> None:
    """Between cycles (owner 2026-11-17): the installed copy compares itself with origin/main's tools/colab/pr_checker.py and, when that
    one compiles, replaces itself and restarts on it, so a landed change to the checker needs no hand swap. A copy inside a git tree (a run
    from a checkout) is that tree's version and stays."""
    me = Path(__file__).resolve()
    if sh(["git", "-C", str(me.parent), "rev-parse", "--git-dir"]).returncode == 0:
        return
    sh(["git", "fetch", "-q", "origin"], cwd=REPO)
    new = subprocess.run(["git", "show", "origin/main:tools/colab/pr_checker.py"], cwd=REPO, capture_output=True).stdout
    if not new or new == me.read_bytes():
        return
    try:
        compile(new, str(me), "exec")
    except SyntaxError as e:
        print(f"self-update: origin/main's pr_checker.py does not compile ({e}); keeping the running copy", flush=True)
        return
    tmp = me.with_name(me.name + ".new")
    tmp.write_bytes(new)
    os.replace(tmp, me)
    print(f"== {time.strftime('%F %T', time.gmtime())} UTC self-update: restarting on origin/main's pr_checker.py", flush=True)
    os.execv(sys.executable, [sys.executable, str(me)] + sys.argv[1:])


def check_claims(now: float | None = None) -> dict[int, dict]:
    now = time.time() if now is None else now
    held: dict[int, dict] = {}
    for f in CLAIMS.glob("*.json") if CLAIMS.exists() else []:
        try:
            c = json.loads(f.read_text())
        except (OSError, ValueError):
            continue
        if now - float(c.get("t", 0)) < CLAIM_S and f.stem.isdigit():
            held[int(f.stem)] = c
    return held


def held_elsewhere(p: dict, held: dict[int, dict]) -> str:
    c = held.get(p["number"])
    return c["sandbox"] if c and c.get("sandbox") != SANDBOX and c.get("head") == p["headRefOid"] else ""


def claim_check(p: dict, settle: float = CLAIM_SETTLE_S) -> bool:
    """Write this sandbox's claim, give the Drive time to bring the other's, keep the PR only if the claim still names this sandbox.
    Without the Drive (a hand run elsewhere) every claim holds."""
    if not CLAIMS.parent.parent.exists():
        return True
    CLAIMS.mkdir(parents=True, exist_ok=True)
    f = CLAIMS / f"{p['number']}.json"
    f.write_text(json.dumps({"sandbox": SANDBOX, "head": p["headRefOid"], "t": time.time()}))
    time.sleep(settle)
    try:
        c = json.loads(f.read_text())
    except (OSError, ValueError):
        return False
    return c.get("sandbox") == SANDBOX and c.get("head") == p["headRefOid"]


def release_check(p: dict) -> None:
    f = CLAIMS / f"{p['number']}.json"
    try:
        if json.loads(f.read_text()).get("sandbox") == SANDBOX:
            f.unlink()
    except (OSError, ValueError):
        pass


def help_batch() -> int:
    """Nothing to check: one batch round of batch_bot.py (it starts a batch only by its own rules: green PRs, claims, the shared loop
    state on the Drive). Not where sandbox 2's batch loop itself runs this checker (--no-helper)."""
    new = sh(["git", "show", "origin/main:tools/colab/batch_bot.py"], cwd=REPO).stdout
    if not new:
        return -1
    path = INSTALL / "batch_bot.py"
    if not path.exists() or path.read_text(encoding="utf-8", errors="replace") != new:
        tmp = path.with_name("batch_bot.py.new")
        tmp.write_text(new, encoding="utf-8")
        os.replace(tmp, path)
    status("prcheck", "проверять нечего: ход батчера (batch_bot.py loop --once)")
    with open("/content/batch_loop.log", "ab") as log:
        return subprocess.run([sys.executable, str(path), "loop", "--once", "--no-helper"], cwd=str(INSTALL), stdout=log,
                              stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL).returncode


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--once", action="store_true")
    ap.add_argument("--pr", type=int)
    ap.add_argument("--no-comment", action="store_true")
    ap.add_argument("--no-self-update", action="store_true", help="keep the running copy (a branch version under test)")
    ap.add_argument("--no-helper", action="store_true", help="never run a batch round when idle (the cycle batch_bot.py runs itself)")
    args = ap.parse_args()
    ensure_tree()
    while True:
        if not (args.once or args.pr or args.no_self_update):
            self_update()
        n = cycle(args)
        if args.once or args.pr:
            return 0
        if not n:
            status("prcheck", "все открытые PR проверены на своих головах: жду новых коммитов")
            if not args.no_helper:
                help_batch()
                status("prcheck", "все открытые PR проверены на своих головах: жду новых коммитов")
            time.sleep(300)


if __name__ == "__main__":
    sys.exit(main())
