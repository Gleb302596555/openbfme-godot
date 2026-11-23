// OpenBFME. GPL-3.0.
//
// Module data of W3DTreeDraw / W3DPropDraw / W3DFloorDraw. See W3DTreeDraw.h for the sources.

#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DTreeDraw.h"
#include "Common/Xfer.h"
#include "GameLogic/SnapshotXfer.h"

#include <cstddef>
#include <cstdint>

#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif

namespace
{
// RW 0xDA3A84
const char *const kWeatherNames[] = { "NORMAL", "SNOWY", nullptr };

// RW 0x73A302: the FXList is looked up by name (an unknown, non-empty, non-"None" name is an INI error in retail). The FXList store
// is not ported (stop S-111): the name is kept as written.
void parseFXListName(INI *ini, void *, void *store, const void *)
{
	*static_cast<std::string *>(store) = ini->getNextAsciiString();
}

// RW 0x4CF066: WeatherTexture = WEATHER NAME. The weather word is parseIndexList over RW 0xDA3A84, the name parseAsciiString.
void parseWeatherTexture(INI *ini, void *, void *store, const void *)
{
	const int weather = INI::scanIndexList(ini->getNextToken(), kWeatherNames);
	const std::string name = ini->getNextAsciiString();
	static_cast<std::vector<std::pair<int, std::string>> *>(store)->emplace_back(weather, name);
}

// RW 0x4CEF06: HideIfModelConditions = flags. A fresh flag set is parsed from the rest of the line and appended.
void parseHideIfModelConditions(INI *ini, void *, void *store, const void *)
{
	ModelConditionFlags flags;
	ModelCondition::parseFromLine(ini, flags);
	static_cast<std::vector<ModelConditionFlags> *>(store)->push_back(flags);
}

#define TREE_ROW(name, parse, field) { name, parse, nullptr, (int)offsetof(W3DTreeDrawModuleData, field) }

// RW 0xBE2E80, 24 rows in the binary's order.
const FieldParse kTreeFieldParse[] = {
	TREE_ROW("ModelName", INI::parseAsciiString, m_modelName),
	TREE_ROW("TextureName", INI::parseAsciiString, m_textureName),
	TREE_ROW("MoveOutwardTime", INI::parseDurationUnsignedInt, m_framesToMoveOutward),
	TREE_ROW("MoveInwardTime", INI::parseDurationUnsignedInt, m_framesToMoveInward),
	TREE_ROW("MoveOutwardDistanceFactor", INI::parseReal, m_maxOutwardMovement),
	TREE_ROW("DarkeningFactor", INI::parseReal, m_darkening),
	TREE_ROW("ToppleFX", parseFXListName, m_toppleFX),
	TREE_ROW("BounceFX", parseFXListName, m_bounceFX),
	TREE_ROW("StumpName", INI::parseAsciiString, m_stumpName),
	TREE_ROW("KillWhenFinishedToppling", INI::parseBool, m_killWhenToppled),
	TREE_ROW("DoTopple", INI::parseBool, m_doTopple),
	TREE_ROW("InitialVelocityPercent", INI::parsePercentToReal, m_initialVelocityPercent),
	TREE_ROW("InitialAccelPercent", INI::parsePercentToReal, m_initialAccelPercent),
	TREE_ROW("BounceVelocityPercent", INI::parsePercentToReal, m_bounceVelocityPercent),
	TREE_ROW("MinimumToppleSpeed", INI::parsePositiveNonZeroReal, m_minimumToppleSpeed),
	TREE_ROW("SinkDistance", INI::parsePositiveNonZeroReal, m_sinkDistance),
	TREE_ROW("SinkTime", INI::parseDurationUnsignedInt, m_sinkFrames),
	TREE_ROW("MorphTree", INI::parseAsciiString, m_morphTree),
	TREE_ROW("MorphTime", INI::parseDurationUnsignedInt, m_morphTime),
	TREE_ROW("MorphFX", parseFXListName, m_morphFX),
	TREE_ROW("TaintedTree", INI::parseBool, m_taintedTree),
	TREE_ROW("FadeRate", INI::parseUnsignedInt, m_fadeRate),
	TREE_ROW("FadeTarget", INI::parseUnsignedInt, m_fadeTarget),
	TREE_ROW("FadeDistance", INI::parseReal, m_fadeDistance),
	{ nullptr, nullptr, nullptr, 0 }
};
#undef TREE_ROW

// RW 0xBE3340
const FieldParse kPropFieldParse[] = {
	{ "ModelName", INI::parseAsciiString, nullptr, (int)offsetof(W3DPropDrawModuleData, m_modelName) },
	{ "DistanceFog", INI::parseBool, nullptr, (int)offsetof(W3DPropDrawModuleData, m_distanceFog) },
	{ nullptr, nullptr, nullptr, 0 }
};

// RW 0xBE3548
const FieldParse kFloorFieldParse[] = {
	{ "StaticModelLODMode", INI::parseBool, nullptr, (int)offsetof(W3DFloorDrawModuleData, m_staticModelLODMode) },
	{ "ForceToBack", INI::parseBool, nullptr, (int)offsetof(W3DFloorDrawModuleData, m_forceToBack) },
	{ "StartHidden", INI::parseBool, nullptr, (int)offsetof(W3DFloorDrawModuleData, m_startHidden) },
	{ "FloorFadeRateOnObjectDeath", INI::parseReal, nullptr, (int)offsetof(W3DFloorDrawModuleData, m_floorFadeRateOnObjectDeath) },
	{ "WeatherTexture", parseWeatherTexture, nullptr, (int)offsetof(W3DFloorDrawModuleData, m_weatherTextures) },
	{ "HideIfModelConditions", parseHideIfModelConditions, nullptr, (int)offsetof(W3DFloorDrawModuleData, m_hideIfModelConditions) },
	{ nullptr, nullptr, nullptr, 0 }
};
} // namespace

