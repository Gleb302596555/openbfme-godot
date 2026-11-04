#!/usr/bin/env python3
"""The mod field audit (the owner's task of 2026-11-04, workspace/ennorath-audit/TASK-1-mod-data-audit.md): every field a mod's INI writes, and whether
this port reads it and uses it. A field the port does not use is a problem found before it shows in a game.

    field_audit.py --base <ROTWK_INSTALL> --overlay <mod install> --out <dir> [--src engine/src] [--rw engine/data/rotwk-201]

The three sides:
  * the mod: the INI of the base archives with the overlay's over them (tools/mod/ennorath_graph.py's mount, #include and #define), every definition's
    fields; a module's fields (`Behavior = <Class> ModuleTag_x` .. End) are the class's, a field of a block nested in a module is `<Class>/<Key>`'s;
  * RotWK: the binary's field table of every module class (engine/data/rotwk-201/, read from game.dat by tools/rw_object_model/extract.py): a field
    RotWK's table does not name is the mod's own error (RotWK refuses or skips it), not a port gap;
  * the port: the source (engine/src). A module class bound with bindTypedData<T>("Class") parses its fields through T::buildFieldParse's tables
    ({ "Field", parser, user, OFF(m_member) }); a class without one is raw (RawModuleData: the lines are kept, no value is parsed). A typed field is
    USED when its member appears outside the table and its parser; a raw class's field when its name appears as a string in the source outside a
    table (a raw reader such as ThingTemplate::findField). bindModuleProc("Class") says the runtime class is ported.

Statuses, from the worst:
  missing-module   the class has no runtime class (an UnportedModule is made): nothing it says happens
  raw              the runtime class is ported but the class's data is raw: no value of it is parsed (unless a raw reader names it)
  parsed-unused    the value is parsed into a member that nothing else reads
  used             the member (or the raw name) is read outside the parser
  not-in-rotwk     RotWK's table does not have the field: the mod's own error, RotWK does not use it either
The match of a member to its uses is by name (a heuristic: two classes with a member of the same name count each other's uses); every used row
names the first use so a reader can check it.
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ennorath_graph as eg  # noqa: E402

MODULE_KEYS = {"behavior", "body", "draw", "clientupdate", "clientbehavior", "objectcreationlist"}
MODULE_KEYS.discard("objectcreationlist")
NESTED_KEYS = eg.BLOCK_KEYS


# ---- the mod side --------------------------------------------------------------------------------------------------------------------------------
class ModField:
    __slots__ = ("count", "defs", "values", "src")

    def __init__(self):
        self.count = 0
        self.defs: set[str] = set()
        self.values: dict[str, int] = defaultdict(int)
        self.src = ""


def mod_fields(vfs) -> dict[tuple[str, str, str], ModField]:
    """(block type, owner, field) -> what the mod writes. The owner is the module class (with /<nested key> inside a nested block) or "" for the
    definition's own fields."""
    out: dict[tuple[str, str, str], ModField] = defaultdict(ModField)
    names = sorted(vfs.files)

    def order(nm):
        return (0, nm) if nm.startswith("data/ini/default/") else ((2, nm) if nm.startswith("data/ini/object/") else (1, nm))

    for nm in sorted(names, key=order):
        if not nm.endswith(".ini") or nm.startswith(("data/ini/campaigns", "data/ini/mappedimages", "data/ini/livingworld")):
            continue
        lines = eg.expand(vfs, nm, {})
        i, n = 0, len(lines)
        while i < n:
            text, src = lines[i]
            toks = text.split()
            if "=" in text or len(toks) < 2 or toks[0].lower() not in eg.BLOCK_TYPES:
                i += 1
                continue
            typ = toks[0].lower()
            typ = "object" if typ == "childobject" else typ
            defname = toks[1]
            # the stack of open blocks inside the definition: (owner of the fields inside, opened by a module)
            stack: list[str] = []
            i += 1
            while i < n:
                t, s = lines[i]
                tk = t.split()
                if not tk:
                    i += 1
                    continue
                if "=" not in t and tk[0].lower() in eg.BLOCK_TYPES and len(tk) >= 2 and not stack:
                    break  # the next definition (a definition without a final End at depth 0)
                if "=" in t:
                    k, v = t.split("=", 1)
                    key = k.strip()
                    vt = v.split()
                    if vt and vt[-1].lower().startswith("moduletag_") and key.lower() in MODULE_KEYS:
                        stack.append(vt[0])  # the module class
                        i += 1
                        continue
                    owner = stack[-1] if stack else ""
                    f = out[(typ, owner, key)]
                    f.count += 1
                    f.defs.add(defname)
                    f.values[v.strip()[:60]] += 1
                    f.src = f.src or s
                    if key.lower() in NESTED_KEYS:
                        stack.append((stack[-1] if stack else "") + "/" + key)
                elif tk[0].lower() == "end":
                    if not stack:
                        i += 1
                        break
                    stack.pop()
                else:
                    # a nested block header without '=' (a weapon's DamageNugget, a Draw's ConditionState, ...)
                    stack.append((stack[-1] if stack else "") + "/" + tk[0])
                i += 1
    return out


