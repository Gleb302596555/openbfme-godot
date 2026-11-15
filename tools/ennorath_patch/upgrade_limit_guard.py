#!/usr/bin/env python3
"""Ennorath 1.9.8.3 game.dat: the upgrade-limit guard (fork issue 514, Local 69).

RotWK 2.01's upgrade masks are 0x24 dwords (1152 bits) and UpgradeCenter::newUpgrade (RW 0x66FC27) hands out the next mask bit with no
ceiling (RW 0x66FCB7 mov eax,[edi+0x10] / 0x66FCBA mov [esi+0x38],eax / 0x66FCBD inc dword ptr [edi+0x10]): upgrade number 1153 writes
past the end of every mask it is put into (the neighbouring mask, or Object+0x31C, the object's Team pointer). This patch replaces those
three instructions with a jump into a new section that does the same thing while the bit is below 0x480 and otherwise shows a message
box naming the upgrade and ends the process (ExitProcess(1)), so the load stops instead of corrupting memory. Below the limit nothing
changes: the same bit, the same counter, the same return to RW 0x66FCC0.

The input is Ennorath 1.9.8.3's pySAGE-patched game.dat (md5 below). The patch follows pySAGE's framework (sage_patch): verify every
original byte before writing, new code in a new section appended after the last one (pySAGE's .cmdext), pySAGE's sections and hooks
untouched, the PE checksum left as it is (pySAGE does not update it either).

    python upgrade_limit_guard.py <ennorath-1.9.8.3 game.dat> <output>          # writes the patched copy, prints its md5
    python upgrade_limit_guard.py --check <ennorath-1.9.8.3 game.dat> <output>  # listing before / after, pySAGE sections compared

Applying it twice, or to any other file, fails before anything is written (the md5, the hook's bytes and the section list are checked).
No bytes of the game are stored here beyond the short hook contract quoted below.
"""
from __future__ import annotations

import argparse
import hashlib
import struct
import sys
from pathlib import Path

INPUT_MD5 = "9e999515651ab72609c621d8847a8646"  # Ennorath 1.9.8.3 game.dat (pySAGE: 60 FPS, chat hotkeys, ...)
SECTION_NAME = ".upglim"
SECTION_CHARACTERISTICS = 0xE0000060  # code | initialized data | execute | read | write (the message buffer lives in it), as pySAGE's .spstore
PYSAGE_SECTIONS = (".fxrate", ".spstore", ".txtclip", ".cmdext")  # the last four sections of the input, in this order

LIMIT = 0x480  # 0x24 dwords * 32 bits: the masks' size (RW 0x66F613 push 0x90, RW 0x446152 cmp edi,0x24, RW 0x8BD7D8 cmp edi,0x480)

HOOK_VA = 0x66FCB7      # RW: mov eax,[edi+0x10] / mov [esi+0x38],eax / inc dword ptr [edi+0x10]
RESUME_VA = 0x66FCC0    # RW: push esi ; mov ecx,edi ; call 0x66F248 (link the template into the list)
HOOK_ORIGINAL = bytes.fromhex("8b4710 894638 ff4710")
# the contract around the hook (RW 0x66FCAD .. 0x66FCC8): cmp byte ptr [ebp+0xc],0 ; mov [esi+0xc],eax ; pop ebx ; je 0x66FCC0 ;
# the three instructions above ; push esi ; mov ecx,edi ; call 0x66F248
CONTEXT_VA = 0x66FCAD
CONTEXT_ORIGINAL = bytes.fromhex("807d0c00 89460c 5b 7409 8b4710 894638 ff4710 56 8bcf e880f5ffff")

# imports the guard calls through the existing IAT (msvcr71 / user32 / kernel32 of RotWK 2.01; unbound on disk)
IAT = {"wsprintfA": 0xBD07F0, "MessageBoxA": 0xBD07FC, "ExitProcess": 0xBD0268}

MB_FLAGS = 0x10 | 0x1000 | 0x10000 | 0x40000  # MB_ICONERROR | MB_SYSTEMMODAL | MB_SETFOREGROUND | MB_TOPMOST
TITLE = b"RotWK upgrade limit"
FORMAT = (b"Too many upgrades: the INI defines upgrade '%.200s' as number %u, but RotWK's upgrade masks hold only %u.\r\n\r\n"
          b"The game stops here instead of corrupting memory (game.dat patch: upgrade limit guard, fork issue 514).")

# the section's layout
OFF_CODE, OFF_FORMAT, OFF_TITLE, OFF_EMPTY, OFF_BUFFER = 0x000, 0x100, 0x200, 0x240, 0x400
BUFFER_SIZE = 0x400  # wsprintfA writes at most 1024 bytes
SECTION_SIZE = OFF_BUFFER + BUFFER_SIZE


