#!/usr/bin/env python3
"""Static candidate primitives for top-level INI fields, not proof of a reader.

A tracked value is a pointer loaded from a specified absolute global. The
analysis propagates possible values over direct CFG edges within each function;
indirect edges, stack spills and unmodelled transformations are not followed.
The input is an address/size inventory and the loaded image; no game bytes are
written by this module.
"""
from collections import defaultdict, deque
import struct

from capstone import Cs, CS_ARCH_X86, CS_MODE_32, CS_AC_READ
from capstone.x86 import X86_OP_IMM, X86_OP_MEM, X86_OP_REG, X86_REG_INVALID

# Aliases are normalized before killing values: writing AL invalidates EAX.
FAMILIES = {
    name: full for full, aliases in {
        'eax': ('eax', 'ax', 'al', 'ah'), 'ebx': ('ebx', 'bx', 'bl', 'bh'),
        'ecx': ('ecx', 'cx', 'cl', 'ch'), 'edx': ('edx', 'dx', 'dl', 'dh'),
        'esi': ('esi', 'si', 'sil'), 'edi': ('edi', 'di', 'dil'),
        'ebp': ('ebp', 'bp', 'bpl'), 'esp': ('esp', 'sp', 'spl'),
    }.items() for name in aliases
}
VOLATILE = {'eax', 'ecx', 'edx'}


