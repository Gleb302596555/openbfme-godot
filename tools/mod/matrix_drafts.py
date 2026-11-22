#!/usr/bin/env python3
"""Task drafts from the button matrix (owner 2026-11-17: "черновики новых задач"): the red buttons grouped by the module class that
fails them, one draft per group in the fork's Port issue format, so Claude only checks the addresses, picks and publishes.

    python3 tools/mod/matrix_drafts.py [--ini <ennorath-ini zip>] [--out workspace/issue-drafts/matrix] [--top 20] [--no-gh]

Reads docs/ennorath/MATRIX.tsv (the verdicts), docs/ennorath/MATRIX_GAPS.tsv (each red button's module class and the RotWK functions of it
the port lacks, with addresses) and, with --ini, the lint of tools/mod/matrix_review.py (a button the lint flags is likely the sweep's
error: it goes to a separate list, not into a draft). With gh, the open issues are searched for the class and the buttons: a group an
open task already names is marked, not drafted again. Writes <out>/index.md (groups ranked by red buttons, then by missing functions)
and <out>/NN-<class>.md for the top groups. Names, verdicts and addresses only: no game data. The output dir is ignored by git
(workspace/); the stop groups' drafts come from tools/mod/stop_groups.py (Port 268, docs/ennorath/STOP_GROUPS.md).
"""
from __future__ import annotations

import argparse
import collections
import csv
import json
import os
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
import matrix_review as mr  # noqa: E402

GH_REPO = "Gleb302596555/openbfme-godot"
FN_COL_PREFIX = "specific none functions"


def gaps_rows() -> dict[tuple[str, str, str], dict]:
    path = REPO / "docs/ennorath/MATRIX_GAPS.tsv"
    if not path.exists():
        return {}
    with path.open(encoding="utf-8", newline="") as f:
        rows = list(csv.DictReader(f, delimiter="\t"))
    out: dict[tuple[str, str, str], dict] = {}
    for r in rows:
        k = (r["faction"], r["object"], r["button"])
        # the button's main class row first (the class that fails it), else its first row
        if k not in out or (r.get("main class") == "yes" and out[k].get("main class") != "yes"):
            out[k] = r
    return out


def functions(row: dict) -> list[str]:
    col = next((c for c in row if c.startswith(FN_COL_PREFIX)), "")
    return [x.split("@")[0] for x in (row.get(col) or "").split() if x.startswith("0x")]


def open_issues() -> list[dict]:
    env = dict(os.environ)
    tok = Path("/root/.gh_token")  # a Colab sandbox keeps its token there (the git credential helper's file)
    if "GH_TOKEN" not in env and tok.exists():
        env["GH_TOKEN"] = tok.read_text().strip()
    try:
        out = subprocess.run(["gh", "issue", "list", "-R", GH_REPO, "--state", "open", "--limit", "300", "--json", "number,title,body"],
                             capture_output=True, text=True, encoding="utf-8", check=True, env=env).stdout
        return json.loads(out)
    except (OSError, subprocess.CalledProcessError, ValueError):
        return []


def covering(issues: list[dict], cls: str, buttons: list[str]) -> list[str]:
    hits = []
    for i in issues:
        # the class in the title (an umbrella issue's body names many classes), or one of the exact buttons anywhere
        text = (i.get("title") or "") + "\n" + (i.get("body") or "")
        if (cls and re.search(rf"\b{re.escape(cls)}\b", i.get("title") or "")) or any(re.search(rf"\b{re.escape(b)}\b", text) for b in buttons):
            hits.append(f"#{i['number']} {i['title'][:80]}")
    return hits


