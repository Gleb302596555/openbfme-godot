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
	// RW 0x004CE793 (DONOR BFME2 0x000CEF9F, W3DPropDrawReact.cpp)
	if (!m_propCreated && (posX != 0.0f || posY != 0.0f))
	{
		m_propCreated = true;
		return m_data != nullptr && !m_data->m_modelName.empty();
	}
	return false;
}

void W3DPropDrawState::xfer(Xfer &x)
{
	// RW 0x004B1B3F (DONOR BFME2 0x000B221A, W3DPropDrawXfer.cpp)
	SnapshotXfer::version(x, 1, 1);
}

W3DFloorDrawState::W3DFloorDrawState(const W3DFloorDrawModuleData *data)
	: W3DPropDrawState(data)
	, m_floorData(data)
	, m_hiddenBySubObject(data ? data->m_startHidden : false) // RW 0x004CE91B seeds +0x15 from data->m_startHidden (+0x1E)
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
	// RW 0x004CEA6F (DONOR BFME2 0x000CF25F)
	if (!m_registered)
	{
		m_registered = true;
		m_activeWeatherTexture = resolveWeatherTexture(weather);
		return m_floorData != nullptr && !m_floorData->m_modelName.empty();
	}
	return false;
}

bool W3DFloorDrawState::replaceModelConditionState(const ModelConditionFlags &flags)
{
	// RW 0x004CEB80 (DONOR BFME2 0x000CF370): tests each entry in m_hideIfModelConditions via RW 0x004CE8CA
	bool hide = false;
	if (m_floorData)
	{
		for (const ModelConditionFlags &mask : m_floorData->m_hideIfModelConditions)
		{
			if (mask.any() && flags.testForAll(mask))
			{
				hide = true;
				break;
			}
		}
	}
	const bool changed = (m_hiddenByModelCondition != hide);
	m_hiddenByModelCondition = hide;
	return changed;
}

void W3DFloorDrawState::setSubObjectHidden(bool hidden)
{
	// RW 0x004CECA1 (DONOR BFME2 0x000CF4AE): sets +0x15
	m_hiddenBySubObject = hidden;
}

void W3DFloorDrawState::setHidden(bool hidden)
{
	// RW 0x004CECB5 (DONOR BFME2 0x000CF4C2): sets +0x16
	m_hiddenByModule = hidden;
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
