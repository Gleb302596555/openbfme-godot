#!/usr/bin/env python3
"""Explain the DISABLED matrix rows from an extracted INI tree or INI zip.

Uses ennorath_graph's include/define/inheritance handling. The generated TSV
contains game/mod data: keep it outside Git (for example in workspace/).
Conditions are data references, not proof that a sweep fixture satisfies them.
This is an initial inventory: model-condition setters are only partially resolved
and nested prerequisite blocks are not resolved; none is not a complete verdict.

--md writes MATRIX_GREY.md: the counts and the comparison of ControlBar::evaluate's
common gates with retail getCommandAvailability (RW 0x942733). The report holds
counts and retail addresses only, never the mod's names or values, so it can be
committed while the TSV cannot.
"""
from __future__ import annotations

import argparse
import csv
import sys
import zipfile
from collections import Counter, defaultdict
from pathlib import Path

import ennorath_graph as graph

ROOT = Path(__file__).resolve().parents[2]
COLUMNS = ('faction', 'object', 'button', 'condition', 'value', 'satisfied by', 'note')
GATE_OPTIONS = {'NEED_UPGRADE', 'NEED_SPECIAL_POWER_SCIENCE', 'NEEDS_CASTLE_KINDOF',
                'NOT_QUEUEABLE', 'SINGLE_USE_COMMAND', 'HIDE_WHILE_DISABLED',
                'MOUNTED_ONLY', 'UNMOUNTED_ONLY', 'ON_GROUND_ONLY'}
BUTTON_FIELDS = {
    'neededupgrade': 'upgrade', 'science': 'science', 'requiredobject': 'object',
    'requiredobjectfilter': 'object', 'enableonmodelcondition': 'model',
    'disableonmodelcondition': 'model', 'requirelevel': 'level',
}
UPGRADE_FIELDS = {'requiredobjectfilter': 'object', 'prerequisites': 'object',
                  'buildcost': 'other'}
POWER_FIELDS = {'requiredsciences': 'science', 'preventactivationconditions': 'other'}


