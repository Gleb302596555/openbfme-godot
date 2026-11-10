// OpenBFME. GPL-3.0.
// Lane HORDE-2: a horde's banner carrier and the replenishment through it. See GameLogic/Module/BannerCarrierUpdate.h.
//
// TARGET FACTS (RotWK game.dat, caveat S-001; read with Ghidra):
//   * HordeContain::update calls RW 0x8719E4(force = 0) every update (RW 0x8730A5; also from the contained state RW 0x871A6F): with an AI, the countdown H+0x27C is
//     decremented when not 0; at 0, with no carrier (H+0x26C) and BannerCarrierMinLevel (data + 0x27C, a byte) < the horde's veterancy level (object + 0x26C -> + 0x24), and
//     the horde not busy (RW 0x8B50A5 on object + 0x254, not ported): unless forced, nothing while frame - 4 * LOGICFRAMES_PER_SECOND <= the horde's last shot frame (firing
//     tracker + 0x44, RW 0x68B645); else RW 0x870204;
//   * RW 0x870204: with no carrier and BannerCarriersAllowed (data + 0x218) not empty, the first name's template is made on the horde's team (the original team for a
//     TEMPORARILY_DEFECTED horde) with the horde's transform; a horde under construction with a producer's exit interface hands it to the exit, else it stands at the
//     horde's position; it joins the horde (HordeContainInterface slot 0x74) and the countdown becomes its BannerCarrierUpdate's MeleeFreeBannerReSpawnTime (data + 0x14);
//   * BannerCarrierUpdate::update RW 0x89ACE4 and the member spawn RW 0x89A392 / 0x873AE3: see BannerCarrierUpdate.h.
// INFERENCE (stop S-587): the carrier's place in the formation is its BannerCarrierPosition entry (UnitType = the payload template, else the first entry), rotated like a
// slot; the carrier id is cleared by RW 0x871915 (lane BANNER-1: a removal, not a garrison release); the copied upgrades / weapon set flags (RW 0x68DEEC / 0x691059 with 0x18 .. 0x1A), the experience
// sharing (RW 0x79DC9E) and the producer's exit are not ported. The rank is XP-1's ExperienceTracker rank (lane INTEG-1); BannerCarrierMinLevel defaults to 1 (2.01
// hordes without the field get a carrier at rank 2), the Angmar hordes set 0 (a carrier at rank 1, from the start) or 6. A horde's rank gain calls bannerCheck(1)
// (RW 0x873ACF, HordeContainExperience.cpp).

#include "GameLogic/Module/BannerCarrierUpdate.h"
#include "GameLogic/SnapshotXfer.h"

#include "Common/AsciiString.h"
#include "Common/GameCommon.h"
#include "Common/Player.h"
#include "Common/ScoreKeeper.h"
#include "Common/StateHash.h"
#include "Common/Thing/ModuleFactory.h"
#include "Common/Thing/ThingFactory.h"
#include "Common/Thing/ThingTemplate.h"
#include "GameLogic/Combat/ObjectWeapons.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/FXEvents.h"
#include "GameLogic/Module/ActiveBody.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/EmotionModules.h"
#include "GameLogic/Module/UpgradeModule.h"
#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Object/Contain/HordeContainBehaviorData.h"
#include "GameLogic/Object/Contain/HordeContainCore.h"
#include "GameLogic/Object/Contain/HordeContainRuntime.h"
#include "GameLogic/Module/StructureModules.h"
#include "GameLogic/Object/AttributeModifierPool.h"
#include "GameLogic/Object/ExperienceTracker.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/Object/PartitionManager.h"
#include "GameLogic/SimMath.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <stdexcept>

