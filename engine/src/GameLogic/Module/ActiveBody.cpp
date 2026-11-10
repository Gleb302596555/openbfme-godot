// OpenBFME. GPL-3.0.
// See GameLogic/Module/ActiveBody.h.

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif

#include "GameLogic/Module/ActiveBody.h"
#include "Common/Audio/AudioLog.h"
#include "Common/Audio/AudioRequests.h"
#include "GameLogic/UnitSpecificSound.h"
#include "GameLogic/Object/ExperienceTracker.h"
#include "GameLogic/Object/Contain/HordeFlank.h"
#include "GameLogic/SimMath.h"
#include "GameLogic/ExperienceWorld.h"
#include "GameLogic/Object/Object.h"

#include "Common/Player.h"
#include "Common/StateHash.h"
#include "GameLogic/SnapshotXfer.h"
#include "Common/Thing/ThingTemplate.h"
#include "GameLogic/Armor.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Combat/CombatState.h"
#include "GameLogic/ArmorSet.h"
#include "GameLogic/Damage.h"
#include "GameLogic/DamageFX.h"
#include "GameLogic/Module/DamageModule.h"
#include "Common/PlayerList.h"
#include "GameLogic/Module/BloodthirstyUpdate.h"
#include "GameLogic/Module/HeroModules.h"
#include "GameLogic/Module/MoneyEventModules.h"
#include "GameLogic/ObjectTemplateInfo.h"
#include "GameLogic/Object/ObjectGeometry.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/AI/AIWorld.h"
#include "GameLogic/AI/AIStateMachine.h"
#include "GameLogic/FXEvents.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/PhysicsBehavior.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/Object/PartitionManager.h"
#include "GameLogic/Module/EmotionModules.h"
#include "GameLogic/System/EmotionSystem.h"
#include "GameLogic/ObjectCreationList.h"
#include "GameLogic/Weapon.h"

#include <cstddef>
#include <cstdio>
#include <stdexcept>

namespace
{
// RW 0x73A302 (parseFXList): the token is stored; retail looks it up and rejects an unknown name other than None (the FXList store is
// not loaded: stop S-149)
void parseFXName(INI *ini, void *, void *store, const void *)
{
	*static_cast<std::string *>(store) = ini->getNextToken();
}

// PORT-45, RW 0x8C3F1F: one `DamageCreationList = <OCL> <FXTrigger> [<death side>]` line per record. The ObjectCreationList is resolved
// through the store at RW 0x8C3F37 -> RW 0x73A368 (RW 0x5EFFE7; a name the store lacks, NONE included, is NULL), the FXTrigger through
// scanIndexList (RW 0x42B999) over RW 0xDB2AF4 (NONE, CATAPULT_ROCK, TREBUCHET_ROCK) and the optional death side over RW 0xDB2B04
// (NONE, FRONT_DESTROYED, RIGHT_DESTROYED, BACK_DESTROYED, LEFT_DESTROYED), the last token 0 when absent (RW 0x8C3F68 / 0x8C3F7E).
// The 12-byte record goes to the data's vector (RW 0x8C3F85 `lea ecx,[ebx+0x58]` + push_back RW 0x6E0A42).
void parseDamageCreation(INI *ini, void *, void *store, const void *)
{
	std::vector<ActiveBodyModuleData::DamageCreation> *list = static_cast<std::vector<ActiveBodyModuleData::DamageCreation> *>(store);
	ActiveBodyModuleData::DamageCreation entry;
	const char *token = ini->getNextTokenOrNull(); // RW 0x8C3F2E .. 0x8C3F37: the OCL
	if (token)
	{
		// RW 0x5EFFE7: the store's list, NULL when it has none of that name (no error)
		entry.m_oclName = TheObjectCreationListStore && TheObjectCreationListStore->findObjectCreationList(token) ? token : std::string();
	}
	token = ini->getNextTokenOrNull(); // RW 0x8C3F42 after scanIndexList, the FXTrigger token
	entry.m_fxTrigger = token ? (unsigned)INI::scanIndexList(token, TheFXTriggerNames) : 0; // RW 0x8C3F47 .. 0x8C3F5B
	token = ini->getNextTokenOrNull();
	entry.m_deathSide = token ? (unsigned)INI::scanIndexList(token, TheDamageSideNames) : 0; // RW 0x8C3F63 .. 0x8C3F7C
	list->push_back(std::move(entry));
}

// lane OWNERFIX-1: HealingBuffFx (RW 0x73A302, + 0x48) replaces the GameData default the constructor took (RW 0x8C373F); "None" stores none
void parseHealingBuffFx(INI *ini, void *data, void *store, const void *)
{
	parseFXName(ini, data, store, nullptr);
	static_cast<ActiveBodyModuleData *>(data)->m_healingBuffFxSet = true;
}

#define AB_OFF(member) (int)offsetof(ActiveBodyModuleData, member)
const FieldParse kActiveBodyFieldParse[] = {
	{ "MaxHealth", INI::parseReal, nullptr, AB_OFF(m_maxHealth) },
	{ "MaxHealthDamaged", INI::parseReal, nullptr, AB_OFF(m_maxHealthDamaged) },
	{ "MaxHealthReallyDamaged", INI::parseReal, nullptr, AB_OFF(m_maxHealthReallyDamaged) },
	{ "InitialHealth", INI::parseReal, nullptr, AB_OFF(m_initialHealth) },
	{ "RecoveryTime", INI::parseDurationUnsignedInt, nullptr, AB_OFF(m_recoveryTime) },
	{ "DodgePercent", INI::parsePercentToReal, nullptr, AB_OFF(m_dodgePercent) },
	{ "EnteringDamagedTransitionTime", INI::parseDurationUnsignedInt, nullptr, AB_OFF(m_enteringDamagedTransitionTime) },
	{ "EnteringReallyDamagedTransitionTime", INI::parseDurationUnsignedInt, nullptr, AB_OFF(m_enteringReallyDamagedTransitionTime) },
	{ "GrabObject", INI::parseAsciiString, nullptr, AB_OFF(m_grabObject) },
	{ "GrabFX", parseFXName, nullptr, AB_OFF(m_grabFX) },
	{ "GrabDamage", INI::parseReal, nullptr, AB_OFF(m_grabDamage) },
	{ "GrabOffset", INI::parseCoord2D, nullptr, AB_OFF(m_grabOffset) },
	{ "UseDefaultDamageSettings", INI::parseBool, nullptr, AB_OFF(m_useDefaultDamageSettings) },
	{ "DamageCreationList", parseDamageCreation, nullptr, AB_OFF(m_damageCreationList) },
	{ "HealingBuffFx", parseHealingBuffFx, nullptr, AB_OFF(m_healingBuffFx) },
	{ "DamagedAttributeModifier", INI::parseAsciiString, nullptr, AB_OFF(m_damagedAttributeModifier) },
	{ "ReallyDamagedAttributeModifier", INI::parseAsciiString, nullptr, AB_OFF(m_reallyDamagedAttributeModifier) },
	{ "CheerRadius", INI::parseReal, nullptr, AB_OFF(m_cheerRadius) },
	{ "RemoveUpgradesOnDeath", INI::parseBool, nullptr, AB_OFF(m_removeUpgradesOnDeath) },
	{ "BurningDeathBehavior", INI::parseBool, nullptr, AB_OFF(m_burningDeathBehavior) },
	{ "BurningDeathFX", parseFXName, nullptr, AB_OFF(m_burningDeathFX) },
	{ nullptr, nullptr, nullptr, 0 }
};
#undef AB_OFF
} // namespace

void ActiveBodyModuleData::buildFieldParse(MultiIniFieldParse &p)
{
	p.add(kActiveBodyFieldParse);
}

// RW 0x8C3841
ActiveBody::ActiveBody(Thing *thing, const ActiveBodyModuleData *data)
	: BehaviorModule(thing, data)
	, m_data(data)
{
	m_initialHealth = data->m_initialHealth; // +0x1C = data + 0xC
	m_maxHealth = data->m_maxHealth;         // +0x20 = data + 0x8
	const float inverse = SimMath::divf32(1.0f, m_maxHealth); // RW 0x8C392B divss
	m_damagedFraction = SimMath::mulf32(data->m_maxHealthDamaged, inverse); // RW 0x8C3930 mulss
	m_reallyDamagedFraction = SimMath::mulf32(data->m_maxHealthReallyDamaged, inverse); // RW 0x8C393E mulss
	m_currentHealth = data->m_initialHealth == -1.0f ? data->m_maxHealth : data->m_initialHealth;
	m_previousHealth = m_currentHealth;
	m_reviveReference = m_currentHealth; // lane HERO-1: module + 0x2C starts at the actual initial health
	m_curDamageState = BODY_PRISTINE; // RW 0x8C3841 ends with setCorrectDamageState: its effects need the finished object, so applyInitialDamageState runs after Object::buildModules
	if (data->m_useDefaultDamageSettings)
	{
		const GameLogicSettings &settings = getObject()->logic().settings();
		if ((m_damagedFraction == 0.0f || m_reallyDamagedFraction == 0.0f) && !settings.bodyThresholdsLoaded)
		{
			throw std::logic_error("ActiveBody: the template uses the default damage thresholds but GameLogicSettings has none (GameData UnitDamagedThreshold / UnitReallyDamagedThreshold not loaded)");
		}
		if (m_damagedFraction == 0.0f)
		{
			m_damagedFraction = settings.unitDamagedThreshold;
		}
		if (m_reallyDamagedFraction == 0.0f)
		{
			m_reallyDamagedFraction = settings.unitReallyDamagedThreshold;
		}
	}
}

