"""Tests of binary_map.py / class_map.py on hand-assembled x86 (no retail bytes): run with `python -m pytest tools/rw_object_model`."""
import struct
import sys
from pathlib import Path

import capstone
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import binary_map as bm  # noqa: E402
import class_map as cm  # noqa: E402

BASE = 0x400000
TEXT = (0x401000, 0x402000)
DATA = (0x402000, 0x403000)


def fake(code: dict[int, bytes], data: dict[int, bytes]) -> bm.Binary:
    """A Binary over a synthetic image: .text 0x401000.., .rdata 0x402000.."""
    b = bm.Binary.__new__(bm.Binary)
    img = bytearray(0x3000)
    for va, bs in list(code.items()) + list(data.items()):
        img[va - BASE:va - BASE + len(bs)] = bs
    b.base, b.image, b.text, b.data_ranges = BASE, bytes(img), TEXT, [DATA]
    b.entry, b.imports, b.sha256, b.sections = TEXT[0], {}, "test", {}
    b.md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    b.md.detail = True
    return b


def dd(*vals):
    return b"".join(struct.pack("<I", v) for v in vals)


def rel32(at, size, target):
    return struct.pack("<i", target - (at + size))


# 0x401000: mov eax, ecx; mov dword ptr [eax], 0x402000; lea ecx, [eax + 4]; call 0x401020; ret
CTOR = bytes.fromhex("8BC1") + bytes.fromhex("C700") + dd(0x402000) + bytes.fromhex("8D4804") + b"\xE8" + rel32(0x40100B, 5, 0x401020) + b"\xC3"
# 0x401040: cmp eax, 2; ja 0x40104C; jmp [eax*4 + 0x402100]; ret; ret; ret; ret
SWITCH = bytes.fromhex("83F802") + bytes.fromhex("7707") + bytes.fromhex("FF2485") + dd(0x402100) + b"\xC3\xC3\xC3\xC3"


def image():
    return fake({0x401000: CTOR, 0x401020: b"\xC3", 0x401040: SWITCH},
                {0x402000: dd(0x401020, 0x401020), 0x402100: dd(0x40104D, 0x40104E, 0x40104F, 0xDEADBEEF)})


def test_switch_bound_is_proven_by_its_guard():
    b = image()
    owned, calls, tails, refs, unresolved, returns, pops, installs = bm.decode_function(b, 0x401040, {0x401040}, set())
    assert unresolved == []
    assert {0x40104C, 0x40104D, 0x40104E, 0x40104F} <= set(owned)  # three cases and the default; the fourth word is not read


def test_switch_without_a_guard_is_listed_not_followed():
    b = fake({0x401040: bytes.fromhex("FF2485") + dd(0x402100)}, {0x402100: dd(0x401050)})
    owned, calls, tails, refs, unresolved, returns, pops, installs = bm.decode_function(b, 0x401040, {0x401040}, set())
    assert unresolved == [0x401040] and 0x401050 not in owned


def test_misaligned_extra_block_is_refused():
    b = image()
    with pytest.raises(bm.MapError, match="misaligned"):
        bm.decode_function(b, 0x401000, {0x401000, 0x401020}, set(), extra=(0x401003,))  # inside `mov [eax], imm`


def test_installed_table_is_recorded():
    b = image()
    res = bm.decode_function(b, 0x401000, {0x401000, 0x401020}, set())
    assert res[7] == {0x402000} and res[1] == {0x401020} and res[6] == {0}


def test_null_join_keeps_the_pointer():
    x = ("ret", 0x401000, 0x401234)
    assert cm.join(cm.NULL, x) == x and cm.join(x, cm.NULL) == x
    assert cm.join(x, ("this",)) is None and cm.join(("this",), ("this",)) == ("this",)


def test_ctor_facts_install_on_this_and_base_call_on_this_plus_4():
    b = image()
    f = cm.facts_of(b, 0x401000, [(0x401000, len(CTOR))], {0x401020: {0}})
    assert f["vt"] == [(0x401002, ("this",), 0, 0x402000)]
    assert f["calls"] == [(0x40100B, 0x401020, ("add", ("this",), 4))]
    assert f["rets"] == [("ret", 0x401020, 0x40100B)]