namespace
{
const char *const kStop =
	"[S-587] banner carriers: the spawn (RW 0x8719E4 / 0x870204: countdown, BannerCarrierMinLevel below the veterancy level, no shot for 4 seconds, BannerCarriersAllowed[0], "
	"the countdown from MeleeFreeBannerReSpawnTime; the rank is XP-1's ExperienceTracker rank, and a horde's rank gain forces the check, RW 0x873ACF) and BannerCarrierUpdate's "
	"replenishment (RW 0x89ACE4: the emotion gate RW 0x8B50A5, the container's last shot and its horde's attackedWithin RW 0x86EE52 over MeleeFreeUnitSpawnTime; "
	"RW 0x89A392: capacity, AllowBannerSpawnUpgrade RW 0x89A308, the score keeper off, UnitSpawnFX; RW 0x873AE3: the carrier's matrix, weapon set flags 0x18 .. 0x1A, "
	"the template's experience pool, the attribute modifiers, the lifetime and the formation modifiers), the DISABLED_HELD update (RW 0x855830) and the carrier's "
	"removal RW 0x871915 (DiedRespawnTime; not on a garrison release) are ported, as is nearby replenishment (RW 0x89AB8F: allied "
	"hordes in distance order, capacity, one/all, shroud/UnitSpawnFX, expedited banner countdown); NOT ported: RW 0x8719E4's RW 0x8B50A5, the carrier's copied "
	"upgrades / experience, the producer's exit, RW 0x871915's H+0x264 branch (BannerCarrierDestroyHordeOnDeath is ported), RW 0x873AE3's defection hand-over, RW 0x625E0A, interface slots 0x174 / 0x10 and behavior links, the ExpLevelDraw grammar (raw token; the "
	"morph side is lane PORT-75: MorphCondition RW 0x89A90F parsed and applied by RW 0x89A5C5, BannerMorphFX + 0x30 played from the contain's accept path); INFERENCE: the carrier stands at its BannerCarrierPosition, a carrier lost on its way into a garrison is cleared as by RW 0x871915";

HordeContain *hordeContainOf(Object *o)
{
	if (!o)
	{
		return nullptr;
	}
	ContainModuleInterface *c = o->getContain();
	return c && c->getHordeContainInterface() ? dynamic_cast<HordeContain *>(c) : nullptr;
}

const FieldParse kBannerParse[] = {
	{ "IdleSpawnRate", INI::parseDurationUnsignedInt, nullptr, (int)offsetof(BannerCarrierUpdateModuleData, m_idleSpawnRate) },
	{ "MeleeFreeUnitSpawnTime", INI::parseDurationUnsignedInt, nullptr, (int)offsetof(BannerCarrierUpdateModuleData, m_meleeFreeUnitSpawnTime) },
	{ "DiedRespawnTime", INI::parseDurationUnsignedInt, nullptr, (int)offsetof(BannerCarrierUpdateModuleData, m_diedRespawnTime) },
	{ "MeleeFreeBannerReSpawnTime", INI::parseDurationUnsignedInt, nullptr, (int)offsetof(BannerCarrierUpdateModuleData, m_meleeFreeBannerReSpawnTime) },
	{ "MorphCondition", BannerCarrierUpdateModuleData::parseMorphCondition, nullptr, (int)offsetof(BannerCarrierUpdateModuleData, m_morphConditions) }, // RW 0x89A90F
	{ "ExpLevelDraw", BannerCarrierUpdateModuleData::parseRawLine, nullptr, (int)offsetof(BannerCarrierUpdateModuleData, m_expLevelDraws) },
	{ "BannerMorphFX", INI::parseAsciiString, nullptr, (int)offsetof(BannerCarrierUpdateModuleData, m_bannerMorphFX) },
	{ "UnitSpawnFX", INI::parseAsciiString, nullptr, (int)offsetof(BannerCarrierUpdateModuleData, m_unitSpawnFX) },
	{ "ReplenishNearbyHorde", INI::parseBool, nullptr, (int)offsetof(BannerCarrierUpdateModuleData, m_replenishNearbyHorde) },
	{ "ReplenishAllNearbyHordes", INI::parseBool, nullptr, (int)offsetof(BannerCarrierUpdateModuleData, m_replenishAllNearbyHordes) },
	{ "ScanHordeDistance", INI::parseReal, nullptr, (int)offsetof(BannerCarrierUpdateModuleData, m_scanHordeDistance) },
	{ "UpgradeRequired", INI::parseAsciiString, nullptr, (int)offsetof(BannerCarrierUpdateModuleData, m_upgradeRequired) },
	{ nullptr, nullptr, nullptr, 0 }
};
} // namespace

void BannerCarrierUpdateModuleData::parseRawLine(INI *ini, void *, void *store, const void *)
{
	std::string line;
	for (const char *tok = ini->getNextTokenOrNull(); tok; tok = ini->getNextTokenOrNull())
	{
		line += (line.empty() ? "" : " ") + std::string(tok);
	}
	static_cast<std::vector<std::string> *>(store)->push_back(line);
}

namespace
{
// The quoted ModelState token of a MorphCondition line as a model-condition bit set: RW 0x4B5E05's BitFlags::parse over the names of RW 0xD9FAD8
// (port ModelCondition::indexOf), the three forms of Common/ModelState.h — NONE, a plain list (the first plain name clears what came before) and
// +NAME / -NAME adjusting the set. DONOR: BFME2 0x0049714F hands the quoted token to the rowed 0x0033394D, a 19-word set (Rva000B664E).
ModelConditionFlags parseModelStateList(const char *text)
{
	ModelConditionFlags flags;
	bool plain = false;
	bool adjusted = false;
	const std::string s(text ? text : "");
	size_t i = 0;
	while (i < s.size())
	{
		while (i < s.size() && std::isspace((unsigned char)s[i]))
		{
			++i;
		}
		if (i >= s.size())
		{
			break;
		}
		const size_t start = i;
		while (i < s.size() && !std::isspace((unsigned char)s[i]))
		{
			++i;
		}
		std::string word = s.substr(start, i - start);
		if (word.size() == 4 && AsciiStringUtil::compareNoCase(word, "NONE") == 0)
		{
			if (plain || adjusted)
			{
				throw INIException(3, "you may not mix normal and +- ops in bitstring lists");
			}
			flags.clear();
			continue;
		}
		bool set = true;
		if (!word.empty() && (word[0] == '+' || word[0] == '-'))
		{
			set = word[0] == '+';
			word.erase(0, 1);
			if (plain)
			{
				throw INIException(3, "you may not mix normal and +- ops in bitstring lists");
			}
			adjusted = true;
		}
		else
		{
			if (adjusted)
			{
				throw INIException(3, "you may not mix normal and +- ops in bitstring lists");
			}
			if (!plain)
			{
				flags.clear();
				plain = true;
			}
		}
		const int bit = ModelCondition::indexOf(word);
		if (bit < 0)
		{
			throw INIException(3, "Unknown model condition name '%s'", word.c_str());
		}
		flags.set(bit, set);
	}
	return flags;
}
} // namespace

