"""Tests of the upgrade-limit guard builder (fork issue 514, Local 69) on a synthetic PE that carries only the quoted hook contract.

The emulator tests (Unicorn, as pySAGE uses) run the hooked x86 on synthetic templates; they skip where unicorn is not installed (CI).
With ENNORATH_1983_GAME_DAT naming Ennorath 1.9.8.3's game.dat, the last test also patches the real file in memory.
"""
from __future__ import annotations

import hashlib
import os
import struct
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import upgrade_limit_guard as g  # noqa: E402

BASE = 0x400000
TEXT_RVA, TEXT_SIZE = 0x1000, 0x270000           # covers the hook at RW 0x66FCB7 (RVA 0x26FCB7)
RDATA_RVA, RDATA_SIZE = 0x7D0000, 0x1000         # the IAT slots RW 0xBD0268 / 0xBD07F0 / 0xBD07FC
PYSAGE = [(".fxrate", 0x60000060), (".spstore", 0xE0000060), (".txtclip", 0x60000060), (".cmdext", 0x60000060)]


def synthetic_pe(last_sections=PYSAGE, imports=None) -> bytearray:
    """a minimal PE32 with RotWK's ImageBase, a .text holding the hook contract, an .rdata holding three unbound IAT slots and
    pySAGE's four sections after them"""
    imports = imports or {va: name for name, va in g.IAT.items()}
    secs = [(".text", TEXT_RVA, TEXT_SIZE, 0x60000020), (".rdata", RDATA_RVA, RDATA_SIZE, 0x40000040)]
    rva = RDATA_RVA + RDATA_SIZE
    for name, ch in last_sections:
        secs.append((name, rva, 0x200, ch))
        rva += 0x1000
    data = bytearray(0x1000)
    data[0:2] = b"MZ"
    struct.pack_into("<I", data, 0x3C, 0x80)
    data[0x80:0x84] = b"PE\0\0"
    struct.pack_into("<HHIIIHH", data, 0x84, 0x14C, len(secs), 0, 0, 0, 224, 0x10F)
    opt = 0x98
    struct.pack_into("<H", data, opt, 0x10B)
    struct.pack_into("<III", data, opt + 28, BASE, 0x1000, 0x200)
    struct.pack_into("<II", data, opt + 56, rva, 0x1000)
    sectab = opt + 224
    praw = 0x1000
    for i, (name, srva, size, ch) in enumerate(secs):
        o = sectab + 40 * i
        data[o:o + 8] = name.encode().ljust(8, b"\0")
        struct.pack_into("<IIII", data, o + 8, size, srva, size, praw)
        struct.pack_into("<I", data, o + 36, ch)
        body = bytearray(size)
        if name in dict(last_sections):
            body[:8] = name.encode().ljust(8, b"\0")  # some content to compare
        data += body
        praw += size
    c = g.CONTEXT_VA - BASE - TEXT_RVA + 0x1000
    data[c:c + len(g.CONTEXT_ORIGINAL)] = g.CONTEXT_ORIGINAL
    rdata_raw = 0x1000 + TEXT_SIZE
    hint = 0x900
    for va, name in imports.items():
        struct.pack_into("<I", data, rdata_raw + va - BASE - RDATA_RVA, RDATA_RVA + hint)
        data[rdata_raw + hint + 2:rdata_raw + hint + 2 + len(name) + 1] = name.encode() + b"\0"
        hint += 0x40
    return data


def sections(data):
    return g._pe(data)["sections"]


def test_contract_constants_agree():
    # the three replaced instructions sit inside the quoted context, 10 bytes in, and the hook resumes right after them
    assert g.CONTEXT_ORIGINAL[g.HOOK_VA - g.CONTEXT_VA:][:len(g.HOOK_ORIGINAL)] == g.HOOK_ORIGINAL
    assert g.HOOK_VA + len(g.HOOK_ORIGINAL) == g.RESUME_VA
    assert g.LIMIT == 0x24 * 32 == 1152


