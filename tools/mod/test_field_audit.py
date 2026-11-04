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
		"AIBase", "ArmyDefinition", "Bridge", "FxThing", "Logic", "MiscAudio", "Object", "Scheme", "Weapon",
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
	int m_copied = 0;
	float m_factor = 1.0f;
	// a product is no declaration (AudioEventRTS.h's `return m_perFilePitch * m_pitchShift;`)
	float factored() const { return m_used * m_factor; }
	int m_shown = 0;
	int m_spare = 0, m_rare = 0; // a declarator list (CastleModules.h's m_maxCastleRadius)
	std::string m_tagA, m_tagB; // its later name is no read (CreateAHeroSystem.h)
	std::vector<std::string> m_names;
	std::vector<int> bones;
	PartInfo part;
	int m_slot = 0;
	int m_left = 0;
	int m_right = 0;
	int m_spanMin = 0;
	int m_spanMax = 0;
	int lanes[4] = {};
	static constexpr int MAX_STEPS = 3;
	int m_steps[2][MAX_STEPS] = {};
	int m_flags[2] = {};
	int m_plain = 0;
	float m_regen = 0.0f;
	float m_gain[2] = {};
	float m_glow = 0.0f;
};
struct PartInfo
{
	float width = 0.0f, depth = 0.0f, angle = 0.0f;
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

// a parser handing a member of the instance to a helper (W3DModelDraw.cpp's parseParticleSysBoneInto(ini, ..(instance)->particleSysBones))
void parseBoneInto(INI *ini, std::vector<int> &out) { out.push_back(1); }
void parseBone(INI *ini, void *instance, void *, const void *)
{
	parseBoneInto(ini, static_cast<TypedThingModuleData *>(instance)->bones);
}

// parsers a macro defines (W3DModelDraw.cpp's SHADOW_FIELD_PARSER), each handing the address of a member to a parser; one handing the
// address of a member of a call's result (parseTurretArtAngle's &lastTurret(..).artAngle)
#define PART_PARSER(Name, Member)                                          \
	void Name(INI *ini, void *instance, void *, const void *)              \
	{                                                                      \
		TypedThingModuleData *self = static_cast<TypedThingModuleData *>(instance); \
		INI::parseReal(ini, instance, &self->part.Member, nullptr);        \
	}
PART_PARSER(parsePartWidth, width)
PART_PARSER(parsePartDepth, depth)
#undef PART_PARSER
void parseAngle(INI *ini, void *instance, void *, const void *)
{
	INI::parseReal(ini, instance, &partOf(static_cast<TypedThingModuleData *>(instance), "Angle").angle, nullptr);
}
// a template parser (SpecialAbilityModules.cpp's parseAnimAndDuration<0>) and one shared by two rows, its userData picking the member
// (CameraSettings.cpp's parseShake): which member is the row's is not told
template <int Which>
void parseSlot(INI *ini, void *instance, void *, const void *)
{
	static_cast<TypedThingModuleData *>(instance)->m_slot = Which;
}
void parseSide(INI *ini, void *instance, void *, const void *userData)
{
	TypedThingModuleData *d = static_cast<TypedThingModuleData *>(instance);
	(userData ? d->m_left : d->m_right) = 1;
}
// one row's parser storing two members (WeaponTemplate's DelayBetweenShots min and max): no other row's
void parseSpan(INI *ini, void *instance, void *, const void *)
{
	TypedThingModuleData *d = static_cast<TypedThingModuleData *>(instance);
	d->m_spanMin = 1;
	d->m_spanMax = 2;
}
// a store through an index (LivingWorldAutoResolveData.cpp's parseArmorRow: a->scalar[scanUnitType(..)] = value)
void parseLane(INI *ini, void *instance, void *, const void *)
{
	TypedThingModuleData *d = static_cast<TypedThingModuleData *>(instance);
	d->lanes[laneIndex(ini->getNextToken())] = 1;
}
// a nested block of any names, kept by name (SoundSelectorClientBehavior.cpp's kUnitSpecificTable: its one row the catch-all)
void parseUnitSpecificLine(INI *ini, void *, void *store, const void *userData) { (*static_cast<NameMap *>(store))[(const char *)userData] = 1; }
const FieldParse kUnitSpecific[] = {
	{ nullptr, parseUnitSpecificLine, nullptr, 0 }
};
void parseUnitSpecific(INI *ini, void *instance, void *, const void *)
{
	ini->initFromINI(&static_cast<TypedThingModuleData *>(instance)->m_unitSpecific, kUnitSpecific);
}

const FieldParse kTyped[] = {
	{ "UnitSpecificSounds", parseUnitSpecific, nullptr, 0 },
	{ "Curve", Curve::parseCurve, nullptr, TT_OFF(m_curve) },
	{ "Used", INI::parseReal, nullptr, TT_OFF(m_used) },
	{ "Unused", &INI::parseReal, nullptr, TT_OFF(m_unused) },
	{ "FX", parseFX, nullptr, 0 },
	{ "Bone", parseBone, nullptr, 0 },
	{ "PartWidth", parsePartWidth, nullptr, 0 },
	{ "PartDepth", parsePartDepth, nullptr, 0 },
	{ "Angle", parseAngle, nullptr, 0 },
	{ "Slot", parseSlot<0>, nullptr, 0 },
	{ "Left", parseSide, nullptr, 0 },
	{ "Right", parseSide, (const void *)1, 0 },
	{ "Span", parseSpan, nullptr, 0 },
	{ "Lane", parseLane, nullptr, 0 },
	{ "Names", INI::parseAsciiStringVector, nullptr, TT_OFF(m_names) },
	{ "Level", INI::parseInt, nullptr, TT_OFF(m_level) },
	{ "Rare", INI::parseInt, nullptr, TT_OFF(m_rare) },
	{ "Shared", INI::parseInt, nullptr, TT_OFF(m_shared) },
	{ "Copied", INI::parseInt, nullptr, TT_OFF(m_copied) },
	{ "Factor", INI::parseReal, nullptr, TT_OFF(m_factor) },
	{ "Shown", INI::parseInt, nullptr, TT_OFF(m_shown) },
	{ "TagB", INI::parseAsciiString, nullptr, TT_OFF(m_tagB) },
	{ "Regen%PerSecond", INI::parsePercentToReal, nullptr, TT_OFF(m_regen) }, // a '%' in the name (RebuildHoleModules.cpp)
	MUX_ROWS(TypedThingModuleData),
	{ nullptr, nullptr, nullptr, 0 }
};

// a macro of rows without parameters (FireWeaponWhenDamagedBehavior.cpp's FWWD_ROWS), expanded by its name
#define PLAIN_ROWS                                                         \
	{ "Plain", INI::parseInt, nullptr, TT_OFF(m_plain) }
const FieldParse kPlain[] = { PLAIN_ROWS, { nullptr, nullptr, nullptr, 0 } };
#undef PLAIN_ROWS

// rows a builder makes at run time (TransitionDamageFX.cpp's buildTable): names from name arrays and a loop's number, and an array's elements
void parseStep(INI *ini, void *instance, void *, const void *)
{
	static_cast<TypedThingModuleData *>(instance)->m_steps[0][0] = 1;
}
void parseFlag(INI *ini, void *instance, void *, const void *)
{
	static_cast<TypedThingModuleData *>(instance)->m_flags[0] = 1;
}
std::vector<FieldParse> buildTable()
{
	static std::vector<std::string> names;
	static const char *const kPhases[] = { "Early", "Late" };
	static const char *const kSides[] = { "LeftFlag", "RightFlag" };
	std::vector<FieldParse> rows;
	for (int p = 0; p < 2; ++p)
	{
		for (int i = 0; i < TypedThingModuleData::MAX_STEPS; ++i)
		{
			names.push_back(std::string(kPhases[p]) + "Step" + std::to_string(i + 1));
			rows.push_back({ names.back().c_str(), parseStep, nullptr, 0 });
		}
	}
	for (int k = 0; k < 2; ++k)
		rows.push_back({ kSides[k], parseFlag, nullptr, 0 });
	// through a helper, and a local prefix on the names of an array of rows (AudioSettings.cpp's reverb multipliers and LivingWorld rows)
	auto add = [&](const char *name, INIFieldParseProc proc, int offset) { rows.push_back({ name, proc, nullptr, offset }); };
	static std::deque<std::string> gains;
	for (int i = 0; i < 2; ++i)
	{
		gains.push_back(std::string("Global") + kPhases[i] + "Gain");
		add(gains.back().c_str(), INI::parseReal, TT_OFF(m_gain) + i * (int)sizeof(float));
	}
	struct Row
	{
		const char *name;
		INIFieldParseProc proc;
		int member;
	};
	for (int g = 0; g < 2; ++g)
	{
		const std::string prefix = g == 0 ? "" : "Night";
		const Row group[] = {
			{ "Glow", INI::parseReal, TT_OFF(m_glow) },
		};
		for (const Row &r : group)
		{
			gains.push_back(prefix + r.name);
			add(gains.back().c_str(), r.proc, r.member);
		}
	}
	return rows;
}
} // namespace