// RW 0x89A90F (table row RW 0xC66090 + 0x18), DONOR BFME2 0x0049714F (345 B): `UnitType:<ThingTemplate> [Locomotor:<LocomotorSet>] ModelState:"<names>"`.
// UnitType and ModelState are mandatory and in that order; Locomotor is optional and sits between them (DONOR 0x00496ADE's record: name +0x00, set +0x08,
// locomotor +0x54). The error texts are the donor's.
void BannerCarrierUpdateModuleData::parseMorphCondition(INI *ini, void *, void *store, const void *)
{
	BannerCarrierUpdateModuleData::BannerMorphCondition entry;
	const char *token = ini->getNextTokenOrNull(ini->getSepsColon());
	if (!token || std::strcmp(token, "UnitType") != 0)
	{
		throw INIException(3, "UnitType expected");
	}
	entry.unitType = ini->getNextToken(ini->getSepsColon());
	token = ini->getNextTokenOrNull(ini->getSepsColon());
	if (token && std::strcmp(token, "Locomotor") == 0)
	{
		if (const char *name = ini->getNextToken(ini->getSepsColon()))
		{
			entry.locomotor = name;
		}
		token = ini->getNextTokenOrNull(ini->getSepsColon());
	}
	if (!token || std::strcmp(token, "ModelState") != 0)
	{
		throw INIException(3, "'ModelState' expected");
	}
	entry.modelState = parseModelStateList(ini->getNextToken(ini->getSepsQuote()));
	static_cast<std::vector<BannerCarrierUpdateModuleData::BannerMorphCondition> *>(store)->push_back(entry);
}

void BannerCarrierUpdateModuleData::buildFieldParse(MultiIniFieldParse &p)
{
	p.add(kBannerParse); // RW table 0xC66090
}

const char *BannerCarrierUpdate::stopLine()
{
	return kStop;
}

BannerCarrierUpdate::BannerCarrierUpdate(Thing *thing, const BannerCarrierUpdateModuleData *data)
	: UpdateModule(thing, data)
	, m_data(data)
{
	setWakeFrame(getObject(), UPDATE_SLEEP((int)(data->m_idleSpawnRate > 0 ? data->m_idleSpawnRate : 1)));
	// INFERENCE, stop S-587 (lane SPELLFIX-1): retail's ctor RW 0x89A258 is the UpdateModule ctor (RW 0x653114: next call frame + 0x14 = 0) and the vtables,
	// no wake of its own, so retail's first update runs at once. Porting that alone lets an Angmar horde's carrier (BannerCarrierMinLevel 0: a carrier from
	// the start) spawn a member while production still adds the horde's own members one by one, overfilling AngmarNecromancerHorde (8 of 7 Slots); how
	// retail's member production keeps the horde within its Slots is not read yet. Until it is, the first update waits IdleSpawnRate, and Heal's replenish
	// ping (DeletionUpdate 1 s, IdleSpawnRate 2 s) dies before it refills a horde (SPELLFIX-1's should_fail test)
}

void BannerCarrierUpdate::registerClass(ModuleFactory &modules)
{
	modules.bindTypedData<BannerCarrierUpdateModuleData>("BannerCarrierUpdate", MODULETYPE_BEHAVIOR);
	modules.bindModuleProc("BannerCarrierUpdate", MODULETYPE_BEHAVIOR, [](Thing *thing, const ModuleData *data, const ModuleFactory::ModuleTemplate &) -> std::unique_ptr<Module> {
		const BannerCarrierUpdateModuleData *typed = dynamic_cast<const BannerCarrierUpdateModuleData *>(data);
		if (!typed)
		{
			throw std::logic_error("BannerCarrierUpdate: the module data is not typed");
		}
		return std::make_unique<BannerCarrierUpdate>(thing, typed);
	});
}

namespace
{
// RW 0x693A1A(false): the object itself when its template is a HORDE (template + 0x114 bit 13), else its container when that is a HORDE, else none
Object *hordeObjectOf(Object *o)
{
	if (o->isKindOfName("HORDE"))
	{
		return o;
	}
	Object *c = o->getContainedBy();
	return c && c->isKindOfName("HORDE") ? c : nullptr;
}

// RW 0x8B50A5 on Object + 0x254 (the EmotionTrackerUpdate): the running nugget's type (nugget + 4 -> + 4) is one of RW 0x8B4F39's 3 .. 7 and 10 (POINT, FEAR,
// UNCONTROLLABLE_FEAR, TERROR, DOOM, BRACE_FOR_BEING_CRUSHED)
bool emotionBusy(Object &o)
{
	const EmotionTrackerUpdate *t = EmotionTrackerUpdate::of(o);
	if (!t || !t->current())
	{
		return false;
	}
	switch (t->current()->type())
	{
	case EMOTION_POINT:
	case EMOTION_FEAR:
	case EMOTION_UNCONTROLLABLE_FEAR:
	case EMOTION_TERROR:
	case EMOTION_DOOM:
	case EMOTION_BRACE_FOR_BEING_CRUSHED:
		return true;
	default:
		return false;
	}
}

// RW 0x89A308: the carrier's outermost container (Object + 0x27C chain), when its template is a GARRISON (template + 0x116 bit 6), refuses while it has an
// AllowBannerSpawnUpgrade module (found by name, RW 0x68BDA5; ExtraUpgradeModules.h) whose mux is not upgraded (slot 0): retail's two GarrisonTowerExpansions
bool bannerSpawnAllowed(Object &carrier)
{
	Object *top = &carrier;
	while (Object *c = top->getContainedBy())
	{
		top = c;
	}
	if (!top->isKindOfName("GARRISON"))
	{
		return true;
	}
	const BehaviorModule *m = top->findModule("AllowBannerSpawnUpgrade");
	if (!m)
	{
		return true;
	}
	const UpgradeModule *u = dynamic_cast<const UpgradeModule *>(m);
	if (!u)
	{
		throw std::logic_error("BannerCarrierUpdate: AllowBannerSpawnUpgrade of " + top->getTemplate()->getName() + " is not an upgrade module");
	}
	return u->isAlreadyUpgraded();
}
} // namespace