def test_apply_appends_the_section_after_cmdext_and_hooks_newupgrade():
    src = synthetic_pe()
    data = bytearray(src)
    base = g.apply(data, expected_md5=None)
    before, after = sections(src), sections(data)
    assert [s["name"] for s in after] == [s["name"] for s in before] + [g.SECTION_NAME]
    new = after[-1]
    cmdext = before[-1]
    assert new["rva"] == cmdext["rva"] + 0x1000 and base == BASE + new["rva"]
    assert new["praw"] == len(src) and new["chars"] == g.SECTION_CHARACTERISTICS and new["vsize"] == g.SECTION_SIZE
    assert g._pe(data)["size_of_image"] == new["rva"] + 0x1000
    # pySAGE's sections (and every other one): same header, same bytes
    for s, t in zip(before, after):
        assert {k: s[k] for k in ("name", "vsize", "rva", "rsize", "praw", "chars")} == {k: t[k] for k in ("name", "vsize", "rva", "rsize", "praw", "chars")}
        if s["name"] != ".text":
            assert src[s["praw"]:s["praw"] + s["rsize"]] == data[t["praw"]:t["praw"] + t["rsize"]]
    # .text differs in exactly the 9 hook bytes: jmp rel32 to the section + 4 nops
    h = g.va_to_off(g._pe(src), g.HOOK_VA)
    diff = [i for i in range(len(src)) if src[i] != data[i]]
    assert all(h <= i < h + 9 or i < 0x1000 for i in diff)
    assert data[h] == 0xE9 and struct.unpack_from("<i", data, h + 1)[0] == base - (g.HOOK_VA + 5) and data[h + 5:h + 9] == b"\x90" * 4
    assert g.verify(data) == [] and g.verify(src) == [f"no {g.SECTION_NAME} section"]


def test_section_content_resumes_at_0x66fcc0_and_carries_the_strings():
    base = 0xED7000
    s = g.section_bytes(base)
    code = g.guard_code(base)
    assert s[:len(code)] == code and code[:3] == g.HOOK_ORIGINAL[:3]
    j = code.index(b"\xe9")
    assert struct.unpack_from("<i", code, j + 1)[0] == g.RESUME_VA - (base + j + 5)
    assert s[g.OFF_FORMAT:g.OFF_FORMAT + len(g.FORMAT) + 1] == g.FORMAT + b"\0"
    assert s[g.OFF_TITLE:g.OFF_TITLE + len(g.TITLE) + 1] == g.TITLE + b"\0"
    assert s[g.OFF_EMPTY] == 0 and s[g.OFF_BUFFER:] == bytes(g.BUFFER_SIZE)
    for name, va in g.IAT.items():
        assert b"\xff\x15" + struct.pack("<I", va) in code, name


def test_refuses_a_file_that_is_not_ennorath_1983():
    data = synthetic_pe()
    keep = bytes(data)
    with pytest.raises(g.PatchError, match="md5"):
        g.apply(data)  # the default: Ennorath 1.9.8.3's md5
    assert bytes(data) == keep


def test_refuses_a_changed_hook_before_writing():
    data = synthetic_pe()
    h = g.va_to_off(g._pe(data), g.HOOK_VA)
    data[h + 4] ^= 0xFF
    keep = bytes(data)
    with pytest.raises(g.PatchError, match="newUpgrade @0x66FCAD"):
        g.apply(data, expected_md5=None)
    assert bytes(data) == keep


def test_refuses_a_second_application():
    data = synthetic_pe()
    g.apply(data, expected_md5=None)
    keep = bytes(data)
    with pytest.raises(g.PatchError, match="already"):
        g.apply(data, expected_md5=None)
    assert bytes(data) == keep


def test_refuses_without_pysage_sections_last():
    data = synthetic_pe(last_sections=PYSAGE[:3])
    with pytest.raises(g.PatchError, match="pySAGE"):
        g.apply(data, expected_md5=None)


def test_refuses_an_iat_slot_that_imports_something_else():
    imports = {va: name for name, va in g.IAT.items()}
    imports[g.IAT["MessageBoxA"]] = "MessageBoxW"
    data = synthetic_pe(imports=imports)
    with pytest.raises(g.PatchError, match="MessageBoxA"):
        g.apply(data, expected_md5=None)


