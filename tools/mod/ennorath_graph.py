#!/usr/bin/env python3
"""Ennorath audit (docs/ENNORATH_FACTION_PLAN.md, E1): the reachability graph of one faction, built from the mounted data.

Reads the .big archives of the clean 2.01 base and of the Ennorath overlay (layers in mount order, the overlay over the base), keeps
the INI tree in memory, resolves #include / #define and ChildObject, then walks from a PlayerTemplate through command sets, command
buttons, objects, upgrades, sciences, special powers and object creation lists. Nothing game-derived is written inside the repository:
the dumps go to the ignored workspace/ennorath-audit/ folder, only IDs reach docs/.

    ennorath_graph.py --base <ROTWK_INSTALL> --overlay <mod install> --faction FactionMen --out <dir>

The parse is a reachability tool, not the engine's parser: a block's fields are read as `Key = value` lines, nested blocks by the
`Name ... End` grammar; every identifier-looking token of a field value that names a known definition is an edge. The edge kind is the
field name, so a reader can see why an object is reachable.
"""
from __future__ import annotations

import argparse
import json
import posixpath
import re
import struct
import sys
from collections import defaultdict, deque
from pathlib import Path


def big_entries(path: Path):
    with open(path, "rb") as f:
        head = f.read(16)
        if head[:4] not in (b"BIGF", b"BIG4"):
            raise SystemExit(f"not a BIG archive: {path}")
        count = struct.unpack(">I", head[8:12])[0]
        out = []
        for _ in range(count):
            off, size = struct.unpack(">II", f.read(8))
            name = bytearray()
            while True:
                c = f.read(1)
                if c == b"\0":
                    break
                name += c
            out.append((name.decode("latin1").replace("\\", "/").lower(), off, size))
    return out


class Vfs:
    """INI text by virtual path; later layers replace earlier ones (base first, overlay last)."""

    def __init__(self):
        self.files: dict[str, tuple[Path, int, int, str]] = {}

    def add_archive(self, path: Path, label: str):
        for name, off, size in big_entries(path):
            if (name.endswith((".ini", ".inc", ".str")) and name.startswith("data/ini/")) or (name.startswith("bases/") and name.endswith(".bse")):
                self.files[name] = (path, off, size, label)

    def read(self, name: str) -> str | None:
        e = self.files.get(name.lower().replace("\\", "/"))
        if not e:
            return None
        with open(e[0], "rb") as f:
            f.seek(e[1])
            return f.read(e[2]).decode("latin1")

    def source(self, name: str) -> str:
        e = self.files.get(name.lower().replace("\\", "/"))
        return f"{e[3]}:{name}" if e else "?"


def mount(base: Path, overlay: Path, bfme2: Path | None = None) -> Vfs:
    """BFME2 1.06 first (lowest priority), RotWK 2.01 over it, the overlay over both: the retail mount order."""
    v = Vfs()
    if bfme2:
        for pat in ("ini.big", "_patch101.big", "_patch103.big", "bases.big"):
            for p in sorted(bfme2.glob("*.big"), key=lambda q: q.name.lower()):
                if p.name.lower() == pat:
                    v.add_archive(p, "bfme2/" + p.name)
    for pat in ("ini.big", "bases.big", "_patch201ini.big"):
        for p in sorted(base.glob("*.big"), key=lambda q: q.name.lower()):
            if p.name.lower() == pat:
                v.add_archive(p, "base/" + p.name)
    # the overlay's archives in case-insensitive name order; the folder's loose files are not read here
    for p in sorted(overlay.glob("*.big"), key=lambda q: q.name.lower()):
        v.add_archive(p, "overlay/" + p.name)
    return v


INCLUDE = re.compile(r'^\s*#include\s+"([^"]+)"', re.I)
DEFINE = re.compile(r"^\s*#define\s+(\S+)\s*(.*)$", re.I)


MISSING: list[str] = []