W3DTreeDrawModuleData::W3DTreeDrawModuleData() = default;

void W3DTreeDrawModuleData::buildFieldParse(MultiIniFieldParse &p)
{
	p.add(kTreeFieldParse); // registry: tables [0xBE2E80]
}

void W3DPropDrawModuleData::buildFieldParse(MultiIniFieldParse &p)
{
	p.add(kPropFieldParse); // registry: tables [0xBE3340]
}

void W3DFloorDrawModuleData::buildFieldParse(MultiIniFieldParse &p)
{
	p.add(kPropFieldParse);  // registry: tables [0xBE3340, 0xBE3548]
	p.add(kFloorFieldParse);
}

namespace W3DTreeDrawTables
{
const FieldParse *tree() { return kTreeFieldParse; }
const FieldParse *prop() { return kPropFieldParse; }
const FieldParse *floor() { return kFloorFieldParse; }
const char *const *weatherNames() { return kWeatherNames; }
} // namespace W3DTreeDrawTables

// ---------------------------------------------------------------------------------------------------------------------
// W3DPropDrawState & W3DFloorDrawState (Local 88 #714)
// TARGET W3DPropDraw: RW 0x004B1B3F xfer, RW 0x004CE6AE GetSnapshotName, RW 0x004CE6BF getModuleNameKey,
//   RW 0x004CE75C getModelName, RW 0x004CE793 reactToTransformChange, RW 0x004CE84C ModuleData deleting dtor.
// TARGET W3DFloorDraw: RW 0x004CE868 xfer, RW 0x004CE95F GetSnapshotName, RW 0x004CE971 getModuleNameKey,
//   RW 0x004CEA6F reactToTransformChange, RW 0x004CEB80 replaceModelConditionState, RW 0x004CEBCA doDrawModule,
//   RW 0x004CEC85 scalar deleting dtor, RW 0x004CECA1 setSubObjectHidden, RW 0x004CECB5 setHidden,
//   RW 0x004CF013 ModuleData scalar deleting dtor.
// ---------------------------------------------------------------------------------------------------------------------

