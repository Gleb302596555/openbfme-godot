"""modifier_map.py: the attribute modifier map (owner 2026-11-19): every attribute a ModifierList of the mod changes, who in RotWK 2.01 reads it,
and whether the port reads it there.

Three independent sources, joined per attribute:

1. The mod (RotWK data with the Ennorath overlay, ennorath_graph's mount): every ModifierList with its `Modifier = <ATTRIBUTE> <value> [names]`
   lines, Category, Duration and the rest of its fields; every field of any definition (objects and their modules, weapons, upgrades, special
   powers, object creation lists, sciences, ExperienceLevel, crates) whose value names a ModifierList; and whether a playable faction reaches the
   list (ennorath_graph.walk from each PlayerTemplate with PlayableSide = Yes; an ExperienceLevel counts when one of its TargetNames is reached).
2. RotWK 2.01 (game.dat): the attribute names from the table RW 0xDA6D28 (28 pointers; the index is the attribute's number), and every reader.
   Every read reaches TheModifierListStore's value query RW 0x6144BF(list index, attribute, &out, flag); the functions that pass their own
   argument on to it (`push dword ptr [ebp + N]`) or tail-jump to such a function are wrappers and are followed the same way: the pool's sum and
   product (RW 0x804F39 / 0x804FFF), their patched copy outside .text (RW 0xECA015, called through RW 0xECA000 by AdjustDamage's ARMOR read),
   the pool's apply on add (RW 0x8052FB: HEALTH, HEALTH_MULT), Object::getAttributeModifierSum / Product (RW 0x68C818 / 0x68C82D). Every
   executable section is scanned (RotWK 2.01 carries patched code in stxt774, stxt371, .mackt, .danetta). At each call the attribute is the k-th
   push counted back from the call (stopping at the previous call, ret or jmp): an immediate, or a register whose values are followed back through
   mov / xor / add / sub / setcc (EmotionNugget::canApply RW 0x8E149D reads RESIST_FEAR or RESIST_TERROR: both are listed); anything else is
   listed as unresolved (modifier-unresolved.tsv), never guessed. Function bounds come from the whole-binary inventory
   (rw_function_blocks_full.json, the maps run's angr pass; the default rw_function_blocks.json holds module code only).
3. The port (engine/src, code only: comments and string literals blanked): the calls of Object::attributeModifierSum / attributeModifierProduct
   (attribute = argument 0) and of every port function that passes one of its own parameters on to such a call (a wrapper, found to a fixed point,
   e.g. Economy::attributeModifierProduct(obj, type): argument 1). The attribute argument is an integer literal or an ATTRIBUTE_* enumerator of
   engine/src; other arguments are listed as unresolved. The `RW 0x...` citations of each file give the RotWK functions it ports.

A RotWK reader is
   ported         a file cites an address inside the reader's function and reads the same attribute;
   cited-no-read  a file cites the function but no file citing it reads that attribute (the reader's port lost the attribute: Local 85's RANGE);
   not-cited      no file cites the function.
An attribute's verdict: `dead in RotWK` (the mod sets it, RotWK reads it nowhere: no effect in the original either), `not read by the port`,
`partly ported`, `ported`, `port-only reads` (the port reads it where RotWK has no reader), `unknown to RotWK` (a mod name the table lacks).

Outputs (--out): modifier-map.md (the summary and the per attribute detail), modifier-lists.tsv, modifier-readers.tsv, modifier-port-reads.tsv,
modifier-unresolved.tsv, modifier-matrix.tsv (the button matrix's red buttons with their modules' lists and the attributes the port does not read).

    python tools/mod/modifier_map.py --base <RotWK install> --overlay <Ennorath install> --game <game.dat> --out <dir>
"""
from __future__ import annotations

import argparse
import csv
import json
import re
import struct
import sys
from collections import defaultdict
from dataclasses import dataclass, field
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import citation_check as cc  # noqa: E402

NAMES_TABLE = 0xDA6D28
NAME_COUNT = 28
# The one function every attribute read reaches: TheModifierListStore's value query RW 0x6144BF(list index, attribute, &out, flag) (ecx =
# RW 0xDE3C14) hands the attribute to the list's own RW 0x805268, which compares it with each entry. The pool's sum and product (RW 0x804F39 /
# RW 0x804FFF, their patched copy RW 0xECA015) pass their attribute on to it (`push dword ptr [ebp + 8]`), and so does the pool's apply on add
# (RW 0x8052FB: HEALTH, HEALTH_MULT): all of them are found as wrappers from here. RW 0x805268 has no other caller.
SEEDS = {0x6144BF: 1}
KNOWN = {0x6144BF: "list value", 0x804F39: "sum", 0x804FFF: "product", 0xECA015: "sum (patch copy)", 0x8052FB: "apply on add"}
CITE = re.compile(r"\bRW\s+0x([0-9A-Fa-f]{5,8})\b")


# ---- RotWK --------------------------------------------------------------------------------------------------------------------------------