def test_cli_writes_nothing_on_refusal_and_never_overwrites_the_input(tmp_path):
    src = tmp_path / "game.dat"
    src.write_bytes(synthetic_pe())
    out = tmp_path / "game-upgrades.dat"
    with pytest.raises(SystemExit, match="refused: md5"):
        g.main([str(src), str(out)])
    assert not out.exists()
    with pytest.raises(SystemExit, match="new file"):
        g.main([str(src), str(src)])


# --- the hooked x86 in an emulator ---------------------------------------------------------------------------------------------------

STUBS = {"wsprintfA": 0x7000_0000, "MessageBoxA": 0x7000_0010, "ExitProcess": 0x7000_0020}
CENTER, TEMPLATE, NAME_STR, NAME_DATA, STACK = 0x6000_0000, 0x6000_1000, 0x6000_2000, 0x6000_3000, 0x6100_0000


def run_guard(counter: int, name: str | None):
    """run newUpgrade's hooked bit assignment from RW 0x66FCB7 with the center's counter `counter` and an upgrade named `name`
    (None = an AsciiString with no data); returns (template bit, counter after, stop address, calls)"""
    uc_mod = pytest.importorskip("unicorn")
    from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBP, UC_X86_REG_EDI, UC_X86_REG_EIP, UC_X86_REG_ESI, UC_X86_REG_ESP

    data = synthetic_pe()
    g.apply(data, expected_md5=None)
    pe = g._pe(data)
    uc = uc_mod.Uc(uc_mod.UC_ARCH_X86, uc_mod.UC_MODE_32)
    uc.mem_map(BASE, g._align(pe["size_of_image"], 0x1000))
    for s in pe["sections"]:
        uc.mem_write(BASE + s["rva"], bytes(data[s["praw"]:s["praw"] + s["rsize"]]))
    for name_, va in g.IAT.items():  # the loader's job: the IAT slots point at the imports (stubs here)
        uc.mem_write(va, struct.pack("<I", STUBS[name_]))
    uc.mem_map(0x7000_0000, 0x1000)
    uc.mem_write(0x7000_0000, b"\xc3" * 0x100)
    uc.mem_map(0x6000_0000, 0x10000)
    uc.mem_map(STACK - 0x10000, 0x20000)
    uc.mem_write(CENTER + 0x10, struct.pack("<I", counter & 0xFFFFFFFF))
    uc.mem_write(TEMPLATE + 0x38, struct.pack("<i", -1))  # the constructor's "no bit" (RW 0x66F888)
    uc.mem_write(NAME_STR, struct.pack("<I", 0 if name is None else NAME_DATA))
    if name is not None:
        uc.mem_write(NAME_DATA + 8, name.encode() + b"\0")
    ebp = STACK - 0x100
    uc.mem_write(ebp + 8, struct.pack("<I", NAME_STR))
    uc.reg_write(UC_X86_REG_EBP, ebp)
    uc.reg_write(UC_X86_REG_ESP, ebp - 0x40)
    uc.reg_write(UC_X86_REG_EDI, CENTER)
    uc.reg_write(UC_X86_REG_ESI, TEMPLATE)
    calls = []

    def arg(i):
        esp = uc.reg_read(UC_X86_REG_ESP)
        return struct.unpack("<I", uc.mem_read(esp + 4 + 4 * i, 4))[0]

    def cstr(va):
        b = bytes(uc.mem_read(va, 1200))
        return b[:b.index(b"\0")].decode("latin1")

    def hook(uc_, address, size, user):
        if address == STUBS["wsprintfA"]:
            buf, fmt, nm, num, lim = (arg(i) for i in range(5))
            head, rest = cstr(fmt).split("%.200s", 1)  # the guard's format: %.200s, then %u, %u
            mid, tail = rest.split("%u", 1)
            text = head + cstr(nm)[:200] + mid + str(num) + tail.replace("%u", str(lim), 1)
            uc_.mem_write(buf, text.encode("latin1") + b"\0")
            calls.append(("wsprintfA", cstr(nm), num, lim))
            uc_.reg_write(UC_X86_REG_EAX, len(text))
        elif address == STUBS["MessageBoxA"]:
            calls.append(("MessageBoxA", arg(0), cstr(arg(1)), cstr(arg(2)), arg(3)))
            esp = uc_.reg_read(UC_X86_REG_ESP)
            ret = struct.unpack("<I", uc_.mem_read(esp, 4))[0]
            uc_.reg_write(UC_X86_REG_ESP, esp + 4 + 16)  # stdcall
            uc_.reg_write(UC_X86_REG_EIP, ret)
            uc_.reg_write(UC_X86_REG_EAX, 1)
        elif address == STUBS["ExitProcess"]:
            calls.append(("ExitProcess", arg(0)))
            uc_.emu_stop()
        elif address == g.RESUME_VA:
            uc_.emu_stop()

    uc.hook_add(uc_mod.UC_HOOK_CODE, hook)
    uc.emu_start(g.HOOK_VA, 0, count=200)
    bit = struct.unpack("<i", uc.mem_read(TEMPLATE + 0x38, 4))[0]
    after = struct.unpack("<I", uc.mem_read(CENTER + 0x10, 4))[0]
    return bit, after, uc.reg_read(UC_X86_REG_EIP), calls


