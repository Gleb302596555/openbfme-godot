// OpenBFME. GPL-3.0. See EvaSightingNotice.h.

#include "GameLogic/Object/EvaSightingNotice.h"

#include "Common/INIDataTypes.h"
#include "Common/Player.h"
#include "Common/Thing/ThingTemplate.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/CastleModules.h"
#include "GameLogic/Module/SlavedUpdate.h"
#include "GameLogic/Module/SpawnBehavior.h"
#include "GameLogic/Object/EvaDamageNotice.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/Object/PartitionManager.h"
#include "GameLogic/ObjectTemplateInfo.h"

namespace
{
int kindBit(const char *name)
{
	return ObjectTemplateInfoBuilder::kindOfIndex(name);
}
} // namespace

bool EvaSighting::findClosestLocalObject(const GameLogic &logic, const Object &sightingObj, const Player &localPlayer, float scanRange, Coord3D &outPos)
{
	// RW 0x672DFB: PartitionManager::getClosestObject (RW 0xA39090)
	// Filters: alive, not self, owned by localPlayer, none of INERT_SHROUD_REVEALER (122) / IGNORE_FOR_EVA_SPEECH_POSITION (158)
	static const int kInertShroudRevealer = kindBit("INERT_SHROUD_REVEALER");
	static const int kIgnoreForEvaSpeech = kindBit("IGNORE_FOR_EVA_SPEECH_POSITION");

	PartitionFilterFn filter([&](Object &cand) {
		if (&cand == &sightingObj)
		{
			return false;
		}
		if (cand.isEffectivelyDead())
		{
			return false;
		}
		if (cand.getControllingPlayer() != &localPlayer)
		{
			return false;
		}
		if (kInertShroudRevealer >= 0 && cand.isKindOf((unsigned)kInertShroudRevealer))
		{
			return false;
		}
		if (kIgnoreForEvaSpeech >= 0 && cand.isKindOf((unsigned)kIgnoreForEvaSpeech))
		{
			return false;
		}
		return true;
	});

	float distSqr = 0.0f;
	const Object *closest = logic.partition().getClosestObject(*sightingObj.getPosition(), scanRange, FROM_CENTER_2D, { &filter }, nullptr, &distSqr);
	if (closest && closest->getPosition())
	{
		outPos = *closest->getPosition();
		return true;
	}
	return false;
}

bool EvaSighting::ownerHasAnotherSightedObject(const GameLogic &logic, const Object &obj, const Player &owner, const Player &localPlayer, const std::string &eventName, const std::function<bool(ObjectID)> &isFullyObscured)
{
	// RW 0x6ABABD / RW 0x672D90: iterate owner's objects
	static const int kIgnoredInGui = kindBit("IGNORED_IN_GUI");

	for (const Object *other = logic.getFirstObject(); other; other = other->getNextObject())
	{
		if (other == &obj)
		{
			continue;
		}
		if (other->getControllingPlayer() != &owner)
		{
			continue;
		}
		if (other->isEffectivelyDead())
		{
			continue;
		}
		if (kIgnoredInGui >= 0 && other->isKindOf((unsigned)kIgnoredInGui))
		{
			continue;
		}
		const ThingTemplate *tt = other->getTemplate();
		if (!tt)
		{
			continue;
		}
		const std::string ev = EvaDamage::templateEvent(*tt, "EvaEnemyObjectSightedEvent");
		if (ev != eventName)
		{
			continue;
		}
		if (isFullyObscured && isFullyObscured(other->getID()))
		{
			continue;
		}
		if (!EvaDamage::isVisibleTo(logic, *other, &localPlayer, true))
		{
			continue;
		}
		return true; // found another visible unobscured unit with the same event
	}
	return false;
}

bool EvaSighting::isSlaveControlledBySlaver(const Object &obj, ObjectID &outSlaverId)
{
	// RW 0x678D1D..0x678D24: SlavedUpdateInterface::of(obj)->useSlaverAsControlForEvaObjectSightedEvents()
	const SlavedUpdateInterface *slaved = SlavedUpdateInterface::of(obj);
	if (slaved && slaved->useSlaverAsControlForEvaObjectSightedEvents())
	{
		outSlaverId = slaved->getSlaverID();
		return true;
	}
	return false;
}

void EvaSighting::forEachSpawnedSlave(Object &slaverObj, const std::function<void(ObjectID)> &fn)
{
	// RW 0x6709A2 / 0x670983: SpawnBehaviorInterface::of(slaverObj)->forEachSpawnID(fn)
	if (SpawnBehaviorInterface *spawn = SpawnBehaviorInterface::of(slaverObj))
	{
		spawn->forEachSpawnID(fn);
	}
}

bool EvaSighting::getCastleInfo(Object &obj, ObjectID &outCastleId, std::string &outCastleEvent)
{
	// RW 0x678DE7..0x678E7C: STRUCTURE, CastleMemberBehavior, CastleBehavior
	static const int kStructure = kindBit("STRUCTURE");
	if (kStructure < 0 || !obj.isKindOf((unsigned)kStructure))
	{
		return false;
	}
	CastleMemberBehavior *member = CastleMemberBehavior::of(obj);
	if (!member)
	{
		return false;
	}
	ObjectID castleId = member->castleId();
	if (castleId == INVALID_ID)
	{
		return false;
	}
	Object *castleObj = obj.logic().findObjectByID(castleId);
	if (!castleObj)
	{
		return false;
	}
	CastleBehavior *castle = CastleBehavior::of(*castleObj);
	if (!castle)
	{
		return false;
	}
	const std::string &ev = castle->evaEnemyCastleSightedEvent();
	if (ev.empty())
	{
		return false;
	}
	outCastleId = castleId;
	outCastleEvent = ev;
	return true;
}

void EvaSighting::forEachCastleMember(Object &castleObj, const std::function<void(ObjectID)> &fn)
{
	// RW 0x678EB6..0x678EC0: CastleBehavior::forEachMemberId
	if (CastleBehavior *castle = CastleBehavior::of(castleObj))
	{
		castle->forEachMemberId(fn);
	}
}