@dataclass
class Reader:
    site: int          # the call instruction
    function: int      # its function (inventory)
    kind: str          # sum / product
    attribute: int     # the attribute number
    via: tuple[int, ...] = ()  # the wrappers between the call and the pool method, innermost first


@dataclass
class Unresolved:
    site: int
    function: int | None
    target: int
    why: str


class Image:
    def __init__(self, game: Path):
        import pefile
        pe = pefile.PE(str(game), fast_load=True)
        self.base = pe.OPTIONAL_HEADER.ImageBase
        self.data = pe.get_memory_mapped_image()
        # every executable section: RotWK 2.01 carries patched code outside .text (stxt774, stxt371, .mackt, .danetta: AdjustDamage's ARMOR
        # query RW 0x5D8AF1 calls the copy of Object::getAttributeModifierSum at RW 0xECA000)
        self.code = [(self.base + s.VirtualAddress, self.base + s.VirtualAddress + s.Misc_VirtualSize)
                     for s in pe.sections if s.Characteristics & 0x20000000]

    def executable(self, va: int) -> bool:
        return any(a <= va < b for a, b in self.code)

    def u32(self, va: int) -> int:
        return struct.unpack_from("<I", self.data, va - self.base)[0]

    def cstr(self, va: int) -> str:
        off = va - self.base
        end = self.data.index(b"\0", off)
        return self.data[off:end].decode("latin-1")

    def bytes_at(self, va: int, n: int) -> bytes:
        off = va - self.base
        return self.data[off:off + n]

    def branch_index(self) -> dict[int, list[tuple[int, str]]]:
        """target -> [(site, 'call' | 'jmp')] for every rel32 E8 / E9 in the executable sections (a byte scan: a hit inside another
        instruction is dropped later, when the decode of its function does not land on it)."""
        out: dict[int, list[tuple[int, str]]] = defaultdict(list)
        for start, end in self.code:
            t = self.data[start - self.base:end - self.base]
            for i in range(len(t) - 5):
                op = t[i]
                if op in (0xE8, 0xE9):
                    site = start + i
                    target = (site + 5 + struct.unpack_from("<i", t, i + 1)[0]) & 0xFFFFFFFF
                    if self.executable(target):
                        out[target].append((site, "call" if op == 0xE8 else "jmp"))
        return out


def attribute_names(img: Image) -> list[str]:
    return [img.cstr(img.u32(NAMES_TABLE + 4 * i)) for i in range(NAME_COUNT)]


def decode_run(md, img: Image, start: int, site: int):
    """The instructions from `start` up to and including the one at `site` (None when the decode does not land on `site`)."""
    out = []
    for ins in md.disasm(img.bytes_at(start, site - start + 16), start):
        out.append(ins)
        if ins.address >= site:
            break
    return out if out and out[-1].address == site else None


REG_IMM = re.compile(r"^(e[a-d]x|e[sd]i|ebp)$")


