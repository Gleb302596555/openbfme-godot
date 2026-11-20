"""Synthetic examples; no game or third-party disassembler is needed."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from stop_groups import Inventory, cluster, enrich, read_stops, rw_addresses


def row(n, gap='', kind='code'):
    return f'| S-{n} | {kind} | area | {gap} | PORT-1 | — | — | — |\n'


def test_function_reference_unplaced_closed(tmp_path):
    p = tmp_path/'stops.md'
    p.write_text(row(325, 'RW 0x401002') + row(326, 'RW 0x401008') + row(327, 'waits for S-325') + row(328) + row(329, 'RW 0x401003', 'closed'), encoding='utf-8')
    stops = read_stops([p])
    inv = Inventory([{'address': 0x401000, 'blocks': [[0x401000, 32]]}])
    enrich(stops, {}, set(), inv)
    groups, unplaced = cluster(stops, {})
    assert sorted(groups) == [['S-325', 'S-326', 'S-327']]
    assert unplaced == ['S-328']
    assert 'S-329' not in stops


def test_split_large_component_by_function(tmp_path):
    p = tmp_path/'stops.md'
    p.write_text(''.join(row(n, f'ModuleX RW 0x{0x401002 if n < 45 else 0x402002:X}') for n in range(80)), encoding='utf-8')
    stops = read_stops([p])
    inv = Inventory([{'address': 0x401000, 'blocks': [[0x401000, 32]]}, {'address': 0x402000, 'blocks': [[0x402000, 32]]}])
    enrich(stops, {}, {'ModuleX'}, inv)
    groups, unplaced = cluster(stops, {}, limit=40)
    assert max(map(len, groups)) <= 40
    assert sum(map(len, groups)) == 80
    assert all(len(set().union(*(stops[s].functions for s in group))) == 1 for group in groups)
    assert not unplaced


def test_only_rw_citations():
    assert rw_addresses('BFME2 0x401000 RW 0x402000 / 0x402010 and RVA 0x403000') == {0x402000, 0x402010}


def test_known_map_start_beats_false_interior_seed():
    from stop_groups import BinaryResolver
    resolver = BinaryResolver.__new__(BinaryResolver)
    resolver.lo, resolver.hi = 0x401000, 0x403000
    resolver.map_starts = [0x401000]
    resolver.seeds = [0x401000, 0x401100]
    resolver.cache = {}
    resolver.reaches = lambda start, address: start in (0x401000, 0x401100)
    assert resolver.owner(0x401108) == 0x401000


def test_historical_shared_function_does_not_merge(tmp_path):
    p = tmp_path/'stops.md'
    p.write_text(row(10, 'Gap X remains. Earlier fixed since RW 0x401002 landed its hit.') +
                 row(11, 'Gap Y remains. Already ported since RW 0x401008.'), encoding='utf-8')
    stops = read_stops([p])
    inv = Inventory([{'address': 0x401000, 'blocks': [[0x401000, 32]]}])
    enrich(stops, {}, set(), inv)
    assert stops['S-10'].functions == stops['S-11'].functions == {0x401000}
    groups, _ = cluster(stops, {})
    assert not any({'S-10', 'S-11'} <= set(g) for g in groups)


def test_incidental_class_is_not_primary(tmp_path):
    p = tmp_path/'stops.md'
    p.write_text(row(1, 'The caller uses ModuleX for a different path.') + row(2, 'ModuleX is missing a path.'), encoding='utf-8')
    stops = read_stops([p])
    enrich(stops, {}, {'ModuleX'}, Inventory([]))
    assert stops['S-1'].classes == {'ModuleX'}
    assert not stops['S-1'].primary_classes
    assert stops['S-2'].primary_classes == {'ModuleX'}


def test_output_has_every_stop_and_all_references(tmp_path):
    import csv
    from stop_groups import output
    p = tmp_path/'stops.md'
    p.write_text(row(10, 'RW 0x401002, compare S-001 and S-999') + row(11) + row(12, kind='closed'), encoding='utf-8')
    stops = read_stops([p])
    inv = Inventory([{'address': 0x401000, 'blocks': [[0x401000, 32]]}])
    enrich(stops, {}, set(), inv)
    groups, unplaced = cluster(stops, {})
    out = tmp_path/'report'
    output(stops, groups, unplaced, {}, [], [], None, out, 'regenerate')
    with out.with_suffix('.tsv').open(encoding='utf-8') as f:
        rows = list(csv.DictReader(f, delimiter='\t'))
    assert {r['id'] for r in rows} == {'S-10', 'S-11'}
    assert rows[0]['references'] == 'S-001, S-999'
    assert rows[1]['group'] == 'unplaced'
    assert 'lookup unavailable' in out.with_suffix('.md').read_text()


def test_fragment_parses_only_its_first_row(tmp_path):
    fragments = tmp_path/'stops.d'
    fragments.mkdir()
    p = fragments/'S-10.md'
    p.write_text(row(10) + '\n## Notes\n' + row(11), encoding='utf-8')
    assert set(read_stops([p])) == {'S-10'}


def test_reviewed_broad_dispatcher_is_not_a_merge_pivot(tmp_path):
    from stop_groups import BROAD_DISPATCHERS
    p = tmp_path/'stops.md'
    p.write_text(row(1, 'RW 0x4BE600') + row(2, 'RW 0x4BEE00'), encoding='utf-8')
    stops = read_stops([p])
    inv = Inventory([{'address': 0x4BE587, 'blocks': [[0x4BE587, 0x1000]]}])
    enrich(stops, {}, set(), inv)
    groups, unplaced = cluster(stops, {}, stoplist=BROAD_DISPATCHERS)
    assert groups == [['S-1'], ['S-2']]
    assert not unplaced
    assert all(s.functions == {0x4BE587} for s in stops.values())


def test_seh_entry_after_ret_and_ret_imm():
    from stop_groups import seh_after_return
    prologue = b'\xb8\x22\x05\xb9\x00\xe8\x00\x00\x00\x00'
    assert seh_after_return(b'\x90\x90\xc3' + prologue, 3)
    assert seh_after_return(b'\xc2\x04\x00' + prologue, 3)
    assert not seh_after_return(b'\x90\x90\x90' + prologue, 3)
    assert not seh_after_return(b'\xc3', 0)


def test_port_and_rotwk_named_classes_are_reported_separately(tmp_path):
    p = tmp_path/'stops.md'
    p.write_text(row(1, 'PortWidget calls RetailWidget::update'), encoding='utf-8')
    stops = read_stops([p])
    funcs = {0x401000: {'names': {'RetailWidget::update (tier A)'}, 'classes': {'ModuleX'}, 'status': 'none'}}
    enrich(stops, funcs, {'ModuleX'}, Inventory([]), {'PortWidget'})
    assert stops['S-1'].port_classes == {'PortWidget'}
    assert stops['S-1'].rotwk_classes == {'RetailWidget'}
    assert not stops['S-1'].primary_classes


def test_cross_group_references_remain_visible_after_split(tmp_path):
    import csv
    from stop_groups import output
    p = tmp_path/'stops.md'
    p.write_text(row(1, 'RW 0x401002 waits for S-2') + row(2, 'RW 0x402002'), encoding='utf-8')
    stops = read_stops([p])
    inv = Inventory([{'address': 0x401000, 'blocks': [[0x401000, 32]]},
                     {'address': 0x402000, 'blocks': [[0x402000, 32]]}])
    enrich(stops, {}, set(), inv)
    groups, unplaced = cluster(stops, {}, limit=1)
    out = tmp_path/'report'
    output(stops, groups, unplaced, {}, [], [], None, out, 'regenerate')
    with out.with_suffix('.tsv').open(encoding='utf-8') as f:
        rows = {r['id']: r for r in csv.DictReader(f, delimiter='\t')}
    assert rows['S-1']['group'] != rows['S-2']['group']
    assert 'cross-group reference S-2' in rows['S-1']['why']
    assert 'cross-group reference S-1' in rows['S-2']['why']
    assert rows['S-2']['referenced by'] == 'S-1'


def test_generic_class_names_are_metadata_not_placement(tmp_path):
    p = tmp_path/'stops.md'
    p.write_text(row(1, 'Object and Player are named without a mechanism'), encoding='utf-8')
    stops = read_stops([p])
    funcs = {0x401000: {'names': {'Object::setTeam (tier A)', 'Player::update (tier A)'},
                        'classes': {'ModuleX'}, 'status': 'none'}}
    enrich(stops, funcs, {'ModuleX'}, Inventory([]), {'Object', 'Player'})
    assert stops['S-1'].port_classes == {'Object', 'Player'}
    assert stops['S-1'].rotwk_classes == {'Object', 'Player'}
    assert not stops['S-1'].classes
    assert cluster(stops, funcs)[1] == ['S-1']