class PatchError(ValueError):
    pass


# --- PE helpers (no third-party dependency) ------------------------------------------------------------------------------------------

def _pe(data) -> dict:
    if data[:2] != b"MZ":
        raise PatchError("not a PE file (no MZ)")
    e = struct.unpack_from("<I", data, 0x3C)[0]
    if data[e:e + 4] != b"PE\0\0":
        raise PatchError("not a PE file (no PE signature)")
    nsec, = struct.unpack_from("<H", data, e + 6)
    szopt, = struct.unpack_from("<H", data, e + 20)
    opt = e + 24
    if struct.unpack_from("<H", data, opt)[0] != 0x10B:
        raise PatchError("not a PE32 image")
    image_base, sec_align, file_align = struct.unpack_from("<III", data, opt + 28)
    size_of_image, size_of_headers = struct.unpack_from("<II", data, opt + 56)
    sectab = opt + szopt
    sections = []
    for i in range(nsec):
        o = sectab + 40 * i
        name = bytes(data[o:o + 8]).rstrip(b"\0").decode("latin1")
        vsize, rva, rsize, praw = struct.unpack_from("<IIII", data, o + 8)
        chars, = struct.unpack_from("<I", data, o + 36)
        sections.append({"name": name, "vsize": vsize, "rva": rva, "rsize": rsize, "praw": praw, "chars": chars, "hdr": o})
    return {"e": e, "opt": opt, "nsec": nsec, "sectab": sectab, "image_base": image_base, "sec_align": sec_align,
            "file_align": file_align, "size_of_image": size_of_image, "size_of_headers": size_of_headers, "sections": sections}


def _align(x: int, a: int) -> int:
    return (x + a - 1) // a * a


def va_to_off(pe: dict, va: int) -> int:
    rva = va - pe["image_base"]
    for s in pe["sections"]:
        if s["rva"] <= rva < s["rva"] + max(s["vsize"], s["rsize"]) and rva - s["rva"] < s["rsize"]:
            return s["praw"] + rva - s["rva"]
    raise PatchError(f"VA 0x{va:X} is in no section's file data")


def _import_name(data, pe: dict, iat_va: int) -> str:
    """the name an unbound IAT slot imports (the slot holds the RVA of its IMAGE_IMPORT_BY_NAME)"""
    rva, = struct.unpack_from("<I", data, va_to_off(pe, iat_va))
    if rva & 0x80000000 or not rva or rva >= pe["size_of_image"]:
        raise PatchError(f"IAT slot 0x{iat_va:X} is not an unbound import by name (0x{rva:08X})")
    o = va_to_off(pe, pe["image_base"] + rva) + 2  # skip the hint
    end = bytes(data[o:o + 64]).find(b"\0")
    return bytes(data[o:o + end]).decode("latin1") if end > 0 else ""


def _rel32(src_va: int, size: int, target: int) -> bytes:
    return struct.pack("<i", target - (src_va + size))


# --- the guard -----------------------------------------------------------------------------------------------------------------------

def guard_code(base: int) -> bytes:
    """the code at the section's start (base = its VA); see the module docstring"""
    c = bytearray()

    def at() -> int:
        return base + OFF_CODE + len(c)

    c += bytes.fromhex("8b4710")                  # mov eax,[edi+0x10]        the center's next bit (RW 0x66FCB7)
    c += b"\x3d" + struct.pack("<I", LIMIT)       # cmp eax,0x480
    c += bytes.fromhex("730b")                    # jae over                  (unsigned: a negative counter is refused too)
    c += bytes.fromhex("894638")                  # mov [esi+0x38],eax        template->bit (RW 0x66FCBA)
    c += bytes.fromhex("ff4710")                  # inc dword ptr [edi+0x10]  (RW 0x66FCBD)
    c += b"\xe9" + _rel32(at(), 5, RESUME_VA)     # jmp 0x66FCC0
    # over: the name is newUpgrade's first argument, an AsciiString& ([ebp+8], as RW 0x66FC94 pushes it); its data pointer null = "",
    # else the characters start 8 bytes in (RW 0x66F6EE .. 0x66F6FA)
    c += bytes.fromhex("8b4d08")                  # mov ecx,[ebp+8]
    c += bytes.fromhex("8b09")                    # mov ecx,[ecx]
    c += bytes.fromhex("85c9")                    # test ecx,ecx
    c += bytes.fromhex("7405")                    # jz empty
    c += bytes.fromhex("83c108")                  # add ecx,8
    c += bytes.fromhex("eb05")                    # jmp fmt
    c += b"\xb9" + struct.pack("<I", base + OFF_EMPTY)    # empty: mov ecx,""
    c += b"\x68" + struct.pack("<I", LIMIT)       # fmt: push 1152
    c += b"\x40"                                  # inc eax                   the upgrade's number (bit + 1)
    c += b"\x50"                                  # push eax
    c += b"\x51"                                  # push ecx                  name
    c += b"\x68" + struct.pack("<I", base + OFF_FORMAT)
    c += b"\x68" + struct.pack("<I", base + OFF_BUFFER)
    c += b"\xff\x15" + struct.pack("<I", IAT["wsprintfA"])    # call [wsprintfA]  (cdecl)
    c += bytes.fromhex("83c414")                  # add esp,0x14
    c += b"\x68" + struct.pack("<I", MB_FLAGS)
    c += b"\x68" + struct.pack("<I", base + OFF_TITLE)
    c += b"\x68" + struct.pack("<I", base + OFF_BUFFER)
    c += bytes.fromhex("6a00")                    # push 0 (no owner window)
    c += b"\xff\x15" + struct.pack("<I", IAT["MessageBoxA"])
    c += bytes.fromhex("6a01")                    # push 1
    c += b"\xff\x15" + struct.pack("<I", IAT["ExitProcess"])
    c += b"\xcc"                                  # int3: ExitProcess does not return
    assert len(c) <= OFF_FORMAT
    return bytes(c)


