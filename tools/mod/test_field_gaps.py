"""Synthetic tests of tools/mod/field_gaps.py: a hand-written audit and a small function map, no game data."""
from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import field_gaps as fg  # noqa: E402
import matrix_gaps as mg  # noqa: E402

MAP = "module\tdepth\tfunction\tstatus\tcalled from\tvcalls\tneighbour cited\tdecomp\n" \
      "SpecialAbilityUpdate\t0\t0x00852B14\tnone\t\t\t\t\n" \
      "SpecialAbilityUpdate\t1\t0x00852C00\tnone\t\t\t\t\n" \
      "SpecialAbilityUpdate\t0\t0x00851000\tported\t\t\t\t\n" \
      "ArmorUpgrade\t0\t0x00500000\tported\t\t\t\t\n"

AUDIT = [
    {"status": "parsed-unused", "block": "object", "owner": "SpecialAbilityUpdate", "field": "PreTriggerUnstealthTime", "defs": "40", "how": "struct", "stop": "S-1"},
    {"status": "not-read", "block": "object", "owner": "ArmorUpgrade", "field": "Unknown", "defs": "3"},
    {"status": "not-read", "block": "gamedata", "owner": "", "field": "SomeTiming", "defs": "1"},
    {"status": "used", "block": "object", "owner": "SpecialAbilityUpdate", "field": "SpecialPowerTemplate", "defs": "900"},
]


def test_unused_fields_get_their_owner_class_candidates(tmp_path):
    m = tmp_path / "map.tsv"
    m.write_text(MAP, encoding="utf-8")
    gaps, classes, reach = mg.read_map(m)
    rows = fg.join(AUDIT, gaps, classes, reach, {0x852C00: 4})
    assert [r[3] for r in rows] == ["PreTriggerUnstealthTime", "Unknown", "SomeTiming"]  # by definitions, the used field left out
    first = rows[0]
    assert first[5:] == ["struct", "S-1", "yes", "2", "1", "0", "0x00852B14@0"]  # the 4-byte function is trivial
    assert rows[1][7] == "yes" and rows[1][9] == "0" and rows[1][11].startswith("no specific none function")
    assert rows[2][7] == "no" and rows[2][11].startswith("the owner is not a module class")


def test_write_has_the_header(tmp_path):
    out = tmp_path / "FIELD_GAPS.tsv"
    fg.write([["parsed-unused", "object", "C", "F", "1", "struct", "", "yes", "1", "1", "0", "0x00400000@0"]], out)
    lines = out.read_text(encoding="utf-8").splitlines()
    assert lines[0].split("\t") == fg.HEADER and lines[1].endswith("0x00400000@0")


def test_named_template_getters_preserves_aliases_and_owner():
    from io import StringIO
    xmap = StringIO(
        "rotwk_rva,decomp_qualname,tier\n"
        "0x100,ThingTemplate::getFirst,A\n"
        "0x100,WeaponTemplate::getSecond,B\n"
        "0x100,ThingTemplate::getFirst,A\n"
        "0x100,ThingTemplate::getUntrusted,C\n"
        "0x200,ThingTemplate::setFirst,A\n"
    )
    assert fg.named_template_getters(xmap, 0x400000) == {
        0x400100: {('object', 'ThingTemplate::getFirst'), ('weapon', 'WeaponTemplate::getSecond')}
    }


def test_coverage_counts_missing_getter_as_unrouted():
    def row(block, note, route):
        return ['', block, '', '', '0', '', '', '', '', '', '', note, route]
    counts = fg.coverage_by_block([
        row('object', 'no named getter candidate', 'getter'),
        row('object', '0x00123456 (none)', 'getter 0x00112233 ThingTemplate::getFoo'),
        row('object', '0x00123457 (ported)', 'global Example +0x10'),
        row('object', 'candidate', 'module'),
        row('weather', 'no route', ''),
    ])
    assert dict(counts['object']) == {'total': 4, 'unrouted': 1, 'getter': 1, 'candidates': 2, 'global': 1, 'module': 1}
    assert dict(counts['weather']) == {'total': 1, 'unrouted': 1}


def test_extra_inventory_must_match_and_preserves_original_blocks():
    import pytest
    main = {0x1000: ((0x1000, 2),)}
    extra = {'sha256': 'digest', 'missing_seeds': [], 'functions': [
        {'address': 0x1000, 'blocks': [[0x1000, 3]]},
        {'address': 0x2000, 'blocks': [[0x2000, 4]]},
    ]}
    assert fg.merge_extra_inventory(main, 'digest', extra) == {
        0x1000: ((0x1000, 2),), 0x2000: ((0x2000, 4),)}
    with pytest.raises(ValueError, match='does not match'):
        fg.merge_extra_inventory(main, 'wrong', extra)
    with pytest.raises(ValueError, match='missing seeds'):
        fg.merge_extra_inventory(main, 'digest', {**extra, 'missing_seeds': [0x2000]})
    with pytest.raises(ValueError, match='duplicate'):
        fg.merge_extra_inventory(main, 'digest', {**extra, 'functions': extra['functions'] * 2})


def test_two_hop_array_reader_preserves_call_chain_and_hop_limit():
    # global in ECX -> 0x1010 -> 0x1020 -> 0x1030. Only the first two
    # receivers are explored; the indexed read at 0x1020 is reached.
    code = bytearray(bytes.fromhex('8b0d00200000 e805000000 c3'))
    code.extend(b'\x90' * (0x10 - len(code)))
    code.extend(bytes.fromhex('e80b000000 c3'))
    code.extend(b'\x90' * (0x20 - len(code)))
    code.extend(bytes.fromhex('8a44c134 e807000000 c3'))
    code.extend(b'\x90' * (0x30 - len(code)))
    code.extend(bytes.fromhex('8a44c134 c3'))
    blocks = {0x1000: ((0x1000, 12),), 0x1010: ((0x1010, 6),),
              0x1020: ((0x1020, 10),), 0x1030: ((0x1030, 5),)}
    import block_readers as br
    calls = set()
    br.global_reads(code, 0x1000, {0x1000: blocks[0x1000]}, {0x2000}, calls_out=calls, call_targets=blocks)
    origins, reads = fg.linked_array_reads(code, 0x1000, blocks, calls, {0x2000},
                                           {0x2000: {(0x34, 8)}}, {0x2000: {(0x30, 8)}})
    assert origins[(0x1020, 0x2000)] == {((0x1000, 0x1006), (0x1010, 0x1010))}
    assert reads[(0x2000, ('indexed', 8), 0x34)] == {(0x1020, 0x1020)}
    assert (0x1030, 0x2000) not in origins


def test_a_module_field_is_one_row():
    audit = [{"status": "parsed-unused", "block": "Object", "owner": "SomeUpdate", "field": "Rate", "defs": "3"}]
    rows = fg.join(audit, {"SomeUpdate": {}}, {"SomeUpdate"}, block_candidates={})
    assert len(rows) == 1 and rows[0][-1] == "module"