# ControlBar::evaluate's common gates against retail getCommandAvailability (RW 0x942733), in retail order.
# Addresses were read this session with `tools/mod/rw.py dis 0x942733` (RotWK 2.01 game.dat, MD5 daed3668...);
# "availability" integers are retail's: 0 disabled, 1 enabled, 2 active, 3 hidden, 6 unaffordable.
BRANCHES = (
    ('0x942756 .. 0x94276E', 'the controlling player (RW 0x6A8839); none -> 3',
     'evaluate returns Hidden without a player', 'same', 'dis this session'),
    ('0x942778 .. 0x942795', 'command 0x20 substitutes the natural command center (RW 0x6AD0D9), 0x26 the other center (RW 0x6AD0F8); no object -> 3',
     'not in evaluate: its callers pass the object; the substitution is S-850\'s', 'missing', 'dis this session'),
    ('0x9427A0 .. 0x9427A8', 'Object + 0x457 bits 0/1 (script disabled / unpowered) -> 3',
     'obj.scriptStatus() & 3 (lane SCRIPT-2)', 'same', 'dis this session'),
    ('0x9427AE .. 0x9427B7', 'Object + 0x1C8 byte & 0x20 (DISABLED_UNMANNED) -> 3',
     'obj.isDisabledByType(DISABLED_UNMANNED)', 'same', 'dis this session'),
    ('0x9427BD .. 0x9427E5', 'MOUNTED_ONLY (0x4000000) without the model condition, UNMOUNTED_ONLY (0x8000000) with it -> 0 (RW 0x9438C8)',
     'ModelCondition MOUNTED (lane FACT-A)', 'same', 'dis this session'),
    ('0x9427EB .. 0x942807', 'ON_GROUND_ONLY (0x40000000) and the AI (Object + 0x260) vslot 0x228 = RW 0x66288A true -> 0',
     'nothing: AI + 0x1F0 / + 0x1FC / + 0x140 and RW 0x5E2DF4 / 0x661E50 have no counterpart (stop S-7427)', 'missing', 'dis this session'),
    ('0x94280D .. 0x942820', 'command != 0x18 (SPECIAL_POWER, RW 0xDA4D10\'s index 24): RW 0x942490, the Enable/DisableOnModelCondition masks; its result 3 -> 0',
     'modelConditionsAllow, skipped for SPECIAL_POWER as retail does (it defers to RW 0x9435C3 / 0x9435DC after readiness)', 'same', 'dis this session'),
    ('0x942826 .. 0x94282D', 'Object + 0x45B (the single-use command flag) set -> 0, for every command',
     'PORTED HERE (PORT-220): Object::hasSingleUseCommandBeenUsed; the writer is processCommandUI\'s RW 0x94066B in pressButton', 'same', 'dis this session'),
    ('0x942833 .. 0x94284E', 'no experience tracker (Object + 0x26C) -> 3; RequireLevel (button + 0x108) above the rank -> 3',
     'getExperienceTracker / m_requireLevel', 'same', 'dis this session'),
    ('0x942854 .. 0x9428C6', 'the disabled mask: TEMPORARILY_BUSY alone ignored, otherwise recurse with forceDisabled and hide only if the recursion hides; commands 0x11 / 0x28 / 0x10 / 0x14 / 0x15 / 0x1B exempt',
     'evaluate\'s ignoreDisabled recursion over the same six commands', 'same', 'dis this session'),
    ('0x9428A3 .. 0x942993', 'NEED_UPGRADE (0x40): every NeededUpgrade (+ 0x28), or any with NeededUpgradeAny (+ 0x34); PLAYER -> RW 0x6AC2AF, OBJECT -> RW 0x691421; missing -> 0, except a missing OBJECT entry hides a build button whose template is Buildable = No / Only_By_AI for a human -> 3',
     'lane FACT-B, both lists and the Buildable exception', 'same', 'port comment + docs/re/rotwk/RW_0x942733.md'),
    ('0x942993 .. 0x942A1B', 'NEEDS_CASTLE_KINDOF (0x800) through the callback RW 0x942348: no owned object of AffectsKindOf -> 3',
     'castleKindOfAllows (PORT-130); the callback\'s receiver and ordered scan stay in the RW doc\'s unresolved list', 'same', 'port comment + docs/re/rotwk/RW_0x942733.md'),
    ('0x942A2B .. 0x942A58', 'the production interface with NOT_QUEUEABLE (0x10000) and a first production -> 0',
     'getProductionUpdate()->firstProduction()', 'same', 'port comment + docs/re/rotwk/RW_0x942733.md'),
    ('0x9438CF ..', 'the per-command jump table: every GUI_COMMAND arm after the common gates',
     'partly ported, arm by arm; the inventory and the remaining arms are docs/re/rotwk/RW_0x942733.md\'s and S-290\'s, not re-read this session', 'differs', 'docs/re/rotwk/RW_0x942733.md'),
    ('arms 1 / 0x35 / 3', 'PR #518 (Port 212, issue #502) moves FOUNDATION_CONSTRUCT and DOZER_CONSTRUCT out of the UNIT_BUILD arm so a plot\'s button no longer asks canMakeUnit',
     'not touched here: this branch stays in the common gates, before the switch; its own verdict and stop S-7444 are that PR\'s', 'same', 'gh pr diff 518'),
)


class IniVfs(graph.Vfs):
    """Read the effective mounted INI snapshot without extracting or committing it."""
    def __init__(self, source: Path):
        super().__init__()
        self.archive = None
        if source.is_dir():
            for p in source.rglob('*'):
                if p.is_file():
                    name = p.relative_to(source).as_posix().lower()
                    if name.startswith('data/ini/') and name.endswith(('.ini', '.inc', '.str')):
                        self.files[name] = (p, 0, p.stat().st_size, 'snapshot')
        elif zipfile.is_zipfile(source):
            self.archive = zipfile.ZipFile(source)
            self.members = {}
            for info in self.archive.infolist():
                name = info.filename.replace('\\', '/').lower()
                if name.startswith('data/ini/') and name.endswith(('.ini', '.inc', '.str')):
                    if name in self.members:
                        raise ValueError(f'duplicate case-insensitive archive path: {name}')
                    self.members[name] = info.filename
                    self.files[name] = (source, 0, info.file_size, 'snapshot')
        else:
            raise ValueError(f'not a directory or zip: {source}')
        if not self.files:
            raise ValueError('snapshot has no data/ini files')

    def read(self, name):
        key = name.lower().replace('\\', '/')
        if self.archive is not None:
            return self.archive.read(self.members[key]).decode('latin1') if key in self.members else None
        return super().read(key)

    def close(self):
        if self.archive is not None:
            self.archive.close()