// RW 0x89ACE4 (BFME2 BannerCarrierUpdate::update 0x897524, byte-matched C++ in Open-BFME-2 BannerCarrierUpdate.cpp)
UpdateSleepTime BannerCarrierUpdate::update()
{
	Object *obj = getObject();
	const UpdateSleepTime sleep = UPDATE_SLEEP((int)(m_data->m_idleSpawnRate > 0 ? m_data->m_idleSpawnRate : 1));
	if (!m_data->m_upgradeRequired.empty() && !obj->hasUpgrade(m_data->m_upgradeRequired))
	{
		return sleep;
	}
	if (obj->testModelCondition(0x45) || obj->testStatus(0x57) || obj->testModelCondition(0x220) || obj->testStatus(0x3E))
	{
		return sleep;
	}
	if (AIUpdateInterface *ai = obj->getAIUpdateInterface())
	{
		if (ai->isMoving() || ai->mover().isWaitingForPath())
		{
			return sleep;
		}
	}
	// RW 0x89ADA3 .. 0x89ADC1 (lane BANNER-1): the horde object's emotion tracker (the carrier's own when it is in no horde)
	{
		Object *h = hordeObjectOf(obj);
		if (emotionBusy(h ? *h : *obj))
		{
			return sleep;
		}
	}
	if (m_data->m_replenishNearbyHorde)
	{
		Player *owner = obj->getControllingPlayer();
		if (!owner)
		{
			return sleep;
		}
		GameLogic &logic = obj->logic();
		PartitionFilterFn allies([&](Object &o) {
			return o.isKindOfName("HORDE") && owner->getRelationship(o.getTeam()) == ALLIES;
		});
		const auto hits = logic.partition().iterateObjectsInRange(*obj->getPosition(), m_data->m_scanHordeDistance,
			FROM_CENTER_2D, { &allies }, ITER_SORTED_NEAR_TO_FAR); // RW 0x89AB8F
		for (const PartitionHit &hit : hits)
		{
			Object *target = hit.object;
			HordeContain *hc = hordeContainOf(target);
			if (!hc || target->testStatus(2)) // UNDER_CONSTRUCTION
			{
				continue;
			}
			if ((int)hc->payloadCount() < hc->getSlotCapacity())
			{
				if (Object *member = hc->spawnReplacementMember(*target->getPosition(), target->getOrientation())) // RW 0x89AC6E: the target horde's matrix
				{
					member->updateShroudNow();
					if (FXEventLog::isFXName(m_data->m_unitSpawnFX))
					{
						logic.fxEvents().emit(FXEventLog::objectEvent(FXEvent::OBJECT_FX,
							"BannerCarrierUpdate UnitSpawnFX", logic.getFrame(), m_data->m_unitSpawnFX, *member));
					}
					++m_spawned;
				}
				if (!m_data->m_replenishAllNearbyHordes)
				{
					break;
				}
			}
			else
			{
				hc->expediteBannerRespawn();
			}
		}
		return sleep;
	}
	if (obj->testModelCondition(0x25) || obj->testModelCondition(0x3D)) // ATTACKING / MOVING
	{
		return sleep;
	}
	const unsigned frame = obj->logic().getFrame();
	Object *container = obj->getContainedBy();
	// RW 0x89ADF7 .. 0x89AE13 (lane BANNER-1): the CONTAINER's last shot (RW 0x68B645 on Object + 0x27C: its firing tracker + 0x44, 0 without one), unsigned
	// `ja`: no member while it fired after frame - MeleeFreeUnitSpawnTime. HORDE-2 read the carrier's own weapons here
	if (container)
	{
		const unsigned lastShot = container->getWeapons() ? container->getWeapons()->firingTracker().lastShotFrame() : 0u;
		if (lastShot > frame - m_data->m_meleeFreeUnitSpawnTime)
		{
			return sleep;
		}
	}
	// RW 0x89AE15 .. 0x89AE58 (lane BANNER-1): the container's horde (RW 0x693A1A) asks its HordeContainInterface slot 0x90 (RW 0x86EE52, HordeContain::attackedWithin:
	// a member removed within MeleeFreeUnitSpawnTime, or a member damaged within 4 seconds). HORDE-2's stand-in (a member IS_ATTACKING, or damaged within
	// MeleeFreeUnitSpawnTime) held the refill back while the members merely stood in their attack state
	if (container)
	{
		if (Object *h = hordeObjectOf(container))
		{
			if (HordeContain *hc = hordeContainOf(h))
			{
				ObjectID attacker = INVALID_ID;
				if (hc->attackedWithin(m_data->m_meleeFreeUnitSpawnTime, attacker))
				{
					return sleep;
				}
			}
		}
	}
	if (spawnMember())
	{
		++m_spawned;
	}
	return sleep;
}

