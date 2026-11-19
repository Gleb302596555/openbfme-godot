"""port_dossier.py: the dossier of a Port / Local task (owner 2026-11-19: agents spend most of their time reading RotWK before they port).

From the issue's text: the module classes it names (a word that is a module of docs/ennorath/RW_FUNCTION_MAP.tsv) and its `RW 0x...`
addresses. The dossier lists, from the maps in the repository:
  * the function map: per class, its functions by status and the depth-0 ones not ported yet (address, callers), and the status of every
    address the issue names that is a function start of the map;
  * the button matrix's red buttons whose module class is the task's (MATRIX_GAPS.tsv);
  * the mod's fields of those classes no reader uses yet (FIELD_GAPS.tsv);
  * the attribute modifier reads in those classes or at the named functions (MODIFIER_READERS.tsv), and the red buttons of those classes whose
    modifier lists change an attribute the port does not read there (MODIFIER_MATRIX.tsv).
The disassembly of the task's functions needs game.dat, which never goes to GitHub in the clear: sandbox 3's loop (tools/colab/autostart.d/
dossier.sh, tools/mod/dossier_disasm.py) pushes it encrypted with the arena key to refs/arena/dossier/<issue> and plain to the private Drive.

    port_dossier.py --issue <n>      (GITHUB_TOKEN, GITHUB_REPOSITORY): posts or updates the issue's `<!-- dossier -->` comment
    port_dossier.py --text <file>    prints the dossier of an issue text (no GitHub)
"""
from __future__ import annotations

import argparse
import csv
import json
import os
import re
import sys
import urllib.request
from collections import Counter, defaultdict
from pathlib import Path

DOCS = Path(__file__).resolve().parents[2] / "docs" / "ennorath"
CITE = re.compile(r"\bRW\s+0x([0-9A-Fa-f]{5,8})\b")
MARK = "<!-- dossier -->"
TASK = re.compile(r"^\s*(Port|Local)[\s-]+\d+", re.I)
LIMIT = 25  # rows per section: the comment stays readable; the full lists are in the maps


def rows(name: str) -> list[dict]:
    p = DOCS / name
    if not p.exists():
        return []
    with p.open(encoding="utf-8", newline="") as s:
        lines = [l for l in s if not l.startswith("#")]
    return list(csv.DictReader(lines, delimiter="\t", quoting=csv.QUOTE_NONE))


def classes_in(text: str, known: set[str]) -> list[str]:
    return sorted({w for w in re.findall(r"\b[A-Z][A-Za-z0-9]{3,}\b", text) if w in known})


def rw(addr: str) -> str:
    """`0x00892C33` -> `RW 0x892C33` (the citations' form)."""
    return f"RW 0x{int(addr, 16):06X}"


