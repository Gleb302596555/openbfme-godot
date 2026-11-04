#!/usr/bin/env python3
"""The mod field audit (the owner's task of 2026-11-04, workspace/ennorath-audit/TASK-1-mod-data-audit.md): every field a mod's INI writes, and whether
this port reads it and uses it. A field the port does not use is a problem found before it shows in a game.

    field_audit.py --base <ROTWK_INSTALL> --overlay <mod install> --coverage <module-coverage.tsv> --out <dir> [--src engine/src] [--rw engine/data/rotwk-201]

The coverage table is the engine's own (`OPENBFME_MODULE_COVERAGE_OUT=<tsv> openbfme_tests.exe "-tc=module coverage: every registry class*"`, columns
class type status typed ai drawkind). A previous field-audit.tsv in --out is compared with the new one (field-audit-diff.md) before it is replaced.

The three sides:
  * the mod: the INI of the base archives with the overlay's over them (tools/mod/ennorath_graph.py's mount, #include and #define), every definition's
    fields; a module's fields (`Behavior = <Class> ModuleTag_x` .. End) are the class's, a field of a block nested in a module is `<Class>/<Key>`'s, a
    field of a block nested in a definition `/<Key>`'s;
  * RotWK: the binary's field tables (engine/data/rotwk-201/, read from game.dat by tools/rw_object_model/extract.py): the module classes' tables and
    the Object table. A field RotWK's table does not name is the mod's own error (RotWK refuses or skips it), not a port gap;
  * the port: the engine's coverage table says whether a module class has a runtime class (`ported`), only typed data (`parsed-only`) or nothing
    (`missing`, an UnportedModule is made), and whether its data is typed or raw (RawModuleData keeps the lines, no value is parsed). The source
    (engine/src) gives the rest: the data struct a class is bound to (bindTypedData<T>, bind<Runtime, Data>, bindRuntime<..>, bindDataProc), the field
    tables and the member each row stores into (offsetof(Struct, m_member), a row macro, a typedef), the block keywords the engine parses for real
    (INIBlockStubs.cpp: the others are recording stubs that keep no value).

USED means a READ of the member outside the field tables, the parse functions, the constructors and xfer/crc, in a file that can see the struct
(its header's file and every file including that header, transitively). Two structs with a member of the same name in one file still count each
other's reads: every used row names the first read so it can be checked.

Statuses, from the worst:
  missing-module   the class has no runtime class (an UnportedModule is made): nothing it says happens
  parsed-only-module  the class's data is typed but there is no runtime class: the values are parsed and dropped
  stub-block       the definition's block keyword is a recording stub: the block is skipped, no value is kept
  raw              the runtime class is ported but its data is raw: no value is parsed (unless a raw reader names the field: used-raw)
  struct-unknown   the coverage says typed, but no binding in the source names the data struct (the tool cannot follow it)
  no-table-row     the class's (or the block's) tables have no row of that name (the engine would refuse it: the tool missed a table, or a nested parser)
  unmapped-block   no struct of the port is known for that definition type (DEF_STRUCTS)
  parsed-unused    the value is parsed into a member that nothing reads
  used-raw         a raw class whose field name is read by name (a raw reader such as RawModuleData's find) in a file that names the class
  used             the member is read outside the parser
  not-in-rotwk     RotWK's table does not have the field: the mod's own error, RotWK does not use it either
  unknown-class    the class is not in RotWK's module registry
"""
from __future__ import annotations

import argparse
import csv
import json
import re
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ennorath_graph as eg  # noqa: E402

MODULE_KEYS = {"behavior", "body", "draw", "clientupdate", "clientbehavior"}
# keywords that open nothing although they have no '=' (a one-line form; ThingFactory.cpp's keyword table)
ONE_LINE = {"removemodule"}
# the definition types folded into another (ChildObject and ObjectReskin are Objects)
FOLD = {"childobject": "object", "objectreskin": "object"}

# The port's structs of a definition type's own fields (the struct a block keyword's parser fills). Object's fields are not in a struct: ThingFactory
# keeps them by RotWK's Object table and readers ask for them by name (ThingTemplate::findField).
DEF_STRUCTS = {
    "weapon": ["WeaponTemplate"],
    "armor": ["ArmorTemplate"],
    "damagefx": ["DamageFX"],
    "locomotor": ["LocomotorTemplate"],
    "upgrade": ["UpgradeTemplate"],
    "science": ["ScienceInfo"],
    "specialpower": ["SpecialPowerTemplate"],
    "commandbutton": ["CommandButton"],
    "commandset": ["CommandSet"],
    "fxlist": ["FXList"],
    "fxparticlesystem": ["FXParticleSystemTemplate"],
    "playertemplate": ["PlayerTemplate"],
    "modifierlist": ["AttributeModifier", "ModifierList"],
    "experiencelevel": ["ExperienceLevel"],
    "stancetemplate": ["StanceTemplate"],
    "crate": ["CrateTemplate"],
    "objectcreationlist": ["ObjectCreationList"],
}

# The files of a definition type's parser beyond the ones registering its keyword (registerBlock): where its tables and nested blocks' tables live
# (checked by hand against the parse functions the registerBlock lambdas call).
DEF_FILES = {
    "weapon": ["GameLogic/Object/Weapon.cpp", "GameLogic/Object/WeaponNugget.cpp"],
    "fxlist": ["GameClient/FXList.cpp"],
    "fxparticlesystem": ["GameClient/FXParticleSystem.cpp"],
    "objectcreationlist": ["GameLogic/ObjectCreationList.cpp"],
    "commandset": ["GameClient/ControlBarCommands.cpp"],
    "commandbutton": ["GameClient/ControlBarCommands.cpp"],
    "audiosettings": ["Common/Audio/AudioSettings.cpp"],
    "miscaudio": ["Common/Audio/MiscAudio.cpp"],
    "audioevent": ["Common/Audio/AudioEventInfo.cpp"],
    "ambientstream": ["Common/Audio/AudioEventInfo.cpp"],
    "streamedsound": ["Common/Audio/AudioEventInfo.cpp"],
    "musictrack": ["Common/Audio/AudioEventInfo.cpp"],
    "dialogevent": ["Common/Audio/AudioEventInfo.cpp"],
    "multisound": ["Common/Audio/AudioEventInfo.cpp"],
    "newevaevent": ["Common/Audio/EvaEvents.cpp"],
    "predefinedevaevent": ["Common/Audio/EvaEvents.cpp"],
    "miscevadata": ["Common/Audio/EvaEvents.cpp"],
    "scoredkillevaannouncer": ["Common/Audio/EvaEvents.cpp"],
    "largegroupaudiomap": ["Common/Audio/LargeGroupAudio.cpp"],
    "largegroupaudiounusedknownkeys": ["Common/Audio/LargeGroupAudio.cpp"],
    "strategichud": ["GameClient/GUI/AptScreens/StrategicHUDData.cpp"],
    "awardsystem": ["GameLogic/CreateAHeroSystem.cpp"],
    # GameData is read in parts, each reader with its own table and the other rows skipped
    "gamedata": ["GameClient/CameraSettings.cpp", "GameClient/MapObjectDrawables.cpp", "GameClient/TutorialSettings.cpp", "GameLogic/AI/AIPathfindConfig.cpp",
                 "GameLogic/AI/AIWorld.cpp", "GameLogic/EconomySettings.cpp", "GameLogic/GameLogicSettings.cpp", "GameLogic/ProductionSettings.cpp",
                 "GameLogic/System/VisionSettings.cpp", "GameNetwork/NetworkSettings.cpp"],
}
# CommandSet's slot keys "1" .. "33" are generated (RW 0xC4F3D8; ControlBarCommands.cpp SetTable): every one stores CommandSet::m_command
COMMANDSET_SLOTS = 33


