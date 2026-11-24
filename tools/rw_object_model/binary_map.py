"""Whole-image function inventory of RotWK game.dat by exact recursive descent (no heuristics).

Run: python tools/rw_object_model/binary_map.py functions [--refresh]   (RW_GAME_DAT names the image)

What "exact" means here, and what is left out on purpose:
- A function entry is an address with code evidence: the PE entry point, the target of a direct call, the immediate of an
  instruction that is a .text address (push offset fn / mov [m], fn: callbacks, SEH handlers), or a slot of a vtable that code
  installs (`mov [reg + d], vtable`, see vtables()). Nothing is guessed from byte patterns (no prologue scan).
- Inside a function every direct branch is followed. `jmp` to another entry is a tail call. An indirect `jmp [idx*4 + table]`
  is followed only when its bound is proven by the `cmp idx, N` / `ja` guard in the same straight-line block (MSVC's switch,
  also through a byte index table `movzx idx, byte ptr [idx + bytes]`); otherwise it is listed in `unresolved_switches`.
- Calls that never return (imports ExitProcess / exit / _exit / abort / RaiseException-free throws, and functions proven to
  have no return path) end the block, found by iterating to a fixed point.
- Data-only references (a function pointer that only a data table holds, e.g. INI field parsers) are added in a second
  stage from words in .rdata / .data that point at a .text address no function owns yet, kept apart as `data_entries`, and
  each must decode cleanly or it is reported in `rejected_data_entries`.
The cache stores addresses and sizes only (never bytes), bound to the image's SHA256; keep it under ignored workspace/.
"""
from __future__ import annotations

import hashlib
import json
import os
import struct
import sys
from pathlib import Path

import capstone
from capstone.x86 import X86_OP_IMM, X86_OP_MEM, X86_OP_REG

ENV_VAR = "RW_GAME_DAT"
DEFAULT_CACHE = Path("workspace/rebuild/rw_binary_functions.json")
NORETURN_IMPORTS = {"ExitProcess", "exit", "_exit", "abort", "_CxxThrowException", "_invalid_parameter_noinfo", "terminate",
                    "?terminate@@YAXXZ", "_amsg_exit", "FatalAppExitA", "ExitThread", "_endthread", "_endthreadex"}
STOP_MNEMONICS = {"ret", "retf", "int3", "ud2", "hlt", "iret", "iretd"}


class MapError(RuntimeError):
    pass


class Binary:
    def __init__(self, path: str | None = None):
        path = path or os.environ.get(ENV_VAR)
        if not path:
            raise MapError(f"{ENV_VAR} is not set (path of the RotWK game.dat)")
        self.path = path
        raw = Path(path).read_bytes()
        self.sha256 = hashlib.sha256(raw).hexdigest()
        pe = struct.unpack_from("<I", raw, 0x3C)[0]
        nsec = struct.unpack_from("<H", raw, pe + 6)[0]
        opt = pe + 24
        self.base = struct.unpack_from("<I", raw, opt + 28)[0]
        self.entry = self.base + struct.unpack_from("<I", raw, opt + 16)[0]
        size_of_image = struct.unpack_from("<I", raw, opt + 56)[0]
        first = opt + struct.unpack_from("<H", raw, pe + 20)[0]
        image = bytearray(size_of_image)
        self.sections = {}
        for i in range(nsec):
            name, vsize, va, rsize, rptr = struct.unpack_from("<8sIIII", raw, first + i * 40)[:5]
            image[va:va + min(rsize, vsize or rsize)] = raw[rptr:rptr + min(rsize, vsize or rsize)]
            self.sections[name.rstrip(b"\0").decode("latin-1")] = (self.base + va, self.base + va + max(vsize, rsize))
        self.image = bytes(image)
        self.text = self.sections[".text"]
        self.data_ranges = [self.sections[".rdata"], self.sections[".data"]]
        self.md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
        self.md.detail = True
        self.imports = self._imports(opt)

    def _imports(self, opt):
        """IAT slot VA -> imported name."""
        rva, size = struct.unpack_from("<II", self.image, struct.unpack_from("<I", self.image, 0x3C)[0] + 24 + 96 + 8)
        out = {}
        if not rva:
            return out
        for d in range(rva, rva + size, 20):
            olt, _, _, name, iat = struct.unpack_from("<IIIII", self.image, d)
            if not name:
                break
            thunk = olt or iat
            k = 0
            while True:
                v = struct.unpack_from("<I", self.image, thunk + 4 * k)[0]
                if not v:
                    break
                if not v & 0x80000000:
                    end = self.image.index(b"\0", v + 2)
                    out[self.base + iat + 4 * k] = self.image[v + 2:end].decode("latin-1")
                k += 1
        return out

    def u32(self, va: int) -> int:
        return struct.unpack_from("<I", self.image, va - self.base)[0]

    def u8(self, va: int) -> int:
        return self.image[va - self.base]

    def in_text(self, va: int) -> bool:
        return self.text[0] <= va < self.text[1]

    def in_data(self, va: int) -> bool:
        return any(a <= va < b for a, b in self.data_ranges)

    def insn(self, va: int):
        o = va - self.base
        return next(self.md.disasm(self.image[o:o + 16], va, count=1), None)