def section_bytes(base: int) -> bytes:
    s = bytearray(SECTION_SIZE)
    code = guard_code(base)
    s[OFF_CODE:OFF_CODE + len(code)] = code
    assert len(FORMAT) + 1 <= OFF_TITLE - OFF_FORMAT and len(TITLE) + 1 <= OFF_EMPTY - OFF_TITLE
    s[OFF_FORMAT:OFF_FORMAT + len(FORMAT)] = FORMAT
    s[OFF_TITLE:OFF_TITLE + len(TITLE)] = TITLE
    return bytes(s)


def hook_bytes(base: int) -> bytes:
    return b"\xe9" + _rel32(HOOK_VA, 5, base + OFF_CODE) + b"\x90" * (len(HOOK_ORIGINAL) - 5)


def check_input(data, expected_md5: str | None = INPUT_MD5) -> dict:
    """every condition the patch needs, before anything is written; returns the parsed PE"""
    if expected_md5 is not None:
        got = hashlib.md5(bytes(data)).hexdigest()
        if got != expected_md5:
            raise PatchError(f"md5 {got} is not Ennorath 1.9.8.3's game.dat ({expected_md5}); refusing (patched already, or another file)")
    pe = _pe(data)
    names = [s["name"] for s in pe["sections"]]
    if SECTION_NAME in names:
        raise PatchError(f"{SECTION_NAME} is already there: the guard is applied")
    if tuple(names[-len(PYSAGE_SECTIONS):]) != PYSAGE_SECTIONS:
        raise PatchError(f"the last sections are {names[-4:]}, not pySAGE's {list(PYSAGE_SECTIONS)}")
    got = bytes(data[va_to_off(pe, CONTEXT_VA):va_to_off(pe, CONTEXT_VA) + len(CONTEXT_ORIGINAL)])
    if got != CONTEXT_ORIGINAL:
        raise PatchError(f"newUpgrade @0x{CONTEXT_VA:X}: expected {CONTEXT_ORIGINAL.hex()} got {got.hex()}")
    for name, va in IAT.items():
        imported = _import_name(data, pe, va)
        if imported != name:
            raise PatchError(f"IAT 0x{va:X}: expected {name}, the slot imports {imported!r}")
    if pe["sectab"] + 40 * (pe["nsec"] + 1) > pe["size_of_headers"]:
        raise PatchError("no room in the PE header for another section")
    return pe


def apply(data: bytearray, expected_md5: str | None = INPUT_MD5) -> int:
    """patch `data` in place; returns the new section's VA. Raises PatchError before changing anything"""
    pe = check_input(data, expected_md5)
    last = max(pe["sections"], key=lambda s: s["rva"])
    rva = _align(last["rva"] + max(last["vsize"], 1), pe["sec_align"])
    base = pe["image_base"] + rva
    content = section_bytes(base)
    raw_size = _align(len(content), pe["file_align"])
    praw = _align(len(data), pe["file_align"])
    hdr = bytearray(40)
    hdr[0:8] = SECTION_NAME.encode("ascii").ljust(8, b"\0")
    struct.pack_into("<IIII", hdr, 8, len(content), rva, raw_size, praw)
    struct.pack_into("<I", hdr, 36, SECTION_CHARACTERISTICS)
    o = pe["sectab"] + 40 * pe["nsec"]
    if any(data[o:o + 40]):
        raise PatchError("the header slot after the last section is not empty")
    # all checks passed: write
    data[o:o + 40] = hdr
    struct.pack_into("<H", data, pe["e"] + 6, pe["nsec"] + 1)
    struct.pack_into("<I", data, pe["opt"] + 56, _align(rva + len(content), pe["sec_align"]))
    if len(data) < praw:
        data += b"\0" * (praw - len(data))
    data += content + b"\0" * (raw_size - len(content))
    hook = va_to_off(pe, HOOK_VA)
    data[hook:hook + len(HOOK_ORIGINAL)] = hook_bytes(base)
    return base


