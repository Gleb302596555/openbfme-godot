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


def test_an_instance_two_tables_claim_proves_neither():
    shared, own = ("ptr", 0x1000, (0,)), ("ptr", 0x2000, ())
    kept = br.assign_instances({"audiosettings": (shared, 8, 0.85, {}), "locomotor": (shared, 49, 1.0, {}), "weather": (own, 19, 0.87, {})})
    assert set(kept) == {"weather"}
