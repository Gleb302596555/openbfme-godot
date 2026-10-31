"""rw.py <game.dat> <cmd> ...
  dis <va> [n]        disassemble n instructions (default 60)
  xref <va>           calls / jumps (rel32) to va, and instructions with va as an immediate or displacement
  str <text>          addresses of an ASCII string (and their xrefs)
  float <value>       addresses of a float32 constant (and their xrefs)
"""
import struct
import sys

import capstone
import pefile

path, cmd, args = sys.argv[1], sys.argv[2], sys.argv[3:]
pe = pefile.PE(path, fast_load=True)
base = pe.OPTIONAL_HEADER.ImageBase
data = pe.get_memory_mapped_image()
text = next(s for s in pe.sections if s.Name.rstrip(b"\0") == b".text")
tstart, tend = text.VirtualAddress, text.VirtualAddress + text.Misc_VirtualSize
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
md.detail = False


def dis(va, n):
    off = va - base
    out = []
    for ins in md.disasm(data[off:off + n * 16], va):
        out.append(f"{ins.address:08X}  {ins.mnemonic:8s} {ins.op_str}")
        if len(out) >= n:
            break
    return out


def xref(target):
    hits = []
    t = data[tstart:tend]
    # rel32 calls / jumps
    for i in range(len(t) - 5):
        op = t[i]
        if op in (0xE8, 0xE9):
            rel = struct.unpack_from("<i", t, i + 1)[0]
            src = base + tstart + i
            if src + 5 + rel == target:
                hits.append((src, "call" if op == 0xE8 else "jmp"))
    # the address as a 32-bit value anywhere in .text (imm / disp)
    needle = struct.pack("<I", target)
    i = t.find(needle)
    while i >= 0:
        hits.append((base + tstart + i, "imm"))
        i = t.find(needle, i + 1)
    return hits


def find_bytes(b):
    res = []
    i = data.find(b)
    while i >= 0:
        res.append(base + i)
        i = data.find(b, i + 1)
    return res


if cmd == "dis":
    print("\n".join(dis(int(args[0], 16), int(args[1]) if len(args) > 1 else 60)))
elif cmd == "xref":
    for src, kind in xref(int(args[0], 16)):
        print(f"{src:08X} {kind}")
elif cmd in ("str", "float"):
    b = args[0].encode() + b"\0" if cmd == "str" else struct.pack("<f", float(args[0]))
    for a in find_bytes(b)[:20]:
        print(f"at {a:08X}")
        for src, kind in xref(a)[:20]:
            print(f"   {src:08X} {kind}")