def dossier(text: str) -> str:
    fmap = rows("RW_FUNCTION_MAP.tsv")
    known = {r["module"] for r in fmap}
    classes = classes_in(text, known)
    addrs = {int(a, 16) for a in CITE.findall(text)}
    out = [MARK, "## Досье задачи (карты репозитория, автоматически)", ""]
    if not classes and not addrs:
        out.append("В тексте задачи нет ни класса модуля из карты функций, ни адреса `RW 0x...`: досье не собрано.")
        return "\n".join(out)
    out.append(f"Классы: {', '.join(f'`{c}`' for c in classes) or '—'}; адресов RW в задаче: {len(addrs)}. "
               "Дизассемблер функций задачи — файл `dossier-<номер issue>.txt` в файлах арены (выдаёт дашборд).")
    # the function map
    out += ["", "### Карта функций (`docs/ennorath/RW_FUNCTION_MAP.tsv`)", ""]
    by_start = {int(r["function"], 16): r for r in fmap if r.get("function", "").startswith("0x")}
    for c in classes:
        mine = [r for r in fmap if r["module"] == c]
        st = Counter(r["status"] for r in mine)
        todo = [r for r in mine if r["status"] != "ported" and r["depth"] == "0"]
        out.append(f"- `{c}`: {len(mine)} функций, " + ", ".join(f"{k} {v}" for k, v in st.most_common()) +
                   (f"; не перенесены (глубина 0): " + ", ".join(f"`{rw(r['function'])}`" + (f" ({r['status']})" if r["status"] != "none" else "")
                                                             for r in todo[:LIMIT]) + (" …" if len(todo) > LIMIT else "") if todo else ""))
    named = [(a, by_start[a]) for a in sorted(addrs) if a in by_start]
    if named:
        out.append("- адреса задачи, которые есть в карте: " + ", ".join(f"`RW 0x{a:06X}` {r['status']} ({r['module']})" for a, r in named[:LIMIT]))
    # the button matrix
    gaps = [r for r in rows("MATRIX_GAPS.tsv") if r.get("module class") in classes]
    if gaps:
        out += ["", f"### Красные кнопки матрицы этих классов (`MATRIX_GAPS.tsv`, {len(gaps)})", ""]
        out += [f"- {r['faction']} `{r['object']}` `{r['button']}`: {r['result']}" for r in gaps[:LIMIT]]
    # the field audit
    fields = [r for r in rows("FIELD_GAPS.tsv") if r.get("owner") in classes]
    if fields:
        out += ["", f"### Поля мода этих классов без читателя (`FIELD_GAPS.tsv`, {len(fields)})", ""]
        out += [f"- `{r['owner']} {r['field']}` ({r['status']}, {r['defs']} определений{', стоп ' + r['stop'] if r.get('stop') else ''})"
                for r in fields[:LIMIT]]
    # the modifier map
    funcs = {f"RW 0x{a:06X}" for a in addrs}
    mods = [r for r in rows("MODIFIER_READERS.tsv")
            if r.get("reached from (RW_FUNCTION_MAP module)") in classes or r.get("function") in funcs]
    if mods:
        out += ["", "### Чтения атрибутов-модификаторов (`MODIFIER_READERS.tsv`)", ""]
        out += [f"- {r['attribute']}: вызов `{r['call']}` в `{r['function']}` — {r['status']}" for r in mods[:LIMIT]]
    # the matrix <-> modifier link: red buttons of these classes whose modifier lists change an attribute the port does not read there
    unread = [r for r in rows("MODIFIER_MATRIX.tsv") if r.get("module class") in classes and r.get("attributes the port does not read")]
    if unread:
        out += ["", f"### Кнопки, чьи модификаторы порт не читает (`MODIFIER_MATRIX.tsv`, {len(unread)})", ""]
        out += [f"- {r['faction']} `{r['object']}` `{r['button']}`: списки {r['modifier lists']} меняют "
                f"{r['attributes the port does not read']} — чтение не перенесено" for r in unread[:LIMIT]]
    out += ["", "_Досье обновляется при правке задачи; полные списки — в картах `docs/ennorath/`._"]
    return "\n".join(out)


def api(path: str, token: str, method: str = "GET", body: dict | None = None):
    req = urllib.request.Request(f"https://api.github.com{path}", method=method, data=json.dumps(body).encode() if body else None,
                                 headers={"Authorization": f"Bearer {token}", "Accept": "application/vnd.github+json"})
    with urllib.request.urlopen(req) as r:
        data = r.read()
    return json.loads(data) if data else None


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--issue", type=int)
    ap.add_argument("--text", type=Path)
    a = ap.parse_args(argv)
    if a.text:
        print(dossier(a.text.read_text(encoding="utf-8")))
        return 0
    token, repo = os.environ["GITHUB_TOKEN"], os.environ["GITHUB_REPOSITORY"]
    issue = api(f"/repos/{repo}/issues/{a.issue}", token)
    if "pull_request" in issue or not TASK.match(issue.get("title") or ""):
        print(f"#{a.issue}: not a Port / Local task")
        return 0
    body = dossier((issue.get("title") or "") + "\n" + (issue.get("body") or ""))
    comments = api(f"/repos/{repo}/issues/{a.issue}/comments?per_page=100", token) or []
    old = next((c for c in comments if (c.get("body") or "").startswith(MARK)), None)
    if old and old["body"] == body:
        print(f"#{a.issue}: the dossier is up to date")
    elif old:
        api(f"/repos/{repo}/issues/comments/{old['id']}", token, method="PATCH", body={"body": body})
        print(f"#{a.issue}: the dossier updated")
    else:
        api(f"/repos/{repo}/issues/{a.issue}/comments", token, method="POST", body={"body": body})
        print(f"#{a.issue}: the dossier posted")
    return 0


if __name__ == "__main__":
    sys.exit(main())