def load(source: Path):
    vfs = IniVfs(source)
    graph.MISSING.clear()
    try:
        db = graph.load_db(vfs)
        if graph.MISSING:
            raise ValueError('missing includes: ' + ', '.join(sorted(set(graph.MISSING))))
        return db
    finally:
        vfs.close()


class Explainer:
    def __init__(self, db):
        self.db = db
        self.names = {(typ, name.lower()): name for typ, name in db.defs}
        self.upgrade_buttons = defaultdict(set)
        self.science_buttons = defaultdict(set)
        self.button_sets = defaultdict(set)
        self.set_objects = defaultdict(set)
        for typ, name in db.defs:
            fields = self.fields(typ, name)
            if typ == 'commandbutton':
                commands = {v.upper() for k, v in fields if k.lower() == 'command'}
                # A NeededUpgrade is a prerequisite, not a producer.
                for k, v in fields:
                    if k.lower() == 'upgrade' and commands & {'PLAYER_UPGRADE', 'OBJECT_UPGRADE', 'CASTLE_UPGRADE'}:
                        for token in graph.tokens_of(v):
                            self.upgrade_buttons[token.lower()].add(name)
                    if k.lower() == 'science' and 'PURCHASE_SCIENCE' in commands:
                        for token in graph.tokens_of(v):
                            self.science_buttons[token.lower()].add(name)
            elif typ == 'commandset':
                for k, v in fields:
                    if k.isdigit():
                        for token in graph.tokens_of(v):
                            self.button_sets[token.lower()].add(name)
            elif typ == 'object':
                for k, v in fields:
                    if k.lower() == 'commandset':
                        for token in graph.tokens_of(v):
                            self.set_objects[token.lower()].add(name)

    def fields(self, typ, name):
        actual = self.names.get((typ, name.lower()))
        if actual is None:
            return []
        return [(k, v) for k, v, _src, module in graph.effective_fields(self.db, typ, actual) if not module]

    def producers(self, typ, value):
        tokens = list(graph.tokens_of(value))
        if typ == 'upgrade':
            producers = set()
            for token in tokens:
                for button in self.upgrade_buttons.get(token.lower(), ()):
                    for cmdset in self.button_sets.get(button.lower(), ()):
                        producers.update(self.set_objects.get(cmdset.lower(), ()))
            return ', '.join(sorted(producers)) or '-'
        if typ == 'science':
            buttons = set()
            for token in tokens:
                buttons.update(self.science_buttons.get(token.lower(), ()))
            return ', '.join(sorted(buttons)) or '-'
        if typ == 'object':
            objects = {self.names[('object', t.lower())] for t in tokens if ('object', t.lower()) in self.names}
            return ', '.join(sorted(objects)) or '-'
        return '-'

    def model_producers(self, object_name, value):
        """Only identify explicit setters on this object; animation states are readers."""
        name = self.names.get(('object', object_name.lower()))
        if not name:
            return '-'
        fields = graph.effective_fields(self.db, 'object', name)
        wanted = {token.upper() for token in graph.tokens_of(value)}
        setters = set()
        for key, val, _src, tag in fields:
            if tag and key.lower() == 'setmodelcondition':
                # SpecialPowerModule::trigger RW 0x897987: this field sets the
                # condition only when its duration is positive and it triggers.
                if wanted.intersection(t.upper() for t in graph.tokens_of(val)):
                    setters.add(object_name + ':' + tag)
            elif tag and key.lower() == 'behavior' and 'MOUNTED' in wanted:
                if val.split() and val.split()[0].lower() == 'togglemountedspecialabilityupdate':
                    setters.add(object_name + ':' + tag + ' (mount replacement)')
        if 'WEAPONSET_TOGGLE_1' in wanted:
            # RW 0x77B529 / WeaponSetToggle::registerHandlers: this command
            # toggles weapon-set flag 1, mirrored to the model condition.
            sets = {token.lower() for key, val, _src, tag in fields
                    if not tag and key.lower() == 'commandset' for token in graph.tokens_of(val)}
            for (typ, button), _definition in self.db.defs.items():
                if typ != 'commandbutton' or not (self.button_sets.get(button.lower(), set())
                                                   and any(s.lower() in sets for s in self.button_sets[button.lower()])):
                    continue
                if any(k.lower() == 'command' and v.upper() == 'TOGGLE_WEAPONSET' for k, v in self.fields(typ, button)):
                    setters.add(button + ' (weapon-set toggle)')
        return ', '.join(sorted(setters)) or '-'

    def explain(self, row):
        button = row['button']
        if ('commandbutton', button.lower()) not in self.names:
            raise ValueError(f'unknown matrix button: {button}')
        fields = self.fields('commandbutton', button)
        result = []
        def emit(kind, value, note):
            result.append({**{k: row[k] for k in ('faction', 'object', 'button')},
                           'condition': kind, 'value': value,
                           'satisfied by': self.model_producers(row['object'], value) if kind == 'model' else self.producers(kind, value), 'note': note})
        upgrades, powers = [], []
        for key, value in fields:
            low = key.lower()
            if low in BUTTON_FIELDS:
                note = 'CommandButton.' + key
                if BUTTON_FIELDS[low] == 'model':
                    note += ' (candidate setters; activation and duration not verified)'
                emit(BUTTON_FIELDS[low], value, note)
                if low == 'neededupgrade':
                    upgrades.extend(graph.tokens_of(value))
            elif low == 'options':
                for opt in value.split():
                    if opt.upper() in GATE_OPTIONS:
                        emit('option', opt, 'CommandButton.Options')
            elif low == 'upgrade':
                upgrades.extend(graph.tokens_of(value))
            elif low == 'specialpower':
                powers.extend(graph.tokens_of(value))
            elif low == 'neededupgradeany':
                emit('other', value, 'CommandButton.' + key)
        for typ, refs, allowed in (('upgrade', upgrades, UPGRADE_FIELDS), ('specialpower', powers, POWER_FIELDS)):
            for ref in dict.fromkeys(refs):
                if (typ, ref.lower()) not in self.names:
                    raise ValueError(f'unknown {typ}: {ref} in {button}')
                for key, value in self.fields(typ, ref):
                    if key.lower() in allowed:
                        emit(allowed[key.lower()], value, typ + '.' + key + ' (' + ref + ')')
        if not result:
            emit('none', '-', 'No recognized gating field; runtime and fixture checks still apply')
        return result


