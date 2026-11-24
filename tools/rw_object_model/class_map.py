"""Exact class model of RotWK game.dat over the whole-image function inventory (binary_map.py).

Run: python tools/rw_object_model/class_map.py facts      (per-function dataflow facts, cached)
     python tools/rw_object_model/class_map.py report     (vtables, classes, typed globals / fields, virtual calls, readers)

Value model (per function, registers only, forward dataflow over the function's own blocks):
  ('this',)          ecx at entry
  ('imm', v)         a constant
  ('ld', B, d)       the dword at [B + d]; B is a value or None for an absolute address d
  ('add', B, k)      B + k
  ('ret', f)         eax after a direct call of f
A join of two different values is unknown (None): nothing is guessed. Callee-saved registers (ebx esi edi ebp) survive a
call, eax / ecx / edx do not. A switch target starts with every register unknown.

Facts recorded per function: vtable installs (mov [R + d], V), stores of a value into [R + d] or a global, direct calls
with the value of ecx, virtual calls `call [R + d]` with the expression of R, and every load with a known base.
"""
from __future__ import annotations

import json
import pickle
import sys
from collections import defaultdict
from pathlib import Path

from capstone.x86 import X86_OP_IMM, X86_OP_MEM, X86_OP_REG

sys.path.insert(0, str(Path(__file__).resolve().parent))
import binary_map as bm  # noqa: E402

FACTS_CACHE = Path("workspace/rebuild/rw_binary_facts.pickle")
REGS = ("eax", "ecx", "edx", "ebx", "esi", "edi", "ebp")
VOLATILE = ("eax", "ecx", "edx")
SUB = {"ax": "eax", "al": "eax", "ah": "eax", "cx": "ecx", "cl": "ecx", "ch": "ecx", "dx": "edx", "dl": "edx", "dh": "edx",
       "bx": "ebx", "bl": "ebx", "bh": "ebx", "si": "esi", "di": "edi", "bp": "ebp"}
MAX_DEPTH = 6


def depth(v):
    d = 0
    while isinstance(v, tuple) and len(v) == 3 and v[0] in ("ld", "add"):
        v = v[1]
        d += 1
    return d


def add(v, k):
    if v is None:
        return None
    if v[0] == "sp":
        return ("sp", v[1] + k)
    if v[0] == "imm":
        return ("imm", (v[1] + k) & 0xFFFFFFFF)
    if v[0] == "add":
        k += v[2]
        v = v[1]
    return v if k == 0 else ("add", v, k)


def load(base, disp):
    v = ("ld", base, disp & 0xFFFFFFFF)
    return v if depth(v) <= MAX_DEPTH else None


NULL = ("imm", 0)


def join(a, b):
    """Join of two values at a merge: equal -> kept; null and X -> X (a pointer that may be null: null has no type and no
    fields, so every fact about X still holds whenever the pointer is used); anything else -> unknown."""
    if a == b:
        return a
    if a == NULL and isinstance(b, tuple):
        return b
    if b == NULL and isinstance(a, tuple):
        return a
    return None


def split(v):
    """(base, offset) of an address value: ('add', B, k) -> (B, k), else (v, 0)."""
    if v is not None and v[0] == "add":
        return v[1], v[2]
    return v, 0


def full(reg):
    return SUB.get(reg, reg)


def reg_value(st, reg):
    """Value of a full register; esp is an ('sp', offset) address value."""
    if reg == "esp":
        return ("sp", st["esp"]) if st.get("esp") is not None else None
    v = st.get(reg)
    return v if isinstance(v, tuple) else None