# ---- RotWK's tables --------------------------------------------------------------------------------------------------------------------------------
def rotwk_tables(rw: Path) -> dict[str, set[str]]:
    """module class -> the field names its RotWK tables name (lower case)"""
    tables = json.loads((rw / "field-tables.json").read_text(encoding="utf-8"))["tables"]
    registry = json.loads((rw / "module-registry.json").read_text(encoding="utf-8"))["classes"]
    out: dict[str, set[str]] = defaultdict(set)
    for c in registry:
        for ref in c.get("tables", []):
            for row in tables.get(ref["table"], {}).get("rows", []):
                out[c["name"]].add(row["name"].lower())
    return out


# ---- the port's source -----------------------------------------------------------------------------------------------------------------------------
ROW = re.compile(r'\{\s*"([A-Za-z0-9_]+)"\s*,\s*([A-Za-z0-9_:<>]+)\s*,\s*[^,{}]*,\s*([^{}]*?)\}')
MEMBER = re.compile(r"([A-Za-z_][A-Za-z0-9_]*)\s*\)*\s*$")
TABLE = re.compile(r"FieldParse\s+(\w+)\s*\[\s*\]\s*=")
BIND_TYPED = re.compile(r'bindTypedData<\s*([\w:]+)\s*>\(\s*"(\w+)"')
BIND_PROC = re.compile(r'bindModuleProc\(\s*"(\w+)"')
BUILD = re.compile(r"(\w+)::buildFieldParse\s*\(\s*MultiIniFieldParse\s*&\s*(\w+)\s*\)\s*\{", re.S)
STRING = re.compile(r'"([A-Za-z0-9_]+)"')
WORD = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")


class Source:
    def __init__(self, root: Path):
        self.files: dict[str, list[str]] = {}
        for p in sorted(root.rglob("*")):
            if p.suffix in (".cpp", ".h"):
                self.files[str(p.relative_to(root.parent.parent)).replace("\\", "/")] = p.read_text(encoding="utf-8", errors="replace").splitlines()
        self.tables: dict[str, list[tuple[str, str, str, str]]] = {}  # table name -> [(field, parser, member, file:line)]
        self.table_lines: set[tuple[str, int]] = set()
        self.typed: dict[str, str] = {}  # class -> data struct
        self.ported: set[str] = set()
        self.builds: dict[str, list[str]] = defaultdict(list)  # data struct -> table names and base structs it adds
        self._scan()
        self._index_words()

    def _scan(self):
        for f, lines in self.files.items():
            text = "\n".join(lines)
            for m in BIND_TYPED.finditer(text):
                self.typed[m.group(2)] = m.group(1).split("::")[-1]
            for m in BIND_PROC.finditer(text):
                self.ported.add(m.group(1))
            for m in BUILD.finditer(text):
                body, depth, j = [], 1, m.end()
                while j < len(text) and depth:
                    depth += {"{": 1, "}": -1}.get(text[j], 0)
                    j += 1
                body = text[m.end():j]
                for a in re.finditer(r"\.add\(\s*&?(\w+)", body):
                    self.builds[m.group(1)].append(a.group(1))
                for b in re.finditer(r"(\w+)::buildFieldParse\(", body):
                    self.builds[m.group(1)].append(b.group(1))
            cur = None
            for n, line in enumerate(lines, 1):
                t = TABLE.search(line)
                if t:
                    cur = t.group(1)
                    self.tables.setdefault(cur, [])
                for r in ROW.finditer(line):
                    member = MEMBER.search(r.group(3).strip())
                    entry = (r.group(1), r.group(2), member.group(1) if member else "", f"{f}:{n}")
                    self.tables.setdefault(cur or f"{f}:{n}", []).append(entry)
                    self.table_lines.add((f, n))
                if cur and "};" in line:
                    cur = None

    def _index_words(self):
        """word -> [(file, line)] outside the tables (the uses of members and of raw field names)"""
        self.words: dict[str, list[tuple[str, int]]] = defaultdict(list)
        self.strings: dict[str, list[tuple[str, int]]] = defaultdict(list)
        for f, lines in self.files.items():
            for n, line in enumerate(lines, 1):
                if (f, n) in self.table_lines:
                    continue
                code = line.split("//", 1)[0]
                for w in set(WORD.findall(code)):
                    if w.startswith("m_") or w[:1].islower():
                        self.words[w].append((f, n))
                for s in set(STRING.findall(code)):
                    self.strings[s.lower()].append((f, n))

    def rows_of_struct(self, struct: str, seen=None) -> list[tuple[str, str, str, str]]:
        seen = seen or set()
        if struct in seen:
            return []
        seen.add(struct)
        out = []
        for item in self.builds.get(struct, []):
            if item in self.tables:
                out += self.tables[item]
            else:
                out += self.rows_of_struct(item, seen)
        return out

    def member_use(self, member: str, parser_names: set[str]) -> str:
        for f, n in self.words.get(member, []):
            line = self.files[f][n - 1]
            if any(p in line for p in parser_names) and "parse" in line.lower():
                continue  # the parser's own store
            if re.search(r"offsetof|_OFF\(|fieldOffset", line):
                continue
            return f"{f}:{n}"
        return ""

    def string_use(self, field: str) -> str:
        for f, n in self.strings.get(field.lower(), []):
            return f"{f}:{n}"
        return ""


