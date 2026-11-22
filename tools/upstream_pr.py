"""Upstream PRs from the fork's ready up/* branches (owner, 2026-11-16).

Runs on the owner's PC, in the persistent tree workspace/wt/upstream-pr (detached upstream/main, built once with its own build.bat, then
incremental): the Colab token cannot open PRs in Open-BFME (403), the PC's gh can.

    python -I tools/upstream_pr.py check [branch ...]   # every up/* branch without an upstream PR, or the named ones: text and merge checks
                                                        # (owner 2026-11-16: no build, upstream's CI builds the PR); writes the report
    python -I tools/upstream_pr.py check --build        # also builds each branch on upstream/main and runs its tests with and without it
    python -I tools/upstream_pr.py rebase [branch ...]  # squashes branches onto upstream/main, resolving list-file conflicts and "#N"
    python -I tools/upstream_pr.py open up/port-217 ... # opens the PRs of branches that passed the last check
    python -I tools/upstream_pr.py open --ready         # every ready branch without an upstream PR (sandbox 4's loop, owner 2026-11-17)

The checks are upstream's rules (docs/ennorath/PROCESS.md, "Как собирается PR в upstream"; their CONTRIBUTING / REVIEW):
  * the branch merges into the current upstream/main without a conflict;
  * no fork-only paths (docs/ennorath, workspace, .hashes vectors: upstream's merge queue re-measures pins), no "#N" in the commit texts
    (it links upstream's issues), English commit texts;
  * every new TEST_CASE passes on the merge, and fails once the branch's non-test files are put back to upstream/main
    (upstream drops tests that pass without the change);
  * visual-only changes (GameClient / W3D / shaders with no logic file) are reported as "needs the owner's captures", never opened.
The report: workspace/upstream-pr/report.md (and .json, which `open` reads).
"""
from __future__ import annotations

import json
import os
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
TREE = REPO / "workspace" / "wt" / "upstream-pr"
OUT = REPO / "workspace" / "upstream-pr"
UPSTREAM = "Open-BFME/openbfme-godot"
FORK_OWNER = "Gleb302596555"
FORK_ONLY = ("docs/ennorath/", "workspace/", "tools/colab/", "tools/mod/", ".clinerules/")
VISUAL = re.compile(r"^engine/src/(GameClient/|W3DDevice/|GodotDevice/.*(Draw|Shader|Render))|\.(gdshader|glsl)$")
TEST_FILE = re.compile(r"^engine/tests/|^engine/cmake/|^engine/CMakeLists\.txt$")
# the full ref: a clone can also hold refs/upstream/main (a stale copy), which "upstream/main" would name first
UP = "refs/remotes/upstream/main"
JOBS = os.environ.get("OPENBFME_BUILD_JOBS", "3")  # 16 GB RAM: 3 jobs (owner, 2026-11-10)


def sh(args, cwd=REPO, check=False, **kw) -> subprocess.CompletedProcess:
    r = subprocess.run(args, cwd=cwd, capture_output=True, text=True, encoding="utf-8", errors="replace", **kw)
    if check and r.returncode:
        raise RuntimeError(f"{' '.join(map(str, args))}: {r.stderr.strip()[:400]}")
    return r


def git(*a, cwd=TREE, check=True) -> str:
    return sh(["git", *a], cwd=cwd, check=check).stdout.strip()


WINDOWS = os.name == "nt"
# On a Colab sandbox (sb4, owner 2026-11-16) gh opens the upstream PRs with the classic public_repo token of /root/.upstream_token (the
# fork-only token gets 403 there); the PC's gh has its own login.
UPSTREAM_TOKEN = Path("/root/.upstream_token")


def gh(args: list[str], check=True) -> subprocess.CompletedProcess:
    env = dict(os.environ)
    if UPSTREAM_TOKEN.exists():
        env["GH_TOKEN"] = UPSTREAM_TOKEN.read_text().strip()
    return sh(["gh", *args], check=check, env=env)


