#!/usr/bin/env python3
"""The fork's light checks on GitHub's runner (.github/workflows/fork-tools.yml, owner 2026-11-17): what needs no game file, run on every pull
request and every push to main, so an agent sees them in minutes and the Colab sandboxes keep their time for the tests on the game's data.

    PR_BODY=... python3 tools/ci/tools_check.py

Run in GitHub's merge of the pull request into main (or a pushed main commit), after the tools' pytest. It checks:
 - tools/fragments.py check, with the issue the description closes (a stop id reserved for another issue, a broken fragment);
 - tools/matrix_run.py diff against HEAD^1 when the change touches docs/ennorath/MATRIX.tsv (a button that passed may not get worse unexplained).
Each finding is a GitHub annotation (the dashboard, lib/prs.ts, shows the error ones as the pull request's red lines). Exit 1 on an error.
"""
from __future__ import annotations

import os
import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from fork_pr_check import annotation, sh  # noqa: E402


def main() -> int:
    body = os.environ.get("PR_BODY", "")
    files = sh(["git", "diff", "--name-only", "HEAD^1", "HEAD"]).stdout.split()
    errors = 0
    issue = re.search(r"(?:closes|fixes|resolves|issue)\s+#(\d+)", body, re.I)
    if Path("tools/fragments.py").exists():
        frag = sh([sys.executable, "tools/fragments.py", "check"] + (["--issue", issue.group(1)] if issue else []))
        out = (frag.stdout + frag.stderr).strip()
        if frag.returncode:
            # a stop id another branch took first is renumbered by the owner when landing (as pr_checker.py says): a warning only
            taken = [l for l in out.splitlines() if "is already a row of docs/STOPS.md" in l]
            problems = [l for l in out.splitlines() if l.strip().startswith("- ") or ": " in l]
            if problems and len(taken) == len(problems):
                print(annotation("warning", "Номер стопа уже занят", "владелец перенумерует при вливании: " + "; ".join(taken)[:800]))
            else:
                print(annotation("error", "Фрагменты стопов", out[-1500:]))
                errors += 1
        else:
            print("stop fragments ok")
    if "docs/ennorath/MATRIX.tsv" in files and Path("tools/matrix_run.py").exists():
        md = sh([sys.executable, "tools/matrix_run.py", "diff", "--against", "HEAD^1"])
        if md.returncode:
            print(annotation("error", "Матрица ухудшает кнопки, проходившие на main",
                             (md.stdout + md.stderr).strip()[-1500:] + "\n(объяснение — docs/ennorath/matrix_explain.d/<задача>.md)"))
            errors += 1
        else:
            print("matrix diff ok")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