def global_reads(data, base, blocks, globals_by_address, pointer_paths=(), virtual_getters=(), skipped=None, *, static_bases=None,
                 subobject_offsets=None, indexed_fields=None, initial_receivers=None, calls_out=None,
                 call_targets=None, indexed_bases=None):
    """Return (function, global address, pointer path, offset, instruction).

    Propagate possible pointer values along direct CFG edges. Union at joins is
    a may-analysis, not a guarantee that a candidate is a reachable read.
    Calls kill volatile registers, partial or other writes kill aliases; stack
    spills and indirect branches are not followed. Indexed reads are emitted
    only for explicitly configured array offsets and scales; bounds are unknown.
    LEA/ADD can propagate only explicitly configured subobject shifts, once per path;
    address-taking itself is not a read. Each load and access belongs to the
    same recovered function, unless initial_receivers explicitly supplies a
    receiver from a separately checked call site. calls_out optionally records
    direct calls and tail jumps carrying a tracked ECX receiver.
    """
    decoder = Cs(CS_ARCH_X86, CS_MODE_32)
    decoder.detail = True
    result = set()
    pointer_paths = set(pointer_paths)
    virtual_getters = set(virtual_getters)
    static_bases = static_bases or {}
    subobject_offsets = subobject_offsets or {}
    indexed_fields = indexed_fields or {}
    initial_receivers = initial_receivers or {}
    call_targets = set(blocks) if call_targets is None else set(call_targets)
    indexed_bases = indexed_bases or {}
    absolute_fields = defaultdict(set)
    for address, offsets in static_bases.items():
        for offset in offsets:
            absolute_fields[(address + offset) & 0xffffffff].add((address, offset))
    for function, spans in sorted(blocks.items()):
        decoded = {}
        try:
            for start, size in sorted(spans):
                if size <= 0 or start < base or start + size > base + len(data):
                    raise ValueError(f'invalid block at {start:#x}')
                instructions = list(decoder.disasm(data[start-base:start-base+size], start))
                if not instructions or instructions[-1].address + instructions[-1].size != start + size:
                    raise ValueError(f'truncated block at {start:#x}')
                decoded[start] = instructions
            if function not in decoded:
                raise ValueError(f'function has no entry block at {function:#x}')
        except ValueError:
            if skipped is None:
                raise
            skipped.append(function)
            continue
        states = {function: {'ecx': set(initial_receivers[function])} if function in initial_receivers else {}}
        pending = deque([function])
        while pending:
            start = pending.popleft()
            tracked = {reg: set(values) for reg, values in states[start].items()}
            for ins in decoded[start]:
                for op in ins.operands:
                    if ins.mnemonic == 'lea' or op.type != X86_OP_MEM or not (op.access & CS_AC_READ):
                        continue
                    if op.mem.index != X86_REG_INVALID:
                        reg = FAMILIES.get(ins.reg_name(op.mem.base))
                        for pointer, path in tracked.get(reg, ()):
                            if not path and (op.mem.disp, op.mem.scale) in indexed_fields.get(pointer, ()):
                                result.add((function, pointer, ('indexed', op.mem.scale), op.mem.disp, ins.address))
                        continue
                    if op.mem.base == X86_REG_INVALID:
                        for pointer, offset in absolute_fields.get(op.mem.disp & 0xffffffff, ()):
                            result.add((function, pointer, ('static',), offset, ins.address))
                    reg = FAMILIES.get(ins.reg_name(op.mem.base))
                    for pointer, path in tracked.get(reg, ()):
                        if path and isinstance(path[-1], tuple) and path[-1][0] == 'indexed-base':
                            _, array_base, scale = path[-1]
                            offset = array_base + op.mem.disp
                            if (offset, scale) in indexed_fields.get(pointer, ()):
                                result.add((function, pointer, ('indexed', scale), offset, ins.address))
                        elif path and isinstance(path[-1], tuple) and path[-1][0] == 'base':
                            result.add((function, pointer, path[:-1], op.mem.disp + path[-1][1], ins.address))
                        elif not path or path[-1] != 'vtable':
                            result.add((function, pointer, path, op.mem.disp, ins.address))
                if (calls_out is not None and ins.mnemonic in ('call', 'jmp') and ins.operands
                        and ins.operands[0].type == X86_OP_IMM):
                    target = ins.operands[0].imm
                    if target in call_targets and target != function:
                        for pointer, path in tracked.get('ecx', ()):
                            calls_out.add((function, ins.address, target, pointer, path))
                copied = set()
                virtual_returns = set()
                if ins.mnemonic == 'call' and ins.operands and ins.operands[0].type == X86_OP_MEM:
                    mem = ins.operands[0].mem
                    if mem.index == X86_REG_INVALID:
                        tables = tracked.get(FAMILIES.get(ins.reg_name(mem.base)), ())
                        receivers = tracked.get('ecx', ())
                        for pointer, path in receivers:
                            if (pointer, path, mem.disp) in virtual_getters and (pointer, path + ('vtable',)) in tables:
                                virtual_returns.add((pointer, path + (f'v{mem.disp:X}',)))
                if ins.mnemonic == 'mov' and len(ins.operands) == 2:
                    dst, src = ins.operands
                    if dst.type == X86_OP_REG and dst.size == 4:
                        if src.type == X86_OP_MEM and src.size == 4 and src.mem.base == X86_REG_INVALID and src.mem.index == X86_REG_INVALID:
                            absolute = src.mem.disp & 0xffffffff
                            if absolute in globals_by_address:
                                copied.add((absolute, ()))
                        elif src.type == X86_OP_MEM and src.size == 4 and src.mem.index == X86_REG_INVALID:
                            for pointer, path in tracked.get(FAMILIES.get(ins.reg_name(src.mem.base)), ()):
                                nested = path + (src.mem.disp,)
                                if (pointer, nested) in pointer_paths:
                                    copied.add((pointer, nested))
                                if src.mem.disp == 0 and any(p == pointer and pp == path for p, pp, _ in virtual_getters):
                                    copied.add((pointer, path + ('vtable',)))
                        elif src.type == X86_OP_IMM and (src.imm & 0xffffffff) in static_bases:
                            copied.add((src.imm & 0xffffffff, ('static',)))
                        elif src.type == X86_OP_REG and src.size == 4:
                            copied.update(tracked.get(FAMILIES.get(ins.reg_name(src.reg)), ()))
                # A verified array base taken by LEA is not a read; following
                # it to a configured member preserves the unknown dynamic index.
                if ins.mnemonic == 'lea' and len(ins.operands) == 2:
                    dst, src = ins.operands
                    if (dst.type == X86_OP_REG and dst.size == 4 and src.type == X86_OP_MEM
                            and src.mem.index != X86_REG_INVALID):
                        reg = FAMILIES.get(ins.reg_name(src.mem.base))
                        for pointer, path in tracked.get(reg, ()):
                            if not path and (src.mem.disp, src.mem.scale) in indexed_bases.get(pointer, ()):
                                copied.add((pointer, (('indexed-base', src.mem.disp, src.mem.scale),)))
                # Only known singleton subobjects are propagated. Not chaining
                # shifts keeps the may-analysis finite even on CFG loops.
                if ins.mnemonic in ('lea', 'add') and len(ins.operands) == 2:
                    dst, src = ins.operands
                    reg, shift = None, None
                    if dst.type == X86_OP_REG and dst.size == 4:
                        if ins.mnemonic == 'lea' and src.type == X86_OP_MEM and src.mem.index == X86_REG_INVALID:
                            reg, shift = FAMILIES.get(ins.reg_name(src.mem.base)), src.mem.disp
                        elif ins.mnemonic == 'add' and src.type == X86_OP_IMM:
                            reg, shift = FAMILIES.get(ins.reg_name(dst.reg)), src.imm
                    for pointer, path in tracked.get(reg, ()):
                        if not path and shift in subobject_offsets.get(pointer, ()):
                            copied.add((pointer, (('base', shift),)))
                _, writes = ins.regs_access()
                for reg in writes:
                    tracked.pop(FAMILIES.get(ins.reg_name(reg)), None)
                if ins.mnemonic == 'call':
                    for reg in VOLATILE:
                        tracked.pop(reg, None)
                if virtual_returns:
                    tracked['eax'] = virtual_returns
                if copied:
                    tracked[FAMILIES[ins.reg_name(ins.operands[0].reg)]] = copied
            last = decoded[start][-1]
            successors = set()
            if last.mnemonic.startswith('j'):
                if last.operands and last.operands[0].type == X86_OP_IMM:
                    successors.add(last.operands[0].imm)
                if last.mnemonic != 'jmp':
                    successors.add(last.address + last.size)
            elif not (last.mnemonic.startswith('ret') or last.mnemonic in {'int3', 'ud2', 'hlt'}):
                successors.add(last.address + last.size)
            for target in sorted(successors & decoded.keys()):
                previous = states.get(target)
                merged = {reg: set(values) for reg, values in (previous or {}).items()}
                for reg, values in tracked.items():
                    merged.setdefault(reg, set()).update(values)
                if previous is None or merged != previous:
                    states[target] = merged
                    pending.append(target)
    return sorted(result)