// RW 0x8C1B79
BodyDamageType ActiveBody::calcDamageState() const
{
	if (m_currentHealth == 0.0f)
	{
		return BODY_RUBBLE;
	}
	if (SimMath::mulf32(m_reallyDamagedFraction, m_maxHealth) >= m_currentHealth)
	{
		return BODY_REALLYDAMAGED;
	}
	if (SimMath::mulf32(m_damagedFraction, m_maxHealth) >= m_currentHealth)
	{
		return BODY_DAMAGED;
	}
	return BODY_PRISTINE;
}

// ZH ActiveBody::setInitialHealth (ActiveBody.cpp): the initial health is a percent of the max health; the current and previous health follow
// RW 0x8C1BF9 / BFME2 slot 9: slot 0x28 of body interface
void ActiveBody::resetDamageState(int state)
{
	if (state == 0)
	{
		m_currentHealth = m_maxHealth;
		m_curDamageState = BODY_PRISTINE;
	}
}

void ActiveBody::setInitialHealth(int initialPercent)
{
	m_initialHealth = SimMath::divf32(SimMath::mulf32(m_maxHealth, (float)initialPercent), 100.0f); // ZH order: (max * percent) / 100
	m_currentHealth = m_initialHealth;
	m_previousHealth = m_currentHealth;
	m_reviveReference = m_currentHealth;
	updateDamageState();
}

// lane SUMMON-1: RW 0x8C47DE (vt[0x58]) with its second argument 0. A percent above 100.0 (RW 0xBD88D8, comiss) first scales the max health (interface + 0x10)
// by percent * 0.01 (RW 0xBE5600; SSE: (max * percent) * 0.01); then the health moves to percent * 0.01 of the revive reference (interface + 0x1C) through
// internalChangeHealth (vt[0x84], RW 0x8C31A5) by (percent * 0.01) * reference - health. The second argument's branch (RW 0x8C481D: the health set with three
// random draws and RW 0x8C3CB8) is not a caller's here
void ActiveBody::setInitialHealthPercent(float percent)
{
	const float scale = 0.01f; // RW 0xBE5600
	if (percent > 100.0f)
	{
		m_maxHealth = SimMath::mulf32(SimMath::mulf32(m_maxHealth, percent), scale);
	}
	const float fraction = SimMath::mulf32(percent, scale);
	internalChangeHealth(SimMath::subf32(SimMath::mulf32(fraction, m_reviveReference), m_currentHealth));
}

void ActiveBody::crc(StateHasher &h) const
{
	h.addFloat(m_currentHealth);
	h.addFloat(m_previousHealth);
	h.addFloat(m_reviveReference); // lane HERO-1
	if (m_indestructible)
	{
		h.addU32(0x1D35u); // lane SCRIPT-2: hashed only when set, so a game that never sets it keeps its hash
	}
	h.addFloat(m_initialHealth);
	h.addFloat(m_maxHealth);
	h.addFloat(m_damagedFraction);
	h.addFloat(m_reallyDamagedFraction);
	h.addFloat(m_damageScalar);
	h.addU32(m_lastDamageFrame);
	h.addU32(m_lastDamager);
	if (m_lastAttackerTaken)
	{
		h.addU32(0xB6u); // lane PLAY-4: hashed only when set
	}
	if (m_lastDamagerPlayerMask != 0)
	{
		h.addU32(m_lastDamagerPlayerMask ^ 0xCA391u); // lane CAMP-1: hashed only when set (a game without player damage keeps its hash)
	}
	h.addU32((std::uint32_t)m_curDamageState);
	if (m_lastDamageType != DAMAGE_UNDEFINED)
	{
		h.addU32((std::uint32_t)m_lastDamageType ^ 0xFA40u); // lane FX-4: hashed only when a hit set it (a game without hits keeps its hash)
	}
	if (m_lastDamageNoEffect)
	{
		h.addU32(0xD178u); // Local 30, RW 0x744203: record +0x78; false leaves hash unchanged
	}
}

void ActiveBody::evaluateVisualCondition()
{
	// RW 0x8C1D53 -> Object RW 0x6903B6: the damage flags follow the stored body state, without the construction suppression of internalChangeHealth.
	// The ambient-sound / damage-stage tail remains S-653; this is the existing damage-flag mapping.
	Object &obj = *getObject();
	Object::ModelConditionBits clear{}, set{};
	const int damaged = CombatNames::modelCondition("DAMAGED"), really = CombatNames::modelCondition("REALLYDAMAGED"), rubble = CombatNames::modelCondition("RUBBLE");
	for (int bit : { damaged, really, rubble })
	{
		clear[(size_t)bit >> 5] |= 1u << (bit & 31);
	}
	const int on = m_curDamageState == BODY_DAMAGED ? damaged : m_curDamageState == BODY_REALLYDAMAGED ? really : m_curDamageState == BODY_RUBBLE ? rubble : -1;
	if (on >= 0)
	{
		set[(size_t)on >> 5] |= 1u << (on & 31);
	}
	obj.clearAndSetModelConditionFlags(clear, set);
}