def _switch_targets(b: Binary, block: list, jmp):
    """Targets of `jmp [idx*4 + table]` when the straight-line block proves the bound; None when it does not."""
    m = jmp.operands[0].mem
    if not (m.index and m.scale == 4 and not m.base and (b.in_data(m.disp) or b.in_text(m.disp))):
        return None
    idx = jmp.reg_name(m.index)
    table = m.disp
    byte_table = None
    bound = None
    for ins in reversed(block[:-1]):
        ops = ins.operands
        if ins.mnemonic == "movzx" and len(ops) == 2 and ops[0].type == X86_OP_REG and jmp.reg_name(ops[0].reg) == idx \
                and ops[1].type == X86_OP_MEM and ops[1].size == 1 and ops[1].mem.index == 0 and ops[1].mem.base:
            byte_table = ops[1].mem.disp
            idx = ins.reg_name(ops[1].mem.base)
            continue
        if ins.mnemonic == "cmp" and len(ops) == 2 and ops[0].type == X86_OP_REG and ins.reg_name(ops[0].reg) == idx \
                and ops[1].type == X86_OP_IMM:
            bound = ops[1].imm
            break
        if ops and ops[0].type == X86_OP_REG and ins.reg_name(ops[0].reg) == idx and ins.mnemonic not in ("cmp", "test"):
            return None  # the index is redefined between the guard and the jump: no proof
    if bound is None:
        return None
    if byte_table is not None:
        count = max(b.u8(byte_table + i) for i in range(bound + 1)) + 1
    else:
        count = bound + 1
    return [b.u32(table + 4 * i) for i in range(count)]


