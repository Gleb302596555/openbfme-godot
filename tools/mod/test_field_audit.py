"""tools/mod/field_audit.py over a made-up mod, engine source, RotWK field tables and coverage table (no mod or game bytes).

The made-up engine has a typed module (TypedThing: one field read, one parsed and never read, one parsed by a row macro of a multi-line #define, one
with its own parser), a raw one (RawThing), a missing one (GoneThing), a module with a one-line or block row (EmotionThing: AddEmotion opens its
nugget only after OVERRIDE, RW 0x8B618C) and a script row (ScriptThing), a real definition block (Weapon) and a recording stub (Bridge)."""
from __future__ import annotations

import json
import sys
from pathlib import Path

import pytest

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import ennorath_graph as eg  # noqa: E402
import field_audit as fa  # noqa: E402

STUBS = r'''
const std::vector<std::string> &RotwkBlockKeywords()
{
	static const std::vector<std::string> keywords = {
		"Bridge", "Object", "Weapon",
	};
	return keywords;
}
'''

TYPED_H = r'''
#pragma once
#include "Common/Module.h"
class TypedThingModuleData : public ModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse &p);
	float m_used = 0.0f;
	float m_unused = 0.0f;
	DieMux m_dieMux;
	int m_fx = 0;
	int m_level = 0;
	std::vector<std::string> m_names;
};
'''

TYPED_CPP = r'''
#include "GameLogic/Module/TypedThing.h"

#define TT_OFF(m) (int)offsetof(TypedThingModuleData, m)
#define MUX_ROWS(T)                                                       \
	{ "DeathTypes", INI::parseInt, nullptr, (int)offsetof(T, m_dieMux.m_deathTypes) }

namespace
{
void parseFX(INI *ini, void *instance, void *, const void *)
{
	static_cast<TypedThingModuleData *>(instance)->m_fx = 1;
}

const FieldParse kTyped[] = {
	{ "Used", INI::parseReal, nullptr, TT_OFF(m_used) },
	{ "Unused", &INI::parseReal, nullptr, TT_OFF(m_unused) },
	{ "FX", parseFX, nullptr, 0 },
	{ "Names", INI::parseAsciiStringVector, nullptr, TT_OFF(m_names) },
	{ "Level", INI::parseInt, nullptr, TT_OFF(m_level) },
	MUX_ROWS(TypedThingModuleData),
	{ nullptr, nullptr, nullptr, 0 }
};
} // namespace

void TypedThingModuleData::buildFieldParse(MultiIniFieldParse &p) { p.add(kTyped); }

// a report line and a block comment naming the member are not reads of it
std::vector<std::string> TypedThing::stopLines() { return { "[S-1] m_unused is kept, nothing reads it" }; }
void TypedThing::note() { /* m_unused */ }

float TypedThing::speed() const
{
	if (m_data->m_dieMux.applies() && m_data->m_fx)
		return m_data->m_used * 2.0f;
	return 0.0f;
}

void TypedThing::registerModule(ModuleFactory &modules)
{
	static const char kName[] = "TypedThing";
	modules.bindTypedData<TypedThingModuleData>(kName, MODULETYPE_BEHAVIOR);
	modules.bindModuleProc(kName, MODULETYPE_BEHAVIOR, nullptr);
}
'''

EMOTION_CPP = r'''
#include "GameLogic/Module/EmotionThing.h"
struct EmotionThingModuleData : public ModuleData
{
	int m_count = 0;
	static void buildFieldParse(MultiIniFieldParse &p);
};
void parseAddEmotion(INI *ini, void *instance, void *, const void *)
{
	static_cast<EmotionThingModuleData *>(instance)->m_count += 1;
}
const FieldParse kEmotion[] = {
	{ "AddEmotion", parseAddEmotion, nullptr, 0 },
	{ nullptr, nullptr, nullptr, 0 }
};
void EmotionThingModuleData::buildFieldParse(MultiIniFieldParse &p) { p.add(kEmotion); }
int EmotionThing::count() const { return m_data->m_count; }
void EmotionThing::registerModule(ModuleFactory &modules)
{
	bind<EmotionThing, EmotionThingModuleData>(modules, "EmotionThing");
}
'''

