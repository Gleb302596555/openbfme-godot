"""pysage_dump.py: the pySAGE sections of a mod's patched game.dat, disassembled for a Local / Port task (owner 2026-11-22).

Ennorath patches its RotWK game.dat with pySAGE: new sections (.chatmut, .mapfind, .optui, ...) and `jmp` / `call` hooks into them from the
retail code. The dossier (tools/mod/dossier_disasm.py) covers the vanilla 2.01 functions only, so an agent porting a pySAGE edit as an
`ennorath.*` feature of a ModEngineFeatures profile (docs/ennorath/ENNORATH_EXE.md) gets this file instead:

    python tools/mod/pysage_dump.py <patched game.dat> <vanilla 2.01 game.dat> <out.txt> .sec1,.sec2 <hook RW hex,...> [<.rdata RW hex,...>] [--ver 1.9.8.5]

Code is found recursively across all named sections at once: from the hook sites' `jmp` / `call` targets, the 8 dwords at each .rdata
spot, leading pointer tables, jumps / calls between the sections and function addresses taken as values (text-looking bytes refused).
Every instruction is printed as `PYSAGE <ver> <section> 0x<addr>` (the citation form of lane MODS-3); what is not code is hex-dumped,
followed by which instructions use which of the section's data; then each hook site and .rdata spot, vanilla against patched.
The output is mod binary text: it goes to the agent's sandbox (/content/sandbox/work-dossier-<issue>-<name>.txt) and Drive
openbfme-runs/dossiers/, never to GitHub in plain text (as the dossiers).
"""
from __future__ import annotations

import re
import struct
import sys

VERSION = "1.9.8.5"


def pe(path):
    d = open(path, "rb").read()
    o = struct.unpack_from("<I", d, 0x3C)[0]; n = struct.unpack_from("<H", d, o + 6)[0]; opt = struct.unpack_from("<H", d, o + 20)[0]
    base = struct.unpack_from("<I", d, o + 24 + 28)[0]; secs = []
    for i in range(n):
        s = o + 24 + opt + 40 * i
        name = d[s:s + 8].rstrip(b"\0").decode("latin1"); vsz, va, rsz, raw = struct.unpack_from("<IIII", d, s + 8)
        secs.append((name, base + va, vsz, raw, rsz))
    return d, base, secs
def read(d, secs, addr, size):
    for name, va, vsz, raw, rsz in secs:
        if va <= addr < va + max(vsz, rsz):
            off = raw + addr - va
            return d[off:off + size]
    raise KeyError(hex(addr))