def draft(cls: str, items: list[tuple[tuple[str, str, str], tuple[str, str], dict]], fns: list[str], cover: list[str]) -> str:
    kinds = collections.Counter(cat for _, (cat, _), _ in items)
    kind_text = ", ".join(f"{k} {n}" for k, n in kinds.most_common())
    lines = [f"Port <N>: {cls}: {len(items)} red button{'s' if len(items) > 1 else ''} of the matrix ({kind_text})", "",
             "<!-- draft by tools/mod/matrix_drafts.py: check every address with rw.py dis before publishing; reserve the stop ids "
             "(python tools/fragments.py reserve <issue> 3) and put them in Do not touch -->", ""]
    if cover:
        lines += ["> Already named by open tasks (check before publishing): " + "; ".join(cover[:5]), ""]
    lines += ["## Problem", "",
              f"The button matrix (`docs/ennorath/MATRIX.tsv`, `tools/matrix_run.py`) presses these buttons on Ennorath's objects and they fail; "
              f"their module class is `{cls}` (`docs/ennorath/MATRIX_GAPS.tsv`, main class):", ""]
    for (fac, obj, btn), (cat, detail), row in items:
        tag = f" (`{row.get('module tag', '')}`)" if row.get("module tag") else ""
        lines.append(f"- {fac.removeprefix('Faction')} `{obj}` `{btn}`{tag}: **{cat}** — {detail[:220]}")
    lines += ["", "## Known (from the function map; check each again)", ""]
    if fns:
        lines.append(f"- RotWK functions of `{cls}` the port has no citation of (smallest call depth first): "
                     + ", ".join(f"RW {f}" for f in fns[:16]) + (" …" if len(fns) > 16 else ""))
    else:
        lines.append(f"- The map lists no uncited function of `{cls}` for these buttons: the failure is in a ported function (compare it "
                     "with RotWK linearly) or outside the class (the command path, the target, the weapon).")
    lines.append(f"- `grep -i {cls} docs/ennorath/RW_FUNCTION_MAP.tsv | grep -v ported` lists the class's functions with their status and callers.")
    lines += ["", "## Task", "",
              f"First check main: if these buttons pass now (`python3 tools/matrix_run.py run --factions <faction> --objects <object>`), "
              "stop and say so here.", "",
              f"1. Read how RotWK runs `{cls}` for these buttons (the functions above, from the module's create and its update / "
              "special-power slots) and write in the PR what each branch does, with addresses.",
              "2. Port what is missing in `openbfme_core`, one mechanism for every listed button (not a fix per button); cite each ported "
              "function at its first instruction (`RW 0x…`).",
              "3. Re-run the matrix for the listed objects and put the before / after verdicts in the PR.", "",
              "## Tests", "",
              f"`PORT-<N> ...` on synthetic INI: an object with `{cls}` and the data these buttons use -> pressing the button has the "
              "RotWK effect (state, weapon, spawn, damage: what the branch does); failing without the change; one test keeps a neighbour "
              "behaviour.", "",
              "## In game (for the check block)", ""]
    for (fac, obj, btn), (cat, _), _ in items[:6]:
        lines.append(f"- {fac.removeprefix('Faction')}: spawn `{obj}` (Cheat Mode, `docs/CHEAT_MODE.md`), press `{btn}`: it works as in "
                     f"RotWK (now {cat}).")
    lines += ["", "## Do not touch", "",
              "- Open tasks' areas: where two share a file, change only your own functions, members and rows; merge `origin/main` when one "
              "lands.",
              "- Any `*.hashes` file: if a pin moves (`perf1`, `uber1`, `save4*`, skirmish AI vs AI), name the cause in the PR; the owner's "
              "side re-measures.",
              "- `docs/STOPS.md`: only close or narrow existing rows; new rows go to `docs/stops.d/S-<id>.md` fragments with this issue's "
              "reserved ids (`docs/stops.d/RESERVED.tsv`): S-… .. S-….",
              "- `docs/ennorath/MATRIX.tsv` and the other generated maps: the maps run rewrites them after the landing.", "",
              "## Done", "",
              "Branch from `origin/main`; code, tests `PORT-<N>*`, the matrix before / after for the listed objects, PR with "
              "`Closes #<this issue>`. Report in Russian."]
    return "\n".join(lines) + "\n"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--ini", type=Path, help="the Ennorath INI archive: the lint drops the sweep's own errors")
    ap.add_argument("--out", type=Path, default=REPO / "workspace" / "issue-drafts" / "matrix")
    ap.add_argument("--top", type=int, default=20)
    ap.add_argument("--no-gh", action="store_true")
    a = ap.parse_args()
    st = mr.status(mr.rows_of((REPO / "docs/ennorath/MATRIX.tsv").read_text(encoding="utf-8")))
    red = {k: v for k, v in st.items() if v[0] not in mr.NOT_RED}
    flags = mr.lint(st, mr.Mod(a.ini)) if a.ini else {}
    gaps = gaps_rows()
    groups: dict[str, list] = collections.defaultdict(list)
    suspect = []
    for k, v in sorted(red.items()):
        if flags.get(k):
            suspect.append((k, v, flags[k]))
            continue
        row = gaps.get(k, {})
        cls = row.get("module class") or ""
        if cls in ("", "-"):  # the map knows no module class for the button: grouped by the failure, listed without a draft
            cls = f"(no module class: {v[0]})"
        groups[cls].append((k, v, row))
    issues = [] if a.no_gh else open_issues()
    ranked = []
    for cls, items in groups.items():
        fns = list(dict.fromkeys(f for _, _, row in items for f in functions(row)))
        ranked.append((cls, items, fns, covering(issues, cls if not cls.startswith("(") else "", [k[2] for k, _, _ in items])))
    ranked.sort(key=lambda g: (-len(g[1]), -len(g[2]), g[0]))
    a.out.mkdir(parents=True, exist_ok=True)
    for old in a.out.glob("*.md"):
        old.unlink()
    index = ["# Matrix task drafts (tools/mod/matrix_drafts.py)", "",
             f"{len(red)} red buttons: {len(red) - len(suspect)} in {len(ranked)} class groups, {len(suspect)} flagged by the lint "
             "(likely the sweep's own error, no draft). Drafts for the top groups; a group open tasks already name is marked.", "",
             "| # | class | red | missing RW functions | kinds | open tasks naming it | draft |", "|---|---|---|---|---|---|---|"]
    for n, (cls, items, fns, cover) in enumerate(ranked, 1):
        kinds = ", ".join(f"{k} {c}" for k, c in collections.Counter(v[0] for _, v, _ in items).most_common(3))
        name = ""
        if n <= a.top and not cls.startswith("("):
            name = f"{n:02d}-{re.sub(r'[^A-Za-z0-9]+', '-', cls)}.md"
            (a.out / name).write_text(draft(cls, items, fns, cover), encoding="utf-8")
        index.append(f"| {n} | `{cls}` | {len(items)} | {len(fns)} | {kinds} | {'; '.join(c.split(' ')[0] for c in cover[:4]) or '—'} | "
                     f"{name or '—'} |")
    if suspect:
        index += ["", "## Flagged by the lint (check the sweep first)", ""]
        index += [f"- {k[0].removeprefix('Faction')} `{k[1]}` `{k[2]}` ({v[0]}): {'; '.join(f)[:200]}" for k, v, f in suspect]
    (a.out / "index.md").write_text("\n".join(index) + "\n", encoding="utf-8")
    print(f"matrix drafts: {len(ranked)} groups, {min(a.top, len(ranked))} drafted, {len(suspect)} flagged -> {a.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
