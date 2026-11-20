"""Synthetic x86 only: no retail image needed."""
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
import block_readers as br
import rw_function_map as fm


def test_global_copy_overwrite_store_and_partial_register():
    # mov eax,[0x2000]; mov ebx,eax; mov ecx,[ebx+0x3c];
    # mov ebx,7; mov edx,[ebx+0x40]; mov [eax+0x44],ecx;
    # mov al,1; mov edx,[eax+0x48]; ret
    code = bytes.fromhex('a100200000 89c3 8b4b3c bb07000000 8b5340 894844 b001 8b5048 c3')
    reads = br.global_reads(code, 0x1000, {0x1000: ((0x1000, len(code)),)}, {0x2000})
    assert reads == [(0x1000, 0x2000, (), 0x3c, 0x1007)]


def test_getter_two_callers():
    # Two direct calls to 0x1010; the third function is the getter.
    data = bytes.fromhex('e80b000000c3 e805000000c3 90909090 c3')
    blocks = {0x1000: ((0x1000, 6),), 0x1006: ((0x1006, 6),), 0x1010: ((0x1010, 1),)}
    functions, _ = fm.decode_block_inventory(data, 0x1000, blocks)
    assert br.getter_callers(functions, {0x1010}) == {0x1010: [0x1000, 0x1006]}


def test_volatile_registers_killed_at_calls():
    # Saved EBX survives a call; EAX does not.
    code = bytes.fromhex('a100200000 89c3 e800000000 8b483c 8b4b40 c3')
    assert br.global_reads(code, 0x1000, {0x1000: ((0x1000, len(code)),)}, {0x2000}) == [
        (0x1000, 0x2000, (), 0x40, 0x100f)]


def test_field_table_and_route_join():
    import struct
    import field_gaps as fg
    data = bytearray(0x80)
    struct.pack_into('<IIII', data, 0, 0x1040, 0x2000, 0, 0x3c)
    data[0x40:0x46] = b'Field\0'
    assert br.field_table(data, 0x1000, 0x1000) == {'field': 0x3c}
    audit = [{'status': 'not-read', 'block': 'gamedata', 'field': 'Field', 'defs': '1'}]
    candidates = {('gamedata', 'field'): ('global Example +0x3C', '0x00001000 (none)')}
    assert fg.join(audit, {}, set(), block_candidates=candidates)[0][11:] == [
        '0x00001000 (none)', 'global Example +0x3C']


def test_lea_is_not_a_read():
    code = bytes.fromhex('a100200000 8d483c c3')
    assert br.global_reads(code, 0x1000, {0x1000: ((0x1000, len(code)),)}, {0x2000}) == []


def test_nested_singleton_pointer():
    # mov eax,[0x2000]; mov eax,[eax+0x18]; mov edx,[eax+0x3c]; ret
    code = bytes.fromhex('a100200000 8b4018 8b503c c3')
    reads = br.global_reads(code, 0x1000, {0x1000: ((0x1000, len(code)),)},
                            {0x2000}, {(0x2000, (0x18,))})
    assert reads == [(0x1000, 0x2000, (), 0x18, 0x1005),
                     (0x1000, 0x2000, (0x18,), 0x3c, 0x1008)]


def test_getter_offsets_with_receiver_copy_and_overwrite():
    # mov esi,ecx; mov eax,[esi+0x3c]; xor esi,esi; mov eax,[esi+0x40]; ret
    code = bytes.fromhex('89ce 8b463c 31f6 8b4640 c3')
    assert br.getter_offsets(code, 0x1000, {0x1000: ((0x1000, len(code)),)}, {0x1000}) == {0x1000: {0x3c}}


def test_global_pointer_across_branch_and_clobber():
    # Two arms join: one preserves EBX, the other overwrites it. A may-read at
    # the join is a candidate; after both arms overwrite EBX it is not.
    code = bytes.fromhex('a100200000 89c3 7404 31db eb02 90 90 8b4b3c 31db 8b4b40 c3')
    blocks = {0x1000: ((0x1000, 9), (0x1009, 4), (0x100d, 2), (0x100f, 9))}
    assert br.global_reads(code, 0x1000, blocks, {0x2000}) == [(0x1000, 0x2000, (), 0x3c, 0x100f)]


def test_bounded_field_table_without_sentinel():
    import struct
    import pytest
    data = bytearray(64)
    struct.pack_into('<IIII', data, 0, 0x1020, 0x2000, 0, 0x10)
    data[0x20:0x25] = b'Name\0'
    assert br.field_table(data, 0x1000, 0x1000, 0x1010) == {'name': 0x10}
    with pytest.raises(ValueError):
        br.field_table(data, 0x1000, 0x1000, 0x100f)


