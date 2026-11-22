"""Synthetic x86 and source trees: no retail inputs needed."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from rw_function_map import Function, decode_function, walk_graph, scan_citations, importance


def test_cfg_calls_vcalls_and_tail_jumps():
    # 1000: call 1010; call [eax+0x24]; jz 100d; ret; ret.
    # 1010: jmp 1020 (a known tail-call entry); 1020: ret.
    code = bytes.fromhex('e8 0b 00 00 00 ff 50 24 74 03 c3 90 90 c3 90 90 e9 0b 00 00 00') + b'\x90' * 11 + b'\xc3'
    entries = {0x1000, 0x1010, 0x1020}
    first = decode_function(code, 0x1000, 0x1000, 0x1010, entries)
    tail = decode_function(code, 0x1000, 0x1010, 0x1020, entries)
    assert first.calls == {0x1010}
    assert first.vcalls == {0x24}
    assert first.instructions == 5
    assert tail.calls == {0x1020}
    functions = {0x1000: first, 0x1010: tail, 0x1020: Function(frozenset(), frozenset(), 1)}
    depths, incoming = walk_graph({0x1000}, functions, {0x1010})
    assert depths == {0x1000: 0, 0x1010: 1}
    assert incoming == {0x1010: {0x1000}}
    depths, incoming = walk_graph({0x1000}, functions, set())
    assert depths[0x1020] == 2
    assert incoming[0x1020] == {0x1010}


def test_citations_and_importance(tmp_path):
    (tmp_path / 'test.cpp').write_text('// RW 0x401234\n// NOT PORTED\n// RW 0x401235\n\n// RW 0x401236 S-6011\n// RW 0x401236 real\n', encoding='utf-8')
    status = scan_citations(tmp_path)
    assert status == {0x401234: 'partial', 0x401235: 'ported', 0x401236: 'ported'}
    assert status.get(0x401237, 'none') == 'none'
    entries = [(3, 0, 1, 'a', 1), (10, 2, 2, 'b', 2), (10, 1, 2, 'c', 3), (10, 1, 5, 'd', 4)]
    assert [x[3] for x in sorted(entries, key=lambda x: importance(*x))] == ['d', 'c', 'b', 'a']


def test_neighbours_include_callers_and_callees_only():
    from rw_function_map import cited_neighbours
    functions = {
        1: Function(frozenset({2}), frozenset(), 1),
        2: Function(frozenset({3, 4}), frozenset(), 1),
        4: Function(frozenset(), frozenset(), 1),
    }
    assert cited_neighbours(2, functions, {1: 'partial', 3: 'ported'}) == [
        ('callee', 3, 'ported'), ('caller', 1, 'partial')]
    assert cited_neighbours(4, functions, {}) == []


def test_decomp_lookup_is_optional_and_respects_function_extents(tmp_path):
    from rw_function_map import DecompLookup
    path = tmp_path / 'rw_map.csv'
    assert DecompLookup(path).get(0x401001) == ''
    path.write_text(
        'rotwk_rva,size,bfme2_rva,tier,decomp_qualname,decomp_name\n'
        '0x1000,4,0x2000,A,Thing::run,run\n'
        '0x1010,2,, -,Other::run,run\n'.replace(', -,', ',-,'), encoding='utf-8')
    lookup = DecompLookup(path)
    assert lookup.get(0x401000) == 'Thing::run (tier A)'
    assert lookup.get(0x401003) == 'Thing::run (tier A)'
    assert lookup.get(0x401004) == ''
    assert lookup.get(0x401010) == ''


def test_reports_are_deterministic_sorted_and_show_status_changes(tmp_path, capsys):
    from rw_function_map import MapRow, write_reports
    tsv, md = tmp_path / 'map.tsv', tmp_path / 'map.md'
    rows = [
        MapRow('Low', 0, 0x401000, 'none', objects=1, instructions=20),
        MapRow('High', 1, 0x401010, 'partial', (0x401000,), (0x24,),
               (('caller', 0x401000, 'ported'),), 'Foo::bar (tier A)', 10, 4),
        MapRow('High', 0, 0x401020, 'none', objects=10, instructions=2),
    ]
    assert write_reports(rows, tsv, md, 'example', {0x401100: 'allocator'}, {0x401200: 201}, 200) == []
    text = md.read_text(encoding='utf-8')
    assert text.index('0x00401020') < text.index('0x00401010') < text.index('0x00401000')
    assert '0x00401100: allocator' in text and '201 call sites' in text
    content = tsv.read_text(encoding='utf-8')
    assert '\tdecomp\n' in content and 'vcall +0x24' in content and 'Foo::bar (tier A)' in content
    assert write_reports(list(reversed(rows)), tsv, md, 'example', {0x401100: 'allocator'}, {0x401200: 201}, 200) == []
    assert content == tsv.read_text(encoding='utf-8')
    from dataclasses import replace
    changed = [replace(r, status='ported') if r.function == 0x401010 else r for r in rows]
    assert write_reports(changed, tsv, md, 'example', {}, {}, 200) == [('High', 0x401010, 'partial')]
    assert 'partial -> ported' in capsys.readouterr().out


def test_reports_reject_ambiguous_rows_and_existing_format(tmp_path):
    import pytest
    from rw_function_map import MapRow, write_reports
    tsv, md = tmp_path / 'map.tsv', tmp_path / 'map.md'
    row = MapRow('X', 0, 0x401000, 'none')
    with pytest.raises(ValueError, match='duplicate'):
        write_reports([row, row], tsv, md, 'example', {}, {}, 200)
    tsv.write_text('unexpected\theader\n', encoding='utf-8')
    with pytest.raises(ValueError, match='columns'):
        write_reports([row], tsv, md, 'example', {}, {}, 200)


def test_module_tables_use_constructor_not_module_data():
    import pytest
    from rw_function_map import discover_module_tables
    image = object()
    records = [{'name': 'Thing', 'create': '0x1000', 'vtable': '0x9000'}]
    def constructor_finder(img, address):
        assert img is image and address == 0x1000
        return 0x1100
    def table_finder(img, address):
        assert img is image and address == 0x1100
        return {0x10: 0x3100, 0: 0x3000}
    result = discover_module_tables(image, records, constructor_finder, table_finder)
    assert result[0].data_vtable == 0x9000
    assert result[0].tables == ((0, 0x3000), (0x10, 0x3100))
    with pytest.raises(ValueError, match='constructor not found'):
        discover_module_tables(image, records, lambda *_: None, table_finder)
    with pytest.raises(ValueError, match='primary vtable not found'):
        discover_module_tables(image, records, constructor_finder, lambda *_: {0x10: 0x3100})
    with pytest.raises(ValueError, match='duplicate module'):
        discover_module_tables(image, records * 2, constructor_finder, table_finder)


def test_replay_module_inheritance_and_replacements():
    import pytest
    from rw_function_map import ModuleEntry, add_module, copy_module_state, remove_module
    make = lambda n, t, **kw: ModuleEntry(n, t, 'behavior', kw.get('mask', 1), kw.get('ai', False), kw.get('slot7', False), kw.get('inheritable', False))
    parent = (make('A', 'custom_tag'), make('OldAI', 'ai', ai=True), make('OldBody', 'body', slot7=True),
              make('Keep', 'keep', mask=2, inheritable=True))
    copied = copy_module_state(parent)
    # RW 0x73F49B: new AI removes the parent's AI even with a different tag.
    child = add_module(copied, make('NewAI', 'new_ai', ai=True), child=True)
    assert {x.name for x in child} == {'A', 'NewAI', 'OldBody', 'Keep'}
    # RW 0x73F4B4: slot 7 clears only in child mode.
    child = add_module(child, make('NewBody', 'new_body', slot7=True), child=True)
    assert {x.name for x in child} == {'A', 'NewAI', 'NewBody', 'Keep'}
    child = add_module(child, make('B', 'custom_tag'), child=True)
    assert {x.name for x in child} == {'B', 'NewAI', 'NewBody', 'Keep'}
    child, old = remove_module(child, 'custom_tag')
    child = add_module(child, make('B', 'new_tag'), child=True, replacing=old)
    assert any(x.tag == 'new_tag' for x in child)
    with pytest.raises(ValueError, match='same class'):
        add_module(child, make('Wrong', 'other'), child=True, replacing=old)
    with pytest.raises(ValueError, match='expected one'):
        remove_module(child, 'missing')
    # RW 0x73F33D: non-child mask clears copied entries, except inheritable.
    plain = add_module(copied, make('C', 'new', mask=1), child=False)
    assert {x.name for x in plain} == {'Keep', 'C'}
    draw = ModuleEntry('DrawA', 'draw', 'draw', 4, False, False)
    with pytest.raises(ValueError, match='duplicate'):
        add_module((draw,), ModuleEntry('DrawB', 'draw', 'draw', 4, False, False), child=True)


def test_parse_module_events_uses_any_tag_and_skips_nested_blocks():
    import pytest
    from types import SimpleNamespace
    from rw_function_map import parse_module_events
    grammar = SimpleNamespace(classes={'A': {}, 'B': {}}, object={'armorset': ('block', {'armor': ('line', None)}), 'behavior': ('line', None)})
    text = [
        'ArmorSet', 'Armor = X', 'End',
        'Behavior = A CustomTag', 'SomeField = 1', 'End',
        'RemoveModule CustomTag',
        'ReplaceModule OldTag', 'Behavior = B NewTag', 'End', 'End',
        'InheritableModule', 'Draw = A DrawTag', 'End', 'End', 'End',
    ]
    lines = [(line, str(i)) for i, line in enumerate(text)]
    events, end = parse_module_events(lines, 0, grammar)
    assert end == len(lines)
    assert [(x.action, x.name, x.tag, x.inheritable) for x in events] == [
        ('add', 'A', 'CustomTag', False), ('remove', '', 'CustomTag', False),
        ('replace_begin', '', 'OldTag', False), ('add', 'B', 'NewTag', False),
        ('replace_end', '', 'OldTag', False), ('add', 'A', 'DrawTag', True)]
    with pytest.raises(ValueError, match='unknown module class'):
        parse_module_events([('Behavior = Unknown tag', '1'), ('End', '2')], 0, grammar)
    with pytest.raises(ValueError, match='not closed'):
        parse_module_events([('foo = bar', '1')], 0, grammar)


def test_module_field_that_can_be_one_line_does_not_eat_object_end():
    from types import SimpleNamespace
    from rw_function_map import parse_module_events
    grammar = SimpleNamespace(
        classes={'A': {'addemotion': ('block', {'subfield': ('line', None)})}},
        object={'geometry': ('line', None)})
    text = ['Behavior = A Arbitrary', 'AddEmotion = value', 'AddEmotion = OVERRIDE other', 'End', 'AddEmotion = last', 'End', 'Geometry = X', 'End']
    events, end = parse_module_events([(line, str(i)) for i, line in enumerate(text)], 0, grammar)
    assert end == len(text)
    assert [(e.name, e.tag) for e in events] == [('A', 'Arbitrary')]


def test_replay_extracted_events_uses_registry_and_replacement():
    import pytest
    from rw_function_map import ModuleEntry, ModuleEvent, replay_module_events
    registry = [dict(name='A', type=0, mask=1, isAiModuleData=False, slot7=False),
                dict(name='AI', type=0, mask=17, isAiModuleData=True, slot7=False)]
    initial = (ModuleEntry('A', 'old', 'behavior', 1, False, False),)
    events = [ModuleEvent('replace_begin', tag='old', source='2'),
              ModuleEvent('add', tag='new', name='A', category='behavior', source='3'),
              ModuleEvent('replace_end', tag='old', source='4')]
    result = replay_module_events(initial, events, registry, child=True)
    assert [(x.name, x.tag) for x in result] == [('A', 'new')]
    with pytest.raises(ValueError, match='3: ReplaceModule'):
        replay_module_events(initial, [events[0], ModuleEvent('add', tag='new', name='AI', category='behavior', source='3')], registry, child=True)
    with pytest.raises(ValueError, match='unclosed replacement'):
        replay_module_events(initial, events[:2], registry, child=True)


def test_object_boundary_mismatch_is_not_silently_accepted():
    import pytest
    from types import SimpleNamespace
    from rw_function_map import parse_module_events
    grammar = SimpleNamespace(classes={}, object={})
    lines = [('End', '1'), ('Other = ignored', '2')]
    with pytest.raises(ValueError, match='boundary mismatch'):
        parse_module_events(lines, 0, grammar, limit=2)


def test_ordered_object_counts_deduplicate_and_require_loaded_parent():
    import pytest
    from rw_function_map import ModuleEvent, ObjectDefinition, resolve_objects
    reg = [dict(name='A', type=0, mask=0, isAiModuleData=False, slot7=False)]
    event = lambda tag: ModuleEvent('add', name='A', tag=tag, category='behavior')
    default = ObjectDefinition('DefaultThingTemplate', 'object', '', (event('one'),))
    parent = ObjectDefinition('Parent', 'object', '', (event('two'),))
    child = ObjectDefinition('Child', 'childobject', 'Parent', (ModuleEvent('remove', tag='one'),))
    objects, counts = resolve_objects([default, parent, child], reg)
    assert [x.tag for x in objects['Child']] == ['two']
    assert counts['A'] == 3
    with pytest.raises(ValueError, match='not yet loaded'):
        resolve_objects([child, parent], reg)
    merged = ObjectDefinition('Parent', 'object', '', (event('three'),))
    objects, _ = resolve_objects([parent, merged], reg)
    assert [x.tag for x in objects['Parent']] == ['two', 'three']


def test_inventory_counts_sites_not_calling_functions():
    import pytest
    from rw_function_map import decode_inventory
    # Two calls in a single function to a known target, then a return.
    blob = bytes.fromhex('e8 0b 00 00 00 e8 06 00 00 00 c3') + b'\xcc' * 5 + b'\xc3'
    funcs, stops = decode_inventory(blob, 0x1000, {0x1000: 0x100b, 0x1010: 0x1011}, threshold=1)
    assert funcs[0x1000].calls == {0x1010}
    assert funcs[0x1000].callsites == ((0x1000, 0x1010), (0x1005, 0x1010))
    assert stops == {0x1010: 2}
    with pytest.raises(ValueError, match='overlapping'):
        decode_inventory(blob, 0x1000, {0x1000: 0x1010, 0x100f: 0x1011})


def test_compare_confirmed_parent_interfaces_and_new_slots():
    import pytest
    from rw_function_map import overridden_slots
    assert overridden_slots({0: (1, 2, 30, 4), 16: (8, 9), 32: (12,)},
                            {0: (1, 2, 3), 16: (8, 7)}) == [
                                (0, 2, 30), (0, 3, 4), (16, 1, 9), (32, 0, 12)]
    with pytest.raises(ValueError, match='vtable'):
        overridden_slots({1: (3,)}, {})


def test_ini_archive_inventory_with_includes_default_child_and_duplicates(tmp_path):
    import zipfile
    from types import SimpleNamespace
    from rw_function_map import inventory_from_ini
    grammar = SimpleNamespace(classes={'A': {}, 'B': {}}, object={'behavior': ('line', None)})
    reg = [dict(name=x, type=0, mask=0, isAiModuleData=False, slot7=False) for x in ('A', 'B')]
    path = tmp_path / 'ini.zip'
    with zipfile.ZipFile(path, 'w') as z:
        z.writestr('data/ini/default/object.ini', 'Object DefaultThingTemplate\nBehavior = A tag_a\nEnd\nEnd\n')
        z.writestr('data/ini/object/one.ini', 'Object Base\n#include "part.inc"\nEnd\n'
                   'Object Base\nBehavior = B tag_c\nEnd\nEnd\n'
                   'ChildObject Child Base\nRemoveModule tag_b\nEnd\n')
        z.writestr('data/ini/object/part.inc', 'Behavior = B tag_b\nEnd\n')
    objects, counts, defs = inventory_from_ini(path, grammar, reg)
    assert len(defs) == 4
    assert {x.tag for x in objects['Base']} == {'tag_a', 'tag_b', 'tag_c'}
    assert {x.tag for x in objects['Child']} == {'tag_a', 'tag_c'}
    assert counts == {'A': 3, 'B': 2}


def test_block_inventory_skips_gaps_and_counts_shared_sites_once():
    from rw_function_map import decode_block_inventory
    # call 0x1020; ret; gap (not code); shared tail call 0x1020; ret
    blob = bytearray(b'\xcc' * 0x30)
    blob[0:6] = b'\xe8\x1b\x00\x00\x00\xc3'
    blob[0x10:0x16] = b'\xe8\x0b\x00\x00\x00\xc3'
    blob[0x20] = 0xc3
    blocks = {0x1000: ((0x1000, 6), (0x1010, 6)),
              0x1010: ((0x1010, 6),), 0x1020: ((0x1020, 1),)}
    functions, stops = decode_block_inventory(bytes(blob), 0x1000, blocks, 1)
    assert functions[0x1000].instructions == 4
    assert functions[0x1000].calls == {0x1020}
    assert stops == {0x1020: 2}  # not 3: the shared site is physical code once


def test_block_inventory_rejects_truncated_instruction_and_absent_entry():
    import pytest
    from rw_function_map import decode_block_inventory
    with pytest.raises(ValueError, match='truncated'):
        decode_block_inventory(b'\xe8\0\0\0\0', 0x1000, {0x1000: ((0x1000, 4),)})
    with pytest.raises(ValueError, match='entry has no'):
        decode_block_inventory(b'\xc3\xc3', 0x1000, {0x1000: ((0x1001, 1),)})


def test_direct_call_targets_must_have_their_own_cfg_entry():
    import pytest
    from rw_function_map import Function, verify_direct_calls
    functions = {0x401000: Function(frozenset({0x401010, 0x30000000}), frozenset(), 1),
                 0x401010: Function(frozenset(), frozenset(), 1)}
    verify_direct_calls(functions, 0x401000, 0x402000)  # imported target is outside .text
    with pytest.raises(ValueError, match='direct call targets'):
        functions[0x401000] = Function(frozenset({0x401020}), frozenset(), 1)
        verify_direct_calls(functions, 0x401000, 0x402000)


def test_cached_block_inventory_is_bound_to_roots_and_image(tmp_path):
    import hashlib
    import json
    import pytest
    from rw_function_map import load_function_blocks
    game = tmp_path / 'fake.dat'
    game.write_bytes(b'not retail bytes')
    digest = hashlib.sha256(game.read_bytes()).hexdigest()
    cache = tmp_path / 'blocks.json'
    record = {'sha256': digest, 'analyzer': 'synthetic', 'seed_count': 1,
              'missing_seeds': [], 'functions': [{'address': 0x401000, 'blocks': [[0x401000, 1]]}]}
    cache.write_text(json.dumps(record), encoding='utf-8')
    registry = [{'create': '0x401000', 'createData': '0x401000'}]
    with pytest.raises(ValueError, match='different vtable roots'):
        load_function_blocks(game, cache, registry, {0x402000})
    blocks, analyzer, loaded_digest = load_function_blocks(game, cache, registry)
    assert blocks == {0x401000: ((0x401000, 1),)}
    assert analyzer == 'synthetic' and loaded_digest == digest
    record['sha256'] = '0' * 64
    cache.write_text(json.dumps(record), encoding='utf-8')
    with pytest.raises(ValueError, match='SHA256'):
        load_function_blocks(game, cache, registry)


def test_display_command_has_no_developer_machine_prefix():
    from pathlib import Path
    from rw_function_map import display_command
    absolute = str(Path.cwd() / 'workspace/RotWK/game.dat')
    command = display_command(['--game', absolute, '--ini', '/tmp/ennorath-ini-1.9.8.3.zip'])
    assert '/home/' not in command and '/Users/' not in command
    assert 'workspace/RotWK/game.dat' in command
    assert '/tmp/ennorath-ini-1.9.8.3.zip' in command


def test_reports_accept_library_rows(tmp_path):
    # 2026-11-21: build_rows gives library functions the status 'library'; write_reports refused them ("invalid map row") and the sb3 maps
    # run failed at every main from 1cd735ca to c0388357
    from rw_function_map import MapRow, write_reports
    tsv, md = tmp_path / 'map.tsv', tmp_path / 'map.md'
    rows = [MapRow('Low', 0, 0x401000, 'library', objects=1, instructions=20), MapRow('Low', 1, 0x401010, 'none', objects=1, instructions=5)]
    write_reports(rows, tsv, md, 'example', {}, {}, 200)
    assert '\tlibrary\t' in tsv.read_text(encoding='utf-8') and 'library 1' in md.read_text(encoding='utf-8')