def markdown(rows, result, matrix_name, ini_name):
    """The committable half of the report: counts and the retail comparison, never a mod name or value."""
    counts = Counter(r['condition'] for r in result)
    options = Counter(r['value'] for r in result if r['condition'] == 'option')
    details = Counter(r.get('detail', '') for r in rows)
    none_buttons = sorted({r['button'] for r in result if r['condition'] == 'none'})
    out = [
        '# MATRIX_GREY: why the DISABLED buttons of the matrix are gray',
        '',
        f'Generated by `tools/mod/matrix_grey.py` from `{matrix_name}` and a mounted INI snapshot',
        f'(`{ini_name}`). The per-condition rows carry the mod\'s names and values, so they stay in the',
        'TSV outside Git; this report holds counts and retail addresses only.',
        '',
        '## Counts',
        '',
        '| measure | value |',
        '|---|---|',
        f'| matrix rows with result DISABLED | {len(rows)} |',
        f'| unique buttons behind them | {len({r["button"] for r in rows})} |',
        f'| condition rows in the TSV | {len(result)} |',
        f'| buttons with no recognized gating field (`none`) | {len(none_buttons)} |',
        '',
        '| condition | rows |',
        '|---|---|',
    ]
    out += [f'| {k} | {counts[k]} |' for k in sorted(counts)]
    out += ['', '| Options bit seen (retail names, RW 0xDA4C88) | rows |', '|---|---|']
    out += [f'| {k} | {options[k]} |' for k in sorted(options)]
    if any(details):
        out += ['', '| matrix `detail` | rows |', '|---|---|']
        out += [f'| {k or "(empty)"} | {v} |' for k, v in sorted(details.items())]
    out += [
        '',
        '`none` is not "unexplained gray": it says the button carries no INI field this tool',
        'recognizes as a gate. Retail\'s common gates below still apply to it (its object may be',
        'disabled, unpowered, out of rank, single-use spent), and the sweep fixture\'s own state',
        '(upgrades researched, sciences owned, buildings present) decides the rest.',
        '',
        '## RW 0x942733 against ControlBar::evaluate',
        '',
        'Retail\'s availability integers are not the port\'s ButtonState ordinals: 0 disabled,',
        '1 enabled, 2 active, 3 hidden, 6 unaffordable; the port shows 0 as Restricted and 3 as Hidden.',
        'Every address was read linearly from vanilla RotWK 2.01 (`tools/mod/rw.py dis`, the owner\'s',
        '`game.dat`, MD5 `daed3668...`); the option names come from retail\'s own table RW 0xDA4C88 and',
        'the command numbers from RW 0xDA4D10.',
        '',
        '| RW | what it decides | the port | verdict | evidence |',
        '|---|---|---|---|---|',
    ]
    out += [f'| {rw} | {rule} | {port} | {verdict} | {ev} |' for rw, rule, port, verdict, ev in BRANCHES]
    out += [
        '',
        'The single-use gate (Object + 0x45B) is this issue\'s port; its writers are the constructor',
        'RW 0x699C88, the setter RW 0x940397 and processCommandUI\'s inline RW 0x94066B, and a linear',
        'scan of the whole `.text` finds no other access and nothing that clears it again.',
        'ON_GROUND_ONLY stays a stop (S-7427). The deeper analysis, the census and the remaining',
        'unresolved branches are `docs/ennorath/PORT220_ANALYSIS.md`; the arm inventory of the',
        'per-command switch is `docs/re/rotwk/RW_0x942733.md`.',
        '',
        '## The `none` list and the run',
        '',
        f'The {len(none_buttons)} buttons whose row is `none` are listed in the TSV next to every other row.',
        '`none` means the button carries no INI field the tool recognizes as a gate, so its gray comes from',
        'a runtime state (the common gates above: disabled, unpowered, out of rank, single-use spent) or',
        'from the sweep fixture\'s own state, not from a data requirement the sweep could buy.',
        '',
        '```',
        'python tools/mod/matrix_grey.py \\',
        f'    --ini <the mounted INI snapshot, a directory or a zip> \\',
        f'    --matrix {matrix_name} \\',
        '    --out docs/ennorath/MATRIX_GREY.tsv --md docs/ennorath/MATRIX_GREY.md',
        '```',
        '',
        'Both halves come from one run and sit next to `docs/ennorath/MATRIX.tsv`, the sweep it reads: the',
        'TSV is the per-condition table (`faction object button condition value satisfied by note`), this MD',
        'the counts and the retail comparison. Neither names the mod\'s game files or quotes their contents',
        'beyond the INI field values the sweep already published in `MATRIX.tsv`.',
        '',
    ]
    return '\n'.join(out)