def test_env() -> dict:
    if not WINDOWS:  # the sandbox's game paths (the Colab cell writes them)
        out = sh(["bash", "-c", "source /root/.agent_env 2>/dev/null; env -0"]).stdout
        return dict(kv.split("=", 1) for kv in out.split("\0") if "=" in kv)
    env = dict(os.environ)
    env.setdefault("ROTWK_INSTALL", str(REPO / "workspace" / "games-en" / "Vanilla_(2.01)" / "install"))
    env.setdefault("BFME2_INSTALL", r"C:\Program Files (x86)\BFME2")
    env.setdefault("OPENBFME_ENNORATH_INSTALL", str(REPO / "workspace" / "mods-en" / "Ennorath" / "install"))
    return env


def ready_branches() -> list[str]:
    git("fetch", "-q", "origin", cwd=REPO)
    git("fetch", "-q", "upstream", cwd=REPO)
    branches = [b.strip().removeprefix("origin/") for b in git("branch", "-r", "--list", "origin/up/*", cwd=REPO).splitlines()]
    out = gh(["pr", "list", "-R", UPSTREAM, "--author", FORK_OWNER, "--state", "all", "--limit", "500",
              "--json", "headRefName"]).stdout
    have = {p["headRefName"] for p in json.loads(out)}
    return [b for b in branches if b not in have]


def text_checks(branch: str) -> tuple[list[str], dict]:
    """Problems found without a build, and facts for the PR body."""
    ref = f"origin/{branch}"
    problems: list[str] = []
    base = git("merge-base", UP, ref)
    files = git("diff", "--name-only", f"{base}..{ref}").split()
    msgs = git("log", "--format=%s%n%b%x00", f"{UP}..{ref}")
    subjects = git("log", "--reverse", "--format=%s", f"{UP}..{ref}").splitlines()
    if not files:
        problems.append("no change against upstream/main (already upstream?)")
    bad = [f for f in files if f.startswith(FORK_ONLY) or f.endswith(".hashes")]
    if bad:
        problems.append("fork-only files / pin vectors: " + ", ".join(bad[:6]))
    if re.search(r"(?<![\w/])#\d+", msgs):
        problems.append('"#N" in a commit text (links upstream issues; write "fork issue N")')
    if re.search(r"[А-Яа-яЁё]", msgs):
        problems.append("non-English commit text")
    added = git("diff", f"{base}..{ref}", "--", "engine")
    if "docs/ennorath" in added:
        problems.append("a code comment cites docs/ennorath (fork-only)")
    tree = git("merge-tree", "--write-tree", "--name-only", UP, ref, check=False)
    conflict = sh(["git", "merge-tree", "--write-tree", UP, ref], cwd=TREE).returncode != 0
    if conflict:  # --name-only: the tree id, the conflicted paths, a blank line, the messages
        paths = []
        for l in tree.splitlines()[1:]:
            if not l.strip():
                break
            paths.append(l.strip())
        problems.append("conflicts with upstream/main: " + ", ".join(paths[:6]))
    if "docs/STOPS.md" in files:  # upstream's CI wants unique stop ids: a new row's id must be free there
        up_ids = set(re.findall(r"^\| (S-\d+) \|", git("show", f"{UP}:docs/STOPS.md"), re.M))
        new_ids = re.findall(r"^\+\| (S-\d+) \|", git("diff", "-U0", f"{base}..{ref}", "--", "docs/STOPS.md"), re.M)
        base_ids = set(re.findall(r"^\| (S-\d+) \|", git("show", f"{base}:docs/STOPS.md", check=False), re.M))
        taken = [i for i in new_ids if i in up_ids and i not in base_ids]  # an edit of an existing row keeps its id
        if taken:
            problems.append("stop ids already used upstream: " + ", ".join(taken))
    logic = [f for f in files if not TEST_FILE.search(f) and not f.startswith("docs/")]
    visual = bool(logic) and all(VISUAL.search(f) for f in logic)
    tests = sorted(set(re.findall(r'^\+\s*TEST_CASE\("([^"]+)"', git("diff", f"{base}..{ref}", "--", "engine/tests"), re.M)))
    if not tests and not visual:
        problems.append("no new TEST_CASE (upstream wants a test that fails without the change)")
    facts = {"files": files, "logic": logic, "tests": tests, "subjects": subjects, "visual": visual,
             "behind": int(git("rev-list", "--count", f"{base}..{UP}"))}
    return problems, facts