# ---- the mod side --------------------------------------------------------------------------------------------------------------------------------
class ModField:
    __slots__ = ("count", "defs", "values", "src")

    def __init__(self):
        self.count = 0
        self.defs: set[str] = set()
        self.values: dict[str, int] = defaultdict(int)
        self.src = ""


def mod_fields(vfs, block_types: set[str], diag: dict | None = None, grammar: "Grammar | None" = None) -> dict[tuple[str, str, str], ModField]:
    """(block type, owner, field) -> what the mod writes. The owner is the module class (with /<nested key> inside a nested block), /<key> for a
    block nested in the definition, or "" for the definition's own fields. Where RotWK's grammar knows the open block (a module class, Object and
    the sub-tables of both), a line without '=' is what the table says: a field (`ParticleSysBone Bone Fx`), a sub-block or a script (BeginScript up
    to EndScript, one field); elsewhere a line without '=' opens a nested block. diag gets the definitions whose blocks did not close and the module
    classes' definitions."""
    out: dict[tuple[str, str, str], ModField] = defaultdict(ModField)
    diag = diag if diag is not None else {}
    diag.setdefault("unclosed", [])
    modules = diag.setdefault("modules", defaultdict(set))
    classes = grammar.classes if grammar else {}

    def order(nm):
        return (0, nm) if nm.startswith("data/ini/default/") else ((2, nm) if nm.startswith("data/ini/object/") else (1, nm))

    for nm in sorted(vfs.files, key=order):
        if not nm.endswith(".ini") or nm.startswith(("data/ini/campaigns", "data/ini/mappedimages", "data/ini/livingworld")):
            continue
        lines = eg.expand(vfs, nm, {})
        i, n = 0, len(lines)
        while i < n:
            text, src = lines[i]
            toks = text.split()
            if "=" in text or not toks or toks[0].lower() not in block_types:
                i += 1
                continue
            typ = FOLD.get(toks[0].lower(), toks[0].lower())
            defname = toks[1] if len(toks) > 1 else ""
            root = grammar.object if grammar and typ == "object" else None
            stack: list[tuple[str, dict | None]] = []  # the open blocks inside the definition: (owner of the fields inside, its grammar node)

            def add(owner, key, value, s):
                f = out[(typ, owner, key)]
                f.count += 1
                f.defs.add(defname)
                f.values[value.strip()[:60]] += 1
                f.src = f.src or s

            i += 1
            while i < n:
                t, s = lines[i]
                tk = t.split()
                if t.startswith("#"):
                    diag.setdefault("directives", []).append(f"{s}: {t[:80]}")  # a directive expand() could not apply (a broken #include)
                    i += 1
                    continue
                owner, node = stack[-1] if stack else ("", root)
                key = t.split("=", 1)[0].strip() if "=" in t else tk[0]
                value = t.split("=", 1)[1] if "=" in t else t[len(tk[0]):]
                kind = node.get(key.lower()) if node is not None else None
                if "=" not in t and tk[0].lower() in block_types and kind is None:
                    if stack:
                        diag["unclosed"].append(f"{typ} {defname} ({src}; open: {' > '.join(o for o, _ in stack)} at {s})")
                    break  # the next definition (a definition without a final End)
                vt = value.split()
                if key.lower() in MODULE_KEYS and len(vt) >= 2:  # `Behavior = <Class> <tag>`: the tag is any name (Draw_PalanrirWatcherEffects)
                    stack.append((vt[0], classes.get(vt[0])))  # the module class
                    modules[vt[0]].add(defname)
                elif kind is not None and kind[0] == "script":
                    j = i + 1
                    while j < n and lines[j][0].split()[0].lower() != "endscript":
                        j += 1
                    add(owner, key, " ".join(x[0] for x in lines[i + 1:j]), s)
                    i = j
                elif kind is not None and kind[0] == "block":
                    add(owner, key, value, s)
                    if opens_block(lines, i + 1, kind[1], node):
                        stack.append((owner + "/" + key, kind[1]))
                elif kind is not None or "=" in t:
                    add(owner, key, value, s)
                    if kind is None and key.lower() in eg.BLOCK_KEYS:
                        stack.append((owner + "/" + key, None))
                elif tk[0].lower() == "end":
                    if not stack:
                        i += 1
                        break
                    stack.pop()
                elif tk[0].lower() in ONE_LINE:
                    pass
                else:
                    # a nested block header the grammar does not know (a weapon's DamageNugget, an FXList's nugget, ...)
                    stack.append((owner + "/" + tk[0], None))
                i += 1
    return out