def getter_callers(functions, getters):
    """Map each known getter entry to direct caller entries, including tailcalls.

    The graph is produced by rw_function_map.decode_block_inventory; indirect
    calls cannot be resolved and getter identification belongs to the caller.
    """
    result = defaultdict(set)
    for caller, function in functions.items():
        for target in function.calls & getters:
            result[target].add(caller)
    return {getter: sorted(result[getter]) for getter in sorted(getters)}


def field_table(data, base, address, end_address=None):
    """Read 16-byte INI field entries from a mapped image, through the terminator.

    Values and raw bytes must never be written to the report. A malformed table
    or name is an error rather than a partial result. end_address is an
    explicitly verified exclusive bound for a table without a null sentinel.
    """
    rows = {}
    if end_address is not None and (end_address < address or (end_address - address) % 16):
        raise ValueError('invalid field table bound')
    while True:
        if address == end_address:
            return rows
        pos = address - base
        if pos < 0 or pos + 16 > len(data):
            raise ValueError(f'field table outside image at {address:#x}')
        name_ptr, parser, _, offset = struct.unpack_from('<IIII', data, pos)
        if name_ptr == 0:
            return rows
        name_pos = name_ptr - base
        if not 0 <= name_pos < len(data):
            raise ValueError(f'field name outside image at {address:#x}')
        end = data.find(b'\0', name_pos, min(name_pos + 256, len(data)))
        if end < 0 or not parser:
            raise ValueError(f'invalid field entry at {address:#x}')
        name = data[name_pos:end].decode('ascii')
        if name.lower() in rows:
            raise ValueError(f'duplicate field name {name}')
        rows[name.lower()] = offset
        address += 16


def getter_offsets(data, base, blocks, getters):
    """Find read or address-taking offsets from ECX in named thiscall getters.

    Address-taking (LEA/ADD) is a hint, not proof a value is read. Deliberately inspect only each entry basic block: branch joins and indirect
    calls need type/flow information not present in the inventory. An offset
    is only a hint: named BFME2 matches can be wrong, especially tier B.
    """
    decoder = Cs(CS_ARCH_X86, CS_MODE_32)
    decoder.detail = True
    result = defaultdict(set)
    for address in getters:
        spans = blocks.get(address, ())
        entry = next((size for start, size in spans if start == address), None)
        if entry is None:
            continue
        if entry <= 0 or address < base or address + entry > base + len(data):
            raise ValueError(f'invalid getter block at {address:#x}')
        instructions = list(decoder.disasm(data[address-base:address-base+entry], address))
        if not instructions or instructions[-1].address + instructions[-1].size != address + entry:
            raise ValueError(f'truncated getter block at {address:#x}')
        tracked = {'ecx'}
        for ins in instructions:
            for op in ins.operands:
                if (op.type == X86_OP_MEM and (ins.mnemonic == 'lea' or op.access & CS_AC_READ)
                        and op.mem.index == X86_REG_INVALID and FAMILIES.get(ins.reg_name(op.mem.base)) in tracked):
                    result[address].add(op.mem.disp)
            if ins.mnemonic == 'add' and len(ins.operands) == 2:
                dst, src = ins.operands
                if (dst.type == X86_OP_REG and dst.size == 4 and src.type == X86_OP_IMM
                        and FAMILIES.get(ins.reg_name(dst.reg)) in tracked):
                    result[address].add(src.imm)
            copied = False
            if ins.mnemonic == 'mov' and len(ins.operands) == 2:
                dst, src = ins.operands
                copied = (dst.type == X86_OP_REG and dst.size == 4 and src.type == X86_OP_REG and src.size == 4
                          and FAMILIES.get(ins.reg_name(src.reg)) in tracked)
            _, writes = ins.regs_access()
            for reg in writes:
                tracked.discard(FAMILIES.get(ins.reg_name(reg)))
            if ins.mnemonic == 'call':
                tracked.difference_update(VOLATILE)
            if copied:
                tracked.add(FAMILIES[ins.reg_name(ins.operands[0].reg)])
    return dict(result)


