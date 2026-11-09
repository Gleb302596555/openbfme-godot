// OpenBFME unit tests. GPL-3.0.
// Port 34 (issue #119): GameData/DefaultStructureRubbleHeight (+0xAE4, row RW 0xC004E0) and Object/StructureRubbleHeight
// (ThingTemplate +0x607, row RW 0xDA4688, INI::parseUnsignedByte RW 0x42EB75, getter RW 0x8C1B42).
// ActiveBody::setCorrectDamageState (RW 0x8C2A5D) for non-WALK_ON_TOP_OF_WALL STRUCTURE templates:
// - entering BODY_RUBBLE (RW 0x8C2DD4..0x8C2E13): movsx eax, byte [tt + 0x607]; > 0 uses (float)eax, else DefaultStructureRubbleHeight,
//   passed to Object::setGeometryMaxHeight (RW 0x68B2CB -> RW 0xAD2C60 + RW 0xAD2860);
// - leaving BODY_RUBBLE (RW 0x8C2D87..0x8C2DAE): (&tt->geometryInfo)->getMaxHeightAbovePosition() (RW 0xAD1920 on tt + 0xA0)
//   passed to Object::setGeometryMaxHeight (RW 0x68B2CB);
// - StructureCollapseUpdate::getCollapseHeight (RW 0x8A7A04) reads tt + 0xA0 directly so RUBBLE does not truncate collapse sinking.

#include "doctest.h"

#include "CombatTestUtil.h"
#include "Common/Thing/ThingTemplate.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Combat/CombatQueries.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/Object/ObjectGeometry.h"
#include "GameLogic/Object/PartitionManager.h"

using namespace combattest;

namespace
{
DamageInfo killHit(float amount)
{
	DamageInfo info;
	info.m_input.m_damageType = DAMAGE_SIEGE;
	info.m_input.m_deathType = DEATH_NORMAL;
	info.m_input.m_amount = amount;
	return info;
}

std::string keepStructureIni(const std::string &name, const std::string &extraFields = "")
{
	return "Object " + name + "\n"
		"  KindOf = STRUCTURE IMMOBILE SELECTABLE\n"
		"  Geometry = CYLINDER\n"
		"  GeometryMajorRadius = 30\n"
		"  GeometryHeight = 105\n" +
		extraFields +
		"  ArmorSet\n"
		"    Conditions = None\n"
		"    Armor = NoArmor\n"
		"  End\n"
		"  Body = StructureBody ModuleTag_Body\n"
		"    MaxHealth = 1000\n"
		"    MaxHealthDamaged = 500\n"
		"    MaxHealthReallyDamaged = 200\n"
		"  End\n"
		"  Behavior = KeepObjectDie ModuleTag_Keep\n"
		"  End\n"
		"End\n";
}

struct Port34World : CombatWorld
{
	explicit Port34World(const std::string &objects)
		: CombatWorld("")
	{
		const std::string err = w.load(objects, INI_LOAD_OVERWRITE, "port34.ini");
		REQUIRE_MESSAGE(err.empty(), err);
	}
};
} // namespace