def block_end(lines, j: int, child: dict | None) -> int:
    """the index of the End closing a block-kind row's sub-table that starts at line j, or -1 when the row is one line: the lines up to that End
    must all be rows of the sub-table (a script row skips to its EndScript, a block row inside recurses). A block row can be one line (AddEmotion
    opens its nugget only after OVERRIDE, RW 0x8B618C), and then the next line is the parent's. Without a sub-table the next End closes it."""
    while j < len(lines):
        t = lines[j][0]
        key = (t.split("=", 1)[0] if "=" in t else t).split()
        if not key:
            j += 1
            continue
        k = key[0].lower()
        if k == "end":
            return j
        if child is None:
            j += 1
            continue
        if k not in child:
            return -1
        kind, grand = child[k]
        if kind == "script":
            while j < len(lines) and lines[j][0].split()[0].lower() != "endscript":
                j += 1
        elif kind == "block":
            e = block_end(lines, j + 1, grand)
            if e >= 0:
                j = e
        j += 1
    return -1


def opens_block(lines, j: int, child: dict | None, parent: dict | None = None) -> bool:
    """whether the block-kind row before line j opens its sub-table. The End found must also leave the parent open: the line after it is a row of
    the parent or the parent's End. (`AddEmotion = Alert_Base` as the module's last row is followed by the module's End, which the next line, a
    Behavior of the Object, proves is not the nugget's.)"""
    e = block_end(lines, j, child)
    if e < 0:
        return False
    if parent is None:
        return True
    for t, _ in lines[e + 1:]:
        key = (t.split("=", 1)[0] if "=" in t else t).split()
        if key:
            return key[0].lower() == "end" or key[0].lower() in parent
    return True


# ---- RotWK's tables --------------------------------------------------------------------------------------------------------------------------------
class Grammar:
    """RotWK's field tables as a tree: a node is {lower field name: (kind, child node or None)}; kind is line, block (a sub-table, the rows after
    it up to End) or script (the lines up to EndScript)"""

    def __init__(self, rw: Path):
        doc = json.loads((rw / "field-tables.json").read_text(encoding="utf-8"))
        self.tables, self.functions = doc["tables"], doc["functions"]
        self._nodes: dict[tuple[str, ...], dict] = {}
        registry = json.loads((rw / "module-registry.json").read_text(encoding="utf-8"))["classes"]
        self.classes = {c["name"]: self.node(tuple(t["table"] for t in c.get("tables", []))) for c in registry}
        self.object = self.node((doc["objectTable"]["table"],))

    def node(self, tables: tuple[str, ...]) -> dict:
        if tables in self._nodes:
            return self._nodes[tables]
        out: dict = {}
        self._nodes[tables] = out
        for t in tables:
            for row in self.tables.get(t, {}).get("rows", []):
                fn = self.functions.get(row["fn"], {})
                kind = fn.get("kind", "line")
                child = self.node(tuple(x["table"] for x in fn.get("tables", []))) if kind == "block" else None
                out.setdefault(row["name"].lower(), (kind, child))
        return out


def rotwk_tables(rw: Path) -> tuple[dict[str, set[str]], set[str]]:
    """(module class -> the field names its RotWK tables name, the Object table's names); lower case"""
    doc = json.loads((rw / "field-tables.json").read_text(encoding="utf-8"))
    tables = doc["tables"]
    registry = json.loads((rw / "module-registry.json").read_text(encoding="utf-8"))["classes"]
    out: dict[str, set[str]] = defaultdict(set)
    for c in registry:
        out[c["name"]]  # a class without a table is still a known class
        for ref in c.get("tables", []):
            for row in tables.get(ref["table"], {}).get("rows", []):
                out[c["name"]].add(row["name"].lower())
    obj = {row["name"].lower() for row in tables[doc["objectTable"]["table"]]["rows"]}
    return out, obj


# W3DDrawKind (W3DDrawModules.h): 0 model, 1 tree, 2 prop, 3 floor are drawn by the client's draw runtime from the typed data; 4 draws nothing
# (W3DDefaultDraw), 5 is an effect draw the engine does not draw (S-113)
DRAWN_KINDS = {"0", "1", "2", "3"}


def load_coverage(path: Path) -> dict[str, dict[str, str]]:
    """the engine's table; a Draw class the client draws (drawkind 0..3) is "ported": its runtime is the draw runtime, not a bound module proc"""
    with open(path, encoding="utf-8", newline="") as f:
        rows = list(csv.DictReader(f, delimiter="\t"))
    need = {"class", "type", "status", "typed", "drawkind"}
    if not rows or not need <= set(rows[0]):
        raise SystemExit(f"{path}: not the engine's module coverage table (columns {sorted(need)} needed)")
    out = {}
    for r in rows:
        r = dict(r)
        if r["type"] == "Draw" and r["status"] == "parsed-only" and r["drawkind"] in DRAWN_KINDS:
            r["status"] = "ported"
        out[r["class"]] = r
    return out


# ---- the port's source -----------------------------------------------------------------------------------------------------------------------------
ALIAS_TYPEDEF = re.compile(r"\btypedef\s+(?:struct\s+|class\s+)?([\w:]+)\s+(\w+)\s*;")
ALIAS_USING = re.compile(r"\busing\s+(\w+)\s*=\s*([\w:]+)\s*;")
MACRO = re.compile(r"^\s*#define\s+(\w+)\s*\(([^)]*)\)(.*)$")
OFFSETOF = re.compile(r"offsetof\s*\(\s*([\w:]+)\s*,")
NAMEVAR = re.compile(r"\b(\w+)(?:\[\])?\s*=\s*\"(\w+)\"")
LOOPVAR = re.compile(r"for\s*\(\s*const\s+char\s*\*\s*(\w+)\s*:\s*\{([^}]*)\}")
BIND = re.compile(r"\b(\w+)\s*<\s*([\w:]+)(?:\s*,\s*([\w:]+))?\s*>\s*\(\s*(?:(\w+)\s*,\s*)?(\"\w+\"|\w+)\s*[,)]")
BIND_PROC = re.compile(r"\bbindDataProc\s*\(\s*(\"\w+\"|\w+)\s*,")
PROC_STRUCT = re.compile(r"&\s*([\w:]+)::buildFieldParse")
# { "Name", parser, user, offset } or { "Name", parser, offset-macro(...) }: an argument may hold a macro call with commas (UM_NAMES(a, b))
_ARG = r"(?:[^,{}()]|\((?:[^()]|\([^()]*\))*\))*"
ROW_BRACE = re.compile(r"\{\s*\"([A-Za-z0-9_]+)\"\s*,\s*&?\s*([\w:<>]+)\s*,\s*(?:" + _ARG + r",\s*)?(" + _ARG + r")\}")
ROW_CALL = re.compile(r"\b(\w+)\s*\(\s*(?:[\w:]+\s*,\s*)?\"([A-Z][A-Za-z0-9_]*)\"\s*,\s*&?\s*([\w:]+)\s*,([^;]*?)\)\s*[,;)}]")
ROW_STRUCT_FIRST = re.compile(r"\b(\w+)\s*\(\s*([\w:]+)\s*,\s*\"([A-Z][A-Za-z0-9_]*)\"\s*,\s*([\w.]+)\s*\)")
TABLE = re.compile(r"\bFieldParse\s+(\w+)\s*\[\s*\]\s*=")
FUNC = re.compile(r"^\s*(?:[\w:<>\*&,]+\s+)*?[\*&]?((?:\w+::)?~?\w+)\s*\(")
NOT_FUNC = {"if", "for", "while", "switch", "return", "sizeof", "static_assert", "catch", "namespace"}
STRUCT_DECL = re.compile(r"^\s*(?:template\s*<[^>]*>\s*)?(?:struct|class)\s+(?:\w+\s+)?(\w+)\s*(?:final\s*)?(?::\s*([^{;]+))?\{?\s*$")
INCLUDE = re.compile(r"^\s*#include\s+\"([^\"]+)\"")
STRING = re.compile(r"\"([A-Za-z0-9_]+)\"")
WRITE_AFTER = re.compile(r"\s*(?:[-+*/|&^]|<<|>>)?=(?!=)")