def verify(data) -> list[str]:
    """the problems that mean `data` does not carry the guard (empty == carries it)"""
    try:
        pe = _pe(data)
    except PatchError as e:
        return [str(e)]
    sec = next((s for s in pe["sections"] if s["name"] == SECTION_NAME), None)
    if sec is None:
        return [f"no {SECTION_NAME} section"]
    base = pe["image_base"] + sec["rva"]
    out = []
    if bytes(data[sec["praw"]:sec["praw"] + SECTION_SIZE]) != section_bytes(base):
        out.append(f"{SECTION_NAME} content differs")
    h = va_to_off(pe, HOOK_VA)
    if bytes(data[h:h + len(HOOK_ORIGINAL)]) != hook_bytes(base):
        out.append(f"hook @0x{HOOK_VA:X} differs")
    return out


# --- CLI -----------------------------------------------------------------------------------------------------------------------------

def _listing(data, pe: dict, va: int, n: int) -> list[str]:
    import capstone  # only --check needs it
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    o = va_to_off(pe, va)
    out = []
    for ins in md.disasm(bytes(data[o:o + n * 16]), va):
        out.append(f"{ins.address:08X}  {ins.bytes.hex():<16} {ins.mnemonic:8s} {ins.op_str}")
        if len(out) >= n:
            break
    return out


def check_report(src: bytes, dst: bytes) -> list[str]:
    """the --check listing: the hook site before / after, the new section's code, pySAGE's sections compared"""
    pi, po = _pe(src), _pe(dst)
    lines = ["before (input):"] + _listing(src, pi, CONTEXT_VA, 10)
    lines += ["after (output):"] + _listing(dst, po, CONTEXT_VA, 12)
    sec = next(s for s in po["sections"] if s["name"] == SECTION_NAME)
    base = po["image_base"] + sec["rva"]
    lines += [f"{SECTION_NAME} @ VA 0x{base:X} (raw 0x{sec['praw']:X}, vsize 0x{sec['vsize']:X}, characteristics 0x{sec['chars']:08X}):"]
    lines += _listing(dst, po, base, 33)
    for s in pi["sections"]:
        t = next((x for x in po["sections"] if x["name"] == s["name"]), None)
        same_hdr = t is not None and all(s[k] == t[k] for k in ("vsize", "rva", "rsize", "praw", "chars"))
        a = src[s["praw"]:s["praw"] + s["rsize"]]
        b = dst[t["praw"]:t["praw"] + t["rsize"]] if t else b""
        if s["name"] == ".text":
            h = va_to_off(pi, HOOK_VA) - s["praw"]
            diff = [i for i in range(len(a)) if i < len(b) and a[i] != b[i]] if a != b else []
            same = bool(diff) and min(diff) >= h and max(diff) < h + len(HOOK_ORIGINAL)
            lines.append(f"section {s['name']:<9} header {'same' if same_hdr else 'CHANGED'}; data differs only in the 9 hook bytes: {same}")
        else:
            lines.append(f"section {s['name']:<9} header {'same' if same_hdr else 'CHANGED'}; data {'identical' if a == b else 'CHANGED'}")
    return lines


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("input", type=Path, help="Ennorath 1.9.8.3's game.dat (md5 %s)" % INPUT_MD5)
    ap.add_argument("output", type=Path, help="the patched copy (must not be the input)")
    ap.add_argument("--check", action="store_true", help="also print the before / after listing (needs capstone)")
    a = ap.parse_args(argv)
    if a.output.resolve() == a.input.resolve():
        raise SystemExit("the output must be a new file: the input stays untouched")
    src = a.input.read_bytes()
    data = bytearray(src)
    try:
        base = apply(data)
    except PatchError as e:
        raise SystemExit(f"refused: {e}")
    problems = verify(data)
    if problems:
        raise SystemExit(f"internal error, not written: {problems}")
    a.output.write_bytes(data)
    print(f"wrote {a.output}: {len(src)} -> {len(data)} bytes, {SECTION_NAME} at VA 0x{base:X}, md5 {hashlib.md5(data).hexdigest()}")
    if a.check:
        print("\n".join(check_report(src, bytes(data))))
    return 0


if __name__ == "__main__":
    sys.exit(main())