def test_virtual_singleton_getter_requires_same_receiver_and_slot():
    # mov ecx,[0x2000]; mov eax,[ecx]; call [eax+0x13c]; mov edx,[eax+0x3c]
    code = bytes.fromhex('8b0d00200000 8b01 ff903c010000 8b503c c3')
    blocks = {0x1000: ((0x1000, len(code)),)}
    reads = br.global_reads(code, 0x1000, blocks, {0x2000}, virtual_getters={(0x2000, (), 0x13c)})
    assert (0x1000, 0x2000, ('v13C',), 0x3c, 0x100e) in reads
    assert not any(path == ('v13C',) for _, _, path, _, _ in br.global_reads(
        code, 0x1000, blocks, {0x2000}, virtual_getters={(0x2000, (), 0x140)}))


def test_join_explains_unavailable_top_level_routes():
    import field_gaps as fg
    rows = fg.join([
        {'status': 'not-read', 'block': 'unknown', 'field': 'F', 'defs': '1'},
        {'status': 'not-read', 'block': 'weapon', 'field': 'F', 'defs': '1'},
        {'status': 'not-read', 'block': 'gamedata', 'field': 'F', 'defs': '1'},
        {'status': 'not-read', 'block': 'strategichud', 'owner': '/StatsDisplay', 'field': 'F', 'defs': '1'},
    ], {}, set(), block_candidates={})
    notes = {row[1]: row[11] for row in rows}
    assert notes['unknown'] == 'no reader route implemented for this top-level block'
    assert notes['weapon'].startswith('template getter route unavailable')
    assert notes['gamedata'] == 'field absent from the recovered singleton table'
    assert notes['strategichud'] == 'no reader route implemented for this nested top-level subtable'


def test_getter_address_taken_is_candidate_not_global_read():
    # lea eax,[ecx+0x78]; add ecx,0x324; ret
    code = bytes.fromhex('8d4178 81c124030000 c3')
    blocks = {0x1000: ((0x1000, len(code)),)}
    assert br.getter_offsets(code, 0x1000, blocks, {0x1000}) == {0x1000: {0x78, 0x324}}


def test_singleton_subobject_offsets_are_relative_to_global_pointer():
    import field_gaps as fg
    # RW 0x5DC77C passes TheEva + 0x80 to the field parser.
    assert fg.singleton_offset('miscevadata', 4) == 0x84
    assert fg.singleton_offset('gamedata', 4) == 4
    assert fg.singleton_offset('skirmishaidata', 0x95b) == 0x96b


def test_static_singleton_absolute_and_register_reads_but_not_stores():
    # mov eax,0x2000; mov ecx,[eax+0x3c]; mov [0x2040],ecx;
    # mov edx,[0x2044]; ret
    code = bytes.fromhex('b800200000 8b483c 890d40200000 8b1544200000 c3')
    blocks = {0x1000: ((0x1000, len(code)),)}
    reads = br.global_reads(code, 0x1000, blocks, set(), static_bases={0x2000: {0x3c, 0x40, 0x44}})
    assert reads == [(0x1000, 0x2000, ('static',), 0x3c, 0x1005),
                     (0x1000, 0x2000, ('static',), 0x44, 0x100e)]


def test_static_array_element_candidate():
    # Read field +0x10 of the second of two adjacent static objects.
    code = bytes.fromhex('a150200000 c3')
    blocks = {0x1000: ((0x1000, len(code)),)}
    reads = br.global_reads(code, 0x1000, blocks, set(), static_bases={0x2000: {0x10}, 0x2040: {0x10}})
    assert reads == [(0x1000, 0x2040, ('static',), 0x10, 0x1000)]


def test_weatherdata_singleton_array_offsets():
    import field_gaps as fg
    name, pointer, table, start, stride, count = fg.SINGLETON_ARRAYS['weatherdata']
    assert (name, pointer, table) == ('TheGlobalWeatherSystem', 0xDE772C, 0xC22D0C)
    # HasLightning is +4 within each of five records; a tracked load from
    # index 2 reads +0x44; an indexed operand is ignored without configuration.
    code = bytes.fromhex('a12c77de00 8a5044 8a548834 c3')
    reads = br.global_reads(code, 0x1000, {0x1000: ((0x1000, len(code)),)}, {pointer})
    assert reads == [(0x1000, pointer, (), start + 2 * stride + 4, 0x1005)]
    assert count == 5


def test_known_subobject_shift_lea_add_and_clobber():
    # eax=[0x2000]; lea ebx,[eax+0x10]; read [ebx+4];
    # add eax,0x10; read [eax+8]; overwrite ebx and read: not a candidate.
    code = bytes.fromhex('a100200000 8d5810 8b4b04 83c010 8b5008 31db 8b4b0c c3')
    blocks = {0x1000: ((0x1000, len(code)),)}
    assert br.global_reads(code, 0x1000, blocks, {0x2000}, subobject_offsets={0x2000: {0x10}}) == [
        (0x1000, 0x2000, (), 0x14, 0x1008),
        (0x1000, 0x2000, (), 0x18, 0x100e)]
    assert br.global_reads(code, 0x1000, blocks, {0x2000}) == []