def expand(vfs: Vfs, name: str, defines: dict[str, str], seen: tuple = ()) -> list[tuple[str, str]]:
    """lines of a file with #include spliced in and #define applied: (text, 'file:line')."""
    text = vfs.read(name)
    if text is None:
        MISSING.append(name)
        return []
    out = []
    base_dir = name.rsplit("/", 1)[0] if "/" in name else ""
    for n, raw in enumerate(text.splitlines(), 1):
        line = re.split(r";|//", raw, maxsplit=1)[0].rstrip()
        if not line.strip():
            continue
        m = INCLUDE.match(line)
        if m:
            inc = m.group(1).replace("\\", "/").lower().lstrip("/")
            cand = inc if inc in vfs.files else posixpath.normpath(base_dir + "/" + inc).lower()
            if cand not in vfs.files:
                cand = posixpath.normpath("data/ini/" + inc)
            if cand in seen:
                continue
            out += expand(vfs, cand, defines, seen + (name,))
            continue
        m = DEFINE.match(line)
        if m:
            defines[m.group(1)] = m.group(2).strip()
            continue
        if defines and "(" not in line.split("=")[0]:
            for k in sorted(defines, key=len, reverse=True):
                if k in line:
                    line = re.sub(r"(?<![A-Za-z0-9_])" + re.escape(k) + r"(?![A-Za-z0-9_])", defines[k], line)
        out.append((line.strip(), f"{name}:{n}"))
    return out


BLOCK_TYPES = {
    "object", "childobject", "objectreskin", "commandset", "commandbutton", "upgrade", "science", "specialpower",
    "objectcreationlist", "playertemplate", "weapon", "locomotor", "fxlist", "armor", "attributemodifier", "modifierlist",
    "multisound", "sciencepurchasecommandset", "audioevent", "particlesystem", "damagefx", "stancetemplate",
    "experiencelevel", "mappedimage", "eva", "crate", "audiofile", "dialogevent", "firelogicsystem",
}


BLOCK_KEYS = {"conditionstate", "transitionstate", "aliasconditionstate", "idleanimationstate", "defaultconditionstate"}

STORED_TYPES = {"object", "objectreskin", "commandset", "commandbutton", "upgrade", "science", "specialpower", "objectcreationlist",
                "playertemplate", "weapon", "modifierlist", "fxlist"}


class Db:
    def __init__(self):
        self.defs: dict[tuple[str, str], dict] = {}  # (type, name) -> {fields, parent, src}

    def add(self, typ, name, fields, parent, src):
        self.defs[(typ, name)] = {"fields": fields, "parent": parent, "src": src}


def parse(lines: list[tuple[str, str]], db: Db):
    """A definition runs from its `Type Name` header to the next header: `Behavior = X ModuleTag_1` opens a block too, so the `End`
    count cannot be trusted without the engine's grammar. Fields are every `Key = value` line in between (nested keys flattened)."""
    n = len(lines)

    def header(i):
        text = lines[i][0]
        toks = text.split()
        if "=" in text or len(toks) < 2 or toks[0].lower() not in BLOCK_TYPES:
            return None
        return toks

    i = 0
    while i < n:
        toks = header(i)
        if toks is None:
            i += 1
            continue
        typ = toks[0].lower()
        name = toks[1]
        parent = toks[2] if typ == "childobject" and len(toks) > 2 else None
        src = lines[i][1]
        if typ == "childobject":
            typ = "object"
        fields: list[tuple[str, str, str]] = []
        module = ""
        depth = 0  # open blocks inside the definition, counted only to know where a module ends
        i += 1
        while i < n and header(i) is None:
            t = lines[i][0]
            tk = t.split()
            if "=" in t:
                k, v = t.split("=", 1)
                vt = v.split()
                opens = False
                if vt and vt[-1].lower().startswith("moduletag_"):
                    opens = True
                    if depth == 0:
                        module = vt[-1]
                elif k.strip().lower() in BLOCK_KEYS:
                    opens = True
                fields.append((k.strip(), v.strip(), module))
                if opens:
                    depth += 1
            elif tk:
                if tk[0].lower() == "end":
                    depth = max(0, depth - 1)
                    if depth == 0:
                        module = ""
                else:
                    depth += 1
            i += 1
        if typ in STORED_TYPES:
            db.add(typ, name, fields, parent, src)


LOAD_FILES_LAST = ("data/ini/object/",)


def load_db(vfs: Vfs) -> Db:
    db = Db()
    names = sorted(vfs.files)
    # retail order: default/, top-level files, then the object folders; .inc files are only read through #include
    def key(nm):
        if nm.startswith("data/ini/default/"):
            return (0, nm)
        if nm.startswith("data/ini/object/"):
            return (2, nm)
        return (1, nm)
    for nm in sorted(names, key=key):
        if not nm.endswith(".ini") or nm.startswith(("data/ini/campaigns", "data/ini/mappedimages", "data/ini/livingworld")):
            continue
        parse(expand(vfs, nm, {}), db)
    add_castle_templates(vfs, db)
    return db