TEST_CASE("PORT-34: DefaultStructureRubbleHeight lowers active shapes on entering RUBBLE and restores template height on leaving RUBBLE")
{
	Port34World w(keepStructureIni("GondorBarracksLike"));
	w.logic->settings().defaultStructureRubbleHeight = 8.0f;

	Object *b = w.unit("GondorBarracksLike", 'B', 500.0f, 500.0f);
	REQUIRE(b != nullptr);
	REQUIRE(b->getTemplate() != nullptr);
	CHECK(b->getTemplate()->structureRubbleHeight() == 0);
	CHECK_FALSE(b->hasGeometryHeights());
	CHECK(b->geometryActive().empty());
	CHECK(ObjectGeometry::maxHeightAbovePosition(b->geometryShapes()) == 105.0f);
	CHECK(CombatQueries::geometry(*b).height == 105.0f);
	CHECK(w.logic->partition().geometryOf(*b).centerZ == 52.5f);

	// 3D bounding-sphere partition query from z = 120:
	// Standing: centerZ = 52.5, sphere = hypot(30, 52.5) = 60.46693, top = 112.96693 -> distance 7.033 within radius 20.
	// Rubble (height 8): centerZ = 4.0, sphere = hypot(30, 4.0) = 30.26549, top = 34.26549 -> distance 85.734 outside radius 20.
	const Coord3D highProbe{ 500.0f, 500.0f, 120.0f };
	{
		PartitionHits hits = w.logic->partition().iterateObjectsInRange(highProbe, 20.0f, FROM_BOUNDINGSPHERE_3D, {}, ITER_FASTEST);
		REQUIRE(hits.size() == 1);
		CHECK(hits[0].object == b);
	}

	DamageInfo hit = killHit(1000.0f);
	b->attemptDamage(hit);
	REQUIRE(b->getBodyModule()->getDamageState() == BODY_RUBBLE);
	CHECK(b->hasGeometryHeights());
	CHECK(b->geometryActive().empty());
	CHECK(ObjectGeometry::maxHeightAbovePosition(b->geometryShapes()) == 8.0f);
	CHECK(CombatQueries::geometry(*b).height == 8.0f);
	CHECK(w.logic->partition().geometryOf(*b).centerZ == 4.0f);
	{
		PartitionHits hits = w.logic->partition().iterateObjectsInRange(highProbe, 20.0f, FROM_BOUNDINGSPHERE_3D, {}, ITER_FASTEST);
		CHECK(hits.empty());
	}

	// Restore health out of RUBBLE (RW 0x8C31A5 / setInitialHealth -> RW 0x8C2D87..0x8C2DAE): restores template maxHeightAbovePosition (105.0f).
	b->getBodyModule()->setInitialHealth(50);
	REQUIRE(b->getBodyModule()->getDamageState() == BODY_DAMAGED);
	CHECK(ObjectGeometry::maxHeightAbovePosition(b->geometryShapes()) == 105.0f);
	CHECK(CombatQueries::geometry(*b).height == 105.0f);
	CHECK(w.logic->partition().geometryOf(*b).centerZ == 52.5f);
	{
		PartitionHits hits = w.logic->partition().iterateObjectsInRange(highProbe, 20.0f, FROM_BOUNDINGSPHERE_3D, {}, ITER_FASTEST);
		REQUIRE(hits.size() == 1);
		CHECK(hits[0].object == b);
	}
}

TEST_CASE("PORT-34: StructureRubbleHeight overrides DefaultStructureRubbleHeight for 1..127 and falls back for 0 and >= 128 (RW 0x8C2DE0 movsx), including ChildObject and BFME1")
{
	const std::string ini =
		keepStructureIni("CustomRubble19", "  StructureRubbleHeight = 19\n") +
		keepStructureIni("CustomRubble127", "  StructureRubbleHeight = 127\n") +
		keepStructureIni("CustomRubble0", "  StructureRubbleHeight = 0\n") +
		keepStructureIni("CustomRubble128", "  StructureRubbleHeight = 128\n") +
		keepStructureIni("CustomRubble200", "  StructureRubbleHeight = 200\n") +
		"ChildObject ChildInherits19 CustomRubble19\nEnd\n"
		"ChildObject ChildResets0 CustomRubble19\n  StructureRubbleHeight = 0\nEnd\n";

	Port34World w(ini);
	w.logic->settings().defaultStructureRubbleHeight = 8.0f;

	struct Case
	{
		const char *name;
		std::int8_t expectedByte;
		float expectedRubbleHeight;
	};
	const Case cases[] = {
		{ "CustomRubble19", 19, 19.0f },
		{ "CustomRubble127", 127, 127.0f },
		{ "CustomRubble0", 0, 8.0f },
		{ "CustomRubble128", -128, 8.0f },
		{ "CustomRubble200", -56, 8.0f },
		{ "ChildInherits19", 19, 19.0f },
		{ "ChildResets0", 0, 8.0f },
	};

	float x = 150.0f;
	for (const Case &c : cases)
	{
		CAPTURE(c.name);
		Object *obj = w.unit(c.name, 'B', x, 500.0f);
		x += 90.0f;
		REQUIRE(obj != nullptr);
		CHECK(obj->getTemplate()->structureRubbleHeight() == c.expectedByte);
		DamageInfo hit = killHit(1000.0f);
		obj->attemptDamage(hit);
		REQUIRE(obj->getBodyModule()->getDamageState() == BODY_RUBBLE);
		CHECK(ObjectGeometry::maxHeightAbovePosition(obj->geometryShapes()) == c.expectedRubbleHeight);
		CHECK(CombatQueries::geometry(*obj).height == c.expectedRubbleHeight);
	}

	// BFME1 field table (Bfme1ObjectFields.inc): StructureRubbleHeight parses as S_UnsignedByte rather than S_Raw.
	movetest::MoveWorld bfme1World(40, 40, "", true);
	CombatModules::registerAll(bfme1World.w.modules);
	StructureModules::registerAll(bfme1World.w.modules);
	bfme1World.w.fx.env.blocks.registerBlock("Armor", [](INI *p) { ArmorStore::parseArmorDefinitionGlobal(p); });
	REQUIRE(bfme1World.w.load(kArmors, INI_LOAD_OVERWRITE, "bfme1_armor.ini").empty());
	REQUIRE(bfme1World.w.load(keepStructureIni("Bfme1Keep", "  StructureRubbleHeight = 14\n"), INI_LOAD_OVERWRITE, "bfme1_keep.ini").empty());
	bfme1World.buildMap();
	bfme1World.logic->settings().defaultStructureRubbleHeight = 8.0f;

	Object *b1 = bfme1World.spawn("Bfme1Keep", 200.0f, 200.0f, 0.0f, bfme1World.teamOf("Bob"));
	REQUIRE(b1 != nullptr);
	CHECK(b1->getTemplate()->bfme1Data());
	CHECK(b1->getTemplate()->structureRubbleHeight() == 14);
	DamageInfo hit = killHit(1000.0f);
	b1->attemptDamage(hit);
	REQUIRE(b1->getBodyModule()->getDamageState() == BODY_RUBBLE);
	CHECK(ObjectGeometry::maxHeightAbovePosition(b1->geometryShapes()) == 14.0f);
}