def test_subobject_shift_in_loop_is_bounded():
    # The shifted pointer is not shifted a second time at the back edge.
    code = bytes.fromhex('a100200000 83c010 8b4804 ebf8')
    blocks = {0x1000: ((0x1000, 5), (0x1005, 8))}
    assert br.global_reads(code, 0x1000, blocks, {0x2000}, subobject_offsets={0x2000: {0x10}}) == [
        (0x1000, 0x2000, (), 0x14, 0x1008)]


def test_configured_indexed_singleton_reads_and_store_exclusion():
    # eax=[0x2000]; read byte [eax+ecx*8+0x34]; store it back;
    # wrong scale and overwritten base must not be reported.
    code = bytes.fromhex('a100200000 8a54c834 8854c834 8a548834 31c0 8a54c834 c3')
    blocks = {0x1000: ((0x1000, len(code)),)}
    assert br.global_reads(code, 0x1000, blocks, {0x2000},
                           indexed_fields={0x2000: {(0x34, 8)}}) == [
        (0x1000, 0x2000, ('indexed', 8), 0x34, 0x1005)]


def test_one_hop_receiver_collection_call_and_tail_jump():
    # Caller 1 passes a global in ECX to a direct call; caller 2 overwrites ECX
    # before a tail jump and must not become an origin. Callee reads an array.
    data = bytearray.fromhex('8b0d00200000 e815000000 c3 8b0d00200000 31c9 e906000000 c3')
    data.extend(b'\x90' * (0x20 - len(data)))
    data.extend(bytes.fromhex('8a44c134 c3'))
    blocks = {0x1000: ((0x1000, 12),), 0x100c: ((0x100c, 14),), 0x1020: ((0x1020, 5),)}
    calls = set()
    br.global_reads(data, 0x1000, blocks, {0x2000}, calls_out=calls)
    assert calls == {(0x1000, 0x1006, 0x1020, 0x2000, ())}
    reads = br.global_reads(data, 0x1000, {0x1020: blocks[0x1020]}, set(),
                            indexed_fields={0x2000: {(0x34, 8)}}, initial_receivers={0x1020: {(0x2000, ())}})
    assert reads == [(0x1020, 0x2000, ('indexed', 8), 0x34, 0x1020)]


def test_indexed_lea_address_is_not_read_until_dereferenced():
    # ecx is the receiver, eax is an index. LEA forms a record pointer;
    # only the following cmp reads its +0 field. A store and unrelated +8
    # member must not become candidates.
    code = bytes.fromhex('8d54c130 833a00 c7420400000000 8b4208 c3')
    blocks = {0x1000: ((0x1000, len(code)),)}
    reads = br.global_reads(code, 0x1000, blocks, set(),
        initial_receivers={0x1000: {(0x2000, ())}}, indexed_bases={0x2000: {(0x30, 8)}},
        indexed_fields={0x2000: {(0x30, 8), (0x34, 8)}})
    assert reads == [(0x1000, 0x2000, ('indexed', 8), 0x30, 0x1004)]


def test_direct_call_collection_uses_full_target_inventory():
    # Callee is outside the subset being scanned, yet its known function
    # entry is an allowed target for the next bounded receiver hop.
    code = bytes.fromhex('8b0d00200000 e805000000 c3')
    calls = set()
    br.global_reads(code, 0x1000, {0x1000: ((0x1000, len(code)),)}, {0x2000},
                    calls_out=calls, call_targets={0x1010})
    assert calls == {(0x1000, 0x1006, 0x1010, 0x2000, ())}


def test_an_instance_two_tables_claim_proves_neither():
    shared, own = ("ptr", 0x1000, (0,)), ("ptr", 0x2000, ())
    kept = br.assign_instances({"audiosettings": (shared, 8, 0.85, {}), "locomotor": (shared, 49, 1.0, {}), "weather": (own, 19, 0.87, {})})
    assert set(kept) == {"weather"}


def test_malformed_block_can_be_explicitly_skipped():
    # Preserve main's opt-in handling of mis-split whole-image CFG blocks.
    bad = bytes.fromhex('0f')
    blocks = {0x1000: ((0x1000, 1),)}
    import pytest
    with pytest.raises(ValueError, match='truncated'):
        br.global_reads(bad, 0x1000, blocks, set())
    skipped = []
    assert br.global_reads(bad, 0x1000, blocks, set(), (), (), skipped) == []
    assert skipped == [0x1000]
