#!/usr/bin/env python3
"""The batch bot (owner 2026-11-17: "сделай бота батча"): the mechanical half of a landing, on a Colab sandbox, so Claude only reviews the
result and runs tools/land.py --batch <name> on the PC.

    python3 tools/colab/batch_bot.py plan                       # the PRs a batch would take now, and why the others wait
    python3 tools/colab/batch_bot.py run [--name 2026-11-18a] [--prs 601,605] [--no-comment] [--rounds 3]

`run`:
 1. takes every open PR whose newest checker report (tools/colab/pr_checker.py, `<!-- prcheck <head> -->`) is on its current head and
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


def log(msg: str) -> None:
    print(f"[{time.strftime('%H:%M:%S')}] {msg}", flush=True)


# ---------------------------------------------------------------- 1. candidates

def verdict(p: dict) -> str:
    """'green' or why the PR waits."""
    if p["isDraft"]:
        return "draft"
    if p["headRefName"].startswith(("claude/", "agent/claude/", "up/", "batch/")):
        return "Claude's own / not an agent PR"
    head = p["headRefOid"]
    extra = json.loads(gh("pr", "view", str(p["number"]), "--json", "comments,commits"))
    last_commit = max((c.get("committedDate") or "" for c in extra.get("commits") or []), default="")
    checks = [c for c in extra.get("comments") or [] if (c.get("body") or "").startswith(f"<!-- prcheck {head}")]
    real = [c for c in checks if "⏸️" not in c["body"]]
    if any(RETURNED.search(c.get("body") or "") and not c["body"].startswith("<!-- prcheck") and (c.get("createdAt") or "") > last_commit
           for c in extra.get("comments") or []):
        return "returned to its agent (no commit since)"
    if not real:
        return "not checked on its current head yet"
    if "❌" in real[-1]["body"]:
        return "red in the checker's report"
    return "green"


def candidates(only: list[int] | None) -> tuple[list[dict], list[tuple[dict, str]]]:
    prs = json.loads(gh("pr", "list", "--state", "open", "--limit", "100", "--json",
                        "number,title,body,headRefOid,headRefName,isDraft,author"))
    prs = sorted((p for p in prs if not only or p["number"] in only), key=lambda p: p["number"])
    take, wait = [], []
    for p in prs:
        why = verdict(p)
        (take.append(p) if why == "green" else wait.append((p, why)))
    return take, wait


# ---------------------------------------------------------------- 2-3. merging, stops

def ensure_tree() -> None:
    RUNS.mkdir(parents=True, exist_ok=True)
    if not (WT / ".git").exists():
        sh(["git", "worktree", "prune"], cwd=REPO)
        sh(["git", "worktree", "add", "--detach", str(WT), "origin/main"], cwd=REPO, check=True)


def pr_files(n: int) -> list[str]:
    return git("diff", "--name-only", f"origin/main...refs/batch/{n}").split()


def next_free_id() -> int:
    out = sh([sys.executable, "tools/fragments.py", "next"], check=True).stdout.strip()
    return int(out.removeprefix("S-"))


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
        if re.search(rf"\bS-{old}\b", text):
            p.write_text(re.sub(rf"\bS-{old}\b", f"S-{new}", text), encoding="utf-8")
            git("add", f)
    renumbered.append(f"#{n}: S-{old} -> S-{new} ({why})")


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
        elif f in UNION or (f.startswith("engine/cmake/") and f.endswith(".cmake")):
            parts = [sh(["git", "show", f":{k}:{f}"]).stdout for k in (2, 1, 3)]
            tmp = [WT.parent / f".batch-merge{k}" for k in range(3)]
            for t, text in zip(tmp, parts):
                t.write_text(text, encoding="utf-8")
            sh(["git", "merge-file", "--union", *map(str, tmp)])
            (WT / f).write_text(tmp[0].read_text(encoding="utf-8"), encoding="utf-8")
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
    git("checkout", "-q", "-f", "-B", f"batch/{name}", "origin/main")
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
    diff = git("diff", f"origin/main...refs/batch/{p['number']}", "--", "engine/tests")
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
    md = sh([sys.executable, "tools/matrix_run.py", "diff", "--against", "origin/main"], timeout=600)
    (out_dir / "matrix_diff.log").write_text(md.stdout + md.stderr, encoding="utf-8")
    res["matrix diff"] = ("pass" if md.returncode == 0 else "fail", (md.stdout + md.stderr).strip()[-600:])
    return res


# ---------------------------------------------------------------- 5. bisect

def merge_points() -> list[tuple[str, int | None]]:
    """The batch's first-parent commits after origin/main, oldest first, each with the PR it brings (renumber commits: their PR)."""
    out = []
    for line in git("log", "--first-parent", "--reverse", "--format=%H %s", "origin/main..HEAD").splitlines():
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