// RW 0x8C2A5D (setCorrectDamageState). TARGET FACTS read from the binary: the damage state field (+0x30) is rewritten from calcDamageState (RW 0x8C1B79); when it changed vslot 0x34 runs
// (RW 0x8C283C: the Damaged / ReallyDamaged attribute modifiers, UPGRADE-1's). For a template that is KindOf STRUCTURE (tt KindOf mask byte 0x108 bit 0x80 = bit 7) the state RUBBLE does:
//   entering (new > old, new == 3): the structure's footprint leaves the pathfinder and is entered again (RW 0x6E85FB remove, 0x6E85E9 add: the add sees the RUBBLE state), the object
//   gets the status NO_COLLISIONS (RW 0x8C2E3B: 0x62684D(4, 1)), the geometry height becomes the template's StructureRubbleHeight (RW tt + 0x607, a byte; <= 0 takes GlobalData
//   DefaultStructureRubbleHeight, RW + 0xAE4) through RW 0x68B2CB, and RW 0x8C1BB7 runs (BFME2 0x4BDA67, decomp ActiveBodyDamageState.cpp:54-59,
//   tier B offsets-only): the object's ExperienceTracker (+ 0x26C) is reset (RW 0x79DA66), the upgrades go when RemoveUpgradesOnDeath
//   (data + 0x50 -> rva0028DA28), the command points are released (0x68E114 = removeFromPlayerCommandPoints) and the attribute modifier pool's
//   entries are cleared (rva00298E6A);
//   leaving it (new < 3, old == 3): the template geometry height returns (RW 0x8C2D87..0x8C2DAE -> 0xAD1920 + 0x68B2CB), the footprint is re-entered, NO_COLLISIONS is cleared (RW 0x8C2DB3..0x8C2DCC).
// Port 34 r2: the rubble heights of the two template classes are two branches of ONE block, as in RW. A WALK_ON_TOP_OF_WALL template (KindOf 60, byte 0x10F
// bit 0x10, RW 0x8C2A8C) branches off at RW 0x8C2A93:
//   entering RUBBLE (RW 0x8C2A99: [body + 0x30] == 3, the state just written) sets GlobalData DefaultStructureRubbleHeight (RW 0x8C2AB3..0x8C2AC4, 0x68B2CB)
//   ALWAYS — the template's StructureRubbleHeight byte is never read, and the body's own damage state field (+ 0x30) is left alone;
//   leaving it (RW 0x8C2A9F: edi, the old state, == 3) calls RW 0x699349 with the TEMPLATE's GeometryInfo (lea [tt + 0xA0]): the whole GeometryInfo becomes the
//   template's again (shape flags and heights too, the shortest way out of rubble).
// Only then does the common tail run (RW 0x8C2AC9: push the object into RW 0x6E85FB, the pathfind map; the two statuses) — not twice. Every other structure
// (RW 0x8C2A93 -> 0x8C2CCF) still takes the template's byte when > 0, else GlobalData, on enter (RW 0x8C2DDA..0x8C2E13) and the template's max height on exit
// (RW 0x8C2D87..0x8C2DAE).
// NOT PORTED (S-342): the Bookend machinery of walls — status BOOKENDING 0x53 and CAN_NOT_WALK_ON 0x4D with the body state forced to 0, the BookendUpdate
// module search at RW 0x8C2CEF..0x8C2D82, the StatusFX / StatusDamageFX blocks RW 0x8C2B07 / 0x8C2B28 / 0x8C2C04, the client draw-module search
// RW 0x8C2C8F..0x8C2CCA (S-142) and the attribute modifiers at RW 0x8C2C64..0x8C2EF1 — and the upgrade removal / attribute pool clear of RW 0x8C1BB7.
// The model conditions DAMAGED / REALLYDAMAGED / RUBBLE: internalChangeHealth calls the visual update (vslot 0x94, RW 0x8C32A8) after the state changed, not while the
// object is UNDER_CONSTRUCTION (unless dead). RW 0x8C1D53 -> Object RW 0x6903B6 sets the state's flag and clears the other two.
void ActiveBody::updateDamageState(bool initial)
{
	const BodyDamageType old = m_curDamageState;
	const BodyDamageType now = calcDamageState();
	m_curDamageState = now;
	if (now == old)
	{
		return;
	}
	Object &obj = *getObject();
	// lane PLAY-4, RW 0x8C3290 .. 0x8C32A8 (internalChangeHealth after vslot 0x54; BFME2 decomp ActiveBody.cpp, ZH "we just don't have all the art states for
	// that during buildup animation"): the visual change (vslot 0x94) is skipped while the object is UNDER_CONSTRUCTION, unless the health reached 0. A rising
	// structure (1 hit point at the start, RW 0x85895D) keeps its construction states instead of REALLYDAMAGED / DAMAGED.
	if (m_currentHealth <= 0.0f || !obj.testStatus((unsigned)CombatNames::statuses().underConstruction))
	{
		evaluateVisualCondition();
	}
	// lane FX-4: RotWK's setCorrectDamageState does not tell the damage modules (ZH's does); attemptDamage / attemptHealing do (notifyDamageStateChange)
	if (!obj.isKindOf((unsigned)CombatNames::kinds().structure))
	{
		return;
	}
	static const int kWalkOnTopOfWall = CombatNames::kindOf("WALK_ON_TOP_OF_WALL");
	const bool nonWallStructure = !obj.isKindOf((unsigned)kWalkOnTopOfWall);
	AIWorld *world = obj.logic().aiWorld();
	const bool mapReady = world && world->mapReady() && !initial;
	if (now == BODY_RUBBLE && old != BODY_RUBBLE)
	{
		// Port 34 r2, RW 0x8C2A99 -> 0x8C2AB3: a wall's rubble is GameData's height — the template's StructureRubbleHeight byte is never read
		const ThingTemplate *tt = obj.getTemplate();
		const std::int8_t tplRubble = tt ? tt->structureRubbleHeight() : 0;
		const float rubbleHeight = nonWallStructure && tplRubble > 0 ? static_cast<float>(tplRubble)
			: obj.logic().settings().defaultStructureRubbleHeight; // non-wall: RW 0x8C2DDA..0x8C2E13, the byte when > 0 else GameData + 0xAE4
		obj.setGeometryMaxHeight(rubbleHeight); // RW 0x68B2CB
		if (mapReady)
		{
			world->removeObjectFromPathfindMap(obj);
			world->addObjectToPathfindMap(obj);
		}
		// RW 0x8C1BB7: the experience reset, then the command points; RemoveUpgradesOnDeath and the attribute pool clear are S-342's
		if (ExperienceTracker *xp = obj.getExperienceTracker())
		{
			xp->reset(); // RW 0x79DA66 on object + 0x26C (BFME2 decomp ActiveBodyDamageState.cpp:54-59, B2 0x4BDA67)
		}
		obj.removeFromPlayerCommandPoints(); // RW 0x8C1BB7 -> 0x68E114
		obj.setStatus((unsigned)CombatNames::statuses().noCollisions, true); // RW 0x8C2E3B: after the rubble reset
		++obj.logic().combat().counters().rubbleEntered;
	}
	else if (old == BODY_RUBBLE && now != BODY_RUBBLE)
	{
		if (nonWallStructure)
		{
			// Port 34, RW 0x8C2D87..0x8C2DAE: (&tt->geometryInfo)->getMaxHeightAbovePosition() (RW 0xAD1920 on tt + 0xA0) -> obj->setGeometryMaxHeight (RW 0x68B2CB)
			if (const ThingTemplate *tt = obj.getTemplate())
			{
				const float restoredHeight = ObjectGeometry::maxHeightAbovePosition(ObjectGeometry::cachedShapesOf(*tt->getFinalOverride()));
				obj.setGeometryMaxHeight(restoredHeight);
			}
		}
		else if (const ThingTemplate *tt = obj.getTemplate())
		{
			// Port 34 r2, RW 0x8C2A9F..0x8C2AAC: a wall comes out of rubble as the template's whole GeometryInfo (RW 0x699349 on tt + 0xA0)
			obj.setGeometryInfoFrom(*tt->getFinalOverride());
		}
		if (mapReady)
		{
			world->removeObjectFromPathfindMap(obj);
			world->addObjectToPathfindMap(obj);
		}
		obj.setStatus((unsigned)CombatNames::statuses().noCollisions, false);
	}
}

// ---------------------------------------------------------------------------------------------------------------------------------
// lane COMBAT-1: the damage pipeline
// ---------------------------------------------------------------------------------------------------------------------------------
// RW 0x5D893C reads these four through the victim (scratch/weapon1/damage.md A5). The flank test (RW 0x5D89E6: the source object found by id, then
// Object::isFlankedBy RW 0x68FB63 on the victim) is lane HORDE-2's HordeFlank (S-582); an estimate (no victim / source pair) and a source that is gone answer
// "not flanked". Lane XP-1: INVULNERABLE and ARMOR are the victim's AttributeModifierPoolUpdate queries with the damage type's name (Damage.h: RW 0x68C818 ->
// 0x804F39); in the available binary both sites (RW 0x5D8A64, 0x5D8AF1) call into the community-added .danetta section, so the retail 2.01 form of these two
// queries is not verified (stop S-636, caveat S-001)
class ActiveBody::DamageHost : public AdjustDamageHost
{
public:
	explicit DamageHost(GameLogic &logic, Object *victim = nullptr, unsigned sourceId = 0)
		: m_logic(logic)
		, m_victim(victim)
		, m_sourceId(sourceId)
	{
	}
	bool victimFlankedByAttacker() override
	{
		Object *source = m_victim && m_sourceId != 0 ? m_logic.findObjectByID(m_sourceId) : nullptr;
		if (!source)
		{
			return false;
		}
		return HordeFlank::isFlankedBy(*m_victim, *source);
	}
	bool invulnerableTo(int damageType) override
	{
		float v = 0.0f;
		return m_victim && damageType >= 0 && damageType < DAMAGE_NUM_TYPES && m_victim->attributeModifierSum(ATTRIBUTE_MODIFIER_INVULNERABLE, TheDamageNames[damageType], v);
	}
	float armorModifierSum(int damageType) override
	{
		float v = 0.0f;
		if (m_victim && damageType >= 0 && damageType < DAMAGE_NUM_TYPES)
		{
			m_victim->attributeModifierSum(ATTRIBUTE_MODIFIER_ARMOR, TheDamageNames[damageType], v);
		}
		return v;
	}
	float armorMaxBonus() override { return m_logic.settings().attributeModifierArmorMaxBonus; } // GlobalData AttributeModifierArmorMaxBonus (RW +0xAE8, row RW 0xC00490, AdjustDamage RW 0x5D8B02..0x5D8B3D)

private:
	GameLogic &m_logic;
	Object *m_victim;
	unsigned m_sourceId;
};

const ArmorTemplate *ActiveBody::currentArmor() const
{
	const Object &obj = *getObject();
	const ThingTemplate *tt = obj.getTemplate();
	const ArmorTemplateSet *set = FindArmorTemplateSet(tt->armorTemplateSets(), obj.armorSetFlags());
	if (!set || set->m_armorName.empty())
	{
		return nullptr;
	}
	if (!TheArmorStore)
	{
		throw std::logic_error("ActiveBody: no ArmorStore is installed (the damage pipeline runs outside a world context)");
	}
	const ArmorTemplate *armor = TheArmorStore->findArmorTemplate(set->m_armorName);
	if (!armor && !m_armorReported)
	{
		m_armorReported = true;
		obj.logic().reportError("ActiveBody of " + tt->getName() + ": the armor '" + set->m_armorName + "' is not in the ArmorStore (RW looks the name up per hit and takes no armour)");
	}
	return armor;
}

float ActiveBody::estimateDamage(const DamageInfoInput &input) const
{
	DamageHost host(getObject()->logic(), getObject());
	const ArmorTemplate *armor = currentArmor();
	return AdjustDamage(armor, armor, input, true, host);
}

