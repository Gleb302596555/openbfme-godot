"""Synthetic matrix/INI fixtures; no retail or mod files."""
import csv
import sys
import zipfile
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).parent))
import matrix_grey as m

INI = '''
#define TEST_UPGRADE Upgrade_Fictional
Upgrade Upgrade_Fictional
  BuildCost = 125
  RequiredObjectFilter = ANY +Workshop
End
CommandButton BuyUpgrade
  Command = PLAYER_UPGRADE
  Upgrade = TEST_UPGRADE
End
CommandButton NeedsUpgrade
  Command = STOP
  Options = NEED_UPGRADE
  NeededUpgrade = TEST_UPGRADE
End
CommandButton BuyScience
  Command = PURCHASE_SCIENCE
  Science = SCIENCE_Fictional
End
CommandButton NeedsScience
  Command = SPECIAL_POWER
  SpecialPower = FictionalPower
End
SpecialPower FictionalPower
  RequiredSciences = SCIENCE_Fictional
End
CommandButton NeedsObject
  Command = STOP
  RequiredObject = Workshop
End
CommandButton NeedsModel
  Command = STOP
  EnableOnModelCondition = MOUNTED
End
CommandButton NoGate
  Command = STOP
End
CommandButton ToggleWeapon
  Command = TOGGLE_WEAPONSET
End
CommandSet FactoryCommands
  1 = BuyUpgrade
  2 = ToggleWeapon
End
Object Workshop
  CommandSet = FactoryCommands
  Behavior = SpecialPowerModule ModuleTag_Pulse
    SetModelCondition = ModelConditionState:USER_1
    SetModelConditionTime = 2
  End
  Behavior = ToggleMountedSpecialAbilityUpdate ModuleTag_Mount
  End
End
'''


def fixture(tmp_path):
    ini = tmp_path / 'data/ini/commands.ini'
    ini.parent.mkdir(parents=True)
    ini.write_text(INI, encoding='ascii')
    return m.Explainer(m.load(tmp_path))


def row(button):
    return {'faction': 'FakeFaction', 'object': 'FakeObject', 'button': button, 'result': 'DISABLED'}


def test_gates_and_producer_references(tmp_path):
    e = fixture(tmp_path)
    upgrade = e.explain(row('NeedsUpgrade'))
    assert [(x['condition'], x['value'], x['satisfied by']) for x in upgrade] == [
        ('option', 'NEED_UPGRADE', '-'), ('upgrade', 'Upgrade_Fictional', 'Workshop'),
        ('other', '125', '-'), ('object', 'ANY +Workshop', 'Workshop')]
    science = e.explain(row('NeedsScience'))
    assert [(x['condition'], x['value'], x['satisfied by']) for x in science] == [
        ('science', 'SCIENCE_Fictional', 'BuyScience')]
    obj = e.explain(row('NeedsObject'))[0]
    assert (obj['condition'], obj['value'], obj['satisfied by'], obj['note']) == ('object', 'Workshop', 'Workshop', 'CommandButton.RequiredObject')
    model = e.explain(row('NeedsModel'))[0]
    assert (model['condition'], model['value'], model['satisfied by']) == ('model', 'MOUNTED', '-')
    assert e.explain(row('NoGate'))[0]['condition'] == 'none'
    upgrade_button = e.explain(row('BuyUpgrade'))
    assert {(x['condition'], x['value']) for x in upgrade_button} == {('other', '125'), ('object', 'ANY +Workshop')}


def test_zip_and_csv_output(tmp_path):
    archive = tmp_path / 'ini.zip'
    with zipfile.ZipFile(archive, 'w') as z:
        z.writestr('data/ini/commands.ini', INI)
    matrix = tmp_path / 'matrix.tsv'
    matrix.write_text('# header\nfaction\tobject\tbutton\tresult\tdetail\nF\tO\tNoGate\tDISABLED\tx\nF\tO\tNeedsModel\tPASS\tx\n', encoding='utf-8')
    out = tmp_path / 'out.tsv'
    assert m.main(['--ini', str(archive), '--matrix', str(matrix), '--out', str(out)]) == 0
    with out.open(encoding='utf-8', newline='') as f:
        rows = list(csv.DictReader(f, delimiter='\t'))
    assert len(rows) == 1 and rows[0]['condition'] == 'none'