def decode_function(b: Binary, start: int, entries: set[int], noreturn: set[int], extra: tuple = ()):
    """Instructions owned by the function at start: {va: size}, direct calls, tail calls, immediates into .text, flags."""
    owned: dict[int, int] = {}
    calls: set[int] = set()
    tails: set[int] = set()
    code_refs: set[int] = set()
    unresolved: list[int] = []
    returns = False
    pops: set[int] = set()
    installs: set[int] = set()
    pending = [start] + list(extra)
    while pending:
        a = pending.pop()
        block = []
        while a not in owned:
            if not b.in_text(a):
                raise MapError(f"function 0x{start:X} runs out of .text at 0x{a:X}")
            ins = b.insn(a)
            if ins is None:
                raise MapError(f"function 0x{start:X}: undecodable instruction at 0x{a:X}")
            owned[a] = ins.size
            block.append(ins)
            mn = ins.mnemonic
            for op in ins.operands:
                if op.type == X86_OP_IMM and b.in_text(op.imm & 0xFFFFFFFF) and mn not in ("call", "jmp") and not mn.startswith("j"):
                    code_refs.add(op.imm & 0xFFFFFFFF)
            if mn == "mov" and len(ins.operands) == 2 and ins.operands[0].type == X86_OP_MEM and ins.operands[0].size == 4                     and ins.operands[0].mem.base and ins.operands[1].type == X86_OP_IMM:
                v = ins.operands[1].imm & 0xFFFFFFFF
                if b.in_data(v) and b.in_text(b.u32(v)):
                    installs.add(v)  # `mov [reg + d], table`: a vtable installed on an object
            if mn in STOP_MNEMONICS:
                returns = returns or mn.startswith("ret") or mn.startswith("iret")
                if mn == "ret":
                    pops.add(ins.operands[0].imm if ins.operands else 0)
                break
            if mn == "call":
                op = ins.operands[0]
                if op.type == X86_OP_IMM:
                    calls.add(op.imm)
                    if op.imm in noreturn:
                        break
                elif op.type == X86_OP_MEM and not op.mem.base and not op.mem.index and b.imports.get(op.mem.disp) in NORETURN_IMPORTS:
                    break
                a += ins.size
                continue
            if mn == "jmp":
                op = ins.operands[0]
                if op.type == X86_OP_IMM:
                    if op.imm in entries and op.imm != start:
                        tails.add(op.imm)
                    else:
                        pending.append(op.imm)
                elif op.type == X86_OP_MEM:
                    targets = _switch_targets(b, block, ins)
                    if targets is None:
                        if not op.mem.base and not op.mem.index and op.mem.disp in b.imports:
                            tails.add(-op.mem.disp)  # tail call through the IAT
                            returns = True
                        else:
                            unresolved.append(ins.address)
                    else:
                        pending.extend(targets)
                else:
                    unresolved.append(ins.address)  # jmp reg
                break
            if mn.startswith("j") or mn.startswith("loop"):
                op = ins.operands[0]
                if op.type == X86_OP_IMM:
                    pending.append(op.imm)
            a += ins.size
    prev_end = None
    for a in sorted(owned):
        if prev_end is not None and a < prev_end:
            raise MapError(f"function 0x{start:X}: instruction at 0x{a:X} starts inside the previous one (misaligned decode)")
        prev_end = a + owned[a]
    return owned, calls, tails, code_refs, unresolved, returns, pops, installs


def owners(funcs):
    own: dict[int, list[int]] = {}
    for f, r in funcs.items():
        for a in r[0]:
            own.setdefault(a, []).append(f)
    return own


def closure(b: Binary, entries: set[int], noreturn: set[int], funcs: dict, errors: dict, extras: dict):
    """Decode every entry and every direct call / tail-call target reached from them (exact code evidence)."""
    queue = sorted(entries - funcs.keys())
    while queue:
        f = queue.pop()
        if f in funcs or f in errors:
            continue
        try:
            res = decode_function(b, f, entries, noreturn, tuple(sorted(extras.get(f, ()))))
        except MapError as e:
            errors[f] = str(e)
            continue
        funcs[f] = res
        new = {t for t in (res[1] | {t for t in res[2] if t > 0}) if b.in_text(t)} - entries
        entries |= new
        queue.extend(sorted(new))


def candidates(b: Binary, funcs, owned):
    """Code immediates and data words that are .text addresses no decoded function owns: (address, tier)."""
    out = {}
    for r in funcs.values():
        for v in r[3]:
            if v not in funcs and v not in owned:
                out.setdefault(v, "code-imm")
    for lo, hi in b.data_ranges:
        for va in range(lo, hi - 3, 4):
            v = b.u32(va)
            if b.in_text(v) and v not in funcs and v not in owned:
                out.setdefault(v, "data")
    return out


def log(msg):
    print(f"[{__import__('time').strftime('%H:%M:%S')}] {msg}", file=sys.stderr, flush=True)