// RW 0x8C31A5
void ActiveBody::internalChangeHealth(float delta)
{
	BodyHealth h;
	h.health = m_currentHealth;
	h.maxHealth = m_maxHealth;
	h.previousHealth = m_previousHealth;
	InternalChangeHealth(h, delta);
	m_currentHealth = h.health;
	m_previousHealth = h.previousHealth;
	updateDamageState(); // RW 0x8C31A5 calls vslot 0x54 = setCorrectDamageState(false)
}

// RW 0x8C4AB4
void ImmortalBody::internalChangeHealth(float delta)
{
	BodyHealth h;
	h.health = m_currentHealth;
	h.maxHealth = getMaxHealth();
	h.previousHealth = m_previousHealth;
	ImmortalInternalChangeHealth(h, delta);
	m_currentHealth = h.health;
	m_previousHealth = h.previousHealth;
	updateDamageState();
}

// RW 0x8C49C7
void HighlanderBody::attemptDamage(DamageInfo &info)
{
	info.m_input.m_amount = HighlanderClampAmount(info.m_input.m_amount, getHealth(), info.m_input.m_damageType);
	ActiveBody::attemptDamage(info);
}

namespace
{
// lane SOUND-707, RW 0x8C4422 .. 0x8C44C1 (attemptDamage, inside the state-changed branch RW 0x8C438F, after the modules' slot 2 loop): with a drawable
// (RW 0x70E013) the object's SoundOnDamaged (row 0x2D, RW 0x8C22C0) when the new state is DAMAGED (1, RW 0x8C4439), SoundOnReallyDamaged (row 0x2E,
// RW 0x8C22D9) when REALLYDAMAGED (2, RW 0x8C4460), nothing for another state; an AudioEventRTS (RW 0x6DA95E) for the object's id (+ 0x74, RW 0x6DA6A7)
// added through TheAudio's slot 0x64. Any change into those states by a hit plays it (a hit that leaves RUBBLE does not; healing, RW 0x8C3139, never calls it).
// INFERENCE: the drawable test is taken as true (every retail logic object has a drawable); the row comes from the template, not through the draw
// module's sound selectors (RW 0x676A8A asks them first; S-701, as SoundMoveStart)
void playDamageStateSound(const Object &obj, BodyDamageType state)
{
	const char *row = state == BODY_DAMAGED ? "SoundOnDamaged" : state == BODY_REALLYDAMAGED ? "SoundOnReallyDamaged" : nullptr;
	const ThingTemplate *tt = obj.getTemplate();
	if (!row || !tt)
	{
		return;
	}
	const std::string sound = UnitSpecificSound::templateSound(*tt, row);
	if (!sound.empty())
	{
		AudioLog::Scope scope(row);
		AudioApi::playSoundForObject(sound, obj.getID());
	}
}
} // namespace

