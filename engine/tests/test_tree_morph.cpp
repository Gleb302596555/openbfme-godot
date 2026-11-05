// OpenBFME unit tests. GPL-3.0.
// The Taint / Elven Wood tree morph (TerrainTrees.h, stop S-921) on retail data: RW 0x67F6F0 starts the morph of the trees within the radius (+ 7) that
// the type takes (Taint: not a TaintedTree; Elven Wood: a TaintedTree), a toppled one is harvested of its whole value (RW 0x67F738); TheTerrainLogic's
// update counts MorphTime down (RW 0x4EACB0) and a tree without a MorphTree target goes at the end (RW 0x684412); a morphing tree refuses the topple
// (RW 0x4E9E4E). Expectations from the RW bodies.

#include "doctest.h"
#include "HudTestUtil.h"

#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DTreeDraw.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Map/TerrainTrees.h"

#include <cstdint>

namespace
{
// the first live record whose type is (or is not) a TaintedTree and has no MorphTree target
TerrainTrees::Record *treeOf(GameLogic &logic, bool tainted)
{
	for (const TerrainTrees::Record &t : logic.trees().records())
	{
		const W3DTreeDrawModuleData *d = TerrainTrees::treeDrawOf(t.drawTmpl);
		if (t.clientId != 0 && d && d->m_taintedTree == tainted && !logic.trees().morphTargetOf(t.tmpl))
		{
			return const_cast<TerrainTrees::Record *>(&t);
		}
	}
	return nullptr;
}

size_t countEvents(GameLogic &logic, TerrainTrees::Event::Kind kind, std::uint32_t id)
{
	size_t n = 0;
	for (const TerrainTrees::Event &e : logic.trees().events())
	{
		n += (e.kind == kind && e.clientId == id) ? 1u : 0u;
	}
	return n;
}
} // namespace

TEST_CASE("tree morph retail: Taint morphs a living tree, which goes after MorphTime without a target (RW 0x67F6F0, 0x4EACB0, 0x684412)")
{
	if (!hudtest::haveWorld("tree morph"))
	{
		return;
	}
	hudtest::Rig rig(hudtest::shared(), "map mp fall back 4p", "FactionAngmar");
	GameLogic &logic = rig.logic();
	TerrainTrees &trees = logic.trees();
	TerrainTrees::Record *tree = treeOf(logic, false);
	REQUIRE(tree != nullptr);
	const std::uint32_t id = tree->clientId;
	const Coord3D at = tree->position;
	const std::int32_t morphTime = (std::int32_t)TerrainTrees::treeDrawOf(tree->drawTmpl)->m_morphTime;
	REQUIRE(morphTime > 1); // tolerance: MorphTime is the data's (a duration in logic frames); the count down needs at least two frames to look at
	// Elven Wood (type 2) passes a tree that is not tainted (RW 0x4EC45D)
	CHECK(trees.morphTreesInRadius(at, 0.0f, 2) == 0);
	CHECK(tree->morph == 0);
	// Taint (type 1): the tree within radius 0 + 7 morphs, without model B
	CHECK(trees.morphTreesInRadius(at, 0.0f, 1) >= 1); // tolerance: the other trees within 7 of this one also start; the count depends on the map, this tree is checked below
	CHECK(tree->morph == 1);
	CHECK(tree->morphLeft == morphTime);
	CHECK_FALSE(tree->morphTarget);
	CHECK(countEvents(logic, TerrainTrees::Event::MORPH, id) == 1);
	// a second spell does not restart it (RW 0x4EC910)
	trees.morphTreesInRadius(at, 0.0f, 1);
	CHECK(countEvents(logic, TerrainTrees::Event::MORPH, id) == 1);
	// a morphing tree refuses the topple (RW 0x4E9EAD)
	trees.toppleTree(*tree, Coord3D{ at.x + 10.0f, at.y, at.z }, nullptr);
	CHECK_FALSE(tree->toppled);
	// TheTerrainLogic's update counts down once per logic frame
	rig.frame(1);
	CHECK(tree->morphLeft == morphTime - 1);
	rig.frame((int)(morphTime - 2));
	CHECK(tree->clientId == id);
	CHECK(tree->morph == 1);
	rig.frame(1); // the last frame: no target, the tree goes at once (slot 0x58)
	CHECK(countEvents(logic, TerrainTrees::Event::MORPH_END, id) == 1);
	CHECK(countEvents(logic, TerrainTrees::Event::REMOVE_NOW, id) == 1);
	CHECK(tree->clientId == 0);
	CHECK(trees.morphing() == 0);
}

TEST_CASE("tree morph retail: a toppled tree is harvested of its whole value instead (RW 0x4EC486 -> 0x67F738)")
{
	if (!hudtest::haveWorld("tree morph toppled"))
	{
		return;
	}
	hudtest::Rig rig(hudtest::shared(), "map mp fall back 4p", "FactionAngmar");
	GameLogic &logic = rig.logic();
	TerrainTrees &trees = logic.trees();
	TerrainTrees::Record *tree = treeOf(logic, false);
	REQUIRE(tree != nullptr);
	const std::uint32_t id = tree->clientId;
	const Coord3D at = tree->position;
	trees.toppleTree(*tree, Coord3D{ at.x + 10.0f, at.y, at.z }, nullptr);
	REQUIRE(tree->toppled);
	CHECK(trees.morphTreesInRadius(at, 0.0f, 1) == 0);
	CHECK(countEvents(logic, TerrainTrees::Event::REMOVE, id) == 1); // the lumber is gone: the tree sinks (slot 0x7C)
	CHECK(tree->clientId == 0);
	CHECK(countEvents(logic, TerrainTrees::Event::MORPH, id) == 0);
}
