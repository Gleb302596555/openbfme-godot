// OpenBFME unit tests: LOCAL-7 AttackContactPoint (issue #112, TOP20 #26).
// RotWK addresses:
//   Parser row RW 0xDA44C8 (name RW 0xC25744 "AttackContactPoint", parser RW 0xAD3D30, ThingTemplate + 0xA0)
//   GeometryInfo::getBestContactPoint RW 0xAD30E0
//   Object::getWorldspaceBestContactPoint RW 0x690BD2
//   Pathfinder::adjustMeleeOffset RW 0x6EA0EC / 0x6EA11E
//   AIAttackFireDuringApproachState::update RW 0x75232F / 0x75253D
//   AIAttackFireWeaponState::onEnter RW 0x74C258 / 0x74C5FF

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

TEST_CASE("LOCAL-7 attack contact point: attackers choose closest contact point on an object, fallback to center without points")
{
	CombatWorld w;
	w.combat().setAutoAcquireEnabled(false);
	const std::string ini =
		makeWall("ContactWall", "  AttackContactPoint = X:-30 Y:0 Z:0\n  AttackContactPoint = X:30 Y:0 Z:0\n") +
		makeWall("PlainWall", "");
	std::string err = w.w.load(ini, INI_LOAD_OVERWRITE, "local7_walls.ini");
	REQUIRE_MESSAGE(err.empty(), err);

	Object *contactWall = w.unit("ContactWall", 'B', 200.0f, 200.0f);
	REQUIRE(contactWall != nullptr);
	Object *plainWall = w.unit("PlainWall", 'B', 200.0f, 200.0f);
	REQUIRE(plainWall != nullptr);

	SUBCASE("west attacker chooses west contact point (-30 relative -> 170 world)")
	{
		Coord3D pt;
		bool hasPt = ObjectGeometry::worldspaceBestContactPoint(*contactWall, Coord3D{ 100.0f, 200.0f, 0.0f }, "", false, pt);
		CHECK(hasPt);
		CHECK(pt.x == 170.0f);
		CHECK(pt.y == 200.0f);

		Object *atkWest = w.unit("Swordsman", 'A', 100.0f, 200.0f);
		w.frames(2);
		AIUpdateInterface *ai = atkWest->getAIUpdateInterface();
		REQUIRE(ai != nullptr);
		REQUIRE(ai->aiAttackObject(contactWall, CMD_FROM_PLAYER));
		w.frames(1);

		// Check the approach destination
		CHECK(ai->mover().requestedDestination().x == 170.0f);
		CHECK(ai->mover().requestedDestination().y == 200.0f);
	}

	SUBCASE("east attacker chooses east contact point (+30 relative -> 230 world)")
	{
		Object *atkEast = w.unit("Swordsman", 'A', 300.0f, 200.0f);
		w.frames(2);
		AIUpdateInterface *ai = atkEast->getAIUpdateInterface();
		REQUIRE(ai != nullptr);
		REQUIRE(ai->aiAttackObject(contactWall, CMD_FROM_PLAYER));
		w.frames(1);

		// Check the approach destination
		CHECK(ai->mover().requestedDestination().x == 230.0f);
		CHECK(ai->mover().requestedDestination().y == 200.0f);
	}

	SUBCASE("attacker on object without contact points chooses object center (200 world)")
	{
		Object *atkPlain = w.unit("Swordsman", 'A', 100.0f, 200.0f);
		w.frames(2);
		AIUpdateInterface *ai = atkPlain->getAIUpdateInterface();
		REQUIRE(ai != nullptr);
		REQUIRE(ai->aiAttackObject(plainWall, CMD_FROM_PLAYER));
		w.frames(1);

		// Plain object without contact points falls back to object center (adjusted to cell center 200.5)
		CHECK(ai->mover().requestedDestination().x == 200.5f);
		CHECK(ai->mover().requestedDestination().y == 200.5f);
	}
}