TEST_CASE("PORT-34: SPHERE mutates majorRadius while BOX and CYLINDER mutate height, and inactive shapes stay untouched (RW 0xAD2C60 + RW 0x8C2D9D)")
{
	const std::string multiIni =
		"Object MultiShapeKeep\n"
		"  KindOf = STRUCTURE IMMOBILE SELECTABLE\n"
		"  Geometry = BOX\n"
		"  GeometryName = Base\n"
		"  GeometryMajorRadius = 20\n"
		"  GeometryMinorRadius = 15\n"
		"  GeometryHeight = 60\n"
		"  GeometryIsSmall = No\n"
		"  AdditionalGeometry = BOX\n"
		"  GeometryName = UpgradeTower\n"
		"  GeometryActive = No\n"
		"  GeometryMajorRadius = 12\n"
		"  GeometryMinorRadius = 12\n"
		"  GeometryHeight = 110\n"
		"  AdditionalGeometry = SPHERE\n"
		"  GeometryName = Dome\n"
		"  GeometryActive = Yes\n"
		"  GeometryMajorRadius = 40\n"
		"  GeometryHeight = 99\n"
		"  ArmorSet\n"
		"    Conditions = None\n"
		"    Armor = NoArmor\n"
		"  End\n"
		"  Body = StructureBody ModuleTag_Body\n"
		"    MaxHealth = 1000\n"
		"    MaxHealthDamaged = 500\n"
		"    MaxHealthReallyDamaged = 200\n"
		"  End\n"
		"  Behavior = KeepObjectDie ModuleTag_Keep\n"
		"  End\n"
		"End\n";

	Port34World w(multiIni);
	w.logic->settings().defaultStructureRubbleHeight = 8.0f;

	Object *m = w.unit("MultiShapeKeep", 'B', 400.0f, 400.0f);
	REQUIRE(m != nullptr);
	// Standing: active Base (height 60, circle hypot(20,15)=25) + inactive UpgradeTower (110) + active Dome (SPHERE majorRadius 40).
	CHECK(ObjectGeometry::maxHeightAbovePosition(m->geometryShapes()) == 60.0f);
	CHECK(CombatQueries::boundingCircleRadius(*m) == 40.0f);

	// Enter RUBBLE: active Base height -> 8, active Dome majorRadius -> 8 (height stays 99), inactive UpgradeTower height stays 110!
	DamageInfo hit = killHit(1000.0f);
	m->attemptDamage(hit);
	REQUIRE(m->getBodyModule()->getDamageState() == BODY_RUBBLE);
	const std::vector<ObjectGeometry::Shape> &rubbleShapes = m->geometryShapes();
	REQUIRE(rubbleShapes.size() == 3);
	CHECK(rubbleShapes[0].active);
	CHECK(rubbleShapes[0].height == 8.0f);
	CHECK(rubbleShapes[0].majorRadius == 20.0f);
	CHECK_FALSE(rubbleShapes[1].active);
	CHECK(rubbleShapes[1].height == 110.0f);
	CHECK(rubbleShapes[2].active);
	CHECK(rubbleShapes[2].majorRadius == 8.0f);
	CHECK(rubbleShapes[2].height == 99.0f);
	CHECK(ObjectGeometry::maxHeightAbovePosition(rubbleShapes) == 8.0f);
	// With SPHERE majorRadius lowered from 40 to 8, Base's hypot(20, 15) == 25 now dominates the bounding circle (RW 0xAD2860).
	CHECK(CombatQueries::boundingCircleRadius(*m) == 25.0f);

	// GeometryUpgrade-activated shape before RUBBLE preserves its active flag across RUBBLE -> repair.
	Object *upgraded = w.unit("MultiShapeKeep", 'B', 600.0f, 400.0f);
	REQUIRE(upgraded != nullptr);
	upgraded->setGeometryActive("UpgradeTower", true);
	upgraded->setGeometryActive("Dome", false);
	CHECK(ObjectGeometry::maxHeightAbovePosition(upgraded->geometryShapes()) == 110.0f);

	upgraded->attemptDamage(hit);
	REQUIRE(upgraded->getBodyModule()->getDamageState() == BODY_RUBBLE);
	CHECK(ObjectGeometry::maxHeightAbovePosition(upgraded->geometryShapes()) == 8.0f);
	CHECK(upgraded->geometryShapes()[1].active);
	CHECK_FALSE(upgraded->geometryShapes()[2].active);

	// Leaving RUBBLE (RW 0x8C2D9D..0x8C2DAE) reads template's maxHeightAbovePosition (60.0f on template's default active shapes)
	// and preserves the object's GeometryUpgrade active flags.
	upgraded->getBodyModule()->setInitialHealth(100);
	REQUIRE(upgraded->getBodyModule()->getDamageState() == BODY_PRISTINE);
	CHECK(upgraded->geometryShapes()[1].active);
	CHECK_FALSE(upgraded->geometryShapes()[2].active);
	CHECK(ObjectGeometry::maxHeightAbovePosition(upgraded->geometryShapes()) == 60.0f);
}

