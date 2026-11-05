// OpenBFME. GPL-3.0.
//
// TerrainTaintClear: TheTerrainLogic's radius clear RW 0x683393 and the castle's clear RW 0x683955 (TerrainTaintClear.h holds the target facts).

#include "GameLogic/Map/TerrainTaintClear.h"

#include "GameLogic/GameLogic.h"
#include "GameLogic/Map/TerrainTrees.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/Object/PartitionManager.h"
#include "GameLogic/ObjectTemplateInfo.h"
#include "GameLogic/System/TaintManager.h"

#include <vector>

namespace
{
// RW 0xA39340 with the KindOf filter (RW 0x444D39(0, bit) / 0x445139): the objects of KindOf `bit` within the radius, FROM_CENTER_2D, nearest first
std::vector<Object *> kindInRange(GameLogic &logic, const Coord3D &position, float radius, int bit)
{
	PartitionFilterFn kind([bit](Object &o) { return bit >= 0 && o.isKindOf((unsigned)bit); });
	std::vector<Object *> found;
	for (const PartitionHit &h : logic.partition().iterateObjectsInRange(position, radius, FROM_CENTER_2D, { &kind }, ITER_SORTED_NEAR_TO_FAR))
	{
		found.push_back(h.object);
	}
	return found;
}
} // namespace

namespace TerrainTaintClear
{
// RW 0x683393
int clearRadius(GameLogic &logic, const Coord3D *position, float radius, std::int32_t owner)
{
	if (!position) // RW 0x6833AA (the managers always exist in the port)
	{
		return 0;
	}
	logic.taint().paint(*position, radius, TaintManager::NEUTRAL, true, owner); // RW 0x6833D8 .. 0x6833EA
	static const int kTaintEffect = ObjectTemplateInfoBuilder::kindOfIndex("TAINTEFFECT"); // RW 0x6833F4: 0xAF
	int destroyed = 0;
	for (Object *o : kindInRange(logic, *position, radius, kTaintEffect)) // RW 0x683424 .. 0x68344C
	{
		logic.destroyObject(o); // RW 0x68343D (RW 0x62BBAB)
		++destroyed;
	}
	logic.trees().removeTreesInRadius(*position, radius); // RW 0x683458 (RW 0x67F63F)
	logic.trees().hideScorches(*position, radius);        // RW 0x68345D .. 0x683498: TheTerrainVisual slot 0x5C
	return destroyed;
}

// RW 0x683955
int clearForCastle(GameLogic &logic, const Coord3D *position, float radius)
{
	if (!position) // RW 0x683966
	{
		return 0;
	}
	static const int kTaint = ObjectTemplateInfoBuilder::kindOfIndex("TAINT"); // RW 0x683996: 0x98
	int destroyed = 0;
	for (Object *o : kindInRange(logic, *position, radius, kTaint)) // RW 0x6839CD .. 0x683A0B
	{
		const Coord3D at = *o->getPosition();
		clearRadius(logic, &at, radius, -1); // RW 0x6839DF .. 0x6839E3
		logic.destroyObject(o);              // RW 0x6839EF
		++destroyed;
	}
	clearRadius(logic, position, radius, -1); // RW 0x683A0D .. 0x683A10
	return destroyed;
}
} // namespace TerrainTaintClear