def test_bad_references_and_missing_includes_fail_loudly(tmp_path):
    e = fixture(tmp_path)
    with pytest.raises(ValueError, match='unknown matrix button'):
        e.explain(row('Absent'))
    ini = tmp_path / 'data/ini/commands.ini'
    ini.write_text('#include "missing.inc"\n', encoding='ascii')
    with pytest.raises(ValueError, match='missing includes'):
        m.load(tmp_path)


def test_model_setters_only_for_the_selected_object(tmp_path):
    e = fixture(tmp_path)
    assert e.model_producers('Workshop', 'USER_1') == 'Workshop:ModuleTag_Pulse'
    assert e.model_producers('Workshop', 'MOUNTED') == 'Workshop:ModuleTag_Mount (mount replacement)'
    assert e.model_producers('Workshop', 'WEAPONSET_TOGGLE_1') == 'ToggleWeapon (weapon-set toggle)'
    assert e.model_producers('Workshop', 'USER_9') == '-'
    assert e.model_producers('Unknown', 'USER_1') == '-'

def test_markdown_report_counts_and_branches_without_mod_values(tmp_path):
    archive = tmp_path / 'ini.zip'
    with zipfile.ZipFile(archive, 'w') as z:
        z.writestr('data/ini/commands.ini', INI)
    matrix = tmp_path / 'matrix.tsv'
    matrix.write_text('# header\nfaction\tobject\tbutton\tresult\tdetail\n'
                      'F\tO\tNoGate\tDISABLED\tstate 2\nF\tO\tNeedsModel\tDISABLED\tstate 4\n'
                      'F\tO\tNeedsUpgrade\tPASS\tstate 1\n', encoding='utf-8')
    out, md = tmp_path / 'out.tsv', tmp_path / 'MATRIX_GREY.md'
    assert m.main(['--ini', str(archive), '--matrix', str(matrix), '--out', str(out), '--md', str(md)]) == 0
    text = md.read_text(encoding='utf-8')
    assert '| matrix rows with result DISABLED | 2 |' in text
    assert '| buttons with no recognized gating field (`none`) | 1 |' in text
    assert '| none | 1 |' in text and '| model | 1 |' in text
    assert '| state 2 | 1 |' in text and '| state 1 | 0 |' not in text  # only the DISABLED rows are counted
    assert '`none` is not "unexplained gray"' in text
    # the retail comparison: the gate this issue ported, the stop it left, and retail's order
    assert '## RW 0x942733 against ControlBar::evaluate' in text
    assert '| RW | what it decides | the port | verdict | evidence |' in text
    assert '0x942826' in text and 'PORTED HERE (PORT-220)' in text and '| same | dis this session |' in text
    assert '0x9427EB' in text and 'S-7427' in text and 'missing' in text
    assert text.index('0x9427BD') < text.index('0x942826') < text.index('0x942833')
    assert 'PR #518' in text and 'python tools/mod/matrix_grey.py' in text
    # the committable half carries no fixture name or value
    for leaked in ('NoGate', 'NeedsModel', 'NeedsUpgrade', 'Upgrade_Fictional', 'Workshop', 'FakeFaction'):
        assert leaked not in text


PAUSED_INI = '''
CommandButton PausedPower
  Command = SPECIAL_POWER
  SpecialPower = PausedSP
End
SpecialPower PausedSP
  ReloadTime = 1000
End
Object PausedHero
  Behavior = UnpauseSpecialPowerUpgrade ModuleTag_Unpause
    SpecialPowerTemplate = PausedSP
    TriggeredBy = Upgrade_Level_1
  End
  Behavior = SpecialPowerModule ModuleTag_Power
    SpecialPowerTemplate = PausedSP
    StartsPaused = Yes
  End
End
Object FreeHero
  Behavior = UnpauseSpecialPowerUpgrade ModuleTag_Unpause
    SpecialPowerTemplate = PausedSP
    TriggeredBy = Upgrade_Level_1
  End
  Behavior = SpecialPowerModule ModuleTag_Power
    SpecialPowerTemplate = PausedSP
  End
End
'''