bool W3DPropDrawState::reactToTransformChange(float posX, float posY)
{
	// RW 0x004CE793: +0x0C is the "record made" flag (RW 0x4CE7A1); the drawable's position comes from RW 0x676711 (RW 0x4CE7B1 /
	// RW 0x4CE7C9) and (0, 0) returns before the flag is set (RW 0x4CE7BA..0x4CE7DE), so a prop at the origin is never handed over.
	if (m_propCreated || (posX == 0.0f && posY == 0.0f))
	{
		return false;
	}
	m_propCreated = true; // RW 0x4CE7E0
	if (m_data == nullptr) // RW 0x4CE7E4..0x4CE7E9: no module data, no hand-over
	{
		return false;
	}
	// RW 0x4CE7F2..0x4CE808: the module data's DistanceFog (+0xC) and ModelName (+8), the position and the drawable's scale go to
	// RW 0x467AD4 -> RW 0x4EE5B8. Nothing re-reads the drawable afterwards, so the record keeps this first position.
	m_record.added = true;
	m_record.modelName = m_data->m_modelName;
	m_record.distanceFog = m_data->m_distanceFog;
	m_record.x = posX;
	m_record.y = posY;
	return true;
}

void W3DPropDrawState::xfer(Xfer &x)
{
	// RW 0x004B1B3F (DONOR BFME2 0x000B221A, W3DPropDrawXfer.cpp)
	SnapshotXfer::version(x, 1, 1);
}

W3DFloorDrawState::W3DFloorDrawState(const W3DFloorDrawModuleData *data)
	: W3DPropDrawState(data)
	, m_floorData(data)
	// RW 0x004CE91B: the constructor clears +0x14..+0x17 (RW 0x4CE93D..0x4CE94D) and then, when the module data's StartHidden (+0x1E)
	// is set, sets +0x14 (RW 0x4CE950..0x4CE955) - the flag RW 0x4CEA6F tests before it adds the record. The three hide flags stay
	// clear, so a StartHidden floor keeps its model and is visible to the refresh below; it is only its first-transform registration
	// that StartHidden suppresses.
	, m_registered(data ? data->m_startHidden : false)
{
}

std::string W3DFloorDrawState::resolveWeatherTexture(int weather) const
{
	// RW 0x004CEA1C
	if (!m_floorData)
	{
		return std::string();
	}
	for (const auto &entry : m_floorData->m_weatherTextures)
	{
		if (entry.first == weather)
		{
			return entry.second;
		}
	}
	return std::string();
}

bool W3DFloorDrawState::reactToTransformChange(int weather)
{
	// RW 0x004CEA6F: already added (+0x14) returns at RW 0x4CEA83; the flag is set at RW 0x4CEA8B before the module data test, so even
	// a module without data is marked. Without data nothing is handed over (RW 0x4CEA8F); the arguments at RW 0x4CEAA0..0x4CEAB4 are
	// the data's StaticModelLODMode (+0x1C), ForceToBack (+0x1D), the ModelName (+8) and the weather texture of RW 0x4CEA1C, and they
	// go to the terrain visual's floor list through RW 0x467B4D.
	if (m_registered)
	{
		return false;
	}
	m_registered = true; // RW 0x4CEA8B
	m_weather = weather;
	if (!m_floorData) // RW 0x4CEA8F
	{
		return false;
	}
	m_floorRecord.added = true;
	m_floorRecord.modelName = m_floorData->m_modelName;
	m_floorRecord.weatherTexture = resolveWeatherTexture(weather); // RW 0x4CEA9B -> RW 0x4CEA1C
	m_floorRecord.forceToBack = m_floorData->m_forceToBack;         // RW 0x4CEAA9
	m_floorRecord.staticModelLODMode = m_floorData->m_staticModelLODMode; // RW 0x4CEAA0
	return true;
}