def settle(b: Binary, funcs, entries, noreturn, extras, tier, rejected, errors, protected=frozenset()):
    """Make the result independent of discovery order: decode every function again with the final entry set (a jump to a
    function found later is a tail call, not shared code), then fold a function known only from data or a code immediate
    that runs into the code of exactly one other function into it as an extra block (SEH __except bodies, catch
    continuations). Repeat until nothing changes. Functions with call evidence are never folded."""
    while True:
        entries |= set(funcs)
        for f in sorted(funcs):
            try:
                funcs[f] = decode_function(b, f, entries, noreturn, tuple(sorted(extras.get(f, ()))))
            except MapError as e:
                if not extras.get(f):
                    errors[f] = str(e)
                    del funcs[f]
                    continue
                # an extra block accepted earlier no longer fits (the entry set grew): drop the blocks, keep the function
                for x in extras.pop(f):
                    rejected[x] = ("extra-block", f"dropped from 0x{f:X}: {e}")
                try:
                    funcs[f] = decode_function(b, f, entries, noreturn)
                except MapError as e2:
                    errors[f] = str(e2)
                    del funcs[f]
        called = set()
        for r in funcs.values():
            called |= r[1] | {x for x in r[2] if x > 0}
        own: dict[int, set[int]] = {}
        for f, r in funcs.items():
            for a in r[0]:
                own.setdefault(a, set()).add(f)
        changed = False
        for f in sorted(funcs):
            if f in called or f == b.entry or f in protected:
                continue  # call evidence or a slot of an installed table: an entry, never folded
            others = set()
            for a in funcs[f][0]:
                others |= own[a] - {f}
            if not others:
                continue
            if len(others) == 1:
                parent = others.pop()
                trial = set(extras.get(parent, ())) | {f} | extras.get(f, set())
                try:
                    decode_function(b, parent, entries, noreturn, tuple(sorted(trial)))
                except MapError as e:
                    # the candidate does not fit the parent's instruction stream (a data word that points inside an instruction)
                    rejected[f] = (tier.get(f, "data"), f"does not fold into 0x{parent:X}: {e}")
                else:
                    extras[parent] = trial
                    extras.pop(f, None)
                    tier[f] = "extra-block"
            else:
                rejected[f] = (tier.get(f, "data"), "runs into " + ", ".join(f"0x{c:X}" for c in sorted(others)))
            del funcs[f]
            entries.discard(f)
            changed = True
        log(f"settle: {len(funcs)} functions, folded / rejected this pass" if changed else f"settle: stable at {len(funcs)} functions")
        if not changed:
            return


def vtable_slots(b: Binary, funcs):
    """The .text pointers of every installed table, from its start while the words are .text addresses (a table ends at the
    first word that is not; an adjacent table's slots are function entries too)."""
    out = set()
    for r in funcs.values():
        for v in r[7]:
            a = v
            while b.in_data(a) and b.in_text(b.u32(a)):
                out.add(b.u32(a))
                a += 4
    return out


def close_noreturn(b: Binary, funcs, entries, noreturn, extras):
    """The noreturn set's fixed point without a new discovery: a function with no return path, no tail call and no unresolved
    jump never returns; its callers are decoded again with it (only theirs), which can make them noreturn in turn. Decoding a
    caller again here is a trial only: the blocks it would lose are re-attributed by the full rebuild the caller runs next."""
    nr = set(noreturn)
    trial = dict(funcs)
    callers: dict[int, set[int]] = {}
    for f, r in trial.items():
        for c in r[1]:
            callers.setdefault(c, set()).add(f)
    new = {f for f, r in trial.items() if not r[5] and not r[2] and not r[4]} - nr
    while new:
        nr |= new
        redo = set()
        for f in new:
            redo |= callers.get(f, set())
        new = set()
        for f in redo:
            try:
                r = decode_function(b, f, entries, nr, tuple(sorted(extras.get(f, ()))))
            except MapError:
                continue
            trial[f] = r
            if not r[5] and not r[2] and not r[4] and f not in nr:
                new.add(f)
    return nr