// RW 0x8C3FA3, in the order of scratch/weapon1/damage.md 2.7
void ActiveBody::attemptDamage(DamageInfo &info)
{
	Object &obj = *getObject();
	GameLogic &logic = obj.logic();
	const CombatNames::Status &st = CombatNames::statuses();
	const int type = info.m_input.m_damageType;
	// lane SCRIPT-2: RW 0x8C3FDA, before every other test: an indestructible body takes nothing
	if (m_indestructible)
	{
		return;
	}
	// INVUL-1: RW 0x8C3FE7 `push 0x14A; call 0x46E918` (Object::testModelCondition): model condition INVULNERABLE takes nothing, of any
	// damage type (ProductionUpdate's UnitInvulnerableTime, the castle unpack). Donor: Open-BFME-2 ActiveBody.cpp:2189 (BFME2 0x8BFE07, same
	// shape); both editions. The function's other retail differences stay open (S-322, ATTEMPTDMG-2)
	static const int kInvulnerable = CombatNames::modelCondition("INVULNERABLE"); // 0x14A
	if (obj.testModelCondition(kInvulnerable))
	{
		return;
	}
	// (3) UNATTACKABLE only takes UNRESISTABLE
	if (obj.testStatus((unsigned)st.unattackable) && type != DAMAGE_UNRESISTABLE)
	{
		return;
	}
	// (5)
	info.m_output.m_actualDamageDealt = 0.0f;
	info.m_output.m_actualDamageClipped = 0.0f;
	if (obj.isEffectivelyDead())
	{
		return;
	}
	// (6)
	DamageHost host(logic, &obj, info.m_input.m_sourceID);
	const ArmorTemplate *armor = currentArmor();
	float amount = AdjustDamage(armor, armor, info.m_input, false, host);
	if (type == DAMAGE_HEALING)
	{
		if (!info.m_input.m_kill)
		{
			attemptHealing(info);
		}
		return;
	}
	// (7) the body scalar (not for UNRESISTABLE), (9) the gate
	if (type != DAMAGE_UNRESISTABLE)
	{
		amount = SimMath::mulf32(amount, m_damageScalar);
	}
	if (Object *source = logic.findObjectByID(info.m_input.m_sourceID))
	{
		const unsigned bloodthirsty = (unsigned)CombatNames::status("BLOODTHIRSTY");
		Object *sourceHorde = source->getHordeObject(false);
		if (source->testStatus(bloodthirsty) && sourceHorde)
		{
			if (BloodthirstyUpdate *module = BloodthirstyUpdate::find(*sourceHorde); module && module->isValidSacrifice(&obj))
			{
				info.m_input.m_kill = true;
				module->addSacrifice(&obj);
			}
		}
	}
	const float previous = m_currentHealth;
	// TOP20 #21: the burning death's body half (BurningDeathBehavior + 0x51, BurningDeathFX + 0x54); its AI half is AIBurningDeathState
	bool clipped = false;                          // RW EBP - 0x11
	ContainModuleInterface *hordeHelper = nullptr; // RW EBP - 0x2C
	if (amount > 0.0f || info.m_input.m_kill)
	{
		// (10) RW 0x8C4107 .. 0x8C411F: a contained object's container's horde interface (its contain + 0x258, slot 0x7C); RW 0x8C4169 .. 0x8C4214: the cap
		Object *outer = obj.getContainedBy();
		if (outer)
		{
			ContainModuleInterface *c = outer->getContain();
			hordeHelper = c && c->getHordeContainInterface() ? c : nullptr;
		}
		if (burningDeathCaps(obj, type, outer, hordeHelper))
		{
			// RW 0x8C41DF .. 0x8C4214: health (vslot 0x10, x87) - 1.0f (RW 0xBD1908) stored as a float; the smaller of it and the amount; a change marks the hit
			const float limit = SimMath::subf32(getHealth(), 1.0f);
			const float capped = limit > amount ? amount : limit; // fcompi; ja keeps the amount
			if (capped != amount)                                  // ucomiss; lahf; test ah, 0x44; jnp
			{
				amount = capped;
				clipped = true;
			}
		}
		// (11) kill takes the whole health (RW 0x8C4218, after the cap: a capped kill stays capped and does not die, as RW)
		if (info.m_input.m_kill)
		{
			amount = m_currentHealth;
		}
		// (13) internalChangeHealth(-amount) (the info goes down with it in RW: DelayedDeathBody's override reads and clears its kill flag)
		const BodyDamageType stateBefore = m_curDamageState; // lane FX-4: RW 0x8C3FA3 keeps + 0x30 (EBP - 0x1C) before the health change (vslot 0x84, RW 0x8C4257)
		m_currentInfo = &info;
		internalChangeHealth(SimMath::subf32(0.0f, amount));
		m_currentInfo = nullptr;
		scheduleRecovery(); // RW 0x8C425D .. 0x8C4276: recovery runs before output amounts and record selection
		// (15)
		info.m_output.m_actualDamageDealt = amount;
		info.m_output.m_actualDamageClipped = SimMath::subf32(m_previousHealth, m_currentHealth);
		// (16)
		// Local 30, RW 0x8C4294 .. 0x8C42F5: a record from this or the previous frame is retained unless the incoming source exists and either the
		// previous source is gone or the incoming source is INFANTRY, CAVALRY, or one of RW 0x68DAE6's five significant-building kinds
		// (Object::isFactionStructure: FS_POWER, FS_FACTORY, FS_BASE_DEFENSE, FS_TECHNOLOGY, FS_CASH_PRODUCER).
		// The subtraction is unsigned, including frame zero (0 - 1 == 0xFFFFFFFF == initial frame). All parts of the record are replaced together.
		const UnsignedInt now = logic.getFrame();
		bool replaceRecord = m_lastDamageFrame != now && m_lastDamageFrame != now - 1u;
		if (!replaceRecord)
		{
			const Object *previousSource = logic.findObjectByID(m_lastDamager);
			const Object *incomingSource = logic.findObjectByID(info.m_input.m_sourceID);
			if (incomingSource)
			{
				replaceRecord = !previousSource || incomingSource->isKindOfName("INFANTRY") || incomingSource->isKindOfName("CAVALRY") ||
				                incomingSource->isKindOfName("FS_POWER") || incomingSource->isKindOfName("FS_FACTORY") ||
				                incomingSource->isKindOfName("FS_BASE_DEFENSE") || incomingSource->isKindOfName("FS_TECHNOLOGY") ||
				                incomingSource->isKindOfName("FS_CASH_PRODUCER");
			}
		}
		if (replaceRecord)
		{
			m_lastDamageFrame = now;
			m_lastDamager = info.m_input.m_sourceID;
			m_lastDamagerPlayerMask = info.m_input.m_sourcePlayerMask; // lane CAMP-1: record + 0xC
			m_lastDamageType = info.m_input.m_damageType;               // lane FX-4: record + 0x10
			m_lastDamageNoEffect = info.m_output.m_noEffect;           // RW 0x744203: record + 0x78
			m_lastAttackerTaken = false;                               // RW 0x8C42FA: record + 0xB6 cleared with the record
		}
		// lane CAMP-1, RW 0x8C430F .. 0x8C4341: the last damage record's source object, when it exists, marks its controlling player as an attacker of the
		// victim's controlling player (RW 0x6AAC05). Runs after the record block whether or not the record changed this hit (reads m_lastDamager).
		if (const Object *source = logic.findObjectByID(m_lastDamager))
		{
			Player *victimPlayer = obj.getControllingPlayer();
			const Player *sourcePlayer = source->getControllingPlayer();
			if (victimPlayer && sourcePlayer)
			{
				victimPlayer->setAttackedBy(sourcePlayer->getPlayerIndex(), logic.getFrame());
			}
		}
		++logic.combat().counters().damageApplications;
		if (const CombatState::DamageTrace &trace = logic.combat().damageTrace())
		{
			trace(obj, info.m_input.m_sourceID, info.m_output.m_actualDamageDealt, info.m_output.m_actualDamageClipped); // lane SHIP-1: a test observer
		}
		// ZH: the damage modules hear of the hit (RW 0x79B757 CastleMemberBehavior::onDamage reads the amount dealt)
		if (info.m_output.m_actualDamageDealt > 0.0f)
		{
			for (const std::unique_ptr<BehaviorModule> &m : obj.modules())
			{
				if (DamageModuleInterface *d = m->getDamage())
				{
					d->onDamage(info);
				}
			}
			// P-19, RW 0x8C4378 .. 0x8C4384 -> RW 0x68B8A3: the source object's modules hear of the hit they landed (behaviour slot 0x14: PillageModule, RW 0x88826E)
			if (Object *source = logic.findObjectByID(info.m_input.m_sourceID))
			{
				for (const std::unique_ptr<BehaviorModule> &m : source->modules())
				{
					if (m->getModuleClassName() == "PillageModule")
					{
						static_cast<PillageModule *>(m.get())->onDamageDealt(obj);
					}
				}
			}
		}
		// lane FX-4, RW 0x8C4389 .. 0x8C43C1: the damage state changed by this hit: every damage module's slot 2 (info, old, new), after the record copy and the
		// onDamage loop, before the death
		if (m_curDamageState != stateBefore)
		{
			notifyDamageStateChange(&info, stateBefore, m_curDamageState);
			playDamageStateSound(obj, m_curDamageState);
		}
		// (18) death: health <= 0 and the hit took health; TOP20 #21, RW 0x8C458A .. 0x8C45C0: a capped hit scores the kill too (RW 0x6955BC(obj, 1)) and
		// skips the death (the unit dies when its burning death ends)
		if (clipped || (m_currentHealth <= 0.0f && previous > 0.0f))
		{
			if (Object *killer = logic.findObjectByID(info.m_input.m_sourceID))
			{
				killer->scoreTheKill(obj);
			}
			// RW 0x8C4593: RespawnBody owns the death callback when RespawnUpdate exists.
			if (!clipped && !dynamic_cast<RespawnUpdate *>(obj.findModule("RespawnUpdate")))
			{
				// TOP20 #20, RW 0x8C460C .. 0x8C463E, read before the death: the victim counts as contained only in a container (Object + 0x27C) whose
				// contain module (+ 0x258) holds two or more (vslot 0x114(0) >= 2)
				bool contained = false;
				if (Object *outer = obj.getContainedBy())
				{
					ContainModuleInterface *c = outer->getContain();
					contained = !c || c->getContainCount() >= 2;
				}
				DieModuleInterface::Event event;
				event.sourceId = info.m_input.m_sourceID;
				event.deathType = info.m_input.m_deathType;
				event.damageType = type;
				event.damageSubType = info.m_input.m_damageSubType; // lane HERO-2
				event.damageAmount = info.m_input.m_amount;
				event.actualDamageDealt = info.m_output.m_actualDamageDealt;
				event.actualDamageClipped = info.m_output.m_actualDamageClipped;
				obj.friend_onDie(event);
				// TOP20 #20, RW 0x8C4679 .. 0x8C46A6: after the death (and the linked object's, RW 0x8C464A), a victim that was not contained, killed by a
				// source (DamageInfo + 8), and neither PROJECTILE nor INERT makes its enemies around cheer (primary vslot 0x58)
				if (!contained && info.m_input.m_sourceID != INVALID_ID && !isCheerExempt(obj))
				{
					cheer();
				}
			}
		}
		// lane XP-1: RW 0x8C46A9 .. 0x8C46EA: the attacker's player earns skill points for the share of the victim's maximum health the hit took (capped at
		// 1.0), after the kill credit and the death (ExperienceWorld::awardSkillPointsForDamage)
		if (Object *attacker = logic.findObjectByID(info.m_input.m_sourceID))
		{
			float fraction = SimMath::divf32(info.m_output.m_actualDamageClipped, m_maxHealth);
			if (fraction > 1.0f)
			{
				fraction = 1.0f;
			}
			if (fraction > 0.0f)
			{
				logic.experience().awardSkillPointsForDamage(*attacker, obj, fraction);
			}
		}
	}
	// (19) doDamageFX (RW 0x8C2F02, lane FX-2)
	doDamageFX(info);
	if (clipped)
	{
		startBurningDeath(hordeHelper);
	}
}

// PORT-2, RW 0x8C425D .. 0x8C4276: the body's RecoveryTime, read after the health change and before the record. `mov eax,[module data]; mov
// eax,[eax+0x24]` (the row RW 0xC71DA8, +0x24) → `test eax,eax; jbe` skips a zero value → `TheGameLogic + 0x40` is the current frame (RW 0x8C426D,
// the same counter the record below stores at RW 0x8C4294 .. 0x8C4309) → the deadline `frame + RecoveryTime` in one 32-bit `add` → `call 0x9F3A3C`,
// which is `ret 4`, the image's shared empty function. Retail computes the deadline on every damaging hit that reached the health change (not on a
// healing hit, not on a hit a branch above refused) and the callee throws it away; nothing stores it, so nothing here does either. The count the
// hook keeps is a test observer only and is not hashed or saved (there is no state).
void ActiveBody::scheduleRecovery()
{
	const UnsignedInt recoveryTime = m_data->m_recoveryTime; // RW 0x8C4260: module data + 0x24
	if (recoveryTime == 0)                                   // RW 0x8C4263 test / jbe
	{
		return;
	}
	const UnsignedInt deadline = getObject()->logic().getFrame() + recoveryTime; // RW 0x8C4267 .. 0x8C4270: [TheGameLogic + 0x40] + RecoveryTime
	// RW 0x8C4273 .. 0x8C4276 hands `deadline` to the object and returns; RW 0x9F3A3C reads nothing. The two statics are the test observer of that
	// arithmetic (like bodyMaxHealthCallsIgnored): they are never hashed and never saved, because retail has no state here
	lastRecoveryDeadline() = deadline;
	++recoverySchedules();
}