class Row:
    __slots__ = ("field", "parser", "member", "struct", "file", "table", "func", "line")

    def __init__(self, field, parser, member, struct, file, table, func, line):
        self.field, self.parser, self.member, self.struct = field, parser, member, struct
        self.file, self.table, self.func, self.line = file, table, func, line

    @property
    def loc(self):
        return f"{self.file}:{self.line}"


class Source:
    def __init__(self, root: Path):
        self.root = root
        self.files: dict[str, list[str]] = {}
        for p in sorted(root.rglob("*")):
            if p.suffix in (".cpp", ".h", ".hpp", ".inc"):
                self.files[p.relative_to(root).as_posix()] = p.read_text(encoding="utf-8", errors="replace").splitlines()
        self.rows: list[Row] = []
        self.row_lines: set[tuple[str, int]] = set()
        self.binds: dict[str, tuple[str, str]] = {}  # module class -> (data struct, file:line)
        self.funcs: dict[tuple[str, str], list[str]] = defaultdict(list)  # (file, [Struct::]function) -> body lines
        self.line_func: dict[str, list[str]] = {}  # file -> the function of every line ("" outside one)
        self.bases: dict[str, set[str]] = defaultdict(set)
        self.decl: dict[str, str] = {}  # struct -> the file declaring it
        self.includes: dict[str, set[str]] = defaultdict(set)
        self.macro_rows: dict[str, list] = defaultdict(list)  # a multi-line #define's rows (DM_ROWS(T)): expanded where the macro is used
        for f, lines in self.files.items():
            self._scan(f, lines)
        self._infer_custom_members()
        self._resolve_includes()
        self._visible: dict[str, set[str]] = {}
        self._words: dict[str, dict[str, list[int]]] = {}

    # -- scanning
    def _scan(self, f: str, lines: list[str]):
        text = "\n".join(lines)
        alias = {}
        for a in ALIAS_TYPEDEF.finditer(text):
            alias[a.group(2)] = a.group(1).split("::")[-1]
        for a in ALIAS_USING.finditer(text):
            alias[a.group(1)] = a.group(2).split("::")[-1]

        def res(s):
            s = s.split("::")[-1] if s else ""
            return alias.get(s, s)

        macros = {}
        for line in lines:
            m = MACRO.match(line)
            if m:
                o = OFFSETOF.search(m.group(3))
                macros[m.group(1)] = res(o.group(1)) if o and o.group(1) not in m.group(2) else ""
        names = {m.group(1): [m.group(2)] for m in NAMEVAR.finditer(text)}
        for m in LOOPVAR.finditer(text):
            names[m.group(1)] = STRING.findall(m.group(2))

        def class_names(arg):
            if not arg:
                return []
            return [arg.strip('"')] if arg.startswith('"') else names.get(arg, [])

        for m in BIND.finditer(text):
            fn, a, b, first, name = m.groups()
            if not (fn == "bindTypedData" or (b and fn.startswith(("bind", "register")))):
                continue
            line = text.count("\n", 0, m.start()) + 1
            for nm in class_names(first) + class_names(name):
                self.binds.setdefault(nm, (res(b or a), f"{f}:{line}"))
        for m in BIND_PROC.finditer(text):
            # bindDataProc(name, type, [](INI *ini) { ... initFromINIMultiProc(data, &Data::buildFieldParse) ... }): the last struct of the lambda
            body = text[m.end():m.end() + 1500].split("bindModuleProc", 1)[0]
            structs = PROC_STRUCT.findall(body)
            line = text.count("\n", 0, m.start()) + 1
            for nm in class_names(m.group(1)):
                if structs:
                    self.binds.setdefault(nm, (res(structs[-1]), f"{f}:{line}"))

        stack: list[str] = []  # the open braces: "ns" (namespace, extern), "class:<Name>" (a class body) or "code"
        func, table, pending, pending_class, pending_ns = "", None, "", "", False

        def code_depth():
            return sum(1 for k in stack if k == "code")

        line_func = []
        in_define = None  # (macro name, its parameters) inside a multi-line #define
        for n, line in enumerate(lines, 1):
            if in_define or (line.lstrip().startswith("#define") and line.rstrip().endswith("\\")):
                if not in_define:
                    dm = re.match(r"\s*#define\s+(\w+)\s*\(([^)]*)\)", line)
                    in_define = (dm.group(1), [x.strip() for x in dm.group(2).split(",")]) if dm else ("", [])
                for field, parser, member, struct in self._rows(line.split("//", 1)[0], macros, lambda x: x.split("::")[-1] if x else ""):
                    self.macro_rows[in_define[0]].append((field, parser, member, struct, in_define[1]))
                if not line.rstrip().endswith("\\"):
                    in_define = None
                line_func.append("")
                continue
            m = INCLUDE.match(line)
            if m:
                self.includes[f].add(m.group(1))
            code = line.split("//", 1)[0]
            m = STRUCT_DECL.match(code)
            if m:
                self.decl.setdefault(m.group(1), f)
                if code_depth() == 0:
                    pending_class = m.group(1)
                if m.group(2):
                    for b in re.sub(r"\b(public|private|protected|virtual)\b", "", m.group(2)).split(","):
                        b = b.strip().split("<")[0]
                        if b:
                            self.bases[m.group(1)].add(res(b))
            elif code_depth() == 0 and not code.lstrip().startswith("#"):
                fm = FUNC.match(code)
                if fm and fm.group(1).split("::")[-1] not in NOT_FUNC and not code.rstrip().endswith(";"):
                    pending = fm.group(1)
                    if "::" in pending:
                        cls, fn = pending.split("::", 1)
                        pending = f"{res(cls)}::{fn}"
                    else:
                        owner = next((k[6:] for k in reversed(stack) if k.startswith("class:")), "")
                        pending = f"{owner}::{pending}" if owner else pending
            t = TABLE.search(code)
            if t:
                table = t.group(1)
            for field, parser, member, struct in self._rows(code, macros, res):
                self.rows.append(Row(field, parser, member, struct, f, table or "", func or pending, n))
                self.row_lines.add((f, n))
            for mm in re.finditer(r"\b(\w+)\s*\(\s*([\w:]+)\s*\)", code):
                for field, parser, member, struct, params in self.macro_rows.get(mm.group(1), ()):
                    arg = res(mm.group(2))
                    self.rows.append(Row(field, parser, member, arg if not struct or struct in params else struct, f, table or "", func or pending, n))
                    self.row_lines.add((f, n))
            touched = func  # the function a one-line body (`{ p.add(kRows); }`) opens and closes on this line
            if code_depth() == 0 and re.search(r"^\s*(namespace\b|extern\s+\"C\")", code) and not code.rstrip().endswith(";"):
                pending_ns, pending = True, ""  # its { can be on the next line
            for ch in code:
                if ch == "{":
                    if code_depth() == 0 and pending_ns:
                        stack.append("ns")
                        pending_ns = False
                    elif code_depth() == 0 and pending_class and not pending:
                        stack.append("class:" + pending_class)
                        pending_class = ""
                    else:
                        if code_depth() == 0:
                            func = touched = pending
                        stack.append("code")
                elif ch == "}" and stack:
                    stack.pop()
                    if code_depth() == 0:
                        func, pending = "", ""
            cur = func or touched or ""
            line_func.append(cur)
            if cur:
                self.funcs[(f, cur)].append(code)
            if table and "};" in code:
                table = None
        self.line_func[f] = line_func

    def _infer_custom_members(self):
        """a row with its own parser and no member (`{ "FX", parseFX, nullptr, 0 }`): the members that parser's body names (m_*), in the row's
        file; several are joined with |"""
        by_short: dict[tuple[str, str], list[str]] = defaultdict(list)
        for (f, fn), body in self.funcs.items():
            by_short[(f, fn.split("::")[-1])] += body
        for r in self.rows:
            if r.member.startswith("m_"):
                continue
            body = by_short.get((r.file, r.parser.split("::")[-1]))
            if body:
                members = sorted(set(re.findall(r"\b(m_\w+)", "\n".join(body))))
                if members:
                    r.member = "|".join(members)

    @staticmethod
    def _rows(code, macros, res):
        out = []
        for r in ROW_BRACE.finditer(code):
            rest = r.group(3).strip()
            o = OFFSETOF.search(rest)
            struct = res(o.group(1)) if o else ""
            if not struct:
                mm = re.match(r"\(?\s*(?:\(\s*int\s*\)\s*)?(\w+)\s*\(", rest)
                struct = macros.get(mm.group(1), "") if mm else ""
            ids = re.findall(r"\b(m_\w+)", rest) or re.findall(r"(\w+)\s*\)*\s*$", rest)
            out.append((r.group(1), r.group(2), ids[-1] if ids else "", struct))
        if out:
            return out
        for r in ROW_CALL.finditer(code):
            macro, field, parser, rest = r.groups()
            if "parse" not in parser.lower() and macro not in macros:
                continue
            o = OFFSETOF.search(rest)
            struct = res(o.group(1)) if o else macros.get(macro, "")
            ids = re.findall(r"\b(m_\w+)", rest) or re.findall(r"(\w+)", rest)
            out.append((field, parser, ids[-1] if ids else "", struct))
        if out:
            return out
        for r in ROW_STRUCT_FIRST.finditer(code):
            # STR_ROW(W3DTruckDrawModuleData, "LeftFrontTireBone", m_leftFrontTireBone): a row macro of this file whose first argument is the struct
            macro, struct, field, member = r.groups()
            if macro in macros:
                out.append((field, macro, member.split(".")[-1], res(struct)))
        return out

    def _resolve_includes(self):
        by_suffix: dict[str, list[str]] = defaultdict(list)
        for f in self.files:
            parts = f.split("/")
            for i in range(len(parts)):
                by_suffix["/".join(parts[i:])].append(f)
        self.included_by: dict[str, set[str]] = defaultdict(set)
        for f, incs in self.includes.items():
            here = f.rsplit("/", 1)[0] if "/" in f else ""
            for inc in incs:
                inc = inc.replace("\\", "/")
                cands = [here + "/" + inc] if (here + "/" + inc) in self.files else by_suffix.get(inc, [])
                for c in cands:
                    self.included_by[c].add(f)

    # -- queries
    def visible(self, struct: str) -> set[str]:
        """the files that can see the struct: the file declaring it and every file including that one (transitively)"""
        if struct in self._visible:
            return self._visible[struct]
        h = self.decl.get(struct)
        out: set[str] = set()
        if h:
            todo = [h]
            while todo:
                x = todo.pop()
                if x in out:
                    continue
                out.add(x)
                todo += self.included_by.get(x, ())
        self._visible[struct] = out
        return out

    def ancestors(self, struct: str) -> set[str]:
        out, todo = set(), [struct]
        while todo:
            s = todo.pop()
            if s and s not in out:
                out.add(s)
                todo += self.bases.get(s, ())
        return out

    def rows_of_struct(self, struct: str) -> list[Row]:
        """the rows a struct parses: the rows storing into it or a base, and the tables its buildFieldParse/getFieldParse add (by name, in the file
        of the function first), recursively through the build functions they call"""
        structs = self.ancestors(struct)
        out = [r for r in self.rows if r.struct in structs]
        seen_funcs: set[tuple[str, str]] = set()
        todo = [(f, fn) for (f, fn) in self.funcs if fn.split("::")[0] in structs and fn.split("::")[-1] in ("buildFieldParse", "getFieldParse")]
        while todo:
            key = todo.pop()
            if key in seen_funcs:
                continue
            seen_funcs.add(key)
            f, fn = key
            out += [r for r in self.rows if r.file == f and r.func == fn]
            body = "\n".join(self.funcs.get(key, []))
            for a in re.finditer(r"\.add\(\s*&?\s*(\w+)", body):
                tname = a.group(1)
                same = [r for r in self.rows if r.table == tname and r.file == f]
                if not same:
                    # a table of another file only when no other file has a table of that name
                    files = {r.file for r in self.rows if r.table == tname}
                    same = [r for r in self.rows if r.table == tname] if len(files) == 1 else []
                out += same
            for c in re.finditer(r"(?:(\w+)::)?(buildFieldParse|getFieldParse|build\w*FieldParse|\w*[Ff]ields?)\s*\(", body):
                cls, fname = c.group(1), c.group(2)
                if cls:
                    todo += [(g, h) for (g, h) in self.funcs if h == f"{cls}::{fname}"]
                else:
                    todo += [(g, h) for (g, h) in self.funcs if g == f and h.split("::")[-1] == fname]
        uniq, seen = [], set()
        for r in out:
            if (r.file, r.line, r.field) not in seen:
                seen.add((r.file, r.line, r.field))
                uniq.append(r)
        return uniq

    def parser_rows(self, parser: str, file: str, depth: int = 2) -> list[Row]:
        """the rows a row's own parser reads its sub-block with: the tables and the X::fieldParse()/getFieldParse() functions its body names (the
        parser's file first), and the parsers it calls in turn (depth levels)"""
        short = parser.split("::")[-1]
        bodies = [b for (f, fn), b in self.funcs.items() if fn.split("::")[-1] == short and f == file]
        bodies = bodies or [b for (f, fn), b in self.funcs.items() if fn.split("::")[-1] == short]
        out: list[Row] = []
        for body in bodies:
            text = "\n".join(body)
            for m in re.finditer(r"\b(?:(\w+)::)?(\w*[Ff]ield[Pp]arse\w*|\w*[Ff]ields)\s*\(", text):
                fn = f"{m.group(1)}::{m.group(2)}" if m.group(1) else m.group(2)
                out += [r for r in self.rows if r.func == fn or r.func.endswith("::" + fn)]
            for m in re.finditer(r"\b(k\w+)\b", text):
                out += [r for r in self.rows if r.table == m.group(1)]
            if depth > 1:
                for m in set(re.findall(r"\b(parse\w+|Parse\w+)\s*\(", text)):
                    if m != short:
                        out += self.parser_rows(m, file, depth - 1)
        return out

    def words_of(self, f: str) -> dict[str, list[int]]:
        w = self._words.get(f)
        if w is None:
            w = defaultdict(list)
            for n, line in enumerate(self.files[f], 1):
                code = line.split("//", 1)[0]
                for x in set(re.findall(r"[A-Za-z_]\w*", code)):
                    w[x].append(n)
            self._words[f] = w
        return w

    def is_read(self, f: str, n: int, member: str, struct: str) -> bool:
        if (f, n) in self.row_lines:
            return False
        code = self.files[f][n - 1].split("//", 1)[0]
        if re.search(r"offsetof|_OFF\s*\(|_ROW\s*\(", code):
            return False
        fn = self.line_func[f][n - 1]
        short = fn.split("::")[-1].lower()
        cls = fn.split("::")[0] if "::" in fn else ""
        if "parse" in short or short in ("xfer", "crc", "xferimpl") or (cls and short == cls.lower()) or short.startswith("~"):
            return False
        for m in re.finditer(r"\b" + re.escape(member) + r"\b", code):
            before, after = code[:m.start()], code[m.end():]
            if WRITE_AFTER.match(after):
                continue  # a store (m_x = ..., m_x += ...)
            if re.match(r"\s*[;{\[]", after) and re.search(r"[\w>\*&]\s+[\*&]?\s*$", before) and not re.search(r"[(.>]\s*$", before.rstrip()[-1:] or " "):
                continue  # a declaration (Type m_x; Type m_x{...}; Type m_x[N])
            if re.match(r"\s*[({]", after) and re.search(r"[:,]\s*$", before) and not fn:
                continue  # a constructor's initialiser list outside a recorded body
            if re.match(r"\s*[,)]", after) and re.search(r"\b(?:struct|class)\b|^\s*[\w:<>]+\s+$", before):
                continue
            return True
        return False

    def member_read(self, member: str, struct: str) -> str:
        files = self.visible(struct) if struct else set()
        scope = sorted(files) if files else sorted(self.files)
        for f in scope:
            for n in self.words_of(f).get(member, ()):
                if self.is_read(f, n, member, struct):
                    return f"{f}:{n}"
        return ""

    def string_read(self, field: str, near: str = "") -> str:
        """the first source line naming "field" as a string outside the tables and the .inc field lists; with near, only in a file that also names
        near (a raw class's reader names its class)"""
        for f, n in self.string_lines(field):
            if not near or re.search(r"\b" + re.escape(near) + r"\b", self._text[f]):
                return f"{f}:{n}"
        return ""

    def string_lines(self, field: str) -> list[tuple[str, int]]:
        """the source lines naming "field" (any case) as a string literal outside the tables and the .inc field lists"""
        if not hasattr(self, "_strings"):
            self._strings: dict[str, list[tuple[str, int]]] = defaultdict(list)
            self._text = {f: "\n".join(lines) for f, lines in self.files.items()}
            for f, lines in self.files.items():
                if f.endswith(".inc"):
                    continue
                for n, line in enumerate(lines, 1):
                    if (f, n) not in self.row_lines:
                        for s in set(STRING.findall(line.split("//", 1)[0])):
                            self._strings[s.lower()].append((f, n))
        return self._strings.get(field.lower(), [])

    def block_files(self, keyword: str) -> set[str]:
        """the files registering a parser for the block keyword (registerBlock("Keyword", ...) or INIBlockStubs.cpp's keyword == "Keyword")"""
        if not hasattr(self, "_block_files"):
            self._block_files: dict[str, set[str]] = defaultdict(set)
            for f, lines in self.files.items():
                for line in lines:
                    for k in re.findall(r"registerBlock\(\s*\"(\w+)\"", line):
                        self._block_files[k.lower()].add(f)
        return self._block_files.get(keyword.lower(), set())

    def block_keywords(self) -> tuple[set[str], set[str]]:
        """(every RotWK block keyword, the ones the engine parses for real), lower case, from INIBlockStubs.cpp and the registerBlock calls"""
        stubs = "\n".join(self.files.get("Common/INI/INIBlockStubs.cpp", []))
        m = re.search(r"RotwkBlockKeywords\(\)\s*\{(.*?)\};", stubs, re.S)
        if not m:
            raise SystemExit("engine/src/Common/INI/INIBlockStubs.cpp: RotwkBlockKeywords not found")
        every = {k.lower() for k in STRING.findall(m.group(1))}
        real = {k.lower() for k in re.findall(r"else if \(keyword == \"(\w+)\"\)", stubs)}
        for lines in self.files.values():
            for line in lines:
                for k in re.findall(r"registerBlock\(\s*\"(\w+)\"", line):
                    real.add(k.lower())
        return every, real


