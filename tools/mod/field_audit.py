#!/usr/bin/env python3
"""The mod field audit (the owner's task of 2026-11-04, workspace/ennorath-audit/TASK-1-mod-data-audit.md): every field a mod's INI writes, and whether
this port reads it and uses it. A field the port does not use is a problem found before it shows in a game.

    field_audit.py --base <ROTWK_INSTALL> --overlay <mod install> --coverage <module-coverage.tsv> --out <dir> [--src engine/src] [--rw engine/data/rotwk-201]
                   [--manual <manual-check.tsv>]

--manual takes the hand-checked verdicts (columns block owner field status-tool verdict where note; verdict used, parsed-unused or not-read): the
verdict is the field's status, the tool's own goes to the tool column. A verdict the tool now disagrees with in a new way (its status is neither
the one the check saw nor the verdict) is listed in field-audit.md, with the rows that match no field and the ones skipped (`(75 fields)`).

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
(its header's file and every file including that header, transitively). Code only: text in quotes and comments, a local or a parameter of the
member's name and a declaration are no reads. A file naming the struct comes first; a read in a file that does not name it is marked INFERENCE
unless one class in the source declares that name. A block's parser consuming the local it parsed (initFromINI(&local, ..) then local.member)
is a read too, marked STAGED. A where with STAGED or INFERENCE is a lead to check, not a proof.

Known limit (KNOWN_LIMIT, also in field-audit.md): a member of another struct with the same name, read in a file that names our struct too,
counts as our read (`x.side` of some other struct in a file that also names ArmyDefinition); such a where is marked AMBIGUOUS. It can point at the other
struct's read; the status is wrong only when our member has no read of its own.

Statuses, from the worst:
  missing-module   the class has no runtime class (an UnportedModule is made): nothing it says happens
  parsed-only-module  the class's data is typed but there is no runtime class: the values are parsed and dropped
  stub-block       the definition's block keyword is a recording stub: the block is skipped, no value is kept (a block with a hand-written
                   reader of its own that does not ask for the field is parsed-unused, how own-reader-unasked: CursorFile.cpp's MouseCursor)
  raw              the runtime class is ported but its data is raw: no value is parsed (unless a raw reader names the field: used-raw)
  struct-unknown   the coverage says typed, but no binding in the source names the data struct (the tool cannot follow it)
  no-table-row     the class's (or the block's) tables have no row of that name and no catch-all row (the engine would refuse it: the tool missed a
                   table, or a nested parser)
  not-read         nothing reads the line: a hand check (--manual), or the block table's catch-all row takes the name and applies nothing
                   (how catch-all: GameLogicSettings.cpp's noteUnapplied for GameData, AIData and MultiplayerSettings, stop S-152)
  unmapped-block   no struct of the port is known for that definition type (DEF_STRUCTS)
  parsed-unused    the value is parsed into a member that nothing reads
  custom-parser    the row's own parser stores it where the tool cannot follow (no member named in its body): check by hand
  own-reader       a stub block read by a hand-written reader of its own that names the field (TerrainTypes.cpp): the use not followed
  dynamic-row      the row is made at run time from a name list (MiscAudio, "Color" + to_string(i)): parsed, the use not followed
  used-outside     a parsed-only class (no runtime class) whose data member another file reads unambiguously (W3DStreakDraw's data drawn by
                   GodotGameWorld.cpp): used, but outside the module; an AMBIGUOUS or INFERENCE read stays parsed-only-module with its where
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
    "waterset": ["WaterSetting"],
    "bridge": ["RoadType"],
    "road": ["RoadType"],
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
    "rank": ["GameLogic/RankInfo.cpp"],
    "crate": ["GameLogic/CrateTemplate.cpp"],
    "waterset": ["GameClient/MapWeather.cpp"],
    "bridge": ["GameClient/TerrainRoads.cpp"],
    "road": ["GameClient/TerrainRoads.cpp"],
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


def mod_fields(vfs, block_types: set[str], diag: dict | None = None, grammar: "Grammar | None" = None,
               line_rows: dict[str, set[str]] | None = None) -> dict[tuple[str, str, str], ModField]:
    """(block type, owner, field) -> what the mod writes. The owner is the module class (with /<nested key> inside a nested block), /<key> for a
    block nested in the definition, or "" for the definition's own fields. Where RotWK's grammar knows the open block (a module class, Object and
    the sub-tables of both), a line without '=' is what the table says: a field (`ParticleSysBone Bone Fx`), a sub-block or a script (BeginScript up
    to EndScript, one field); elsewhere a line without '=' opens a nested block when an End closes it (headers(); line_rows: the names the port's
    tables of a block type parse as one line, never a block). diag gets the definitions whose blocks did not close and the module classes'
    definitions."""
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
            # outside RotWK's grammar, the lines without '=' that open a block: the ones an End closes (the others are fields, `Side Mordor`)
            heads = None if root is not None else headers(lines, i + 1, block_types, (line_rows or {}).get(typ, set()))
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
                key, value = field_key(t)
                if node is None:
                    kind = None
                elif tk[0].lower() in block_types:
                    kind = node.get(key.lower())  # a definition keyword is a row only by name (a catch-all does not take the next definition)
                else:
                    kind = row_kind(node, key)
                if "=" not in t and tk[0].lower() in block_types and kind is None:
                    if stack:
                        diag["unclosed"].append(f"{typ} {defname} ({src}; open: {' > '.join(o for o, _ in stack)} at {s})")
                    break  # the next definition (a definition without a final End)
                vt = value.replace("=", " ").split()  # '=' is a separator: `Draw == W3DScriptedModelDraw ModuleTag_01` names W3DScriptedModelDraw
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
                    if opens_block(lines, i + 1, kind[1], node, classes):
                        stack.append((owner + "/" + key, kind[1]))
                elif kind is not None or ("=" in t and key.lower() != "end"):
                    add(owner, key, value, s)
                    if kind is None and key.lower() in eg.BLOCK_KEYS:
                        stack.append((owner + "/" + key, None))
                elif key.lower() == "end":
                    # INI's first token closes the block, the rest of the line is not read (ZH INI::initFromINIMulti; INI.cpp:699): a mod's
                    # `End            StateName    = Sword` (a lost line break) ends the block there
                    if not stack:
                        i += 1
                        break
                    stack.pop()
                elif tk[0].lower() in ONE_LINE:
                    pass
                elif heads is not None and i not in heads:
                    add(owner, key, value, s)  # a field written without '=' (ControlBarScheme's `ScreenCreationRes X:1024 Y:768`)
                else:
                    # a nested block header the grammar does not know (a weapon's DamageNugget, an FXList's nugget, ...)
                    stack.append((owner + "/" + tk[0], None))
                i += 1
    return out


def headers(lines, j: int, block_types: set[str], line_names: set[str] = frozenset()) -> set[int]:
    """a definition without RotWK's grammar (starting at line j): the lines without '=' that open a nested block. INI takes the first token for
    the field and '=' is optional, so `Side Mordor` is a field and `ImagePart` a block: which is which, only the Ends tell. The definition's last
    End is its own; every other End closes the innermost open line, a one-word one before others (ControlBarScheme's ImagePart after
    `Position X:0 Y:520`), and the lines it passes over are fields. A '=' line opening a state block (eg.BLOCK_KEYS) is always one; a name the
    port's tables parse as one line (line_names: StancesBehavior.cpp's AttributeModifier, INI::parseAsciiString) never is. The limit: a named
    block holding only lines of several words that no port table names is not told from them: the innermost is taken, as before"""
    span = []
    while j < len(lines):
        t = lines[j][0]
        tk = t.split()
        if tk and not t.startswith("#"):
            if "=" not in t and tk[0].lower() in block_types:
                break  # the next definition
            span.append((j, tk, "=" in t))
        j += 1
    last = max((k for k, tk, _ in span if tk[0].lower() == "end"), default=-1)
    open_: list[tuple[int, int, bool]] = []  # (line, tokens, forced)
    out: set[int] = set()
    for k, tk, eq in span:
        key = tk[0].lower() if not eq else tk[0].split("=")[0].lower()
        if key == "end":
            if k == last:
                break
            if not open_:
                continue
            if open_[-1][2]:
                open_.pop()  # a state block's End
                continue
            free = [x for x in open_ if not x[2]]
            pick = next((x for x in reversed(free) if x[1] == 1), free[-1])
            out.add(pick[0])
            del open_[open_.index(pick):]
        elif eq:
            if key in eg.BLOCK_KEYS:
                open_.append((k, len(tk), True))
        elif key not in ONE_LINE and key not in line_names:
            open_.append((k, len(tk), False))
    return out


def block_end(lines, j: int, child: dict | None, classes: dict | None = None) -> int:
    """the index of the End closing a block-kind row's sub-table that starts at line j, or -1 when the row is one line: the lines up to that End
    must all be rows of the sub-table (a script row skips to its EndScript, a block row inside recurses). A block row can be one line (AddEmotion
    opens its nugget only after OVERRIDE, RW 0x8B618C), and then the next line is the parent's. Without a sub-table the next End closes it. A
    module inside (`Behavior = <Class> <tag>` .. End: ReplaceModule's, AddModule's) is passed over up to its own End (module_end)."""
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
        if classes is not None and k in MODULE_KEYS and "=" in t:
            vt = t.split("=", 1)[1].replace("=", " ").split()
            if vt:
                j = module_end(lines, j + 1, classes.get(vt[0]))
                if j < 0:
                    return -1
                j += 1
                continue
        if row_kind(child, k) is None:
            return -1
        kind, grand = row_kind(child, k)
        if kind == "script":
            while j < len(lines) and lines[j][0].split()[0].lower() != "endscript":
                j += 1
        elif kind == "block":
            e = block_end(lines, j + 1, grand)
            if e >= 0:
                j = e
        j += 1
    return -1


def module_end(lines, j: int, node: dict | None) -> int:
    """the index of a module's End from line j: its block rows' sub-tables and its scripts are passed over; a line its class's table does not
    name (the mod's own error, or a class RotWK does not know) is one line"""
    while j < len(lines):
        t = lines[j][0]
        key = (t.split("=", 1)[0] if "=" in t else t).split()
        if key:
            k = key[0].lower()
            if k == "end":
                return j
            kind = row_kind(node, k) if node is not None else None
            if kind and kind[0] == "script":
                while j < len(lines) and lines[j][0].split()[0].lower() != "endscript":
                    j += 1
            elif kind and kind[0] == "block":
                e = block_end(lines, j + 1, kind[1])
                if e >= 0:
                    j = e
        j += 1
    return -1


def opens_block(lines, j: int, child: dict | None, parent: dict | None = None, classes: dict | None = None) -> bool:
    """whether the block-kind row before line j opens its sub-table. The End found must also leave the parent open: the line after it is a row of
    the parent or the parent's End. (`AddEmotion = Alert_Base` as the module's last row is followed by the module's End, which the next line, a
    Behavior of the Object, proves is not the nugget's.)"""
    e = block_end(lines, j, child, classes)
    if e < 0:
        return False
    if parent is None:
        return True
    for t, _ in lines[e + 1:]:
        key = (t.split("=", 1)[0] if "=" in t else t).split()
        if key:
            return key[0].lower() == "end" or row_kind(parent, key[0]) is not None
    return True


CATCH_ALL = "*"  # a grammar node's key when its table takes any name


def row_kind(node: dict, key: str):
    """the node's (kind, child) of a row name: its own row, else a catch-all table's line (End is never a row)"""
    k = key.lower()
    return node.get(k) or (node.get(CATCH_ALL) if k != "end" else None)


def field_key(text: str) -> tuple[str, str]:
    """the field name and the rest of an INI line: the name is the first token up to a blank or '=' (INI's field token, ZH INI.cpp's
    strtok(" \t\n=")), so `Envelope InitialOpacity = 0.0 ..` is Envelope's line"""
    m = re.match(r"\s*([^\s=]+)\s*=?(.*)", text)
    return (m.group(1), m.group(2)) if m else ("", "")


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
        # RW 0x73C13C (ThingFactory::buildFieldParse): Object is read by two tables, the Object table and the audio table (VoiceSelect, SoundOnDamaged ..)
        self.object = self.node((doc["objectTable"]["table"], doc["audioTable"]["table"]))

    def node(self, tables: tuple[str, ...]) -> dict:
        if tables in self._nodes:
            return self._nodes[tables]
        out: dict = {}
        self._nodes[tables] = out
        for t in tables:
            table = self.tables.get(t, {})
            if table.get("catchAll"):
                out[CATCH_ALL] = ("line", None)  # a table that takes any name (UnitSpecificSounds' sub-table: every line is a sound name and its value)
            for row in table.get("rows", []):
                fn = self.functions.get(row["fn"], {})
                kind = fn.get("kind", "line")
                subs = [x["table"] for x in fn.get("tables", [])]
                # a block whose table is picked by the value (MeleeBehavior = Amoeba, RW 0x86C30A): any of the dispatch tables' rows is a row of it
                subs += [x["table"] for alt in fn.get("dispatch", {}).get("tables", []) for x in alt]
                child = self.node(tuple(dict.fromkeys(subs))) if kind == "block" else None
                out.setdefault(row["name"].lower(), (kind, child))
        return out


def rotwk_tables(rw: Path) -> tuple[dict[str, set[str]], set[str]]:
    """(module class -> the field names its RotWK tables name, the names of the Object table and the audio table); lower case"""
    doc = json.loads((rw / "field-tables.json").read_text(encoding="utf-8"))
    tables = doc["tables"]
    registry = json.loads((rw / "module-registry.json").read_text(encoding="utf-8"))["classes"]
    out: dict[str, set[str]] = defaultdict(set)
    for c in registry:
        out[c["name"]]  # a class without a table is still a known class
        for ref in c.get("tables", []):
            for row in tables.get(ref["table"], {}).get("rows", []):
                out[c["name"]].add(row["name"].lower())
    obj = {row["name"].lower() for key in ("objectTable", "audioTable") for row in tables[doc[key]["table"]]["rows"]}
    return out, obj


# W3DDrawKind (W3DDrawModules.h): 0 model, 1 tree, 2 prop, 3 floor are drawn by the client's draw runtime from the typed data; 4 draws nothing
# (W3DDefaultDraw), 5 is an effect draw the engine does not draw (S-113)
DRAWN_KINDS = {"0", "1", "2", "3"}


def load_coverage(path: Path) -> dict[str, dict[str, str]]:
    """the engine's table; a Draw class the client draws (drawkind 0..3) is "ported": its runtime is the draw runtime, not a bound module proc"""
    with open(path, encoding="utf-8", newline="") as f:
        rows = list(csv.DictReader(f, delimiter="\t", quoting=csv.QUOTE_NONE))
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
ROW_BRACE = re.compile(r"\{\s*\"([A-Za-z0-9_%]+)\"\s*,\s*&?\s*([\w:<>]+)\s*,\s*(?:" + _ARG + r",\s*)?(" + _ARG + r")\}")
ROW_CALL = re.compile(r"\b(\w+)\s*\(\s*(?:([\w:]+)\s*,\s*)?\"([A-Z][A-Za-z0-9_%]*)\"\s*,\s*&?\s*([\w:]+)\s*,([^;]*?)\)\s*[,;)}]")
ROW_STRUCT_FIRST = re.compile(r"\b(\w+)\s*\(\s*([\w:]+)\s*,\s*\"([A-Z][A-Za-z0-9_%]*)\"\s*,\s*([\w.]+)\s*\)")
TABLE = re.compile(r"\bFieldParse\s+(\w+)\s*\[\s*\]\s*=")
# a table's catch-all row: no name, a parser (GameLogicSettings.cpp's `{ nullptr, noteUnapplied, nullptr, 0 }`); the end row has no parser
CATCH_ALL_ROW = re.compile(r"\{\s*(?:nullptr|NULL)\s*,\s*(?!(?:nullptr|NULL)\b)(\w+)\s*,")
FUNC = re.compile(r"^\s*(?:[\w:<>\*&,]+\s+)*?[\*&]?((?:\w+::)?~?\w+)\s*\(")
NOT_FUNC = {"if", "for", "while", "switch", "return", "sizeof", "static_assert", "catch", "namespace"}
STRUCT_DECL = re.compile(r"^\s*(?:template\s*<[^>]*>\s*)?(?:struct|class)\s+(?:\w+\s+)?(\w+)\s*(?:final\s*)?(?::\s*([^{;]+))?\{?\s*$")
INCLUDE = re.compile(r"^\s*#include\s+\"([^\"]+)\"")
STRING = re.compile(r"\"([A-Za-z0-9_]+)\"")


def code_only(lines: list[str]) -> list[str]:
    """the lines with comments (// and /* */, across lines) and the text of string and character literals blanked: a word inside a report line's
    text ("the human side's script library") is not a read of a member of that name. Positions and the quotes are kept."""
    out, in_block = [], False
    for line in lines:
        buf, i, n = [], 0, len(line)
        while i < n:
            c = line[i]
            if in_block:
                if line.startswith("*/", i):
                    in_block = False
                    buf.append("  ")
                    i += 2
                else:
                    buf.append(" ")
                    i += 1
            elif line.startswith("//", i):
                buf.append(" " * (n - i))
                break
            elif line.startswith("/*", i):
                in_block = True
                buf.append("  ")
                i += 2
            elif c in "\"'":
                j = i + 1
                while j < n and line[j] != c:
                    j += 2 if line[j] == "\\" else 1
                j = min(j, n - 1)
                buf.append(c + " " * (j - i - 1) + (line[j] if j > i else ""))
                i = j + 1
            else:
                buf.append(c)
                i += 1
        out.append("".join(buf))
    return out
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
        self.line_class: dict[str, list[str]] = {}  # file -> the innermost class of every line ("" outside one)
        self.bases: dict[str, set[str]] = defaultdict(set)
        self.decl: dict[str, str] = {}  # struct -> the file declaring it
        self.decl_all: dict[str, list[str]] = defaultdict(list)  # struct -> every file declaring a struct of that name
        self.includes: dict[str, set[str]] = defaultdict(set)
        self.macro_rows: dict[str, list] = defaultdict(list)  # a multi-line #define's rows (DM_ROWS(T)): expanded where the macro is used
        self.catch_alls: dict[tuple[str, str], tuple[str, int]] = {}  # (file, table) -> (the catch-all row's parser, its line)
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
                if o and re.search(r",\s*\w+\s*(?:\.|->)", m.group(3)[o.start():]):
                    macros[m.group(1)] = "?"  # ES_OFF(member) = offsetof(State, values.member): a member of a member, as offsetof(S, a.b) itself
        # the file's name arrays (`static const char *const kStates[] = { "Damaged", "ReallyDamaged", "Rubble" };`)
        arrays = {m.group(1): STRING.findall(m.group(2)) for m in re.finditer(r"\b(\w+)\s*\[\s*\w*\s*\]\s*=\s*\{([^{}]*)\}", text)}
        # and arrays of rows, by the first literal of each (AudioSettings.cpp's `const Row group[] = { { "MicrophoneMaxDistanceToCamera", .. }, .. };`)
        for m in re.finditer(r"\b(\w+)\s*\[\s*\w*\s*\]\s*=\s*\{\s*(\{.*?\})\s*,?\s*\};", text, re.S):
            arrays.setdefault(m.group(1), re.findall(r"\{\s*\"([A-Za-z0-9_%]+)\"", m.group(2)))
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

        line_func, line_class = [], []  # every line's function and innermost class ("" outside one)
        in_define = None  # (macro name, its parameters) inside a multi-line #define
        defines: dict[str, list[str]] = defaultdict(list)  # a multi-line #define's lines (its first holds the parameters)
        for n, line in enumerate(lines, 1):
            if in_define or (line.lstrip().startswith("#define") and line.rstrip().endswith("\\")):
                if not in_define:
                    dm = re.match(r"\s*#define\s+(\w+)\s*\(([^)]*)\)", line)
                    # a macro without parameters (FireWeaponWhenDamagedBehavior.cpp's `#define FWWD_ROWS \` of rows): None, expanded by its name
                    plain = re.match(r"\s*#define\s+(\w+)\s*\\", line)
                    in_define = ((dm.group(1), [x.strip() for x in dm.group(2).split(",")]) if dm else (plain.group(1), None) if plain
                                 else ("", []))
                for field, parser, member, struct in self._rows(line.split("//", 1)[0], macros, lambda x: x.split("::")[-1] if x else ""):
                    self.macro_rows[in_define[0]].append((field, parser, member, struct, in_define[1]))
                defines[in_define[0]].append(line.split("//", 1)[0].rstrip().rstrip("\\"))
                if not line.rstrip().endswith("\\"):
                    in_define = None
                line_func.append("")
                line_class.append("")
                continue
            m = INCLUDE.match(line)
            if m:
                self.includes[f].add(m.group(1))
            code = line.split("//", 1)[0]
            m = STRUCT_DECL.match(code)
            if m:
                self.decl.setdefault(m.group(1), f)
                if f not in self.decl_all[m.group(1)]:
                    self.decl_all[m.group(1)].append(f)
                if code_depth() == 0:
                    pending_class = m.group(1)
                if m.group(2):
                    for b in re.sub(r"\b(public|private|protected|virtual)\b", "", m.group(2)).split(","):
                        b = b.strip().split("<")[0]
                        if b:
                            self.bases[m.group(1)].add(res(b))
            elif code_depth() == 0 and not code.lstrip().startswith("#"):
                # a function a macro defines (W3DModelDraw.cpp's SHADOW_FIELD_PARSER(parseShadowSizeX, sizeX, INI::parseReal)): its body with the
                # arguments put in, under the name the argument gives it
                mm = re.match(r"\s*(\w+)\s*\((.*)\)\s*;?\s*$", code)
                if mm and mm.group(1) in defines:
                    head, *body = defines[mm.group(1)]
                    dm = re.match(r"\s*#define\s+\w+\s*\(([^)]*)\)", head)
                    params = [x.strip() for x in dm.group(1).split(",")] if dm else []
                    args = [x.strip() for x in mm.group(2).split(",")]
                    sub = dict(zip(params, args)) if len(args) == len(params) else {}
                    text = "\n".join(body)
                    name = next((sub[p] for p in params if p in sub and re.search(r"\b[\w:<>\*&]+\s+" + re.escape(p) + r"\s*\(", text)), "")
                    if name:
                        self.funcs[(f, name)] += [re.sub(r"\b\w+\b", lambda w: sub.get(w.group(0), w.group(0)), x) for x in body]
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
            c = CATCH_ALL_ROW.search(code) if table else None
            if c:
                self.catch_alls.setdefault((f, table), (c.group(1), n))
            for field, parser, member, struct in self._rows(code, macros, res):
                self.rows.append(Row(field, parser, member, struct, f, table or "", func or pending, n))
                self.row_lines.add((f, n))
            for field, parser, member, struct in self._generated_rows(f, lines, n, code, arrays, macros, res):
                self.rows.append(Row(field, parser, member, struct, f, table or "", func or pending, n))
                self.row_lines.add((f, n))
            for name, mrows in self.macro_rows.items():
                if name and mrows[0][4] is None and not code.lstrip().startswith("#") and re.search(r"\b" + name + r"\b", code):
                    for field, parser, member, struct, _ in mrows:
                        self.rows.append(Row(field, parser, member, res(struct), f, table or "", func or pending, n))
                        self.row_lines.add((f, n))
            for mm in re.finditer(r"\b(\w+)\s*\(\s*([\w:]+)\s*\)", code):
                for field, parser, member, struct, params in self.macro_rows.get(mm.group(1), ()):
                    if params is None:
                        continue
                    arg = res(mm.group(2))
                    self.rows.append(Row(field, parser, member, "?" if struct == "?" else (arg if not struct or struct in params else struct), f, table or "", func or pending, n))
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
            line_class.append(next((k[6:] for k in reversed(stack) if k.startswith("class:")), ""))
            if cur:
                self.funcs[(f, cur)].append(code)
            if table and "};" in code:
                table = None
        self.line_func[f] = line_func
        self.line_class[f] = line_class

    def _infer_custom_members(self):
        """a row with its own parser and no member (`{ "FX", parseFX, nullptr, 0 }`): the members that parser's body names (m_*), in the row's
        file; several are joined with |"""
        by_short: dict[tuple[str, str], list[str]] = defaultdict(list)
        for (f, fn), body in self.funcs.items():
            by_short[(f, fn.split("::")[-1])] += body
        for r in self.rows:
            if r.member.startswith("m_") or re.fullmatch(r"[A-Za-z_]\w+", r.member):
                continue
            short = re.sub(r"<[^<>]*>$", "", r.parser.split("::")[-1])  # parseAnimAndDuration<0>: the template's body
            body = by_short.get((r.file, short)) or self._nested_body(r.file, short)
            if body:
                text = "\n".join(body)
                # the m_* members it names, and the plain members it stores into (info.model = ..., d->names.push_back(...), also through an
                # index: LivingWorldAutoResolveData.cpp's parseArmorRow `a->scalar[scanUnitType(..)] = value`)
                members = set(re.findall(r"\b(m_\w+)", text))
                members |= set(re.findall(r"(?:->|\.)\s*([a-z]\w{2,})\s*(?:\[(?:[^\[\]]|\[[^\[\]]*\])*\]\s*)?"
                                          r"(?:=(?!=)|\.\s*(?:push_back|emplace_back|assign|insert|append)\b)", text))
                # a member of the instance handed to a helper (W3DModelDraw.cpp's parseParticleSysBoneInto(ini, ..(instance)->particleSysBones))
                # (the last name of a chain that is no call: (instance)->gauge.cool is cool, (instance)->list.push_back(..) is list)
                members |= {re.split(r"\s*(?:\.|->)\s*", c)[-1] for c in
                            re.findall(r"\(\s*(?:instance|store)\s*\)\s*->\s*([a-z]\w+(?:\s*(?:\.|->)\s*[a-z]\w+)*)\b(?!\s*\()", text)}
                # the address of a member handed to a parser (`Parser(ini, instance, &self->shadow.sizeX, nullptr)`,
                # `INI::parseAngleReal(ini, instance, &lastTurret(..).artAngle, nullptr)`): the last name of the argument
                members |= self._address_members(text)
                if members:
                    r.member = "|".join(sorted(members))

    def _nested_body(self, f: str, name: str) -> list[str]:
        """the body of a function defined inside another one (EvaEvents.cpp's parseRecord: `struct Parsers { static void priority(..) { .. } }`,
        which the scan records as its outer function's lines): from its signature to its closing brace"""
        code = [x.split("//", 1)[0] for x in self.files[f]]
        sig = re.compile(r"\b(?:static\s+)?(?:void|bool|int)\s+" + re.escape(name) + r"\s*\(")
        for i, line in enumerate(code):
            m = sig.search(line)
            if not m:
                continue
            out, depth, opened = [], 0, False
            for x in code[i:i + 80]:
                out.append(x)
                depth += x.count("{") - x.count("}")
                opened = opened or "{" in x
                if opened and depth <= 0:
                    return out
                if not opened and x.rstrip().endswith(";"):
                    break
        return []

    @staticmethod
    def _address_members(text: str) -> set[str]:
        """the members whose address a call argument takes: `&<expression>.member` or `->member` up to the argument's end (a `,` or `)` outside
        the expression's own parentheses)"""
        out = set()
        for m in re.finditer(r"(?<![&\w)\]])&(?!&)", text):
            depth, j = 0, m.end()
            while j < len(text):
                c = text[j]
                if c in "([":
                    depth += 1
                elif c in ")]":
                    if depth == 0:
                        break
                    depth -= 1
                elif c in ",;" and depth == 0:
                    break
                j += 1
            a = re.search(r"(?:->|\.)\s*([a-z]\w{2,})\s*$", text[m.end():j])
            if a:
                out.add(a.group(1))
        return out

    def _generated_rows(self, f: str, lines: list[str], n: int, code: str, arrays: dict[str, list[str]], macros: dict[str, str],
                        res) -> list[tuple[str, str, str, str]]:
        """(field, parser, member, struct) of a row a table builder pushes with a name it makes (TransitionDamageFX.cpp's buildTable): an element
        of a name array (`rows.push_back({ kSub[k], parseSubObjects, ..})`) or the name pushed just before (`names.push_back(std::string(kStates[s])
        + kKinds[kind] + std::to_string(i + 1)); rows.push_back({ names.back().c_str(), parseEntry, ..})`, also through a helper:
        AudioSettings.cpp's `add(reverbNames.back().c_str(), INI::parsePercentToReal, AS(reverbMultiplier) + ..)`): every name the parts make, a
        number running from 1 to the bound of the loop over its variable, a local's literals (`prefix = g == 0 ? "" : "LivingWorld"`) or the
        names of the rows of an array the loop walks (`prefix + r.name` over `group[] = { { "MicrophoneMaxDistanceToCamera", .. } }`: such a
        row's parser and member are the named row's)"""
        m = re.search(r"push_back\(\s*\{\s*(?:(\w+)\s*\[\s*\w+\s*\]|(\w+)\.back\(\)\.c_str\(\))\s*,\s*&?\s*([\w:<>.]+)\s*,(.*)\}", code)
        m = m or re.search(r"\b\w+\(\s*()(\w+)\.back\(\)\.c_str\(\)\s*,\s*&?\s*([\w:<>.]+)\s*,(.*)\)\s*;", code)
        if not m:
            return []
        rest = m.group(4)
        member, struct = "", ""
        o = re.search(r"\b(\w+)\s*\(\s*(\w+)\s*\)", rest)
        if o and o.group(1) in macros:
            member, struct = o.group(2), macros[o.group(1)]
        if m.group(1):
            return [(x, m.group(3), member, struct) for x in arrays.get(m.group(1), [])]
        expr = next((e.group(1) for k in range(n - 2, max(-1, n - 5), -1)
                     for e in [re.search(r"\b" + re.escape(m.group(2)) + r"\.push_back\((.*)\);", lines[k])] if e), "")
        parts, depth, cur = [], 0, ""
        for c in expr:
            depth += (c == "(") - (c == ")")
            if c == "+" and depth == 0:
                parts.append(cur)
                cur = ""
            else:
                cur += c
        parts.append(cur)
        names, based = [""], False
        for p in (x.strip() for x in parts if x.strip()):
            a = re.search(r"\b(\w+)\s*\[", p)
            loop = re.fullmatch(r"(\w+)\s*\.\s*name", p)
            walk = loop and next((w.group(1) for k in range(n - 2, max(-1, n - 5), -1)
                                  for w in [re.search(r"for\s*\(\s*const\s+\w+\s*&\s*" + loop.group(1) + r"\s*:\s*(\w+)\s*\)", lines[k])] if w), "")
            local = re.fullmatch(r"\w+", p) and next((d.group(1) for k in range(n - 2, max(-1, n - 30), -1)
                                                      for d in [re.search(r"\b" + p + r"\s*=\s*([^;]+);", lines[k])] if d), "")
            if re.fullmatch(r'(?:std::string\s*\()?\s*"[^"]*"\s*\)?', p):
                vals = STRING.findall(p)
            elif a and a.group(1) in arrays:
                vals = arrays[a.group(1)]
            elif walk and walk in arrays:
                vals, based = arrays[walk], True
            elif local:
                vals = STRING.findall(local) + ([""] if '""' in local else [])
            elif "to_string" in p:
                var = re.search(r"to_string\(\s*(\w+)\s*\+\s*1\s*\)", p)
                bound = var and next((b.group(1) for k in range(n - 2, max(-1, n - 8), -1)
                                      for b in [re.search(r"\b" + re.escape(var.group(1)) + r"\s*<\s*([\w:]+)", lines[k])] if b), "")
                count = self._constant(bound) if bound else 0
                if not count:
                    return []  # a number of rows the tool cannot tell: no rows (the field stays no-table-row, a lead)
                vals = [str(i) for i in range(1, count + 1)]
            else:
                return []
            names = [x + v for x in names for v in vals]
        if based:
            # the walked array's own row of that name gives the parser and the member (its row in this file)
            own = {r.field: r for r in self.rows if r.file == f}
            out = []
            for x in names:
                base = next((own[b] for b in sorted(own, key=len, reverse=True) if x.endswith(b)), None)
                if base and x != base.field:
                    out.append((x, base.parser, base.member, base.struct))
            return out
        return [(x, m.group(3), member, struct) for x in names]

    def _constant(self, name: str) -> int:
        """the value of an integer constant (`static constexpr int MAX_FX = 12;`, Data::MAX_FX) or a literal; 0 when it is not found"""
        if name.isdigit():
            return int(name)
        short = name.split("::")[-1]
        for lines in self.files.values():
            for line in lines:
                c = re.search(r"\b" + re.escape(short) + r"\s*=\s*(\d+)\s*[;,]", line)
                if c and re.search(r"\b(?:constexpr|const|enum)\b|^\s*" + re.escape(short), line):
                    return int(c.group(1))
        return 0

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
            # the last name of the offset argument; an element of an array member (ES_OFF(goodMP[0])) is the array's, and so is a byte step into
            # it (SkirmishAIData.cpp's AD_OFF(percentToSave) + 4)
            ids = re.findall(r"\b(m_\w+)", rest) or re.findall(r"(\w+)\s*(?:\[[^\]]*\]\s*)?\)*\s*$", re.sub(r"\s*\+\s*\d+\s*$", "", rest))
            if re.search(r"\w\s*\.\s*\w", rest.split(",", 1)[-1]):
                struct = "?"  # offsetof(BlockState, values.x): the member belongs to a member's type, read through another struct: search everywhere
            out.append((r.group(1), r.group(2), ids[-1] if ids else "", struct))
        if out:
            return out
        for r in ROW_CALL.finditer(code):
            macro, first, field, parser, rest = r.groups()
            if "parse" not in parser.lower() and macro not in macros:
                continue
            o = OFFSETOF.search(rest)
            struct = res(o.group(1)) if o else macros.get(macro, "")
            if not o and not struct and first and macro in macros:
                struct = res(first)  # the struct is the macro's first argument (FormationAssistant.cpp's FIELD(F::Decal, "Texture", ..))
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
    def visible(self, struct: str, origin: str = "") -> set[str]:
        """the files that can see the struct: the file declaring it and every file including that one (transitively). A name several files
        declare (FormationAssistant.h's nested Unit and others) is the declaration origin (the row's file) sees, when it sees one"""
        near = [h for h in self.decl_all.get(struct, ()) if origin and h in self.forward(origin)]
        h = near[0] if near else self.decl.get(struct)
        return self.visible_file(h) if h else set()

    def visible_file(self, h: str) -> set[str]:
        """the file and every file including it (transitively)"""
        if h in self._visible:
            return self._visible[h]
        out: set[str] = set()
        todo = [h]
        while todo:
            x = todo.pop()
            if x in out:
                continue
            out.add(x)
            todo += self.included_by.get(x, ())
        self._visible[h] = out
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
            # a table a free function of the file builds (TransitionDamageFX.cpp's `table = buildTable(); p.add(table.data());`)
            for c in set(re.findall(r"(?<!->)(?<![.:\w])\s*\b(\w+)\s*\(", body)):
                out += [r for r in self.rows if r.file == f and r.func == c]
            for c in re.finditer(r"(?:(\w+)::)?(buildFieldParse|getFieldParse|build\w*FieldParse|\w*[Ff]ields?)\s*\(", body):
                cls, fname = c.group(1), c.group(2)
                if cls:
                    todo += [(g, h) for (g, h) in self.funcs if h == f"{cls}::{fname}"]
                else:
                    # an unqualified call is the file's function or a base's method of another file (UpgradeModules.cpp's
                    # buildBaseFieldParse(p) is UpgradeModuleData's, in UpgradeModule.cpp)
                    todo += [(g, h) for (g, h) in self.funcs if h.split("::")[-1] == fname and (g == f or h.split("::")[0] in structs)]
        uniq, seen = [], set()
        for r in out:
            if (r.file, r.line, r.field) not in seen:
                seen.add((r.file, r.line, r.field))
                uniq.append(r)
        return uniq

    def parser_rows(self, parser: str, file: str, depth: int = 2) -> list[Row]:
        """the rows a row's own parser reads its sub-block with: the tables and the X::fieldParse()/getFieldParse() functions its body names (the
        parser's file first), and the parsers it calls in turn (depth levels)"""
        # a template parser's class argument (FXList.cpp's parseNugget<TerrainScorchFXNugget>): its body's unqualified n->fieldParse() is that
        # class's, not every nugget's
        targs = re.findall(r"<\s*([\w:]+)\s*>", parser)
        short = re.sub(r"<[^<>]*>$", "", parser.split("::")[-1])
        keys = [(f, fn) for (f, fn) in self.funcs if fn.split("::")[-1] == short and f == file]
        keys = keys or [(f, fn) for (f, fn) in self.funcs if fn.split("::")[-1] == short]
        bodies = [self.funcs[k] for k in keys]
        # a table of the parser's own (FormationAssistant.cpp's parseDecal: `const FieldParse fields[] = {FIELD(F::Decal, "Texture", ..)}`)
        out: list[Row] = [r for r in self.rows if (r.file, r.func) in set(keys)]
        for body in bodies:
            text = "\n".join(body)
            for m in re.finditer(r"(->|\.)?\s*\b(?:(\w+)::)?(\w*[Ff]ield[Pp]arse\w*|\w*[Ff]ields)\s*\(", text):
                cls = m.group(2) or (targs[0].split("::")[-1] if targs and m.group(1) else "")
                fn = f"{cls}::{m.group(3)}" if cls else m.group(3)
                out += self.table_rows(fn)
            if targs:
                # the tables of the file's free functions the body calls (parseNugget's baseTable(), the rows every nugget shares; not its
                # n->fieldParse(), the class argument's above)
                calls = set(re.findall(r"(?<!->)(?<![.:\w])\s*\b(\w+)\s*\(", text))
                out += [r for r in self.rows if r.file in {k[0] for k in keys} and r.func.split("::")[-1] in calls and r not in out]
            for m in re.finditer(r"\b(k\w+)\b", text):
                out += [r for r in self.rows if r.table == m.group(1)]
            # a parse method it calls on an object of a class its body names (FunctionCurve::parseFunctionCurve's curve->parse(ini), the table
            # local to FunctionCurve::parse)
            for name in set(re.findall(r"(?:->|\.)\s*([Pp]arse\w*)\s*\(", text)):
                out += [r for r in self.rows if "::" in r.func and r.func.split("::")[-1] == name and r.func.split("::")[0] != "INI"
                        and re.search(r"\b" + re.escape(r.func.split("::")[0]) + r"\b", text)]
            if depth > 1:
                for m in set(re.findall(r"\b(parse\w+|Parse\w+)\s*\(", text)):
                    if m != short:
                        out += self.parser_rows(m, file, depth - 1)
        return out

    def table_rows(self, fn: str) -> list[Row]:
        """the rows of a table function ([Cls::]fieldParse); one without rows of its own hands out another function's table (FXList.cpp's
        FXList::fieldParse() returns FXListParseAccess::table()) or a table of its file by name (EmotionSystem.cpp's EmotionNuggetTemplate::
        fieldParse() returns kNuggetParse): the rows of the functions of its file its body calls and of the tables it names"""
        out = [r for r in self.rows if r.func == fn or r.func.endswith("::" + fn)]
        if not out:
            for (g, h), body in self.funcs.items():
                if h == fn or h.endswith("::" + fn):
                    text = "\n".join(body)
                    calls = set(re.findall(r"\b(\w+)\s*\(", text)) - {fn.split("::")[-1]}
                    tables = set(re.findall(r"\b(k\w+)\b", text))
                    out += [r for r in self.rows if r.file == g and (r.func.split("::")[-1] in calls or r.table in tables)]
        return out

    def words_of(self, f: str) -> dict[str, list[int]]:
        w = self._words.get(f)
        if w is None:
            w = defaultdict(list)
            for n, code in enumerate(self.code_of(f), 1):
                for x in set(re.findall(r"[A-Za-z_]\w*", code)):
                    w[x].append(n)
            self._words[f] = w
        return w

    def code_of(self, f: str) -> list[str]:
        """the file's lines without comments and literal text (code_only)"""
        if not hasattr(self, "_code"):
            self._code: dict[str, list[str]] = {}
        if f not in self._code:
            self._code[f] = code_only(self.files[f])
        return self._code[f]

    def local_named(self, f: str, n: int, name: str) -> bool:
        """whether the function around line n declares a local or a parameter of that name before it (the signature's lines included)"""
        code, k = self.code_of(f), self.func_start(f, n)
        decl = re.compile(r"(?:[\w>\*&]\s+|[\*&])" + re.escape(name) + r"\s*(?:[=;,)\[{(:]|$)")
        members = set(self.declarations(f, name))
        for i in range(max(0, k - 3), n):
            if i in members:
                continue  # the class's member above a one-line method (LivingWorldAutoResolveData.h's scalar, then damageScalar() { return scalar[..]; })
            for m in decl.finditer(code[i]):
                head = code[i][:m.start() + 1].rstrip()
                if not re.search(r"\b(?:return|case|else|throw|delete|sizeof|do)$", head):
                    return True
        return False

    def func_start(self, f: str, n: int) -> int:
        """the index (0-based) of the first line of the function around line n"""
        funcs, k = self.line_func[f], n - 1
        while k > 0 and funcs[k - 1] == funcs[n - 1]:
            k -= 1
        return k

    def staged_read(self, f: str, n: int, member: str, names: set[str] = frozenset()) -> tuple[str, str]:
        """a block's parser that parses a local of its own and then consumes it (SkirmishAIData.cpp's parseArmyDefinition: `ArmyDefinition army;
        ini->initFromINI(&army, ..); nameToKey(army.side)`, Locomotor.cpp's parseLocomotorSet: entry.speed into the set): a read of `<local>.member`
        after that initFromINI in the same function. Returns (how, destination): how is "key" when the value goes into a lookup (a
        nameToKey/find*/scanIndexList call or an operator[]), "copy" when it is stored elsewhere or passed on (destination: the member it is stored
        into, "" when it is not a member), "" when it is no such read. With names, the local's declared type must be one of them
        (parseArmyDefinition's army is no AIBase)"""
        code = self.code_of(f)
        start = self.func_start(f, n)
        local = set()
        for i in range(start, n - 1):
            local |= set(re.findall(r"initFromINI\w*\s*\(\s*&\s*(\w+)\s*,", code[i]))
        if names:
            body = "\n".join(code[start:n - 1])
            local = {v for v in local if any(t.split("::")[-1] in names
                                             for t in re.findall(r"([\w:]+)\s+" + re.escape(v) + r"\s*(?:;|\{|=|\()", body))}
        if not local:
            return "", ""
        line = code[n - 1]
        for m in re.finditer(r"\b(\w+)\s*\.\s*" + re.escape(member) + r"\b", line):
            if m.group(1) not in local or re.search(r"&\s*$", line[:m.start()]):
                continue  # &entry.x hands the member to a writer
            after = line[m.end():]
            if WRITE_AFTER.match(after) or re.match(r"\s*\.\s*(?:push_back|emplace_back|insert|clear|assign|append|resize|erase)\b", after):
                continue
            if re.match(r"\s*\.\s*(?:empty|size|length)\s*\(", after):
                continue  # only whether it was given (PlayerTemplate.cpp's parseBlock: a stop line's flag), not the value
            if re.match(r"\s*if\s*\(", line) and re.search(r"\)\s*throw\b", line):
                continue  # a check that only refuses the block (AwardSystemManager.cpp's `if (missing(a.image)) throw ..`)
            # the innermost open call or subscript around the read
            depth, j = 0, m.start() - 1
            while j >= 0:
                c = line[j]
                if c in ")]":
                    depth += 1
                elif c in "([":
                    if depth == 0:
                        break
                    depth -= 1
                j -= 1
            if j >= 0 and line[j] == "[":
                return "key", ""
            call = re.search(r"(\w+)\s*$", line[:j]) if j >= 0 else None
            if call and re.fullmatch(r"nameToKey|findKey|find\w*|scanIndexList\w*|lookup\w*", call.group(1)):
                return "key", ""
            # stored into a member (`def.livingWorldRgb = packRgb(block.livingWorld)`, `m_dozers[key] = rec.unit`): that member's read is the use;
            # a local or a call's argument (set.set(condition, loco, entry.speed)) is not followed
            dest = re.search(r"(\w+)\s*(?:\[[^=]*\])?\s*=(?!=)[^=;]*$", line[:m.start()])
            dest = dest.group(1) if dest and not self.local_named(f, n, dest.group(1)) else ""
            return "copy", dest
        return "", ""

    def is_read(self, f: str, n: int, member: str, struct: str, parser: tuple[str, str] = ("", "")) -> bool:
        if (f, n) in self.row_lines:
            return False
        code = self.code_of(f)[n - 1]
        if re.search(r"offsetof|_OFF\s*\(|_ROW\s*\(", code):
            return False
        fn = self.line_func[f][n - 1]
        short = fn.split("::")[-1].lower()
        cls = fn.split("::")[0] if "::" in fn else ""
        if "parse" in short or short in ("xfer", "crc", "xferimpl") or (cls and short == cls.lower()) or short.startswith("~"):
            return False
        if fn and self.hashes(f, n):
            return False  # a hash or a save of the state (Object.cpp's crcOmittingModules(StateHasher &h, ..), SnapshotXfer.cpp's damageInput(Xfer &x, ..)), no use
        if fn and (f, fn) == parser:
            return False  # the function holding the row's table parses the block, whatever its name (AwardSystemManager.cpp's objectAward)
        if not fn and n - 1 in self.declarations(f, member):
            return False  # a class-level declaration, a declarator list's later name too (`std::string a, b;`)
        for m in re.finditer(r"\b" + re.escape(member) + r"\b", code):
            before, after = code[:m.start()], code[m.end():]
            if not member.startswith("m_") and not re.search(r"(?:\.|->)\s*$", before):
                # a bare `side` is a local or a parameter of that name in a function declaring one (LiveGame.cpp's local, findDozer(const std::string
                # &side)); otherwise this member only inside a method (Cls::fn) or in a file declaring such a member
                if (fn and self.local_named(f, n, member)) or ("::" not in fn and not self.declares(f, member)):
                    continue
                if re.match(r"\s*[,)=]", after) and re.search(r"[\w>]\s*[&\*]?\s*$", before) and not re.search(r"\b(?:return|case|throw)\s*$", before):
                    continue  # a parameter in a signature (`const std::string &side)`, its line not yet the function's)
            if WRITE_AFTER.match(after):
                continue  # a store (m_x = ..., m_x += ...)
            prev = re.search(r"(\w+)\W*$", before)
            if (re.match(r"\s*[;{\[]", after) and re.search(r"[\w>\*&]\s+[\*&]?\s*$", before) and not before.rstrip().endswith(("->", ".", "(", "::"))
                    and not (prev and prev.group(1) in ("return", "case", "else", "throw", "co_return", "delete", "sizeof", "do"))
                    and re.fullmatch(r"\s*(?:[\w:<>,]+\s*)+[\*&]?\s*", before)):
                continue  # a declaration (Type m_x; Type *m_x; Type m_x{...}; Type m_x[N]) from the line's start: not `return m_x;`, not `a * m_x;`
            if re.match(r"\s*[({]", after) and re.search(r"[:,]\s*$", before) and not fn:
                continue  # a constructor's initialiser list outside a recorded body
            if re.match(r"\s*[,)]", after) and re.search(r"\b(?:struct|class)\b|^\s*[\w:<>]+\s+$", before):
                continue
            return True
        return False

    def hashes(self, f: str, n: int) -> bool:
        """whether the signature of the function around line n takes a StateHasher or an Xfer: its body's first line up to the brace and the lines
        before it that continue the signature (not a declaration ended by `;` above a one-line getter: AutoPickUpUpdate.h's xfer(Xfer &x) override;)"""
        code, k = self.code_of(f), self.func_start(f, n)
        sig = [code[k].split("{", 1)[0]]
        i = k - 1
        while i >= max(0, k - 3) and code[i].strip() and not code[i].rstrip().endswith((";", "}", "{")):
            sig.insert(0, code[i])
            i -= 1
        return bool(re.search(r"\b(?:StateHasher|Xfer)\s*[&\*]", "\n".join(sig)))

    def forward(self, f: str) -> set[str]:
        """the file and every file it includes, transitively"""
        if not hasattr(self, "_forward"):
            self._forward: dict[str, set[str]] = {}
            self._includes_of: dict[str, set[str]] = defaultdict(set)
            for h, users in self.included_by.items():
                for u in users:
                    self._includes_of[u].add(h)
        if f not in self._forward:
            out, todo = set(), [f]
            while todo:
                x = todo.pop()
                if x not in out:
                    out.add(x)
                    todo += self._includes_of.get(x, ())
            self._forward[f] = out
        return self._forward[f]

    def declares(self, f: str, member: str) -> bool:
        """a class-level declaration of the member in the file (a local variable of a function body does not count)"""
        return bool(self.declarations(f, member))

    def declarations(self, f: str, member: str) -> list[int]:
        """the lines (0-based) of the file's class-level declarations of the member"""
        if not hasattr(self, "_decls"):
            self._decls: dict[tuple[str, str], list[int]] = {}
        if (f, member) not in self._decls:
            self._decls[(f, member)] = self._declarations(f, member)
        return self._decls[(f, member)]

    def _declarations(self, f: str, member: str) -> list[int]:
        funcs = self.line_func.get(f, [])
        pat = re.compile(r"^\s*[\w:<>,\s\*&]+[\s\*&]" + re.escape(member) + r"\s*(?:=[^=]|;|\[|\{)")
        # a later name of a declarator list (CastleModules.h's `float m_decalSize = 1.0f, .., m_maxCastleRadius = ..;`), not a parameter list
        listed = re.compile(r"^\s*[\w:<>]+[\s\*&]+\w+[^(]*,\s*[\*&]?" + re.escape(member) + r"\s*(?:=[^=]|;|,|\[|\{)")
        return [n for n, line in enumerate(self.files[f]) if (pat.match(line) or listed.match(line)) and not (n < len(funcs) and funcs[n])]

    def member_owners(self, member: str) -> set[str]:
        """the classes declaring a member of that name (class-level declarations, anywhere in the source)"""
        if not hasattr(self, "_owners"):
            self._owners: dict[str, set[str]] = {}
        if member not in self._owners:
            self._owners[member] = {self.line_class[f][n] for f in self.files if member in self.words_of(f)
                                    for n in self.declarations(f, member) if self.line_class[f][n]}
        return self._owners[member]

    def ambiguous(self, f: str, member: str, names: set[str]) -> str:
        """" AMBIGUOUS: .." when the reading file names another class declaring a member of that name as well as ours: the read may be that
        class's (the known limit), check it by hand"""
        others = sorted(o for o in self.member_owners(member) - names if o in self.words_of(f))
        return f" AMBIGUOUS: the file also names {', '.join(others[:3])} with a member {member}" if others else ""

    def declared_once(self, member: str) -> bool:
        """whether one class-level declaration in the whole source has that name: then a read of it is that member, whatever the reading
        file names (AimWeaponBehavior.cpp reads WeaponTemplate's m_isAimingWeapon through a getter's result)"""
        if not hasattr(self, "_once"):
            self._once: dict[str, bool] = {}
        if member not in self._once:
            self._once[member] = sum(len(self.declarations(f, member)) for f in self.files if member in self.words_of(f)) == 1
        return self._once[member]

    def declaring_by_depth(self, origin: str, member: str) -> list[list[str]]:
        """the files declaring the member, grouped by include depth from origin (origin itself first)"""
        if not hasattr(self, "_includes_of"):
            self.forward(origin)
        out, level, seen = [], [origin], set()
        while level:
            hits = [f for f in level if self.declares(f, member)]
            if hits:
                out.append(hits)
            seen |= set(level)
            level = sorted({h for f in level for h in self._includes_of.get(f, ()) if h not in seen})
        return out

    def member_read(self, member: str, struct: str, origin: str = "", staged: bool = True, parser: tuple[str, str] = ("", ""), depth: int = 2,
                    skip: frozenset = frozenset()) -> str:
        """the first read of the member in a file that sees its struct. Without a struct ("?": a member of a member, offsetof(S, a.b), or a member a
        row's own parser stores into): the files that see a file declaring a member of that name among the files origin (the row's file) includes,
        the nearest declarations first (EvaEvents.cpp's SideCtx::side, then EvaEvents.h's EvaSideSound::side); everywhere if there is none"""
        files = self.visible(struct, origin) if struct and struct != "?" else set()
        if files:
            # a member of a base is read where the base is seen (FlingPassengerSpecialAbilityUpdateModuleData's m_customAnimCondition is
            # SpecialAbilityUpdateModuleData's, read in SpecialAbilityModules.cpp, which never sees the derived struct)
            files = set(files)  # (visible() is cached)
            for base in (self.member_owners(member) & self.ancestors(struct)) - {struct}:
                files |= self.visible(base, origin)
        # a file naming the struct (or a base) in its code first: `.side` in a file that never names ArmyDefinition is likely another struct's
        # member of that name (LivingWorldUnits.cpp's ObjectFilter f.side); such a read still counts, marked INFERENCE. Without a struct, the
        # struct is the class of the nearest declarations (W3DModelDraw.h's ModelShadowInfo::maxHeight, not DrawableIconUI.h's box.maxHeight)
        scopes = [(sorted(files), self.ancestors(struct))] if files else []
        if not files and origin:
            for hits in self.declaring_by_depth(origin, member):
                owners = {self.line_class[h][n] for h in hits for n in self.declarations(h, member) if self.line_class[h][n]}
                scopes.append((sorted({x for h in hits for x in (self.visible_file(h) if h.endswith((".h", ".hpp")) else {h})}),
                               {a for o in owners for a in self.ancestors(o)}))
        for scope, names in scopes or [(sorted(self.files), set())]:
            named = [f for f in scope if not names or self.names_any(f, names)]
            for f in named:
                for n in self.words_of(f).get(member, ()):
                    if self.is_read(f, n, member, struct, parser) and self.copy_used(f, n, member, depth, skip):
                        return f"{f}:{n}" + (self.ambiguous(f, member, names) if names else "")
            # then a block's parser consuming the local it parsed (staged_read; the parsers are no reads otherwise): a key is a use; a value stored
            # into a member is used when that member is read (one step); a value passed to a call is marked, where it goes is not followed
            # (a key anywhere before a copy: `chain.unit < 0` is a check, `combatChains[chain.unit]` the use)
            found = [(f, n, *self.staged_read(f, n, member, names)) for f in (named if staged else ()) for n in self.words_of(f).get(member, ())
                     if (f, n) not in self.row_lines]
            for f, n, how, dest in sorted(found, key=lambda x: x[2] != "key"):
                fn = self.line_func[f][n - 1]
                if how == "key":
                    return f"{f}:{n} STAGED: the block's parser {fn} uses it as a key"
                if how == "copy" and dest:
                    use = self.member_read(dest, "?", origin=f, staged=False)
                    if use:
                        return f"{f}:{n} STAGED: the block's parser {fn} stores it in {dest}, read at {use}"
            for f, n, how, dest in found:
                if how == "copy" and not dest:
                    return (f"{f}:{n} STAGED: the block's parser {self.line_func[f][n - 1]} passes it on; INFERENCE: the read of where it goes is "
                            "not followed")
            for f in sorted(set(scope) - set(named)):
                for n in self.words_of(f).get(member, ()):
                    if self.is_read(f, n, member, struct, parser) and self.copy_used(f, n, member, depth, skip):
                        # one declaration of that name in the source: the read is this member, whatever the file names
                        return f"{f}:{n}" if self.declared_once(member) else f"{f}:{n} INFERENCE: the file does not name {self._named(struct, names)}"
        return ""

    @staticmethod
    def _named(struct: str, names: set[str]) -> str:
        return struct if struct and struct != "?" else ", ".join(sorted(names)[:3])

    def copy_dest(self, f: str, n: int, member: str) -> str:
        """the member a read only copies the value into (VisionSettings.cpp's `out.stealthFriendlyOpacity = s.stealth;`, also through a call such as
        `def.rgb = packRgb(block.rgb)`), or "": a local, a call's argument or any other use is no copy"""
        code = self.code_of(f)[n - 1]
        for m in re.finditer(r"\b" + re.escape(member) + r"\b", code):
            d = re.search(r"(\w+)\s*(?:\[[^=]*\])?\s*(?<![<>!=+\-*/|&^])=(?!=)[^=;?]*$", code[:m.start()])
            if not d or re.search(r"\b(?:return|if|while|for)\b", code[:d.start()]):
                return ""
            dest = d.group(1)
            return "" if self.local_named(f, n, dest) else dest
        return ""

    def copy_used(self, f: str, n: int, member: str, depth: int, skip: frozenset) -> bool:
        """whether a read counts: a copy into a member counts only when that member is read in turn (depth steps; at the last step a copy
        counts, as before)"""
        if (f, n) in skip:
            return False
        dest = self.copy_dest(f, n, member)
        if not dest or depth <= 0:
            return True
        key = (dest, f, depth - 1, skip | {(f, n)})
        if not hasattr(self, "_copies"):
            self._copies: dict = {}
        if key not in self._copies:
            self._copies[key] = bool(self.member_read(dest, "?", origin=f, staged=False, depth=depth - 1, skip=skip | {(f, n)}))
        return self._copies[key]

    def names_any(self, f: str, names: set[str]) -> bool:
        """whether the file's code (not its comments or literals) names one of the structs"""
        words = self.words_of(f)
        return any(nm in words for nm in names)

    def string_read(self, field: str, near: str = "", lists: bool = False) -> str:
        """the first source line naming "field" as a string outside the tables and the .inc field lists; with near, only in a file that also names
        near (a raw class's reader names its class). A line of a name list (`"VoiceSelect", "VoiceMove", ..` in UnitVoiceResponse.cpp's kRowNames)
        is not a request for the name: the list is read by index, which rows are asked is not followed; with lists, only such lines"""
        for f, n in self.string_lines(field):
            if (not near or re.search(r"\b" + re.escape(near) + r"\b", self._text[f])) and self.name_list_line(f, n) == lists:
                return f"{f}:{n}"
        return ""

    def name_list_line(self, f: str, n: int) -> bool:
        """a line holding only string literals (two or more), commas and braces: a row of a name list"""
        line = self.files[f][n - 1].split("//", 1)[0]
        return len(STRING.findall(line)) >= 2 and not re.sub(r"\"[^\"]*\"|[\s,{}]|const\s+char\s*\*\s*const\s+\w+\[[^\]]*\]\s*=", "", line)

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

    def branch_member(self, f: str, n: int) -> tuple[str, int]:
        """(member, line) of the first member the branch a key test opens names (TerrainRoads.cpp's `if (key == "roadwidth") { cur.roadWidth = ..; }`,
        CursorFile.cpp's `ls >> cur->image;`, ProductionSettings.cpp's parseMultiPlay(value, out.multiPlayUnitSpeedMult, n)), up to the next else;
        a call (val.c_str()) is no member. ("", 0) when the line is no key test or the branch names no member"""
        code = self.code_of(f)
        if not re.search(r"==\s*\"\w+\"|compareNoCase\s*\([^;]*\"\w+\"\s*\)", self.files[f][n - 1]):
            return "", 0
        for k in range(n, min(len(code), n + 8)):
            x = code[k]
            if re.match(r"\s*(?:\}\s*)?else\b", x):
                break
            m = re.search(r"\b[A-Za-z_]\w*\s*(?:\.|->)\s*((?:[A-Za-z_]\w*\s*(?:\.|->)\s*)*[A-Za-z_]\w*)\b(?!\s*\()", x)
            if m:
                return re.split(r"\s*(?:\.|->)\s*", m.group(1))[-1], k + 1
        return "", 0

    def files_naming(self, name: str) -> set[str]:
        """the files whose code names the word (UnitSpecificSound: the readers of the UnitSpecificSounds lines)"""
        return {f for f in self.files if name in self.words_of(f)}

    def name_prefixes(self, files: set[str]) -> list[tuple[str, int, str]]:
        """(file, line, literal) of the literals a name is made of by appending to them (`"VoiceEnterUnit" + ..`,
        `std::string(enterState ? "VoiceEnterStateAttackUnit" : "VoiceAttackUnit") + ..`), the longest first"""
        out = []
        for f in sorted(files):
            for n, line in enumerate(self.files[f], 1):
                m = re.search(r"^(.*\"\s*\)?\s*)\+", line.split("//", 1)[0])
                if m:
                    out += [(f, n, s) for s in STRING.findall(m.group(1))]
        return sorted(out, key=lambda x: -len(x[2]))

    def own_reader_files(self, keyword: str) -> set[str]:
        """the files (other than the INI keyword table) naming a block keyword as a string or its data\\ini file (terrain.ini)"""
        if not hasattr(self, "_text"):
            self.string_lines("")
        k = keyword.lower()
        return {f for f, text in self._text.items() if f != "Common/INI/INIBlockStubs.cpp" and not f.endswith(".inc")
                and (f'"{k}"' in text.lower() or f"{k}.ini" in text.lower())}

    def block_files(self, keyword: str) -> set[str]:
        """the files registering a parser for the block keyword (registerBlock("Keyword", ...) or INIBlockStubs.cpp's keyword == "Keyword")"""
        if not hasattr(self, "_block_files"):
            self._block_files: dict[str, set[str]] = defaultdict(set)
            self._block_tables: dict[str, set[tuple[str, str]]] = defaultdict(set)
            tables = {(r.file, r.table) for r in self.rows if r.table}
            for f, lines in self.files.items():
                for line in lines:
                    for k in re.findall(r"registerBlock\(\s*\"(\w+)\"", line):
                        self._block_files[k.lower()].add(f)
                    # a registration helper given the keyword and its table: reg("AIData", kAIDataFields, 2u) (GameLogicSettings.cpp), a loader
                    # given them after other arguments (VisionSettings.cpp's Load m(nullptr, "MultiplayerSettings", kMultiplayer, ..))
                    for k, t in re.findall(r"\b\w+\s*\((?:[^()\"]*,)?\s*\"([A-Z]\w+)\"\s*,\s*(k\w+)\s*[,)]", line):
                        if (f, t) in tables:
                            self._block_files[k.lower()].add(f)
                            self._block_tables[k.lower()].add((f, t))
        return self._block_files.get(keyword.lower(), set())

    def catch_all(self, keyword: str) -> tuple[str, str, str, int] | None:
        """(file, table, parser, line) of the catch-all row of the table a registration helper gives the block (GameLogicSettings.cpp's GameData,
        AIData and MultiplayerSettings: `{ nullptr, noteUnapplied, nullptr, 0 }` takes every name no row has, nested blocks included, records it
        unapplied and applies nothing, stop S-152); None without one"""
        self.block_files(keyword)
        return next(((f, t, *self.catch_alls[(f, t)]) for f, t in sorted(self._block_tables.get(keyword.lower(), ())) if (f, t) in self.catch_alls),
                    None)

    def block_parser_rows(self, keyword: str) -> list[Row]:
        """the rows of the parser a registerBlock("Keyword", lambda) calls: the tables the lambda names and the parse functions it calls
        (parser_rows). Several blocks of one file (SkirmishAIData.cpp's ArmyDefinition, AIBase, AIDozerAssignment) each have a Side row: the
        block's own is the one its parser reads"""
        out: list[Row] = []
        for f in self.block_files(keyword):
            lines = self.code_of(f)
            for n, line in enumerate(self.files[f]):
                if not re.search(r"registerBlock\(\s*\"" + re.escape(keyword) + r"\"", line, re.I):
                    continue
                # the call up to its end: the lambda's braces closed (its [&fa](INI *ini) can be on a line of its own), or a `;` before any
                body, depth, opened = [], 0, False
                for x in lines[n:n + 40]:
                    body.append(x)
                    depth += x.count("{") - x.count("}")
                    opened = opened or "{" in x
                    if (opened and depth <= 0) or (not opened and x.rstrip().endswith(";")):
                        break
                text = "\n".join(body)
                # a table of the lambda's own (FormationAssistant.cpp's `const FieldParse fields[] = {{"FormationPreviewDecal", parseDecal, ..}}`)
                out += [r for r in self.rows if r.file == f and n < r.line <= n + len(body)]
                for t in set(re.findall(r"\b(k\w+)\b", text)):
                    out += [r for r in self.rows if r.table == t and r.file == f]
                for p in set(re.findall(r"\b(\w*[Pp]arse\w*)\s*\(", text)):
                    out += self.parser_rows(p, f)
        return out

    def line_rows(self, keywords: set[str]) -> dict[str, set[str]]:
        """block keyword -> the row names (lower case) its parser's files parse with a plain INI::parse* reader: one line each, never a block"""
        out = {}
        for k in keywords:
            files = self.block_files(k) | {f for f in DEF_FILES.get(k, []) if f in self.files}
            out[k] = {r.field.lower() for r in self.rows if r.file in files and r.parser.startswith("INI::parse")}
        return out

    def block_keywords(self) -> tuple[set[str], set[str]]:
        """(every RotWK block keyword, the ones the engine parses for real), lower case, from INIBlockStubs.cpp and the registerBlock calls"""
        stubs = "\n".join(self.files.get("Common/INI/INIBlockStubs.cpp", []))
        m = re.search(r"RotwkBlockKeywords\(\)\s*\{(.*?)\};", stubs, re.S)
        if not m:
            raise SystemExit("engine/src/Common/INI/INIBlockStubs.cpp: RotwkBlockKeywords not found")
        every = {k.lower() for k in STRING.findall(m.group(1))}
        real = {k.lower() for k in re.findall(r"else if \(keyword == \"(\w+)\"\)", stubs)}
        real |= {k for k in every if self.block_files(k)}
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
    """the STOPS.md rows naming the module class and the field; for a definition's field (no class), the rows naming the field when its name is
    distinctive (8+ characters, HitPassengerPercentage -> S-321)"""
    if not stops or not (cls or len(field) >= 8):
        return ""
    hits = [sid for sid, line in stops if (not cls or re.search(r"\b" + re.escape(cls) + r"\b", line)) and re.search(r"\b" + re.escape(field) + r"\b", line)]
    return " ".join(hits[:3])


# ---- the join --------------------------------------------------------------------------------------------------------------------------------------
ORDER = ["missing-module", "parsed-only-module", "stub-block", "raw", "struct-unknown", "no-table-row", "not-read", "unmapped-block", "parsed-unused", "custom-parser",
         "own-reader", "dynamic-row", "used-raw", "used-outside", "used", "not-in-rotwk", "unknown-class"]


def audit(mod, rw, rw_object, coverage, src: Source, real_blocks: set[str], stops=()):
    rows = []
    rows_cache: dict[str, list[Row]] = {}

    def rows_of(struct):
        if struct not in rows_cache:
            rows_cache[struct] = src.rows_of_struct(struct)
        return rows_cache[struct]

    def files_of(structs):
        return {r.file for s in structs for r in rows_of(s)}

    def dynamic_status(field, hit, numbered):
        """a row made at run time. Numbered ("Color" + std::to_string(i + 1), then ROW(.., d, m_colorKey[i]) with `static DefaultColorModuleData d;`):
        the member the row stores is followed, and so is the member a name on one line or a key test's branch reads it into. Named by a list
        of the block's own (MiscAudio::fieldNames()): read when another line asks for the name as a literal (`miscAudio.get("EnabledHotKeyPressed")`);
        a name list of another file (read by index) or nothing is left to a hand check"""
        f, n = hit.rsplit(":", 1)
        n = int(n)
        if numbered and "to_string" in src.files[f][n - 1]:
            code = src.code_of(f)
            near = [(m.group(1), k + 1) for k in range(n, min(len(code), n + 3)) for m in [re.search(r"\b(m_\w+)\s*\[", code[k])] if m]
            if near:
                body = "\n".join(code[max(0, n - 8):n])
                st = re.findall(r"\bstatic\s+(\w+)\s+\w+\s*;", body)
                # (the row's own line, ROW(names[i].c_str(), .., d, m_colorKey[i]), is no read)
                use = src.member_read(near[0][0], st[-1] if st else "?", origin=f, skip=frozenset({(f, near[0][1])}))
                return ("used", use, near[0][0], "generated-row") if use else ("parsed-unused", hit, near[0][0], "generated-row")
            return "dynamic-row", hit, "", "string"
        # the name and the member it is read into on one line (AIPathfindConfig.cpp's getInt(gd, "MaxCellsPatchPath", c.patchPathLimit, ..)):
        # that member is followed
        into = re.search(r"\"" + re.escape(field) + r"\"\s*,\s*&?\s*\w+\s*(?:\.|->)\s*(\w+)\s*[,)]", src.files[f][n - 1])
        if into:
            use = src.member_read(into.group(1), "?", origin=f, skip=frozenset({(f, n)}))  # (that line stores it)
            return ("used", use, into.group(1), "named-row") if use else ("parsed-unused", hit, into.group(1), "named-row")
        # a key test whose branch stores it (ProductionSettings.cpp's compareNoCase(key, "MultiPlayUnitSpeedMult") .. out.multiPlayUnitSpeedMult)
        member, k = src.branch_member(f, n)
        if member:
            use = src.member_read(member, "?", origin=f, skip=frozenset({(f, k)}))
            return ("used", use, member, "named-row") if use else ("parsed-unused", f"{f}:{k}", member, "named-row")
        # only in a file that sees the list's header, and only a distinctive name (8+ characters: a "Color" or "Alpha" elsewhere is some other
        # key, GodotGameWorld.cpp's dictionaries)
        hdr = f.rsplit(".", 1)[0] + ".h"
        seen = src.visible_file(hdr) if hdr in src.files else set()
        asks = [(g, k) for g, k in src.string_lines(field) if g != f and g in seen] if len(field) >= 8 else []
        use = next((f"{g}:{k}" for g, k in asks if not src.name_list_line(g, k)), "")
        if use:
            return "used", f"{use} (asks for the row by name)", "", "by-name"
        listed = next((f"{g}:{k}" for g, k in asks if src.name_list_line(g, k)), "")
        # a name list of another file is read by index too (UnitVoiceResponse.cpp's kRowNames for SoundSelectorClientBehavior's rows): which
        # rows it asks for is not followed, a lead for the hand check
        return "dynamic-row", f"{listed} (a name list of another file, read by index)" if listed else hit, "", "string"

    def typed_status(field, structs, nested, files=(), path=(), block="", block_path=()):
        cands = [r for s in structs for r in rows_of(s) if r.field.lower() == field.lower()] if not path else []
        how = "struct"
        if not cands and block:
            # the block's own parser's rows before any row of that name in its files; a nested block's through the parser of its key's row
            # (ArmyDefinition's ArmyMemberDefinition row -> parseArmyMemberDefinition -> its table, not CombatChainDefinition's Unit)
            level = src.block_parser_rows(block)
            for key in block_path:
                heads = [r for r in level if r.field.lower() == key.lower()]
                level = [x for h in heads for x in src.parser_rows(h.parser, h.file)]
            cands = [r for r in level if r.field.lower() == field.lower()]
            how = "block-parser"
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
                numbered = re.fullmatch(r"([A-Za-z_]+?)(\d+)", field)
                if not hit and numbered:
                    # numbered names made at run time: "Color" + std::to_string(i + 1)
                    hit = next((f"{f}:{n}" for f, n in src.string_lines(numbered.group(1)) if f in fs and "to_string" in src.files[f][n - 1]), "")
                if hit:
                    return dynamic_status(field, hit, numbered)
        if not cands and block:
            # a name no row of the block's table has, or a nested block whose key has none, falls to the table's catch-all row: the port loads it
            # and applies nothing (the name is in RotWK's own table or the mod's error; the field is not read either way)
            ca = src.catch_all(block)
            if ca and not (block_path and any(r.file == ca[0] and r.table == ca[1] and r.field.lower() == block_path[0].lower() for r in src.rows)):
                # unless a hand-written reader of the block's text names it (Shell.cpp's readGameDataBool(text, "ShellMapOn")): a hand check
                hit = next(((f, n) for f, n in src.string_lines(field) if f in src.own_reader_files(block) and f != ca[0]), None)
                if hit:
                    return "own-reader", f"{hit[0]}:{hit[1]}", "", "string"
                return "not-read", f"{ca[0]}:{ca[3]} (the catch-all row: {ca[2]} records it unapplied, S-152)", "", "catch-all"
        if not cands:
            return "no-table-row", "", "", how
        followed = False
        for r in cands:
            for m in r.member.split("|"):
                if not re.fullmatch(r"[A-Za-z_]\w+", m) or m in ("nullptr", "NULL"):
                    continue
                followed = True
                # a row of the struct's own tables without offsetof stores into that struct; a nested or block-file row of no known struct: anywhere
                use = src.member_read(m, r.struct or (structs[0] if how == "struct" and structs else "?"), origin=r.file, parser=(r.file, r.func))
                if use and "|" in r.member and sum(1 for x in src.rows if x.file == r.file and x.parser == r.parser) > 1:
                    # a parser shared by several rows (CameraSettings.cpp's parseShake, its userData picks the member): which one this row's is, is
                    # not told (one row's parser storing a min and a max is no such case)
                    use += f" INFERENCE: its parser names several members ({r.member}), {m} may be another row's"
                if use:
                    return "used", use, m, how
        r = cands[0]
        if not followed:
            return "custom-parser", r.loc, r.parser, how  # the row's own parser stores where the tool cannot follow
        return "parsed-unused", r.loc, r.member, how

    object_names = {d for (t, _, _), mf in mod.items() if t == "object" for d in mf.defs}

    def by_name(reader, field, names):
        """a line of a block kept by name (UnitSpecificSounds): read when a reader of the block (a file naming its reader class) asks for that
        name, as a literal or as a literal prefix and an Object's name (UnitVoiceResponse.cpp's "VoiceEnterUnit" + target->getTemplate()->getName())"""
        readers = src.files_naming(reader)
        hit = next((f"{f}:{n}" for f, n in src.string_lines(field) if f in readers), "")
        if hit:
            return "used", hit, "by-name"
        for f, n, prefix in src.name_prefixes(readers):
            rest = field[len(prefix):]
            if field.startswith(prefix) and rest:
                if rest in names:
                    return "used", f"{f}:{n} (\"{prefix}\" + the Object {rest})", "by-name-prefix"
                return "parsed-unused", f"{f}:{n} asks for \"{prefix}\" + an Object's name; no Object {rest}", "by-name-prefix"
        return "parsed-unused", "no reader asks for this name", "by-name"

    def raw_block(owner, field):
        """an Object's nested block the template keeps as raw lines (ThingFactory.cpp, stop S-071): its readers are the files that walk the raw
        blocks (rawBlocks) and name the block (AIThreatFinder.cpp's "ThreatBreakdown", LivingWorldUnits.cpp's readSets(tt, "AutoResolveArmor",
        "Armor", ..)); a field is read when such a file names it. None when no file reads the block"""
        block = owner.split("/")[1]
        readers = {f for f in src.files_naming("rawBlocks") | src.files_naming("m_rawBlocks") if any(g == f for g, _ in src.string_lines(block))}
        if not readers:
            return None
        hit = next((f"{f}:{n}" for f, n in src.string_lines(field) if f in readers), "")
        if hit:
            return "used", hit, "raw-block"
        return "parsed-unused", f"the block's readers ({', '.join(sorted(readers))}) do not ask for it", "raw-block"

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
                    last = "/" + owner.split("/")[-1].lower()
                    if status == "no-table-row" and nested and last in BY_NAME_BLOCKS:
                        # a module's block of any names (a sound selector's UnitSpecificSounds: SoundSelectorClientBehavior.cpp's kUnitSpecificTable,
                        # a catch-all row) is asked for by the Object's reader (UnitVoiceResponse::objectNameVoice asks the selectors first)
                        status, where, how = by_name(BY_NAME_BLOCKS[last], field, object_names)
                if cov["status"] == "parsed-only" and status in ("used", "parsed-unused"):
                    # the class has no runtime class, but another file may draw / use its parsed data (W3DStreakDraw's data, drawn by
                    # GodotGameWorld.cpp): an unambiguous read of the member is used-outside (its own status, never folded into used); an
                    # AMBIGUOUS or INFERENCE read stays parsed-only-module and keeps its where as a lead to check by hand
                    certain = status == "used" and "AMBIGUOUS" not in where and "INFERENCE" not in where
                    status = "used-outside" if certain else "parsed-only-module"
        elif typ == "object":
            if not nested and field.lower() not in rw_object:
                status = "not-in-rotwk"
            elif owner.lower() in BY_NAME_BLOCKS:
                status, where, how = by_name(BY_NAME_BLOCKS[owner.lower()], field, object_names)
            elif nested:
                structs = [s for s in NESTED_DEF_STRUCTS.get(owner.lower(), [])]
                if not structs:
                    # a second reader of the Object block parsing the nested block (FormationAssistant.cpp's FormationPreviewDecal row ->
                    # parseDecal -> Texture), else a raw block read by name, else the tool cannot tell
                    status, where, member, how = typed_status(field, [], False, block="object", block_path=tuple(owner.split("/")[1:]))
                    if status == "no-table-row":
                        raw = raw_block(owner, field)
                        status, where, member, how = (raw[0], raw[1], "", raw[2]) if raw else ("unmapped-block", "", "", "")
                else:
                    struct = structs[0]
                    status, where, member, how = typed_status(field, structs, False)
            else:
                # another reader of the Object block with its own table (FormationAssistant's FormationPreviewDecal), else ThingFactory's slot read
                # by name (ThingTemplate::findField)
                others = {f for f in src.block_files("object") if any(r.file == f and r.field.lower() == field.lower() for r in src.rows)}
                if others:
                    status, where, member, how = typed_status(field, [], False, others)
                if not others or status != "used":
                    use = src.string_read(field)
                    listed = "" if use else src.string_read(field, lists=True)
                    if listed:
                        # only in a name list read by index (the voice and sound rows): which rows the port asks for is not followed
                        status, where, how = "dynamic-row", f"{listed} (a name list read by index: the rows asked are not followed)", "by-index"
                    elif use or not others:
                        status, where, how = ("used", use, "by-name") if use else ("parsed-unused", "", "by-name")
        elif typ not in real_blocks:
            # a recording stub in the INI registry, but a hand-written reader of its own may read the file (TerrainTypes.cpp reads terrain.ini's
            # Texture lines): a file naming the block keyword or its .ini that names the field as a string (any case)
            readers = src.own_reader_files(typ)
            hits = [(f, n) for f, n in src.string_lines(field) if f in readers]
            status, where, how = ("own-reader", f"{hits[0][0]}:{hits[0][1]}", "string") if hits else ("stub-block", "", "")
            if not hits and readers:
                # the block is read by its own reader (CursorFile.cpp reads Mouse.ini's MouseCursor blocks), which does not ask for this field
                status, where, how = "parsed-unused", f"its own reader ({', '.join(sorted(readers))}) does not ask for it", "own-reader-unasked"
            # a key test whose branch stores the value into a member (TerrainRoads.cpp's `if (key == "roadwidth") cur.roadWidth = ..`): followed
            for f, n in hits:
                member, k = src.branch_member(f, n)
                if member:
                    use = src.member_read(member, "?", origin=f, skip=frozenset({(f, k)}))
                    status, where, how = ("used", use, "own-reader-member") if use else ("parsed-unused", f"{f}:{k}", "own-reader-member")
                    break
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
                status, where, member, how = typed_status(field, structs, nested, files, block=typ, block_path=tuple(owner.split("/")[1:]))
        top =sorted(mf.values.items(), key=lambda kv: -kv[1])[:3]
        rows.append({"status": status, "block": typ, "owner": owner, "field": field, "defs": len(mf.defs), "uses": mf.count, "struct": struct,
                     "member": member, "where": where, "how": how, "stop": stops_for(stops, cls, field), "values": " | ".join(v for v, _ in top),
                     "src": mf.src, "tool": status})
    # a name no reader asks for itself may be asked for through another field holding it (SpecialPower's
    # UnitSpecificSoundToUseAsInitiateIntendToDoVoice = VoiceInitiateCaptureBuilding): read when that field is
    by_key = {(r["block"], r["owner"], r["field"]): r for r in rows}
    holders: dict[str, list[tuple]] = defaultdict(list)
    for key, mf in mod.items():
        if key[1].lower() not in BY_NAME_BLOCKS:
            for v in mf.values:
                for tok in v.replace("=", " ").split():
                    holders[tok].append(key)
    for r in rows:
        if r["how"] == "by-name" and r["status"] == "parsed-unused" and holders.get(r["field"]):
            refs = sorted(holders[r["field"]], key=lambda k: by_key[k]["status"] != "used")
            ref = by_key[refs[0]]
            name = "/".join(x for x in refs[0] if x)
            r["status"] = r["tool"] = "used" if ref["status"] == "used" else "parsed-unused"
            r["where"] = f"named by {name} ({ref['status']}{': ' + ref['where'] if ref['where'] else ''})"
            r["how"] = "by-name-field"
    rank = {s: i for i, s in enumerate(ORDER)}
    rows.sort(key=lambda r: (rank.get(r["status"], 99), -r["defs"], r["block"], r["owner"], r["field"]))
    return rows


# Object's nested blocks kept as raw lines and read by name (ThingFactory.cpp keeps them, stop S-071; GameLogic/UnitSpecificSound.cpp's rawValue
# answers a name): every line is a name and a value, the name is the field
BY_NAME_BLOCKS = {"/unitspecificsounds": "UnitSpecificSound", "/unitspecificfx": "UnitSpecificFX"}

# Object's nested blocks: the struct each one's parser fills (ThingFactory's keyword table; ArmorSet.cpp, WeaponSet.cpp, ...)
NESTED_DEF_STRUCTS = {
    "/armorset": ["ArmorTemplateSet"],
    "/weaponset": ["WeaponTemplateSet"],
    "/locomotorset": ["LocomotorSetEntry"],  # Locomotor.cpp's LocomotorSet table
}


# ---- the output ------------------------------------------------------------------------------------------------------------------------------------
KNOWN_LIMIT = ("text in quotes and comments, a local or a parameter of the member's name and a declaration are not taken for reads any more; "
               "a member of another struct with the same name, read in a file that names our struct too, still is (its where is marked AMBIGUOUS and can "
               "point at the other struct's read; the status is wrong only when our member has no read of its own). STAGED, INFERENCE and "
               "AMBIGUOUS in where are leads to check by hand; so is `INFERENCE: its parser names several members` (one parser stores several "
               "members, which one is this row's is not told) and `(asks for the row by name)` (a literal of the row's name in a reader). "
               "Every .ini of the archives is read, also one the game never loads (fxparticlesystemcustom.ini: its InitFile is commented out in "
               "default/subsystemlegend.ini), so such a file's fields are reported as if loaded. Outside RotWK's grammar a line without '=' "
               "opens a block only when an End closes it (headers()). Rows a builder makes at run time (name arrays, a loop's number, add(..)) "
               "are followed only in the forms the tool knows; the rest stay no-table-row. A catch-all row (GameLogicSettings.cpp's "
               "noteUnapplied, S-152) gives not-read: the port loads the field and applies nothing.")
COLS = ["status", "block", "owner", "field", "defs", "uses", "struct", "member", "where", "how", "stop", "values", "src", "tool"]

# a hand-checked verdict (manual-check.tsv, columns block owner field status-tool verdict where note)
VERDICTS = {"used", "parsed-unused", "not-read"}
# the statuses that leave the use to a hand check: they never contradict a verdict
HAND = {"custom-parser", "own-reader", "dynamic-row", "unmapped-block"}


def apply_manual(rows, path: Path) -> dict:
    """sets the status of every field a hand check gave a verdict to; the tool's own status stays in the tool column. Returns the rows to look at
    again: conflicts (the tool's status now is neither the one the check saw nor the verdict: the tool erred again or the code changed), unmatched
    (no field of that name any more) and skipped (an aggregate row such as `(75 fields)` or a verdict that is not one of VERDICTS)"""
    with open(path, encoding="utf-8", newline="") as f:
        checks = list(csv.DictReader(f, delimiter="\t", quoting=csv.QUOTE_NONE))
    need = {"block", "owner", "field", "status-tool", "verdict", "where"}
    if not checks or not need <= set(checks[0]):
        raise SystemExit(f"{path}: not a manual check table (columns {sorted(need)} needed)")
    by_key = {(r["block"], r["owner"], r["field"]): r for r in rows}
    out = {"applied": 0, "conflicts": [], "unmatched": [], "skipped": []}
    for c in checks:
        key = (c["block"], c["owner"], c["field"])
        if c["verdict"] not in VERDICTS or not re.fullmatch(r"[\w.%]+", c["field"] or ""):  # (a '%' in a name: HoleHealthRegen%PerSecond)
            out["skipped"].append(c)
            continue
        r = by_key.get(key)
        if r is None:
            out["unmatched"].append(c)
            continue
        if r["status"] not in (c["status-tool"], c["verdict"]) and r["status"] not in HAND:
            out["conflicts"].append((c, r["status"], r["where"]))
        r["tool"] = r["status"]
        r["status"], r["where"], r["how"] = c["verdict"], f"{c['where']} MANUAL" + (f": {c['note']}" if c.get("note") else ""), "manual"
        out["applied"] += 1
    return out


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


def write(rows, out: Path, queue, diag, manual: dict | None = None):
    out.mkdir(parents=True, exist_ok=True)
    old = {}
    prev = out / "field-audit.tsv"
    if prev.exists():
        with open(prev, encoding="utf-8", newline="") as f:
            for r in csv.DictReader(f, delimiter="\t", quoting=csv.QUOTE_NONE):  # a mod value's quote is text, not a quoted cell
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
        f.write(f"\nKnown limit: {KNOWN_LIMIT}\n")
        if manual is not None:
            f.write(f"\nHand-checked verdicts applied: {manual['applied']} (status = the verdict, the tool's own in the tool column of field-audit.tsv)\n")
            f.write(f"\n## Hand checks to look at again ({len(manual['conflicts'])} conflicts, {len(manual['unmatched'])} unmatched, "
                    f"{len(manual['skipped'])} skipped)\n\n| block | owner | field | checked as | verdict | tool now | where now |\n|---|---|---|---|---|---|---|\n")
            for c, now, where in manual["conflicts"]:
                f.write(f"| {c['block']} | {c['owner']} | {c['field']} | {c['status-tool']} | {c['verdict']} | {now} | {where} |\n")
            for kind in ("unmatched", "skipped"):
                for c in manual[kind]:
                    f.write(f"| {c['block']} | {c['owner']} | {c['field']} | {c['status-tool']} | {c['verdict']} | {kind} | |\n")
        if diag.get("unclosed"):
            f.write(f"\n{len(diag['unclosed'])} definitions ended at a new definition with a block still open (first: {diag['unclosed'][0]})\n")
        f.write("\n## Module queue (missing, parsed-only, raw; by the mod's definitions)\n\n| class | status | definitions | type |\n|---|---|---|---|\n")
        for q in queue:
            f.write(f"| {q[0]} | {q[1]} | {q[2]} | {q[3]} |\n")
        for status in ORDER[:ORDER.index("dynamic-row") + 1]:
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
    ap.add_argument("--manual", type=Path, help="hand-checked verdicts (manual-check.tsv): they set the status, a disagreement is listed")
    a = ap.parse_args()
    src = Source(a.src)
    every, real = src.block_keywords()
    vfs = eg.mount(a.base, a.overlay)
    diag: dict = {}
    mod = mod_fields(vfs, every, diag, Grammar(a.rw), src.line_rows(every & real))
    rw, rw_object = rotwk_tables(a.rw)
    coverage = load_coverage(a.coverage)
    rows = audit(mod, rw, rw_object, coverage, src, real, load_stops(a.stops))
    manual = apply_manual(rows, a.manual) if a.manual else None
    counts, changed = write(rows, a.out, module_queue(rows, coverage, diag.get("modules", {})), diag, manual)
    if manual is not None:
        print(f"hand checks: {manual['applied']} applied, {len(manual['conflicts'])} conflicts, {len(manual['unmatched'])} unmatched, "
              f"{len(manual['skipped'])} skipped (field-audit.md)")
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
