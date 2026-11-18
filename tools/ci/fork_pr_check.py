#!/usr/bin/env python3
"""The checks of a pull request from another account's fork, on GitHub's runner (.github/workflows/fork-pr.yml, owner 2026-11-17).

    PR_TITLE=... PR_BODY=... python3 tools/ci/fork_pr_check.py

Run in GitHub's merge of the pull request into main (HEAD^1 is main), after engine/build/openbfme_tests was built. It checks:
 - the pull request does not change .github/ or tools/ci/ (it could rewrite this very check);
 - tools/fragments.py check, with the issue the description closes (a stop id reserved for another issue is red);
 - its own tests: the prefixes of the TEST_CASEs it adds (e.g. `PORT-206*`), else `PORT-N*` from a "Port N" title; a pattern that matches
   no test is red, a failing one red; a test that skips for want of the game files is a warning (it runs in the owner's batch).
Each finding is a GitHub annotation (::error / ::warning): the dashboard (lib/prs.ts) shows the error ones as the pull request's red lines.
Exit 1 on any error. Nothing here needs the game, a secret or the network.
"""
from __future__ import annotations

import os
import re
import subprocess
import sys
from pathlib import Path

EXE = Path("engine/build/openbfme_tests")
GUARDED = (".github/", "tools/ci/")


def sh(cmd: list[str], timeout: int | None = None) -> subprocess.CompletedProcess:
    return subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=timeout)


def own_tests(diff: str, title: str) -> list[str]:
    """The test patterns of the pull request (as tools/colab/pr_checker.py own_tests)."""
    pats = set()
    for name in re.findall(r'^\+\s*TEST_CASE\("([^"]+)"', diff, re.M):
        pats.add(re.split(r"[:\s]", name, maxsplit=1)[0] + "*")
    m = re.match(r"(Port|Local)\s+(\d+)", title, re.I)
    if m and not pats:
        pats.add(f"{m.group(1).upper()}-{m.group(2)}*")
    return sorted(pats)


def annotation(kind: str, title: str, message: str) -> str:
    """A workflow command line; GitHub's escaping of a property and of the message."""
    esc = lambda s: s.replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A")
    prop = esc(title).replace(":", "%3A").replace(",", "%2C")
    return f"::{kind} title={prop}::{esc(message)}"


def test_result(out: str, code: int) -> tuple[str, str]:
    """('pass' | 'skip' | 'fail' | 'none', detail) of one openbfme_tests -tc run."""
    m = re.search(r"test cases:\s*(\d+)\s*\|\s*(\d+) passed\s*\|\s*(\d+) failed", out)
    if not m or m.group(1) == "0":
        return "none", ""
    if code or m.group(3) != "0":
        errs = [l[:300] for l in out.splitlines() if re.search(r"ERROR|FATAL|SIGSEGV|CRASH", l)][:6]
        return "fail", "\n".join(errs) or out[-1500:]
    skips = [l.strip()[:200] for l in out.splitlines() if l.strip().startswith("SKIP")][:3]
    return ("skip", "; ".join(skips)) if skips else ("pass", "")


def main() -> int:
    title, body = os.environ.get("PR_TITLE", ""), os.environ.get("PR_BODY", "")
    files = sh(["git", "diff", "--name-only", "HEAD^1", "HEAD"]).stdout.split()
    diff = sh(["git", "diff", "HEAD^1", "HEAD"]).stdout
    notes: list[str] = []
    errors = 0

    def note(kind: str, head: str, msg: str) -> None:
        nonlocal errors
        print(annotation(kind, head, msg), flush=True)
        notes.append(f"{'❌' if kind == 'error' else '⚠️' if kind == 'warning' else '✅'} **{head}**: {msg}")
        errors += kind == "error"

    guarded = [f for f in files if f.startswith(GUARDED)]
    if guarded:
        note("error", "Правит проверку", "PR меняет " + ", ".join(guarded[:8]) + ": эти файлы задаёт владелец, их правка не засчитывается")
    issue = re.search(r"(?:closes|fixes|resolves|issue)\s+#(\d+)", body, re.I)
    if Path("tools/fragments.py").exists():
        frag = sh([sys.executable, "tools/fragments.py", "check"] + (["--issue", issue.group(1)] if issue else []))
        if frag.returncode:
            note("error", "Фрагменты стопов", frag.stdout.strip()[-1500:] or frag.stderr.strip()[-1500:])
    pats = own_tests(diff, title)
    if not pats:
        note("error", "Своих тестов нет", "PR не добавляет TEST_CASE и в названии нет «Port N»")
    for p in pats:
        try:
            done = sh([str(EXE), f"-tc={p}"], timeout=2400)
            st, detail = test_result(done.stdout + done.stderr, done.returncode)
        except subprocess.TimeoutExpired:
            st, detail = "fail", "timeout 40 min"
        if st == "none":
            note("error", f"Свои тесты {p}", "ни одного теста по шаблону: тестовый файл не подключён к openbfme_tests?")
        elif st == "fail":
            note("error", f"Свои тесты {p}", detail)
        elif st == "skip":
            note("warning", f"Свои тесты {p}", "пропущены без файлов игры, пойдут в батче владельца: " + detail)
        else:
            note("notice", f"Свои тесты {p}", "прошли")
    summary = os.environ.get("GITHUB_STEP_SUMMARY")
    if summary:
        with open(summary, "a", encoding="utf-8") as s:
            s.write("## Проверка PR из чужого форка\n\n" + "\n".join(f"- {n}" for n in notes) + "\n")
    print("\n".join(notes))
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