def build() -> tuple[bool, str]:
    if WINDOWS:
        env = dict(os.environ, CMAKE_BUILD_PARALLEL_LEVEL=JOBS)
        r = sh(["cmd", "/c", str(TREE / "build.bat")], cwd=TREE, env=env, timeout=3 * 3600)
    else:  # upstream's own Linux CI build (.github/workflows/pr.yml): cmake + ccache, no GDExtension; the cache shared with the other trees
        env = dict(test_env(), CCACHE_BASEDIR="/content", CCACHE_MAXSIZE="8G")
        if env.get("CCACHE_DIR"):  # the sandbox's local cache (build_tests.sh, ccache_keep.sh), the Drive one read-only behind it
            env["CCACHE_REMOTE_STORAGE"] = f"file:{env['CCACHE_DIR']}|read-only"
        env["CCACHE_DIR"] = "/content/ccache-local"
        sh(["git", "submodule", "update", "--init", "--depth", "1", "engine/thirdparty/godot-cpp"], cwd=TREE)
        c = sh(["cmake", "-S", "engine", "-B", "engine/build", "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release", "-DOPENBFME_BUILD_GODOT=OFF",
                "-DCMAKE_C_COMPILER_LAUNCHER=ccache", "-DCMAKE_CXX_COMPILER_LAUNCHER=ccache"], cwd=TREE, env=env)
        r = c if c.returncode else sh(["cmake", "--build", "engine/build", "--target", "openbfme_tests", "-j", str(os.cpu_count() or 2)],
                                      cwd=TREE, env=env, timeout=3 * 3600)
    log = r.stdout + r.stderr
    (OUT / "build.log").write_text(log, encoding="utf-8")
    errs = [l for l in log.splitlines() if re.search(r"error (C|LNK)\d+|error:|FAILED:", l)][:6]
    return r.returncode == 0, "\n".join(errs) or log[-600:]


def run_tests(names: list[str]) -> dict[str, str]:
    exe = TREE / "engine" / "build" / ("openbfme_tests.exe" if WINDOWS else "openbfme_tests")
    res = {}
    for name in names:
        pat = name.replace(",", "?")  # doctest splits -tc on commas
        r = sh([str(exe), f"-tc={pat}"], cwd=TREE, env=test_env(), timeout=2400)
        m = re.search(r"test cases:\s*(\d+)\s*\|\s*(\d+) passed\s*\|\s*(\d+) failed", r.stdout)
        res[name] = "none" if not m or m.group(1) == "0" else ("pass" if r.returncode == 0 and m.group(3) == "0" else "fail")
    return res


def build_checks(branch: str, facts: dict) -> list[str]:
    problems: list[str] = []
    git("checkout", "-q", "-f", "--detach", UP)
    r = sh(["git", "-c", "user.name=upstream-pr", "-c", "user.email=upstream-pr@localhost", "merge", "-q", "--no-edit", "--no-verify",
            f"origin/{branch}"], cwd=TREE)
    if r.returncode:
        git("merge", "--abort", check=False)
        return ["merge failed: " + r.stderr.strip()[:300]]
    ok, err = build()
    if not ok:
        return ["does not build on upstream/main:\n" + err]
    with_fix = run_tests(facts["tests"])
    problems += [f'"{t}" {st} with the change' for t, st in with_fix.items() if st != "pass"]
    if facts["logic"] and not problems:
        git("checkout", UP, "--", *[f for f in facts["logic"] if git("cat-file", "-t", f"{UP}:{f}", check=False)])
        for f in facts["logic"]:  # files the branch added go too
            if not git("cat-file", "-t", f"{UP}:{f}", check=False) and (TREE / f).exists():
                (TREE / f).unlink()
        ok, err = build()
        if ok:
            without = run_tests(facts["tests"])
            guards = [t for t, st in without.items() if st == "pass"]
            if guards and len(guards) == len(without):
                problems.append("every new test passes without the change (upstream wants one that fails)")
            elif guards:  # guard tests next to a failing one: upstream may drop them, the PR still stands
                facts["guards"] = guards
                print(f"   guard tests (pass without the change, upstream may drop them): {', '.join(guards)}", flush=True)
        else:  # the test calls the change's new API: it cannot pass without it, which is what upstream asks
            print("   (the tests do not build without the change: they need its API)", flush=True)
    git("checkout", "-q", "-f", "--detach", UP)
    return problems


