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
	MUX_ROWS(TypedThingModuleData),
	{ nullptr, nullptr, nullptr, 0 }
};
} // namespace

void TypedThingModuleData::buildFieldParse(MultiIniFieldParse &p) { p.add(kTyped); }

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
	static const FieldParse *getFieldParse();
};
const FieldParse *WeaponTemplate::getFieldParse()
{
	static const FieldParse table[] = {
		{ "AttackRange", INI::parseReal, nullptr, (int)offsetof(WeaponTemplate, m_attackRange) },
		{ "Unread", INI::parseReal, nullptr, (int)offsetof(WeaponTemplate, m_unread) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	return table;
}
float WeaponTemplate::range() const { return m_attackRange; }
void WeaponStore::registerBlocks(INIBlockRegistry &registry)
{
	registry.registerBlock("Weapon", [](INI *ini) { WeaponStore::parseWeaponTemplateDefinitionGlobal(ini); });
}
'''

# the Object's own fields are read by name (ThingTemplate::findField)
THING_CPP = r'''
#include "Common/Thing/ThingTemplate.h"
float buildCost(const ThingTemplate &tt) { return number(tt.findField("BuildCost")); }
void ThingFactory::registerBlocks(INIBlockRegistry &registry)
{
	registry.registerBlock("Object", [this](INI *ini) { parse(ini); });
}
'''

# a second reader of the Object block with its own table (FormationAssistant's FormationPreviewDecal) and a row storing into a staging struct's member
# (offsetof(Stage, values.preview)) that another file reads through the settings it is copied into
PREVIEW_CPP = r'''
#include "GameClient/Settings.h"
struct LocalCopy
{
	std::string preview;
};
struct Stage
{
	Settings values;
};
const FieldParse kPreview[] = {
	{ "PreviewDecal", INI::parseAsciiString, nullptr, (int)offsetof(Stage, values.preview) },
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
};
'''
MIDDLE_H = r'''
#include "GameClient/Settings.h"
'''
PREVIEW_USE_CPP = r'''#include "GameClient/Middle.h"
const std::string &previewTexture(const GameLogic &logic) { return logic.settings().preview; }
'''

# DieMux's own method reads the member the MUX_ROWS row stores through m_dieMux (a file that never sees TypedThingModuleData)
DIEMUX_CPP = r'''
bool DieMux::applies() const { return m_deathTypes != 0; }
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
  Behavior = TypedThing ModuleTag_01
    Used = 1.0
    Unused = 2.0
    FX = INITIAL FX_Puff
    DeathTypes = ALL
    Names = A B
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
End

Bridge TestBridge
  Width = 5
End
'''


def rw_tables(rw: Path):
    """the RotWK field tables of the made-up classes and the Object table"""
    def table(*names, fn="0x1"):
        return {"rows": [{"name": n, "fn": fn, "offset": 0, "userData": 0} for n in names], "terminated": True}
    tables = {
        "0xa1": table("Used", "Unused", "FX", "DeathTypes", "Names"),
        "0xa2": table("Radius", "Height"),
        "0xa3": table("Bar"),
        "0xa4": {"rows": [{"name": "AddEmotion", "fn": "0xb1", "offset": 0, "userData": 0}], "terminated": True},
        "0xa5": table("Duration"),
        "0xa6": {"rows": [{"name": "BeginScript", "fn": "0xb2", "offset": 0, "userData": 0}, {"name": "Model", "fn": "0x1", "offset": 0, "userData": 0}],
                 "terminated": True},
        "0xa7": table("BuildCost", "VisionRange", "PreviewDecal"),
    }
    functions = {"0x1": {"kind": "line"}, "0xb1": {"kind": "block", "tables": [{"extra": 0, "table": "0xa5"}]},
                 "0xb2": {"kind": "script", "terminator": "ENDSCRIPT"}}
    (rw / "field-tables.json").write_text(json.dumps({"tables": tables, "functions": functions, "objectTable": {"table": "0xa7", "extra": 0}}),
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
    assert result[0][("object", "TypedThing", "Used")]["where"].endswith("TypedThing.cpp:30")  # the read in speed(), not the table row
    assert result[0][("object", "TypedThing", "Used")]["defs"] == 2
    assert status(result, "object", "TypedThing", "Unused") == "parsed-unused"  # &INI::parseReal: the row is still found
    assert status(result, "object", "TypedThing", "DeathTypes") == "used"  # a row of the multi-line MUX_ROWS(T) macro
    assert result[0][("object", "TypedThing", "DeathTypes")]["where"] == "GameLogic/Module/DieMux.cpp:2"  # m_dieMux.m_deathTypes: read anywhere
    assert status(result, "object", "TypedThing", "FX") == "used"  # its own parser stores m_fx, which speed() reads
    assert status(result, "object", "TypedThing", "Names") == "parsed-unused"  # `std::vector<std::string> m_names;` is a declaration, not a read
    assert status(result, "object", "TypedThing", "Bogus") == "not-in-rotwk"


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
    assert status(result, "weapon", "", "AttackRange") == "used"
    assert status(result, "weapon", "", "Unread") == "parsed-unused"
    assert status(result, "bridge", "", "Width") == "stub-block"
    assert ("object", "SecondUnit", "Used") not in result[0]


def test_binding_and_visibility(result):
    source = result[3]
    assert source.binds["TypedThing"][0] == "TypedThingModuleData"  # bindTypedData<..>(kName, ..) with the name in a variable
    assert source.binds["EmotionThing"][0] == "EmotionThingModuleData"  # bind<Runtime, Data>(modules, "Class")
    assert "GameLogic/Module/TypedThing.cpp" in source.visible("TypedThingModuleData")
    assert "GameLogic/Object/Weapon.cpp" not in source.visible("TypedThingModuleData")