# ---- the join --------------------------------------------------------------------------------------------------------------------------------------
def audit(mod, rw, src: Source):
    rows = []
    for (typ, owner, field), mf in mod.items():
        cls = owner.split("/")[0] if owner else ""
        nested = "/" in owner
        status, where, member = "", "", ""
        if cls:
            if cls not in rw:
                status = "unknown-class"
            elif not nested and field.lower() not in rw[cls]:
                status = "not-in-rotwk"
            elif cls not in src.ported:
                status = "missing-module"
            elif cls not in src.typed:
                use = src.string_use(field)
                status, where = ("used", use) if use else ("raw", "")
            else:
                struct = src.typed[cls]
                hits = [r for r in src.rows_of_struct(struct) if r[0].lower() == field.lower()]
                if not hits:
                    status = "nested" if nested else "no-table-row"
                else:
                    member = hits[0][2]
                    use = src.member_use(member, {hits[0][1].split("::")[-1]}) if member else ""
                    status, where = ("used", use) if use else ("parsed-unused", hits[0][3])
        else:
            # a definition's own field (Object, Weapon, Armor, CommandButton, ...): the tables that name it anywhere in the source
            hits = [r for rs in src.tables.values() for r in rs if r[0].lower() == field.lower()]
            if not hits:
                use = src.string_use(field)
                status, where = ("used", use) if use else ("no-reader", "")
            else:
                uses = [src.member_use(h[2], {h[1].split("::")[-1]}) for h in hits if h[2]]
                use = next((u for u in uses if u), "")
                member = hits[0][2]
                status, where = ("used", use) if use else ("parsed-unused", hits[0][3])
        top = sorted(mf.values.items(), key=lambda kv: -kv[1])[:3]
        rows.append({"block": typ, "owner": owner, "field": field, "uses": mf.count, "defs": len(mf.defs), "status": status, "member": member,
                     "where": where, "values": " | ".join(v for v, _ in top), "src": mf.src})
    order = {"missing-module": 0, "raw": 1, "no-reader": 2, "no-table-row": 3, "parsed-unused": 4, "nested": 5, "unknown-class": 6, "not-in-rotwk": 7, "used": 8}
    rows.sort(key=lambda r: (order.get(r["status"], 9), -r["defs"], r["block"], r["owner"], r["field"]))
    return rows


def write(rows, out: Path):
    out.mkdir(parents=True, exist_ok=True)
    cols = ["status", "block", "owner", "field", "defs", "uses", "member", "where", "values", "src"]
    with open(out / "field-audit.tsv", "w", encoding="utf-8", newline="\n") as f:
        f.write("\t".join(cols) + "\n")
        for r in rows:
            f.write("\t".join(str(r[c]).replace("\t", " ") for c in cols) + "\n")
    counts = defaultdict(int)
    for r in rows:
        counts[r["status"]] += 1
    with open(out / "field-audit.md", "w", encoding="utf-8", newline="\n") as f:
        f.write("# Field audit\n\n| status | fields |\n|---|---|\n")
        for k, v in sorted(counts.items(), key=lambda kv: -kv[1]):
            f.write(f"| {k} | {v} |\n")
        for status in ("missing-module", "raw", "no-reader", "no-table-row", "parsed-unused"):
            part = [r for r in rows if r["status"] == status]
            if not part:
                continue
            f.write(f"\n## {status} ({len(part)})\n\n| block | owner | field | definitions | values | where |\n|---|---|---|---|---|---|\n")
            for r in part:
                f.write(f"| {r['block']} | {r['owner']} | {r['field']} | {r['defs']} | {eg.md_escape(r['values'])} | {r['where']} |\n")
    return counts


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--base", required=True, type=Path)
    ap.add_argument("--overlay", required=True, type=Path)
    ap.add_argument("--out", required=True, type=Path)
    here = Path(__file__).resolve().parents[2]
    ap.add_argument("--src", type=Path, default=here / "engine" / "src")
    ap.add_argument("--rw", type=Path, default=here / "engine" / "data" / "rotwk-201")
    a = ap.parse_args()
    vfs = eg.mount(a.base, a.overlay)
    mod = mod_fields(vfs)
    src = Source(a.src)
    rows = audit(mod, rotwk_tables(a.rw), src)
    counts = write(rows, a.out)
    print(f"{len(rows)} fields of the mod; " + ", ".join(f"{k} {v}" for k, v in sorted(counts.items(), key=lambda kv: -kv[1])))
    print(f"typed classes {len(src.typed)}, ported runtime classes {len(src.ported)}, source tables {len(src.tables)}")
    if eg.MISSING:
        print(f"{len(eg.MISSING)} #include targets not found (first: {eg.MISSING[0]})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