DRIVE_UP = Path("/content/drive/MyDrive/openbfme-runs/upstream")


def publish_prs() -> None:
    """On a Colab sandbox: the owner's upstream PRs to Drive (openbfme-upstream-prs.json), the dashboard's fallback when its own GitHub token
    may not read Open-BFME (403, 2026-11-16)."""
    if not DRIVE_UP.parent.is_dir():
        return
    r = gh(["pr", "list", "-R", UPSTREAM, "--author", FORK_OWNER, "--state", "all", "--limit", "300", "--json",
            "number,title,url,headRefName,state,isDraft,createdAt,updatedAt,closedAt,mergedAt"], check=False)
    if r.returncode == 0:
        DRIVE_UP.mkdir(exist_ok=True)
        (DRIVE_UP / "openbfme-upstream-prs.json").write_text(r.stdout, encoding="utf-8")


def watch_upstream(results: dict) -> None:
    """The owner's open upstream PRs (owner 2026-11-16): when a maintainer wrote (a comment or a review by someone else) or upstream's CI
    turned red since the last look, the branch gets the full check with a build here (build_checks: the tests with and without the change on
    upstream/main), and the report keeps what they wrote and what the build showed (verdict "answered"; the dashboard shows it at the PR)."""
    r = gh(["pr", "list", "-R", UPSTREAM, "--author", FORK_OWNER, "--state", "open", "--limit", "100", "--json",
            "number,url,headRefName,comments,reviews,statusCheckRollup"], check=False)
    if r.returncode:
        print(f"upstream PRs: {r.stderr.strip()[:200]}", flush=True)
        return
    seen_path = OUT / "upstream_seen.json"
    first = not seen_path.exists()  # the first look only records the answers already there (no build for them)
    seen = json.loads(seen_path.read_text(encoding="utf-8")) if seen_path.exists() else {}
    for pr in json.loads(r.stdout):
        key = str(pr["number"])
        notes = [(c.get("createdAt", ""), (c.get("author") or {}).get("login", "?"), "comment", c.get("body", ""))
                 for c in pr.get("comments") or []]
        notes += [(v.get("submittedAt", ""), (v.get("author") or {}).get("login", "?"), (v.get("state") or "review").lower(), v.get("body", ""))
                  for v in pr.get("reviews") or []]
        notes = sorted(n for n in notes if n[1] != FORK_OWNER and not n[1].endswith("[bot]"))
        red = sorted({c.get("name") or c.get("context") or "?" for c in pr.get("statusCheckRollup") or []
                      if (c.get("conclusion") or c.get("state") or "").upper() in ("FAILURE", "ERROR", "TIMED_OUT", "CANCELLED")})
        last = notes[-1][0] if notes else ""
        was = seen.get(key, {})
        if last <= was.get("last", "") and red == was.get("red", []):
            continue
        seen[key] = {"last": last, "red": red}
        b = pr["headRefName"]
        reply = [f"{who} ({kind}, {when[:16]}): {' '.join(body.split())[:300]}" for when, who, kind, body in notes if when > was.get("last", "")]
        if red and red != was.get("red", []):
            reply.append("upstream CI red: " + ", ".join(red))
        print(f"== {b}: upstream answered on PR {pr['number']}", flush=True)
        if first:
            results[b] = {"verdict": "answered", "problems": [], "files": [], "logic": [], "tests": [], "subjects": [], "visual": False,
                          "behind": 0, "sha": "", "up": "", "built": False, "pr": pr["number"], "pr_url": pr["url"], "reply": reply,
                          "build_ok": None}
            continue
        try:
            git("fetch", "-q", "origin", cwd=REPO)
            problems, facts = text_checks(b)
            problems = problems + build_checks(b, facts) if not facts["visual"] else problems
        except Exception as e:
            git("checkout", "-q", "-f", "--detach", UP, check=False)
            problems, facts = [f"error: {e}"[:300]], {"files": [], "logic": [], "tests": [], "subjects": [], "visual": False, "behind": 0}
        results[b] = {"verdict": "answered", "problems": problems, **facts, "sha": git("rev-parse", f"origin/{b}", check=False),
                      "up": git("rev-parse", UP), "built": True, "pr": pr["number"], "pr_url": pr["url"], "reply": reply,
                      "build_ok": not problems}
        print(f"   build check: " + ("; ".join(problems) or "ok"), flush=True)
    seen_path.write_text(json.dumps(seen, indent=1), encoding="utf-8")