# ---- STOPS.md ----------------------------------------------------------------------------------------------------------------------------------------
def load_stops(path: Path | None) -> list[tuple[str, str]]:
    if not path or not path.exists():
        return []
    out = []
    for line in path.read_text(encoding="utf-8").splitlines():
        m = re.match(r"\|\s*(S-\d+)\s*\|", line)
        if m:
            out.append((m.group(1), line))
    return out


def stops_for(stops, cls: str, field: str) -> str:
    if not stops or not cls:
        return ""
    hits = [sid for sid, line in stops if re.search(r"\b" + re.escape(cls) + r"\b", line) and (not field or re.search(r"\b" + re.escape(field) + r"\b", line))]
    return " ".join(hits[:3])


# ---- the join --------------------------------------------------------------------------------------------------------------------------------------
ORDER = ["missing-module", "parsed-only-module", "stub-block", "raw", "struct-unknown", "no-table-row", "unmapped-block", "parsed-unused", "dynamic-row",
         "used-raw", "used", "not-in-rotwk", "unknown-class"]


def audit(mod, rw, rw_object, coverage, src: Source, real_blocks: set[str], stops=()):
    rows = []
    rows_cache: dict[str, list[Row]] = {}

    def rows_of(struct):
        if struct not in rows_cache:
            rows_cache[struct] = src.rows_of_struct(struct)
        return rows_cache[struct]

    def files_of(structs):
        return {r.file for s in structs for r in rows_of(s)}

    def typed_status(field, structs, nested, files=(), path=()):
        cands = [r for s in structs for r in rows_of(s) if r.field.lower() == field.lower()] if not path else []
        how = "struct"
        if path:
            # a block nested in a module: follow the owner's row of each nested key to the tables its parser reads
            level = [r for s in structs for r in rows_of(s)]
            for key in path:
                heads = [r for r in level if r.field.lower() == key.lower()]
                level = [x for h in heads for x in src.parser_rows(h.parser, h.file)]
            cands = [r for r in level if r.field.lower() == field.lower()]
            how = "nested-parser"
        if not cands and (nested or files):
            # a nested block's rows, or a block parsed without one struct: by name, only in the files of the owner's own tables (its sub-parsers
            # live there) and the files registering the block's parser
            fs = files_of(structs) | set(files)
            cands = [r for r in src.rows if r.file in fs and r.field.lower() == field.lower()]
            how = "block-file"
            if not cands:
                # a table built at run time from a list of names (MiscAudio's fieldNames(), TransitionDamageFX's generated rows)
                hit = next((f"{f}:{n}" for f, n in src.string_lines(field) if f in fs), "")
                if hit:
                    return "dynamic-row", hit, "", "string"
        if not cands:
            return "no-table-row", "", "", how
        for r in cands:
            for m in r.member.split("|"):
                if not re.fullmatch(r"[A-Za-z_]\w{2,}", m) or m in ("nullptr", "NULL"):
                    continue
                use = src.member_read(m, r.struct or (structs[0] if structs else ""))
                if use:
                    return "used", use, m, how
        r = cands[0]
        return "parsed-unused", r.loc, r.member, how

    for (typ, owner, field), mf in mod.items():
        cls = owner.split("/")[0] if owner else ""
        nested = "/" in owner
        status, where, member, how, struct = "", "", "", "", ""
        if cls:
            cov = coverage.get(cls)
            if cls not in rw and not cov:
                status = "unknown-class"
            elif not nested and field.lower() not in rw.get(cls, set()):
                status = "not-in-rotwk"
            elif not cov or cov["status"] == "missing":
                status = "missing-module"
            elif cov["typed"] != "1":
                use = src.string_read(field, near=cls)
                status, where = ("used-raw", use) if use else ("raw", "")
                if cov["status"] == "parsed-only":
                    status = "parsed-only-module"
            else:
                struct = src.binds.get(cls, ("", ""))[0]
                if not struct:
                    status = "struct-unknown"
                else:
                    status, where, member, how = typed_status(field, [struct], nested, path=tuple(owner.split("/")[1:]))
                if cov["status"] == "parsed-only" and status in ("used", "parsed-unused"):
                    status = "parsed-only-module"
        elif typ == "object":
            if not nested and field.lower() not in rw_object:
                status = "not-in-rotwk"
            elif nested:
                structs = [s for s in NESTED_DEF_STRUCTS.get(owner.lower(), [])]
                if not structs:
                    status = "unmapped-block"
                else:
                    struct = structs[0]
                    status, where, member, how = typed_status(field, structs, False)
            else:
                use = src.string_read(field)
                status, where, how = ("used", use, "by-name") if use else ("parsed-unused", "", "by-name")
        elif typ not in real_blocks:
            status = "stub-block"
        elif typ == "commandset" and field.isdigit():
            struct, member, how = "CommandSet", "m_command", "slot-key"
            if not 1 <= int(field) <= COMMANDSET_SLOTS:
                status = "not-in-rotwk"
            else:
                where = src.member_read(member, struct)
                status = "used" if where else "parsed-unused"
        else:
            structs = DEF_STRUCTS.get(typ, [])
            files = src.block_files(typ) | {f for f in DEF_FILES.get(typ, []) if f in src.files}
            if not structs and not files:
                status = "unmapped-block"
            else:
                struct = structs[0] if structs else ""
                status, where, member, how = typed_status(field, structs, nested, files)
        top = sorted(mf.values.items(), key=lambda kv: -kv[1])[:3]
        rows.append({"status": status, "block": typ, "owner": owner, "field": field, "defs": len(mf.defs), "uses": mf.count, "struct": struct,
                     "member": member, "where": where, "how": how, "stop": stops_for(stops, cls, field), "values": " | ".join(v for v, _ in top),
                     "src": mf.src})
    rank = {s: i for i, s in enumerate(ORDER)}
    rows.sort(key=lambda r: (rank.get(r["status"], 99), -r["defs"], r["block"], r["owner"], r["field"]))
    return rows