WEAPON_CPP = r'''
#include "GameLogic/Weapon.h"
class WeaponTemplate
{
public:
	float m_attackRange = 0.0f;
	float m_unread = 0.0f;
	struct Pair { int start, cap; } counts[2];
	static const FieldParse *getFieldParse();
};
// a row storing into an array element (EconomySettings.cpp's ES_OFF(goodMP[0])): the member is the array, not what the parser's body names
void parsePair(INI *ini, void *, void *store, const void *) { static_cast<WeaponTemplate::Pair *>(store)->cap = 1; }
const FieldParse *WeaponTemplate::getFieldParse()
{
	static const FieldParse table[] = {
		{ "AttackRange", INI::parseReal, nullptr, (int)offsetof(WeaponTemplate, m_attackRange) },
		{ "Unread", INI::parseReal, nullptr, (int)offsetof(WeaponTemplate, m_unread) },
		{ "Count2", parsePair, nullptr, (int)offsetof(WeaponTemplate, counts[1]) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	return table;
}
float WeaponTemplate::range() const { return m_attackRange; }
int WeaponTemplate::secondCount() const { return counts[1].start; }
void WeaponStore::registerBlocks(INIBlockRegistry &registry)
{
	registry.registerBlock("Weapon", [](INI *ini) { WeaponStore::parseWeaponTemplateDefinitionGlobal(ini); });
}
'''

# the Object's own fields are read by name (ThingTemplate::findField)
THING_CPP = r'''
#include "Common/Thing/ThingTemplate.h"
float buildCost(const ThingTemplate &tt) { return number(tt.findField("BuildCost")); }
const char *selectVoice(const ThingTemplate &tt) { return text(tt.findField("VoiceSelect")); }
void ThingFactory::registerBlocks(INIBlockRegistry &registry)
{
	registry.registerBlock("Object", [this](INI *ini) { parse(ini); });
}
'''

# a second reader of the Object block with its own table (FormationAssistant's FormationPreviewDecal) and a row storing into a staging struct's member
# (offsetof(Stage, values.preview)) that another file reads through the settings it is copied into
PREVIEW_CPP = r'''
#include "GameClient/Settings.h"
// a local variable of the same name (EvaEvents.cpp's parser has one) is not the member's declaration
void parsePreviewName(INI *ini, void *instance, void *, const void *)
{
	std::string preview = ini->getNextToken();
	static_cast<Stage *>(instance)->values.preview = preview;
}
struct Stage
{
	Settings values;
};
// a row macro storing through a member (EconomySettings.cpp's ES_OFF(member) = offsetof(State, values.member)): the member is Settings', not Stage's
#define PV_OFF(m) (int)offsetof(Stage, values.m)
const FieldParse kPreview[] = {
	{ "PreviewDecal", INI::parseAsciiString, nullptr, (int)offsetof(Stage, values.preview) },
	{ "PreviewTint", INI::parseInt, nullptr, (int)offsetof(Stage, values.tint) },
	{ "PreviewScale", INI::parseReal, nullptr, PV_OFF(scale) },
	{ nullptr, nullptr, nullptr, 0 }
};
void PreviewReader::registerBlocks(INIBlockRegistry &registry)
{
	registry.registerBlock("Object", [](INI *ini) { ini->initFromINI(&stage, kPreview); });
}
'''

# the reader sees Settings.h only through Middle.h
SETTINGS_H = r'''
struct Settings
{
	std::string preview;
	int tint = 0;
	float scale = 1.0f;
};
// a parameter of the member's name in the file declaring the member (SkirmishAIData.cpp's findDozer(const std::string &side)) is not its read
const int *Settings::findByTint(const int &tint) const
{
	return lookup(tint);
}
'''
MIDDLE_H = r'''
#include "GameClient/Settings.h"
'''
PREVIEW_USE_CPP = r'''#include "GameClient/Middle.h"
const std::string &previewTexture(const GameLogic &logic) { return logic.settings().preview; }
// a local variable of a member's name (LiveGame.cpp's `side`) is not a read of the member: outside a file declaring it, a read is .tint or ->tint
int tintOf() { int tint = 3; return tint; }
int GameLogic::tintOf(int tint) const { return tint; }
// inside a method a bare name is the member (AudioSettings.cpp's joinFolder(audioRoot, ..))
float Settings::scaled() const
{
	return scale * 2.0f;
}
'''

# DieMux's own method reads the member the MUX_ROWS row stores through m_dieMux (a file that never sees TypedThingModuleData)
DIEMUX_CPP = r'''
bool DieMux::applies() const { return m_deathTypes != 0; }
'''

# a file seeing TypedThing.h that never names TypedThingModuleData: its .m_used / .m_level are another struct's members of those names
OTHER_CPP = r'''#include "GameLogic/Module/TypedThing.h"
bool filterPasses(const Filter &f) { return f.m_used > 0 && f.m_level > 1; }
'''