def build(b: Binary):
    """Stage 1: the PE entry and its direct-call closure, to a noreturn fixed point. Stage 2: candidates from code
    immediates and data words. A candidate that decodes cleanly and owns no decoded instruction is a new function. One
    whose code runs into the code of exactly one function and that function only is that function's extra block (catch
    continuations and other blocks reached only through the exception tables): the function is decoded again with it.
    Anything else is rejected and listed."""
    noreturn = {a for a, n in b.imports.items() if n in NORETURN_IMPORTS}
    extras: dict[int, set[int]] = {}
    while True:
        entries = {b.entry}
        funcs, errors = {}, {}
        closure(b, entries, noreturn, funcs, errors, extras)
        tier = {}
        rejected = {}
        while True:
            own = {}
            for f, r in funcs.items():
                for a in r[0]:
                    own[a] = f
            accepted = 0
            for v, kind in sorted(candidates(b, funcs, own).items()):
                if v in rejected or v in own:
                    continue
                try:
                    res = decode_function(b, v, entries, noreturn)
                except MapError as e:
                    rejected[v] = (kind, str(e))
                    continue
                clash = {own[a] for a in res[0] if a in own}
                if len(clash) == 1:
                    parent = clash.pop()
                    extras.setdefault(parent, set()).add(v)
                    try:
                        pres = decode_function(b, parent, entries, noreturn, tuple(sorted(extras[parent])))
                    except MapError as e:
                        extras[parent].discard(v)
                        rejected[v] = (kind, f"extra block of 0x{parent:X} fails: {e}")
                        continue
                    if any(own.get(a, parent) != parent for a in pres[0]):
                        extras[parent].discard(v)
                        rejected[v] = (kind, f"extra block of 0x{parent:X} runs into another function")
                        continue
                    funcs[parent] = pres
                    for a in pres[0]:
                        own[a] = parent
                    entries |= {x for x in (pres[1] | {y for y in pres[2] if y > 0}) if b.in_text(x)}
                    tier.setdefault(v, "extra-block")
                elif clash:
                    rejected[v] = (kind, "runs into " + ", ".join(f"0x{c:X}" for c in sorted(clash)))
                    continue
                else:
                    entries.add(v)
                    entries |= {x for x in (res[1] | {y for y in res[2] if y > 0}) if b.in_text(x)}
                    funcs[v] = res
                    for a in res[0]:
                        own[a] = v
                    tier[v] = kind
                before = set(funcs)
                closure(b, entries, noreturn, funcs, errors, extras)
                for f in set(funcs) - before:
                    for a in funcs[f][0]:
                        own[a] = f
                accepted += 1
            if not accepted:
                break
        settle(b, funcs, entries, noreturn, extras, tier, rejected, errors)
        log(f"discovery done: {len(funcs)} functions; vtable slots next")
        # slots of every table code installs are function entries (a virtual call reaches them): one that another function owns
        # (reached by a jmp from a function that knew no entry there) becomes an entry, and that owner is decoded again
        tried = set()
        while True:
            slots = vtable_slots(b, funcs)
            new = sorted(s for s in slots if s not in funcs and s not in errors and s not in tried)
            log(f"vtable slots: {len(slots)}, new entries {len(new)}")
            if not new:
                break
            tried.update(new)
            for s in new:
                entries.add(s)
                tier.setdefault(s, "vtable-slot")
            closure(b, entries, noreturn, funcs, errors, extras)
            settle(b, funcs, entries, noreturn, extras, tier, rejected, errors, protected=vtable_slots(b, funcs))
        nr = close_noreturn(b, funcs, entries, noreturn, extras)
        log(f"noreturn check: {len(nr - noreturn)} new noreturn functions (after the callers' fixed point)")
        if nr <= noreturn:
            called = set()
            for r in funcs.values():
                called |= r[1] | {t for t in r[2] if t > 0}
            for f in funcs:
                if f in called or f == b.entry:
                    tier[f] = "call"
                else:
                    tier.setdefault(f, "data")
            slots = vtable_slots(b, funcs)
            for f in funcs:
                if tier.get(f) == "call":
                    continue
                if f in slots:
                    tier[f] = "vtable-slot"
                elif tier.get(f) not in ("data", "code-imm"):
                    tier[f] = "data"  # a function never carries the label of a folded block
            for f in list(tier):
                if f not in funcs and tier[f] not in ("extra-block",):
                    del tier[f]
            build.extras = extras
            return funcs, noreturn, tier, rejected, errors
        noreturn |= nr  # re-decode: blocks after calls to the new noreturn functions belong elsewhere