@pytest.mark.parametrize("counter", [0, 3, 1097, 1151])
def test_emulated_below_the_limit_is_retail(counter):
    bit, after, eip, calls = run_guard(counter, "Upgrade_Test")
    assert (bit, after, eip, calls) == (counter, counter + 1, g.RESUME_VA, [])


@pytest.mark.parametrize("counter", [1152, 1153, 4000, 0xFFFFFFFF])
def test_emulated_at_or_past_the_limit_stops_with_the_name(counter):
    bit, after, eip, calls = run_guard(counter, "Upgrade_Test_0200")
    assert bit == -1 and after == counter  # nothing written: no mask bit, the counter unchanged
    assert [c[0] for c in calls] == ["wsprintfA", "MessageBoxA", "ExitProcess"]
    assert calls[0][1:] == ("Upgrade_Test_0200", (counter + 1) & 0xFFFFFFFF, 1152)
    owner, text, title, flags = calls[1][1:]
    assert owner == 0 and title == "RotWK upgrade limit" and flags == g.MB_FLAGS
    assert "'Upgrade_Test_0200' as number %d" % ((counter + 1) & 0xFFFFFFFF) in text and "hold only 1152" in text
    assert calls[2] == ("ExitProcess", 1)


def test_emulated_empty_name_and_long_name():
    _, _, _, calls = run_guard(1152, None)
    assert calls[0][1] == "" and "upgrade '' as number 1153" in calls[1][2]
    _, _, _, calls = run_guard(1152, "U" * 600)
    assert "'" + "U" * 200 + "' as number" in calls[1][2]


@pytest.mark.skipif(not os.environ.get("ENNORATH_1983_GAME_DAT"), reason="ENNORATH_1983_GAME_DAT (Ennorath 1.9.8.3's game.dat) not set")
def test_the_real_ennorath_1983_file():
    src = Path(os.environ["ENNORATH_1983_GAME_DAT"]).read_bytes()
    assert hashlib.md5(src).hexdigest() == g.INPUT_MD5
    data = bytearray(src)
    base = g.apply(data)
    assert base == 0xED7000 and g.verify(data) == []
    pytest.importorskip("capstone")
    lines = g.check_report(src, bytes(data))
    assert all("CHANGED" not in x for x in lines) and any("only in the 9 hook bytes: True" in x for x in lines)
    with pytest.raises(g.PatchError, match="md5"):
        g.apply(bytearray(data))


def test_make_test_ini_appends_numbered_dummies_after_the_base(tmp_path, capsys):
    import make_test_ini as m

    base = tmp_path / "upgrade.ini"
    base.write_bytes(b'#include "includes/upgrade.inc"\r\nUpgrade Upgrade_Base\r\nEnd\r\n')
    m.main(["60", str(base)])
    out = capsys.readouterr().out
    assert out.startswith('#include "includes/upgrade.inc"\r\nUpgrade Upgrade_Base\r\nEnd\n')
    headers = [line for line in out.splitlines() if line.startswith("Upgrade ")]
    assert headers[0] == "Upgrade Upgrade_Base" and headers[1:] == [f"Upgrade Test_{i:04d}" for i in range(1, 61)]
    assert out.count("\nEnd\n") == 1 + 60  # the base's block and the dummies
    with pytest.raises(ValueError):
        m.dummy_upgrades(0)
