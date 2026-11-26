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
        todo = [r for r in mine if r["status"] not in ("ported", "library") and r["depth"] == "0"]
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
    out += binary_section(fmap, classes, addrs)
    out += ["", "_Досье обновляется при правке задачи; полные списки — в картах `docs/ennorath/`._"]
    return "\n".join(out)


def bfme2_name(mangled: str) -> str:
    """`?evaluateBridgeBroken@ScriptConditions@@QAE_NPAVParameter@@@Z` -> `ScriptConditions::evaluateBridgeBroken`; other forms as given."""
    special = {"0": None, "1": "~", "_G": "deleting dtor", "_E": "deleting dtor", "4": "operator="}
    m = re.match(r"\?\?(_G|_E|[0-4])(\w+)@@", mangled or "")
    if m:
        cls, kind = m.group(2), special[m.group(1)]
        name = f"{cls}::" + (cls if kind is None else "~" + cls if kind == "~" else kind)
    else:
        m = re.match(r"\?([^@?]+)@(\w+)@@", mangled or "")
        name = f"{m.group(2)}::{m.group(1)}" if m else (mangled or "")
    # a BFME2 function without a name (match_rotwk.py's `rva...` placeholders) says nothing
    return "" if re.match(r"(?i)rva[0-9a-f]+::|\w+::rva[0-9a-f]+$", name) else name


def binary_section(fmap: list[dict], classes: list[str], addrs: set[int]) -> list[str]:
    """The whole-image map (RW_BINARY_MAP.tsv, 2026-11-24): the function each named address is in, with its class, table slot, callers and
    BFME2 pair; per named class, the class the binary map gives its functions and that class's virtual methods with no citation."""
    bmap = rows("RW_BINARY_MAP.tsv")
    if not bmap:
        return []
    import bisect
    starts = sorted((int(r["function"], 16), r) for r in bmap)
    keys = [s for s, _ in starts]

    def containing(a: int):
        i = bisect.bisect_right(keys, a) - 1
        if i >= 0 and a < starts[i][0] + int(starts[i][1]["size"] or 0):
            return starts[i][1]
        return None

    def line(r: dict) -> str:
        name = bfme2_name(r.get("BFME2 name", ""))
        return (f"`{rw(r['function'])}` {r['status']}" + (f" ({r['proof']})" if r.get("proof") else "") + (f", класс `{r['class']}` слот `{r['slot']}`" if r.get("slot") else "") +
                f", вызывающих {r['direct callers']} прямых / {r['resolved virtual callers']} виртуальных" +
                (f", BFME2 `{name}` ({r['match tier']})" if name else "") + (f", {r['subsystem']}" if r.get("subsystem") else ""))

    out = ["", "### Карта бинарника (`docs/ennorath/RW_BINARY_MAP.tsv`, `RW_CLASSES.tsv`)", ""]
    seen = set()
    for a in sorted(addrs)[:LIMIT]:
        r = containing(a)
        if r is None:
            out.append(f"- `RW 0x{a:06X}`: вне функций карты (данные или не код)")
        elif r["function"] not in seen:
            seen.add(r["function"])
            inner = "" if int(r["function"], 16) == a else f" (адрес задачи `RW 0x{a:06X}` внутри)"
            out.append(f"- {line(r)}{inner}")
    by_class = defaultdict(list)
    for r in bmap:
        if r.get("class"):
            by_class[r["class"]].append(r)
    by_start = {int(r["function"], 16): r for r in bmap}
    for c in classes:
        owners = Counter(by_start[int(f["function"], 16)]["class"] for f in fmap
                         if f["module"] == c and f.get("function", "").startswith("0x") and by_start.get(int(f["function"], 16), {}).get("class"))
        if not owners:
            continue
        owner = owners.most_common(1)[0][0]
        meth = by_class[owner]
        todo = [r for r in meth if r["status"] in ("none", "partial")]
        out.append(f"- `{c}` в карте бинарника — класс `{owner}`: {len(meth)} методов (таблицы и конструкторы), без ссылки {len(todo)}" +
                   (": " + ", ".join(f"`{rw(r['function'])}` `{r['slot']}`" + (f" {bfme2_name(r['BFME2 name'])}" if bfme2_name(r.get("BFME2 name", "")) else "")
                                     for r in todo[:LIMIT]) + (" …" if len(todo) > LIMIT else "") if todo else ""))
    return out if len(out) > 3 else []


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