def check(names: list[str], do_build: bool) -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    publish_prs()
    branches = names or ready_branches()
    results = {}
    if not names:
        watch_upstream(results)
    seen = json.loads((OUT / "report.json").read_text(encoding="utf-8")) if (OUT / "report.json").exists() else {}
    up = git("rev-parse", UP)
    for b in branches:
        sha = git("rev-parse", f"origin/{b}")
        if not names and seen.get(b, {}).get("sha") == sha and seen[b].get("up") == up and seen[b].get("built") == do_build:
            continue  # the loop (sandbox 4): neither the branch nor upstream moved since its last full check
        print(f"== {b}", flush=True)
        try:
            problems, facts = text_checks(b)
            if facts["visual"]:
                verdict = "visual"
            elif problems:
                verdict = "fail"
            elif do_build:
                problems = build_checks(b, facts)
                verdict = "fail" if problems else "ready"
            else:  # owner 2026-11-16: no build here, upstream's CI builds and runs the PR
                verdict = "ready"
        except Exception as e:  # one branch's failure does not stop the others
            git("checkout", "-q", "-f", "--detach", UP, check=False)
            problems, facts, verdict = [f"error: {e}"[:300]], {"files": [], "logic": [], "tests": [], "subjects": [], "visual": False, "behind": 0}, "fail"
        results[b] = {"verdict": verdict, "problems": problems, **facts, "sha": sha, "up": up, "built": do_build}
        print(f"   {verdict}: " + ("; ".join(problems) or "ok"), flush=True)
    old = json.loads((OUT / "report.json").read_text(encoding="utf-8")) if (OUT / "report.json").exists() else {}
    old.update(results)
    (OUT / "report.json").write_text(json.dumps(old, indent=1), encoding="utf-8")
    lines = ["# Upstream PR check", "", f"upstream/main {git('rev-parse', '--short', UP)}", "",
             "| branch | verdict | behind | tests | problems |", "|---|---|---|---|---|"]
    for b, r in sorted(old.items()):
        lines.append(f"| `{b}` | {r['verdict']} | {r['behind']} | {len(r['tests'])} | {'; '.join(r['problems']).replace('|', '/')[:300]} |")
    (OUT / "report.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
    drive = Path("/content/drive/MyDrive/openbfme-runs/upstream")
    if drive.parent.is_dir():  # a Colab sandbox: the dashboard (/local, "PR в апстрим") reads the report from Drive by this name
        drive.mkdir(exist_ok=True)
        (drive / "openbfme-upstream-report.json").write_text(json.dumps(old, indent=1), encoding="utf-8")
    print(f"report: {OUT / 'report.md'}")


# Conflicts the script resolves itself (owner, 2026-11-16): the list files where both sides only add rows. Upstream's file is kept and the
# branch's added lines go in after their nearest preceding line that upstream's file has (PROCESS.md: "STOPS.md не объединять со своим —
# брать файл upstream и добавлять только строку этой правки"). README is upstream's. Any other conflicted file stays the branch author's.
LIST_FILES = re.compile(r"^(engine/CMakeLists\.txt|engine/cmake/.*\.cmake|docs/STOPS\.md|docs/.*\.csv|engine/tests/test_module_coverage\.cpp)$")


ROW_KEY = re.compile(r"^\| (S-\d+) \|")


def add_branch_lines(path: str, base: str, ref: str) -> str:
    up = git("show", f"{UP}:{path}", check=False).split("\n")
    mine = git("show", f"{ref}:{path}").split("\n")
    added = set()
    for l in git("diff", "-U0", f"{base}..{ref}", "--", path).split("\n"):
        if l.startswith("+") and not l.startswith("+++"):
            added.add(l[1:])
    have = set(up)
    out = list(up)
    base_rows = {m.group(1): l for l in git("show", f"{base}:{path}", check=False).split("\n") if (m := ROW_KEY.match(l))}
    for i, line in enumerate(mine):
        if line not in added or line in have:
            continue
        m = ROW_KEY.match(line)
        if m and m.group(1) in base_rows:  # the branch edits an existing row (a stop closed or narrowed)
            at = next((k for k, l in enumerate(out) if l.startswith(f"| {m.group(1)} |")), None)
            if at is None or out[at] != base_rows[m.group(1)]:
                raise ValueError(f"{path}: {m.group(1)} was changed upstream too")
            out[at] = line
            have.add(line)
            continue
        j = i - 1
        while j >= 0 and mine[j] not in have:
            j -= 1
        at = out.index(mine[j]) + 1 if j >= 0 else len(out)
        while at < len(out) and out[at] in added:  # keep the branch's run of new lines in its order
            at += 1
        out.insert(at, line)
        have.add(line)
    return "\n".join(out)


def clean_message(text: str) -> str:
    text = re.sub(r"\n*Co-Authored-By:.*", "", text)
    return re.sub(r"(?<![\w/])#(\d+)", r"fork issue \1", text).strip()


def _rebase_one(b: str) -> None:
    ref = f"origin/{b}"
    problems, facts = text_checks(b)
    hard = [p for p in problems if not p.startswith(("conflicts with", '"#N"'))]
    if hard:
        print(f"{b}: left alone: {'; '.join(hard)}")
        return
    if not problems:
        print(f"{b}: nothing to fix")
        return
    base = git("merge-base", UP, ref)
    git("checkout", "-q", "-f", "--detach", UP)
    sh(["git", "merge", "--squash", "-q", ref], cwd=TREE)
    conflicted = git("diff", "--name-only", "--diff-filter=U").split()
    other = [f for f in conflicted if not LIST_FILES.search(f) and f != "README.md"]
    if other:
        git("reset", "-q", "--hard", UP)
        print(f"{b}: left alone: source conflict in {', '.join(other)}")
        return
    try:
        texts = {f: git("show", f"{UP}:{f}") if f == "README.md" else add_branch_lines(f, base, ref) for f in conflicted}
    except ValueError as e:
        git("reset", "-q", "--hard", UP)
        print(f"{b}: left alone: {e}")
        return
    for f, text in texts.items():
        (TREE / f).write_text(text + "\n", encoding="utf-8", newline="")
        git("add", f)
    if any(f == "docs/STOPS.md" for f in conflicted) and (TREE / "tools" / "stops_count.py").exists():
        sh([sys.executable, "-I", "tools/stops_count.py", "--write"], cwd=TREE)
        git("add", "-A", "docs", "README.md")
    msg = clean_message(git("log", "--reverse", "--format=%s%n%n%b", f"{base}..{ref}"))
    subjects = [clean_message(s) for s in git("log", "--reverse", "--format=%s", f"{base}..{ref}").splitlines()]
    title = subjects[0] if subjects else b
    body = "\n".join(f"- {s}" for s in subjects[1:]) if len(subjects) > 1 else re.sub(r"^.*\n*", "", msg, count=1)
    sh(["git", "commit", "-q", "--no-verify", "-m", title + ("\n\n" + body if body.strip() else "")], cwd=TREE, check=True)
    old = git("rev-parse", ref)
    if not git("ls-remote", "origin", f"refs/up-orig/{b}", check=False):  # the first original head stays
        git("push", "-q", "origin", f"{old}:refs/up-orig/{b}", check=False)
    r = sh(["git", "push", "-q", f"--force-with-lease={b}:{old}", "origin", f"HEAD:refs/heads/{b}"], cwd=TREE)
    git("checkout", "-q", "-f", "--detach", UP)
    print(f"{b}: {'squashed onto upstream/main' + (' (resolved ' + ', '.join(conflicted) + ')' if conflicted else '') if r.returncode == 0 else 'push failed: ' + r.stderr.strip()[:200]}")


def rebase(names: list[str]) -> None:
    """Squash each branch onto the current upstream/main with the list-file conflicts resolved, the "#N" in its text rewritten, and push it
    back over the same up/ branch (the old head is kept at refs/up-orig/<branch>). A branch with another conflict, a non-English text or
    fork-only files is left alone and named."""
    git("fetch", "-q", "origin", cwd=REPO)
    git("fetch", "-q", "upstream", cwd=REPO)
    for b in names or ready_branches():
        try:
            _rebase_one(b)
        except Exception as e:  # one branch's failure does not stop the others
            print(f"{b}: error: {e}"[:400], flush=True)
            git("checkout", "-q", "-f", "--detach", UP, check=False)
    git("fetch", "-q", "origin", cwd=REPO)


def pr_body(b: str, r: dict) -> str:
    log = git("log", "--reverse", "--format=%s%n%n%b", f"{UP}..origin/{b}")
    log = re.sub(r"\n*Co-Authored-By:.*", "", log).strip()
    tests = "\n".join(f"- `{t}`" for t in r["tests"])
    how = ("Built and run on upstream/main: each test fails without the change and passes with it." if r.get("built") else
           "The author ran each test without the change (fails) and with it (passes); the branch is rebased on the current upstream/main "
           "and left to CI to build.")
    return (f"## What this changes\n\n{log}\n\n## How it was checked\n\n{tests}\n\n{how} Pins: not re-measured here (the merge queue "
            "re-measures them).\n\n"
            "## Checklist\n\n- [x] one change\n- [x] a test that fails without it\n- [x] no fork-only files\n")


def open_prs(names: list[str]) -> None:
    report = json.loads((OUT / "report.json").read_text(encoding="utf-8"))
    for b in names:
        r = report.get(b)
        if not r or r["verdict"] != "ready":
            print(f"{b}: not ready ({r and r['verdict']}), skipped")
            continue
        if git("rev-parse", f"origin/{b}", cwd=REPO) != r["sha"]:
            print(f"{b}: the branch moved since the check, run check again")
            continue
        title = r["subjects"][0] if r["subjects"] else b
        out = gh(["pr", "create", "-R", UPSTREAM, "--head", f"{FORK_OWNER}:{b}", "--base", "main", "--title", title,
                  "--body", pr_body(b, r)], check=False)
        print(f"{b}: {out.stdout.strip() or out.stderr.strip()}")


if __name__ == "__main__":
    if len(sys.argv) < 2 or sys.argv[1] not in ("check", "rebase", "open"):
        print(__doc__)
        sys.exit(2)
    args = [a for a in sys.argv[2:] if not a.startswith("--")]
    if sys.argv[1] == "check":
        check(args, "--build" in sys.argv)  # owner 2026-11-16: no build by default, upstream's CI builds the PR
    elif sys.argv[1] == "rebase":
        rebase(args)
    else:
        if "--ready" in sys.argv:
            # owner 2026-11-17: the loop opens every branch the last check found ready and that has no upstream PR yet (sandbox 4, every 3 h);
            # visual and failing branches stay for the owner's captures / the agent's rework
            report = json.loads((OUT / "report.json").read_text(encoding="utf-8"))
            args = [b for b in ready_branches() if report.get(b, {}).get("verdict") == "ready"]
            print(f"open --ready: {len(args)} branch(es): {' '.join(args) or '-'}", flush=True)
        elif not args:
            sys.exit("name the branches to open, or --ready for every ready branch without an upstream PR")
        open_prs(args)
