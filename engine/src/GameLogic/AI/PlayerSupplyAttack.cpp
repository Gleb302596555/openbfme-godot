// OpenBFME. GPL-3.0.
#include "GameLogic/AI/PlayerSupplyAttack.h"

#include "Common/Player.h"
#include "Common/StateHash.h"
#include "Common/Team.h"
#include "Common/Thing/KindOfTokens.h"
#include "Common/Xfer.h"
#include "GameLogic/Module/BehaviorModule.h"
#include "GameLogic/Object/Object.h"

bool PlayerSupplyAttack::check(const Player &player, const TeamFactory &teams, std::uint32_t frame)
{
	// RW 0x8F5ACB .. 0x8F5B01: frame zero does not clear the ID.
	if (frame == 0)
	{
		m_nextFrame = 10;
		return false;
	}
	m_objectID = INVALID_ID;
	if (frame < m_nextFrame || player.attackedFrame() + 10u < frame)
	{
		return false;
	}
	m_nextFrame = frame + 10u;
	static const int supply = KindOfTokens::indexOf("SUPPLY_GATHERING_CENTER");
	static const int dozer = KindOfTokens::indexOf("DOZER");
	static const int harvester = KindOfTokens::indexOf("HARVESTER");
	auto has = [](const Object &o, int bit) { return bit >= 0 && o.isKindOf((unsigned)bit); };
	// Player +0x34C prototypes in insertion order; each prototype's team and
	// each team's member lists are traversed from their newest element.
	for (const auto &prototype : teams.prototypes())
	{
		if (prototype->getControllingPlayer() != &player)
		{
			continue;
		}
		const auto &instances = prototype->teams();
		for (auto it = instances.rbegin(); it != instances.rend(); ++it)
		{
			for (Object *object = (*it)->getLastMember(); object; object = object->friend_teamPrev())
			{
				if (!has(*object, supply) && !has(*object, dozer) && !has(*object, harvester))
				{
					continue;
				}
				const BodyModuleInterface *body = object->getBodyModule();
				bool noEffect = false;
				if (body && body->lastDamageRecordNoEffect(noEffect) && !noEffect && body->lastDamageFrame() + 10u > frame)
				{
					m_objectID = object->getID();
					return true;
				}
			}
		}
	}
	return false;
}

void PlayerSupplyAttack::crc(StateHasher &hash) const
{
	hash.addU32(m_nextFrame);
	hash.addU32(m_objectID);
}

void PlayerSupplyAttack::xfer(Xfer &x)
{
	x.xferUnsignedInt(&m_nextFrame);
	x.xferObjectID(&m_objectID);
}
