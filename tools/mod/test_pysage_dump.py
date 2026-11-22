"""tools/mod/pysage_dump.py on a synthetic PE pair (no game file needed)."""
from __future__ import annotations

import struct
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import pysage_dump as pd  # noqa: E402

BASE = 0x400000
SECTIONS = ((".text", 0x1000, 0x400), (".rdata", 0x2000, 0x600), (".pys", 0x3000, 0x800))  # name, RVA, raw offset; 0x200 bytes each


def pe(fill: dict[int, bytes]) -> bytes:
    """A minimal PE32 image: the header fields pysage_dump.pe reads, three sections, bytes placed at virtual addresses."""
    img = bytearray(0xA00)
    struct.pack_into("<I", img, 0x3C, 0x40)
    img[0x40:0x44] = b"PE\0\0"
    struct.pack_into("<H", img, 0x40 + 6, len(SECTIONS))
    struct.pack_into("<H", img, 0x40 + 20, 0xE0)
    struct.pack_into("<I", img, 0x40 + 24 + 28, BASE)
    for i, (name, rva, raw) in enumerate(SECTIONS):
        h = 0x40 + 24 + 0xE0 + 40 * i
        img[h:h + 8] = name.encode().ljust(8, b"\0")
        struct.pack_into("<IIII", img, h + 8, 0x200, rva, 0x200, raw)
    for va, b in fill.items():
        rva = va - BASE
        name, srva, raw = next(s for s in SECTIONS if s[1] <= rva < s[1] + 0x200)
        img[raw + rva - srva:raw + rva - srva + len(b)] = b
    return bytes(img)


def test_hooks_calls_taken_addresses_and_rdata_are_followed_text_is_not(tmp_path):
    pytest.importorskip("capstone")
    func = (b"\x55" + b"\x68" + struct.pack("<I", 0x403040) + b"\x68" + struct.pack("<I", 0x403020)
            + b"\xe8" + struct.pack("<i", 0x403014 - 0x403010) + b"\x5d\xc3")
    common = {0x403000: func, 0x403014: b"\x31\xc0\xc3", 0x403020: b"Shift+Esc\0", 0x403040: b"\x55\x5d\xc3", 0x403060: b"\xc3",
              0x402000: struct.pack("<I", 0x403060)}
    van = tmp_path / "van.dat"
    van.write_bytes(pe({**common, 0x401000: b"\xb8\x01\x00\x00\x00\xc3", 0x402000: struct.pack("<I", 0x401000)}))
    pat = tmp_path / "pat.dat"
    pat.write_bytes(pe({**common, 0x401000: b"\xe9" + struct.pack("<i", 0x403000 - 0x401005) + b"\xc3"}))
    out = tmp_path / "out.txt"
    found = pd.dump(str(pat), str(van), str(out), [".pys"], [0x401000], [0x402000], ver="9.9")
    text = out.read_text(encoding="utf-8")
    assert "PYSAGE 9.9 .pys 0x00403000:" in text and "hook RW 0x401000" in text
    assert "0x00403014 (call from 0x40300B)" in text            # a call inside the section
    assert "0x00403040 (address taken at 0x403001)" in text      # push of a function's address
    assert "0x00403060 (.rdata RW 0x402000)" in text             # a method slot in .rdata
    assert "address taken at 0x403006" not in text               # the pushed string is no function
    assert "Shift+Esc" in text                                   # ...and shows in the data dump
    assert "mov eax, 1  <== site" in text and "jmp 0x403000  <== site" in text
    assert found == {".pys": 0x12 + 3 + 3 + 1}


def test_main_without_arguments_prints_the_usage(capsys):
    assert pd.main([]) == 2
    assert "pysage_dump.py <patched game.dat>" in capsys.readouterr().out