def names_cause(p: dict, test: str) -> bool:
    """The checker's rule: the PR text or diff names the pin's test and a pin / cause word."""
    text = ((p.get("body") or "") + "\n" + git("diff", f"origin/main...refs/batch/{p['number']}")).lower()
    return (re.search(r"\bpins?\b|пин|причин|cause|re-?measure|перемер", text) is not None
            and re.split(r"[*?]", test)[0].lower() in text)


def cause_line(p: dict, test: str) -> str:
    key = re.split(r"[*?]", test)[0].lower()
    for line in (p.get("body") or "").splitlines():
        if key in line.lower():
            return line.strip(" -*")[:300]
    return ""


def repin(test: str, p: dict, name: str) -> str:
    env = dict(agent_env(), **{PINS[test]: "1"})
    st, err = run_test(test, env)
    after, err2 = run_test(test)
    changed = [l[3:] for l in git("status", "--porcelain", "--", "engine/tests").splitlines()]
    if after != "pass" or not changed:
        return f"`{test}`: re-measure failed (regen {st}, after {after}: {err2[:200] or 'no data changed'})"
    git("add", "--", *changed)
    cause = cause_line(p, test)
    git(*AUTHOR, "commit", "-q", "--no-verify", "-m",
        f"Batch {name}: {test.rstrip('*')} re-measured, moved by #{p['number']}" + (f": {cause}" if cause else "") +
        "\n\nCo-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>")
    return f"`{test}` re-measured ({len(changed)} files), moved by #{p['number']}" + (f": {cause}" if cause else "")


# ---------------------------------------------------------------- 6-7. returns, report

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
    lines = [f"# Батч {name}", "", f"Ветка `batch/{name}` на `origin/main` {r['main'][:8]}, голова {r.get('head', '?')[:8]} "
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


# ---------------------------------------------------------------- main

def cmd_plan(a) -> int:
    take, wait = candidates(a.prs)
    for p in take:
        print(f"take  #{p['number']} {p['title'][:90]}")
    for p, why in wait:
        print(f"wait  #{p['number']} {p['title'][:70]}: {why}")
    return 0


def cmd_run(a) -> int:
    ensure_tree()
    name = a.name or dt.datetime.utcnow().strftime("%Y-%m-%d-%H%M")
    out_dir = RUNS / name
    out_dir.mkdir(parents=True, exist_ok=True)
    take, wait = candidates(a.prs)
    log(f"batch {name}: {len(take)} green PRs: {' '.join('#%d' % p['number'] for p in take)}")
    rows: dict = {"main": git("rev-parse", "origin/main").strip(), "wait": wait, "out": {}, "repinned": [], "results": {}, "main_red": set(),
                  "merged": [], "conflicted": [], "renumbered": [], "verdict": "нет PR"}
    if not take:
        report(name, out_dir, rows)
        return 0
    out: dict[int, str] = {}
    main_cache: dict[str, str] = {}
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
            git("checkout", "-q", "-f", "--detach", "origin/main")
            ok_main, _ = build("main", out_dir)
            main_cache.update({t: s for t, (s, _) in run_tests(unknown).items()} if ok_main else {t: "build" for t in unknown})
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
        culprit = bisect(new, out_dir, f"batch/{name}") if new else {}
        log(f"bisect: {culprit}")
        repins = []
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
    return 0 if rows["verdict"].startswith("зелёный") else 1


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    for c in ("plan", "run"):
        s = sub.add_parser(c)
        s.add_argument("--prs", type=lambda v: [int(x) for x in v.split(",") if x], default=None)
        if c == "run":
            s.add_argument("--name")
            s.add_argument("--rounds", type=int, default=3)
            s.add_argument("--no-comment", action="store_true", help="do not comment on the excluded PRs (a dry run)")
    a = ap.parse_args()
    return cmd_plan(a) if a.cmd == "plan" else cmd_run(a)


if __name__ == "__main__":
    sys.exit(main())