// RW 0x89A392 (BFME2 0x896BD2): the member of the carrier's horde
bool BannerCarrierUpdate::spawnMember()
{
	Object *obj = getObject();
	Object *container = obj->getContainedBy();
	HordeContain *hc = hordeContainOf(container);
	if (!hc)
	{
		return false;
	}
	// RW 0x89A3CE .. 0x89A3E4 (lane INTEG-1): a member is made only while the horde's slot 0x188 (slot 0x180(0) RW 0x8706DF: the contain count plus the field at
	// interface + 0x58; minus slot 0x184 RW 0x86C085: one for each of H+0x264 and the banner carrier H+0x26C) is below slot 0x17C (RW 0x86C8DE: the contain's Slots,
	// module data + 0x98). HORDE-2's port only asked for a free RankInfo position, so a horde with more positions than Slots (AngmarNecromancerHorde: 15 / 7) grew
	// past its capacity once real carriers appeared (XP-1's rank). Lane BANNER-1: interface + 0x58 is the size of the map at + 0x54 (RW 0x86EE52 walks its head at
	// + 0x54: the members on their way into a garrison), so slot 0x188 is payloadCount(), as the nearby path already counted. INFERENCE: H+0x264 taken as none
	if ((int)hc->payloadCount() >= hc->getSlotCapacity())
	{
		return false;
	}
	if (!bannerSpawnAllowed(*obj)) // RW 0x89A3E9
	{
		return false;
	}
	// RW 0x89A3F2 .. 0x89A40E / 0x89A447: the container's controlling player's ScoreKeeper stops counting (+ 0x110, RW 0x79DC9E) while the member is made
	Player *player = container->getControllingPlayer();
	ScoreKeeper *keeper = player ? &player->getScoreKeeper() : nullptr;
	const bool counting = keeper && keeper->counting();
	if (keeper)
	{
		keeper->setCounting(false);
	}
	// RW 0x89A415: slot 0x18C with the carrier's matrix (Object + 8): the member starts where the carrier stands, facing its way
	Object *member = hc->spawnReplacementMember(*obj->getPosition(), obj->getOrientation());
	if (member)
	{
		member->updateShroudNow(); // RW 0x89A42F: RW 0x68C7E9 (the nearby path's call too)
		GameLogic &logic = obj->logic();
		if (FXEventLog::isFXName(m_data->m_unitSpawnFX)) // RW 0x89A434 .. 0x89A43F: UnitSpawnFX (data + 0x34) at the member (FXList::doFXObj RW 0x4B1B5A)
		{
			logic.fxEvents().emit(FXEventLog::objectEvent(FXEvent::OBJECT_FX, "BannerCarrierUpdate UnitSpawnFX", logic.getFrame(), m_data->m_unitSpawnFX, *member));
		}
	}
	if (keeper)
	{
		keeper->setCounting(counting);
	}
	return member != nullptr;
}

void BannerCarrierUpdate::crc(StateHasher &h) const
{
	UpdateModule::crc(h);
	h.addU64(m_spawned);
}


namespace
{
// DONOR BFME2 .rdata 0x00C4F6E0: the 81 model conditions RW 0x89A5C5 clears before it sets an entry's (the function-static mask the donor builds once).
// In RW's name table (RW 0xD9FAD8) these bits are MOUNTED, PASSENGER_VARIATION_1 .. 5 and USER_1 .. USER_75, i.e. exactly the look bits a banner
// carrier takes over from the horde it leads (test) - a banner carrier that morphs from type A to type B must lose A's bits.
const unsigned kBannerMorphConditionBits[] = {
	214, 194, 195, 196, 197, 198, 184, 185, 186, 187, 188, 458,
	459, 460, 461, 462, 463, 464, 465, 466, 467, 468, 469, 470,
	471, 472, 473, 474, 475, 476, 477, 478, 479, 480, 481, 482,
	483, 484, 485, 486, 487, 488, 489, 490, 491, 492, 493, 494,
	495, 496, 497, 498, 499, 500, 501, 502, 503, 504, 505, 506,
	507, 508, 509, 510, 511, 512, 513, 514, 515, 516, 517, 518,
	519, 520, 521, 522, 523, 524, 525, 526, 527,
};

const Object::ModelConditionBits &morphClearMask()
{
	static const Object::ModelConditionBits mask = [] {
		Object::ModelConditionBits bits{};
		for (const unsigned bit : kBannerMorphConditionBits)
		{
			bits[bit >> 5] |= 1u << (bit & 31u);
		}
		return bits;
	}();
	return mask;
}
} // namespace