bool W3DFloorDrawState::refreshRecord()
{
	// RW 0x004CEADF: with no module data or no drawable nothing happens (RW 0x4CEAF1 / RW 0x4CEAFB); the existing record is dropped
	// first (RW 0x4CEB11 -> RW 0x46AF77 -> RW 0x4E580B clears the record's drawable), then each hide flag returns (RW 0x4CEB1F /
	// RW 0x4CEB25 / RW 0x4CEB2B) and only a visible floor is handed over again (RW 0x4CEB60 -> RW 0x467B4D), with the weather texture
	// resolved once more (RW 0x4CEB3F -> RW 0x4CEA1C).
	if (!m_floorData)
	{
		m_floorRecord.added = false;
		return false;
	}
	m_floorRecord.added = false; // RW 0x4CEB11: the record is cleared before the flags are looked at
	if (!isFloorVisible())
	{
		return false;
	}
	m_floorRecord.added = true;
	m_floorRecord.modelName = m_floorData->m_modelName;
	m_floorRecord.weatherTexture = resolveWeatherTexture(m_weather);
	m_floorRecord.forceToBack = m_floorData->m_forceToBack;
	m_floorRecord.staticModelLODMode = m_floorData->m_staticModelLODMode;
	return true;
}

bool W3DFloorDrawState::replaceModelConditionState(const ModelConditionFlags &flags)
{
	// RW 0x004CEB80 (DONOR BFME2 0x000CF370): an empty HideIfModelConditions vector returns before anything (RW 0x4CEB86..0x4CEB8C),
	// then each entry is tested with RW 0x004CE8CA -- (flags & mask) == mask over 0x13 dwords, no "mask is empty" special case -- into
	// a byte, and only a changed value refreshes the record (RW 0x4CEBB3..0x4CEBBE).
	if (!m_floorData || m_floorData->m_hideIfModelConditions.empty())
	{
		return false;
	}
	bool hide = false;
	for (const ModelConditionFlags &mask : m_floorData->m_hideIfModelConditions) // RW 0x4CEB98..0x4CEBAD
	{
		if (flags.testForAll(mask))
		{
			hide = true;
			break;
		}
	}
	if (hide == m_hiddenByModelCondition)
	{
		return false; // RW 0x4CEBB3: no change, no refresh
	}
	m_hiddenByModelCondition = hide;
	refreshRecord(); // RW 0x4CEBBE -> RW 0x4CEADF
	return true;
}

void W3DFloorDrawState::setSubObjectHidden(bool hidden)
{
	// RW 0x004CECA1: compares with +0x15 and refreshes the record only on a change (RW 0x4CECA5..0x4CECAD)
	if (hidden == m_hiddenBySubObject)
	{
		return;
	}
	m_hiddenBySubObject = hidden;
	refreshRecord();
}

void W3DFloorDrawState::setHidden(bool hidden)
{
	// RW 0x004CECB5: compares with +0x16 and refreshes the record only on a change (RW 0x4CECB9..0x4CECC1)
	if (hidden == m_hiddenByModule)
	{
		return;
	}
	m_hiddenByModule = hidden;
	refreshRecord();
}

void W3DFloorDrawState::xfer(Xfer &x)
{
	// RW 0x004CE868 (DONOR BFME2 0x000CF074): version(1..3), calls W3DPropDraw::xfer (0x004B1B3F),
	// v>=2 xfers +0x16 (m_hiddenByModule), v>=3 xfers +0x17 (m_hiddenByModelCondition) and +0x15 (m_hiddenBySubObject).
	SnapshotXfer::version(x, 3, 3);
	const std::uint8_t v = 3;
	W3DPropDrawState::xfer(x);
	if (v >= 2)
	{
		x.xferBool(&m_hiddenByModule);
	}
	if (v >= 3)
	{
		x.xferBool(&m_hiddenByModelCondition);
		x.xferBool(&m_hiddenBySubObject);
	}
}