def image_sections(pe):
    base = pe.OPTIONAL_HEADER.ImageBase
    return {s.Name.rstrip(b'\0').decode(errors='replace'): (base + s.VirtualAddress, base + s.VirtualAddress + s.Misc_VirtualSize)
            for s in pe.sections}


def find_field_tables(data, base, sections):
    """Every INI field table of the image: runs of 16-byte rows (name -> an ASCII identifier in .rdata / .data, parser -> .text, user data,
    offset < 0x10000) of two rows or more, ended by a null name. Returns {table address: [(name, parser, user data, offset), ...]}."""
    import re
    name_re = re.compile(rb'[A-Za-z][A-Za-z0-9_]{1,63}\0')
    text, named = sections['.text'], (sections['.rdata'], sections['.data'])

    def row(a):
        n, fn, ud, off = struct.unpack_from('<IIII', data, a - base)
        if not (text[0] <= fn < text[1] and off < 0x10000 and any(lo <= n < hi for lo, hi in named)):
            return None
        m = name_re.match(data, n - base)
        return (m.group()[:-1].decode(), fn, ud, off) if m else None

    tables = {}
    for lo, hi in named:
        a = lo
        while a + 32 <= hi:
            r = row(a)
            if r and (a - 16 < lo or not row(a - 16)):
                rows, b = [], a
                while b + 16 <= hi and (r := row(b)):
                    rows.append(r)
                    b += 16
                if len(rows) >= 2 and b + 4 <= hi and struct.unpack_from('<I', data, b - base)[0] == 0:
                    tables[a] = rows
                a = b
            a += 4
    return tables


def match_block_tables(fields_by_block, tables, minimum=2):
    """{(block, owner): table address} where the table names at least half (and `minimum`) of the block's audit fields, the best one."""
    out = {}
    names = {t: {r[0].lower() for r in rows} for t, rows in tables.items()}
    for key, fields in fields_by_block.items():
        best = max(tables, key=lambda t: (len(fields & names[t]), -len(names[t])))
        hit = len(fields & names[best])
        if hit >= minimum and 2 * hit >= len(fields):
            out[key] = best
    return out


