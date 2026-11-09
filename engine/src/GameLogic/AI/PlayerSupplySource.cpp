// OpenBFME. GPL-3.0.
// See PlayerSupplySource.h. No runtime caller yet; the remaining Player AI
// lifecycle and enemy bounds must not be replaced by the manager AI's state.
#include "GameLogic/AI/PlayerSupplySource.h"

#include "Common/Player.h"
#include "Common/Thing/KindOfTokens.h"
#include "GameLogic/AI/PlayerSupplyDistance.h"
#include "GameLogic/Economy.h"
#include "GameLogic/EconomySettings.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/DockUpdate.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/Object/PartitionManager.h"
#include "GameLogic/ObjectTemplateInfo.h"
#include "GameLogic/SimMath.h"

#include <cstring>

namespace
{
// RW 0x8F6BE7 ([esi + 4] + 0x108) and RW 0x660DB2 -> 0x70C4FE read the template's KindOf mask.
bool templateKind(GameLogic &logic, const Object &object, int index)
{
	const ThingTemplate *tpl = object.getTemplate();
	return tpl && index >= 0 && MaskTest(logic.templateInfo(tpl).kindOf, (unsigned)index);
}

// RW 0x8F6CAE .. 0x8F6D0A chains:
//   * 0xBD88B0 (RW 0x660DB2): template KindOf SUPPLY_GATHERING_CENTER;
//   * 0xC10E38 (RW 0x66107F, layer mask RW 0x661069): controlling player == &player;
//   * 0xC10E2C (RW 0x660EB1): Object +0x458 bit 3 (OFF_MAP) clear.
class OwnCenter : public PartitionFilter
{
public:
	OwnCenter(GameLogic &logic, const Player &player)
		: m_logic(logic)
		, m_player(player)
		, m_kind(KindOfTokens::indexOf("SUPPLY_GATHERING_CENTER"))
	{
	}
	bool allow(Object &obj) override
	{
		return templateKind(m_logic, obj, m_kind) && obj.getControllingPlayer() == &m_player && !obj.isOffMap();
	}
	std::uint32_t playerLayerMask() const override
	{
		const unsigned index = (unsigned)m_player.getPlayerIndex();
		return 1u << (index & 31u); // RW 0x661069: shl eax, cl; PartitionManager::layerMask adds layer -1
	}

private:
	GameLogic &m_logic;
	const Player &m_player;
	int m_kind;
};

// RW 0x8F6C57 .. 0x8F6C5D: IMUL keeps the low 32 bits; CMP / JL compare them as signed.
std::int32_t supplyValue(int boxes, int perBox)
{
	const std::uint32_t bits = (std::uint32_t)boxes * (std::uint32_t)perBox;
	std::int32_t value;
	std::memcpy(&value, &bits, sizeof(value));
	return value;
}
} // namespace

Object *PlayerSupplySource::choose(GameLogic &logic, const Player &player, const Coord3D &ownPosition, const Coord3D *enemyMidpoint,
                                   std::int32_t minValue)
{
	static const int structure = KindOfTokens::indexOf("STRUCTURE");
	static const int source = KindOfTokens::indexOf("SUPPLY_SOURCE");
	OwnCenter center(logic, player);
	PartitionFilter *const centerFilter = &center;
	const PartitionFilterList filters(&centerFilter, 1);

	for (;;)
	{
		Object *best = nullptr;
		float bestDistance = 0.0f;
		for (Object *object = logic.getFirstObject(); object; object = object->getNextObject())
		{
			if (!templateKind(logic, *object, structure) || !templateKind(logic, *object, source))
			{
				continue;
			}
			const auto *dock = dynamic_cast<const SupplyWarehouseDockUpdate *>(object->findModule("SupplyWarehouseDockUpdate"));
			if (!dock || supplyValue(dock->boxesStored(), logic.economy().settings().valuePerSupplyBox) < minValue ||
			    player.getRelationship(object->getTeam()) == ENEMIES)
			{
				continue;
			}
			const float radius = SimMath::sseAdd(logic.partition().geometryOf(*object).circle, 200.0f);
			if (logic.partition().getClosestObject(*object->getPosition(), radius, FROM_CENTER_3D, filters))
			{
				continue;
			}
			const float distance = PlayerSupplyDistance::ownSquared(*object->getPosition(), ownPosition);
			if (enemyMidpoint && PlayerSupplyDistance::rejectByEnemy(*object->getPosition(), *enemyMidpoint, distance))
			{
				continue;
			}
			if (!best || PlayerSupplyDistance::closerThan(distance, bestDistance))
			{
				best = object;
				bestDistance = distance;
			}
		}
		if (best)
		{
			return best;
		}
		minValue /= 2; // RW 0x8F6DC4 .. 0x8F6DCF: CDQ / SUB / SAR rounds signed division toward zero; JG 0x64
		if (minValue <= 100)
		{
			return nullptr;
		}
	}
}