def dump(pat: str, van: str, out: str, SECS: list[str], SITES: list[int], SPOTS: list[int], ver: str = VERSION) -> dict[str, int]:
    """Writes the listing to `out`; returns code bytes found per section."""
    import capstone
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    P, pb, ps = pe(pat); V, vb, vs = pe(van)
    sec = {s[0]: s for s in ps if s[0] in SECS}
    missing = [n for n in SECS if n not in sec]
    data = {n: read(P, ps, sec[n][1], sec[n][2]) for n in sec}
    def where(a):
        for n, s in sec.items():
            if s[1] <= a < s[1] + s[2]:
                return n
        return None
    entries, why = set(), {}
    def add(a, reason):
        if where(a) and a not in entries:
            entries.add(a); why[a] = reason
    for site in SITES:
        ins = next(md.disasm(read(P, ps, site, 16), site), None)
        if ins and ins.mnemonic in ("jmp", "call") and ins.op_str.startswith("0x"):
            add(int(ins.op_str, 16), f"hook RW 0x{site:X}")
    for spot in SPOTS:
        for k in range(8):
            v = struct.unpack_from("<I", read(P, ps, spot + 4 * k, 4))[0]
            add(v, f".rdata RW 0x{spot + 4 * k:X}")
    # leading pointer tables
    for n, s in sec.items():
        d = data[n]
        for off in range(0, len(d) - 3, 4):
            v = struct.unpack_from("<I", d, off)[0]
            if where(v) != n:
                break
            add(v, f"table {n} 0x{s[1] + off:X}")
    seen = {}
    changed = True
    while changed:
        changed = False
        todo = sorted(entries - set(seen))
        while todo:
            a = todo.pop()
            while where(a) and a not in seen:
                n = where(a); lo = sec[n][1]
                ins = next(md.disasm(data[n][a - lo:a - lo + 16], a), None)
                if ins is None:
                    break
                seen[a] = ins
                for t in re.findall(r"0x[0-9a-f]+", ins.op_str):
                    t = int(t, 16)
                    if where(t) and (ins.mnemonic.startswith("j") or ins.mnemonic == "call"):
                        if t not in entries: add(t, f"{ins.mnemonic} from 0x{a:X}"); changed = True
                        todo.append(t)
                    elif where(t) and ins.mnemonic in ("push", "mov") and "ptr" not in ins.op_str:
                        # an address taken as a value: a callback if it starts like code
                        head = next(md.disasm(data[where(t)][t - sec[where(t)][1]:t - sec[where(t)][1] + 8], t), None)
                        raw = data[where(t)][t - sec[where(t)][1]:t - sec[where(t)][1] + 8]
                        texty = all(32 <= c < 127 for c in raw[:6]) or (len(raw) >= 6 and all(raw[i] == 0 for i in (1, 3, 5)) and all(32 <= raw[i] < 127 for i in (0, 2, 4)))
                        if head and not texty and (head.mnemonic in ("push", "pushal", "sub", "mov") and head.op_str.split(",")[0] in ("ebp", "", "esp", "edi", "esi", "ebx")):
                            if t not in entries: add(t, f"address taken at 0x{a:X}"); changed = True
                            todo.append(t)
                if ins.mnemonic in ("ret", "jmp", "retn") or (ins.mnemonic == "jmp"):
                    break
                a += ins.size
    import hashlib
    L = [f"# patched game.dat {ver} (md5 {hashlib.md5(open(pat, 'rb').read()).hexdigest()}): pySAGE sections {', '.join(SECS)}",
         f"# vanilla game.dat (md5 {hashlib.md5(open(van, 'rb').read()).hexdigest()}) for the hook sites; capstone {capstone.__version__}; code found recursively from hooks,"
         " .rdata pointers, pointer tables, calls / jumps between the sections and addresses taken as values"]
    if missing:
        L.append(f"# sections not in this game.dat: {', '.join(missing)}")
    for n in SECS:
        if n not in sec:
            continue
        va, vsz = sec[n][1], sec[n][2]; d = data[n]
        L.append(f"\n## {n}: VA 0x{va:X}, virtual size 0x{vsz:X}")
        es = sorted(e for e in entries if where(e) == n)
        L.append("### entry points: " + ("; ".join(f"0x{e:08X} ({why[e]})" for e in es) or "none"))
        covered = set()
        prev = None
        for a in sorted(x for x in seen if where(x) == n):
            ins = seen[a]
            if prev is not None and a != prev:
                L.append("")
            L.append(f"PYSAGE {ver} {n} 0x{a:08X}:  {ins.bytes.hex():<20} {ins.mnemonic} {ins.op_str}" + ("  ; <== entry" if a in entries else ""))
            covered.update(range(a, a + ins.size)); prev = a + ins.size
        rest = [a for a in range(va, va + vsz) if a not in covered]
        if rest:
            L.append(f"\n### not code: {len(rest)} bytes (data / strings / zero storage), hex dump of those rows")
            rows = sorted({a - (a - va) % 16 for a in rest})
            for row in rows:
                b = d[row - va:min(row + 16, va + vsz) - va]
                L.append(f"PYSAGE {ver} {n} 0x{row:08X}:  {b.hex(' '):<47}  " + "".join(chr(c) if 32 <= c < 127 else "." for c in b))
        # data references into this section from code
        refs = {}
        for a, ins in seen.items():
            for t in re.findall(r"0x[0-9a-f]+", ins.op_str):
                t = int(t, 16)
                if where(t) == n and t not in covered and not ins.mnemonic.startswith("j") and ins.mnemonic != "call":
                    refs.setdefault(t, []).append(f"0x{a:08X} {ins.mnemonic} {ins.op_str}")
        if refs:
            L.append(f"\n### data of {n} used by the code")
            for t in sorted(refs):
                L.append(f"0x{t:08X} ({n} + 0x{t - va:X}): " + "; ".join(refs[t][:12]) + (f" (+{len(refs[t]) - 12})" if len(refs[t]) > 12 else ""))
    for site in SITES:
        L.append(f"\n## hook site RW 0x{site:X} (32 bytes from the site)")
        for label, d, s in (("vanilla 2.01", V, vs), (f"patched {ver}", P, ps)):
            L.append(f"-- {label}")
            for ins in md.disasm(read(d, s, site, 32), site):
                L.append(f"   0x{ins.address:08X}:  {ins.bytes.hex():<20} {ins.mnemonic} {ins.op_str}" + ("  <== site" if ins.address == site else ""))
    for spot in SPOTS:
        L.append(f"\n## .rdata RW 0x{spot:X} (8 dwords)")
        for label, d, s in (("vanilla 2.01", V, vs), (f"patched {ver}", P, ps)):
            L.append(f"   {label:<17}: " + " ".join("0x%08X" % x for x in struct.unpack("<8I", read(d, s, spot, 32))))
    open(out, "w", encoding="utf-8").write("\n".join(L) + "\n")
    return {n: sum(ins.size for a, ins in seen.items() if where(a) == n) for n in SECS if n in sec}



def main(argv: list[str]) -> int:
    if len(argv) < 5:
        print(__doc__)
        return 2
    ver = VERSION
    if "--ver" in argv:
        k = argv.index("--ver")
        ver = argv[k + 1]
        argv = argv[:k] + argv[k + 2:]
    hexes = lambda s: [int(x, 16) for x in s.split(",") if x]
    found = dump(argv[0], argv[1], argv[2], argv[3].split(","), hexes(argv[4]), hexes(argv[5]) if len(argv) > 5 else [], ver)
    for name, n in found.items():
        print(f"{name}: {n} bytes as code")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