def test_a_power_that_starts_paused_needs_its_unpause_upgrade(tmp_path):
    """2026-11-18 (Port 281): RW 0x943585 restricts a paused power; StartsPaused = Yes holds until the UnpauseSpecialPowerUpgrade of the same
    power triggers, so its TriggeredBy upgrade is the button's condition. A power that does not start paused needs nothing of it."""
    ini = tmp_path / 'data/ini/paused.ini'
    ini.parent.mkdir(parents=True)
    ini.write_text(PAUSED_INI, encoding='ascii')
    e = m.Explainer(m.load(tmp_path))
    paused = e.explain({'faction': 'F', 'object': 'PausedHero', 'button': 'PausedPower'})
    assert [(x['condition'], x['value']) for x in paused] == [('upgrade', 'Upgrade_Level_1')]
    assert 'ModuleTag_Unpause' in paused[0]['note']
    free = e.explain({'faction': 'F', 'object': 'FreeHero', 'button': 'PausedPower'})
    assert [x['condition'] for x in free] == ['none']


WRITERS_INI = '''
Upgrade Upgrade_Finished
  Type = OBJECT
End
ModifierList DelayedGrant
  Category = BUFF
  Duration = 1800
  Upgrade = Upgrade_Finished Delay:650
End
ModifierList StartupMark
  Category = BUFF
  Duration = 2400
  ModelCondition = USER_8
End
ModifierList PowerMark
  Category = BUFF
  ModelCondition = HERO
End
ModifierList WeaponMark
  Category = WEAPON
  ModelCondition = USER_1
End
ModifierList AuraMark
  Category = BUFF
  ModelCondition = USER_9
End
ModifierList AreaMark
  Category = BUFF
  ModelCondition = USER_3
End
Weapon Signal
  AttributeModifierNugget
    AttributeModifier = WeaponMark
    Radius = 130
    SpecialObjectFilter = NONE +URUK -Excluded ALLIES
  End
End
Weapon Axe
End
SpecialPower ChooseAxe
  Enum = SPECIAL_KNIFE_FIGHTER
End
SpecialPower Restore
  Enum = SPECIAL_ATHELAS
End
CommandButton PressAxe
  Command = SPECIAL_POWER
  SpecialPower = ChooseAxe
End
CommandButton PressRestore
  Command = SPECIAL_POWER
  SpecialPower = Restore
End
CommandButton PressOther
  Command = SPECIAL_POWER
  SpecialPower = Restore
  EnableOnModelCondition = USER_2
End
CommandButton Purchase
  Command = STOP
  DisableOnModelCondition = USER_8
End
CommandSet HeroSet
  1 = PressAxe
  2 = PressRestore
  3 = PressOther
End
Object Hero
  CommandSet = HeroSet
  WeaponSet
    Conditions = None
    Weapon = SECONDARY Axe
  End
  Behavior = WeaponModeSpecialPowerUpdate LockTag
    SpecialPowerTemplate = ChooseAxe
    LockWeaponSlot = SECONDARY
    StartsPaused = Yes
  End
  Behavior = UnpauseSpecialPowerUpgrade UnlockTag
    SpecialPowerTemplate = ChooseAxe
    TriggeredBy = Upgrade_Ready
  End
  Behavior = SpecialPowerModule CastTag
    SpecialPowerTemplate = Restore
    AttributeModifierAffectsSelf = Yes
    AttributeModifier = DelayedGrant
  End
  Behavior = ModelConditionUpgrade GrantTag
    TriggeredBy = Upgrade_Finished
    AddConditionFlags = USER_2
  End
End
ChildObject Ward Hero
  Behavior = SpecialPowerModule CastTag
    SpecialPowerTemplate = Restore
    AttributeModifierAffectsSelf = No
    AttributeModifier = DelayedGrant
  End
End
Object DisplayOnly
  Draw = SomeDraw DrawTag
    ModelConditionState = USER_3
    End
  End
End
Object Tower
  Behavior = AttributeModifierUpgrade StartupTag
    AttributeModifier = StartupMark
    TriggeredBy = Upgrade_Setup
  End
End
Object Officer
  Behavior = AttributeModifierUpgrade HeroTag
    TriggeredBy = Upgrade_Rank
    AttributeModifier = PowerMark
  End
  Behavior = FireWeaponUpdate PulseTag
    HeroModeTrigger = Yes
    FireWeaponNugget
      WeaponName = Signal
    End
  End
End
Object Follower
  KindOf = URUK HORDE
  Behavior = HorseHordeContain HordeTag
    AlternateFormation = FollowerWedge
    ThisFormationIsTheMainFormation = Yes
  End
End
ChildObject FollowerWedge Follower
  Behavior = HorseHordeContain HordeTag
    AlternateFormation = Follower
    ThisFormationIsTheMainFormation = No
  End
End
ChildObject PlainChild Follower
End
ChildObject Excluded Follower
End
ChildObject NotUruk Follower
  KindOf = INFANTRY
End
Object Shrine
  Behavior = PassiveAreaEffectBehavior AreaTag
    ModifierName = AreaMark
    AllowFilter = NONE +Hero
  End
End
Object Stable
  Behavior = AttributeModifierAuraUpdate AuraTag
    TriggeredBy = Upgrade_Stable
    StartsActive = No
    BonusName = AuraMark
    ObjectFilter = ANY +Follower +Beast ALLIES
  End
End
Object Beast
  KindOf = CAVALRY
End
'''


