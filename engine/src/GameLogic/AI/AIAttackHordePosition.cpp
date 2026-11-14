// OpenBFME. GPL-3.0.
// PORT-165 (issue #394): the member-attack wait state of RW 0x74548D.
#include "GameLogic/AI/AIAttackHordePosition.h"

#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object/Object.h"

#include <algorithm>
#include <stdexcept>

AIHordePositionWaitState::AIHordePositionWaitState(AIStateMachine &machine)
	: AIState(machine, "AIHordePositionWaitState")
{
}

StateReturnType AIHordePositionWaitState::onEnter()
{
	// RW 0x747999: KindOf HORDE (+0x114 mask 0x2000) returns 0, otherwise -2.
	return owner().isKindOfName("HORDE") ? STATE_CONTINUE : STATE_FAILURE;
}

StateReturnType AIHordePositionWaitState::update()
{
	// RW 0x742CEA -> 0x742C8C: a dead goal fails; otherwise the horde's
	// interface is asked whether every living AI member attacks the goal (slot
	// 0x148), or is active when there is no goal (slot 0x14C).
	Object *goal = machine().goalObject();
	if (goal && goal->isEffectivelyDead())
	{
		return STATE_FAILURE;
	}
	ContainModuleInterface *contain = owner().getContain();
	if (!contain || !contain->getHordeContainInterface() || !contain->getContainedItemsList())
	{
		throw std::logic_error("AIHordePositionWaitState: HORDE has no horde containment interface");
	}
	const auto members = *contain->getContainedItemsList(); // RW 0x865598 takes a copy
	for (const Object *member : members)
	{
		if (!member || member->isEffectivelyDead())
		{
			continue;
		}
		const AIUpdateInterface *memberAI = member->getAIUpdateInterface();
		if (!memberAI)
		{
			continue;
		}
		// RW 0x86D562 / 0x86D438 query AI vslot 0x1BC (active, not
		// merely the attack-state id). An empty eligible member list passes.
		if (!memberAI->isStateActive())
		{
			return STATE_FAILURE;
		}
		if (goal)
		{
			Object *victim = memberAI->currentVictim(); // RW 0x668303
			if (!victim || victim->getID() != goal->getID())
			{
				return STATE_FAILURE;
			}
			if (goal->isKindOfName("HORDE"))
			{
				ContainModuleInterface *targetContain = goal->getContain();
				if (!targetContain || !targetContain->getContainedItemsList())
				{
					throw std::logic_error("AIHordePositionWaitState: target HORDE has no containment list");
				}
				const auto &targetMembers = *targetContain->getContainedItemsList();
				if (std::find(targetMembers.begin(), targetMembers.end(), victim) == targetMembers.end())
				{
					return STATE_FAILURE; // RW 0x86D4A4: target contain vslot 0xE8
				}
			}
		}
	}
	return STATE_CONTINUE;
}