def blocks_of(b: bm.Binary, spans):
    """Decoded instructions of the function, grouped into basic blocks: {leader: [insn]} and successors."""
    insns = {}
    for start, size in spans:
        a = start
        while a < start + size:
            i = b.insn(a)
            insns[a] = i
            a += i.size
    leaders = {spans[0][0]} if spans else set()
    switch_targets = set()
    for a, i in insns.items():
        mn = i.mnemonic
        if mn == "jmp" or mn.startswith("j") or mn.startswith("loop") or mn in bm.STOP_MNEMONICS or mn == "call":
            nxt = a + i.size
            if nxt in insns and mn != "call":
                leaders.add(nxt)
            if mn != "call" and i.operands and i.operands[0].type == X86_OP_IMM and i.operands[0].imm in insns:
                leaders.add(i.operands[0].imm)
    # switch targets: owned addresses reached by an indirect jump; they start blocks with unknown state
    for a, i in insns.items():
        if i.mnemonic == "jmp" and i.operands[0].type == X86_OP_MEM:
            m = i.operands[0].mem
            if m.index and m.scale == 4 and not m.base:
                k = 0
                while True:
                    try:
                        t = b.u32(m.disp + 4 * k)
                    except Exception:
                        break
                    if t not in insns:
                        break
                    switch_targets.add(t)
                    leaders.add(t)
                    k += 1
    order = sorted(insns)
    blocks, succ = {}, {}
    cur = None
    for a in order:
        if a in leaders or cur is None:
            cur = a
            blocks[cur] = []
        blocks[cur].append(insns[a])
    for lead, body in blocks.items():
        last = body[-1]
        mn = last.mnemonic
        s = []
        nxt = last.address + last.size
        if mn in bm.STOP_MNEMONICS:
            pass
        elif mn == "jmp":
            if last.operands[0].type == X86_OP_IMM and last.operands[0].imm in blocks:
                s.append(last.operands[0].imm)
        elif mn.startswith("j") or mn.startswith("loop"):
            if last.operands[0].type == X86_OP_IMM and last.operands[0].imm in blocks:
                s.append(last.operands[0].imm)
            if nxt in blocks:
                s.append(nxt)
        elif nxt in blocks:
            s.append(nxt)
        succ[lead] = s
    return blocks, succ, switch_targets, spans[0][0] if spans else None


def mem_addr(i, st, m):
    """Address value of a memory operand, or None (an index register makes it unknown)."""
    if m.index:
        return None
    if m.segment:
        return None
    if not m.base:
        return ("imm", m.disp & 0xFFFFFFFF)
    r = i.reg_name(m.base)
    if r == "esp":
        return ("sp", st["esp"] + m.disp) if st.get("esp") is not None else None
    return add(reg_value(st, full(r)), m.disp)