def all_pointer_reads(data, base, blocks, data_range):
    """Every [reg + off] read whose base register holds a pointer loaded from a global of `data_range` (mov r32, [abs]), one nesting level
    deep (mov r32, [that + k]), or a static object's address (mov r32, imm in `data_range`); plus direct [abs] reads of `data_range`.
    Propagated along direct CFG edges as global_reads does (calls kill eax / ecx / edx, any write kills the register).
    Returns a list of (function, kind, root, path, offset, instruction): kind 'ptr' (root = the global), 'static' (root = the object), 'abs'
    (root = the absolute address, offset 0)."""
    decoder = Cs(CS_ARCH_X86, CS_MODE_32)
    decoder.detail = True
    lo, hi = data_range
    out = []
    for function, spans in blocks.items():
        decoded = {}
        for start, size in spans:
            if size <= 0 or start < base or start + size > base + len(data):
                continue
            decoded[start] = list(decoder.disasm(data[start-base:start-base+size], start))
        if function not in decoded:
            continue
        states = {function: {}}
        pending = deque([function])
        visits = defaultdict(int)
        while pending:
            start = pending.popleft()
            visits[start] += 1
            if visits[start] > 8:
                continue
            tracked = {reg: set(values) for reg, values in states[start].items()}
            for ins in decoded[start]:
                for op in ins.operands:
                    if ins.mnemonic == 'lea' or op.type != X86_OP_MEM or not (op.access & CS_AC_READ) or op.mem.index != X86_REG_INVALID:
                        continue
                    if op.mem.base == X86_REG_INVALID:
                        a = op.mem.disp & 0xffffffff
                        if lo <= a < hi:
                            out.append((function, 'abs', a, (), 0, ins.address))
                        continue
                    for kind, root, path in tracked.get(FAMILIES.get(ins.reg_name(op.mem.base)), ()):
                        out.append((function, kind, root, path, op.mem.disp, ins.address))
                copied = set()
                if ins.mnemonic == 'mov' and len(ins.operands) == 2:
                    dst, src = ins.operands
                    if dst.type == X86_OP_REG and dst.size == 4:
                        if src.type == X86_OP_MEM and src.size == 4 and src.mem.index == X86_REG_INVALID:
                            if src.mem.base == X86_REG_INVALID:
                                a = src.mem.disp & 0xffffffff
                                if lo <= a < hi:
                                    copied.add(('ptr', a, ()))
                            else:
                                for kind, root, path in tracked.get(FAMILIES.get(ins.reg_name(src.mem.base)), ()):
                                    if not path:
                                        copied.add((kind, root, (src.mem.disp,)))
                        elif src.type == X86_OP_REG and src.size == 4:
                            copied.update(tracked.get(FAMILIES.get(ins.reg_name(src.reg)), ()))
                        elif src.type == X86_OP_IMM and lo <= (src.imm & 0xffffffff) < hi:
                            copied.add(('static', src.imm & 0xffffffff, ()))
                _, writes = ins.regs_access()
                for reg in writes:
                    tracked.pop(FAMILIES.get(ins.reg_name(reg)), None)
                if ins.mnemonic == 'call':
                    for reg in VOLATILE:
                        tracked.pop(reg, None)
                if copied:
                    tracked[FAMILIES[ins.reg_name(ins.operands[0].reg)]] = copied
            last = decoded[start][-1] if decoded[start] else None
            if last is None:
                continue
            successors = set()
            if last.mnemonic.startswith('j'):
                if last.operands and last.operands[0].type == X86_OP_IMM:
                    successors.add(last.operands[0].imm)
                if last.mnemonic != 'jmp':
                    successors.add(last.address + last.size)
            elif not (last.mnemonic.startswith('ret') or last.mnemonic in {'int3', 'ud2', 'hlt'}):
                successors.add(last.address + last.size)
            for target in successors & decoded.keys():
                previous = states.get(target)
                merged = {reg: set(values) for reg, values in (previous or {}).items()}
                for reg, values in tracked.items():
                    merged.setdefault(reg, set()).update(values)
                if previous is None or merged != previous:
                    states[target] = merged
                    pending.append(target)
    return out


def instance_offsets(reads):
    """{(kind, root, path): {offset: {(function, instruction)}}}; 'abs' reads become offsets of every 'static' root below them later."""
    out = defaultdict(lambda: defaultdict(set))
    for function, kind, root, path, offset, ins in reads:
        out[(kind, root, path)][offset].add((function, ins))
    return out


def match_instance(table_rows, used_names, offsets_by_instance, statics, abs_reads, minimum=3):
    """The instance whose reads best explain the table's USED fields' offsets: score = used offsets it reads, ties by precision (its read
    offsets inside the table's span that are field offsets). A static object also takes the direct [abs] reads inside its span.
    Returns (instance, used offsets hit, precision) or None."""
    field_offsets = {r[3] for r in table_rows}
    used = {r[3] for r in table_rows if r[0].lower() in used_names}
    if not used:
        return None
    span = (min(field_offsets), max(field_offsets) + 8)
    best = None
    candidates = dict(offsets_by_instance)
    for s in statics:
        merged = defaultdict(set)
        for k, v in offsets_by_instance.get(('static', s, ()), {}).items():
            merged[k] |= v
        for a, hits in abs_reads.items():
            if s + span[0] <= a < s + span[1]:
                merged[a - s] |= hits
        candidates[('static', s, ())] = merged
    for inst, offs in candidates.items():
        inside = {o for o in offs if span[0] <= o < span[1]}
        hit = len(used & inside)
        if hit < minimum:
            continue
        precision = len(inside & field_offsets) / max(1, len(inside))
        key = (hit, precision)
        if best is None or key > best[0]:
            best = (key, inst, offs)
    if best is None or best[0][1] < 0.5:
        return None
    return best[1], best[0][0], best[0][1], best[2]


def assign_instances(matches):
    """{block: (instance, hit, precision, offsets)} -> only the blocks whose instance no other block's table also claims: a generic
    pointer or static that explains several tables' offsets (the arena's TheAudio-like roots, a shared nugget pool) proves none of them."""
    claims = defaultdict(int)
    for m in matches.values():
        claims[m[0]] += 1
    return {block: m for block, m in matches.items() if claims[m[0]] == 1}
