"""Pins of the RotWK binary facts Port 232 (issue #555) cites. Runs only when RW_GAME_DAT is set (the path of the RotWK
game.dat); the image's md5 is checked, so a different binary or a wrong citation fails loudly. Caveat S-001: the pin
reads the one RotWK 2.01 image available to the project (md5 daed3668006cd90f01c34e5a7da1901f).

S-523: RW 0xDE49E4 is the global a SpecialPowerTemplate's two default filters are built from (the constructor RW
0x7B1F5B hands it to RW 0x763D11). In the RotWK 2.01 image the global lives inside .data's raw bytes (section offset
0x5B9E4 < the section's raw size 0x81000) and all 28 of its bytes are stored zero — an all-zero KindOfMaskType is what
RW 0x763D11 turns into the default filter: rule NONE, flag 0, both masks empty — nothing is allowed, and
ObjectFilterMatch::isValid (RW 0x762977) answers false. In the BFME2 1.06 image the same global sits in .data's
uninitialized tail (BSS, loader-zeroed), so both editions hand RW 0x763D11 a zero mask. The port builds the same
filter as ObjectFilter::none(empty, empty) (engine/src/GameLogic/ObjectFilter.cpp) and hands it out for an unset
m_objectFilter / m_forbiddenObjectFilter (filterOrDefault, engine/src/GameLogic/ActionManager.cpp).
"""
from __future__ import annotations

import hashlib
import os
import struct
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(ROOT / "tools" / "retail_oracle"))
import disasm  # noqa: E402  (tools/retail_oracle/disasm.py: PE section map and VA translation)

EXPECTED_MD5 = "daed3668006cd90f01c34e5a7da1901f"  # vanilla RotWK 2.01 game.dat

def _rw_game_dat_ready() -> bool:
    path = os.environ.get("RW_GAME_DAT")
    return bool(path) and Path(path).is_file()


pytestmark = pytest.mark.skipif(
    not _rw_game_dat_ready(),
    reason="RW_GAME_DAT not set to an existing RotWK game.dat (the image is not in Git; set the variable to run the pin)",
)


@pytest.fixture(scope="module")
def image():
    path = Path(os.environ["RW_GAME_DAT"])
    data = path.read_bytes()
    return path, data


@pytest.fixture(scope="module")
def sections(image):
    _, data = image
    base, secs = disasm.sections(data)
    return base, secs


def _data_section(sections):
    base, secs = sections
    for name, vsize, sva, rsize, rptr in secs:
        if name.rstrip(b"\0") == b".data":
            return base, vsize, sva, rsize, rptr
    raise AssertionError("no .data section")


def test_s523_image_is_rotwk_201(image):
    path, data = image
    md5 = hashlib.md5(data).hexdigest()  # noqa: S324 (an identity pin, not a security control)
    assert md5 == EXPECTED_MD5, f"{path} is not the RotWK 2.01 game.dat this pin reads (md5 {md5})"


def test_s523_global_is_in_data_virtual_range(sections):
    base, vsize, sva, rsize, rptr = _data_section(sections)
    rva = 0xDE49E4 - base
    assert sva <= rva < sva + vsize, "the global is outside .data's virtual range"
    assert rva + 28 <= sva + vsize, "the global's 28 bytes exceed .data's virtual range"


def test_s523_global_is_zero_initialized(sections, image):
    base, vsize, sva, rsize, rptr = _data_section(sections)
    rva = 0xDE49E4 - base
    off = rva - sva
    if off >= rsize:
        # BSS tail (the BFME2 1.06 layout): the loader gives zero bytes, nothing is stored in the file
        return
    _, data = image
    raw = data[rptr + off: rptr + off + 28]
    assert len(raw) == 28
    assert raw == b"\0" * 28, "the 28 bytes of the global are not stored as zero"