// RW 0x89A5C5 (DONOR BFME2 0x00496E05, 299 B, ret 8): the morph step of one banner carrier.
void BannerCarrierUpdate::morphForTemplate(const std::string &unitType, bool playFX)
{
	Object *obj = getObject();
	// DONOR 0x00496D43: the MorphCondition vector is searched by the template name (compareNoCase); a miss fills the output with an all-zero set
	// (Rva0028F59A(0, 0xb8)), so a type with no line clears the morph bits instead of keeping the previous look.
	const BannerCarrierUpdateModuleData::BannerMorphCondition *entry = nullptr;
	for (const BannerCarrierUpdateModuleData::BannerMorphCondition &candidate : m_data->m_morphConditions)
	{
		if (AsciiStringUtil::compareNoCase(candidate.unitType, unitType) == 0)
		{
			entry = &candidate;
			break;
		}
	}
	Object::ModelConditionBits set{};
	if (entry)
	{
		for (int bit = 0; bit < MODELCONDITION_COUNT; ++bit)
		{
			if (entry->modelState.test(bit))
			{
				set[(size_t)bit >> 5] |= 1u << (bit & 31);
			}
		}
	}
	obj->clearAndSetModelConditionFlags(morphClearMask(), set); // RW 0x68D607
	// DONOR 0x00496E7C: Coord3D::Length() > 0, i.e. an object that stands anywhere but the origin (the port's Coord3D has no Length(), the
	// comparison is the same for a finite position)
	const Coord3D *pos = obj->getPosition();
	if (pos && (pos->x != 0.0f || pos->y != 0.0f || pos->z != 0.0f))
	{
		obj->updateShroudNow(); // RW 0x68C7E9
	}
	// DONOR 0x00496EA1: the FX comes before the locomotor lookup and plays whether or not a line matched (FXList::doFXObj, RW 0x4B1B5A)
	if (playFX && FXEventLog::isFXName(m_data->m_bannerMorphFX))
	{
		GameLogic &logic = obj->logic();
		logic.fxEvents().emit(FXEventLog::objectEvent(FXEvent::OBJECT_FX, "BannerCarrierUpdate BannerMorphFX", logic.getFrame(), m_data->m_bannerMorphFX, *obj));
	}
	// DONOR 0x00496DA3 / 0x00496F00: the entry's LocomotorSet through the AI's slot 0x238; no name, no call
	if (entry && !entry->locomotor.empty())
	{
		int set = -1;
		for (int i = 0; TheLocomotorSetNames[i]; ++i)
		{
			if (AsciiStringUtil::compareNoCase(entry->locomotor, TheLocomotorSetNames[i]) == 0)
			{
				set = i;
				break;
			}
		}
		if (set < 0)
		{
			throw std::logic_error("BannerCarrierUpdate: MorphCondition Locomotor '" + entry->locomotor + "' is not a LocomotorSet name");
		}
		if (AIUpdateInterface *ai = obj->getAIUpdateInterface())
		{
			ai->chooseLocomotorSet(set);
		}
		else
		{
			obj->logic().reportError("BannerCarrierUpdate of " + obj->getTemplate()->getName() + ": MorphCondition Locomotor names a set but the carrier has no AI");
		}
	}
}

// ---- HordeContain's side ------------------------------------------------------------------------------------------------------------------------------------------------
// lane PORT-75: RW 0x89A5C5 (DONOR BFME2 0x00496E05) from the contain's accept path (DONOR BFME2 0x00473212: the object being taken in is one of
// BannerCarriersAllowed -> 0x00473197): a banner carrier that joins a horde takes the look of the horde it leads. The key is the horde's member
// template (DONOR 0x0046A521 getBannerCarrierIndexToUse reports the template the carrier joins for) and the FX is asked for (the accept's
// !suppress). Both of the port's accept implementations call this one reaction.
void HordeContain::bannerCarrierMorph(Object &member)
{
	bool allowed = false;
	for (const std::string &name : m_data->horde.m_bannerCarriersAllowed)
	{
		if (AsciiStringUtil::compareNoCase(name, member.getTemplate()->getName()) == 0)
		{
			allowed = true;
			break;
		}
	}
	if (!allowed)
	{
		return;
	}
	BannerCarrierUpdate *update = dynamic_cast<BannerCarrierUpdate *>(member.findModule("BannerCarrierUpdate"));
	if (!update)
	{
		return;
	}
	update->morphForTemplate(getPayloadMemberTemplateName(), true);
}

// RW 0x8719E4
void HordeContain::bannerCheck(bool force)
{
	Object *horde = getObject();
	if (!horde->getAIUpdateInterface())
	{
		return;
	}
	if (m_bannerCountdown != 0)
	{
		--m_bannerCountdown;
	}
	const HordeContainModuleData &data = m_data->horde;
	if (m_bannerCountdown != 0 || m_bannerCarrier != 0)
	{
		return;
	}
	// RW 0x871A1B: the horde object's ExperienceTracker (object + 0x26C, read without a null test: every object has one, RW 0x69A606) rank (+ 0x24, lane XP-1),
	// `jle`: a carrier needs BannerCarrierMinLevel < rank (lane INTEG-1 connected XP-1's tracker here)
	const ExperienceTracker *tracker = horde->getExperienceTracker();
	if (!tracker)
	{
		throw std::logic_error("HordeContain::bannerCheck: the horde object has no ExperienceTracker (every object is made with one, RW 0x69A606)");
	}
	if (!((int)data.m_bannerCarrierMinLevel < tracker->getRank()))
	{
		return;
	}
	const unsigned frame = horde->logic().getFrame();
	if (!force)
	{
		const unsigned lastShot = horde->getWeapons() ? horde->getWeapons()->firingTracker().lastShotFrame() : 0u;
		if ((unsigned)(frame - 4u * (unsigned)LOGICFRAMES_PER_SECOND) <= lastShot)
		{
			return;
		}
	}
	// RW 0x870204
	if (data.m_bannerCarriersAllowed.empty())
	{
		return;
	}
	const ThingTemplate *tt = horde->logic().things().findTemplate(data.m_bannerCarriersAllowed[0]);
	if (!tt)
	{
		return;
	}
	Object *carrier = horde->logic().newObject(tt, horde->getTeam(), ObjectStatusMaskType{});
	if (!carrier)
	{
		return;
	}
	carrier->setPosition(horde->getPosition());
	carrier->setOrientation(horde->getOrientation());
	m_bannerCarrier = carrier->getID();
	addToContain(carrier);
	if (BannerCarrierUpdate *b = dynamic_cast<BannerCarrierUpdate *>(carrier->findModule("BannerCarrierUpdate")))
	{
		m_bannerCountdown = b->data().m_meleeFreeBannerReSpawnTime;
	}
	m_dirty = true;
}

