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
		"AIBase", "ArmyDefinition", "Bridge", "Object", "Weapon",
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
	int m_shared = 0;
	int m_spare = 0, m_rare = 0; // a declarator list (CastleModules.h's m_maxCastleRadius)
	std::string m_tagA, m_tagB; // its later name is no read (CreateAHeroSystem.h)
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
	{ "Rare", INI::parseInt, nullptr, TT_OFF(m_rare) },
	{ "Shared", INI::parseInt, nullptr, TT_OFF(m_shared) },
	{ "TagB", INI::parseAsciiString, nullptr, TT_OFF(m_tagB) },
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
// a name list read by index (UnitVoiceResponse.cpp's kRowNames): naming VoiceFear there is no request for it
const char *const kRowNames[] = {
	"VoiceSelect", "VoiceFear",
};
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

# a file seeing TypedThing.h that never names TypedThingModuleData: its .m_used / .m_level are another struct's members of those names; m_rare is
# declared by one class only, so its read through a getter's result is TypedThingModuleData's (AimWeaponBehavior.cpp's m_isAimingWeapon)
OTHER_CPP = r'''#include "GameLogic/Module/TypedThing.h"
bool filterPasses(const Filter &f) { return f.m_used > 0 && f.m_level > 1; }
int rareOf(const TypedThing &t) { return t.data()->m_rare; }
struct Filter
{
	float m_used = 0.0f;
	int m_level = 0;
	int m_shared = 0;
};
'''

# a file naming TypedThingModuleData and Filter, both with a member m_shared: its read may be either's (the known limit), marked AMBIGUOUS
AMBIG_CPP = r'''#include "GameLogic/Module/TypedThing.h"
int sharedOf(const Filter &filter, const TypedThingModuleData *d) { return filter.m_shared; }
'''

# a block's parser that parses a local and consumes it (SkirmishAIData.cpp's parseArmyDefinition, Locomotor.cpp's parseLocomotorSet): army.side is a
# key; army.unit is stored into m_units, which unitOf() reads; army.banner into def.bannerRgb, which nothing reads (GameLogicSettings.cpp's
# MultiplayerColor LivingWorldBannerColor); army.speed is passed to a call (parseLocomotorSet's set.set(.., entry.speed)); army.tag is only handed to
# a reader (&army.tag), army.left is only tested for being given (PlayerTemplate.cpp's stop-line flag); base.side is AIBase's, not army.side;
# army.slot is checked, then a key (SkirmishAIData.cpp's parseCombatChainDefinition: chain.unit < 0, then combatChains[chain.unit]). The nested
# Member block's Unit is MemberDef's (its row's parser parseMember), not ArmyDefinition's Unit of the same file
ARMY_CPP = r'''
struct ArmyDefinition
{
	std::string side;
	std::string unit;
	std::string tag;
	std::string left;
	int banner = 0;
	float speed = 0.0f;
	int slot = 0;
	std::vector<MemberDef> members;
};
struct MemberDef
{
	std::string unit;
};
const FieldParse kMemberFields[] = {
	{ "Unit", INI::parseAsciiString, nullptr, (int)offsetof(MemberDef, unit) },
	{ nullptr, nullptr, nullptr, 0 }
};
void parseMember(INI *ini, void *, void *store, const void *)
{
	MemberDef member;
	ini->initFromINI(&member, kMemberFields);
	((ArmyDefinition *)store)->members.push_back(member);
}
struct ColorDef
{
	int bannerRgb = 0;
};
class ArmyStore
{
	std::map<int, std::string> m_units;
};
const FieldParse kArmyFields[] = {
	{ "Side", INI::parseAsciiString, nullptr, (int)offsetof(ArmyDefinition, side) },
	{ "Unit", INI::parseAsciiString, nullptr, (int)offsetof(ArmyDefinition, unit) },
	{ "Tag", INI::parseAsciiString, nullptr, (int)offsetof(ArmyDefinition, tag) },
	{ "Left", INI::parseAsciiString, nullptr, (int)offsetof(ArmyDefinition, left) },
	{ "Banner", INI::parseInt, nullptr, (int)offsetof(ArmyDefinition, banner) },
	{ "Speed", INI::parseReal, nullptr, (int)offsetof(ArmyDefinition, speed) },
	{ "Slot", INI::parseInt, nullptr, (int)offsetof(ArmyDefinition, slot) },
	{ "Member", parseMember, nullptr, 0 },
	{ nullptr, nullptr, nullptr, 0 }
};
void ArmyStore::parseArmyDefinition(INI *ini)
{
	ArmyDefinition army;
	ini->initFromINI(&army, kArmyFields);
	readTag(ini, &army.tag);
	m_units[m_keys.nameToKey(army.side)] = army.unit;
	m_given = !army.left.empty();
	ColorDef def;
	def.bannerRgb = pack(army.banner);
	m_set.set(army.speed);
	if (army.slot < 0)
		return;
	m_slots[army.slot] = 1;
}
const std::string &ArmyStore::unitOf(int key) const { return m_units.at(key); }
struct AIBase
{
	std::string side;
};
const FieldParse kAIBaseFields[] = {
	{ "Side", INI::parseAsciiString, nullptr, (int)offsetof(AIBase, side) },
	{ nullptr, nullptr, nullptr, 0 }
};
void ArmyStore::parseAIBase(INI *ini)
{
	AIBase base;
	ini->initFromINI(&base, kAIBaseFields);
}
void ArmyStore::registerBlocks(INIBlockRegistry &registry)
{
	registry.registerBlock("ArmyDefinition", [this](INI *ini) { parseArmyDefinition(ini); });
	registry.registerBlock("AIBase", [this](INI *ini) { parseAIBase(ini); });
}
'''