def refpack(data: bytes) -> bytes:
    """EA RefPack (QFS) as the 'EAR\0' wrapper of a .bse / .map holds it: u32 LE size, then 0x10FB, a 3-byte BE size and the commands."""
    if data[:4] != b"EAR\x00":
        return data
    p = 4 + 4
    flags = data[p]
    if data[p + 1] != 0xFB:
        raise ValueError("not a RefPack stream")
    p += 2
    p += 4 if flags & 0x80 else 3
    if flags & 0x01:  # compressed size present
        p += 4 if flags & 0x80 else 3
    out = bytearray()
    n = len(data)
    while p < n:
        c0 = data[p]
        if c0 < 0x80:
            c1 = data[p + 1]
            nlit = c0 & 3
            ref = ((c0 & 0x60) << 3) + c1 + 1
            ln = ((c0 & 0x1C) >> 2) + 3
            p += 2
        elif c0 < 0xC0:
            c1, c2 = data[p + 1], data[p + 2]
            nlit = c1 >> 6
            ref = ((c1 & 0x3F) << 8) + c2 + 1
            ln = (c0 & 0x3F) + 4
            p += 3
        elif c0 < 0xE0:
            c1, c2, c3 = data[p + 1], data[p + 2], data[p + 3]
            nlit = c0 & 3
            ref = ((c0 & 0x10) << 12) + (c1 << 8) + c2 + 1
            ln = ((c0 & 0x0C) << 6) + c3 + 5
            p += 4
        elif c0 < 0xFC:
            nlit = ((c0 & 0x1F) << 2) + 4
            ref = 0
            ln = 0
            p += 1
        else:
            nlit = c0 & 3
            ref = 0
            ln = 0
            p += 1
        out += data[p:p + nlit]
        p += nlit
        if ln:
            for _ in range(ln):
                out.append(out[-ref])
        if c0 >= 0xFC:
            break
    return bytes(out)


def add_castle_templates(vfs: Vfs, db: Db):
    """A castle template (Bases/<N>/<N>.bse, named by CastleToUnpackForFaction) lists the objects its layout places. The .bse is a binary
    map chunk, so its object names are the printable strings that name a known object; a name that is only a prop shows up too, which the
    registry reports as a layout member rather than guessing at the entry kind."""
    names = {n.lower(): n for (t, n) in db.defs if t in ("object", "objectreskin")}
    for path in vfs.files:
        if not path.endswith(".bse"):
            continue
        tpl = path.split("/")[1]
        e = vfs.files[path]
        with open(e[0], "rb") as f:
            f.seek(e[1])
            blob = refpack(f.read(e[2]))
        found = []
        for m in re.finditer(rb"[A-Za-z][A-Za-z0-9_]{3,63}", blob):
            nm = names.get(m.group(0).decode("latin1").lower())
            if nm and nm not in found:
                found.append(nm)
        db.add("castletemplate", tpl, [("Entry", nm, "") for nm in found], None, e[3] + ":" + path)


WORD = re.compile(r"[A-Za-z_][A-Za-z0-9_\.\-]*")


SINGLETON_FIELDS = {"commandset", "displayname", "buildcost", "buildtime", "transportslotcount"}


def effective_fields(db: Db, typ: str, name: str, seen=None):
    seen = seen or set()
    d = db.defs.get((typ, name))
    if not d or name in seen:
        return []
    seen.add(name)
    inherited = effective_fields(db, typ, d["parent"], seen) if d["parent"] else []
    own = [(k, v, d["src"], m) for k, v, m in d["fields"]]
    # a child's module of the same tag replaces the parent's, and a top-level singleton field of the child replaces the parent's
    own_modules = {m for _k, _v, _s, m in own if m}
    own_single = {k.lower() for k, _v, _s, m in own if not m and k.lower() in SINGLETON_FIELDS}
    kept = [(k, v, s, m) for k, v, s, m in inherited if m not in own_modules and not (not m and k.lower() in own_single)]
    return kept + own



WALK_TYPES = ("castletemplate", "object", "objectreskin", "commandset", "commandbutton", "upgrade", "science", "specialpower", "objectcreationlist",
              "weapon", "modifierlist", "fxlist", "playertemplate")