# Object's nested blocks: the struct each one's parser fills (ThingFactory's keyword table; ArmorSet.cpp, WeaponSet.cpp, ...)
NESTED_DEF_STRUCTS = {
    "/armorset": ["ArmorTemplateSet"],
    "/weaponset": ["WeaponTemplateSet"],
    "/locomotorset": ["LocomotorSetEntry"],  # Locomotor.cpp's LocomotorSet table
}


# ---- the output ------------------------------------------------------------------------------------------------------------------------------------
COLS = ["status", "block", "owner", "field", "defs", "uses", "struct", "member", "where", "how", "stop", "values", "src"]


def module_queue(rows, coverage, mod_module_defs):
    """the mod's module classes with status missing / parsed-only / raw, by the number of the mod's definitions that use them"""
    out = []
    for cls, defs in mod_module_defs.items():
        cov = coverage.get(cls)
        st = "missing" if not cov else cov["status"]
        raw = cov is not None and cov["typed"] != "1"
        if st in ("missing", "parsed-only") or raw:
            out.append((cls, st if st != "ported" and st != "base-ai" else ("raw" if raw else st), len(defs), cov["type"] if cov else "?"))
    out.sort(key=lambda x: (-x[2], x[0]))
    return out


def write(rows, out: Path, queue, diag):
    out.mkdir(parents=True, exist_ok=True)
    old = {}
    prev = out / "field-audit.tsv"
    if prev.exists():
        with open(prev, encoding="utf-8", newline="") as f:
            for r in csv.DictReader(f, delimiter="\t"):
                old[(r["block"], r["owner"], r["field"])] = r.get("status", "")
    with open(prev, "w", encoding="utf-8", newline="\n") as f:
        f.write("\t".join(COLS) + "\n")
        for r in rows:
            f.write("\t".join(str(r[c]).replace("\t", " ").replace("\n", " ") for c in COLS) + "\n")
    counts = defaultdict(int)
    for r in rows:
        counts[r["status"]] += 1
    with open(out / "module-queue.tsv", "w", encoding="utf-8", newline="\n") as f:
        f.write("class\tstatus\tdefinitions\ttype\n")
        for q in queue:
            f.write("\t".join(map(str, q)) + "\n")
    with open(out / "field-audit.md", "w", encoding="utf-8", newline="\n") as f:
        f.write("# Field audit\n\n| status | fields |\n|---|---|\n")
        for k in ORDER:
            if counts.get(k):
                f.write(f"| {k} | {counts[k]} |\n")
        if diag.get("unclosed"):
            f.write(f"\n{len(diag['unclosed'])} definitions ended at a new definition with a block still open (first: {diag['unclosed'][0]})\n")
        f.write("\n## Module queue (missing, parsed-only, raw; by the mod's definitions)\n\n| class | status | definitions | type |\n|---|---|---|---|\n")
        for q in queue:
            f.write(f"| {q[0]} | {q[1]} | {q[2]} | {q[3]} |\n")
        for status in ORDER[:8]:
            part = [r for r in rows if r["status"] == status]
            if not part:
                continue
            f.write(f"\n## {status} ({len(part)})\n\n| block | owner | field | definitions | values | where | stop |\n|---|---|---|---|---|---|---|\n")
            for r in part:
                f.write(f"| {r['block']} | {r['owner']} | {r['field']} | {r['defs']} | {eg.md_escape(r['values'])} | {r['where']} | {r['stop']} |\n")
    changed = []
    if old:
        new = {(r["block"], r["owner"], r["field"]): r["status"] for r in rows}
        for k in sorted(set(old) | set(new)):
            if old.get(k) != new.get(k):
                changed.append((k, old.get(k, "-"), new.get(k, "-")))
        with open(out / "field-audit-diff.md", "w", encoding="utf-8", newline="\n") as f:
            f.write(f"# Field audit: changes against the previous run ({len(changed)})\n\n| block | owner | field | was | now |\n|---|---|---|---|---|\n")
            for (b, o, fl), was, now in changed:
                f.write(f"| {b} | {o} | {fl} | {was} | {now} |\n")
    return counts, changed


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--base", required=True, type=Path)
    ap.add_argument("--overlay", required=True, type=Path)
    ap.add_argument("--coverage", required=True, type=Path, help="the engine's module coverage table (see the docstring)")
    ap.add_argument("--out", required=True, type=Path)
    here = Path(__file__).resolve().parents[2]
    ap.add_argument("--src", type=Path, default=here / "engine" / "src")
    ap.add_argument("--rw", type=Path, default=here / "engine" / "data" / "rotwk-201")
    ap.add_argument("--stops", type=Path, default=here / "docs" / "STOPS.md")
    a = ap.parse_args()
    src = Source(a.src)
    every, real = src.block_keywords()
    vfs = eg.mount(a.base, a.overlay)
    diag: dict = {}
    mod = mod_fields(vfs, every, diag, Grammar(a.rw))
    rw, rw_object = rotwk_tables(a.rw)
    coverage = load_coverage(a.coverage)
    rows = audit(mod, rw, rw_object, coverage, src, real, load_stops(a.stops))
    counts, changed = write(rows, a.out, module_queue(rows, coverage, diag.get("modules", {})), diag)
    print(f"{len(rows)} fields of the mod; " + ", ".join(f"{k} {counts[k]}" for k in ORDER if counts.get(k)))
    print(f"bound classes {len(src.binds)}, table rows {len(src.rows)}, block keywords {len(every)} (parsed {len(every & real)})")
    if changed:
        print(f"{len(changed)} fields changed status against the previous run (field-audit-diff.md)")
    if diag.get("unclosed"):
        print(f"{len(diag['unclosed'])} definitions with a block left open (first: {diag['unclosed'][0]})")
    if eg.MISSING:
        print(f"{len(eg.MISSING)} #include targets not found (first: {eg.MISSING[0]})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