RAW_CPP = r'''
// RawThing keeps its lines (RawModuleData); one value is read by name
int rawRadius(const RawModuleData &d) { return d.find("RawThing", "Radius"); }
'''

MOD_INI = '''
Object TestUnit
  BuildCost = 100
  VisionRange = 50
  PreviewDecal = Arrow
  PreviewTint = 2
  PreviewScale = 2
  VoiceSelect = TestUnitVoiceSelect
  UnitSpecificSounds
    VoiceGarrison = TestUnitVoiceGarrison
  End
  Behavior = TypedThing ModuleTag_01
    Used = 1.0
    Unused = 2.0
    FX = INITIAL FX_Puff
    DeathTypes = ALL
    Names = A B
    Level = 2
    MeleeBehavior = Amoeba
      FacingBonus = 30.0
    End
    Envelope InitialOpacity = 0.0 PeakOpacity = 1.0
    Bogus = 3
  End
  Behavior = RawThing Some_Tag
    Radius = 10
    Height = 4
  End
  Behavior = GoneThing ModuleTag_03
    Bar = 1
  End
  Behavior = EmotionThing ModuleTag_04
    AddEmotion = Terror_Base
    AddEmotion = OVERRIDE Doom_Base
      Duration = 100
    End
    AddEmotion = Alert_Base
  End
  Draw = ScriptThing ModuleTag_05
    BeginScript
      CurDrawableHideSubObject("A")
      Prev = CurDrawablePrevAnimationState()
    EndScript
    Model = Foo
  End
End

Object SecondUnit
  Behavior = TypedThing ModuleTag_01
    Used = 3.0
  End
End

Weapon TestBow
  AttackRange = 200
  Unread = 1
  Count2 = 1 2
End

Bridge TestBridge
  Width = 5
End
'''


def rw_tables(rw: Path):
    """the RotWK field tables of the made-up classes, the Object table and the Object's audio table"""
    def table(*names, fn="0x1"):
        return {"rows": [{"name": n, "fn": fn, "offset": 0, "userData": 0} for n in names], "terminated": True}
    tables = {
        "0xa1": {"rows": table("Used", "Unused", "FX", "DeathTypes", "Names", "Envelope", "Level")["rows"]
                 + [{"name": "MeleeBehavior", "fn": "0xb3", "offset": 0, "userData": 0}], "terminated": True},
        "0xa2": table("Radius", "Height"),
        "0xa3": table("Bar"),
        "0xa4": {"rows": [{"name": "AddEmotion", "fn": "0xb1", "offset": 0, "userData": 0}], "terminated": True},
        "0xa5": table("Duration"),
        "0xa6": {"rows": [{"name": "BeginScript", "fn": "0xb2", "offset": 0, "userData": 0}, {"name": "Model", "fn": "0x1", "offset": 0, "userData": 0}],
                 "terminated": True},
        "0xa7": {"rows": table("BuildCost", "VisionRange", "PreviewDecal", "PreviewTint", "PreviewScale", "Behavior", "Draw")["rows"]
                 + [{"name": "UnitSpecificSounds", "fn": "0xb4", "offset": 0, "userData": 0}], "terminated": True},
        "0xa9": table("FacingBonus"),
        "0xaa": {"rows": [], "catchAll": "0xb5", "terminated": True},
        "0xa8": table("VoiceSelect"),
    }
    functions = {"0x1": {"kind": "line"}, "0xb1": {"kind": "block", "tables": [{"extra": 0, "table": "0xa5"}]},
                 "0xb2": {"kind": "script", "terminator": "ENDSCRIPT"},
                 # MeleeBehavior's table is picked by its value (RW 0x86C30A); UnitSpecificSounds' table takes any name
                 "0xb3": {"kind": "block", "tables": [], "dispatch": {"names": ["Swarm", "Amoeba"], "tables": [[], [{"extra": 0, "table": "0xa9"}]]}},
                 "0xb4": {"kind": "block", "tables": [{"extra": 0, "table": "0xaa"}]}}
    (rw / "field-tables.json").write_text(json.dumps({"tables": tables, "functions": functions, "objectTable": {"table": "0xa7", "extra": 0},
                                                      "audioTable": {"table": "0xa8", "extra": 292}}),
                                          encoding="utf-8")
    classes = [{"name": n, "tables": [{"table": t, "extra": 0}]} for n, t in
               (("TypedThing", "0xa1"), ("RawThing", "0xa2"), ("GoneThing", "0xa3"), ("EmotionThing", "0xa4"), ("ScriptThing", "0xa6"))]
    (rw / "module-registry.json").write_text(json.dumps({"classes": classes}), encoding="utf-8")