# fields whose value must name a definition; a miss is an unresolved reference (reported, never guessed)
REF_FIELDS = {"object", "upgrade", "specialpower", "science", "commandset", "ocl", "objectcreationlist", "weapon",
              "projectileobject", "createobject", "objects", "spellbook", "spellbookmp", "purchasesciencecommandsetmp",
              "startingbuilding", "buildableheroesmp", "buildableringheroesmp", "commandsetupgrade", "triggeredby",
              "conflictswith", "requiredupgrade", "requiredsciences", "prerequisitesciences", "removesciences",
              "upgradename", "replacementcommandset", "objectreplacement"}
# fields that name objects as a test (filters, damage tables, fear lists, display text), not as content the faction can get
FILTER_FIELDS = {"query", "damagescalar", "attributemodifieraffects", "afraidof", "alwaysafraidof", "displayname", "equivalentto",
                 "bonusfortype", "morphcondition", "bannercarriersallowed", "prerequisites", "object_prereq", "resourcemodifierobjectfilter",
                 "description", "descriptionlabel", "texturename", "textlabel", "buttonimage", "tooltipname", "unittype",
                 "enemyobjectfilter", "targetobjectfilter", "affectsobjects", "scriptname", "kindof", "radiuscursortype"}


# PlayerTemplate fields skirmish does not read: the campaign / War of the Ring spell store and its tactical start (out of scope of the plan)
CAMPAIGN_ONLY_PT_FIELDS = {"spellbook", "purchasesciencecommandset", "startingunittacticalwotr"}


# a gate, not content: the sciences a special power or science needs. A power's modules run only once one of its sciences is reached
# through a purchase button, so the cast set's powers nobody can buy (the campaign spells in the same set) stay out of the graph.
GATE_FIELDS = {"requiredsciences", "prerequisitesciences", "requiredscience"}


def is_filter_field(leaf: str) -> bool:
    return leaf in FILTER_FIELDS or leaf in GATE_FIELDS or leaf.endswith(("filter", "filters")) or leaf.startswith("requirements")


IGNORE_TOKENS = {"yes", "no", "none", "any", "all", "x", "y", "z", "end", "true", "false", "self", "new", "original", "off", "on"}


def tokens_of(value: str):
    for t in WORD.findall(value):
        t = t.strip(".-")
        if t and t.lower() not in IGNORE_TOKENS and not t.isdigit():
            yield t


def kinds_of(db: Db, name: str) -> set[str]:
    out: set[str] = set()
    for k, v, _, _m in effective_fields(db, "object", name):
        if k.lower() == "kindof":
            out |= {t.upper() for t in v.replace("+", " ").replace("-", " ").split()}
    return out


def power_module_index(db: Db):
    """specialpower name (lower) -> [(object, module tag)]: the modules an object carries for that power, with inheritance."""
    idx = defaultdict(list)
    for (t, n) in db.defs:
        if t != "object":
            continue
        seen = set()
        for k, v, _src, m in effective_fields(db, "object", n):
            if m and k.rsplit("/", 1)[-1].lower() in ("specialpowertemplate", "specialpower") and (n, m, v) not in seen:
                seen.add((n, m, v))
                for tok in tokens_of(v):
                    idx[tok.lower()].append((n, m))
    return idx


