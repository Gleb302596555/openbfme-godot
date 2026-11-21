#!/usr/bin/env python3
"""pr_rules.py: the formal rules of a Port / Local pull request, checked on GitHub's runner in seconds (owner 2026-11-20).

What a script can check exactly, so a PR that breaks one comes back to its agent minutes after the push instead of after a sandbox check
and Claude's review (the own-test line is own_test_check.py's, the stop ids fragments.py's, the citations citation_check.py's):

  1. sections  the description has the blocks the templates ask for: an in-game check ("в игре" / "in game"), the maps block ("Карты" /
               "Maps"); a Port PR also "местная правка или движка" (local / engine) and "Осталось" (remaining). Error.
  2. sources   every TEST_CASE the PR adds names where its expectations come from: an `RW 0x...` address, a file of the mod or the game
               (`.ini`, `data\\...`), a donor line (`DONOR`), or `INFERENCE` — in the test's own added lines or in the header comment of a test
               file the PR adds. Error (PROCESS 15.7: an expectation taken from the new code hides the bug).
  3. citations an added block of engine code (3 or more consecutive code lines in engine/src) with no `RW 0x`, `DONOR`, `INFERENCE`, `S-<n>`
               or `ZH` near it (in the block or the 6 added lines above it). Warning only: what is "behaviour" a script cannot tell.
  4. upstream  a branch up/* for Open-BFME names no "#<number>" in its commit messages or added lines (upstream reads it as its own issue).
               Error; --up mode.

    PR_TITLE=... PR_BODY=... pr_rules.py                 # in GitHub's merge of the PR into main (HEAD^1..HEAD), annotations to stdout
    pr_rules.py --pr <n>                                  # a dry run on an open PR through gh (no annotations, a summary)
    pr_rules.py --up <base-ref>                           # an up/* branch: base..HEAD

Exit 1 on an error.
"""
from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys

REPO = "Gleb302596555/openbfme-godot"
SOURCE = re.compile(r"\bRW\s+0x[0-9A-Fa-f]{5,8}|\.ini\b|data\\|data/ini|\bDONOR\b|\bINFERENCE\b|\.big\b|\bmapcache\b|\.map\b", re.I)
CITED = re.compile(r"\bRW\s+0x[0-9A-Fa-f]{5,8}|\bDONOR\b|\bINFERENCE\b|\bS-\d{3,5}\b|\bZH\b|\bBFME2?\s+0x", re.I)
ISSUE_REF = re.compile(r"(?<![\w&/#])#\d{1,5}\b")
TASK_TITLE = re.compile(r"^\s*(Port|Local|PORT|LOCAL)[\s-]+\d+", re.I)


def sh(cmd: list[str]) -> subprocess.CompletedProcess:
    return subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", errors="replace")


def sections(title: str, body: str) -> list[tuple[str, str, str]]:
    """Rule 1: the description's blocks."""
    out = []
    if not TASK_TITLE.match(title):
        return out
    port = re.match(r"\s*port", title, re.I) is not None
    need = [("в игре / in game", r"в\s+игре|in[\s-]+game"), ("Карты / Maps", r"карт[ыа]|\bmaps?\b")]
    if port:
        need += [("местная правка или движка / local or engine", r"местн|движ|\blocal\b|\bengine\b"), ("Осталось / remaining", r"осталось|remaining")]
    missing = [name for name, rx in need if not re.search(rx, body or "", re.I)]
    if missing:
        out.append(("error", "Описание PR", "нет разделов: " + "; ".join(missing)
                    + " — описание пишется по шаблону (раздел «PR» промта): проверка в игре, местная правка или движок, «Осталось», «Карты»"))
    return out


def files_of(diff: str) -> list[tuple[str, list[str], bool]]:
    """(path, the diff lines of the file, the file is new)"""
    out = []
    for part in re.split(r"^diff --git ", diff, flags=re.M)[1:]:
        m = re.match(r"a/(\S+) b/(\S+)", part)
        if not m:
            continue
        lines = part.splitlines()
        out.append((m.group(2), lines, any(l.startswith("new file mode") for l in lines[:6])))
    return out