// RW 0x871915. NOT PORTED (S-587): the H + 0x264 branch (the other non-slot member, not identified). With BannerCarrierDestroyHordeOnDeath (module data
// + 0x224, RW 0x871950) retail keeps the carrier id (H + 0x26C is not cleared, so no new carrier) and kills every member of the contain list copy that
// has a body (RW 0x871983 .. 0x87199B: Object::kill RW 0x698EC3 with DAMAGE_UNRESISTABLE and the BannerCarrierHordeDeathType word, + 0x228, as the death
// type). Without the flag the id is cleared as before
void HordeContain::bannerCarrierRemoved(const Object &member)
{
	if (member.getID() != m_bannerCarrier || m_bannerCarrier == 0)
	{
		return;
	}
	if (m_data->horde.m_bannerCarrierDestroyHordeOnDeath)
	{
		// RW 0x872AB5 -> 0x865598 copies the contain list (RW 0x76CB49) before the kills remove their members from it
		const std::vector<Object *> members(m_members.begin(), m_members.end());
		for (Object *m : members)
		{
			if (m && m->getBodyModule())
			{
				m->kill(DAMAGE_UNRESISTABLE, (int)m_data->horde.m_bannerCarrierHordeDeathType);
			}
		}
		return;
	}
	m_bannerCarrier = 0; // RW 0x8719B5
	// RW 0x8719BC .. 0x8719CE: RW 0x86BF43 finds the carrier's BannerCarrierUpdate; the countdown H + 0x27C = its data + 0x10 (DiedRespawnTime)
	if (const BannerCarrierUpdate *b = dynamic_cast<const BannerCarrierUpdate *>(member.findModule("BannerCarrierUpdate")))
	{
		m_bannerCountdown = b->data().m_diedRespawnTime;
	}
}

bool HordeContain::bannerMemberSlot(const Object &member, Coord3D &pos) const
{
	if (member.getID() != m_bannerCarrier || m_data->horde.m_bannerCarrierPosition.empty())
	{
		return false;
	}
	const std::string payload = getPayloadMemberTemplateName();
	size_t entry = 0;
	for (size_t k = 0; k < m_data->horde.m_bannerCarrierPosition.size(); ++k)
	{
		if (m_data->horde.m_bannerCarrierPosition[k].unitType == payload)
		{
			entry = k;
			break;
		}
	}
	// the entry's slot (RW 0x8776BB appends one per entry; RW 0x86C4EA centres it with the formation), else the entry's own position
	float ex = m_data->horde.m_bannerCarrierPosition[entry].x, ey = m_data->horde.m_bannerCarrierPosition[entry].y;
	const int slot = m_core->bannerSlot(entry);
	if (slot >= 0)
	{
		ex = m_core->slots()[(size_t)slot].x;
		ey = m_core->slots()[(size_t)slot].y;
	}
	const Object *horde = getObject();
	const float a = horde->getOrientation();
	const float c = SimMath::cosDet(a), s = SimMath::sinDet(a);
	pos.x = SimMath::addf32(horde->getPosition()->x, SimMath::subf32(SimMath::mulf32(ex, c), SimMath::mulf32(ey, s)));
	pos.y = SimMath::addf32(horde->getPosition()->y, SimMath::addf32(SimMath::mulf32(ex, s), SimMath::mulf32(ey, c)));
	pos.z = horde->logic().getGroundHeight(pos.x, pos.y);
	return true;
}

