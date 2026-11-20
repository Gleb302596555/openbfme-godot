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
import re
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
LOCK_CONDITIONS = {'WEAPONLOCK_' + slot for slot in ('PRIMARY', 'SECONDARY', 'TERTIARY', 'QUATERNARY', 'QUINARY')}


def model_bits(value):
    return {token.upper() for token in graph.tokens_of(value)}


def last(fields, name):
    return fields.get(name, [''])[-1]


def word(fields, name):
    return next(graph.tokens_of(last(fields, name)), '')


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
        self.set_buttons = defaultdict(set)
        self.set_objects = defaultdict(set)
        self._area_writers = None
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
                            self.set_buttons[name.lower()].add(token.lower())
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

    def model_modules(self, name):
        # The graph sometimes loses the tag in a Draw with an extra End, or for a tag
        # not prefixed ModuleTag_. Recover it from the Behavior declaration itself.
        modules = {}
        current = None
        for key, value, _src, scope in graph.effective_fields(self.db, 'object', name):
            key = key.lower()
            if key == 'behavior':
                tokens = value.split()
                current = None
                if len(tokens) == 2:
                    cls, tag = tokens
                    current = defaultdict(list)
                    modules[tag.lower()] = (cls.lower(), tag, current)
            elif key in ('draw', 'body'):
                current = None
            elif current is not None and (not scope or scope == tag):
                current[key].append(value)
        return list(modules.values())

    def modifier_bits(self, name):
        model = clears = ''
        for key, value in self.fields('modifierlist', name):
            if key.lower() == 'modelcondition':
                model = value
            elif key.lower() == 'clearmodelcondition':
                clears = value
        # RW 0x805A8E sets then clears. A list with no numeric Modifier can still set a flag.
        return model_bits(model) - model_bits(clears)

    def buttons_on(self, name):
        sets = [v for k, v in self.fields('object', name) if k.lower() == 'commandset']
        return sorted(self.set_buttons.get(sets[-1].lower(), ())) if sets else []

    def matches_allied_filter(self, filt, name):
        """Static part of the filters on these producers, assuming an allied/same-player source.

        Do not infer a match from a relationship, an unknown macro or ChildObject alone.
        A match is a candidate: the engine still checks actual relation, distance and runtime state.
        """
        parts = filt.lower().split()
        if not parts or parts[0] not in {'none', 'any', 'all'}:
            return False
        if any(not p.startswith(('+', '-')) and p not in {'allies', 'same_player'} for p in parts[1:]):
            return False
        if any(len(p) == 1 or ':' in p for p in parts[1:]):
            return False
        def related(obj):
            fields = self.fields('object', obj)
            return tuple({t.lower() for k, v in fields if k.lower() == field for t in graph.tokens_of(v)}
                         for field in ('equivalentto', 'buildvariations'))
        eq, variations = related(name)
        def equivalent(obj):
            other_eq, other_variations = related(obj)
            return (name.lower() == obj or obj in eq or name.lower() in other_eq or eq & other_eq
                    or obj in variations or name.lower() in other_variations)
        include = {p[1:] for p in parts[1:] if p.startswith('+')}
        exclude = {p[1:] for p in parts[1:] if p.startswith('-')}
        # RW 0x76379C / 0x73D5C2: explicit template names before KindOf masks.
        is_object = lambda token: ('object', token) in self.names
        if any(equivalent(p) for p in include if is_object(p)):
            return True
        if any(equivalent(p) for p in exclude if is_object(p)):
            return False
        kinds = set()
        for key, val in self.fields('object', name):
            if key.lower() == 'kindof':
                tokens = val.lower().split()
                if not any(t.startswith(('+', '-')) for t in tokens):
                    kinds.clear()
                kinds.update(t.lstrip('+') for t in tokens if not t.startswith('-'))
                kinds.difference_update(t[1:] for t in tokens if t.startswith('-'))
        kinds_in = {p for p in include if not is_object(p)}
        return not kinds & exclude and (parts[0] == 'all' or
               (parts[0] == 'any' and bool(kinds & kinds_in)) or bool(kinds_in and kinds_in <= kinds))

    def formation_base(self, name, modules):
        def formation(items):
            for cls, _tag, fields in reversed(items):
                if cls in {'hordecontain', 'horsehordecontain'}:
                    return word(fields, 'alternateformation').lower(), last(fields, 'thisformationisthemainformation').lower()
            return '', ''
        alt, main = formation(modules)
        base = self.names.get(('object', alt))
        if base and main == 'no' and formation(self.model_modules(base)) == (name.lower(), 'yes'):
            return base
        return None

    def area_model_writers(self):
        if self._area_writers is not None:
            return self._area_writers
        writers = defaultdict(set)
        for typ, name in self.db.defs:
            if typ != 'object':
                continue
            modules = self.model_modules(name)
            for cls, tag, fields in modules:
                source = f'{name}:{tag}'
                upgrades = last(fields, 'triggeredby')
                if cls == 'attributemodifierauraupdate' and last(fields, 'targetenemy').lower() != 'yes' \
                        and last(fields, 'affectcontainedonly').lower() != 'yes':
                    if upgrades or last(fields, 'startsactive').lower() == 'yes':
                        filt = last(fields, 'objectfilter')
                        for bit in self.modifier_bits(word(fields, 'bonusname')):
                            writers[bit].add((name, filt, source, f'{upgrades}; allied aura' if upgrades else 'allied aura'))
                elif cls == 'passiveareaeffectbehavior':
                    filt = last(fields, 'allowfilter')
                    required = last(fields, 'upgraderequired')
                    for modifier in fields.get('modifiername', ()):
                        for bit in self.modifier_bits(modifier):
                            writers[bit].add((name, filt, source, f'{required}; allied passive area' if required else 'allied passive area'))
                elif cls == 'fireweaponupdate' and last(fields, 'heromodetrigger').lower() == 'yes':
                    # RW 0x88F554 -> 0x90EAF9. Only follow a proven HERO grant on the emitter.
                    hero_upgrades = set()
                    for grant, _tag, f in modules:
                        if ((grant == 'attributemodifierupgrade' and 'HERO' in self.modifier_bits(word(f, 'attributemodifier'))) or
                                (grant == 'modelconditionupgrade' and 'HERO' in model_bits(last(f, 'addconditionflags')))):
                            hero_upgrades.update(graph.tokens_of(last(f, 'triggeredby')))
                    if not hero_upgrades:
                        continue
                    for weapon in fields.get('weaponname', ()):
                        # The graph flattens nuggets. Require the contiguous AttributeModifier/
                        # Radius/filter shape; ambiguous layouts aren't resolved by this pass.
                        nugget = None
                        for key, value in self.fields('weapon', weapon):
                            key = key.lower()
                            if key == 'attributemodifier':
                                nugget = {'modifier': value}
                            elif nugget is not None and key in {'radius', 'requiredupgradenames'}:
                                nugget[key] = value
                            elif nugget is not None:
                                if key == 'specialobjectfilter' and 'radius' in nugget:
                                    why = ', '.join(sorted(hero_upgrades)) + '; allied modifier weapon'
                                    if 'requiredupgradenames' in nugget:
                                        why += '; nugget needs ' + nugget['requiredupgradenames']
                                    for bit in self.modifier_bits(nugget['modifier']):
                                        writers[bit].add((name, value, source, why))
                                nugget = None
        self._area_writers = writers
        return writers

    def model_producers(self, object_name, value, *, disable=False, gated_button=''):
        """Known trigger chains (RW 0x942490); an animation condition is a reader, not a setter."""
        name = self.names.get(('object', object_name.lower()))
        if not name:
            return '-'
        wanted = model_bits(value)
        if not wanted:
            return '-'
        modules = self.model_modules(name)
        setters, press = set(), set()
        if disable:
            for cls, tag, fields in modules:
                if cls != 'attributemodifierupgrade' or not wanted & self.modifier_bits(word(fields, 'attributemodifier')):
                    continue
                duration = next((v for k, v in self.fields('modifierlist', word(fields, 'attributemodifier'))
                                 if k.lower() == 'duration'), '')
                if re.fullmatch(r'[0-9]+', duration) and int(duration) > 0:
                    setters.add(f'wait for expiry ({name}:{tag}, {last(fields, "triggeredby")})')
            return ', '.join(sorted(setters)) or '-'

        buttons = []
        for lower in self.buttons_on(name):
            actual = self.names.get(('commandbutton', lower))
            fields = {k.lower(): v for k, v in self.fields('commandbutton', lower)}
            if actual and actual.lower() != gated_button.lower() and not wanted & model_bits(fields.get('enableonmodelcondition', '')):
                buttons.append((actual, fields.get('command', '').upper(), fields.get('specialpower', '').lower()))
        def power_buttons(fields, reason):
            return {f'{button} ({reason})' for button, cmd, power in buttons
                    if cmd == 'SPECIAL_POWER' and power and power == word(fields, 'specialpowertemplate').lower()}
        grants = {}
        slots = {v.split()[0].upper() for k, v, _, _ in graph.effective_fields(self.db, 'object', name)
                 if k.lower() == 'weapon' and len(v.split()) >= 2}
        for cls, tag, fields in modules:
            if cls == 'modelconditionupgrade' and wanted & model_bits(last(fields, 'addconditionflags')):
                for upgrade in graph.tokens_of(last(fields, 'triggeredby')):
                    grants[upgrade.lower()] = upgrade
            if cls == 'attributemodifierupgrade' and wanted & self.modifier_bits(word(fields, 'attributemodifier')):
                setters.update(f'{u} ({name}:{tag})' for u in graph.tokens_of(last(fields, 'triggeredby')))
            if wanted & model_bits(last(fields, 'setmodelcondition')):
                setters.add(f'{name}:{tag}')
            if cls == 'togglemountedspecialabilityupdate' and 'MOUNTED' in wanted:
                setters.add(f'{name}:{tag} (mount replacement)')
            if cls == 'weaponmodespecialpowerupdate' and wanted & LOCK_CONDITIONS:
                slot = last(fields, 'lockweaponslot').upper()
                if slot in slots and 'WEAPONLOCK_' + slot in wanted:
                    unlocks = [u for u, _ in self.unpause_upgrades(name, word(fields, 'specialpowertemplate'))]
                    why = 'weapon slot lock' + (' after ' + '/'.join(unlocks) if unlocks else '')
                    press.update(power_buttons(fields, why))
        for cls, _tag, fields in modules:
            if cls != 'specialpowermodule' or last(fields, 'attributemodifieraffectsself').lower() != 'yes':
                continue
            modifier = word(fields, 'attributemodifier')
            for key, v in self.fields('modifierlist', modifier):
                upgrade = next(graph.tokens_of(v), '').lower()
                if key.lower() == 'upgrade' and upgrade in grants:
                    press.update(power_buttons(fields, f'{modifier} grants {grants[upgrade]}'))
        setters.update(grants.values())
        if 'WEAPONSET_TOGGLE_1' in wanted:
            press.update(f'{b} (weapon-set toggle)' for b, cmd, _ in buttons if cmd == 'TOGGLE_WEAPONSET')

        base = self.formation_base(name, modules)
        for bit in wanted:
            for emitter, filt, source, why in self.area_model_writers().get(bit, ()):
                if emitter != name and self.matches_allied_filter(filt, name):
                    setters.add(f'{source} ({why})')
                elif base and emitter != base and self.matches_allied_filter(filt, base):
                    setters.add(f'{source} ({why}; only when {base} changes formation, not a standalone spawn)')
        # GreyFixture reads a button before the first " (". A press on the selected object
        # must come first; external Object:Tag candidates are not buttons to press there.
        return ', '.join(sorted(press) + sorted(setters - press)) or '-'

    def explain(self, row):
        button = row['button']
        if ('commandbutton', button.lower()) not in self.names:
            raise ValueError(f'unknown matrix button: {button}')
        fields = self.fields('commandbutton', button)
        result = []
        def emit(kind, value, note, *, disable=False):
            if kind == 'model':
                by = self.model_producers(row['object'], value, disable=disable, gated_button=button)
            else:
                by = self.producers(kind, value)
            result.append({**{k: row[k] for k in ('faction', 'object', 'button')},
                           'condition': kind, 'value': value,
                           'satisfied by': by, 'note': note})
        upgrades, powers = [], []
        for key, value in fields:
            low = key.lower()
            if low in BUTTON_FIELDS:
                note = 'CommandButton.' + key
                if low == 'disableonmodelcondition':
                    note += ' (mask must be absent; setting any bit blocks the button)'
                elif low == 'enableonmodelcondition':
                    note += ' (any bit suffices; runtime readiness and actor availability still apply)'
                emit(BUTTON_FIELDS[low], value, note, disable=low == 'disableonmodelcondition')
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
        for power in dict.fromkeys(powers):
            for upgrade, tag in self.unpause_upgrades(row['object'], power):
                emit('upgrade', upgrade, f'UnpauseSpecialPowerUpgrade.TriggeredBy ({tag}): the power starts paused, RW 0x943585 restricts a paused power')
        if not result:
            emit('none', '-', 'No recognized gating field; runtime and fixture checks still apply')
        return result

    def unpause_upgrades(self, object_name, power):
        """The upgrades that unpause `power` on the object (owner 2026-11-18, Port 281's Erkenbrand / Gorkil): retail's SPECIAL_POWER arm
        (RW 0x943585 .. 0x9435D2) restricts a power whose module is paused with no progress; a module with StartsPaused = Yes is paused until
        an UnpauseSpecialPowerUpgrade naming the same SpecialPowerTemplate triggers (its TriggeredBy upgrades). Nothing when no module of the
        power starts paused."""
        name = self.names.get(('object', object_name.lower()))
        if not name:
            return []
        # one group per module, started by its `Behavior =` line: the graph loses the module tags of an object whose Draw block the mod
        # closes with an extra End (Gorkil's WildGoblinKingMounted, 2026-11-18: its modules' fields come untagged), so tags are not used
        modules = defaultdict(list)
        n = 0
        for key, val, _src, tag in graph.effective_fields(self.db, 'object', name):
            if key.lower() == 'behavior':
                n += 1
            if n:
                modules[f'{tag or "-"}#{n}'].append((key.lower(), val))
        def field(fields, key):
            return next((v for k, v in fields if k == key), '')
        def class_of(fields):
            return next((v.split()[0] for k, v in fields if k == 'behavior' and v.split()), '')
        paused = any(field(f, 'specialpowertemplate').strip().lower() == power.lower()
                     and field(f, 'startspaused').strip().split()[:1] == ['Yes'] for f in modules.values())
        if not paused:
            return []
        out = []
        for tag, f in modules.items():
            if class_of(f).lower() == 'unpausespecialpowerupgrade' and field(f, 'specialpowertemplate').strip().lower() == power.lower():
                label = (next((v.split()[1] for k, v in f if k == 'behavior' and len(v.split()) > 1), tag.split('#')[0]))
                out.extend((u, label) for u in graph.tokens_of(field(f, 'triggeredby')))
        return out


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
        # the grey buttons: DISABLED, and GREY-BUG (a grey button the last fixture did not open: its conditions are re-read every run, 2026-11-18)
        return [r for r in reader if r['result'] in ('DISABLED', 'GREY-BUG')]


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
