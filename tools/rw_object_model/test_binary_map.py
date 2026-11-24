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


def test_binary_report_proven_glue_non_code_and_verdicts(tmp_path):
    import binary_report as br
    from collections import Counter

    # Synthetic image:
    # 0x401000: real function using _EH_prolog (mov eax, 0x401020; call 0x401080; ret)
    # 0x401020: 10B __ehhandler$ (mov eax, 0x402000; jmp 0x401090)
    # 0x401030: unwind funclet referenced by FuncInfo.pUnwindMap at 0x402020 (ret)
    # 0x401040: real ~Class() dtor (ret)
    # 0x401050: scalar deleting dtor wrapper calling 0x401040 and delete 0x4010A0 (ret 4)
    # 0x401080: _EH_prolog stub (ret)
    # 0x401090: __CxxFrameHandler stub (ret)
    # 0x4010A0: operator delete stub (ret)
    # 0x4010B0: phantom data entry with iretd (impossible in user-mode code)
    # 0x4010C0: real data callback (ret) - MUST stay none!
    # 0x4010D0: verdict `not reached` with evidence -> not-reached
    # 0x4010E0: verdict `too big` -> MUST stay none!
    # 0x4010F0: verdict `not reached` WITHOUT evidence -> MUST stay none!
    def rel32(src_end, dst):
        return struct.pack("<i", dst - src_end)

    fn_prolog = b"\xB8" + dd(0x401020) + b"\xE8" + rel32(0x40100A, 0x401080) + b"\xC3"
    eh_handler = b"\xB8" + dd(0x402000) + b"\xE9" + rel32(0x40102A, 0x401090)
    # push esi; mov esi, ecx; call 0x401040; test byte ptr [esp+8], 1; je +7; push esi; call 0x4010A0; pop ecx; mov eax, esi; pop esi; ret 4
    sdd = (
        bytes.fromhex("56 8B F1 E8") + rel32(0x401058, 0x401040)
        + bytes.fromhex("F6 44 24 08 01 74 07 56 E8") + rel32(0x401065, 0x4010A0)
        + bytes.fromhex("59 8B C6 5E C2 04 00")
    )
    func_info = dd(0x19930520, 1, 0x402020)
    unwind_map = struct.pack("<iI", -1, 0x401030)

    b = fake(
        {
            0x401000: fn_prolog,
            0x401020: eh_handler,
            0x401030: b"\xC3",
            0x401040: b"\xC3",
            0x401050: sdd,
            0x401080: b"\xC3",
            0x401090: b"\xC3",
            0x4010A0: b"\xC3",
            0x4010B0: b"\xCF",  # iretd
            0x4010C0: b"\xC3",
            0x4010D0: b"\xC3",
            0x4010E0: b"\xC3",
            0x4010F0: b"\xC3",
        },
        {0x402000: func_info, 0x402020: unwind_map},
    )
    rec = {
        "functions": [
            {"address": 0x401000, "size": len(fn_prolog), "evidence": "call", "calls": [0x401080], "tails": []},
            {"address": 0x401020, "size": len(eh_handler), "evidence": "code-imm", "calls": [], "tails": [0x401090]},
            {"address": 0x401030, "size": 1, "evidence": "data", "calls": [], "tails": []},
            {"address": 0x401040, "size": 1, "evidence": "call", "calls": [], "tails": []},
            {"address": 0x401050, "size": len(sdd), "evidence": "vtable-slot", "calls": [0x401040, 0x4010A0], "tails": []},
            {"address": 0x401080, "size": 1, "evidence": "call", "calls": [], "tails": []},
            {"address": 0x401090, "size": 1, "evidence": "call", "calls": [], "tails": []},
            {"address": 0x4010A0, "size": 1, "evidence": "call", "calls": [], "tails": []},
            {"address": 0x4010B0, "size": 1, "evidence": "data", "calls": [], "tails": []},
            {"address": 0x4010C0, "size": 1, "evidence": "data", "calls": [], "tails": []},
            {"address": 0x4010D0, "size": 1, "evidence": "call", "calls": [], "tails": []},
            {"address": 0x4010E0, "size": 1, "evidence": "call", "calls": [], "tails": []},
            {"address": 0x4010F0, "size": 1, "evidence": "call", "calls": [], "tails": []},
        ]
    }
    xmap = {0x4010A0: ("??3@YAXPAX@Z", "library", "A")}

    assert br.proven_seh_glue(b, rec) == {0x401020, 0x401030}
    assert br.proven_deleting_dtors(b, rec, xmap) == {0x401050}
    # Wrapped ~Class() at 0x401040 is NOT excluded:
    assert 0x401040 not in br.proven_deleting_dtors(b, rec, xmap)

    tier = {fn["address"]: fn["evidence"] for fn in rec["functions"]}
    nc = br.proven_non_code(b, rec, tier, Counter(), Counter(), {})
    assert nc == {0x4010B0}
    assert 0x4010C0 not in nc  # real data callback stays untouched!

    vdir = tmp_path / "rw_verdicts.d"
    vdir.mkdir()
    (vdir / "LOCAL-999.tsv").write_text(
        "function\tname\tverdict\tevidence\n"
        "0x4010D0\tfnD\tnot reached\t0 callers in map\n"
        "0x4010E0\tfnE\ttoo big\tneeds whole subsystem\n"
        "0x4010F0\tfnF\tnot reached\t\n",
        encoding="utf-8",
    )
    nr = br.load_not_reached_verdicts(vdir)
    assert nr == {0x4010D0}
    assert 0x4010E0 not in nr  # too big MUST remain open
    assert 0x4010F0 not in nr  # verdict without evidence MUST remain open