COVERAGE = """class\ttype\tstatus\ttyped\tai\tdrawkind
TypedThing\tBehavior\tported\t1\t0\t-
RawThing\tBehavior\tported\t0\t0\t-
GoneThing\tBehavior\tmissing\t0\t0\t-
EmotionThing\tBehavior\tported\t1\t0\t-
ScriptThing\tDraw\tmissing\t0\t0\t5
"""


class MemoryVfs(eg.Vfs):
    def __init__(self, files: dict[str, str]):
        super().__init__()
        self.text = files
        self.files = {k: (Path(k), 0, 0, "mem") for k in files}

    def read(self, name):
        return self.text.get(name.lower().replace("\\", "/"))


@pytest.fixture(scope="module")
def result(tmp_path_factory):
    root = tmp_path_factory.mktemp("audit")
    src = root / "engine" / "src"
    for rel, text in {"Common/INI/INIBlockStubs.cpp": STUBS, "GameLogic/Module/TypedThing.h": TYPED_H, "GameLogic/Module/TypedThing.cpp": TYPED_CPP,
                      "GameLogic/Module/EmotionThing.cpp": EMOTION_CPP, "GameLogic/Object/Weapon.cpp": WEAPON_CPP,
                      "Common/Thing/ThingFactory.cpp": THING_CPP, "GameLogic/Module/RawRead.cpp": RAW_CPP,
                      "GameClient/PreviewReader.cpp": PREVIEW_CPP, "GameClient/PreviewUse.cpp": PREVIEW_USE_CPP, "GameLogic/Module/DieMux.cpp": DIEMUX_CPP,
                      "GameLogic/Module/Other.cpp": OTHER_CPP,
                      "GameClient/Settings.h": SETTINGS_H, "GameClient/Middle.h": MIDDLE_H}.items():
        (src / rel).parent.mkdir(parents=True, exist_ok=True)
        (src / rel).write_text(text, encoding="utf-8")
    rw = root / "rw"
    rw.mkdir()
    rw_tables(rw)
    (root / "coverage.tsv").write_text(COVERAGE, encoding="utf-8")
    source = fa.Source(src)
    every, real = source.block_keywords()
    diag: dict = {}
    mod = fa.mod_fields(MemoryVfs({"data/ini/object/test.ini": MOD_INI}), every, diag, fa.Grammar(rw))
    rw_cls, rw_obj = fa.rotwk_tables(rw)
    rows = fa.audit(mod, rw_cls, rw_obj, fa.load_coverage(root / "coverage.tsv"), source, real)
    return {(r["block"], r["owner"], r["field"]): r for r in rows}, diag, mod, source


def status(result, block, owner, field):
    return result[0][(block, owner, field)]["status"]


def test_typed_module_fields(result):
    assert status(result, "object", "TypedThing", "Used") == "used"
    assert result[0][("object", "TypedThing", "Used")]["where"].endswith("TypedThing.cpp:35")  # the read in speed(), not the table row
    assert result[0][("object", "TypedThing", "Used")]["defs"] == 2
    assert status(result, "object", "TypedThing", "Unused") == "parsed-unused"  # &INI::parseReal: the row is still found
    # (the member's name in a string literal or a /* */ comment of a visible file is not a read)
    assert status(result, "object", "TypedThing", "DeathTypes") == "used"  # a row of the multi-line MUX_ROWS(T) macro
    assert result[0][("object", "TypedThing", "DeathTypes")]["where"] == "GameLogic/Module/DieMux.cpp:2"  # m_dieMux.m_deathTypes: read anywhere
    assert status(result, "object", "TypedThing", "FX") == "used"  # its own parser stores m_fx, which speed() reads
    assert status(result, "object", "TypedThing", "Names") == "parsed-unused"  # `std::vector<std::string> m_names;` is a declaration, not a read
    # a read in a file naming the struct comes first (Other.cpp sorts before TypedThing.cpp); a read only in a file that never names it is marked
    assert status(result, "object", "TypedThing", "Level") == "used"
    assert result[0][("object", "TypedThing", "Level")]["where"] == "GameLogic/Module/Other.cpp:2 INFERENCE: the file does not name TypedThingModuleData"
    assert status(result, "object", "TypedThing", "Bogus") == "not-in-rotwk"  # MeleeBehavior's End did not close the module