def save(b: Binary, cache: Path, funcs, noreturn, tier, rejected, errors, extras=None):
    rec = dict(sha256=b.sha256, analyzer="tools/rw_object_model/binary_map.py recursive descent (exact; see module doc)",
               functions=[dict(address=f, size=sum(r[0].values()), calls=sorted(r[1]), tails=sorted(t for t in r[2] if t > 0),
                               spans=_spans(r[0]), unresolved=r[4], returns=r[5], pops=sorted(r[6]), installs=sorted(r[7])) for f, r in sorted(funcs.items())],
               noreturn=sorted(noreturn), tier={f"0x{k:X}": v for k, v in sorted(tier.items())},
               rejected_candidates={f"0x{k:X}": list(v) for k, v in sorted(rejected.items())},
               call_target_errors={f"0x{k:X}": v for k, v in sorted(errors.items())},
               extra_blocks={f"0x{k:X}": sorted(v) for k, v in sorted((extras or {}).items()) if v})
    shared = {}
    for f, r in funcs.items():
        for a in r[0]:
            shared.setdefault(a, []).append(f)
    rec["shared_code"] = sorted({tuple(sorted(v)) for v in shared.values() if len(v) > 1})
    cache.parent.mkdir(parents=True, exist_ok=True)
    cache.write_text(json.dumps(rec, sort_keys=True) + "\n", encoding="utf-8")
    return rec


def _spans(owned):
    out = []
    for a in sorted(owned):
        if out and out[-1][0] + out[-1][1] == a:
            out[-1][1] += owned[a]
        else:
            out.append([a, owned[a]])
    return out


def load(cache: Path = DEFAULT_CACHE, b: Binary | None = None):
    b = b or Binary()
    rec = json.loads(cache.read_text(encoding="utf-8"))
    if rec["sha256"] != b.sha256:
        raise MapError("binary function cache does not match game.dat (SHA256)")
    return b, rec


def split_at(spans, entry):
    """The spans with one starting exactly at the entry (an extra block right before the entry is merged into its span)."""
    out = []
    for a, s in spans:
        if a < entry < a + s:
            out += [[a, entry - a], [entry, a + s - entry]]
        else:
            out.append([a, s])
    return out


def main(argv):
    import argparse
    import time
    p = argparse.ArgumentParser()
    p.add_argument("cmd", choices=["functions", "export-blocks"])
    p.add_argument("--cache", type=Path, default=DEFAULT_CACHE)
    p.add_argument("--out", type=Path, help="export-blocks: the rw_function_blocks.json-shaped file for tools/re/inventory.py --angr")
    a = p.parse_args(argv)
    if a.cmd == "export-blocks":
        b, rec = load(a.cache)
        out = dict(sha256=b.sha256, analyzer=rec["analyzer"],
                   functions=[dict(address=f["address"], blocks=split_at(f["spans"], f["address"])) for f in rec["functions"]])
        a.out.write_text(json.dumps(out) + chr(10), encoding="utf-8")
        print(f"{len(out['functions'])} functions -> {a.out}")
        return
    t = time.time()
    b = Binary()
    funcs, noreturn, tier, rejected, errors = build(b)
    rec = save(b, a.cache, funcs, noreturn, tier, rejected, errors, getattr(build, "extras", None))
    own = owners(funcs)
    shared = sum(1 for v in own.values() if len(v) > 1)
    unresolved = sum(len(f["unresolved"]) for f in rec["functions"])
    from collections import Counter
    print("tiers", dict(Counter(tier.values())), "rejected", dict(Counter(k for k, _ in rejected.values())), "call-target errors", len(errors))
    print(f"functions {len(funcs)}, noreturn {len(noreturn)}, "
          f"instructions {len(own)}, shared instructions {shared}, unresolved indirect jumps {unresolved}, {time.time() - t:.0f}s")


if __name__ == "__main__":
    main(sys.argv[1:])