// RW slot 0x18C (RW 0x873AE3; BFME2 HordeContain::rva004708BC 0x4708BC, byte-matched C++ in Open-BFME-2 HordeContainIface11CSlots.cpp) behind RW 0x89A392's and
// RW 0x89AB8F's "fewer members than slots": a member of the payload template at the given matrix (lane BANNER-1: with the matrix's orientation, the horde's
// weapon set flags 0x18 .. 0x1A, the template's experience pool and the horde's attribute modifiers and remaining lifetime)
Object *HordeContain::spawnReplacementMember(const Coord3D &pos, float orientation)
{
	Object *horde = getObject();
	const std::vector<int> freeSlots = freeSlotIndices();
	if (freeSlots.empty())
	{
		return nullptr;
	}
	const std::string name = getPayloadMemberTemplateName();
	const ThingTemplate *tt = name.empty() ? nullptr : horde->logic().things().findTemplate(name);
	if (!tt)
	{
		return nullptr;
	}
	Object *m = horde->logic().newObject(tt, horde->getTeam(), ObjectStatusMaskType{});
	if (!m)
	{
		return nullptr;
	}
	m->setPosition(&pos); // RW 0x873B7D: RW 0x68DA67, the matrix
	m->setOrientation(orientation);
	acceptCreatedMember(m); // RW 0x873B89 (RW 0x86CC84): joins without being put on its slot: it walks there from where it was made
	m_producedMembers.erase(std::remove(m_producedMembers.begin(), m_producedMembers.end(), m->getID()), m_producedMembers.end());
	// RW 0x873B92 .. 0x873BCB: the horde's weapon set flags 0x18 .. 0x1A (RW 0x68DEEC on the horde, RW 0x691059 on the member)
	const WeaponConditionFlags &hordeFlags = horde->getWeaponSetFlags();
	for (int bit = 0x18; bit <= 0x1A; ++bit)
	{
		if (((hordeFlags[(size_t)bit >> 5] >> (bit & 31)) & 1u) != 0)
		{
			m->setWeaponSetFlag(bit, true);
		}
	}
	// RW 0x873BD3 .. 0x873C35: the experience pool of the member's template (interface + 0x13C, keyed by template + 0x5E8), else 1.0f (RW 0xBD1908), through
	// setExperienceAndLevel(value, false) (RW 0x79D8EF): a member made for a veteran horde arrives at the horde's level. HORDE-2 made it at level one
	{
		const auto it = m_experiencePools.find(static_cast<const ThingTemplate *>(m->getTemplate())->getTemplateID());
		const float xp = it != m_experiencePools.end() ? it->second : 1.0f;
		ExperienceTracker *tracker = m->getExperienceTracker();
		if (!tracker)
		{
			throw std::logic_error("HordeContain::spawnReplacementMember: the member has no ExperienceTracker (every object is made with one, RW 0x69A606)");
		}
		tracker->setExperienceAndLevel(xp, false);
	}
	// RW 0x873C53 .. 0x873C5F: the horde's AttributeModifierPoolUpdate (RW 0x68C4A6) hands every entry to the member (RW 0x804F02)
	if (const AttributeModifierPool *pool = static_cast<const AttributeModifierPool *>(horde->findModule("AttributeModifierPoolUpdate")))
	{
		pool->handDownTo(*m);
	}
	// RW 0x873C83 .. 0x873CC5: the horde's remaining lifetime (LifetimeUpdate, found by name on both) becomes the member's (RW 0x7A7E41 with min = max)
	if (const LifetimeUpdate *hordeLife = static_cast<const LifetimeUpdate *>(horde->findModule("LifetimeUpdate")))
	{
		if (LifetimeUpdate *memberLife = static_cast<LifetimeUpdate *>(m->findModule("LifetimeUpdate")))
		{
			const unsigned left = hordeLife->dieFrame() - horde->logic().getFrame();
			memberLife->setLifetimeRange(left, left);
		}
	}
	// NOT PORTED (S-587): the TEMPORARILY_DEFECTED hand-over (RW 0x873B4E .. 0x873B61), RW 0x625E0A (RW 0x873B73), interface slot 0x174 (RW 0x873C4B) and the
	// behavior links (RW 0x873CD8 .. 0x873D08). RW 0x873D19 .. 0x873D2F: the horde interface's slot 0x10(0) (not identified) and slot 0x1E4 (RW 0x86C92D): the
	// formation's AttributeModifiers go to every member again, the new one included
	applyFormationModifiers();
	m_dirty = true;
	return m;
}

unsigned HordeContain::payloadCount() const
{
	return (unsigned)(m_members.size() + m_garrisonEntering.size()) - (m_bannerCarrier != 0 ? 1u : 0u);
}

Object *HordeContain::firstMemberNotKindOf(unsigned kindOfBit) const
{
	for (Object *member : m_members) // RW 0x8705BC: the contained list (slot 0x118)
	{
		if (!member->isKindOf(kindOfBit)) // RW 0x444FA2: template + 0x108
		{
			return member;
		}
	}
	if (!m_members.empty())
	{
		return m_members.front(); // RW 0x8705E9
	}
	GameLogic &logic = getObject()->logic();
	for (ObjectID id : m_garrisonEntering) // RW 0x8705F3 .. 0x87062C
	{
		Object *member = logic.findObjectByID(id);
		if (member && !member->isKindOf(kindOfBit))
		{
			return member;
		}
	}
	return m_garrisonEntering.empty() ? nullptr : logic.findObjectByID(*m_garrisonEntering.begin()); // RW 0x87062E .. 0x87064C
}

unsigned HordeContain::lastMemberDamageFrame() const
{
	unsigned last = 0xFFFFFFFFu;
	const auto read = [&](const Object *member) {
		if (member)
		{
			if (const auto *body = dynamic_cast<const ActiveBody *>(member->getBodyModule()))
			{
				const unsigned frame = body->lastDamageFrame();
				if (last == 0xFFFFFFFFu || last < frame)
				{
					last = frame;
				}
			}
		}
	};
	for (const Object *member : m_members)
	{
		read(member);
	}
	for (ObjectID id : m_garrisonEntering)
	{
		read(getObject()->logic().findObjectByID(id));
	}
	return last;
}

// ---- lane SAVE-4b: the snapshot (OpenBFME native, S-4190) ---------------------------------------------------------------------------------------------------
void BannerCarrierUpdate::xfer(Xfer &x)
{
	SnapshotXfer::version(x, 1, 1);
	xferUpdateModule(x);
	SnapshotXfer::u64(x, m_spawned);
}