def walk(db: Db, faction: str):
    index = defaultdict(list)  # lower name -> [(type, name)]
    for (t, n) in db.defs:
        if t in WALK_TYPES:
            index[n.lower()].append((t, n))
    modules_of_power = power_module_index(db)

    def power_gate(power_name: str):
        d = db.defs.get(("specialpower", power_name))
        if not d:
            return []
        out = []
        for k, v, _src, _m in effective_fields(db, "specialpower", power_name):
            if k.lower() in GATE_FIELDS:
                out += [t for t in tokens_of(v) if t.upper().startswith("SCIENCE_")]
        return out

    gated = defaultdict(set)  # object -> module tags that wait for their special power
    for lst in modules_of_power.values():
        for o, m in lst:
            gated[o].add(m)
    start = ("playertemplate", faction)
    if start not in db.defs:
        raise SystemExit(f"no PlayerTemplate {faction}")
    side = next((v for k, v, _s, _m in effective_fields(db, "playertemplate", faction) if k.lower() == "side"), "")
    reached = {start: []}  # node -> list of (from, via)
    order = [start]
    unresolved = []
    q = deque([start])

    def reach(h, node, key):
        if h not in reached:
            reached[h] = []
            order.append(h)
            q.append(h)
        if (node, key) not in reached[h]:
            reached[h].append((node, key))

    def fields_of(node):
        if node[0] == "module":
            obj, tag = node[1].split("/", 1)
            return [f for f in effective_fields(db, "object", obj) if f[3] == tag]
        out = effective_fields(db, node[0], node[1])
        if node[0] == "object":
            out = [f for f in out if not (f[3] and f[3] in gated[node[1]])]
        return out

    def drain():
        while q:
            step(q.popleft())

    def step(node):
        for key, val, src, _m in fields_of(node):
            leaf = key.rsplit("/", 1)[-1].lower()
            if is_filter_field(leaf):
                continue
            if node[0] == "playertemplate" and leaf in CAMPAIGN_ONLY_PT_FIELDS:
                continue
            toks = list(tokens_of(val))
            if leaf == "castletounpackforfaction":  # `<Side> <template>`: only the faction's own side unpacks
                toks = toks[1:2] if toks and toks[0].lower() == side.lower() else []
            for tok in toks:
                for h in index.get(tok.lower(), []):
                    if h == node:
                        continue
                    reach(h, node, key)
            if leaf in REF_FIELDS or (node[0] == "commandset" and leaf.isdigit()):
                for tok in tokens_of(val):
                    if not index.get(tok.lower()):
                        unresolved.append((node, key, tok, src))

    drain()
    # a special power's module runs only on an object the faction has, so the module joins when both the object and the power are reached
    changed = True
    while changed:
        changed = False
        for plow, lst in modules_of_power.items():
            for h in index.get(plow, []):
                if h[0] != "specialpower" or h not in reached:
                    continue
                for o, m in lst:
                    mod = ("module", f"{o}/{m}")
                    gate = power_gate(h[1])
                    open_ = not gate or any(("science", sc) in reached for sc in gate)
                    if open_ and ("object", o) in reached and mod not in reached:
                        reach(mod, h, "implemented-by")
                        changed = True
        drain()
    return reached, order, unresolved, modules_of_power



# ---- registry documents -------------------------------------------------------------------------------------------------------------

def leafmap(db: Db, typ: str, name: str, module: str | None = None):
    """leaf field -> [values] of an effective definition (one module's fields when `module` is given)."""
    out = defaultdict(list)
    for k, v, _src, m in effective_fields(db, typ, name):
        if module is not None and m != module:
            continue
        if module is None and m:
            continue
        out[k.rsplit("/", 1)[-1].lower()].append(v)
    return out


def first(d, key, default=""):
    return d.get(key, [default])[0]


def command_sets_of(db: Db, obj: str):
    """[(command set, condition)]: the object's own set and the sets its modules switch to (CommandSetUpgrade and similar)."""
    out = []
    for k, v, _src, m in effective_fields(db, "object", obj):
        if k.lower().endswith("commandset") and k.lower() not in ("weapontogglecommandset",) and v.split():
            cond = m or "base"
            if m:
                trig = [x[1] for x in effective_fields(db, "object", obj) if x[3] == m and x[0].lower() in ("triggeredby", "upgrade")]
                cond = m + (" <- " + "; ".join(trig) if trig else "")
            out.append((v.split()[0], k, cond))
        elif k.lower() == "weapontogglecommandset":
            out.append((v.split()[0], k, m or "base"))
    seen = set()
    uniq = []
    for x in out:
        if x not in seen:
            seen.add(x)
            uniq.append(x)
    return uniq


def set_buttons(db: Db, cs: str):
    rows = []
    for k, v, _src, _m in effective_fields(db, "commandset", cs):
        if k.isdigit():
            rows.append((int(k), v.split()[0] if v.split() else ""))
    return sorted(rows)


def button_info(db: Db, b: str):
    lm = leafmap(db, "commandbutton", b)
    cmd = first(lm, "command").split()[0] if lm.get("command") else "?"
    target = ""
    for key in ("object", "upgrade", "specialpower", "science", "commandset"):
        if lm.get(key):
            target = f"{key}={lm[key][0].split()[0]}"
            break
    return cmd, target, (lm.get("radiusCursorType".lower()) or [""])[0]


def md_escape(x: str) -> str:
    return x.replace("|", "/")