TEST_CASE("PORT-34: initial RUBBLE spawn, StructureCollapseUpdate sink height, setGeometryInfoFrom, the WALK_ON_TOP_OF_WALL branch, and snapshot xfer v5")
{
	const std::string ini =
		keepStructureIni("PreRuinedKeep", "  StructureRubbleHeight = 11\n") +
		"Object InitialZeroKeep\n"
		"  KindOf = STRUCTURE IMMOBILE SELECTABLE\n"
		"  Geometry = CYLINDER\n"
		"  GeometryMajorRadius = 25\n"
		"  GeometryHeight = 90\n"
		"  ArmorSet\n"
		"    Conditions = None\n"
		"    Armor = NoArmor\n"
		"  End\n"
		"  Body = StructureBody ModuleTag_Body\n"
		"    MaxHealth = 500\n"
		"    InitialHealth = 0\n"
		"  End\n"
		"  Behavior = KeepObjectDie ModuleTag_Keep\n"
		"  End\n"
		"End\n"
		"Object CollapsingTower\n"
		"  KindOf = STRUCTURE IMMOBILE SELECTABLE\n"
		"  Geometry = CYLINDER\n"
		"  GeometryMajorRadius = 20\n"
		"  GeometryHeight = 100\n"
		"  ArmorSet\n"
		"    Conditions = None\n"
		"    Armor = NoArmor\n"
		"  End\n"
		"  Body = StructureBody ModuleTag_Body\n"
		"    MaxHealth = 400\n"
		"  End\n"
		"  Behavior = StructureCollapseUpdate ModuleTag_Collapse\n"
		"    MinCollapseDelay = 0\n"
		"    MaxCollapseDelay = 0\n"
		"    CollapseDamping = 0.0\n"
		"    MaxShudder = 0.0\n"
		"    MinBurstDelay = 99999\n"
		"    MaxBurstDelay = 99999\n"
		"    BigBurstFrequency = 0\n"
		"    DestroyObjectWhenDone = Yes\n"
		"    CollapseHeight = 0\n"
		"  End\n"
		"End\n"
		"Object WallSegmentLike\n"
		"  KindOf = STRUCTURE IMMOBILE WALK_ON_TOP_OF_WALL\n"
		"  Geometry = BOX\n"
		"  GeometryMajorRadius = 20\n"
		"  GeometryMinorRadius = 10\n"
		"  GeometryHeight = 65\n"
		"  ArmorSet\n"
		"    Conditions = None\n"
		"    Armor = NoArmor\n"
		"  End\n"
		"  Body = StructureBody ModuleTag_Body\n"
		"    MaxHealth = 400\n"
		"  End\n"
		"  Behavior = KeepObjectDie ModuleTag_Keep\n"
		"  End\n"
		"End\n"
		"Object DummyHole\n"
		"  KindOf = STRUCTURE IMMOBILE\n"
		"  Geometry = CYLINDER\n"
		"  GeometryMajorRadius = 5\n"
		"  GeometryHeight = 5\n"
		"  Body = StructureBody ModuleTag_Body\n"
		"    MaxHealth = 200\n"
		"  End\n"
		"End\n";

	Port34World w(ini);
	w.logic->settings().defaultStructureRubbleHeight = 8.0f;

	// 1) InitialHealth = 0 spawns directly in BODY_RUBBLE (RW 0x8C39D0), lowers height immediately, and attemptHealing leaves RUBBLE -> 90.0f.
	Object *zero = w.unit("InitialZeroKeep", 'B', 150.0f, 200.0f);
	REQUIRE(zero != nullptr);
	CHECK(zero->getBodyModule()->getDamageState() == BODY_RUBBLE);
	CHECK(ObjectGeometry::maxHeightAbovePosition(zero->geometryShapes()) == 8.0f);
	zero->attemptHealing(500.0f, nullptr);
	CHECK(zero->getBodyModule()->getDamageState() == BODY_PRISTINE);
	CHECK(ObjectGeometry::maxHeightAbovePosition(zero->geometryShapes()) == 90.0f);

	// 2) WALK_ON_TOP_OF_WALL takes the wall branch (RW 0x8C2A8C .. 0x8C2AC9): its rubble is GameData's height (RW 0x8C2AC4),
	//    not the template's 65 — the port dropped that branch until PORT-34 r2.
	Object *wall = w.unit("WallSegmentLike", 'B', 300.0f, 200.0f);
	REQUIRE(wall != nullptr);
	DamageInfo wallHit = killHit(400.0f);
	wall->attemptDamage(wallHit);
	REQUIRE(wall->getBodyModule()->getDamageState() == BODY_RUBBLE);
	CHECK(wall->hasGeometryHeights());
	CHECK(ObjectGeometry::maxHeightAbovePosition(wall->geometryShapes()) == 8.0f);

	// 3) StructureCollapseUpdate reads template height (100.0f, RW 0x8A7A04 on tt + 0xA0) instead of stopping at rubble height (8.0f).
	Object *tower = w.unit("CollapsingTower", 'B', 450.0f, 200.0f);
	REQUIRE(tower != nullptr);
	const ObjectID towerId = tower->getID();
	DamageInfo towerHit = killHit(400.0f);
	tower->attemptDamage(towerHit);
	REQUIRE(tower->getBodyModule()->getDamageState() == BODY_RUBBLE);
	CHECK(ObjectGeometry::maxHeightAbovePosition(tower->geometryShapes()) == 8.0f);
	// Under gravity -2.56/frame^2, reaching -8 takes 3 frames (vel -2.56, -5.12, -7.68 -> sum -15.36); reaching -100 takes 9 frames (-2.56 * 45 = -115.2).
	w.frames(5);
	REQUIRE(w.byId(towerId) != nullptr); // still sinking past -8 because collapse height is 100, not 8!
	w.frames(6);
	CHECK(w.byId(towerId) == nullptr); // finished full 100-unit collapse and destroyed

	// 4) setGeometryInfoFrom (RW 0x699349, RebuildHoleExposeDie) copies rubble heights while keeping geometryActive() empty.
	Object *ruin = w.unit("PreRuinedKeep", 'B', 600.0f, 200.0f);
	Object *standing = w.unit("PreRuinedKeep", 'A', 750.0f, 200.0f);
	Object *hole = w.unit("DummyHole", 'B', 600.0f, 350.0f);
	REQUIRE(ruin != nullptr);
	REQUIRE(standing != nullptr);
	REQUIRE(hole != nullptr);

	DamageInfo ruinHit = killHit(1000.0f);
	ruin->attemptDamage(ruinHit);
	REQUIRE(ruin->getBodyModule()->getDamageState() == BODY_RUBBLE);
	CHECK(ObjectGeometry::maxHeightAbovePosition(ruin->geometryShapes()) == 11.0f);

	hole->setGeometryInfoFrom(*ruin);
	CHECK(hole->geometryActive().empty());
	CHECK(hole->hasGeometryHeights());
	CHECK(ObjectGeometry::maxHeightAbovePosition(hole->geometryShapes()) == 11.0f);

	// 5) Snapshot round-trip (Object::xfer v5) preserves untouched standing object, rubble heights, partition bounds, hash, and post-load repair.
	const ObjectID ruinId = ruin->getID();
	const ObjectID standingId = standing->getID();
	const std::uint32_t savedHash = w.logic->computeStateHash();
	std::vector<std::uint8_t> saved;
	std::string xferError;
	REQUIRE_MESSAGE(w.logic->saveSnapshot(saved, &xferError), xferError);

	// Advance uninterrupted timeline: restore ruin out of RUBBLE and run 4 frames.
	ruin->getBodyModule()->setInitialHealth(50);
	std::vector<std::uint32_t> expectedHashes;
	for (int i = 0; i < 4; ++i)
	{
		w.frames(1);
		expectedHashes.push_back(w.logic->computeStateHash());
	}

	// Restore snapshot and check exact state + byte-identical re-save + identical post-load timeline.
	REQUIRE_MESSAGE(w.logic->loadSnapshot(saved, &xferError), xferError);
	CHECK(w.logic->computeStateHash() == savedHash);

	std::vector<std::uint8_t> resaved;
	REQUIRE_MESSAGE(w.logic->saveSnapshot(resaved, &xferError), xferError);
	CHECK(resaved == saved);

	Object *loadedRuin = w.byId(ruinId);
	Object *loadedStanding = w.byId(standingId);
	REQUIRE(loadedRuin != nullptr);
	REQUIRE(loadedStanding != nullptr);
	CHECK_FALSE(loadedStanding->hasGeometryHeights());
	CHECK(ObjectGeometry::maxHeightAbovePosition(loadedStanding->geometryShapes()) == 105.0f);
	CHECK(loadedRuin->hasGeometryHeights());
	CHECK(ObjectGeometry::maxHeightAbovePosition(loadedRuin->geometryShapes()) == 11.0f);
	CHECK(CombatQueries::geometry(*loadedRuin).height == 11.0f);
	CHECK(w.logic->partition().geometryOf(*loadedRuin).centerZ == 5.5f);

	loadedRuin->getBodyModule()->setInitialHealth(50);
	CHECK(ObjectGeometry::maxHeightAbovePosition(loadedRuin->geometryShapes()) == 105.0f);
	for (size_t i = 0; i < expectedHashes.size(); ++i)
	{
		w.frames(1);
		CHECK(w.logic->computeStateHash() == expectedHashes[i]);
	}
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Port 34 r2 (issue #119): the WALK_ON_TOP_OF_WALL branch of RW 0x8C2A5D (RW 0x8C2A8C .. 0x8C2AC9), the part main's Port 34 left out.
// ---------------------------------------------------------------------------------------------------------------------------------
namespace
{
// a wall: a STRUCTURE that is WALK_ON_TOP_OF_WALL with a template StructureRubbleHeight of its own
std::string wallIni(const std::string &name, int rubbleHeight = 0)
{
	std::string ini = "Object " + name + "\n"
		"  KindOf = STRUCTURE IMMOBILE SELECTABLE WALK_ON_TOP_OF_WALL\n"
		"  Geometry = BOX\n"
		"  GeometryMajorRadius = 10\n"
		"  GeometryMinorRadius = 10\n"
		"  GeometryHeight = 10\n"
		"  GeometryIsSmall = No\n";
	if (rubbleHeight > 0)
	{
		ini += "  StructureRubbleHeight = " + std::to_string(rubbleHeight) + "\n";
	}
	// an inactive second shape: the RW 0x699349 way out must restore the template's flag too, not just the height
	ini +=
		"  AdditionalGeometry = CYLINDER\n"
		"  GeometryName = TowerTop\n"
		"  GeometryActive = No\n"
		"  GeometryMajorRadius = 7\n"
		"  GeometryHeight = 30\n"
		"  ArmorSet\n"
		"    Conditions = None\n"
		"    Armor = NoArmor\n"
		"  End\n"
		"  Body = StructureBody ModuleTag_Body\n"
		"    MaxHealth = 1000\n"
		"    MaxHealthDamaged = 500\n"
		"    MaxHealthReallyDamaged = 200\n"
		"  End\n"
		"  Behavior = KeepObjectDie ModuleTag_Keep\n"
		"  End\n"
		"End\n";
	return ini;
}
} // namespace

TEST_CASE("PORT-34 r2: a destroyed WALK_ON_TOP_OF_WALL structure drops to DefaultStructureRubbleHeight even with a template StructureRubbleHeight (RW 0x8C2A99 -> 0x8C2AC4)")
{
	Port34World w(wallIni("WallWithRubble41", 41) + wallIni("WallNoRubble"));
	w.logic->settings().defaultStructureRubbleHeight = 8.0f;

	Object *wall = w.unit("WallWithRubble41", 'B', 500.0f, 500.0f);
	Object *plainWall = w.unit("WallNoRubble", 'B', 700.0f, 500.0f);
	REQUIRE(wall != nullptr);
	REQUIRE(plainWall != nullptr);
	CHECK(wall->isKindOf((unsigned)CombatNames::kindOf("WALK_ON_TOP_OF_WALL")));
	CHECK(wall->isKindOf((unsigned)CombatNames::kinds().structure));
	// the template's byte is there — it is just not what a wall's rubble uses
	CHECK(wall->getTemplate()->structureRubbleHeight() == 41);
	CHECK(ObjectGeometry::maxHeightAbovePosition(wall->geometryShapes()) == 10.0f);

	DamageInfo wallHit = killHit(1000.0f);
	wall->attemptDamage(wallHit);
	DamageInfo plainWallHit = killHit(1000.0f);
	plainWall->attemptDamage(plainWallHit);
	REQUIRE(wall->getBodyModule()->getDamageState() == BODY_RUBBLE);
	REQUIRE(plainWall->getBodyModule()->getDamageState() == BODY_RUBBLE);

	// RW 0x8C2AC4 calls 0x68B2CB with GlobalData + 0xAE4: 8.0f, not the template's 41
	CHECK(wall->hasGeometryHeights());
	CHECK(ObjectGeometry::maxHeightAbovePosition(wall->geometryShapes()) == 8.0f);
	CHECK(CombatQueries::geometry(*wall).height == 8.0f);
	CHECK(ObjectGeometry::maxHeightAbovePosition(plainWall->geometryShapes()) == 8.0f);

	// the wall branch sets no template byte: a second wall finds its template untouched at 41
	Object *second = w.unit("WallWithRubble41", 'B', 900.0f, 500.0f);
	REQUIRE(second != nullptr);
	CHECK(second->getTemplate() == wall->getTemplate());
	CHECK(second->getTemplate()->structureRubbleHeight() == 41);
	CHECK(ObjectGeometry::maxHeightAbovePosition(second->geometryShapes()) == 10.0f);
}

TEST_CASE("PORT-34 r2: a WALK_ON_TOP_OF_WALL structure leaves RUBBLE through RW 0x699349 with the template GeometryInfo (RW 0x8C2A9F -> 0x8C2AAC)")
{
	Port34World w(wallIni("WallRepairable", 41));
	w.logic->settings().defaultStructureRubbleHeight = 8.0f;

	Object *wall = w.unit("WallRepairable", 'B', 500.0f, 500.0f);
	REQUIRE(wall != nullptr);
	REQUIRE(ObjectGeometry::cachedShapesOf(*wall->getTemplate()).size() == 2);
	const float templateTop = ObjectGeometry::maxHeightAbovePosition(ObjectGeometry::cachedShapesOf(*wall->getTemplate()));
	CHECK(templateTop == 10.0f);
	CHECK_FALSE(wall->hasGeometryHeights());
	CHECK(wall->geometryActive().empty());

	// a wall's own flags and heights before it is destroyed: RW 0x699349 takes the template's whole GeometryInfo back, so these must go
	wall->setGeometryMaxHeight(25.0f);
	wall->setGeometryActive("TowerTop", true);
	REQUIRE(wall->hasGeometryHeights());
	REQUIRE(wall->geometryActive().size() == 2);
	CHECK(wall->geometryActive()[1] == 1);
	CHECK(wall->keepsOwnGeometry());
	// RW 0xAD2C6A skips an inactive shape, so the newly active TowerTop keeps its template extent (30) and the base takes 25
	CHECK(ObjectGeometry::maxHeightAbovePosition(wall->geometryShapes()) == 30.0f);

	DamageInfo wallHit = killHit(1000.0f);
	wall->attemptDamage(wallHit);
	REQUIRE(wall->getBodyModule()->getDamageState() == BODY_RUBBLE);
	CHECK(ObjectGeometry::maxHeightAbovePosition(wall->geometryShapes()) == 8.0f);

	// repair out of RUBBLE: RW 0x699349 (the template's GeometryInfo inside the object), not 0xAD1920 + 0x68B2CB
	wall->getBodyModule()->setInitialHealth(50);
	REQUIRE(wall->getBodyModule()->getDamageState() == BODY_DAMAGED);
	CHECK(ObjectGeometry::maxHeightAbovePosition(wall->geometryShapes()) == 10.0f);
	CHECK(CombatQueries::geometry(*wall).height == 10.0f);
	CHECK_FALSE(wall->hasGeometryHeights());
	CHECK(wall->geometryActive().empty()); // the template's GeometryInfo carries no per-object flag overrides either
	CHECK_FALSE(wall->keepsOwnGeometry());
	CHECK(ObjectGeometry::cachedShapesOf(*wall->getTemplate()).size() == 2);
}

TEST_CASE("PORT-34 r2: a non-wall structure still takes its template StructureRubbleHeight when > 0, else DefaultStructureRubbleHeight (RW 0x8C2DDA)")
{
	Port34World w(keepStructureIni("KeepRubble33", "  StructureRubbleHeight = 33\n") + keepStructureIni("KeepRubble0"));
	w.logic->settings().defaultStructureRubbleHeight = 8.0f;

	Object *withByte = w.unit("KeepRubble33", 'B', 500.0f, 500.0f);
	Object *withoutByte = w.unit("KeepRubble0", 'B', 700.0f, 500.0f);
	REQUIRE(withByte != nullptr);
	REQUIRE(withoutByte != nullptr);
	CHECK_FALSE(withByte->isKindOf((unsigned)CombatNames::kindOf("WALK_ON_TOP_OF_WALL")));

	DamageInfo withByteHit = killHit(1000.0f);
	withByte->attemptDamage(withByteHit);
	DamageInfo withoutByteHit = killHit(1000.0f);
	withoutByte->attemptDamage(withoutByteHit);
	REQUIRE(withByte->getBodyModule()->getDamageState() == BODY_RUBBLE);
	REQUIRE(withoutByte->getBodyModule()->getDamageState() == BODY_RUBBLE);
	CHECK(ObjectGeometry::maxHeightAbovePosition(withByte->geometryShapes()) == 33.0f);
	CHECK(ObjectGeometry::maxHeightAbovePosition(withoutByte->geometryShapes()) == 8.0f);

	// out of RUBBLE the non-wall takes the template's max height (RW 0x8C2D87 -> RW 0xAD1920), not 0x699349: its own extents stay its own
	withByte->getBodyModule()->setInitialHealth(50);
	REQUIRE(withByte->getBodyModule()->getDamageState() == BODY_DAMAGED);
	CHECK(ObjectGeometry::maxHeightAbovePosition(withByte->geometryShapes()) == 105.0f);
	CHECK(withByte->hasGeometryHeights());
}