# the UnitSpecificSounds lines are kept raw and asked for by name (UnitVoiceResponse.cpp): a literal, or a literal and an Object's name
VOICE_CPP = r'''
#include "GameLogic/UnitSpecificSound.h"
std::string garrisonVoice(const ThingTemplate &tt, const ThingTemplate &target)
{
	if (UnitSpecificSound::rawValue(tt, "VoiceGarrison").empty())
		return UnitSpecificSound::rawValue(tt, "VoiceEnterUnit" + target.getName());
	return "";
}
'''

# a second Object reader whose table is the lambda's own and whose nested parser has a table of its own (FormationAssistant.cpp's
# FormationPreviewDecal -> parseDecal -> FIELD(F::Decal, "Texture", ..)); Decal is also a struct of another file (Common/ADecal.h), not seen here.
# readAward holds its table, so its reads are the parser's, and its check only refuses the block (AwardSystemManager.cpp's objectAward)
DECAL_H = r'''
struct Decal
{
	std::string texture;
	int tint = 0;
	int shade = 0;
};
'''
OTHER_DECAL_H = r'''
struct Decal
{
	int texture = 0;
};
'''
DECAL_CPP = r'''#include "GameClient/Decal.h"
#define FIELD(T, N, P, M) {N, P, nullptr, (int)offsetof(T, M)}
void parseDecal(INI *ini, void *instance, void *, const void *)
{
	const FieldParse fields[] = {FIELD(Decal, "Texture", INI::parseAsciiString, texture), FIELD(Decal, "Tint", INI::parseInt, tint),
	                             {nullptr, nullptr, nullptr, 0}};
	ini->initFromINI(instance, fields);
}
void readAward(INI *ini, void *, void *store, const void *)
{
	const FieldParse fields[] = {FIELD(Decal, "Shade", INI::parseInt, shade), {nullptr, nullptr, nullptr, 0}};
	Decal d;
	ini->initFromINI(&d, fields);
	if (d.shade < 0) throw INIException(3, "bad shade");
	static_cast<Store *>(store)->decals.push_back(d);
}
void DecalReader::registerBlocks(INIBlockRegistry &registry)
{
	registry.registerBlock("Object",
		[](INI *ini)
		{
			const FieldParse fields[] = {{"LookDecal", parseDecal, nullptr, 0}, {"Award", readAward, nullptr, 0}, {nullptr, nullptr, nullptr, 0}};
			ini->initFromINI(&g_decal, fields);
		});
}
const std::string &decalTexture() { return g_decal.texture; }
'''

