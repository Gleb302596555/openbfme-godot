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
The GitHub token is the sandbox's git credential (~/.git-credentials), never printed.
"""
from __future__ import annotations

import argparse
import concurrent.futures as cf
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
BASE = ["ENNORATH scenario*", "module coverage*", "modgap2*", "mapobj*", "logic retail: the PlayerTemplate*",
        "save4a retail*", "save4b retail*", "move retail*", "perf1*", "uber1*"]
PINS = {"perf1*", "uber1*"}
SHARED = ("docs/STOPS.md", "docs/ennorath/NIGHT_WATCH.md", "README.md")
DOC_UNION = {"docs/STOPS.md", "docs/ennorath/NIGHT_WATCH.md", "docs/ennorath/PROBLEMS.md", "docs/ennorath/TOP20.md",
             "docs/ennorath/PR_CANDIDATES.md", "docs/ennorath/MODULE_GAPS.md", "docs/ennorath/TEST_BASELINE.md", "engine/CMakeLists.txt"}


SANDBOX = (Path("/content/sandbox_id").read_text().strip() if Path("/content/sandbox_id").exists() else "?")


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


def build() -> tuple[bool, str]:
    # the compiler cache on the sandbox's disk, Drive's read-only behind it (build_tests.sh, OPENBFME_CCACHE_DIR)
    done = sh(["bash", "tools/colab/build_tests.sh"], timeout=3 * 3600, env=dict(os.environ, OPENBFME_CCACHE_DIR="/content/ccache-prcheck"))
    log = done.stdout + done.stderr
    (HOME / "build.log").write_text(log)
    if done.returncode:
        errs = [l for l in log.splitlines() if re.search(r"\berror\b", l)][:12]
        return False, "\n".join(errs) or log[-2000:]
    return True, ""


def run_test(tc: str) -> tuple[str, str, str]:
    """(pattern, 'pass'|'fail'|'none', first error lines)."""
    exe = WT / "engine/build/openbfme_tests"
    try:
        done = sh([str(exe), f"-tc={tc}"], timeout=2400, env=agent_env())
        out = done.stdout + done.stderr
    except subprocess.TimeoutExpired:
        return tc, "fail", "timeout 40 min"
    m = re.search(r"test cases:\s*(\d+)\s*\|\s*(\d+) passed\s*\|\s*(\d+) failed", out)
    if not m or m.group(1) == "0":
        return tc, "none", ""
    errs = "\n".join([l[:220] for l in out.splitlines() if re.search(r"ERROR|FATAL|SIGSEGV|CRASH", l)][:4])
    return tc, ("fail" if (done.returncode or m.group(3) != "0") else "pass"), errs


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


def main_baseline(state: dict, main_sha: str) -> dict:
    """BASE on main itself, once per main commit."""
    if state["main"].get("sha") == main_sha:
        return state["main"]["results"]
    old = state["main"].get("sha")
    if old:  # main moved by docs / tools only (a landing's own commits): the engine is the same, keep its results
        moved = sh(["git", "diff", "--name-only", f"{old}..{main_sha}"]).stdout.split()
        if moved and not any(f.startswith(("engine/", "godot/")) for f in moved):
            state["main"]["sha"] = main_sha
            save_state(state)
            return state["main"]["results"]
    sh(["git", "checkout", "-q", "-f", "--detach", main_sha]).check_returncode()
    ok, err = build()
    if not ok:
        raise RuntimeError(f"main {main_sha[:8]} does not build:\n{err}")
    res = run_set(BASE)
    state["main"] = {"sha": main_sha, "results": {k: v[0] for k, v in res.items()}}
    save_state(state)
    return state["main"]["results"]


def own_tests(diff: str, title: str) -> list[str]:
    pats = set()
    for name in re.findall(r'^\+\s*TEST_CASE\("([^"]+)"', diff, re.M):
        pats.add(re.split(r"[:\s]", name, maxsplit=1)[0] + "*")
    m = re.match(r"(Port|Local)\s+(\d+)", title)
    if m and not pats:
        pats.add(f"{m.group(1).upper()}-{m.group(2)}*")
    return sorted(pats)


def check_pr(pr: dict, state: dict, main_sha: str, open_prs: list[dict], comment: bool) -> str:
    n, sha = pr["number"], pr["headRefOid"]
    base = main_baseline(state, main_sha)
    sh(["git", "fetch", "-q", "origin", f"+pull/{n}/head:refs/prcheck/{n}"]).check_returncode()
    sh(["git", "checkout", "-q", "-f", "--detach", main_sha]).check_returncode()
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
    frag = sh([sys.executable, "tools/fragments.py", "check"]) if (WT / "tools/fragments.py").exists() else None
    if frag and frag.returncode:
        lines.append(f"❌ Фрагменты: {frag.stdout.strip()[:600]}")
    shared = [f for f in changed if f in SHARED]
    if shared:
        lines.append(f"⚠️ Правит общие таблицы {', '.join(f'`{f}`' for f in shared)}: новые строки — файлами `docs/stops.d/S-<id>.md` и `docs/ennorath/night_watch.d/…`, README не трогать (PROCESS 5.7). Допустимо только закрыть или сузить существующий стоп.")
    ok, err = build()
    if not ok:
        lines += ["❌ **Не собирается**:", "```", err[:3000], "```"]
        return finish(n, sha, lines, comment, state)
    lines.append("✅ Собирается (openbfme_tests и GDExtension, GCC).")
    own = own_tests(diff, pr["title"])
    res = run_set(own + BASE)
    for tc in own:
        st, e = res[tc]
        mark = {"pass": "✅", "fail": "❌", "none": "⚠️ не найдено ни одного теста по шаблону"}[st]
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
    if worse:
        lines.append("❌ **Ломает то, что на main зелёное:**")
        for tc, e in worse:
            pin = " — **сдвигает пин**: назови причину в PR (владелец перемеряет)" if tc in PINS else ""
            lines.append(f"- `{tc}`{pin}" + (f"\n```\n{e}\n```" if e else ""))
    else:
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
    try:
        lines += map_section(diff, main_sha)
    except (OSError, ValueError, KeyError) as exc:
        lines.append(f"ℹ️ Карта функций не посчитана: {exc}")
    return finish(n, sha, lines, comment, state)


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


def cycle(args) -> int:
    state = load_state()
    sh(["git", "fetch", "-q", "origin"], cwd=REPO).check_returncode()
    main_sha = sh(["git", "rev-parse", "origin/main"], cwd=REPO).stdout.strip()
    prs = json.loads(gh("pr", "list", "--state", "open", "--limit", "100", "--json", "number,title,headRefOid,headRefName,files,isDraft"))
    todo = [p for p in prs if (args.pr is None or p["number"] == args.pr) and not p["isDraft"]
            and state["checked"].get(str(p["number"])) != p["headRefOid"]
            and not p["headRefName"].startswith(("claude/", "agent/claude/"))]
    for p in sorted(todo, key=lambda p: p["number"]):
        t0 = time.time()
        try:
            body = check_pr(p, state, main_sha, prs, not args.no_comment)
            print(f"#{p['number']}: {body.count('❌')} red, {time.time() - t0:.0f}s", flush=True)
        except Exception as e:  # one PR's failure does not stop the others
            print(f"#{p['number']}: checker error: {e}", flush=True)
    return len(todo)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--once", action="store_true")
    ap.add_argument("--pr", type=int)
    ap.add_argument("--no-comment", action="store_true")
    args = ap.parse_args()
    ensure_tree()
    while True:
        n = cycle(args)
        if args.once or args.pr:
            return 0
        if not n:
            time.sleep(600)


if __name__ == "__main__":
    sys.exit(main())