@pytest.fixture
def writers(tmp_path):
    ini = tmp_path / 'data/ini/writers.ini'
    ini.parent.mkdir(parents=True)
    ini.write_text(WRITERS_INI, encoding='ascii')
    return m.Explainer(m.load(tmp_path))


def test_model_producers_prefer_pressable_buttons_and_keep_the_upgrade(writers):
    assert writers.model_producers('hero', 'WEAPONLOCK_SECONDARY') == 'PressAxe (weapon slot lock after Upgrade_Ready)'
    assert writers.model_producers('Hero', 'WEAPONLOCK_PRIMARY') == '-'
    assert writers.model_producers('Hero', 'USER_2') == 'PressRestore (DelayedGrant grants Upgrade_Finished), Upgrade_Finished'
    assert writers.model_producers('Ward', 'USER_2') == 'Upgrade_Finished'  # no self grant, even with the same button


def test_model_producers_report_actual_external_emitters_and_filters(writers):
    assert writers.model_producers('Follower', 'USER_1') == 'Officer:PulseTag (Upgrade_Rank; allied modifier weapon)'
    assert writers.model_producers('Excluded', 'USER_1') == '-'
    assert writers.model_producers('NotUruk', 'USER_1') == '-'
    assert writers.model_producers('Hero', 'USER_3') == 'Shrine:AreaTag (allied passive area)'
    assert writers.model_producers('Ward', 'USER_3') == '-'
    assert writers.model_producers('DisplayOnly', 'USER_3') == '-'
    assert writers.model_producers('Beast', 'USER_9') == 'Stable:AuraTag (Upgrade_Stable; allied aura)'


def test_formation_prerequisite_is_not_generic_child_equivalence(writers):
    assert writers.model_producers('Follower', 'USER_9') == 'Stable:AuraTag (Upgrade_Stable; allied aura)'
    assert writers.model_producers('FollowerWedge', 'USER_9') == (
        'Stable:AuraTag (Upgrade_Stable; allied aura; only when Follower changes formation, not a standalone spawn)')
    assert writers.model_producers('PlainChild', 'USER_9') == '-'


def test_disable_model_condition_reports_expiry_not_the_upgrade_as_an_unlock(writers):
    result = writers.explain({'faction': 'F', 'object': 'Tower', 'button': 'Purchase'})
    assert [(r['value'], r['satisfied by']) for r in result] == [('USER_8', 'wait for expiry (Tower:StartupTag, Upgrade_Setup)')]
    assert 'mask must be absent' in result[0]['note']