# an Object block kept as raw lines and read by name by a file walking the raw blocks (AIThreatFinder.cpp's ThreatBreakdown / AIKindOf)
THREAT_CPP = r'''
int threatOf(const ThingTemplate &tt)
{
	for (const RawBlock &b : tt.rawBlocks())
		if (b.field == "Threat")
			for (const RawLine &l : b.lines)
				if (l.text.find("Kind") == 0)
					return 1;
	return 0;
}
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
  VoiceFear = TestUnitVoiceFear
  UnitSpecificSounds
    VoiceGarrison = TestUnitVoiceGarrison
    VoiceEnterUnitSecondUnit = A
    VoiceEnterUnitNoSuchUnit = B
    VoiceGetHealed = C
    TestUnitVoiceSelect = D
  End
  LookDecal
    Texture = Arrow
    Tint = 3
  End
  Award
    Shade = 2
  End
  Threat
    Kind = INFANTRY
    Other = 1
  End
  Behavior = TypedThing ModuleTag_01
    Used = 1.0
    Unused = 2.0
    FX = INITIAL FX_Puff
    DeathTypes = ALL
    Names = A B
    Level = 2
    Rare = 1
    Shared = 1
    TagB = X
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

ArmyDefinition TestArmy
  Side = Men
  Unit = Soldier
  Tag = A
  Left = B
  Banner = 1
  Speed = 2
  Slot = 1
  Member
    Unit = Archer
  End
End

AIBase TestBase
  Side = Men
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
        "0xa1": {"rows": table("Used", "Unused", "FX", "DeathTypes", "Names", "Envelope", "Level", "Rare", "TagB", "Shared")["rows"]
                 + [{"name": "MeleeBehavior", "fn": "0xb3", "offset": 0, "userData": 0}], "terminated": True},
        "0xa2": table("Radius", "Height"),
        "0xa3": table("Bar"),
        "0xa4": {"rows": [{"name": "AddEmotion", "fn": "0xb1", "offset": 0, "userData": 0}], "terminated": True},
        "0xa5": table("Duration"),
        "0xa6": {"rows": [{"name": "BeginScript", "fn": "0xb2", "offset": 0, "userData": 0}, {"name": "Model", "fn": "0x1", "offset": 0, "userData": 0}],
                 "terminated": True},
        "0xa7": {"rows": table("BuildCost", "VisionRange", "PreviewDecal", "PreviewTint", "PreviewScale", "Behavior", "Draw")["rows"]
                 + [{"name": n, "fn": "0xb4", "offset": 0, "userData": 0} for n in ("UnitSpecificSounds", "LookDecal", "Award", "Threat")],
                 "terminated": True},
        "0xa9": table("FacingBonus"),
        "0xaa": {"rows": [], "catchAll": "0xb5", "terminated": True},
        "0xa8": table("VoiceSelect", "VoiceFear"),
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
                      "GameLogic/Module/Other.cpp": OTHER_CPP, "GameLogic/Module/Ambig.cpp": AMBIG_CPP, "GameLogic/AI/Army.cpp": ARMY_CPP, "GameClient/Voice.cpp": VOICE_CPP, "GameClient/Decal.h": DECAL_H, "Common/ADecal.h": OTHER_DECAL_H,
                      "GameClient/Decal.cpp": DECAL_CPP, "GameLogic/AI/Threat.cpp": THREAT_CPP,
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
    assert result[0][("object", "TypedThing", "Used")]["where"].endswith("TypedThing.cpp:38")  # the read in speed(), not the table row
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
    assert result[0][("object", "TypedThing", "Rare")]["where"] == "GameLogic/Module/Other.cpp:3"  # the only m_rare of the source
    assert status(result, "object", "TypedThing", "TagB") == "parsed-unused"
    assert result[0][("object", "TypedThing", "Shared")]["where"] == ("GameLogic/Module/Ambig.cpp:2 AMBIGUOUS: the file also names Filter with a "
                                                                      "member m_shared")
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
    assert status(result, "object", "", "VoiceFear") == "dynamic-row"  # only a name list read by index names it: a hand check
    assert status(result, "weapon", "", "AttackRange") == "used"
    assert status(result, "weapon", "", "Unread") == "parsed-unused"
    assert status(result, "weapon", "", "Count2") == "used"
    assert result[0][("weapon", "", "Count2")]["member"] == "counts"
    assert status(result, "bridge", "", "Width") == "stub-block"
    assert ("object", "SecondUnit", "Used") not in result[0]


def army_line(needle: str) -> str:
    """Army.cpp:<the line of ARMY_CPP holding needle>"""
    return "GameLogic/AI/Army.cpp:" + str(next(i for i, x in enumerate(ARMY_CPP.splitlines(), 1) if needle in x))


def test_staged_block_parser(result):
    """the block's parser reads the local it parsed (the parse functions are no reads otherwise): a key is a use, a value stored on is marked"""
    side = result[0][("armydefinition", "", "Side")]
    assert side["status"] == "used"
    assert side["where"].startswith(army_line("nameToKey(army.side)") + " STAGED") and "as a key" in side["where"]
    unit = result[0][("armydefinition", "", "Unit")]
    assert unit["status"] == "used" and unit["where"].endswith("stores it in m_units, read at " + army_line("m_units.at(key)"))
    assert status(result, "armydefinition", "", "Banner") == "parsed-unused"  # def.bannerRgb is read by no one
    speed = result[0][("armydefinition", "", "Speed")]
    assert speed["status"] == "used" and "passes it on; INFERENCE" in speed["where"]
    assert result[0][("armydefinition", "", "Slot")]["where"].startswith(army_line("m_slots[army.slot]") + " STAGED")  # the key, not the check before it
    assert status(result, "armydefinition", "/Member", "Unit") == "parsed-unused"  # members is read by no one
    assert status(result, "armydefinition", "", "Tag") == "parsed-unused"  # &army.tag hands it to a writer
    assert status(result, "armydefinition", "", "Left") == "parsed-unused"
    assert status(result, "aibase", "", "Side") == "parsed-unused"  # army.side is another struct's


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


def test_manual_verdicts(result, tmp_path):
    """a hand check sets the status and keeps the tool's own; a verdict the tool now disagrees with in a new way, a field that is gone and an
    aggregate row are listed"""
    rows = [dict(r) for r in result[0].values()]
    manual = tmp_path / "manual-check.tsv"
    manual.write_text("block\towner\tfield\tstatus-tool\tverdict\twhere\tnote\n"
                      "object\tTypedThing\tFX\tcustom-parser\tused\tGameLogic/Module/TypedThing.cpp:79\tparseFX stores m_fx\n"
                      "object\tTypedThing\tUnused\tcustom-parser\tused\tx.cpp:1\n"  # the tool now says parsed-unused: neither
                      "object\tTypedThing\tNames\tparsed-unused\tused\tx.cpp:2\n"  # the tool now asks for a hand check: no conflict
                      "object\t\tGone\tparsed-unused\tparsed-unused\tnone\n"
                      "aidata\t\t(75 fields)\tstub-block\tused\tGameLogicSettings.cpp:401\n", encoding="utf-8")
    by = {(r["block"], r["owner"], r["field"]): r for r in rows}
    by[("object", "TypedThing", "Names")]["status"] = "custom-parser"  # (FormationPreviewItemDecal: parsed-unused when checked, custom-parser now)
    got = fa.apply_manual(rows, manual)
    fx = by[("object", "TypedThing", "FX")]
    assert (fx["status"], fx["tool"], fx["how"]) == ("used", "used", "manual")
    assert fx["where"] == "GameLogic/Module/TypedThing.cpp:79 MANUAL: parseFX stores m_fx"
    assert got["applied"] == 3
    assert [(c["field"], now) for c, now, _ in got["conflicts"]] == [("Unused", "parsed-unused")]
    assert [c["field"] for c in got["unmatched"]] == ["Gone"] and [c["field"] for c in got["skipped"]] == ["(75 fields)"]
    fa.write(rows, tmp_path, [], {}, got)
    text = (tmp_path / "field-audit.md").read_text(encoding="utf-8")
    assert "| object | TypedThing | Unused | custom-parser | used | parsed-unused |" in text


def test_by_name_block(result):
    """a UnitSpecificSounds line is read when a reader of the block asks for its name: a literal, a literal and an existing Object's name, or
    another field holding the name that is read itself"""
    assert status(result, "object", "/UnitSpecificSounds", "VoiceGarrison") == "used"
    assert result[0][("object", "/UnitSpecificSounds", "VoiceEnterUnitSecondUnit")]["how"] == "by-name-prefix"
    assert status(result, "object", "/UnitSpecificSounds", "VoiceEnterUnitSecondUnit") == "used"
    assert status(result, "object", "/UnitSpecificSounds", "VoiceEnterUnitNoSuchUnit") == "parsed-unused"  # no Object NoSuchUnit
    assert status(result, "object", "/UnitSpecificSounds", "VoiceGetHealed") == "parsed-unused"  # no reader asks for it
    # VoiceSelect = TestUnitVoiceSelect names it, and VoiceSelect is read
    assert status(result, "object", "/UnitSpecificSounds", "TestUnitVoiceSelect") == "used"
    assert result[0][("object", "/UnitSpecificSounds", "TestUnitVoiceSelect")]["where"].startswith("named by object/VoiceSelect (used")


def test_object_nested_readers(result):
    """an Object nested block parsed by a second reader (through the lambda's table and the nested parser's own), or kept raw and read by name"""
    texture = result[0][("object", "/LookDecal", "Texture")]
    assert texture["status"] == "used" and texture["where"].startswith("GameClient/Decal.cpp:")  # the Decal Decal.cpp sees, not ADecal.h's
    assert status(result, "object", "/LookDecal", "Tint") == "parsed-unused"
    assert status(result, "object", "/Award", "Shade") == "parsed-unused"  # the parser's own check that throws is no use
    assert status(result, "object", "/Threat", "Kind") == "used"
    assert status(result, "object", "/Threat", "Other") == "parsed-unused"