// TOP20 #21, RW 0x8C4169 .. 0x8C41DD: the cap applies to an object with an AI (+ 0x260) whose body data has BurningDeathBehavior (+ 0x51), none of the statuses
// of RW 0xDE9B14 (built by RW 0x85189C(0, 0x3A, 0x3D, 0x5A): INSIDE_GARRISON, ENCLOSED, IS_LEAVING_FACTORY; test RW 0x75CDC4 on Object + 0x94), none of the
// model conditions of RW 0xDE9B28 (RW 0x665074(0, 0x81, 0x67, 0x69): STONED, CLIMBING, RAPPELLING; test RW 0x6632E9 on Object + 0x10C), a hit of type 6 (FLAME)
// or 0x17 (LOGICAL_FIRE), no stunned physics (+ 0x264, its + 0x5C), and either no container or a container with a horde interface
bool ActiveBody::burningDeathCaps(const Object &obj, int damageType, const Object *container, const ContainModuleInterface *hordeHelper) const
{
	if (!obj.getAIUpdateInterface() || !m_data->m_burningDeathBehavior)
	{
		return false;
	}
	static const unsigned kInsideGarrison = (unsigned)CombatNames::status("INSIDE_GARRISON");      // 0x3A
	static const unsigned kEnclosed = (unsigned)CombatNames::status("ENCLOSED");                    // 0x3D
	static const unsigned kLeavingFactory = (unsigned)CombatNames::status("IS_LEAVING_FACTORY");     // 0x5A
	if (obj.testStatus(kInsideGarrison) || obj.testStatus(kEnclosed) || obj.testStatus(kLeavingFactory))
	{
		return false;
	}
	static const int kStoned = CombatNames::modelCondition("STONED");         // 0x81
	static const int kClimbing = CombatNames::modelCondition("CLIMBING");     // 0x67
	static const int kRappelling = CombatNames::modelCondition("RAPPELLING"); // 0x69
	if (obj.testModelCondition(kStoned) || obj.testModelCondition(kClimbing) || obj.testModelCondition(kRappelling))
	{
		return false;
	}
	if (damageType != DAMAGE_FLAME && damageType != DAMAGE_LOGICAL_FIRE) // RW 0x8C41B8 / 0x8C41BD: 6, 0x17
	{
		return false;
	}
	if (const PhysicsBehavior *phys = obj.physicsModule(); phys && phys->isStunned())
	{
		return false; // RW 0x8C41C2 .. 0x8C41D3
	}
	return !container || hordeHelper; // RW 0x8C41D5 .. 0x8C41DD
}

// TOP20 #21, RW 0x8C476C .. 0x8C47BD (after doDamageFX and the repulsor status): a capped hit lets the object out of its horde (the horde interface's slot 0x38,
// RW 0x87393E), leaves a locked temporary state (RW 0x663082 -> 0x66309C), makes AI command 0x53 from CMD_FROM_AI (RW 0x8C3EC3(2): AIBurningDeathState) and plays
// BurningDeathFX (+ 0x54) on the object with no other object (FXList::doFXObj RW 0x4B1B5A(fx, obj, 0))
void ActiveBody::startBurningDeath(ContainModuleInterface *hordeHelper)
{
	Object &obj = *getObject();
	if (hordeHelper)
	{
		hordeHelper->removeFromContain(&obj); // RW 0x8C4779 .. 0x8C477C
	}
	if (AIUpdateInterface *ai = obj.getAIUpdateInterface())
	{
		if (ai->stateMachineOrNull() && ai->stateMachineOrNull()->temporaryStateLocked())
		{
			ai->emotionLeaveAIState(); // RW 0x8C477F .. 0x8C4792
		}
		ai->aiBurningDeath(CMD_FROM_AI); // RW 0x8C4797 .. 0x8C479E
	}
	const std::string &fx = m_data->m_burningDeathFX;
	if (FXEventLog::isFXName(fx)) // RW 0x8C47AA .. 0x8C47AF: none ("None" or unset) plays nothing
	{
		GameLogic &logic = obj.logic();
		logic.fxEvents().emit(FXEventLog::objectEvent(FXEvent::OBJECT_FX, "BurningDeathFX", logic.getFrame(), fx, obj));
	}
}

// TOP20 #20: the template exemption both callers of the cheer test (RW 0x8C1E27 .. 0x8C1E3D, 0x8C4686 .. 0x8C469F): ThingTemplate KindOf + 0x108 bit 25
// (PROJECTILE) or + 0x110 bit 25 (KindOf 89, INERT)
bool ActiveBody::isCheerExempt(const Object &obj)
{
	static const int kProjectile = ObjectTemplateInfoBuilder::kindOfIndex("PROJECTILE"); // 25
	static const int kInert = ObjectTemplateInfoBuilder::kindOfIndex("INERT");           // 89
	return obj.isKindOf((unsigned)kProjectile) || obj.isKindOf((unsigned)kInert);
}

// TOP20 #20, RW 0x8C3B89 (ActiveBody primary vslot 0x58, inherited by every body vtable that holds it, RW 0xC71BD0 ..): with CheerRadius (module data
// + 0x4C) above 0, every object ThePartitionManager finds within it (RW 0xA39340: FROM_CENTER_2D, ITER_FASTEST) through the filters RW 0xC11DC0(self, 1, 0)
// (the object's relationship to it is ENEMIES), RW 0xC10E20 (alive: + 0x458 bit 0 clear) and RW 0xC0F374(self) (the same + 0x458 bit 3: never set in the
// port) is asked for EMOTION_CHEER with this object as the source for 10 frames (RW 0x68F37F(1, self, 0xA): the request of the outermost container's
// EmotionTrackerUpdate). Not ported: the debug circle drawn when GlobalData + 0xE9C is set (RW 0x8C3BAF .. 0x8C3C07, TheTacticalView slot 0x30).
void ActiveBody::cheer()
{
	const float radius = m_data->m_cheerRadius;
	if (!(radius > 0.0f)) // RW 0x8C3B9D: comiss 0, radius; jae: 0 >= radius returns
	{
		return;
	}
	Object *self = getObject();
	PartitionFilterFn filters([self](Object &o) { return self->getRelationship(o) == ENEMIES && !o.isEffectivelyDead(); });
	const PartitionHits hits = self->logic().partition().iterateObjectsInRange(*self->getPosition(), radius, FROM_CENTER_2D, { &filters }, ITER_FASTEST);
	for (const PartitionHit &hit : hits)
	{
		EmotionTrackerUpdate::requestEmotion(*hit.object, EMOTION_CHEER, self, 10); // RW 0x8C3C85 .. 0x8C3C8C
	}
}

// PORT-66 (issue #180): RW 0x8C1E4E, the body interface's slot 0x2C. The branches of RW 0x8C1E4E, read linearly:
//   * the seven templates are looked up once (static guards 0xDE9B04 bits 1, 2, 4, 8, 0x10, 0x20, 0x40; RW 0x5F889B findTemplate with the GlobalData
//     pointers +0x9F4 fire small, +0xA00 fire medium, +0xA0C fire large, +0xA18 smoke small, +0xA24 smoke medium, +0xA30 smoke large, +0xA3C aflame;
//     the cached pointers 0xDE9B00, 0xDE9AFC, 0xDE9AF8, 0xDE9AF4, 0xDE9AF0, 0xDE9AEC, 0xDE9AE8);
//   * RW 0x8C1FD3: the object's status 0xA (RW 0x44DDEC, `OBJECT_STATUS_AFLAME`) picks the entry templates: aflame -> fire small = fire medium,
//     fire medium = fire large, fire large = fire large, smoke small / medium / large = fire small, count modifier 2 (RW 0x8C1FEF .. 0x8C2000);
//     else the plain templates and the modifier 1 (RW 0x8C2002 .. 0x8C202A);
//   * RW 0x8C2036: deleteAllParticleSystems, the body's primary vslot 0x50 (RW 0x8C258F);
//   * then, in this order, createParticleSystems (primary vslot 0x4C, RW 0x8C23EA) for the Prefix +0x9F0 / +0x9FC / +0xA08 / +0xA14 / +0xA20 / +0xA2C with
//     the swapped template and the cap Auto*Max * the modifier (RW 0x8C203D .. 0x8C20F2, the imul at each);
//   * RW 0x8C20F5: the status test again; aflame -> one more call for the Prefix +0xA38, the aflame template and AutoAflameParticleMax * 2 (RW 0x8C2107
//     .. 0x8C2126).
// createParticleSystems (RW 0x8C23EA) asks the object for the numbered bones of the prefix (RW 0x68C650 getMultiLogicalBonePosition with start index 1,
// at most 16; a NULL template answers nothing at RW 0x8C2401), caps the count at the bones found, then per system draws a CLIENT random bone (RW 0x6D32E4,
// an unused bone index each) and makes createParticleSystem (RW 0x5F526A), setPosition at the bone (RW 0x5F2F2D), attachToObject (RW 0x5F32D7) and keeps
// the ID on the body's list (+0xC8). The bones and the systems are the client's, as FX-4's TransitionDamageFX systems are (S-340/S-345 called them the
// client's): the logic reads the bones (template data and logic state, the launch bone provider) and emits one event the client plays.
void ActiveBody::updateBodyParticleSystems()
{
	Object *obj = getObject();
	const GameLogicSettings &set = obj->logic().settings();
	// RW 0x8C1FD6: status 0xA of the object (TheObjectStatusNames[10] AFLAME, the same status FlammableUpdate sets on ignition)
	const bool aflame = obj->testStatus(10);
	const GameLogicSettings::AutoBodyParticles *fire[3] = { &set.autoFireSmall, &set.autoFireMedium, &set.autoFireLarge };
	const GameLogicSettings::AutoBodyParticles *smoke[3] = { &set.autoSmokeSmall, &set.autoSmokeMedium, &set.autoSmokeLarge };
	// the swapped template of each entry (RW 0x8C1FDD .. 0x8C202F), the aflame mapping above
	const std::string *systems[7];
	systems[0] = &(aflame ? set.autoFireMedium.system : set.autoFireSmall.system);
	systems[1] = &(aflame ? set.autoFireLarge.system : set.autoFireMedium.system);
	systems[2] = &set.autoFireLarge.system;
	systems[3] = &(aflame ? set.autoFireSmall.system : set.autoSmokeSmall.system);
	systems[4] = &(aflame ? set.autoFireSmall.system : set.autoSmokeMedium.system);
	systems[5] = &(aflame ? set.autoFireSmall.system : set.autoSmokeLarge.system);
	systems[6] = &set.autoAflame.system;
	const GameLogicSettings::AutoBodyParticles *entries[7] = { fire[0], fire[1], fire[2], smoke[0], smoke[1], smoke[2], &set.autoAflame };
	const int countModifier = aflame ? 2 : 1; // RW 0x8C1FF7 / 0x8C202F
	static const std::string kNoName; // the event carries its template names in particleSystems, not an FXList name
	FXEvent e = FXEventLog::objectEvent(FXEvent::BODY_PARTICLE_SYSTEMS, "BodyParticles", obj->logic().getFrame(), kNoName, *obj);
	// the count of the calls: six, then the seventh only while aflame (RW 0x8C20F5)
	const int calls = aflame ? 7 : 6;
	ProjectileLaunchOffsets *provider = obj->logic().combat().launchOffsets();
	e.particleSystems.clear();
	for (int i = 0; i < calls; ++i)
	{
		FXEvent::BodyParticleEntry entry;
		entry.system = *systems[i];
		entry.maxSystems = entries[i]->maxSystems * countModifier;
		// RW 0x8C23EA: no template (NULL in retail) -> nothing at all, not even the bone query
		if (!entry.system.empty() && entry.maxSystems > 0)
		{
			float bones[16][12];
			const int n = provider ? provider->multiLogicalBones(*obj, entries[i]->prefix, obj->getModelConditionBits(), 16, bones) : 0;
			for (int b = 0; b < n; ++b)
			{
				FXEvent::BodyParticleBone bone;
				char name[300];
				std::snprintf(name, sizeof(name), "%s%02d", entries[i]->prefix.c_str(), b + 1); // RW 0x672A73: prefix + 01, 02, ...
				bone.name = name;
				bone.position = Coord3D{ bones[b][3], bones[b][7], bones[b][11] }; // the 3x4 bone matrix's translation, model space
				entry.bones.push_back(std::move(bone));
			}
		}
		e.particleSystems.push_back(std::move(entry));
	}
	obj->logic().fxEvents().emit(e);
}

