// OpenBFME. GPL-3.0.
//
// Weapon nuggets: data, field tables and the registry. See GameLogic/WeaponNugget.h for the target facts. Lane WEAPON-1.

// Local 174: RW 0x90EFDB (WeaponNugget ??0Made002CC6AE@@QAE@XZ); RW 0x90F051 (WeaponNugget ??1Made002CC6AE@@UAE@XZ); RW 0x91193A (WeaponNugget ??0Made002CCB67@@QAE@XZ); RW 0x911973 (WeaponNugget ??1Made002CCB67@@UAE@XZ)
// Local 174: RW 0x911B39 (WeaponNugget ??0Made002CCBCA@@QAE@XZ); RW 0x911B78 (WeaponNugget ??1Made002CCBCA@@UAE@XZ); RW 0x90F130 (WeaponNugget ??0Made002CC711@@QAE@XZ); RW 0x90F74F (WeaponNugget ??0Made002CC774@@QAE@XZ)
// Local 174: RW 0x90FEF7 (WeaponNugget ??0Made002CC841@@QAE@XZ); RW 0x910D2F (WeaponNugget ??0Made002CC907@@QAE@XZ); RW 0x911143 (WeaponNugget ??0Made002CC9D4@@QAE@XZ); RW 0x911829 (WeaponNugget ??0Made002CCB04@@QAE@XZ)
// Local 174: RW 0x911DC5 (WeaponNugget ??0Made002CCC2D@@QAE@XZ); RW 0x911FFC (WeaponNugget ??0Made002CCC90@@QAE@XZ); RW 0x912292 (WeaponNugget ??1Made002CCCF3@@UAE@XZ); RW 0x90F0D6 (WeaponNugget 0xC7B328[9])
// Local 174: RW 0x90F7C9 (WeaponNugget 0xC7B410[9]); RW 0x912087 (WeaponNugget 0xC7BF68[5]); RW 0x90EA77 (WeaponNugget ?q4Notify002CC9D4@@YAXPAXPAVMade002CC9D4@@HH@Z); RW 0x90EAAC (WeaponNugget ?q4Notify002CC64B@@YAXPAXPAVMade002CC64B@@HH@Z)
// Local 174: RW 0x90EEF9 (WeaponNugget ?q4Notify002CC6AE@@YAXPAXPAVMade002CC6AE@@HH@Z); RW 0x90F089 (WeaponNugget ?q4Notify002CC6AE@@YAXPAXPAVMade002CC6AE@@HH@Z); RW 0x90F5A6 (WeaponNugget ?q4Notify002CC774@@YAXPAXPAVMade002CC774@@HH@Z); RW 0x90F791 (WeaponNugget no class)
// Local 174: RW 0x90F7DB (WeaponNugget ?q4Notify002CC7DE@@YAXPAXPAVMade002CC7DE@@HH@Z); RW 0x90FF79 (WeaponNugget ??1Rva00509D4C@@UAE@XZ); RW 0x910A45 (WeaponNugget ??1Rva0050A7AD@@UAE@XZ); RW 0x910A7D (WeaponNugget ?q4Notify002CC907@@YAXPAXPAVMade002CC907@@HH@Z)
// Local 174: RW 0x910D82 (WeaponNugget ?q4Notify002CC971@@YAXPAXPAVMade002CC971@@HH@Z); RW 0x911051 (WeaponNugget ?q4Notify002CC9D4@@YAXPAXPAVMade002CC9D4@@HH@Z); RW 0x9113D2 (WeaponNugget ?q4Notify002CC9D4@@YAXPAXPAVMade002CC9D4@@HH@Z); RW 0x9114C9 (WeaponNugget ?q4Notify002CCAA1@@YAXPAXPAVMade002CCAA1@@HH@Z)
// Local 174: RW 0x9116BA (WeaponNugget ?q4Notify002CCB04@@YAXPAXPAVMade002CCB04@@HH@Z); RW 0x91185A (WeaponNugget ?buildFieldParse@Rva0050B5CFBuildFieldParse@@SAXAAVMultiIniFieldParse@@@Z); RW 0x91187C (WeaponNugget ?q4Notify002CCB67@@YAXPAXPAVMade002CCB67@@HH@Z); RW 0x91197E (WeaponNugget ?q4Notify002CCB04@@YAXPAXPAVMade002CCB04@@HH@Z)
// Local 174: RW 0x911B7D (WeaponNugget ?q4Notify002CCB04@@YAXPAXPAVMade002CCB04@@HH@Z); RW 0x911E39 (WeaponNugget no class); RW 0x911EA2 (WeaponNugget ?q4Notify002CCC90@@YAXPAXPAVMade002CCC90@@HH@Z); RW 0x91204F (WeaponNugget no class)
// Local 174: RW 0x91209F (WeaponNugget ?q4Notify002CCCF3@@YAXPAXPAVMade002CCCF3@@HH@Z)
// Local 190: RW 0x90EA55 / RW 0x91102F / RW 0x91139F (WeaponNugget buildFieldParse helpers), RW 0x90EEC1 (~Made002CC64B WeaponNugget dtor)
#include "GameLogic/WeaponNugget.h"