def test_sources(diff: str) -> list[tuple[str, str, str]]:
    """Rule 2: each added TEST_CASE names a source of its expectations."""
    out = []
    for path, lines, new in files_of(diff):
        if not path.startswith("engine/tests/") or not path.endswith((".cpp", ".h")):
            continue
        added = [l[1:] for l in lines if l.startswith("+") and not l.startswith("+++")]
        header = "\n".join(added[:60]) if new else ""
        header_ok = new and bool(SOURCE.search(header.split("TEST_CASE", 1)[0]))
        idx = [i for i, l in enumerate(added) if re.match(r"\s*TEST_CASE\(", l)]
        for k, i in enumerate(idx):
            end = idx[k + 1] if k + 1 < len(idx) else len(added)
            body = "\n".join(added[max(0, i - 8):end])   # the comment just above the case counts too
            name = re.search(r'TEST_CASE\("([^"]{0,90})', added[i])
            if not SOURCE.search(body) and not header_ok:
                out.append(("error", "Источник ожиданий теста", f"{path}: TEST_CASE «{name.group(1) if name else '?'}» не называет, откуда ожидания "
                            "(адрес RW 0x…, файл INI мода или игры, DONOR, INFERENCE) — ни в своих строках, ни в шапке файла (PROCESS 15.7)"))
    return out


def code_citations(diff: str) -> list[tuple[str, str, str]]:
    """Rule 3: an added block of engine code without a citation nearby (warning)."""
    out = []
    for path, lines, _ in files_of(diff):
        if not path.startswith("engine/src/") or not path.endswith((".cpp", ".h", ".inc")):
            continue
        run: list[str] = []
        recent: list[str] = []

        def close():
            if len(run) >= 3 and not CITED.search("\n".join(recent[-6:] + run)):
                out.append(("warning", "Цитата RW", f"{path}: добавлен блок кода без адреса RW / DONOR / INFERENCE / стопа рядом: «{run[0].strip()[:100]}»"))

        for l in lines:
            if l.startswith("+") and not l.startswith("+++"):
                text = l[1:]
                s = text.strip()
                is_code = bool(s) and not s.startswith(("//", "/*", "*", "#include", "#pragma", "{", "}")) and s not in ("};", "});")
                if is_code:
                    run.append(text)
                else:
                    close()
                    run = []
                recent.append(text)
            else:
                close()
                run = []
                recent = []
        close()
    seen, uniq = set(), []
    for o in out:
        if o[2] not in seen:
            seen.add(o[2])
            uniq.append(o)
    return uniq[:20]


def upstream_refs(messages: str, diff: str) -> list[tuple[str, str, str]]:
    """Rule 4: no "#<number>" in an up/* branch's commit messages or added lines."""
    out = []
    for m in ISSUE_REF.finditer(messages):
        out.append(("error", "Ссылка #N в upstream", f"сообщение коммита: «…{messages[max(0, m.start() - 40):m.end() + 20].strip()}…» — в upstream пиши «fork issue N»"))
    for path, lines, _ in files_of(diff):
        for l in lines:
            if l.startswith("+") and not l.startswith("+++"):
                m = ISSUE_REF.search(l)
                if m:
                    out.append(("error", "Ссылка #N в upstream", f"{path}: «{l[1:].strip()[:120]}» — в upstream пиши «fork issue N»"))
    return out[:20]


def annotation(kind: str, title: str, message: str) -> str:
    esc = lambda s: s.replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A")
    return f"::{kind} title={esc(title).replace(':', '%3A').replace(',', '%2C')}::{esc(message)}"


def report(found: list[tuple[str, str, str]], annotate: bool) -> int:
    for kind, head, msg in found:
        print(annotation(kind, head, msg) if annotate else f"{kind}: {head}: {msg}", flush=True)
    summary = os.environ.get("GITHUB_STEP_SUMMARY")
    if summary and found:
        with open(summary, "a", encoding="utf-8") as s:
            s.write("## Правила PR\n\n" + "\n".join(f"- {'❌' if k == 'error' else '⚠️'} **{h}**: {m}" for k, h, m in found) + "\n")
    return 1 if any(k == "error" for k, _, _ in found) else 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--pr", type=int, help="dry run on an open PR through gh")
    ap.add_argument("--up", help="an up/* branch: check base..HEAD")
    a = ap.parse_args()
    if a.up:
        msgs = sh(["git", "log", "--format=%B", f"{a.up}..HEAD"]).stdout
        diff = sh(["git", "diff", f"{a.up}...HEAD"]).stdout
        return report(upstream_refs(msgs, diff), True)
    if a.pr:
        meta = json.loads(sh(["gh", "pr", "view", str(a.pr), "-R", REPO, "--json", "title,body"]).stdout)
        title, body = meta["title"], meta.get("body") or ""
        diff = sh(["gh", "pr", "diff", str(a.pr), "-R", REPO]).stdout
        return report(sections(title, body) + test_sources(diff) + code_citations(diff), False)
    title, body = os.environ.get("PR_TITLE", ""), os.environ.get("PR_BODY", "")
    diff = sh(["git", "diff", "HEAD^1", "HEAD"]).stdout
    return report(sections(title, body) + test_sources(diff) + code_citations(diff), True)


if __name__ == "__main__":
    sys.exit(main())
