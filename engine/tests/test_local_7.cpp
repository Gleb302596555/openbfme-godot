// OpenBFME unit tests: LOCAL-7 AttackContactPoint (issue #112, TOP20 #26).
// RotWK addresses:
//   Parser row RW 0xDA44C8 (name RW 0xC25744 "AttackContactPoint", parser RW 0xAD3D30, ThingTemplate + 0xA0)
//   GeometryInfo::getBestContactPoint RW 0xAD30E0
//   Object::getWorldspaceBestContactPoint RW 0x690BD2
//   Pathfinder::adjustMeleeOffset RW 0x6EA0EC / 0x6EA11E

#include "doctest.h"

#include "CombatTestUtil.h"

#include "Common/ArchiveFileSystem.h"
#include "Common/BfmeGame.h"
#include "Common/INI.h"
#include "Common/Thing/ThingFactory.h"
#include "Common/Thing/ThingTemplate.h"
#include "GameLogic/AI/AIMove.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/Object/ObjectGeometry.h"

#include <cmath>
#include <cstdlib>
#include <string>

using namespace combattest;

namespace
{
std::string makeWall(const std::string &name, const std::string &contactRows)
{
	return "Object " + name + "\n  KindOf = STRUCTURE IMMOBILE WALK_ON_TOP_OF_WALL\n"
		"  Geometry = BOX\n"
		"  GeometryMajorRadius = 50\n"
		"  GeometryMinorRadius = 20\n"
		"  GeometryHeight = 40\n" +
		contactRows +
		"  ArmorSet\n"
		"    Conditions = None\n"
		"    Armor = PlainArmor\n"
		"  End\n"
		"  Body = ActiveBody ModuleTag_Body\n"
		"    MaxHealth = 100000\n"
		"  End\n"
		"End\n";
}

std::string makeAttackerWithContactLabel(const std::string &name, const std::string &label)
{
	return "Object " + name + "\n"
		"  KindOf = INFANTRY SELECTABLE CAN_ATTACK SCORE\n"
		"  VisionRange = 150\n"
		"  Geometry = CYLINDER\n"
		"  GeometryMajorRadius = 8\n"
		"  GeometryMinorRadius = 8\n"
		"  GeometryHeight = 20\n"
		"  ArmorSet\n"
		"    Conditions = None\n"
		"    Armor = PlainArmor\n"
		"  End\n"
		"  WeaponSet\n"
		"    Conditions = None\n"
		"    Weapon = PRIMARY SwordWeapon\n"
		"  End\n"
		"  Body = ActiveBody ModuleTag_Body\n"
		"    MaxHealth = 100\n"
		"  End\n"
		"  Behavior = AIUpdateInterface ModuleTag_AI\n"
		"    AutoAcquireEnemiesWhenIdle = Yes\n"
		"    MoodAttackCheckRate = 400\n"
		"    SpecialContactPoints = " + label + "\n"
		"  End\n"
		"  Behavior = PhysicsBehavior ModuleTag_Physics\n"
		"  End\n"
		"  LocomotorSet\n"
		"    Locomotor = WalkerLoco\n"
		"    Condition = SET_NORMAL\n"
		"    Speed = 55\n"
		"  End\n"
		"End\n";
}
} // namespace

TEST_CASE("LOCAL-7 attack contact point: parsing AttackContactPoint and SwoopContactPoint into template contact points")
{
	CombatWorld w;
	const std::string ini =
		"Object TestWallPoints\n"
		"  Geometry = BOX\n"
		"  GeometryMajorRadius = 50\n"
		"  GeometryMinorRadius = 20\n"
		"  GeometryHeight = 40\n"
		"  AttackContactPoint = X:11.725 Y:-53.381 Z:0\n"
		"  AttackContactPoint = X:11.68 Y:56.752 Z:0\n"
		"  SwoopContactPoint = X:29.241 Y:-36.937 Z:104.972 Swoop\n"
		"  Body = ActiveBody ModuleTag_Body\n"
		"    MaxHealth = 100\n"
		"  End\n"
		"End\n";
	std::string err = w.w.load(ini, INI_LOAD_OVERWRITE, "test_points.ini");
	REQUIRE_MESSAGE(err.empty(), err);

	const ThingTemplate *tt = w.w.things.findTemplate("TestWallPoints");
	REQUIRE(tt != nullptr);

	std::vector<ObjectGeometry::ContactPoint> pts = ObjectGeometry::contactPointsOf(*tt);
	REQUIRE(pts.size() == 3);

	// tolerance: float coordinate parsed from INI text
	CHECK(pts[0].x == doctest::Approx(11.725f).epsilon(1e-4));
	// tolerance: float coordinate parsed from INI text
	CHECK(pts[0].y == doctest::Approx(-53.381f).epsilon(1e-4));
	// tolerance: float coordinate parsed from INI text
	CHECK(pts[0].z == doctest::Approx(0.0f).epsilon(1e-4));
	CHECK(pts[0].label == "");

	// tolerance: float coordinate parsed from INI text
	CHECK(pts[1].x == doctest::Approx(11.68f).epsilon(1e-4));
	// tolerance: float coordinate parsed from INI text
	CHECK(pts[1].y == doctest::Approx(56.752f).epsilon(1e-4));
	// tolerance: float coordinate parsed from INI text
	CHECK(pts[1].z == doctest::Approx(0.0f).epsilon(1e-4));
	CHECK(pts[1].label == "");

	// tolerance: float coordinate parsed from INI text
	CHECK(pts[2].x == doctest::Approx(29.241f).epsilon(1e-4));
	// tolerance: float coordinate parsed from INI text
	CHECK(pts[2].y == doctest::Approx(-36.937f).epsilon(1e-4));
	// tolerance: float coordinate parsed from INI text
	CHECK(pts[2].z == doctest::Approx(104.972f).epsilon(1e-4));
	CHECK(pts[2].label == "Swoop");
}