def spells_table(db: Db, reached, faction: str):
    pt = leafmap(db, "playertemplate", faction)
    store = first(pt, "purchasesciencecommandsetmp")
    book = first(pt, "spellbookmp")
    cast_set = first(leafmap(db, "object", book), "commandset") if book else ""
    cast_buttons = {b: slot for slot, b in set_buttons(db, cast_set)} if cast_set else {}
    powers_by_science = defaultdict(list)
    for (t, n) in db.defs:
        if t == "specialpower":
            for k, v, _s, _m in effective_fields(db, "specialpower", n):
                if k.lower() in GATE_FIELDS:
                    for tok in tokens_of(v):
                        powers_by_science[tok.upper()].append(n)
    rows = []
    for slot, b in set_buttons(db, store):
        cmd, target, _ = button_info(db, b)
        sci = target.split("=", 1)[1] if target.startswith("science=") else ""
        sd = leafmap(db, "science", sci) if sci else {}
        cost = first(sd, "sciencepurchasepointcostmp") or first(sd, "sciencepurchasepointcost")
        prereq = first(sd, "prerequisitesciences")
        powers = powers_by_science.get(sci.upper(), []) if sci else []
        cast = []
        for pw in powers:
            for cb in cast_buttons:
                if first(leafmap(db, "commandbutton", cb), "specialpower").split()[:1] == [pw]:
                    cast.append((cast_buttons[cb], cb, pw))
        mods = []
        for pw, _cb in [(c[2], c[1]) for c in cast] or [(p, "") for p in powers]:
            for k, v, _s, m in effective_fields(db, "object", book):
                if m and k.lower() in ("specialpowertemplate", "specialpower") and v.split()[:1] == [pw]:
                    tag = m
                    typ = next((x[1].split()[0] for x in effective_fields(db, "object", book) if x[3] == m and x[0].lower() == "behavior" and x[1].split()[-1] == m), "?")
                    refs = []
                    for k2, v2, _s2, m2 in effective_fields(db, "object", book):
                        if m2 == tag and not is_filter_field(k2.lower()) and k2.lower() not in ("behavior", "specialpowertemplate"):
                            refs += [f"{k2}={h[1]}" for h in index_lookup(db, v2) if h[0] in ("object", "objectcreationlist", "upgrade", "modifierlist")]
                    mods.append((pw, typ, tag, sorted(set(refs))))
        rows.append(dict(slot=slot, button=b, command=cmd, science=sci, cost=cost, prereq=prereq, powers=powers, cast=cast, modules=mods))
    return store, book, cast_set, rows


_INDEX = {}


def index_lookup(db: Db, value: str):
    if not _INDEX:
        for (t, n) in db.defs:
            _INDEX.setdefault(n.lower(), []).append((t, n))
    for tok in tokens_of(value):
        for h in _INDEX.get(tok.lower(), []):
            yield h



def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--base", required=True)
    ap.add_argument("--overlay", required=True)
    ap.add_argument("--bfme2", default=None, help="BFME2 1.06 install (lowest mount layer)")
    ap.add_argument("--faction", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    vfs = mount(Path(a.base), Path(a.overlay), Path(a.bfme2) if a.bfme2 else None)
    db = load_db(vfs)
    reached, order, unresolved, _mods = walk(db, a.faction)
    out = Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    rows = []
    for node in order:
        typ, name = node
        d = db.defs.get(node) or {"src": "module of " + node[1].split("/")[0], "parent": None}
        row = {"type": typ, "name": name, "src": d["src"], "parent": d["parent"], "via": [f"{f[0]}:{f[1]}.{k}" for f, k in reached[node][:6]]}
        if typ in ("object", "objectreskin"):
            row["kindof"] = sorted(kinds_of(db, name))
        rows.append(row)
    (out / "graph.json").write_text(json.dumps(rows, indent=1), encoding="utf-8")
    (out / "unresolved.json").write_text(json.dumps([{"node": f"{n[0]}:{n[1]}", "field": k, "token": t, "src": s} for n, k, t, s in unresolved], indent=1), encoding="utf-8")
    c = defaultdict(int)
    for node in order:
        c[node[0]] += 1
    (out / "missing-includes.txt").write_text(chr(10).join(sorted(set(MISSING))), encoding="utf-8")
    print("missing includes:", len(set(MISSING)))
    print(a.faction, dict(c), "unresolved:", len(unresolved))


if __name__ == "__main__":
    main()