// TOP20 #20, RW 0x8C1E18 (ActiveBody primary vslot 8, onDelete): an object deleted while not effectively dead (+ 0x458 bit 0) and neither PROJECTILE nor
// INERT cheers its enemies (vslot 0x58), then the body's particle systems go (vslot 0x50; the port keeps none on the body)
void ActiveBody::onDelete()
{
	Object *obj = getObject();
	if (!obj->isEffectivelyDead() && !isCheerExempt(*obj))
	{
		cheer();
	}
}

// RW 0x8C2F02 doDamageFX (lane FX-2). The body's DamageFX (+0xFC) is the one of the armour set chosen for the object's armour flags (the ArmorSet's DamageFX
// field, resolved by name in TheDamageFXStore; RW keeps the pointer of the set in use); none: no FX. A hit of the type played last (+0x3C) while the frame is
// below +0x38 is throttled. Else RW 0x76256A: the FXList is getDamageFX(type, actual damage dealt, source) (RW 0x76226B: row 0, amount 0 gives none,
// amount >= AmountForMajorFX the major list) and plays as doFXObj(fx, this object, the source) through RW 0x4B1B5A; when one played, +0x3C = the type and
// +0x38 = now + ThrottleTime(type) (RW 0x76225D, row 0).
// PORT-45: RW 0x8C2F75 .. 0x8C2FB8 after that block: the body's copy of the DamageCreationList (body + 0xE0, filled from module data + 0x58 by the
// constructor RW 0x8C3841) is walked entry by entry (12 bytes); an entry whose +0x04 equals the hit's DamageInfo +0x2C (input + 0x28, the weapon's
// FXTrigger), whose +0x08 is 0 and whose +0x00 is not NULL creates its ObjectCreationList on this object (RW 0x5F0126, primary = the object, secondary
// position 0, lifetime 0). The block is not part of the FX branch: every return of it lands on RW 0x8C2F75, so a body without DamageFX, a throttled hit
// and an empty FX row all still create their lists.
void ActiveBody::doDamageFX(const DamageInfo &info)
{
	Object &obj = *getObject();
	GameLogic &logic = obj.logic();
	// RW 0x8C2F75: the damage-creation loop below is reached from every path of the FX block (it starts after the `je` the missing
	// DamageFX / throttled hit takes), so it runs for every hit doDamageFX is called with
	const auto createDamageCreations = [&]() {
		if (!TheObjectCreationListStore || m_data->m_damageCreationList.empty())
		{
			return; // the names were resolved against the store at parse time (RW 0x73A368); no store, no list
		}
		for (const ActiveBodyModuleData::DamageCreation &entry : m_data->m_damageCreationList) // RW 0x8C2F7D .. 0x8C2FB8
		{
			if (entry.m_fxTrigger != (unsigned)info.m_input.m_fxTrigger) // RW 0x8C2F88 cmp [record+4], [DamageInfo+0x2C]
			{
				continue;
			}
			if (entry.m_deathSide != 0) // RW 0x8C2F92 / 0x8C2F95: only the NONE side here
			{
				continue;
			}
			const ObjectCreationList *ocl = entry.m_oclName.empty() ? nullptr : TheObjectCreationListStore->findObjectCreationList(entry.m_oclName); // RW 0x8C2F97
			if (!ocl)
			{
				continue;
			}
			ocl->create(logic, &obj, *obj.getPosition(), nullptr, 0); // RW 0x8C2FA4 -> RW 0x5F0126 with the object as the primary
		}
	};
	const ThingTemplate *tt = obj.getTemplate();
	const ArmorTemplateSet *set = tt ? FindArmorTemplateSet(tt->armorTemplateSets(), obj.armorSetFlags()) : nullptr;
	const DamageFX *dfx = set && TheDamageFXStore && FXEventLog::isFXName(set->m_damageFXName) ? TheDamageFXStore->findDamageFX(set->m_damageFXName) : nullptr;
	const int type = info.m_input.m_damageFXOverride;
	if (!dfx || type < 0 || type >= DAMAGEFX_TYPE_COUNT) // RW 0x8C2F1E / 0x8C2F3F: no DamageFX, an unknown name or type goes to the loop
	{
		createDamageCreations();
		return;
	}
	const UnsignedInt now = logic.getFrame();
	if (type == m_lastDamageFXType && now < m_damageFXThrottleUntil) // RW 0x8C2F20 .. 0x8C2F34: the throttled hit goes to the loop
	{
		createDamageCreations();
		return;
	}
	const std::string &fx = dfx->getDamageFX(type, info.m_output.m_actualDamageDealt);
	if (FXEventLog::isFXName(fx)) // RW 0x8C2F53 / 0x8C2F59: a type whose row is empty still falls through to the loop
	{
		FXEvent e = FXEventLog::objectEvent(FXEvent::OBJECT_FX, "DamageFX", now, fx, obj);
		e.secondary = logic.findObjectByID(info.m_input.m_sourceID) ? info.m_input.m_sourceID : (ObjectID)INVALID_ID;
		logic.fxEvents().emit(e);
		m_lastDamageFXType = type;
		m_damageFXThrottleUntil = now + dfx->getThrottleTime(type);
	}
	createDamageCreations();
}

// lane XP-1: RW 0x8C1CD5 setMaxHealth: the maximum and the initial health become newMax; type 1 changes the health by (health / oldMax) * newMax - health,
// type 2 by newMax - oldMax (SSE single, through internalChangeHealth); then a health above the new maximum comes down to it
void ActiveBody::setMaxHealth(float newMax, int changeType)
{
	const float oldMax = m_maxHealth;
	m_maxHealth = newMax;
	m_initialHealth = newMax;
	m_reviveReference = newMax; // lane HERO-1: RW 0x8C1CD5 stores newMax at module + 0x2C, the revive's reference
	if (changeType == 1)
	{
		internalChangeHealth(SimMath::subf32(SimMath::mulf32(SimMath::divf32(m_currentHealth, oldMax), newMax), m_currentHealth));
	}
	else if (changeType == 2)
	{
		internalChangeHealth(SimMath::subf32(newMax, oldMax));
	}
	if (m_currentHealth > newMax)
	{
		internalChangeHealth(SimMath::subf32(newMax, m_currentHealth));
	}
}