def read_matrix(path):
    with path.open(encoding='utf-8', newline='') as f:
        reader = csv.DictReader((line for line in f if not line.startswith('#')), delimiter='\t')
        if not reader.fieldnames or not {'faction', 'object', 'button', 'result'} <= set(reader.fieldnames):
            raise ValueError('matrix is missing required columns')
        return [r for r in reader if r['result'] == 'DISABLED']


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--ini', type=Path, required=True, help='mounted INI snapshot directory or zip')
    ap.add_argument('--matrix', type=Path, default=ROOT / 'docs/ennorath/MATRIX.tsv')
    ap.add_argument('--out', type=Path, required=True, help='TSV output (keep mod-derived values out of Git)')
    ap.add_argument('--md', type=Path, help='Markdown report (counts and the retail comparison; no mod values, so it may be committed)')
    args = ap.parse_args(argv)
    rows = read_matrix(args.matrix)
    explainer = Explainer(load(args.ini))
    result = [item for row in rows for item in explainer.explain(row)]
    args.out.parent.mkdir(parents=True, exist_ok=True)
    with args.out.open('w', encoding='utf-8', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=COLUMNS, delimiter='\t', lineterminator='\n')
        writer.writeheader()
        writer.writerows(result)
    counts = Counter(r['condition'] for r in result)
    if args.md:
        args.md.parent.mkdir(parents=True, exist_ok=True)
        args.md.write_text(markdown(rows, result, args.matrix.name, args.ini.name), encoding='utf-8')
    print(f'{len(rows)} disabled matrix rows; {len(result)} condition rows; ' + ', '.join(f'{k}={counts[k]}' for k in sorted(counts)))
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, ValueError, zipfile.BadZipFile) as e:
        sys.exit(str(e))