def test_rotwk_grammar_blocks(result):
    # a block whose table is picked by the value (MeleeBehavior = Amoeba): its rows are the block's, its End is not the module's
    assert ("object", "TypedThing/MeleeBehavior", "FacingBonus") in result[0]
    # a sub-table taking any name (UnitSpecificSounds): its lines are the block's, not the Object's
    assert ("object", "/UnitSpecificSounds", "VoiceGarrison") in result[0]
    assert ("object", "", "VoiceGarrison") not in result[0]
    # the field name is the first token (INI's strtok on blank and '='): `Envelope InitialOpacity = 0.0 ..` is Envelope's line
    assert ("object", "TypedThing", "Envelope") in result[0]
    assert ("object", "TypedThing", "Envelope InitialOpacity") not in result[0]


def test_raw_missing_and_unknown_modules(result):
    assert status(result, "object", "RawThing", "Radius") == "used-raw"  # named by a raw reader in a file naming the class
    assert status(result, "object", "RawThing", "Height") == "raw"
    assert status(result, "object", "GoneThing", "Bar") == "missing-module"
    assert result[1]["modules"]["RawThing"] == {"TestUnit"}  # the tag is any name, not only ModuleTag_*


def test_block_rows_and_scripts(result):
    # AddEmotion = Terror_Base is one line; OVERRIDE Doom_Base opens the nugget up to its End; Alert_Base before the module's End is one line again
    assert ("object", "EmotionThing/AddEmotion", "Duration") in result[0]
    assert status(result, "object", "EmotionThing", "AddEmotion") == "used"
    assert result[0][("object", "EmotionThing", "AddEmotion")]["uses"] == 3
    # BeginScript .. EndScript is one field; the lines inside are neither fields nor blocks (a `Prev = ...` line inside is not a field)
    assert ("object", "ScriptThing", "BeginScript") in result[0]
    assert ("object", "ScriptThing", "Prev") not in result[0]
    assert status(result, "object", "ScriptThing", "Model") == "missing-module"
    assert not result[1]["unclosed"]


def test_definition_fields(result):
    assert status(result, "object", "", "BuildCost") == "used"  # read by name
    assert status(result, "object", "", "VisionRange") == "parsed-unused"
    # the Object's second reader parses it into a staging struct; the read is in a file that never sees Stage
    assert status(result, "object", "", "PreviewDecal") == "used"
    assert result[0][("object", "", "PreviewDecal")]["where"] == "GameClient/PreviewUse.cpp:2"
    assert status(result, "object", "", "PreviewTint") == "parsed-unused"  # only a local variable or a parameter of that name is read
    assert status(result, "object", "", "PreviewScale") == "used"  # a method's bare name
    # the Object is read by two RotWK tables (RW 0x73C13C): a field of the audio table is RotWK's, not the mod's error
    assert status(result, "object", "", "VoiceSelect") == "used"
    assert status(result, "weapon", "", "AttackRange") == "used"
    assert status(result, "weapon", "", "Unread") == "parsed-unused"
    assert status(result, "weapon", "", "Count2") == "used"
    assert result[0][("weapon", "", "Count2")]["member"] == "counts"
    assert status(result, "bridge", "", "Width") == "stub-block"
    assert ("object", "SecondUnit", "Used") not in result[0]


def test_binding_and_visibility(result):
    source = result[3]
    assert source.binds["TypedThing"][0] == "TypedThingModuleData"  # bindTypedData<..>(kName, ..) with the name in a variable
    assert source.binds["EmotionThing"][0] == "EmotionThingModuleData"  # bind<Runtime, Data>(modules, "Class")
    assert "GameLogic/Module/TypedThing.cpp" in source.visible("TypedThingModuleData")
    assert "GameLogic/Object/Weapon.cpp" not in source.visible("TypedThingModuleData")


def test_previous_run_with_quotes(tmp_path):
    """a mod value with a quote is text: reading the previous field-audit.tsv back gives every row (csv's default quoting glued rows together)"""
    rows = [{c: "" for c in fa.COLS} | {"status": "used", "block": "object", "owner": "", "field": f"F{i}", "values": v}
            for i, v in enumerate(['"Open quote', "plain", 'Closed" quote', "plain"])]
    fa.write(rows, tmp_path, [], {})
    counts, changed = fa.write(rows, tmp_path, [], {})
    assert not changed  # every row of the previous run read back as it was written
    cov = tmp_path / "coverage.tsv"
    cov.write_text(COVERAGE.replace("RawThing\tBehavior", '"RawThing\tBehavior'), encoding="utf-8")
    assert len(fa.load_coverage(cov)) == 5