#include "Common/AsciiString.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/WeaponSet.h"

#include <cstddef>
#include <cstdint>

namespace
{
template <typename T>
T *nuggetOf(void *instance)
{
	return static_cast<T *>(static_cast<WeaponNugget *>(instance));
}

// scanIndexList over a list, case-insensitive; an unknown name is NOT an error (RW 0x8E094D returns -1)
int indexOfNoCase(const char *token, const char *const *names)
{
	for (int i = 0; names[i]; ++i)
	{
		if (AsciiStringUtil::compareNoCase(token, names[i]) == 0)
		{
			return i;
		}
	}
	return -1;
}
}

// ---- base ------------------------------------------------------------------------------------------------------------------------
const FieldParse *WeaponNugget::getBaseFieldParse()
{
	static const FieldParse table[] = {
		{ "RequiredUpgradeNames", INI::parseAsciiStringVector, nullptr, (int)offsetof(WeaponNugget, m_requiredUpgradeNames) },
		{ "ForbiddenUpgradeNames", INI::parseAsciiStringVector, nullptr, (int)offsetof(WeaponNugget, m_forbiddenUpgradeNames) },
		{ "SpecialObjectFilter", ParseObjectFilter, nullptr, (int)offsetof(WeaponNugget, m_specialObjectFilter) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	return table;
}

WeaponNugget::WeaponNugget()
{
	// RW 0x90D901 / 0x763D11: the SpecialObjectFilter starts as the NONE filter (two empty KindOf masks)
	m_specialObjectFilter = ObjectFilter::none(KindOfMaskType{}, KindOfMaskType{});
}

MetaImpactNugget::MetaImpactNugget()
{
	m_killObjectFilter = ObjectFilter::none(KindOfMaskType{}, KindOfMaskType{}); // RW 0x9108D8
}

const char *WeaponNugget::keyword() const
{
	return WeaponNuggetInfoFor(kind()).keyword;
}

// RW 0x90E9E9: `DamageScalar = <percent> <object filter tokens>`
void ParseDamageScalar(INI *ini, void *instance, void *store, const void *)
{
	std::vector<DamageScalarEntry> *list = static_cast<std::vector<DamageScalarEntry> *>(store);
	DamageScalarEntry entry;
	entry.scalar = ini->scanPercentToReal(ini->getNextToken());
	ParseObjectFilter(ini, instance, &entry.filter, nullptr);
	list->push_back(entry);
}

// RW 0x90F933
void ParseWeaponLaunchBoneSlot(INI *ini, void *, void *store, const void *)
{
	*static_cast<int *>(store) = INI::scanIndexList(ini->getNextToken(), TheWeaponSlotTypeNames);
}

// RW 0x8E09CE: an unknown emotion name is -1, not an error
void ParseEmotionType(INI *ini, void *, void *store, const void *)
{
	*static_cast<int *>(store) = indexOfNoCase(ini->getNextToken(), TheEmotionTypeNames);
}

// RW 0x42ECB2 with userData 0: scanUnsignedInt, no cap
void ParseNuggetUnsignedInt(INI *ini, void *, void *store, const void *)
{
	*static_cast<unsigned *>(store) = ini->scanUnsignedInt(ini->getNextToken());
}

// RW 0x73A302 into a name slot; "None" stores no name
void ParseNuggetFXList(INI *ini, void *instance, void *store, const void *)
{
	WeaponNugget *nugget = static_cast<WeaponNugget *>(instance);
	const std::string name = ini->getNextToken();
	WeaponCheckFXList(nugget->m_references, name);
	*static_cast<std::string *>(store) = (AsciiStringUtil::compareNoCase(name, "None") == 0) ? std::string() : name;
}

// RW 0x42E558 into +0x14A; the retail constructor never initialises that byte (the port defaults it to false and records the line)
void ParseRemoveTargetFromOtherContain(INI *ini, void *instance, void *store, const void *)
{
	GrabNugget *n = nuggetOf<GrabNugget>(instance);
	*static_cast<bool *>(store) = ini->scanBool(ini->getNextToken());
	n->m_removeTargetFromOtherContainSet = true;
}

// ---- DamageNugget ------------------------------------------------------------------------------------------------------------------
DamageNugget::DamageNugget()
{
	m_forceKillObjectFilter = ObjectFilter::none(KindOfMaskType{}, KindOfMaskType{}); // RW 0x90DEDA: the NONE filter
}

const FieldParse *DamageNugget::getFieldParse()
{
	static const FieldParse table[] = {
		{ "Damage", INI::parseReal, nullptr, (int)offsetof(DamageNugget, m_damage) },
		{ "DamageTaperOff", INI::parseReal, nullptr, (int)offsetof(DamageNugget, m_damageTaperOff) },
		{ "AcceptDamageAdd", INI::parseBool, nullptr, (int)offsetof(DamageNugget, m_acceptDamageAdd) },
		{ "Radius", INI::parseReal, nullptr, (int)offsetof(DamageNugget, m_radius) },
		{ "MinRadius", INI::parseReal, nullptr, (int)offsetof(DamageNugget, m_minRadius) },
		{ "DamageArc", INI::parseAngleReal, nullptr, (int)offsetof(DamageNugget, m_damageArc) },
		{ "DamageArcInverted", INI::parseBool, nullptr, (int)offsetof(DamageNugget, m_damageArcInverted) },
		{ "DamageMaxHeight", INI::parseReal, nullptr, (int)offsetof(DamageNugget, m_damageMaxHeight) },
		{ "DamageMaxHeightAboveTerrain", INI::parseReal, nullptr, (int)offsetof(DamageNugget, m_damageMaxHeightAboveTerrain) },
		{ "DelayTime", INI::parseDurationUnsignedInt, nullptr, (int)offsetof(DamageNugget, m_delayTime) },
		{ "DamageType", INI::parseIndexList, TheDamageNames, (int)offsetof(DamageNugget, m_damageType) },
		{ "DeathType", INI::parseIndexList, TheWeaponDeathNames, (int)offsetof(DamageNugget, m_deathType) },
		{ "DamageFXType", INI::parseIndexList, TheDamageFXTypeNames, (int)offsetof(DamageNugget, m_damageFXType) },
		{ "DamageSubType", INI::parseIndexList, TheDamageSubTypeNames, (int)offsetof(DamageNugget, m_damageSubType) },
		{ "DamageScalar", ParseDamageScalar, nullptr, (int)offsetof(DamageNugget, m_damageScalar) },
		{ "DamageSpeed", INI::parseVelocityReal, nullptr, (int)offsetof(DamageNugget, m_damageSpeed) },
		{ "LostLeadershipUselessAgainst", ParseKindOfMask, nullptr, (int)offsetof(DamageNugget, m_lostLeadershipUselessAgainst) },
		{ "FlankingBonus", INI::parsePercentToReal, nullptr, (int)offsetof(DamageNugget, m_flankingBonus) },
		{ "FlankedScalar", INI::parsePercentToReal, nullptr, (int)offsetof(DamageNugget, m_flankedScalar) },
		{ "DrainLife", INI::parseBool, nullptr, (int)offsetof(DamageNugget, m_drainLife) },
		{ "DrainLifeMultiplier", INI::parseReal, nullptr, (int)offsetof(DamageNugget, m_drainLifeMultiplier) },
		{ "CylinderAOE", INI::parseBool, nullptr, (int)offsetof(DamageNugget, m_cylinderAOE) },
		{ "ForceKillObjectFilter", ParseObjectFilter, nullptr, (int)offsetof(DamageNugget, m_forceKillObjectFilter) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	return table;
}

// ---- the other nuggets -----------------------------------------------------------------------------------------------------------
const FieldParse *DamageFieldNugget::getFieldParse()
{
	static const FieldParse table[] = {
		{ "WeaponTemplateName", INI::parseAsciiString, nullptr, (int)offsetof(DamageFieldNugget, m_weaponTemplateName) },
		{ "Duration", INI::parseDurationUnsignedInt, nullptr, (int)offsetof(DamageFieldNugget, m_duration) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	return table;
}

const FieldParse *WeaponOCLNugget::getFieldParse()
{
	static const FieldParse table[] = {
		{ "WeaponOCLName", INI::parseAsciiString, nullptr, (int)offsetof(WeaponOCLNugget, m_weaponOCLName) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	return table;
}

const FieldParse *ProjectileNugget::getFieldParse()
{
	static const FieldParse table[] = {
		{ "WarheadTemplateName", INI::parseAsciiString, nullptr, (int)offsetof(ProjectileNugget, m_warheadTemplateName) },
		{ "ProjectileTemplateName", INI::parseAsciiString, nullptr, (int)offsetof(ProjectileNugget, m_projectileTemplateName) },
		{ "ProjectileStreamName", INI::parseAsciiString, nullptr, (int)offsetof(ProjectileNugget, m_projectileStreamName) },
		{ "WeaponLaunchBoneSlotOverride", ParseWeaponLaunchBoneSlot, nullptr, (int)offsetof(ProjectileNugget, m_weaponLaunchBoneSlotOverride) },
		{ "AlwaysAttackHereOffset", INI::parseCoord3D, nullptr, (int)offsetof(ProjectileNugget, m_alwaysAttackHereOffset) },
		{ "UseAlwaysAttackOffset", INI::parseBool, nullptr, (int)offsetof(ProjectileNugget, m_useAlwaysAttackOffset) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	return table;
}

const FieldParse *MetaImpactNugget::getFieldParse()
{
	static const FieldParse table[] = {
		{ "ShockWaveAmount", INI::parseVelocityReal, nullptr, (int)offsetof(MetaImpactNugget, m_shockWaveAmount) },
		{ "ShockWaveRadius", INI::parseReal, nullptr, (int)offsetof(MetaImpactNugget, m_shockWaveRadius) },
		{ "ShockWaveArc", INI::parseAngleReal, nullptr, (int)offsetof(MetaImpactNugget, m_shockWaveArc) },
		{ "ShockWaveArcInverted", INI::parseBool, nullptr, (int)offsetof(MetaImpactNugget, m_shockWaveArcInverted) },
		{ "ShockWaveTaperOff", INI::parseReal, nullptr, (int)offsetof(MetaImpactNugget, m_shockWaveTaperOff) },
		{ "ShockWaveSpeed", INI::parseVelocityReal, nullptr, (int)offsetof(MetaImpactNugget, m_shockWaveSpeed) },
		{ "ShockWaveZMult", INI::parseReal, nullptr, (int)offsetof(MetaImpactNugget, m_shockWaveZMult) },
		{ "DelayTime", INI::parseDurationUnsignedInt, nullptr, (int)offsetof(MetaImpactNugget, m_delayTime) },
		{ "InvertShockWave", INI::parseBool, nullptr, (int)offsetof(MetaImpactNugget, m_invertShockWave) },
		{ "FlipDirection", INI::parseBool, nullptr, (int)offsetof(MetaImpactNugget, m_flipDirection) },
		{ "HeroResist", INI::parseReal, nullptr, (int)offsetof(MetaImpactNugget, m_heroResist) },
		{ "OnlyWhenJustDied", INI::parseBool, nullptr, (int)offsetof(MetaImpactNugget, m_onlyWhenJustDied) },
		{ "CyclonicFactor", INI::parseReal, nullptr, (int)offsetof(MetaImpactNugget, m_cyclonicFactor) },
		{ "ShockWaveClearRadius", INI::parseBool, nullptr, (int)offsetof(MetaImpactNugget, m_shockWaveClearRadius) },
		{ "ShockWaveClearMult", INI::parseReal, nullptr, (int)offsetof(MetaImpactNugget, m_shockWaveClearMult) },
		{ "ShockWaveClearFlingHeight", INI::parseReal, nullptr, (int)offsetof(MetaImpactNugget, m_shockWaveClearFlingHeight) },
		{ "KillObjectFilter", ParseObjectFilter, nullptr, (int)offsetof(MetaImpactNugget, m_killObjectFilter) },
		{ "AffectHordes", INI::parseBool, nullptr, (int)offsetof(MetaImpactNugget, m_affectHordes) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	return table;
}

const FieldParse *HordeAttackNugget::getFieldParse()
{
	static const FieldParse table[] = {
		{ "ClosestMemberOnly", INI::parseBool, nullptr, (int)offsetof(HordeAttackNugget, m_closestMemberOnly) },
		{ "LockWeaponSlot", INI::parseLookupList, TheWeaponSlotLookup, (int)offsetof(HordeAttackNugget, m_lockWeaponSlot) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	return table;
}

const FieldParse *SpawnAndFadeNugget::getFieldParse()
{
	static const FieldParse table[] = {
		{ "ObjectTargetFilter", ParseObjectFilter, nullptr, (int)offsetof(SpawnAndFadeNugget, m_objectTargetFilter) },
		{ "SpawnedObjectName", INI::parseAsciiString, nullptr, (int)offsetof(SpawnAndFadeNugget, m_spawnedObjectName) },
		{ "SpawnOffset", INI::parseCoord3D, nullptr, (int)offsetof(SpawnAndFadeNugget, m_spawnOffset) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	return table;
}

const FieldParse *GrabNugget::getFieldParse()
{
	static const FieldParse table[] = {
		{ "ContainTargetOnEffect", INI::parseBool, nullptr, (int)offsetof(GrabNugget, m_containTargetOnEffect) },
		{ "ImpactTargetOnEffect", INI::parseBool, nullptr, (int)offsetof(GrabNugget, m_impactTargetOnEffect) },
		{ "RemoveTargetFromOtherContain", ParseRemoveTargetFromOtherContain, nullptr, (int)offsetof(GrabNugget, m_removeTargetFromOtherContain) },
		{ "ShockWaveAmount", INI::parseReal, nullptr, (int)offsetof(GrabNugget, m_shockWaveAmount) },
		{ "ShockWaveRadius", INI::parseReal, nullptr, (int)offsetof(GrabNugget, m_shockWaveRadius) },
		{ "ShockWaveTaperOff", INI::parseReal, nullptr, (int)offsetof(GrabNugget, m_shockWaveTaperOff) },
		{ "ShockWaveSpeed", INI::parseReal, nullptr, (int)offsetof(GrabNugget, m_shockWaveSpeed) },
		{ "ShockWaveZMult", INI::parseReal, nullptr, (int)offsetof(GrabNugget, m_shockWaveZMult) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	return table;
}

const FieldParse *AttributeModifierNugget::getFieldParse()
{
	static const FieldParse table[] = {
		{ "AttributeModifier", INI::parseAsciiString, nullptr, (int)offsetof(AttributeModifierNugget, m_attributeModifier) },
		{ "DamageFXType", INI::parseIndexList, TheDamageFXTypeNames, (int)offsetof(AttributeModifierNugget, m_damageFXType) },
		{ "Radius", INI::parseReal, nullptr, (int)offsetof(AttributeModifierNugget, m_radius) },
		{ "DamageArc", INI::parseAngleReal, nullptr, (int)offsetof(AttributeModifierNugget, m_damageArc) },
		{ "AntiCategories", INI::parseBitString32, TheAntiCategoryNames, (int)offsetof(AttributeModifierNugget, m_antiCategories) },
		{ "AntiFX", ParseNuggetFXList, nullptr, (int)offsetof(AttributeModifierNugget, m_antiFX) },
		{ "AffectHordeMembers", INI::parseBool, nullptr, (int)offsetof(AttributeModifierNugget, m_affectHordeMembers) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	return table;
}

const FieldParse *SpecialModelConditionNugget::getFieldParse()
{
	static const FieldParse table[] = {
		{ "ModelConditionNames", INI::parseAsciiStringVector, nullptr, (int)offsetof(SpecialModelConditionNugget, m_modelConditionNames) },
		{ "ModelConditionDuration", INI::parseDurationUnsignedInt, nullptr, (int)offsetof(SpecialModelConditionNugget, m_modelConditionDuration) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	return table;
}

const FieldParse *ParalyzeNugget::getFieldParse()
{
	static const FieldParse table[] = {
		{ "Radius", INI::parseReal, nullptr, (int)offsetof(ParalyzeNugget, m_radius) },
		{ "Duration", INI::parseDurationUnsignedInt, nullptr, (int)offsetof(ParalyzeNugget, m_duration) },
		{ "DamageArc", INI::parseAngleReal, nullptr, (int)offsetof(ParalyzeNugget, m_damageArc) },
		{ "ParalyzeFX", ParseNuggetFXList, nullptr, (int)offsetof(ParalyzeNugget, m_paralyzeFX) },
		{ "FreezeAnimation", INI::parseBool, nullptr, (int)offsetof(ParalyzeNugget, m_freezeAnimation) },
		{ "AffectHordeMembers", INI::parseBool, nullptr, (int)offsetof(ParalyzeNugget, m_affectHordeMembers) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	return table;
}

const FieldParse *LuaEventNugget::getFieldParse()
{
	static const FieldParse table[] = {
		{ "LuaEvent", INI::parseAsciiString, nullptr, (int)offsetof(LuaEventNugget, m_luaEvent) },
		{ "Radius", INI::parseReal, nullptr, (int)offsetof(LuaEventNugget, m_radius) },
		{ "SendToEnemies", INI::parseBool, nullptr, (int)offsetof(LuaEventNugget, m_sendToEnemies) },
		{ "SendToAllies", INI::parseBool, nullptr, (int)offsetof(LuaEventNugget, m_sendToAllies) },
		{ "SendToNeutral", INI::parseBool, nullptr, (int)offsetof(LuaEventNugget, m_sendToNeutral) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	return table;
}

const FieldParse *FireLogicNugget::getFieldParse()
{
	static const FieldParse table[] = {
		{ "LogicType", INI::parseIndexList, TheFireLogicTypeNames, (int)offsetof(FireLogicNugget, m_logicType) },
		{ "MinMaxBurnRate", INI::parseInt, nullptr, (int)offsetof(FireLogicNugget, m_minMaxBurnRate) },
		{ "MinDecay", INI::parseInt, nullptr, (int)offsetof(FireLogicNugget, m_minDecay) },
		{ "MaxResistance", INI::parseInt, nullptr, (int)offsetof(FireLogicNugget, m_maxResistance) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	return table;
}

const FieldParse *DamageContainedNugget::getFieldParse()
{
	static const FieldParse table[] = {
		{ "KillCount", INI::parseInt, nullptr, (int)offsetof(DamageContainedNugget, m_killCount) },
		{ "KillKindof", ParseKindOfMask, nullptr, (int)offsetof(DamageContainedNugget, m_killKindof) },
		{ "KillKindofNot", ParseKindOfMask, nullptr, (int)offsetof(DamageContainedNugget, m_killKindofNot) },
		{ "DeathType", INI::parseIndexList, TheWeaponDeathNames, (int)offsetof(DamageContainedNugget, m_deathType) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	return table;
}

const FieldParse *DOTNugget::getFieldParse()
{
	static const FieldParse table[] = {
		{ "DamageInterval", INI::parseDurationUnsignedInt, nullptr, (int)offsetof(DOTNugget, m_damageInterval) },
		{ "DamageDuration", INI::parseDurationUnsignedInt, nullptr, (int)offsetof(DOTNugget, m_damageDuration) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	return table;
}

const FieldParse *OpenGateNugget::getFieldParse()
{
	static const FieldParse table[] = {
		{ "Radius", INI::parseReal, nullptr, (int)offsetof(OpenGateNugget, m_radius) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	return table;
}

const FieldParse *EmotionWeaponNugget::getFieldParse()
{
	static const FieldParse table[] = {
		{ "EmotionType", ParseEmotionType, nullptr, (int)offsetof(EmotionWeaponNugget, m_emotionType) },
		{ "Radius", INI::parseReal, nullptr, (int)offsetof(EmotionWeaponNugget, m_radius) },
		{ "Duration", ParseNuggetUnsignedInt, nullptr, (int)offsetof(EmotionWeaponNugget, m_duration) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	return table;
}

const FieldParse *StealMoneyNugget::getFieldParse()
{
	static const FieldParse table[] = {
		{ "AmountStolenPerAttack", INI::parseReal, nullptr, (int)offsetof(StealMoneyNugget, m_amountStolenPerAttack) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	return table;
}

// ---- the registry ---------------------------------------------------------------------------------------------------------------------
namespace
{
template <typename T>
std::unique_ptr<WeaponNugget> make()
{
	return std::unique_ptr<WeaponNugget>(new T());
}

const FieldParse *emptyTable()
{
	// RW 0xC84858: SlaveAttackNugget's own table holds only the terminator
	static const FieldParse table[] = { { nullptr, nullptr, nullptr, 0 } };
	return table;
}

const std::vector<WeaponNuggetInfo> &registry()
{
	static const std::vector<WeaponNuggetInfo> infos = [] {
		const FieldParse *base = WeaponNugget::getBaseFieldParse();
		const FieldParse *damage = DamageNugget::getFieldParse();
		std::vector<WeaponNuggetInfo> v;
		auto add = [&v](WeaponNuggetKind kind, const char *keyword, std::unique_ptr<WeaponNugget> (*create)(), std::initializer_list<const FieldParse *> tables, int flag,
						unsigned rwFn) {
			WeaponNuggetInfo info = { kind, keyword, create, { nullptr, nullptr, nullptr, nullptr }, flag, rwFn };
			size_t i = 0;
			for (const FieldParse *t : tables)
			{
				info.tables[i++] = t;
			}
			v.push_back(info);
		};
		add(NUGGET_DAMAGE, "DamageNugget", make<DamageNugget>, { base, damage }, 0x114, 0x6cd301);
		add(NUGGET_DAMAGE_FIELD, "DamageFieldNugget", make<DamageFieldNugget>, { base, DamageFieldNugget::getFieldParse() }, 0x114, 0x6cd494); // RW 0x006CD494
		add(NUGGET_WEAPON_OCL, "WeaponOCLNugget", make<WeaponOCLNugget>, { base, WeaponOCLNugget::getFieldParse() }, 0, 0x6cd4fe); // RW 0x006CD4FE
		add(NUGGET_PROJECTILE, "ProjectileNugget", make<ProjectileNugget>, { base, ProjectileNugget::getFieldParse() }, 0, 0x6cd561); // RW 0x006CD561
		add(NUGGET_META_IMPACT, "MetaImpactNugget", make<MetaImpactNugget>, { base, MetaImpactNugget::getFieldParse() }, 0, 0x6cd5c4); // RW 0x006CD5C4
		add(NUGGET_HORDE_ATTACK, "HordeAttackNugget", make<HordeAttackNugget>, { base, HordeAttackNugget::getFieldParse() }, 0, 0x6cd8ea); // RW 0x006CD8EA
		add(NUGGET_SPAWN_AND_FADE, "SpawnAndFadeNugget", make<SpawnAndFadeNugget>, { base, SpawnAndFadeNugget::getFieldParse() }, 0, 0x6cd94d); // RW 0x006CD94D
		add(NUGGET_GRAB, "GrabNugget", make<GrabNugget>, { base, GrabNugget::getFieldParse() }, 0x157, 0x6cd627); // RW 0x006CD627
		add(NUGGET_ATTRIBUTE_MODIFIER, "AttributeModifierNugget", make<AttributeModifierNugget>, { base, AttributeModifierNugget::getFieldParse() }, 0, 0x6cd36b); // RW 0x006CD36B
		add(NUGGET_SPECIAL_MODEL_CONDITION, "SpecialModelConditionNugget", make<SpecialModelConditionNugget>, { base, SpecialModelConditionNugget::getFieldParse() }, 0, 0x6cd3ce); // RW 0x006CD3CE
		add(NUGGET_PARALYZE, "ParalyzeNugget", make<ParalyzeNugget>, { base, ParalyzeNugget::getFieldParse() }, 0, 0x6cd431); // RW 0x006CD431
		add(NUGGET_LUA_EVENT, "LuaEventNugget", make<LuaEventNugget>, { LuaEventNugget::getFieldParse() }, 0, 0x6cd9b0); // RW 0x006CD9B0
		add(NUGGET_FIRE_LOGIC, "FireLogicNugget", make<FireLogicNugget>, { base, damage, FireLogicNugget::getFieldParse() }, 0, 0x6cda13); // RW 0x006CDA13
		add(NUGGET_SLAVE_ATTACK, "SlaveAttackNugget", make<SlaveAttackNugget>, { base, emptyTable() }, 0, 0x6cd691); // RW 0x006CD691
		add(NUGGET_DAMAGE_CONTAINED, "DamageContainedNugget", make<DamageContainedNugget>, { base, DamageContainedNugget::getFieldParse() }, 0, 0x6cd6f4); // RW 0x006CD6F4
		add(NUGGET_DOT, "DOTNugget", make<DOTNugget>, { base, damage, DOTNugget::getFieldParse() }, 0x114, 0x6cd757); // RW 0x006CD757
		add(NUGGET_OPEN_GATE, "OpenGateNugget", make<OpenGateNugget>, { base, OpenGateNugget::getFieldParse() }, 0, 0x6cd7c1); // RW 0x006CD7C1
		add(NUGGET_EMOTION_WEAPON, "EmotionWeaponNugget", make<EmotionWeaponNugget>, { base, EmotionWeaponNugget::getFieldParse() }, 0, 0x6cd824); // RW 0x006CD824
		add(NUGGET_STEAL_MONEY, "StealMoneyNugget", make<StealMoneyNugget>, { base, StealMoneyNugget::getFieldParse() }, 0, 0x6cd887); // RW 0x006CD887
		return v;
	}();
	return infos;
}
}

const WeaponNuggetInfo &WeaponNuggetInfoFor(WeaponNuggetKind kind)
{
	return registry().at((size_t)kind);
}

const WeaponNuggetInfo *FindWeaponNuggetInfo(const char *keyword)
{
	for (const WeaponNuggetInfo &info : registry())
	{
		if (std::string(info.keyword) == keyword)
		{
			return &info;
		}
	}
	return nullptr;
}

// RW 0x6CD301 and siblings, minus the append and the flag byte (WeaponTemplate::parseNugget does those)
std::unique_ptr<WeaponNugget> ParseWeaponNugget(INI *ini, const WeaponNuggetInfo &info, const WeaponTemplate *owner)
{
	std::unique_ptr<WeaponNugget> nugget = info.create();
	nugget->m_owner = owner;
	MultiIniFieldParse multi;
	for (size_t i = 0; info.tables[i]; ++i)
	{
		multi.add(info.tables[i], 0);
	}
	ini->initFromINIMulti(static_cast<void *>(nugget.get()), multi);
	return nugget;
}