def _load_value(st, addr):
    """Value read through an address value: a stack slot written on this path, an incoming argument, or ('ld', ...)."""
    if addr is None:
        return None
    if addr[0] == "sp":
        off = addr[1]
        if off in st["stk"]:
            return st["stk"][off]
        if off >= 4 and off % 4 == 0:
            return ("arg", (off - 4) // 4)  # [esp_entry + 4 + 4n]: argument n (the slot is not written on this path)
        return None
    base, k = split(addr)
    return load(None, base[1] + k) if base[0] == "imm" else load(base, k)


SEH_PROLOG_SHAPE = ["push -1", "push eax", "mov eax, dword ptr fs:[0]", "push eax", "mov eax, dword ptr [esp + 0xc]",
                    "mov dword ptr fs:[0], esp", "mov dword ptr [esp + 0xc], ebp", "lea ebp, [esp + 0xc]", "push eax", "ret "]


def seh_prologs(b, rec):
    """Functions whose body is exactly MSVC's _EH_prolog: after `call` from a caller at esp S they leave ebp = S - 4 (the
    caller's own frame: its arguments at [ebp + 8]) and esp = S - 16, and keep ecx edx ebx esi edi."""
    out = set()
    for fn in rec["functions"]:
        if sum(s for _, s in fn["spans"]) != 31:
            continue
        a, shape = fn["address"], []
        for _ in range(len(SEH_PROLOG_SHAPE)):
            i = b.insn(a)
            shape.append(f"{i.mnemonic} {i.op_str}")
            a += i.size
        if shape == SEH_PROLOG_SHAPE:
            out.add(fn["address"])
    return out


def _after_call(st, pops):
    """eax / ecx / edx die; the callee pops its `ret N` bytes (one known N), else esp and the frame below it are unknown."""
    sp = st.get("esp")
    if sp is None:
        return
    if pops is None or len(pops) != 1:
        st["esp"] = None
        return
    n = next(iter(pops))
    st["esp"] = sp + n


def facts_of(b: bm.Binary, f: int, spans, pops, seh=frozenset()):
    blocks, succ, switch_targets, entry = blocks_of(b, spans)
    preds = defaultdict(list)
    for a, ss in succ.items():
        for s in ss:
            preds[s].append(a)
    init = {r: None for r in REGS}
    init["ecx"] = ("this",)
    init["esp"] = 0  # relative to esp at entry (which points at the return address)
    init["stk"] = {}
    state_in = {entry: init}
    out_state = {}
    work = [entry]
    facts = dict(vt=[], st=[], calls=[], vcalls=[], loads=[], args={}, rets=[])

    def call_args(st):
        """The values of [esp], [esp + 4], ... at a call: the callee's arguments 0..5 (None where unknown)."""
        sp = st.get("esp")
        if sp is None:
            return ()
        out = tuple(st["stk"].get(sp + 4 * k) for k in range(6))
        return out if any(x is not None for x in out) else ()
    final = {}

    def run(lead, st, record):
        st = dict(st)
        for i in blocks[lead]:
            mn = i.mnemonic
            ops = i.operands
            if record and mn == "ret":
                facts["rets"].append(st.get("eax"))
            if record:
                for op in ops:
                    if op.type == X86_OP_MEM and mn != "lea":
                        addr = mem_addr(i, st, op.mem)
                        if addr is not None and addr[0] != "sp" and (addr[0] != "imm" or b.in_data(addr[1])):
                            base, k = split(addr)
                            write_only = (op is ops[0] and mn in ("mov", "movss", "movsd", "fstp", "fst", "fistp", "fist", "movq", "movd", "movaps", "movups", "setne", "sete"))
                            facts["loads"].append((i.address, base, k, op.size, bool(write_only)))
            if mn == "call":
                op = ops[0]
                if op.type == X86_OP_IMM and op.imm in seh:
                    sp = st.get("esp")
                    st["eax"] = None
                    st["ebp"] = ("sp", sp - 4) if sp is not None else None
                    st["esp"] = sp - 16 if sp is not None else None
                    continue
                if op.type == X86_OP_IMM:
                    if record:
                        facts["calls"].append((i.address, op.imm, st.get("ecx")))
                        a = call_args(st)
                        if a:
                            facts["args"][i.address] = a
                    for r in VOLATILE:
                        st[r] = None
                    st["eax"] = ("ret", op.imm, i.address)
                    _after_call(st, pops.get(op.imm))
                else:
                    if record and op.type == X86_OP_MEM and not op.mem.base and not op.mem.index:
                        facts.setdefault("icalls", []).append((i.address, op.mem.disp & 0xFFFFFFFF))  # through the IAT or a global pointer
                    elif record and op.type == X86_OP_MEM:
                        a = call_args(st)
                        if a:
                            facts["args"][i.address] = a
                        facts.setdefault("vecx", {})[i.address] = st.get("ecx")
                        facts["vcalls"].append((i.address, mem_addr(i, st, op.mem) if not op.mem.index else None,
                                                reg_value(st, full(i.reg_name(op.mem.base))) if op.mem.base else None, op.mem.disp & 0xFFFFFFFF))
                    elif record and op.type == X86_OP_REG:
                        facts["vcalls"].append((i.address, None, reg_value(st, full(i.reg_name(op.reg))), None))
                    for r in VOLATILE:
                        st[r] = None
                    st["eax"] = ("vret", i.address)
                    _after_call(st, None)
                continue
            if mn == "push" and ops:
                src = ops[0]
                val = ("imm", src.imm & 0xFFFFFFFF) if src.type == X86_OP_IMM else reg_value(st, full(i.reg_name(src.reg))) if src.type == X86_OP_REG and src.size == 4 else None
                if src.type == X86_OP_MEM and src.size == 4:
                    addr = mem_addr(i, st, src.mem)
                    val = _load_value(st, addr)
                sp = st.get("esp")
                if sp is not None:
                    sp = sp - 4
                    st["esp"] = sp
                    st["stk"] = dict(st["stk"])
                    st["stk"][sp] = val
                continue
            if mn == "pop" and ops:
                sp = st.get("esp")
                val = st["stk"].get(sp) if sp is not None else None
                if ops[0].type == X86_OP_REG:
                    r = full(i.reg_name(ops[0].reg))
                    if r in st:
                        st[r] = val
                if sp is not None:
                    st["esp"] = sp + 4
                continue
            if mn in ("leave",):
                eb = st.get("ebp")
                st["esp"] = eb[1] + 4 if eb is not None and eb[0] == "sp" else None
                st["ebp"] = st["stk"].get(eb[1]) if eb is not None and eb[0] == "sp" else None
                continue
            if not ops:
                if mn in ("cdq",):
                    st["edx"] = None
                elif mn in ("pushal", "popal", "pushad", "popad"):
                    for r in REGS:
                        st[r] = None
                continue
            dst = ops[0]
            if mn == "mov" and len(ops) == 2 and dst.type == X86_OP_MEM:
                addr = mem_addr(i, st, dst.mem)
                src = ops[1]
                val = ("imm", src.imm & 0xFFFFFFFF) if src.type == X86_OP_IMM else reg_value(st, full(i.reg_name(src.reg))) if src.type == X86_OP_REG else None
                if addr is not None and addr[0] == "sp" and dst.size == 4:
                    st["stk"] = dict(st["stk"])
                    st["stk"][addr[1]] = val
                    continue
                if addr is not None and addr[0] == "sp":
                    continue
                if record and addr is not None and dst.size == 4:
                    base, k = split(addr)
                    if val is not None and val[0] == "imm" and b.in_data(val[1]) and b.in_text(b.u32(val[1])):
                        facts["vt"].append((i.address, base, k, val[1]))
                    facts["st"].append((i.address, base, k, val))
                continue
            if dst.type != X86_OP_REG:
                continue
            r = full(i.reg_name(dst.reg))
            if r == "esp":
                sp = st.get("esp")
                if mn in ("add", "sub") and len(ops) == 2 and ops[1].type == X86_OP_IMM and sp is not None:
                    st["esp"] = sp + (ops[1].imm if mn == "add" else -ops[1].imm)
                elif mn == "and":
                    st["esp"] = None  # stack alignment: the frame is reached through ebp from here
                elif mn == "mov" and len(ops) == 2 and ops[1].type == X86_OP_REG:
                    v = st.get(full(i.reg_name(ops[1].reg)))
                    st["esp"] = v[1] if v is not None and v[0] == "sp" else None
                elif mn == "lea" and ops[1].type == X86_OP_MEM:
                    v = mem_addr(i, st, ops[1].mem)
                    st["esp"] = v[1] if v is not None and v[0] == "sp" else None
                else:
                    st["esp"] = None
                continue
            if r not in st:
                continue
            if dst.size != 4 and mn not in ("movzx", "movsx"):
                st[r] = None
                continue
            if mn == "mov" and len(ops) == 2:
                src = ops[1]
                if src.type == X86_OP_IMM:
                    st[r] = ("imm", src.imm & 0xFFFFFFFF)
                elif src.type == X86_OP_REG:
                    sr = full(i.reg_name(src.reg))
                    st[r] = (("sp", st["esp"]) if st.get("esp") is not None else None) if sr == "esp" else st.get(sr) if src.size == 4 else None
                elif src.type == X86_OP_MEM and src.size == 4:
                    st[r] = _load_value(st, mem_addr(i, st, src.mem))
                else:
                    st[r] = None
            elif mn == "lea" and ops[1].type == X86_OP_MEM:
                st[r] = mem_addr(i, st, ops[1].mem)
            elif mn in ("add", "sub") and len(ops) == 2 and ops[1].type == X86_OP_IMM:
                k = ops[1].imm if mn == "add" else -ops[1].imm
                st[r] = add(st[r], k)
            elif mn in ("xor", "sub") and len(ops) == 2 and ops[1].type == X86_OP_REG and full(i.reg_name(ops[1].reg)) == r:
                st[r] = ("imm", 0)
            elif mn in ("cmp", "test", "push"):
                pass
            elif mn == "xchg" and len(ops) == 2 and ops[1].type == X86_OP_REG:
                r2 = full(i.reg_name(ops[1].reg))
                if r2 in REGS:
                    st[r], st[r2] = st.get(r2), st[r]
                else:
                    st[r] = None
            else:
                st[r] = None
                if mn in ("mul", "imul", "div", "idiv") and len(ops) == 1:
                    st["eax"] = st["edx"] = None
        return st

    unknown = {r: None for r in REGS}
    unknown["esp"] = None
    unknown["stk"] = {}
    for t in switch_targets:
        state_in.setdefault(t, unknown)
    work = [entry] + sorted(switch_targets)
    while work:
        lead = work.pop()
        st = run(lead, state_in[lead], False)
        out_state[lead] = st
        for s in succ[lead]:
            if s not in state_in:
                state_in[s] = dict(st)
                work.append(s)
            else:
                old = state_in[s]
                merged = {r: join(old[r], st[r]) for r in REGS}
                merged["esp"] = old["esp"] if old["esp"] == st["esp"] else None
                merged["stk"] = {k: join(v, st["stk"][k]) for k, v in old["stk"].items() if k in st["stk"] and join(v, st["stk"][k]) is not None} if merged["esp"] is not None else {}
                if merged != old:
                    state_in[s] = merged
                    work.append(s)
    for lead in blocks:
        if lead in state_in:
            run(lead, state_in[lead], True)
    return facts


def build_facts(cache=FACTS_CACHE):
    b, rec = bm.load()
    out = {}
    pops = {fn["address"]: set(fn["pops"]) for fn in rec["functions"]}
    seh = seh_prologs(b, rec)
    for fn in rec["functions"]:
        out[fn["address"]] = facts_of(b, fn["address"], [tuple(s) for s in fn["spans"]], pops, seh)
    cache.parent.mkdir(parents=True, exist_ok=True)
    with open(cache, "wb") as fh:
        pickle.dump(dict(sha256=b.sha256, facts=out), fh)
    return b, rec, out


def load_facts(b, cache=FACTS_CACHE):
    with open(cache, "rb") as fh:
        d = pickle.load(fh)
    if d["sha256"] != b.sha256:
        raise bm.MapError("facts cache does not match game.dat")
    return d["facts"]


NEW_OPERATOR = 0x42F6E0


class Model:
    """Vtables, classes, the hierarchy, typed values and resolved virtual calls, from the per-function facts."""

    def __init__(self, b, rec, facts):
        self.b, self.rec, self.facts = b, rec, facts
        self.funcs = {fn["address"]: fn for fn in rec["functions"]}
        self.called = set()
        for fn in rec["functions"]:
            self.called.update(fn["calls"])
            self.called.update(fn["tails"])
        self._vtables()
        self._classes()
        self._hierarchy()

    # -- vtables ----------------------------------------------------------------------------------------------------------------
    def _vtables(self):
        """A vtable is a .rdata address that code installs (mov [R + d], V) whose first slot is a function entry. Its slots run
        while each is a function entry and does not reach the next installed vtable."""
        installs = defaultdict(list)  # V -> [(function, site, base, offset)]
        for f, fa in self.facts.items():
            for site, base, k, v in fa["vt"]:
                if self.b.u32(v) in self.funcs:
                    installs[v].append((f, site, base, k))
        starts = sorted(installs)
        startset = set(starts)
        self.vtables = {}
        for v in starts:
            slots = []
            a = v
            while True:
                if a != v and a in startset:
                    break
                if not self.b.in_data(a):
                    break
                x = self.b.u32(a)
                if x not in self.funcs:
                    break
                slots.append(x)
                a += 4
            self.vtables[v] = slots
        self.installs = installs
        self.slot_of = defaultdict(set)  # function -> {(vtable, index)}
        for v, slots in self.vtables.items():
            for n, x in enumerate(slots):
                self.slot_of[x].add((v, n))

    # -- classes ----------------------------------------------------------------------------------------------------------------
    def _classes(self):
        """A class is keyed by the vtable its constructors / destructors install at this + 0 last (in address order) on their
        own object; its layout is every vtable that function installs on this + k."""
        self.layout = {}       # class V0 -> {k: V}
        self.cdtors = defaultdict(set)  # class V0 -> functions installing it on this
        self.class_of_fn = {}  # ctor / dtor -> V0
        for f, fa in self.facts.items():
            own = {}
            for site, base, k, v in sorted(fa["vt"]):
                if base == ("this",) and self.b.u32(v) in self.funcs:
                    own[k] = v
            if 0 in own:
                v0 = own[0]
                self.class_of_fn[f] = v0
                self.cdtors[v0].add(f)
                lay = self.layout.setdefault(v0, {})
                for k, v in own.items():
                    if lay.get(k, v) != v:
                        lay.setdefault("conflicts", set()).add((k, v))
                    else:
                        lay[k] = v
        # objects built inline in a creator: new, then vtable stores on the new object in the same function
        self.inline_types = {}  # (function, new call site) -> V0
        for f, fa in self.facts.items():
            per = defaultdict(dict)
            for site, base, k, v in sorted(fa["vt"]):
                if base is not None and base[0] == "ret" and base[1] == NEW_OPERATOR:
                    per[base][k] = v
            for base, own in per.items():
                if 0 in own:
                    self.inline_types[(f, base[2])] = own[0]

    def _hierarchy(self):
        """Base of class C: a constructor / destructor of C calls, with ecx == this (+ k for a secondary base whose table C
        also installs), a function that installs another class's table on its own this + 0."""
        self.parents = defaultdict(set)  # V0 -> {(k, parent V0)}
        for v0, fns in self.cdtors.items():
            lay = self.layout[v0]
            for f in fns:
                for site, target, ecx in self.facts[f]["calls"]:
                    if target not in self.class_of_fn or target == f:
                        continue
                    pv = self.class_of_fn[target]
                    if pv == v0:
                        continue
                    if ecx == ("this",):
                        self.parents[v0].add((0, pv))
                    elif ecx is not None and ecx[0] == "add" and ecx[1] == ("this",) and ecx[2] in lay:
                        self.parents[v0].add((ecx[2], pv))
        self.children = defaultdict(set)
        for c, ps in self.parents.items():
            for k, p in ps:
                self.children[p].add(c)

    def descendants(self, v0):
        out, todo = set(), [v0]
        while todo:
            c = todo.pop()
            if c in out:
                continue
            out.add(c)
            todo.extend(self.children.get(c, ()))
        return out

    def subobject_table(self, v0, k):
        """The table at offset k of an object whose dynamic class is v0 (bases' tables are overwritten by the class's own)."""
        return self.layout.get(v0, {}).get(k)


class Types:
    """Proven object types. A type is (class V0, subobject offset, open): open means the class or any descendant (a method's
    this); closed means exactly that class (the object a constructor built). Sources, and nothing else:
      this                        -> every class whose table at k holds the function as a slot, or whose ctor / dtor it is
      ('ret', new, site)          -> the class whose tables the same function installs on it, or whose constructor it calls on it
      ('ret', ctor, site)         -> the constructor's class (MSVC constructors return this)
      ('ld', None, G) / ('ld', X, k) -> a global / field that every typed store fills with one class (untyped stores counted)
    """

    def __init__(self, m: Model):
        self.m = m
        self.method_types = defaultdict(set)
        for f, slots in m.slot_of.items():
            for v, n in slots:
                for c, lay in m.layout.items():
                    for k, tv in lay.items():
                        if k != "conflicts" and tv == v:
                            self.method_types[f].add((c, k, True))
        for f, c in m.class_of_fn.items():
            self.method_types[f].add((c, 0, True))
        self.new_types = {}  # (function, new site) -> class
        for (f, site), c in m.inline_types.items():
            self.new_types[(f, site)] = c
        for f, fa in m.facts.items():
            for site, target, ecx in fa["calls"]:
                if target in m.class_of_fn and ecx is not None and ecx[0] == "ret" and ecx[1] == NEW_OPERATOR:
                    self.new_types.setdefault((f, ecx[2]), m.class_of_fn[target])
        self.slot_types = {}      # ('g', addr) or (class, offset) -> class set
        self.slot_untyped = defaultdict(int)
        self._callers()
        self.vcall_at = {(f, site): (rval, disp) for f, fa in m.facts.items() for site, addr, rval, disp in fa["vcalls"]}
        self.site_call = {}
        for f, fa in m.facts.items():
            args = fa.get("args", {})
            for site, target, ecx in fa["calls"]:
                self.site_call[(f, site)] = (ecx, args.get(site, ()))
        for _ in range(6):
            self._memo = {}
            if not self._stores():
                break
        self._memo = {}

    # -- interprocedural ----------------------------------------------------------------------------------------------------------
    def _callers(self):
        """Direct call sites per callee, and the functions whose address code or data holds (their callers are not all known)."""
        self.callers = defaultdict(list)  # g -> [(f, site, ecx, args)]
        self.referenced = set()
        for f, fa in self.m.facts.items():
            args = fa.get("args", {})
            for site, target, ecx in fa["calls"]:
                self.callers[target].append((f, site, ecx, args.get(site, ())))
            for vals in [[v for _, _, _, v in fa["st"]], [x for a in args.values() for x in a]]:
                for v in vals:
                    if v is not None and v[0] == "imm" and v[1] in self.m.funcs:
                        self.referenced.add(v[1])
        b = self.m.b
        for lo, hi in b.data_ranges:
            for va in range(lo, hi - 3, 4):
                v = b.u32(va)
                if v in self.m.funcs:
                    self.referenced.add(v)
        self.summary = {}
        for g, fa in self.m.facts.items():
            r = None
            ok = bool(fa["rets"])
            for v in fa["rets"]:
                r = v if r is None else join(r, v)
                if r is None:
                    ok = False
                    break
            self.summary[g] = r if ok else None

    def closed_callers(self, g):
        """g's call sites when they are all known (only direct calls reach it), else None."""
        if g in self.referenced or g in self.m.slot_of or g not in self.callers:
            return None
        return self.callers[g]

    def of(self, f, v, depth=0):
        """{(class, offset, open)} for value v in function f, or empty when unproven."""
        if v is None or depth > 12:
            return set()
        key = (f, v)
        memo = self._memo.get(key)
        if memo is not None:
            return memo
        self._memo[key] = set()  # a cycle answers 'unproven'
        out = self._of(f, v, depth)
        self._memo[key] = out
        return out

    def _of(self, f, v, depth):
        kind = v[0]
        if kind == "in":
            return self.of(v[1], v[2], depth + 1)
        if kind == "this":
            if f in self.method_types:
                return set(self.method_types[f])
            return self._from_callers(f, lambda c: c[2], depth)
        if kind == "arg":
            n = v[1]
            return self._from_callers(f, lambda c: c[3][n] if n < len(c[3]) else None, depth)
        if kind == "ret":
            g, site = v[1], v[2]
            if g == NEW_OPERATOR:
                c = self.new_types.get((f, site))
                return {(c, 0, False)} if c else set()
            c = self.m.class_of_fn.get(g)
            if c:
                return {(c, 0, False)}
            s = self.summary.get(g)
            if s is None:
                return set()
            call = self.site_call.get((f, site))
            return self.of(f, self.subst(g, s, call), depth + 1)
        if kind == "vret":
            targets = self.vcall_targets(f, v[1], depth + 1)
            if not targets:
                return set()
            call = (self.m.facts[f].get("vecx", {}).get(v[1]), self.m.facts[f].get("args", {}).get(v[1], ()))
            out = None
            for g in targets:
                c = self.m.class_of_fn.get(g)
                ts = {(c, 0, False)} if c else self.of(f, self.subst(g, self.summary.get(g), call), depth + 1) if self.summary.get(g) is not None else set()
                if not ts:
                    return set()  # one possible target returns an unproven value
                out = ts if out is None else out | ts
            return out or set()
        if kind == "add":
            return {(c, k + v[2], o) for c, k, o in self.of(f, v[1], depth + 1)}
        if kind == "ld":
            out = set()
            for key in self.keys(f, v[1], v[2], depth + 1):
                if self.slot_untyped.get(key):
                    continue  # one store of an unproven value: the slot's type is not proven
                for c, o in self.slot_types.get(key, ()):
                    out.add((c, 0, o))
            return out
        return set()

    def vcall_targets(self, f, site, depth=0):
        """The proven target set of the virtual call at site in f, or empty."""
        rec = self.vcall_at.get((f, site))
        if rec is None:
            return set()
        rval, disp = rec
        if rval is None or rval[0] != "ld" or disp is None or disp % 4:
            return set()
        types = self.of(f, rval[1], depth)
        targets = set()
        for c, k, open_ in types:
            for cc in (self.m.descendants(c) if open_ else {c}):
                table = self.m.layout.get(cc, {}).get(k + rval[2])
                if table is None:
                    continue
                slots = self.m.vtables[table]
                if disp // 4 >= len(slots):
                    return set()
                targets.add(slots[disp // 4])
        return targets

    def _from_callers(self, g, pick, depth):
        sites = self.closed_callers(g)
        if not sites:
            return set()
        out = None
        for f, site, ecx, args in sites:
            v = pick((f, site, ecx, args))
            if v == NULL:
                continue
            ts = self.of(f, v, depth + 1)
            if not ts:
                return set()  # one call site passes an unproven value
            out = ts if out is None else out | ts
        return out or set()

    def subst(self, g, expr, call):
        """g's value expression in the caller's terms: this -> the caller's ecx, arg n -> its argument n; what only g can
        name (its own calls' results) is evaluated in g ('in', g, expr)."""
        if expr is None:
            return None
        kind = expr[0]
        if kind == "this":
            return call[0] if call else None
        if kind == "arg":
            return call[1][expr[1]] if call and expr[1] < len(call[1]) else None
        if kind in ("imm",):
            return expr
        if kind == "add":
            inner = self.subst(g, expr[1], call)
            return add(inner, expr[2]) if inner is not None and inner[0] != "in" else ("in", g, expr)
        if kind == "ld":
            if expr[1] is None:
                return expr
            inner = self.subst(g, expr[1], call)
            return load(inner, expr[2]) if inner is not None and inner[0] != "in" else ("in", g, expr)
        return ("in", g, expr)

    def keys(self, f, base, k, depth=0):
        """Storage keys of the dword at [base + k]: ('g', address) for an absolute address, (class, offset) per proven type."""
        if base is None:
            return [("g", k & 0xFFFFFFFF)]
        if base[0] == "imm":
            return [("g", (base[1] + k) & 0xFFFFFFFF)]
        return [(c, off + k) for c, off, _ in self.of(f, base, depth)]

    def _stores(self):
        """Slot types from every store of a pointer value. A store whose base is the function's own this / argument (a setter,
        SAGE's initSubsystem(TheX, ...) writing *sysref) is carried to each call site when the call sites are all known, with
        base and value in the caller's terms (two levels). A store through a pointer nothing proves is not counted: the model
        does not track aliasing (stated in the report)."""
        types = defaultdict(set)
        untyped = defaultdict(int)

        def record(f, base, k, val, level):
            ks = self.keys(f, base, k)
            if not ks and level < 2 and base is not None and self._entry_rooted(base):
                for cf, csite, ecx, args in (self.closed_callers(f) or ()):
                    cbase = self.subst(f, base, (ecx, args))
                    if cbase is None or cbase[0] == "in":
                        continue
                    cval = self.subst(f, val, (ecx, args)) if val is not None else None
                    b2, k2 = split(cbase)
                    record(cf, b2, k2 + k, cval, level + 1)
                return
            vt = {(c, o) for c, off, o in self.of(f, val) if off == 0}  # an open type (a this) keeps "or a descendant"
            for key in ks:
                if vt:
                    types[key] |= vt
                else:
                    untyped[key] += 1

        for f, fa in self.m.facts.items():
            for site, base, k, val in fa["st"]:
                if val is not None and val[0] == "imm":
                    continue  # constants (null, vtables) do not type a pointer slot
                record(f, base, k, val, 0)
        changed = dict(types) != self.slot_types
        self.slot_types = dict(types)
        self.slot_untyped = untyped
        return changed

    @staticmethod
    def _entry_rooted(v):
        while v is not None and v[0] in ("add", "ld") and v[1] is not None:
            v = v[1]
        return v is not None and v[0] in ("this", "arg")


def resolve_vcalls(m: Model, ty: Types):
    """Each `call [R + d]`: R must be a vtable pointer loaded from an object, ('ld', O, j). Targets = slot d / 4 of the table
    at the object's subobject offset, for the proven class (and its descendants for an open type)."""
    out = []
    for f, fa in m.facts.items():
        for site, addr, rval, disp in fa["vcalls"]:
            if rval is None or rval[0] != "ld" or disp is None or disp % 4:
                out.append((f, site, None, "receiver not a vtable load", rval, disp))
                continue
            obj, j = rval[1], rval[2]
            types = ty.of(f, obj)
            if not types:
                out.append((f, site, None, "object type unproven", obj, disp))
                continue
            targets, bad = set(), []
            for c, k, open_ in types:
                for cc in (m.descendants(c) if open_ else {c}):
                    table = m.layout.get(cc, {}).get(k + j)
                    if table is None:
                        continue
                    slots = m.vtables[table]
                    if disp // 4 >= len(slots):
                        bad.append(f"slot {disp // 4} beyond 0x{table:X} ({len(slots)} slots)")
                        continue
                    targets.add(slots[disp // 4])
            if targets:
                out.append((f, site, sorted(targets), "resolved" if not bad else "resolved; " + "; ".join(bad[:2]), obj, disp))
            else:
                out.append((f, site, None, "no table at that subobject" if not bad else "; ".join(bad[:2]), obj, disp))
    return out


def reader_index(m: Model, ty: Types):
    """(class or global, offset) -> [(function, instruction, size, write)] for every load / store with a proven base."""
    idx = defaultdict(list)
    for f, fa in m.facts.items():
        for site, base, k, size, write in fa["loads"]:
            for key in ty.keys(f, base, k):
                idx[key].append((f, site, size, write))
    return idx


if __name__ == "__main__":
    import time
    t = time.time()
    if sys.argv[1] == "facts":
        b, rec, out = build_facts()
        n = {k: sum(len(v[k]) for v in out.values()) for k in ("vt", "st", "calls", "vcalls", "loads")}
        print(f"facts for {len(out)} functions: {n} in {time.time() - t:.0f}s")
    elif sys.argv[1] == "report":
        b, rec = bm.load()
        m = Model(b, rec, load_facts(b))
        roots = [c for c in m.layout if not m.parents.get(c)]
        multi = {c: p for c, p in m.parents.items() if len({x for k, x in p if k == 0}) > 1}
        print(f"vtables {len(m.vtables)}, classes {len(m.layout)}, with a base {len(m.parents)}, roots {len(roots)}, "
              f"ambiguous primary base {len(multi)}, inline-built objects {len(m.inline_types)}, {time.time() - t:.0f}s")
        ty = Types(m)
        vc = resolve_vcalls(m, ty)
        from collections import Counter
        proven = sum(1 for k in ty.slot_types if not ty.slot_untyped.get(k))
        print("typed slots", len(ty.slot_types), "proven", proven, "virtual calls", len(vc), Counter(x[3].split(";")[0] for x in vc))
        idx = reader_index(m, ty)
        print("reader keys", len(idx), f"{time.time() - t:.0f}s")
        from binary_report import write_reports
        xm = next((a.split("=", 1)[1] for a in sys.argv if a.startswith("--xmap=")), None)
        counts, vcount = write_reports(b, rec, m, ty, vc, Path("docs/ennorath"), xmap_path=xm)
        print("reports written:", dict(counts), dict(vcount))