def argument_at(instrs, k: int):
    """The k-th push back from the last instruction (the call): ('imm', value), ('arg', ebp-index), or ('?', why)."""
    pushes = []
    i = len(instrs) - 2
    while i >= 0:
        ins = instrs[i]
        m = ins.mnemonic
        if m == "call" or m.startswith("ret") or m == "jmp":
            break
        if m == "push":
            pushes.append((i, ins.op_str))
            if len(pushes) > k:
                break
        elif m == "pop" or (m in ("add", "sub") and ins.op_str.startswith("esp")):
            return ("?", f"stack changed at {ins.address:#x} ({m} {ins.op_str})")
        i -= 1
    if len(pushes) <= k:
        return ("?", f"{len(pushes)} pushes before the call")
    idx, op = pushes[k]
    if re.fullmatch(r"(0x[0-9a-f]+|\d+)", op):
        return ("imm", int(op, 0))
    m = re.fullmatch(r"dword ptr \[ebp \+ (0x[0-9a-f]+|\d+)\]", op)
    if m:
        off = int(m.group(1), 0)
        if off >= 8 and off % 4 == 0:
            return ("arg", (off - 8) // 4)
        return ("?", f"push {op}")
    if REG_IMM.match(op):
        vals, why = register_values(instrs, idx, op)
        if vals:
            return ("imm", vals if len(vals) > 1 else next(iter(vals)))
        return ("?", f"push {op}: {why}")
    return ("?", f"push {op}")


LOW8 = {"eax": "al", "ecx": "cl", "edx": "dl", "ebx": "bl"}
CALL_CLOBBERS = {"eax", "ecx", "edx"}


def writes(ins, reg: str) -> bool:
    """the instruction writes `reg` (or its low byte) as its first operand."""
    first = ins.op_str.split(",")[0].strip()
    return first in (reg, LOW8.get(reg, "?"), reg[1:] if len(reg) == 3 else "?")


def register_values(instrs, idx: int, reg: str, depth: int = 0):
    """The constant values `reg` can hold before instrs[idx], walking back in address order: mov reg, imm; xor reg, reg; mov reg, reg2;
    add / sub reg, imm; setcc on its low byte after the register was zeroed ({0, 1}). (set, '') or (None, why): anything else is not guessed."""
    if depth > 8:
        return None, "too deep"
    for j in range(idx - 1, -1, -1):
        ins = instrs[j]
        m, ops = ins.mnemonic, ins.op_str
        if m == "call":
            return (None, f"a call at {ins.address:#x} clobbers it") if reg in CALL_CLOBBERS else register_values(instrs, j, reg, depth)
        if not writes(ins, reg) or m in ("cmp", "test", "push"):
            continue
        mm_ = re.fullmatch(rf"{reg}, (0x[0-9a-f]+|\d+)", ops)
        if m == "mov" and mm_:
            return {int(mm_.group(1), 0)}, ""
        if m == "xor" and ops == f"{reg}, {reg}":
            return {0}, ""
        src = re.fullmatch(rf"{reg}, (e[a-d]x|e[sd]i|ebp)", ops)
        if m == "mov" and src:
            return register_values(instrs, j, src.group(1), depth + 1)
        if m in ("inc", "dec") and ops == reg:
            base, why = register_values(instrs, j, reg, depth + 1)
            if base is None:
                return None, why
            return {v + 1 if m == "inc" else v - 1 for v in base}, ""
        if m in ("add", "sub") and mm_:
            base, why = register_values(instrs, j, reg, depth + 1)
            if base is None:
                return None, why
            d = int(mm_.group(1), 0)
            return {v + d if m == "add" else v - d for v in base}, ""
        if m.startswith("set") and ops == LOW8.get(reg):
            base, why = register_values(instrs, j, reg, depth + 1)
            if base == {0}:
                return {0, 1}, ""
            return None, f"{m} {ops} on a register not zeroed"
        return None, f"{m} {ops} at {ins.address:#x}"
    return None, "no constant found"



def rotwk_readers(img: Image, inv: cc.Inventory, blocks: dict[int, list[tuple[int, int]]]):
    import capstone
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    branches = img.branch_index()
    readers: list[Reader] = []
    unresolved: list[Unresolved] = []
    wrappers: dict[int, tuple[str, int, tuple[int, ...]]] = {}
    work = [(t, KNOWN[t], k, ()) for t, k in SEEDS.items()]
    seen = set()
    while work:
        target, kind, k, via = work.pop()
        if (target, k) in seen:
            continue
        seen.add((target, k))
        for site, how in branches.get(target, []):
            fn = inv.owner(site)
            if fn is None:
                # a byte pattern inside data or another instruction is no call; a real one outside the inventory is listed
                ins = next(md.disasm(img.bytes_at(site, 16), site), None)
                if ins is not None and ins.mnemonic in ("call", "jmp") and int(ins.op_str, 0) == target:
                    unresolved.append(Unresolved(site, None, target, f"{how} from outside the inventory"))
                continue
            # the function's blocks in address order up to the site (a register set in an earlier block reaches it: RW 0x8E146D's edi)
            run = []
            for s, n in sorted(blocks.get(fn, ())):
                if s > site:
                    break
                if s <= site < s + n:
                    part = decode_run(md, img, s, site)
                    run = None if part is None else run + part
                    break
                run += list(md.disasm(img.bytes_at(s, n), s))
            if not run or run[-1].address != site:
                continue  # not an instruction of its function: the byte scan's E8 / E9 inside another instruction
            if how == "jmp":
                if fn != target:  # a tail jump: the function passes its own arguments on unchanged
                    wrappers[fn] = (kind, k, via)
                    work.append((fn, KNOWN.get(fn, kind), k, (target,) + via))
                continue
            what, val = argument_at(run, k)
            if what == "imm":
                for v in sorted(val) if isinstance(val, set) else [val]:  # a computed attribute: every value it can take is read
                    readers.append(Reader(site, fn, kind, v, (target,) + via))
            elif what == "arg" and any(i.mnemonic == "mov" and i.op_str == "ebp, esp" for i in run):
                wrappers[fn] = (kind, val, via)
                work.append((fn, KNOWN.get(fn, kind), val, (target,) + via))
            else:
                unresolved.append(Unresolved(site, fn, target, str(val)))
    return readers, unresolved, wrappers


# ---- the port -----------------------------------------------------------------------------------------------------------------------------

def code_only(text: str) -> str:
    """comments and string / character literals blanked, positions kept (field_audit.code_only over the whole text)."""
    import field_audit
    return "\n".join(field_audit.code_only(text.split("\n")))


ENUM = re.compile(r"\b(ATTRIBUTE_\w+)\s*=\s*(0x[0-9A-Fa-f]+|\d+)")
DEF = re.compile(r"(?:\b[\w:<>,&*\s]+?\s)?(?:(\w+)::)?(\w+)\s*\(([^()]*)\)\s*(?:const\s*)?(?:override\s*)?\{")
# (name, argument count) -> the attribute argument: Object::attributeModifierSum(type, name, out) / attributeModifierProduct(type, name, innate,
# out); a port function of the same name and another arity (Economy::attributeModifierProduct(obj, type)) is another function
NOT_FOLLOWED = {"attributeModifierSum", "attributeModifierProduct", "operator", "update", "onObjectCreated", "getValue"}
BASE_QUERIES = {("attributeModifierSum", 3): 0, ("attributeModifierProduct", 4): 0,
                # Economy's (obj, type) seams: Economy::attributeModifierProduct / Sum call a std::function that GameLogic.cpp installs as a lambda
                # around Object::attributeModifierProduct / Sum (setAttributeModifierProvider / setAttributeModifierSumProvider), so the type passes on
                ("attributeModifierProduct", 2): 1, ("attributeModifierSum", 2): 1,
                # the seams themselves, called directly (Economy.cpp: m_attributeSums(obj, COMMAND_POINT_BONUS, sum))
                ("m_attributeModifiers", 3): 1, ("m_attributeSums", 3): 1,
                # the pool's own queries and the list's value query (AttributeModifierPool.cpp: list->value(ATTRIBUTE_HEALTH, ...) = RW 0x6144BF,
                # product(ATTRIBUTE_HEALTH_MULT, ...) = RW 0x804FFF in the apply on add RW 0x8052FB); their names are common words, so a call
                # of them counts only with a named attribute constant (STRICT)
                ("value", 3): 0, ("sum", 3): 0, ("product", 4): 0}
STRICT = {"value", "sum", "product"}
CONST = re.compile(r"\b(?:constexpr|const)\s+(?:int|unsigned|std::uint32_t|std::int32_t|UnsignedInt|Int)\s+(\w+)\s*=\s*(0x[0-9A-Fa-f]+|\d+)\s*;")
LAMBDA = re.compile(r"\]\s*\(([^()]*)\)\s*(?:->\s*\w+\s*)?\{[^{}]*$")


def split_args(text: str, at: int):
    """The top-level arguments of the call whose '(' is at `at` ([] if unbalanced)."""
    depth, cur, args = 0, [], []
    for c in text[at:]:
        if c in "([{":
            depth += 1
            if depth == 1:
                continue
        elif c in ")]}":
            depth -= 1
            if depth == 0:
                args.append("".join(cur).strip())
                return [a for a in args if a]
        elif c == "," and depth == 1:
            args.append("".join(cur).strip())
            cur = []
            continue
        cur.append(c)
    return []


def param_names(params: str) -> list[str]:
    out = []
    for p in params.split(","):
        p = p.split("=")[0].strip()
        m = re.search(r"(\w+)\s*(\[\])?$", p)
        out.append(m.group(1) if m else "")
    return out


@dataclass
class PortRead:
    file: str
    line: int
    call: str
    attribute: int | None
    arg: str
    function: str


def port_scan(src: Path):
    files = {}
    for p in sorted(src.rglob("*")):
        if p.suffix in (".cpp", ".h", ".hpp", ".inl", ".inc") and p.is_file():
            raw = p.read_text(encoding="utf-8", errors="replace")
            files[str(p.relative_to(src.parent)).replace("\\", "/")] = (raw, code_only(raw))
    enums = {}
    for raw, code in files.values():
        for name, val in ENUM.findall(code) + CONST.findall(code):
            enums.setdefault(name, int(val, 0))
    cites = {f: {int(a, 16) for a in CITE.findall(raw)} for f, (raw, _c) in files.items()}

    def functions(code):
        """(name, params, body start, body end) of each definition."""
        out = []
        for m in DEF.finditer(code):
            name = m.group(2)
            if name in ("if", "for", "while", "switch", "catch", "return"):
                continue
            depth, i = 0, m.end() - 1
            while i < len(code):
                if code[i] == "{":
                    depth += 1
                elif code[i] == "}":
                    depth -= 1
                    if depth == 0:
                        break
                i += 1
            out.append((name, param_names(m.group(3)), m.end(), i))
        return out

    defs = {f: functions(code) for f, (_r, code) in files.items()}
    queries = dict(BASE_QUERIES)
    changed = True
    while changed:  # a port function passing its own parameter on as the attribute is a wrapper (fixed point)
        changed = False
        for f, (_raw, code) in files.items():
            for name, params, b0, b1 in defs[f]:
                key = (name, len([p for p in params if p]))
                if key in queries:
                    continue
                body = code[b0:b1]
                for (q, argc), k in list(queries.items()):
                    for m in re.finditer(rf"\b{q}\s*\(", body):
                        args = split_args(body, m.end() - 1)
                        if len(args) == argc and args[k] and args[k] in params:
                            queries[key] = params.index(args[k])
                            changed = True
                            break
                    if key in queries:
                        break
    reads: list[PortRead] = []
    for f, (_raw, code) in files.items():
        line_at = [0]
        for i, c in enumerate(code):
            if c == "\n":
                line_at.append(i + 1)
        import bisect
        for (q, argc), k in queries.items():
            for m in re.finditer(rf"\b{q}\s*\(", code):
                # a definition or declaration of the query itself is not a read
                before = code[max(0, m.start() - 40):m.start()]
                if re.search(r"(bool|float|void|double|int)\s+(\w+::)?$", before):
                    continue
                args = split_args(code, m.end() - 1)
                if len(args) != argc:
                    continue
                a = args[k]
                owner, own_params = next(((n, p) for n, p, b0, b1 in defs[f] if b0 <= m.start() <= b1), ("", []))
                ln = bisect.bisect_right(line_at, m.start())
                if a in own_params and (owner, len([p for p in own_params if p])) in queries:
                    continue  # the wrapper's own pass-through: its callers carry the attribute
                lam = LAMBDA.search(code[max(0, m.start() - 300):m.start()])
                if lam and a in param_names(lam.group(1)):
                    continue  # a lambda seam's parameter (GameLogic.cpp's Economy providers): the seam's callers carry the attribute
                # a conditional picks one of two constants: both are read
                arms = re.fullmatch(r".*\?\s*([\w:x]+)\s*:\s*([\w:x]+)\s*", a)
                for arg in ([arms.group(1), arms.group(2)] if arms else [a]):
                    val, named = None, False
                    if re.fullmatch(r"(0x[0-9A-Fa-f]+|\d+)u?", arg):
                        val = int(arg.rstrip("u"), 0)
                    else:
                        tok = re.sub(r"^\(?\s*(?:\(\s*int\s*\)|static_cast<int>\()?\s*", "", arg).rstrip(")").split("::")[-1].strip()
                        if tok in enums:
                            val, named = enums[tok], True
                    if q in STRICT and not named:
                        continue  # a common name (value / sum / product) counts only with a named attribute constant
                    reads.append(PortRead(f, ln, q, val, a, owner))
    # through calls (owner 2026-11-19: TerrainResourceBehavior.cpp reads PRODUCTION through Economy::productionMoneyProduct): a port function
    # reads the attributes read in its own body; a file reads what its code reads directly and through the port functions it calls that read
    # them directly (by name). Common short names are not followed (a name clash would invent a read).
    fn_attrs: dict[str, set[int]] = defaultdict(set)
    for r in reads:
        if r.attribute is not None and r.function:
            fn_attrs[r.function].add(r.attribute)
    # ONE level only: a name-based transitive closure spreads through common helpers until every file "reads" every attribute
    followable = lambda n: len(n) >= 8 and n not in NOT_FOLLOWED
    called = re.compile(r"\b(\w+)\s*\(")
    via_calls: dict[str, set[int]] = defaultdict(set)
    for f, (_raw, code) in files.items():
        for c in set(called.findall(code)):
            if followable(c) and c in fn_attrs:
                via_calls[f] |= fn_attrs[c]
    return reads, cites, queries, via_calls


# ---- the mod ------------------------------------------------------------------------------------------------------------------------------

@dataclass
class ModList:
    name: str
    src: str
    modifiers: list[tuple[str, str]] = field(default_factory=list)   # (ATTRIBUTE, rest of the line)
    fields: dict[str, list[str]] = field(default_factory=dict)
    refs: list[tuple[str, str, str, str]] = field(default_factory=list)  # (type, name, module class, field)
    factions: set[str] = field(default_factory=set)


# the fields that name a ModifierList (every field of the mod that names one: AttributeModifier(s), Hero / Trigger / ToggleOn / Damaged /
# ReallyDamaged / Deployed AttributeModifier, ModifierName, BonusName); a list name in any other field is another thing of the same name
MOD_FIELD = re.compile(r"AttributeModifier|^ModifierName$|^BonusName$", re.I)


def mod_side(base: Path, overlay: Path, src_dir: Path | None = None, rw_tables: Path | None = None):
    import ennorath_graph as eg
    eg.STORED_TYPES = set(eg.STORED_TYPES) | {"experiencelevel", "crate", "armor"}
    vfs = eg.mount(base, overlay)
    db = eg.load_db(vfs)
    tag_class = {}  # (object, module tag) -> module class, for the special power gates of ennorath_graph.walk
    stance_users: dict[str, set[str]] = defaultdict(set)  # stance template -> the objects that name it
    for (typ, name), d in db.defs.items():
        if typ == "object":
            for k, v, _m in d["fields"]:
                vt = v.split()
                if len(vt) > 1 and vt[-1].lower().startswith("moduletag_"):
                    tag_class[(name, vt[-1])] = vt[0]
                if k.rsplit("/", 1)[-1].lower() == "stancetemplate" and vt:
                    stance_users[vt[0].lower()].add(name.lower())
    lists: dict[str, ModList] = {}
    for (typ, name), d in db.defs.items():
        if typ != "modifierlist":
            continue
        ml = ModList(name, d["src"])
        for k, v, _m in d["fields"]:
            if k.lower() == "modifier":
                toks = v.split(None, 1)
                ml.modifiers.append((toks[0] if toks else "", toks[1] if len(toks) > 1 else ""))
            else:
                ml.fields.setdefault(k, []).append(v)
        lists[name.lower()] = ml
    # the references, with the module class from RotWK's grammar (field_audit.mod_fields: the ennorath_graph parse cannot trust End counts
    # inside draw modules, so its module tags misplace some fields)
    import field_audit as fa
    here = Path(__file__).resolve().parents[2]
    src = fa.Source(src_dir or here / "engine" / "src")
    every, real = src.block_keywords()
    refs_of: dict[tuple[str, str], list[tuple[ModList, str, str]]] = defaultdict(list)  # definition -> (list, module class, field)

    def sink(typ, owner, key, defname, value):
        if typ == "modifierlist" or not MOD_FIELD.search(key.rsplit("/", 1)[-1]):
            return  # a name in another field (a particle system, a sound, a power of the same name) is not a list reference
        for tok in re.split(r"[\s,]+", value):
            ml = lists.get(tok.lower())
            if ml:
                cls = owner.split("/", 1)[0]
                ml.refs.append((typ, defname, cls, key))
                refs_of[(typ, defname)].append((ml, cls, key))

    fa.mod_fields(vfs, every, {}, fa.Grammar(rw_tables or here / "engine" / "data" / "rotwk-201"), src.line_rows(every & real), sink=sink)
    factions = [n for (t, n), d in db.defs.items() if t == "playertemplate"
                and any(k.lower() == "playableside" and v.strip().lower() in ("yes", "true") for k, v, _m in d["fields"])]
    for fac in sorted(factions):
        reached, _order, _unres, modules_of_power = eg.walk(db, fac)
        # modules that run only once their special power is reached: (object, module class) -> their tags
        gated: dict[tuple[str, str], list[str]] = defaultdict(list)
        for lst in modules_of_power.values():
            for o, m in lst:
                gated[(o, tag_class.get((o, m), ""))].append(m)
        objs = {n.lower() for (t, n) in reached if t == "object"}
        for (typ, name), lst in refs_of.items():
            for ml, cls, _k in lst:
                if typ == "object":
                    tags = gated.get((name, cls), [])
                    ok = ("object", name) in reached and (not tags or any(("module", f"{name}/{t}") in reached for t in tags))
                elif typ == "stancetemplate":  # reached through an object that names it (StancesBehavior's StanceTemplate)
                    ok = bool(stance_users.get(name.lower(), set()) & objs)
                elif typ == "experiencelevel":  # reached through its TargetNames
                    d = db.defs[(typ, name)]
                    ok = bool({x.lower() for k, v, _m in d["fields"] if k.lower() == "targetnames" for x in v.split()} & objs)
                else:
                    ok = (typ, name) in reached
                if ok:
                    ml.factions.add(fac)
    return lists, sorted(factions)


# ---- the join -----------------------------------------------------------------------------------------------------------------------------

def reader_status(readers, cites, reads, inv: cc.Inventory, via_calls: dict[str, set[int]] | None = None):
    """reader -> (status, files): the files that cite an address inside the reader's function, and whether one of them reads the attribute."""
    owner_cache: dict[int, int | None] = {}
    files_of: dict[int, set[str]] = defaultdict(set)
    for f, addrs in cites.items():
        for a in addrs:
            if a not in owner_cache:
                owner_cache[a] = inv.owner(a)
            fn = owner_cache[a]
            if fn is not None:
                files_of[fn].add(f)
    reads_in = defaultdict(set)  # file -> attributes read there
    for r in reads:
        if r.attribute is not None:
            reads_in[r.file].add(r.attribute)
    out = {}
    for rd in readers:
        files = files_of.get(rd.function, set())
        if not files:
            out[id(rd)] = ("not-cited", set())
        elif any(rd.attribute in reads_in[f] for f in files):
            out[id(rd)] = ("ported", files)
        elif via_calls and any(rd.attribute in via_calls.get(f, ()) for f in files):
            out[id(rd)] = ("ported-via-call", files)  # the citing file calls a port function that reads the attribute
        else:
            out[id(rd)] = ("cited-no-read", files)
    return out


def verdict(mod_uses: int, rw: int, ported: int, port_reads: int, known: bool) -> str:
    if not known:
        return "unknown to RotWK"
    if rw == 0:
        return "dead in RotWK" + (" (port-only reads)" if port_reads else "")
    if port_reads == 0:
        return "not read by the port"
    if ported < rw:
        return "partly ported"
    return "ported"


def write(out: Path, names, lists, factions, readers, unresolved, wrappers, reads, status, fn_class, queries):
    out.mkdir(parents=True, exist_ok=True)
    by_name = {n.upper(): i for i, n in enumerate(names)}
    with (out / "modifier-lists.tsv").open("w", encoding="utf-8", newline="") as s:
        w = csv.writer(s, delimiter="\t", lineterminator="\n")
        w.writerow(["list", "attribute", "number", "value", "category", "duration", "factions", "references", "source"])
        for ml in sorted(lists.values(), key=lambda m: m.name.lower()):
            for attr, val in ml.modifiers or [("", "")]:
                w.writerow([ml.name, attr, by_name.get(attr.upper(), ""), val, " ".join(ml.fields.get("Category", [])),
                            " ".join(ml.fields.get("Duration", [])), " ".join(sorted(ml.factions)),
                            "; ".join(f"{t}:{n}{'/' + c if c else ''}.{k}" for t, n, c, k in ml.refs[:12]) + (" ..." if len(ml.refs) > 12 else ""),
                            ml.src])
    with (out / "modifier-readers.tsv").open("w", encoding="utf-8", newline="") as s:
        w = csv.writer(s, delimiter="\t", lineterminator="\n")
        w.writerow(["attribute", "number", "kind", "call", "function", "reached from (RW_FUNCTION_MAP module)", "via", "status", "port files"])
        for rd in sorted(readers, key=lambda r: (r.attribute, r.site)):
            st, files = status[id(rd)]
            w.writerow([names[rd.attribute] if rd.attribute < len(names) else f"#{rd.attribute}", rd.attribute, rd.kind, f"RW 0x{rd.site:06X}",
                        f"RW 0x{rd.function:06X}", fn_class.get(rd.function, ""), " ".join(f"0x{v:06X}" for v in rd.via), st, " ".join(sorted(files))])
    with (out / "modifier-port-reads.tsv").open("w", encoding="utf-8", newline="") as s:
        w = csv.writer(s, delimiter="\t", lineterminator="\n")
        w.writerow(["attribute", "number", "file", "line", "call", "argument", "in function"])
        for r in sorted(reads, key=lambda r: (r.attribute if r.attribute is not None else 99, r.file, r.line)):
            a = r.attribute
            w.writerow([names[a] if a is not None and a < len(names) else ("?" if a is None else f"#{a}"), "" if a is None else a,
                        r.file, r.line, r.call, r.arg, r.function])
    with (out / "modifier-unresolved.tsv").open("w", encoding="utf-8", newline="") as s:
        w = csv.writer(s, delimiter="\t", lineterminator="\n")
        w.writerow(["side", "where", "function", "target", "why"])
        for u in unresolved:
            w.writerow(["RotWK", f"RW 0x{u.site:06X}", "" if u.function is None else f"RW 0x{u.function:06X}", f"RW 0x{u.target:06X}", u.why])
        for r in reads:
            if r.attribute is None:
                w.writerow(["port", f"{r.file}:{r.line}", r.function, r.call, f"argument `{r.arg}`"])

    # the summary per attribute
    mod_by_attr = defaultdict(list)
    for ml in lists.values():
        for attr, _v in ml.modifiers:
            mod_by_attr[attr.upper()].append(ml)
    rows = []
    attrs = sorted(set(by_name) | set(mod_by_attr), key=lambda a: by_name.get(a, 999))
    for a in attrs:
        num = by_name.get(a)
        mls = mod_by_attr.get(a, [])
        reached = [m for m in mls if m.factions]
        rds = [r for r in readers if r.attribute == num] if num is not None else []
        ported = sum(1 for r in rds if status[id(r)][0] in ("ported", "ported-via-call"))
        prd = [r for r in reads if r.attribute == num] if num is not None else []
        rows.append((a, num, len(mls), len(reached), len(rds), ported, sum(1 for r in rds if status[id(r)][0] == "cited-no-read"), len(prd),
                     verdict(len(mls), len(rds), ported, len(prd), num is not None)))
    md = ["# Attribute modifier map", "",
          "Generated by `tools/mod/modifier_map.py` (see its docstring for the method). Lists: ModifierList definitions of RotWK 2.01 + Ennorath; "
          f"reached: a playable faction reaches the list ({', '.join(factions)}). RotWK readers: calls of the pool's sum / product "
          f"(RW 0x804F39 / 0x804FFF) and their wrappers ({len(wrappers)} wrappers) with a constant attribute; {len(unresolved)} RotWK calls "
          "are unresolved (modifier-unresolved.tsv).", "",
          "| attribute | # | mod lists | reached | RotWK readers | ported | cited, no read | port reads | verdict |",
          "|---|---|---|---|---|---|---|---|---|"]
    for a, num, n, rch, rw, pt, cnr, pr, v in rows:
        if n == 0 and rw == 0 and pr == 0:
            continue
        md.append(f"| {a} | {'' if num is None else num} | {n} | {rch} | {rw} | {pt} | {cnr} | {pr} | {v} |")
    md += ["", "## RotWK readers not ported", ""]
    for rd in sorted(readers, key=lambda r: (r.attribute, r.site)):
        st, files = status[id(rd)]
        if st in ("ported", "ported-via-call"):
            continue
        md.append(f"- {names[rd.attribute] if rd.attribute < len(names) else rd.attribute}: call RW 0x{rd.site:06X} in RW 0x{rd.function:06X}"
                  f"{' (reached from ' + fn_class[rd.function] + ')' if fn_class.get(rd.function) else ''}: {st}"
                  f"{' (cited in ' + ', '.join(sorted(files)) + ')' if files else ''}")
    md += ["", "## The port's attribute queries", "", "Base queries and the port functions found to pass the attribute on (argument index): "
           + ", ".join(f"`{q}`/{n} ({k})" for (q, n), k in sorted(queries.items())) + "."]
    (out / "modifier-map.md").write_text("\n".join(md) + "\n", encoding="utf-8")
    return rows


def matrix_link(out: Path, lists: dict, rows, gaps_path: Path) -> int:
    """modifier-matrix.tsv (owner 2026-11-19: the maps linked): every red button of MATRIX_GAPS.tsv with the ModifierLists its module names
    (the object's module of that class: refs from mod_side), their attributes and each attribute's verdict here. A button whose list changes an
    attribute the port does not read where RotWK does (`not read by the port` / `partly ported`) shows why it "does nothing"."""
    if not gaps_path.exists():
        return 0
    verdicts = {a: v for a, _n, _m, _r, _rw, _pt, _cnr, _pr, v in rows}
    by_module: dict[tuple[str, str], list] = defaultdict(list)
    for ml in lists.values():
        for typ, name, cls, _k in ml.refs:
            if typ == "object":
                by_module[(name.lower(), cls)].append(ml)
    with gaps_path.open(encoding="utf-8", newline="") as s:
        gaps = list(csv.DictReader([l for l in s if not l.startswith("#")], delimiter="\t", quoting=csv.QUOTE_NONE))
    n = 0
    with (out / "modifier-matrix.tsv").open("w", encoding="utf-8", newline="") as s:
        w = csv.writer(s, delimiter="\t", lineterminator="\n")
        w.writerow(["faction", "object", "button", "result", "module class", "modifier lists", "attributes", "attributes the port does not read"])
        for g in gaps:
            mls = by_module.get(((g.get("module object") or "").lower(), g.get("module class") or ""), [])
            if not mls:
                continue
            attrs = sorted({a.upper() for ml in mls for a, _v in ml.modifiers if a})
            unread = [a for a in attrs if verdicts.get(a) in ("not read by the port", "partly ported")]
            w.writerow([g.get("faction"), g.get("object"), g.get("button"), g.get("result"), g.get("module class"),
                        " ".join(sorted({ml.name for ml in mls})), " ".join(attrs), " ".join(unread)])
            n += 1
    return n


def function_classes(path: Path) -> dict[int, str]:
    out = {}
    if path.exists():
        with path.open(encoding="utf-8") as s:
            for r in csv.DictReader(s, delimiter="\t"):
                try:
                    out.setdefault(int(r["function"], 16), r["module"])
                except (KeyError, ValueError):
                    pass
    return out


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    here = Path(__file__).resolve().parents[2]
    ap.add_argument("--base", required=True, type=Path, help="the RotWK 2.01 install")
    ap.add_argument("--overlay", required=True, type=Path, help="the Ennorath install")
    ap.add_argument("--game", required=True, type=Path, help="RotWK 2.01 game.dat")
    ap.add_argument("--out", required=True, type=Path)
    ap.add_argument("--src", type=Path, default=here / "engine" / "src")
    ap.add_argument("--inventory", type=Path, help="rw_function_blocks.json (default: citation_check.find_inventory)")
    ap.add_argument("--function-map", type=Path, default=here / "docs" / "ennorath" / "RW_FUNCTION_MAP.tsv")
    ap.add_argument("--matrix-gaps", type=Path, default=here / "docs" / "ennorath" / "MATRIX_GAPS.tsv",
                    help="the button matrix's red buttons, linked to their modules' modifier lists (modifier-matrix.tsv)")
    a = ap.parse_args(argv)
    full = cc.DRIVE_COPY.with_name("rw_function_blocks_full.json")  # the whole binary (the maps run's angr pass); the default one holds the
    inv_path = a.inventory or (full if full.exists() else cc.find_inventory(here))  # module code only and misses most readers
    if not inv_path or not inv_path.exists():
        raise SystemExit("modifier_map: no RotWK function inventory (rw_function_blocks.json): pass --inventory")
    functions = json.loads(inv_path.read_text(encoding="utf-8"))["functions"]
    inv = cc.Inventory(functions)
    blocks = {f["address"]: [tuple(b) for b in f["blocks"]] for f in functions}
    img = Image(a.game)
    names = attribute_names(img)
    readers, unresolved, wrappers = rotwk_readers(img, inv, blocks)
    reads, cites, queries, via_calls = port_scan(a.src)
    lists, factions = mod_side(a.base, a.overlay, a.src)
    status = reader_status(readers, cites, reads, inv, via_calls)
    rows = write(a.out, names, lists, factions, readers, unresolved, wrappers, reads, status, function_classes(a.function_map), queries)
    linked = matrix_link(a.out, lists, rows, a.matrix_gaps)
    print(f"{len(lists)} modifier lists, {sum(len(m.modifiers) for m in lists.values())} modifiers; {len(readers)} RotWK readers in "
          f"{len({r.function for r in readers})} functions ({len(wrappers)} wrappers, {len(unresolved)} unresolved); {len(reads)} port reads")
    for a_, num, n, rch, rw, pt, cnr, pr, v in rows:
        if n or rw or pr:
            print(f"  {a_:<26} lists {n:>4} (reached {rch:>4})  RotWK {rw:>3}  ported {pt:>3}  cited-no-read {cnr:>2}  port reads {pr:>3}  {v}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