#include "GameLogic/AI/AIPathfind.h"
#include "GameLogic/AI/AIWorld.h"

namespace
{
class FixedRange : public PathfindAttackRange
{
public:
	FixedRange(Coord3D centre, float reach) : m_centre(centre), m_reach(reach) {}
	bool inRangeFrom(const Coord3D &from, float extra) const override
	{
		if (m_reach <= 0.0f) return false;
		const float dx = from.x - m_centre.x, dy = from.y - m_centre.y;
		return dx * dx + dy * dy + extra * extra <= m_reach * m_reach;
	}
	bool inRangeFromTo(const Coord3D &from, const Coord3D &victimPos, float extra) const override
	{
		if (m_reach <= 0.0f) return false;
		const float dx = from.x - victimPos.x, dy = from.y - victimPos.y;
		return dx * dx + dy * dy + extra * extra <= m_reach * m_reach;
	}
	float attackDistance() const override { return m_reach; }
	bool meleeWeapon() const override { return true; }

private:
	Coord3D m_centre;
	float m_reach;
};
} // namespace

TEST_CASE("LOCAL-7 attack contact point: attackers choose contact points matching their SpecialContactPoints, fallback to center without match")
{
	CombatWorld w;
	w.combat().setAutoAcquireEnabled(false);
	const std::string ini =
		makeWall("ContactWall", "  AttackContactPoint = X:-30 Y:0 Z:0 Ram\n  AttackContactPoint = X:30 Y:0 Z:0\n") +
		makeWall("PlainWall", "") +
		makeAttackerWithContactLabel("RamUnit", "Ram") +
		makeAttackerWithContactLabel("BombUnit", "Bomb") +
		makeAttackerWithContactLabel("SwordsmanUnit", "");
	std::string err = w.w.load(ini, INI_LOAD_OVERWRITE, "local7_walls.ini");
	REQUIRE_MESSAGE(err.empty(), err);

	Object *contactWall = w.unit("ContactWall", 'B', 200.0f, 200.0f);
	REQUIRE(contactWall != nullptr);
	Object *plainWall = w.unit("PlainWall", 'B', 200.0f, 200.0f);
	REQUIRE(plainWall != nullptr);
	Object *nearWall = w.unit("ContactWall", 'B', 60.0f, 60.0f);
	REQUIRE(nearWall != nullptr);
	Object *targetUnit = w.unit("SwordsmanUnit", 'B', 200.0f, 200.0f);
	REQUIRE(targetUnit != nullptr);

	Object *atkRam = w.unit("RamUnit", 'A', 100.0f, 200.0f);
	REQUIRE(atkRam != nullptr);
	Object *atkPlain = w.unit("SwordsmanUnit", 'A', 100.0f, 200.0f);
	REQUIRE(atkPlain != nullptr);
	w.frames(2);

	Pathfinder &pf = w.ai->pathfinder();
	const PathfindObject *ramPf = w.ai->findObjectByID(atkRam->getID());
	REQUIRE(ramPf != nullptr);
	const PathfindObject *swordsmanPf = w.ai->findObjectByID(atkPlain->getID());
	REQUIRE(swordsmanPf != nullptr);
	const PathfindObject *wallPf = w.ai->findObjectByID(contactWall->getID());
	REQUIRE(wallPf != nullptr);
	const PathfindObject *unitPf = w.ai->findObjectByID(targetUnit->getID());
	REQUIRE(unitPf != nullptr);

	SUBCASE("unit WITH SpecialContactPoints = Ram attacks building with matching Ram AttackContactPoint -> RW adds the WORLD contact point to the centre")
	{
		Coord3D ramDest = *contactWall->getPosition();
		const FixedRange reachWall(*contactWall->getPosition(), 50.0f);
		const PathfindLocomotorInfo locoRam = atkRam->getAIUpdateInterface()->locomotorInfo();

		// RW 0x6F37B3 (Pathfinder::findMeleeEngagementLocation): the target is STRUCTURE / WALK_ON_TOP_OF_WALL -> adjustMeleeOffset (RW 0x6EA0EC).
		// RW 0x6EA11E -> RW 0x667C76 finds the "Ram" point: RW 0x690BD2 writes the WORLD point (170, 200, 0) into v and RW 0x6EA0EC exits at RW 0x6EA1D9.
		// PROCESS 14 expectation change (review r3, Claude): r3 expected the contact point itself (170.5, 200.5), which needs v = point - centre; RW
		// 0x6F3927 .. 0x6F3974 adds v to the centre (RW 0x68C9AC) as is: (200, 200) + (170, 200) + 5 = (375, 405), out of reach; the RW 0x6EFE9F ring
		// search then gives the cell (180.5, 200.5), not the contact point.
		REQUIRE(pf.findMeleeEngagementLocation(*ramPf, locoRam, *wallPf, reachWall, &ramDest));
		CHECK(ramDest.x == 180.5f);
		CHECK(ramDest.y == 200.5f);

		const PathfindObject *nearPf = w.ai->findObjectByID(nearWall->getID());
		REQUIRE(nearPf != nullptr);
		Coord3D ramNear = *nearWall->getPosition();
		Coord3D swordNear = *nearWall->getPosition();
		const FixedRange reachNear(*nearWall->getPosition(), 50.0f);
		// the same wall at (60, 60): centre + world point + 5 = (60, 60) + (30, 60) + 5 = (95, 125), in the search's reach, so the ram's cell (70.5, 100.5)
		// differs from the swordsman's (no labels, RW 0x6EA12B steps: (60.5, 60.5)); with v = point - centre it would be the contact point's cell (30.5, 60.5)
		REQUIRE(pf.findMeleeEngagementLocation(*ramPf, locoRam, *nearPf, reachNear, &ramNear));
		REQUIRE(pf.findMeleeEngagementLocation(*swordsmanPf, atkPlain->getAIUpdateInterface()->locomotorInfo(), *nearPf, reachNear, &swordNear));
		CHECK(ramNear.x == 70.5f);
		CHECK(ramNear.y == 100.5f);
		CHECK(swordNear.x == 60.5f);
		CHECK(swordNear.y == 60.5f);
	}

	SUBCASE("unit WITHOUT SpecialContactPoints (Swordsman) attacks building with AttackContactPoint -> regular approach path (not contact point)")
	{
		Coord3D swordsmanDest = *contactWall->getPosition();
		const FixedRange reachWall(*contactWall->getPosition(), 50.0f);
		const PathfindLocomotorInfo locoSwordsman = atkPlain->getAIUpdateInterface()->locomotorInfo();

		// Without SpecialContactPoints, adjustMeleeOffset RW 0x6EA11E -> RW 0x667C76 returns false;
		// it runs the 20 obstacle steps RW 0x6EA12B..0x6EA1EB -> does NOT go to the Ram contact point (170, 200).
		REQUIRE(pf.findMeleeEngagementLocation(*swordsmanPf, locoSwordsman, *wallPf, reachWall, &swordsmanDest));
		CHECK(swordsmanDest.x != 170.5f);
	}

	SUBCASE("unit with SpecialContactPoints attacks another unit (not a building) -> contact points are NOT searched")
	{
		Coord3D ramUnitDest = *targetUnit->getPosition();
		const FixedRange reachUnit(*targetUnit->getPosition(), 50.0f);
		const PathfindLocomotorInfo locoRam = atkRam->getAIUpdateInterface()->locomotorInfo();

		// Target is NOT STRUCTURE or WALK_ON_TOP_OF_WALL -> RW 0x6F38E1..0x6F38FA bypasses adjustMeleeOffset completely.
		REQUIRE(pf.findMeleeEngagementLocation(*ramPf, locoRam, *unitPf, reachUnit, &ramUnitDest));
		// Destination is determined by standard unit melee engagement, not building contact points.
		CHECK(ramUnitDest.x != 170.5f);
	}
}
