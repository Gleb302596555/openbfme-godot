// OpenBFME unit tests. GPL-3.0.
// The radius clear of the Taint / Elven Wood ground (TheTerrainLogic RW 0x683393, GameLogic/Map/TerrainTaintClear.h) and its callers on retail data: the
// cells go back to 0x80, the TAINTEFFECT objects within the radius are destroyed, the trees within it go at once and the client hides the scorch marks; an
// object of KindOf TAINT with an AttributeModifierAuraUpdate clears its Range when it is deleted (RW 0x89EE67). Expectations from the RW bodies.

#include "doctest.h"
#include "HudTestUtil.h"

#include "Common/Thing/ThingFactory.h"
#include "Common/Thing/ThingTemplate.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Map/TerrainTaintClear.h"
#include "GameLogic/Map/TerrainTrees.h"
#include "GameLogic/Module/HeroModules.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/ObjectTemplateInfo.h"
#include "GameLogic/System/TaintManager.h"

#include <string>
#include <vector>

namespace
{
int kindBit(const char *name)
{
	const int k = ObjectTemplateInfoBuilder::kindOfIndex(name);
	REQUIRE(k >= 0); // tolerance: kindOfIndex answers -1 for a name it does not know, any other value is the bit
	return k;
}

// the retail templates of KindOf `name`, in the factory's order
std::vector<std::string> templatesOfKind(GameLogic &logic, const char *name)
{
	const int bit = kindBit(name);
	std::vector<std::string> out;
	for (const ThingTemplate *tt : logic.things().templates())
	{
		if (tt && MaskTest(ObjectTemplateInfoBuilder::build(*tt).kindOf, (unsigned)bit))
		{
			out.push_back(tt->getName());
		}
	}
	return out;
}

const TerrainTrees::Record *firstTree(GameLogic &logic)
{
	for (const TerrainTrees::Record &t : logic.trees().records())
	{
		if (t.clientId != 0)
		{
			return &t;
		}
	}
	return nullptr;
}
} // namespace

TEST_CASE("taint clear retail: the radius clear paints 0x80, destroys the TAINTEFFECT objects, removes the trees and hides the scorches (RW 0x683393)")
{
	if (!hudtest::haveWorld("taint clear"))
	{
		return;
	}
	hudtest::Rig rig(hudtest::shared(), "map mp fall back 4p", "FactionAngmar");
	GameLogic &logic = rig.logic();
	TaintManager &taint = logic.taint();
	REQUIRE(taint.countX() > 1); // tolerance: the map's size in cells varies with the map; one cell would mean the new game never gave the extent (RW 0x62FCF6)
	const TerrainTrees::Record *tree = firstTree(logic);
	REQUIRE(tree != nullptr);
	const Coord3D at = tree->position;
	const std::uint32_t treeId = tree->clientId;
	const std::vector<std::string> effects = templatesOfKind(logic, "TAINTEFFECT");
	REQUIRE_FALSE(effects.empty());
	Object *effect = rig.make(effects.front(), at.x + 10.0f, at.y);
	REQUIRE(effect != nullptr);
	Object *orc = rig.make("MordorFighter", at.x - 10.0f, at.y);
	REQUIRE(orc != nullptr);
	const ObjectID effectId = effect->getID(), orcId = orc->getID();
	rig.frame(1); // the new objects enter ThePartitionManager's cells with the frame
	// the Taint spell's paint, owned by object 5 (RW 0x8C8F65)
	taint.paint(at, 50.0f, 0x00, true, 5);
	REQUIRE(taint.stateAt(at, nullptr) == TaintManager::STATE_TAINT);
	// another owner's clear leaves the cells (RW 0xAD5755), but the objects, trees and scorches go all the same
	CHECK(TerrainTaintClear::clearRadius(logic, &at, 50.0f, 9) == 1);
	CHECK(taint.valueAt(at) == 0);
	CHECK(tree->clientId == 0); // RW 0x67F6BB: the record cleared
	bool removedNow = false, hidden = false;
	for (const TerrainTrees::Event &e : logic.trees().events())
	{
		removedNow = removedNow || (e.kind == TerrainTrees::Event::REMOVE_NOW && e.clientId == treeId);
		hidden = hidden || (e.kind == TerrainTrees::Event::HIDE_SCORCHES && e.radius == 50.0f && e.position.x == at.x && e.position.y == at.y);
	}
	CHECK(removedNow); // slot 0x58
	CHECK(hidden);     // slot 0x5C
	rig.frame(1);
	CHECK(logic.findObjectByID(effectId) == nullptr); // RW 0x62BBAB
	CHECK(logic.findObjectByID(orcId) != nullptr);
	// the owner's own clear (or owner -1) takes the cells back to 0x80, state 0, owner -1
	CHECK(TerrainTaintClear::clearRadius(logic, &at, 50.0f, 5) == 0);
	std::int32_t owner = 77;
	CHECK(taint.stateAt(at, &owner) == TaintManager::STATE_NONE);
	CHECK(owner == -1);
	CHECK(taint.valueAt(at) == 0x80);
}

TEST_CASE("taint clear retail: a TAINT object's aura clears its Range, owned by its id, when the object is deleted (RW 0x89EE67)")
{
	if (!hudtest::haveWorld("taint clear aura"))
	{
		return;
	}
	hudtest::Rig rig(hudtest::shared(), "map mp fall back 4p", "FactionAngmar");
	GameLogic &logic = rig.logic();
	TaintManager &taint = logic.taint();
	Object *source = nullptr;
	const AttributeModifierAuraUpdateModuleData *data = nullptr;
	for (const std::string &name : templatesOfKind(logic, "TAINT"))
	{
		Object *o = rig.make(name, 1000.0f, 1000.0f);
		if (!o)
		{
			continue;
		}
		if (const auto *aura = dynamic_cast<const AttributeModifierAuraUpdate *>(o->findModule("AttributeModifierAuraUpdate")))
		{
			if (aura->getModuleData() && static_cast<const AttributeModifierAuraUpdateModuleData *>(aura->getModuleData())->m_range > 0.0f)
			{
				source = o;
				data = static_cast<const AttributeModifierAuraUpdateModuleData *>(aura->getModuleData());
				break;
			}
		}
		logic.destroyObject(o);
	}
	REQUIRE(source != nullptr);
	const Coord3D at = *source->getPosition();
	const std::int32_t id = (std::int32_t)source->getID();
	taint.paint(at, data->m_range, 0x00, true, id); // as the Taint spell paints for its object
	taint.paint(at, 0.0f, 0x00, true, id + 1);      // the centre cell owned by another object: the owner's clear keeps it
	REQUIRE(taint.stateAt(at, nullptr) == TaintManager::STATE_TAINT);
	logic.destroyObject(source);
	rig.frame(1);
	Coord3D edge = at;
	edge.x = edge.x + taint.cellSize();
	std::int32_t owner = 0;
	CHECK(taint.stateAt(edge, &owner) == TaintManager::STATE_NONE); // the module's Range, owner = its id (RW 0x89EEB5)
	CHECK(owner == -1);
	CHECK(taint.valueAt(at) == 0); // another owner's cell stays
}