// lane FX-4: the loops RW 0x8C4395 (attemptDamage) / RW 0x8C3139 (attemptHealing) run over the object's modules in list order: each module's DAMAGE
// interface (module slot 0x10) slot 2, onBodyDamageStateChange(info, old, new). setCorrectDamageState (RW 0x8C2A5D) makes no such call: a health change by
// any other path (construction, repair, scripts, the maximum health) tells no damage module
void ActiveBody::notifyDamageStateChange(const DamageInfo *info, BodyDamageType oldState, BodyDamageType newState)
{
	for (const std::unique_ptr<BehaviorModule> &m : getObject()->modules())
	{
		if (DamageModuleInterface *d = m->getDamage())
		{
			d->onBodyDamageStateChange(info, oldState, newState);
		}
	}
}

namespace
{
// the dead-but-kept test of RW 0x8C2FC1 (BFME2 0x4BEE21, decomp ActiveBody.cpp attemptHealing, tier B same-shape): the template's
// KeepSelectableWhenDead (+ 0x642, read as PlayerCommands does) or its KindOf WALL_UPGRADE (150, tt + 0x11A bit 6), BRIDGE (22) or
// BRIDGE_TOWER (24) (tt + 0x10A & 0x140)
bool healableWhenDead(const Object &obj)
{
	static const int kWallUpgrade = ObjectTemplateInfoBuilder::kindOfIndex("WALL_UPGRADE");
	static const int kBridge = ObjectTemplateInfoBuilder::kindOfIndex("BRIDGE");
	static const int kBridgeTower = ObjectTemplateInfoBuilder::kindOfIndex("BRIDGE_TOWER");
	if ((kWallUpgrade >= 0 && obj.isKindOf((unsigned)kWallUpgrade)) || (kBridge >= 0 && obj.isKindOf((unsigned)kBridge)) ||
		(kBridgeTower >= 0 && obj.isKindOf((unsigned)kBridgeTower)))
	{
		return true;
	}
	if (const ThingTemplate *tt = static_cast<const ThingTemplate *>(obj.getTemplate()))
	{
		if (const FieldValue *v = tt->getFinalOverride()->findField("KeepSelectableWhenDead"))
		{
			if (const bool *b = std::get_if<bool>(v))
			{
				return *b;
			}
		}
	}
	return false;
}
} // namespace

// RW 0x8C2FC1 attemptHealing (BFME2 0x4BEE21, decomp ActiveBody.cpp, tier B same-shape). RotWK order: validateArmorAndDamageFX (the armour
// validation is S-322's); a body whose module data +0x51 (BurningDeathBehavior) is set while the object is in the model condition
// BURNINGDEATH (index 0x220, the reading SpawnBehavior shares) returns at once; a null DamageInfo returns (ours is a reference); a
// non-HEALING hit goes to attemptDamage; an effectively dead object (Object + 0x458 bit 0) is refused unless healableWhenDead answers
// (KeepSelectableWhenDead / WALL_UPGRADE / BRIDGE / BRIDGE_TOWER). Only then are the outputs zeroed and the amount applied
void ActiveBody::attemptHealing(DamageInfo &info)
{
	Object *obj = getObject();
	static const int kBurningDeath = CombatNames::modelCondition("BURNINGDEATH"); // model condition 0x220
	if (m_data->m_burningDeathBehavior && obj->testModelCondition(kBurningDeath))
	{
		return;
	}
	if (info.m_input.m_damageType != DAMAGE_HEALING)
	{
		attemptDamage(info);
		return;
	}
	if (obj->isEffectivelyDead() && !healableWhenDead(*obj))
	{
		return;
	}
	info.m_output.m_actualDamageDealt = 0.0f;
	info.m_output.m_actualDamageClipped = 0.0f;
	const float amount = info.m_input.m_amount; // adjustDamage returns the amount for HEALING
	if (amount > 0.0f)
	{
		const BodyDamageType stateBefore = m_curDamageState; // lane FX-4: RW 0x8C3080 keeps + 0x30 before the health change (vslot 0x84, RW 0x8C308A)
		internalChangeHealth(amount);
		info.m_output.m_actualDamageDealt = amount;
		info.m_output.m_actualDamageClipped = SimMath::subf32(m_previousHealth, m_currentHealth);
		// lane OWNERFIX-1 (the owner: no particles show the heroes healing): RW 0x8C30B1 .. 0x8C3118 (BFME2 decomp ActiveBody.cpp attemptHealing, tier B
		// same-shape): when the health went up (after the damage modules' onHealing, which this port does not call here), the module data's healing FX (+ 0x48: HealingBuffFx, else GameData's
		// DefaultUnitHealingBuffFxList, RW 0x8C373F) plays on the object (RW 0x4B1B5A, no secondary) unless its template is a STRUCTURE (template + 0x108
		// bit 7) or its status has AIRBORNE_TARGET (Object + 0x94 bit 6). A client effect: the logic's FX log, nothing hashed changes
		if (m_currentHealth > m_previousHealth)
		{
			Object &obj = *getObject();
			GameLogic &logic = obj.logic();
			const std::string &fx = m_data->m_healingBuffFxSet ? m_data->m_healingBuffFx : logic.settings().defaultUnitHealingBuffFxList;
			static const int kStructure = CombatNames::kindOf("STRUCTURE");
			static const int kAirborneTarget = CombatNames::status("AIRBORNE_TARGET");
			if (FXEventLog::isFXName(fx) && !obj.isKindOf((unsigned)kStructure) && !obj.testStatus((unsigned)kAirborneTarget))
			{
				logic.fxEvents().emit(FXEventLog::objectEvent(FXEvent::OBJECT_FX, "HealingBuffFx", logic.getFrame(), fx, obj));
			}
		}
		// lane FX-4, RW 0x8C3131 .. : the damage state changed by the healing: every damage module's slot 2 (info, old, new)
		if (m_curDamageState != stateBefore)
		{
			notifyDamageStateChange(&info, stateBefore, m_curDamageState);
		}
	}
}

// ---- lane SAVE-4b: the snapshot (OpenBFME native, S-4190) -----------------------------------------------------------------------------------------------
// Retail's ActiveBody::xfer (RW 0x8C3388, tier A, (1, 1)) transfers RotWK's body (six reals, the damage and side-destroyed types, the particle systems, the
// subdual and armour bits ...); this body is the port's subset: its fields in the crc's order, then what the crc leaves out (the damage FX throttle). The
// current DamageInfo pointer is null outside a hit (a snapshot is taken between frames); the armour report flag is the report's.
void ActiveBody::xfer(Xfer &x)
{
	XferVersion v{ 1, 3 }; // 2: lane FX-4's last damage type, 3: Local 30's last damage no-effect
	x.xferVersion(v);
	xferBehaviorModule(x);
	x.xferReal(&m_currentHealth);
	x.xferReal(&m_previousHealth);
	x.xferReal(&m_reviveReference);
	x.xferBool(&m_indestructible);
	x.xferReal(&m_initialHealth);
	x.xferReal(&m_maxHealth);
	x.xferReal(&m_damagedFraction);
	x.xferReal(&m_reallyDamagedFraction);
	x.xferReal(&m_damageScalar);
	x.xferUnsignedInt(&m_lastDamageFrame);
	x.xferObjectID(&m_lastDamager);
	SnapshotXfer::maskWithin(x, m_lastDamagerPlayerMask, SnapshotXfer::lowBits(getObject()->logic().players().getPlayerCount()), "a body's last damager players",
		"this game's players"); // lane SAVE-4e
	x.xferBool(&m_lastAttackerTaken); // lane PLAY-4's + 0xB6
	SnapshotXfer::enumIn(x, m_curDamageState, BODY_PRISTINE, BODY_RUBBLE, "a body's damage state");
	SnapshotXfer::enumIn(x, m_lastDamageFXType, 0, SnapshotXfer::nameCount(TheDamageFXTypeNames) - 1, "a body's last damage FX type");
	x.xferUnsignedInt(&m_damageFXThrottleUntil);
	if (v.current >= 2)
	{
		SnapshotXfer::enumIn(x, m_lastDamageType, 0, SnapshotXfer::nameCount(TheDamageNames) - 1, "a body's last damage type");
	}
	else
	{
		m_lastDamageType = DAMAGE_UNDEFINED;
	}
	if (v.current >= 3)
	{
		x.xferBool(&m_lastDamageNoEffect); // RW 0x744203: DamageInfo + 0x78
	}
	else
	{
		m_lastDamageNoEffect = false;
	}
}

void ImmortalBody::xfer(Xfer &x)
{
	SnapshotXfer::version(x, 1, 1);
	ActiveBody::xfer(x);
}

void HighlanderBody::xfer(Xfer &x)
{
	SnapshotXfer::version(x, 1, 1);
	ActiveBody::xfer(x);
}