void TypedThingModuleData::buildFieldParse(MultiIniFieldParse &p)
{
	p.add(kTyped);
	p.add(kPlain);
	static const std::vector<FieldParse> table = buildTable();
	p.add(table.data());
}
int TypedThing::stepOf() const { return m_data->m_steps[1][2]; }
float TypedThing::plainRegen() const { return m_data->m_plain * m_data->m_regen; }
float TypedThing::glow() const { return m_data->m_gain[1] + m_data->m_glow; }

// a report line and a block comment naming the member are not reads of it
std::vector<std::string> TypedThing::stopLines() { return { "[S-1] m_unused is kept, nothing reads it" }; }
void TypedThing::note() { /* m_unused */ }

float TypedThing::speed() const
{
	if (m_data->m_dieMux.applies() && m_data->m_fx)
		return m_data->m_used * 2.0f;
	return 0.0f;
}

int TypedThing::boneCount() const
{
	int n = 0;
	for (int b : m_data->bones)
		n += b;
	return n;
}

float TypedThing::partWidth() const { return m_data->part.width; }
int TypedThing::slotOf() const { return m_data->m_slot + m_data->m_right + m_data->m_spanMax; }
int TypedThing::laneOf(int i) const { return m_data->lanes[i]; }

void TypedThing::registerModule(ModuleFactory &modules)
{
	static const char kName[] = "TypedThing";
	modules.bindTypedData<TypedThingModuleData>(kName, MODULETYPE_BEHAVIOR);
	modules.bindModuleProc(kName, MODULETYPE_BEHAVIOR, nullptr);
}
'''

# a module whose data derives from a base declaring the member; the base's own file reads it and never sees the derived struct
# (FlingPassengerSpecialAbilityUpdateModuleData's m_customAnimCondition, read in SpecialAbilityModules.cpp)
BASE_H = r'''
class BaseThingModuleData : public ModuleData
{
public:
	static void buildBaseFieldParse(MultiIniFieldParse &p);
	int m_baseLevel = 0;
	int m_baseFlag = 0;
};
'''
# the base's own table, added by the derived struct's unqualified buildBaseFieldParse(p) (UpgradeModule.cpp's CustomAnimAndDuration row)
BASE_CPP = r'''#include "GameLogic/Module/BaseThing.h"
int BaseThing::level() const { return static_cast<const BaseThingModuleData *>(m_data)->m_baseLevel + m_data->m_baseFlag; }
void parseBaseFlag(INI *ini, void *instance, void *, const void *) { static_cast<BaseThingModuleData *>(instance)->m_baseFlag = 1; }
const FieldParse kBase[] = { { "BaseFlag", parseBaseFlag, nullptr, 0 }, { nullptr, nullptr, nullptr, 0 } };
void BaseThingModuleData::buildBaseFieldParse(MultiIniFieldParse &p)
{
	p.add(kBase);
}
'''
DERIVED_CPP = r'''#include "GameLogic/Module/BaseThing.h"
class DerivedThingModuleData : public BaseThingModuleData
{
public:
	static void buildFieldParse(MultiIniFieldParse &p);
};
const FieldParse kDerived[] = {
	{ "BaseLevel", INI::parseInt, nullptr, (int)offsetof(DerivedThingModuleData, m_baseLevel) },
	{ nullptr, nullptr, nullptr, 0 }
};
void DerivedThingModuleData::buildFieldParse(MultiIniFieldParse &p)
{
	p.add(kDerived);
	buildBaseFieldParse(p);
}
void DerivedThing::registerModule(ModuleFactory &modules)
{
	modules.bindTypedData<DerivedThingModuleData>("DerivedThing", MODULETYPE_BEHAVIOR);
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
	NuggetThing copy;
	ini->initFromINI(&copy, NuggetThing::fieldParse());
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

# the nugget's table in a file of its own, handed out by name (EmotionSystem.cpp's EmotionNuggetTemplate::fieldParse() returns kNuggetParse)
NUGGET_CPP = r'''
struct NuggetThing
{
	unsigned duration = 0;
	static const FieldParse *fieldParse();
};
const FieldParse kNuggetParse[] = {
	{ "Duration", INI::parseDurationUnsignedInt, nullptr, (int)offsetof(NuggetThing, duration) },
	{ nullptr, nullptr, nullptr, 0 }
};
const FieldParse *NuggetThing::fieldParse()
{
	return kNuggetParse;
}
unsigned nuggetDuration(const NuggetThing &n) { return n.duration; }
'''

WEAPON_CPP = r'''
#include "GameLogic/Weapon.h"
class WeaponTemplate
{
public:
	float m_attackRange = 0.0f;
	float m_unread = 0.0f;
	float heat = 0.0f;
	float ranges[2] = {};
	int id = 0;
	float hashOnly = 0.0f;
	float saveOnly = 0.0f;
	// the member just above a one-line method reading it (LivingWorldAutoResolveData.h's scalar and damageScalar()) is no local of the method
	float scalar[4] = {};
	float scalarOf(int i) const { return scalar[i]; }
	// a getter under a save's declaration is no save (AutoPickUpUpdate.h's xfer(Xfer &x) override; then getRunFromButtonNumber())
	float readOnly = 0.0f;
	void xfer(Xfer &x);
	float readOf() const { return readOnly; }
	WeaponGauge gauge;
	struct Pair { int start, cap; } counts[2];
	static const FieldParse *getFieldParse();
};
// a row storing into an array element (EconomySettings.cpp's ES_OFF(goodMP[0])): the member is the array, not what the parser's body names
void parsePair(INI *ini, void *, void *store, const void *) { static_cast<WeaponTemplate::Pair *>(store)->cap = 1; }
void parseScalar(INI *ini, void *instance, void *, const void *) { static_cast<WeaponTemplate *>(instance)->scalar[kindOf(ini)] = 1.0f; }
const FieldParse *WeaponTemplate::getFieldParse()
{
	static const FieldParse table[] = {
		{ "AttackRange", INI::parseReal, nullptr, (int)offsetof(WeaponTemplate, m_attackRange) },
		{ "Unread", INI::parseReal, nullptr, (int)offsetof(WeaponTemplate, m_unread) },
		{ "Count2", parsePair, nullptr, (int)offsetof(WeaponTemplate, counts[1]) },
		{ "Range2", INI::parseReal, nullptr, (int)offsetof(WeaponTemplate, ranges) + 4 }, // a byte step into an array (SkirmishAIData.cpp's AD_OFF)
		{ "Id", INI::parseInt, nullptr, (int)offsetof(WeaponTemplate, id) }, // a two-letter member (AODCrushCollide.cpp's fx[0])
		{ "HashOnly", INI::parseReal, nullptr, (int)offsetof(WeaponTemplate, hashOnly) },
		{ "SaveOnly", INI::parseReal, nullptr, (int)offsetof(WeaponTemplate, saveOnly) },
		{ "ReadOnly", INI::parseReal, nullptr, (int)offsetof(WeaponTemplate, readOnly) },
		{ "Scalar", parseScalar, nullptr, 0 },
		{ nullptr, nullptr, nullptr, 0 }
	};
	return table;
}
// a parser defined inside another function (EvaEvents.cpp's parseRecord: struct Parsers { static void priority(..) { .. } })
void WeaponStore::parseExtra(INI *ini, WeaponTemplate &w)
{
	struct Parsers
	{
		static void heat(INI *ini2, void *instance, void *, const void *) { INI::parseReal(ini2, nullptr, &static_cast<WeaponTemplate *>(instance)->heat, nullptr); }
		static void cool(INI *ini2, void *instance, void *, const void *) { INI::parseReal(ini2, nullptr, &static_cast<WeaponTemplate *>(instance)->gauge.cool, nullptr); }
	};
	const FieldParse table[] = { { "Heat", Parsers::heat, nullptr, 0 }, { "Cool", Parsers::cool, nullptr, 0 }, { nullptr, nullptr, nullptr, 0 } };
	ini->initFromINI(&w, table);
}
// numbered rows made at run time (FXParticleSystem.cpp's "Color" + std::to_string(i + 1) into m_colorKey[i]); keyOf reads them
struct WeaponKeys
{
	int m_key[4];
	int m_tag[4];
};
const FieldParse *keyTable()
{
	static Table t;
	if (t.empty())
	{
		static WeaponKeys d;
		for (int i = 0; i < 4; ++i)
		{
			static std::string names[4];
			names[i] = "Key" + std::to_string(i + 1);
			t.push_back(ROW(names[i].c_str(), INI::parseInt, nullptr, d, m_key[i]));
		}
	}
	return t.data();
}
// the row's own line is no read: nothing reads m_tag
const FieldParse *tagTable()
{
	static Table t;
	if (t.empty())
	{
		static WeaponKeys d;
		for (int i = 0; i < 4; ++i)
		{
			static std::string names[4];
			names[i] = "Tag" + std::to_string(i + 1);
			t.push_back(ROW(names[i].c_str(), INI::parseInt, nullptr, d, m_tag[i]));
		}
	}
	return t.data();
}
int keyOf(const WeaponKeys &k) { return k.m_key[0]; }
float WeaponTemplate::heatOf() const { return heat; }
float WeaponTemplate::secondRange() const { return ranges[1]; }
int WeaponTemplate::idOf() const { return id; }
float WeaponTemplate::range() const { return m_attackRange; }
int WeaponTemplate::secondCount() const { return counts[1].start; }
// a hash of the state is no use of the value (Object.cpp's crcOmittingModules hashes m_fxTrigger): nothing else reads hashOnly
void WeaponTemplate::crcOmittingParts(StateHasher &h,
                                      bool deep) const
{
	h.addFloat(hashOnly);
}
// so is a save helper of any name (SnapshotXfer.cpp's damageInput(Xfer &x, DamageInfoInput &in)): nothing else reads saveOnly
void saveWeapon(Xfer &x, WeaponTemplate &w)
{
	x.xferReal(&w.saveOnly);
}
void WeaponStore::registerBlocks(INIBlockRegistry &registry)
{
	registry.registerBlock("Weapon", [](INI *ini) { WeaponStore::parseWeaponTemplateDefinitionGlobal(ini); });
}
'''

# a member of a member (the Cool row's gauge.cool) declared in a header; GaugeUse.cpp reads a cool of its own Dial, and never names WeaponGauge
WEAPON_H = r'''
struct WeaponGauge
{
	float cool = 0.0f;
};
'''
GAUGE_USE_CPP = r'''#include "GameLogic/Weapon.h"
struct Dial
{
	float cool = 0.0f;
};
float dialCool(const Dial &d) { return d.cool; }
'''

# a block whose rows come from a name list of its own and are asked for by name (MiscAudio.cpp's fieldNames(), HotKey.cpp's
# miscAudio.get("EnabledHotKeyPressed")); nothing asks for DingSound
MISC_H = r'''
class Misc
{
public:
	static const std::vector<std::string> &fieldNames();
	const std::string &get(const std::string &name) const;
};
struct Levels
{
	int volume = 0;
	int pitch = 0;
};
'''
MISC_CPP = r'''#include "Common/Audio/Misc.h"
const std::vector<std::string> &Misc::fieldNames()
{
	static const std::vector<std::string> names = {
		"ClickSound", "DingSound", "BuzzSound", "Pan",
	};
	return names;
}
// a name and the member it is read into on one line (AIPathfindConfig.cpp's getInt(gd, "MaxCellsPatchPath", c.patchPathLimit, ..))
void Misc::load(const Data &gd, Levels &out)
{
	getInt(gd, "Volume", out.volume, error);
	getInt(gd, "Pitch", out.pitch, error);
}
void Misc::registerBlocks(INIBlockRegistry &registry)
{
	registry.registerBlock("MiscAudio", [this](INI *ini) { parse(ini); });
}
'''
CLICK_CPP = r'''#include "Common/Audio/Misc.h"
const std::string &clickSound(const Misc &m) { return m.get("ClickSound"); }
int volumeOf(const Levels &l) { return l.volume; }
// a short name asked for is some other key; a name list of this file is read by index (which rows, is not followed)
int pan(const Misc &m) { return m.get("Pan").size(); }
const char *const kSamples[] = {
	"BuzzSound", "Other",
};
'''

# a stub block read by a reader of its own (TerrainRoads.cpp): its key tests store into a member; bridgeHeight reads height, nothing reads depth
BRIDGE_H = r'''
struct BridgeDef
{
	float height = 0.0f;
	float depth = 0.0f;
};
'''
BRIDGE_CPP = r'''#include "GameClient/BridgeReader.h"
void readBridges(const std::string &key, const std::string &val, BridgeDef &cur)
{
	// the file is "Data\INI\Bridge.ini"
	if (key == "height")
	{
		cur.height = (float)std::atof(val.c_str());
	}
	else if (key == "depth")
	{
		cur.depth = (float)std::atof(val.c_str());
	}
}
float bridgeHeight(const BridgeDef &b) { return b.height; }
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

# a loader copying the values into a settings struct (VisionSettings.cpp's `out.stealthFriendlyOpacity = s.stealth;`): a copy is a use only
# when what it is copied into is read; out.lonely is read by no one, out.shown is
COPY_CPP = r'''#include "GameLogic/Module/TypedThing.h"
struct Out
{
	int lonely = 0;
	int shown = 0;
};
void loadOut(const TypedThingModuleData &d, Out &out)
{
	out.lonely = d.m_copied;
	out.shown = clampShown(d.m_shown);
}
int shownOf(const Out &o) { return o.shown; }
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

# a nugget block of an FXList-like definition (FXList.cpp): the definition's fieldParse() hands out another function's table, whose rows parse
# each nugget with a template parser (parseNugget<TerrainScorchFXNugget>); the nugget's rows are its class's fieldParse() and the shared baseTable().
# Both nuggets have a Type row: Scorch's is m_scorchType, not Shake's m_type
FX_H = r'''
struct ShakeNugget
{
	int m_type = 0;
	const FieldParse *fieldParse() const;
};
struct ScorchNugget
{
	int m_scorchType = 0;
	int m_weather = 0;
	const FieldParse *fieldParse() const;
};
'''
FX_CPP = r'''#include "GameClient/FxThing.h"
#define ROW(token, proc, ud, obj, member) FieldParse{ token, proc, ud, offsetIn(obj, (obj).member) }
const FieldParse *baseTable()
{
	static Table t;
	static ScorchNugget d;
	t.push_back(ROW("Weather", INI::parseInt, nullptr, d, m_weather));
	return t.data();
}
template <class T>
void parseNugget(INI *ini, void *instance, void *, const void *)
{
	std::unique_ptr<T> n = std::make_unique<T>();
	MultiIniFieldParse tables;
	tables.add(n->fieldParse());
	tables.add(baseTable());
	ini->initFromINIMulti(n.get(), tables);
}
const FieldParse *ShakeNugget::fieldParse() const
{
	static Table t;
	static ShakeNugget d;
	t.push_back(ROW("Type", INI::parseInt, nullptr, d, m_type));
	return t.data();
}
const FieldParse *ScorchNugget::fieldParse() const
{
	static Table t;
	static ScorchNugget d;
	t.push_back(ROW("Type", INI::parseInt, nullptr, d, m_scorchType));
	return t.data();
}
struct FxAccess
{
	static const FieldParse *table()
	{
		static Table t;
		t.push_back(FieldParse{ "Shake", parseNugget<ShakeNugget>, nullptr, 0 });
		t.push_back(FieldParse{ "Scorch", parseNugget<ScorchNugget>, nullptr, 0 });
		return t.data();
	}
};
const FieldParse *FxThing::fieldParse() { return FxAccess::table(); }
void FxStore::parseDefinition(INI *ini) { ini->initFromINI(&m_fx, FxThing::fieldParse()); }
int ShakeNugget::shake() const { return m_type; }
int ScorchNugget::scorch() const { return m_scorchType + m_weather; }
void FxStore::registerBlocks(INIBlockRegistry &registry)
{
	registry.registerBlock("FxThing", [this](INI *ini) { parseDefinition(ini); });
}
'''

# a block whose table a registration helper gives and ends in a catch-all row (GameLogicSettings.cpp's GameData): Speed is read; any other name, a
# nested block whose key has no row included, is taken by the catch-all and applied nowhere; Order's nested block has a row of its own
LOGIC_CPP = r'''
struct BlockState
{
	float speed = 0.0f;
	int order = 0;
};
const FieldParse kLogicFields[] = {
	{ "Speed", INI::parseReal, nullptr, (int)offsetof(BlockState, speed) },
	{ "Order", parseOrderBlock, nullptr, 0 },
	{ nullptr, noteUnapplied, nullptr, 0 }
};
float speedOf(const BlockState &s) { return s.speed; }
void Load::reg(const char *name, const FieldParse *table)
{
	m_env.blocks.registerBlock(name, [this, table](INI *ini) { ini->initFromINI(&m_state, table); });
}
Load::Load()
{
	reg("Logic", kLogicFields, 1u);
}
'''

# a second reader of the Logic block with a table of its own, given after another argument (VisionSettings.cpp's Load m(nullptr,
# "MultiplayerSettings", kMultiplayer, ..)), and a hand-written reader of the block's text (Shell.cpp's readGameDataBool(text, "ShellMapOn"))
SHROUD_CPP = r'''
struct ShroudState
{
	bool shroud = false;
};
const FieldParse kShroudFields[] = {
	{ "Shroud", INI::parseBool, nullptr, (int)offsetof(ShroudState, shroud) },
	{ nullptr, nullptr, nullptr, 0 }
};
bool shroudOf(const ShroudState &s) { return s.shroud; }
bool loadShroud(ShroudState &s)
{
	Load m(nullptr, "Logic", kShroudFields, &s);
	return m.run();
}
'''
# a row's parser of another file calling a parse method on the object (FunctionCurve::parseFunctionCurve's curve->parse(ini)), whose table is
# local to the method
CURVE_CPP = r'''
struct Curve
{
	float m_key = 0.0f;
	float m_pad = 0.0f;
};
float keyOf(const Curve &c) { return c.m_key; }
void Curve::parse(INI *ini)
{
	FieldParse fields[] = {
		{ "Key", INI::parseReal, nullptr, (int)offsetof(Curve, m_key) },
		{ "Pad", INI::parseReal, nullptr, (int)offsetof(Curve, m_pad) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	ini->initFromINI(this, fields);
}
void Curve::parseCurve(INI *ini, void *, void *store, const void *)
{
	auto *curve = reinterpret_cast<Curve *>(store);
	curve->parse(ini);
}
'''

SHELL_CPP = r'''
bool shellOn(const std::string &text)
{
	bool inBlock = text == "logic";
	return inBlock && readBool(text, "ShellOn");
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
  ReplaceModule ModuleTag_09
    Behavior = TypedThing ModuleTag_10
      Rare = 2
      MeleeBehavior = Amoeba
        FacingBonus = 10.0
      End
    End
  End
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
    Bone = BONE01
    PartWidth = 2
    PartDepth = 3
    Angle = 4
    Slot = 1
    Left = 1
    Span = 1 2
    Lane = LEFT
    LateStep3 = 1
    RightFlag = 1
    Regen%PerSecond = 5%
    Plain = 1
    GlobalLateGain = 1
    NightGlow = 2
    DeathTypes = ALL
    Names = A B
    Level = 2
    Rare = 1
    Shared = 1
    Copied = 1
    Factor = 2
    Shown = 1
    TagB = X
    MeleeBehavior = Amoeba
      FacingBonus = 30.0
    End      Lost = 1
    Curve = FCurve
      Key = 1
      Pad = 2
    End
    UnitSpecificSounds
      VoiceGarrison = TestUnitVoiceGarrison
      VoiceNobody = A
    End
    Envelope InitialOpacity = 0.0 PeakOpacity = 1.0
    Bogus = 3
  End
  Behavior = DerivedThing ModuleTag_06
    BaseLevel = 2
    BaseFlag = 1
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
  Heat = 1
  Range2 = 5
  Id = 7
  HashOnly = 1
  SaveOnly = 1
  ReadOnly = 1
  Scalar = HERO 50%
  Cool = 2
  Key2 = 3
  Tag3 = 1
End

MiscAudio
  ClickSound = A
  DingSound = B
  Volume = 1
  Pitch = 2
  BuzzSound = C
  Pan = D
End

ArmyDefinition TestArmy
  Side = Men
  Unit = Soldier
  Tag = A
  Left = B
  Banner = 1
  Speed = 2
  Slot = 1
  Member Archers
    Unit Archer
  End
End

AIBase TestBase
  Side = Men
End

Scheme TestScheme
  ScreenCreationRes X:1024 Y:768
  Side Men
  ImagePart
    Position X:0 Y:520
    Layer 4
  End
End

FxThing TestFx
  Scorch
    Type = 2
    Weather = 1
  End
End

Bridge TestBridge
  Width = 5
  Height = 2
  Depth = 3
End

Logic
  Speed = 2
  ShellMap = X
  Shroud = Yes
  ShellOn = Yes
  BuildList Rohan
    Structure Fort
      Angle = 1
    End
  End
  Order
    Rank = 1
  End
End
'''


def rw_tables(rw: Path):
    """the RotWK field tables of the made-up classes, the Object table and the Object's audio table"""
    def table(*names, fn="0x1"):
        return {"rows": [{"name": n, "fn": fn, "offset": 0, "userData": 0} for n in names], "terminated": True}
    tables = {
        "0xa1": {"rows": table("Used", "Unused", "FX", "Bone", "PartWidth", "PartDepth", "Angle", "Slot", "Left", "Span", "Lane", "LateStep3", "RightFlag", "Regen%PerSecond", "Plain", "GlobalLateGain", "NightGlow", "DeathTypes", "Names", "Envelope", "Level", "Rare", "TagB", "Shared", "Copied", "Shown", "Factor")["rows"]
                 + [{"name": "MeleeBehavior", "fn": "0xb3", "offset": 0, "userData": 0}, {"name": "UnitSpecificSounds", "fn": "0xb4", "offset": 0, "userData": 0},
                    {"name": "Curve", "fn": "0xb6", "offset": 0, "userData": 0}],
                 "terminated": True},
        "0xac": table("Key", "Pad"),
        "0xa2": table("Radius", "Height"),
        "0xa3": table("Bar"),
        "0xa4": {"rows": [{"name": "AddEmotion", "fn": "0xb1", "offset": 0, "userData": 0}], "terminated": True},
        "0xa5": table("Duration"),
        "0xa6": {"rows": [{"name": "BeginScript", "fn": "0xb2", "offset": 0, "userData": 0}, {"name": "Model", "fn": "0x1", "offset": 0, "userData": 0}],
                 "terminated": True},
        "0xa7": {"rows": table("BuildCost", "VisionRange", "PreviewDecal", "PreviewTint", "PreviewScale", "Behavior", "Draw")["rows"]
                 + [{"name": n, "fn": "0xb4", "offset": 0, "userData": 0} for n in ("UnitSpecificSounds", "LookDecal", "Award", "Threat")]
                 + [{"name": "ReplaceModule", "fn": "0xb7", "offset": 0, "userData": 0}],
                 "terminated": True},
        "0xa9": table("FacingBonus"),
        "0xab": table("BaseLevel", "BaseFlag"),
        "0xaa": {"rows": [], "catchAll": "0xb5", "terminated": True},
        "0xa8": table("VoiceSelect", "VoiceFear"),
    }
    functions = {"0x1": {"kind": "line"}, "0xb1": {"kind": "block", "tables": [{"extra": 0, "table": "0xa5"}]},
                 "0xb2": {"kind": "script", "terminator": "ENDSCRIPT"},
                 # MeleeBehavior's table is picked by its value (RW 0x86C30A); UnitSpecificSounds' table takes any name
                 "0xb3": {"kind": "block", "tables": [], "dispatch": {"names": ["Swarm", "Amoeba"], "tables": [[], [{"extra": 0, "table": "0xa9"}]]}},
                 "0xb4": {"kind": "block", "tables": [{"extra": 0, "table": "0xaa"}]}, "0xb6": {"kind": "block", "tables": [{"extra": 0, "table": "0xac"}]},
                 # ReplaceModule's sub-table is the Object's own (a module inside, then its End)
                 "0xb7": {"kind": "block", "tables": [{"extra": 0, "table": "0xa7"}]}}
    (rw / "field-tables.json").write_text(json.dumps({"tables": tables, "functions": functions, "objectTable": {"table": "0xa7", "extra": 0},
                                                      "audioTable": {"table": "0xa8", "extra": 292}}),
                                          encoding="utf-8")
    classes = [{"name": n, "tables": [{"table": t, "extra": 0}]} for n, t in
               (("TypedThing", "0xa1"), ("RawThing", "0xa2"), ("GoneThing", "0xa3"), ("EmotionThing", "0xa4"), ("ScriptThing", "0xa6"), ("DerivedThing", "0xab"))]
    (rw / "module-registry.json").write_text(json.dumps({"classes": classes}), encoding="utf-8")


COVERAGE = """class\ttype\tstatus\ttyped\tai\tdrawkind
TypedThing\tBehavior\tported\t1\t0\t-
RawThing\tBehavior\tported\t0\t0\t-
GoneThing\tBehavior\tmissing\t0\t0\t-
EmotionThing\tBehavior\tported\t1\t0\t-
ScriptThing\tDraw\tmissing\t0\t0\t5
DerivedThing\tBehavior\tported\t1\t0\t-
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
                      "GameLogic/Module/Other.cpp": OTHER_CPP, "GameLogic/Module/Ambig.cpp": AMBIG_CPP, "GameLogic/Module/Copy.cpp": COPY_CPP, "GameLogic/AI/Army.cpp": ARMY_CPP, "GameClient/Voice.cpp": VOICE_CPP, "GameClient/Decal.h": DECAL_H, "Common/ADecal.h": OTHER_DECAL_H,
                      "GameClient/Decal.cpp": DECAL_CPP, "GameLogic/AI/Threat.cpp": THREAT_CPP,
                      "GameClient/Settings.h": SETTINGS_H, "GameClient/Middle.h": MIDDLE_H, "GameLogic/Weapon.h": WEAPON_H,
                      "GameLogic/GaugeUse.cpp": GAUGE_USE_CPP, "GameLogic/Module/BaseThing.h": BASE_H, "GameLogic/Module/BaseThing.cpp": BASE_CPP,
                      "GameLogic/Module/DerivedThing.cpp": DERIVED_CPP, "Common/Audio/Misc.h": MISC_H, "Common/Audio/Misc.cpp": MISC_CPP,
                      "GameClient/Click.cpp": CLICK_CPP, "GameClient/BridgeReader.h": BRIDGE_H, "GameClient/BridgeReader.cpp": BRIDGE_CPP,
                      "GameClient/FxThing.h": FX_H, "GameClient/FxThing.cpp": FX_CPP, "GameLogic/System/Nugget.cpp": NUGGET_CPP,
                      "GameLogic/Logic.cpp": LOGIC_CPP, "GameLogic/Shroud.cpp": SHROUD_CPP,
                      "GameClient/Shell.cpp": SHELL_CPP, "Common/Curve.cpp": CURVE_CPP}.items():
        (src / rel).parent.mkdir(parents=True, exist_ok=True)
        (src / rel).write_text(text, encoding="utf-8")
    rw = root / "rw"
    rw.mkdir()
    rw_tables(rw)
    (root / "coverage.tsv").write_text(COVERAGE, encoding="utf-8")
    source = fa.Source(src)
    every, real = source.block_keywords()
    diag: dict = {}
    mod = fa.mod_fields(MemoryVfs({"data/ini/object/test.ini": MOD_INI}), every, diag, fa.Grammar(rw), source.line_rows(every & real))
    rw_cls, rw_obj = fa.rotwk_tables(rw)
    rows = fa.audit(mod, rw_cls, rw_obj, fa.load_coverage(root / "coverage.tsv"), source, real)
    return {(r["block"], r["owner"], r["field"]): r for r in rows}, diag, mod, source


def status(result, block, owner, field):
    return result[0][(block, owner, field)]["status"]


def test_typed_module_fields(result):
    assert status(result, "object", "TypedThing", "Used") == "used"
    used_line = next(i for i, x in enumerate(TYPED_CPP.splitlines(), 1) if "return m_data->m_used" in x)
    assert result[0][("object", "TypedThing", "Used")]["where"] == f"GameLogic/Module/TypedThing.cpp:{used_line}"  # the read in speed(), not the table row
    assert result[0][("object", "TypedThing", "Used")]["defs"] == 2
    assert status(result, "object", "TypedThing", "Unused") == "parsed-unused"  # &INI::parseReal: the row is still found
    # (the member's name in a string literal or a /* */ comment of a visible file is not a read)
    assert status(result, "object", "TypedThing", "DeathTypes") == "used"  # a row of the multi-line MUX_ROWS(T) macro
    assert result[0][("object", "TypedThing", "DeathTypes")]["where"] == "GameLogic/Module/DieMux.cpp:2"  # m_dieMux.m_deathTypes: read anywhere
    assert status(result, "object", "TypedThing", "FX") == "used"  # its own parser stores m_fx, which speed() reads
    # its own parser hands (instance)->bones to a helper; boneCount() reads it
    assert status(result, "object", "TypedThing", "Bone") == "used"
    assert result[0][("object", "TypedThing", "Bone")]["member"] == "bones"
    # parsers a macro defines, each storing through the address of a member: partWidth() reads width, nothing reads depth or angle
    assert (status(result, "object", "TypedThing", "PartWidth"), result[0][("object", "TypedThing", "PartWidth")]["member"]) == ("used", "width")
    assert (status(result, "object", "TypedThing", "PartDepth"), result[0][("object", "TypedThing", "PartDepth")]["member"]) == ("parsed-unused", "depth")
    assert (status(result, "object", "TypedThing", "Angle"), result[0][("object", "TypedThing", "Angle")]["member"]) == ("parsed-unused", "angle")
    assert (status(result, "object", "TypedThing", "Slot"), result[0][("object", "TypedThing", "Slot")]["member"]) == ("used", "m_slot")
    left = result[0][("object", "TypedThing", "Left")]
    assert left["status"] == "used" and "INFERENCE: its parser names several members (m_left|m_right)" in left["where"]
    span = result[0][("object", "TypedThing", "Span")]
    assert span["status"] == "used" and "INFERENCE" not in span["where"]
    assert (status(result, "object", "TypedThing", "Lane"), result[0][("object", "TypedThing", "Lane")]["member"]) == ("used", "lanes")
    # rows a builder makes at run time: kPhases[p] + "Step" + to_string(i + 1) up to MAX_STEPS, and the elements of kSides
    assert (status(result, "object", "TypedThing", "LateStep3"), result[0][("object", "TypedThing", "LateStep3")]["member"]) == ("used", "m_steps")
    assert (status(result, "object", "TypedThing", "RightFlag"), result[0][("object", "TypedThing", "RightFlag")]["member"]) == ("parsed-unused", "m_flags")
    assert (status(result, "object", "TypedThing", "GlobalLateGain"), result[0][("object", "TypedThing", "GlobalLateGain")]["member"]) == ("used", "m_gain")
    assert (status(result, "object", "TypedThing", "NightGlow"), result[0][("object", "TypedThing", "NightGlow")]["member"]) == ("used", "m_glow")
    assert (status(result, "object", "TypedThing", "Regen%PerSecond"), result[0][("object", "TypedThing", "Regen%PerSecond")]["member"]) == ("used", "m_regen")
    assert (status(result, "object", "TypedThing", "Plain"), result[0][("object", "TypedThing", "Plain")]["member"]) == ("used", "m_plain")
    # a row of the base's table, added by an unqualified buildBaseFieldParse(p) of the base's file
    assert (status(result, "object", "DerivedThing", "BaseFlag"), result[0][("object", "DerivedThing", "BaseFlag")]["member"]) == ("used", "m_baseFlag")
    assert status(result, "object", "TypedThing", "Names") == "parsed-unused"  # `std::vector<std::string> m_names;` is a declaration, not a read
    # a nested block read by a parse method its row's parser calls on the object (curve->parse(ini)): the method's local table
    assert (status(result, "object", "TypedThing/Curve", "Key"), result[0][("object", "TypedThing/Curve", "Key")]["member"]) == ("used", "m_key")
    assert status(result, "object", "TypedThing/Curve", "Pad") == "parsed-unused"
    # a read in a file naming the struct comes first (Other.cpp sorts before TypedThing.cpp); a read only in a file that never names it is marked
    assert status(result, "object", "TypedThing", "Level") == "used"
    assert result[0][("object", "TypedThing", "Level")]["where"] == "GameLogic/Module/Other.cpp:2 INFERENCE: the file does not name TypedThingModuleData"
    assert result[0][("object", "TypedThing", "Rare")]["where"] == "GameLogic/Module/Other.cpp:3"  # the only m_rare of the source
    assert status(result, "object", "TypedThing", "TagB") == "parsed-unused"
    assert status(result, "object", "TypedThing", "Factor") == "used"
    assert status(result, "object", "TypedThing", "Copied") == "parsed-unused"  # copied into out.lonely, which no one reads
    assert result[0][("object", "TypedThing", "Shown")]["where"] == "GameLogic/Module/Copy.cpp:10"  # copied into out.shown, which shownOf reads
    assert result[0][("object", "TypedThing", "Shared")]["where"] == ("GameLogic/Module/Ambig.cpp:2 AMBIGUOUS: the file also names Filter with a "
                                                                      "member m_shared")
    assert status(result, "object", "TypedThing", "Bogus") == "not-in-rotwk"  # MeleeBehavior's End did not close the module
    # a member of the base, read where the base is seen
    assert result[0][("object", "DerivedThing", "BaseLevel")]["where"] == "GameLogic/Module/BaseThing.cpp:2"


def test_rotwk_grammar_blocks(result):
    # a block whose table is picked by the value (MeleeBehavior = Amoeba): its rows are the block's, its End is not the module's
    assert ("object", "TypedThing/MeleeBehavior", "FacingBonus") in result[0]
    # its `End      Lost = 1` (a lost line break) is its End: INI reads the first token, the rest of the line is not read
    assert not any(k[1].startswith("TypedThing/MeleeBehavior") and k[2] in ("End", "Lost") for k in result[0])
    assert ("object", "TypedThing/UnitSpecificSounds", "VoiceGarrison") in result[0]
    # ReplaceModule's module (with a block of its own inside) and both Ends: the Object goes on after them
    assert result[0][("object", "TypedThing", "Rare")]["uses"] == 2 and result[0][("object", "TypedThing/MeleeBehavior", "FacingBonus")]["uses"] == 2
    assert not result[1]["unclosed"] and ("object", "", "LookDecal") in result[0]
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
    # the nugget's rows: a fieldParse() returning its table by name
    duration = result[0][("object", "EmotionThing/AddEmotion", "Duration")]
    assert (duration["status"], duration["member"]) == ("used", "duration")
    assert status(result, "object", "EmotionThing", "AddEmotion") == "used"
    assert result[0][("object", "EmotionThing", "AddEmotion")]["uses"] == 3
    # BeginScript .. EndScript is one field; the lines inside are neither fields nor blocks (a `Prev = ...` line inside is not a field)
    assert ("object", "ScriptThing", "BeginScript") in result[0]
    assert ("object", "ScriptThing", "Prev") not in result[0]
    assert status(result, "object", "ScriptThing", "Model") == "missing-module"
    # outside RotWK's grammar a line without '=' is a field unless an End closes it (ControlBarScheme's `Side Mordor`, then ImagePart .. End);
    # a one-word line is the block before the lines after it
    mod = result[2]
    for key in (("scheme", "", "ScreenCreationRes"), ("scheme", "", "Side"), ("scheme", "/ImagePart", "Position"), ("scheme", "/ImagePart", "Layer")):
        assert key in mod, key
    assert mod[("scheme", "", "Side")].values == {"Men": 1}
    # a name the port's table parses as one line is never the block (stances.ini's `Stance Aggressive` / `AttributeModifier X` / End)
    assert mod[("armydefinition", "/Member", "Unit")].values == {"Archer": 1}
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
    assert (status(result, "weapon", "", "Heat"), result[0][("weapon", "", "Heat")]["member"]) == ("used", "heat")  # a parser inside parseExtra
    assert (status(result, "weapon", "", "Range2"), result[0][("weapon", "", "Range2")]["member"]) == ("used", "ranges")
    assert (status(result, "weapon", "", "Id"), result[0][("weapon", "", "Id")]["member"]) == ("used", "id")
    assert status(result, "weapon", "", "HashOnly") == "parsed-unused"  # only crcOmittingParts(StateHasher &h, ..) reads it
    assert status(result, "weapon", "", "SaveOnly") == "parsed-unused"  # only saveWeapon(Xfer &x, ..) reads it
    assert status(result, "weapon", "", "ReadOnly") == "used"  # readOf() under `void xfer(Xfer &x);`
    scalar = result[0][("weapon", "", "Scalar")]
    assert (scalar["status"], scalar["member"], scalar["where"]) == ("used", "scalar", "GameLogic/Object/Weapon.cpp:" + str(
        next(i for i, x in enumerate(WEAPON_CPP.splitlines(), 1) if "return scalar[i]" in x)))
    # the struct of a member of a member is its declaring class: Dial's d.cool is no read of WeaponGauge::cool (only a marked lead)
    assert result[0][("weapon", "", "Cool")]["where"] == "GameLogic/GaugeUse.cpp:6 INFERENCE: the file does not name WeaponGauge"
    # rows made at run time: a numbered one follows the member it stores; a listed one is read when another file asks for its name
    assert (status(result, "weapon", "", "Key2"), result[0][("weapon", "", "Key2")]["member"]) == ("used", "m_key")
    assert (status(result, "weapon", "", "Tag3"), result[0][("weapon", "", "Tag3")]["member"]) == ("parsed-unused", "m_tag")
    assert status(result, "miscaudio", "", "ClickSound") == "used"
    assert result[0][("miscaudio", "", "ClickSound")]["where"] == "GameClient/Click.cpp:2 (asks for the row by name)"
    assert status(result, "miscaudio", "", "DingSound") == "dynamic-row"
    assert (status(result, "miscaudio", "", "Volume"), result[0][("miscaudio", "", "Volume")]["where"]) == ("used", "GameClient/Click.cpp:3")
    assert status(result, "miscaudio", "", "Pitch") == "parsed-unused"
    assert status(result, "miscaudio", "", "Pan") == "dynamic-row"
    buzz = result[0][("miscaudio", "", "BuzzSound")]
    assert (buzz["status"], buzz["where"]) == ("dynamic-row", "GameClient/Click.cpp:7 (a name list of another file, read by index)")
    # a nugget's rows: its template parser's class (parseNugget<ScorchNugget> -> ScorchNugget::fieldParse) and the shared table, reached through
    # the definition's fieldParse() handing out another function's table
    scorch = result[0][("fxthing", "/Scorch", "Type")]
    assert (scorch["status"], scorch["member"], scorch["how"]) == ("used", "m_scorchType", "block-parser")
    assert (status(result, "fxthing", "/Scorch", "Weather"), result[0][("fxthing", "/Scorch", "Weather")]["member"]) == ("used", "m_weather")
    assert status(result, "bridge", "", "Width") == "stub-block"
    # its own reader stores Height and Depth into members (TerrainRoads.cpp): followed
    assert (status(result, "bridge", "", "Height"), result[0][("bridge", "", "Height")]["where"]) == ("used", "GameClient/BridgeReader.cpp:14")
    assert status(result, "bridge", "", "Depth") == "parsed-unused"
    assert ("object", "SecondUnit", "Used") not in result[0]
    # the block table's catch-all row takes a name no row has, and a nested block whose key has no row: loaded, applied nowhere (S-152)
    assert status(result, "logic", "", "Speed") == "used"
    catch = f"GameLogic/Logic.cpp:{next(i for i, x in enumerate(LOGIC_CPP.splitlines(), 1) if 'noteUnapplied' in x)}"
    for owner, field in (("", "ShellMap"), ("/BuildList/Structure", "Angle")):
        row = result[0][("logic", owner, field)]
        assert (row["status"], row["how"]) == ("not-read", "catch-all") and row["where"].startswith(catch + " (the catch-all row: noteUnapplied"), field
    assert status(result, "logic", "/Order", "Rank") == "no-table-row"  # Order has a row: its parser's table is not followed here
    # the block's second reader has a row of it; a hand-written reader of the block's text names it (a hand check)
    assert (status(result, "logic", "", "Shroud"), result[0][("logic", "", "Shroud")]["member"]) == ("used", "shroud")
    shell = result[0][("logic", "", "ShellOn")]
    assert (shell["status"], shell["where"]) == ("own-reader", "GameClient/Shell.cpp:" + str(next(i for i, x in enumerate(SHELL_CPP.splitlines(), 1) if "ShellOn" in x)))


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
    assert len(fa.load_coverage(cov)) == len(COVERAGE.splitlines()) - 1  # every class row of COVERAGE, the quoted one too


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
                      "object\tTypedThing\tRegen%PerSecond\tused\tused\tx.cpp:3\n"  # a '%' in the name
                      "aidata\t\t(75 fields)\tstub-block\tused\tGameLogicSettings.cpp:401\n", encoding="utf-8")
    by = {(r["block"], r["owner"], r["field"]): r for r in rows}
    by[("object", "TypedThing", "Names")]["status"] = "custom-parser"  # (FormationPreviewItemDecal: parsed-unused when checked, custom-parser now)
    got = fa.apply_manual(rows, manual)
    fx = by[("object", "TypedThing", "FX")]
    assert (fx["status"], fx["tool"], fx["how"]) == ("used", "used", "manual")
    assert fx["where"] == "GameLogic/Module/TypedThing.cpp:79 MANUAL: parseFX stores m_fx"
    assert got["applied"] == 4 and by[("object", "TypedThing", "Regen%PerSecond")]["how"] == "manual"
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
    # a module's UnitSpecificSounds (a sound selector's, its table a catch-all row): asked for through the same reader
    assert (status(result, "object", "TypedThing/UnitSpecificSounds", "VoiceGarrison"),
            result[0][("object", "TypedThing/UnitSpecificSounds", "VoiceGarrison")]["how"]) == ("used", "by-name")
    assert status(result, "object", "TypedThing/UnitSpecificSounds", "VoiceNobody") == "parsed-unused"


def test_object_nested_readers(result):
    """an Object nested block parsed by a second reader (through the lambda's table and the nested parser's own), or kept raw and read by name"""
    texture = result[0][("object", "/LookDecal", "Texture")]
    assert texture["status"] == "used" and texture["where"].startswith("GameClient/Decal.cpp:")  # the Decal Decal.cpp sees, not ADecal.h's
    assert status(result, "object", "/LookDecal", "Tint") == "parsed-unused"
    assert status(result, "object", "/Award", "Shade") == "parsed-unused"  # the parser's own check that throws is no use
    assert status(result, "object", "/Threat", "Kind") == "used"
    assert status(result, "object", "/Threat", "Other") == "parsed-unused"
