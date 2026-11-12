// OpenBFME unit tests. GPL-3.0.
// LOCAL-42 (fork issue 353): a map tree made an object, TerrainLogic RW 0x68449B. Synthetic INI only (CombatWorld), no game files.
//   * the closest record within 10.0 (RW 0x67E078(pos, 10.0, collector, 0, 0)); with a template (+ 0x14): ThingFactory::newObject for the neutral player's
//     default team (RW 0x6D165E), teleported onto the record (RW 0x696E63), the record cleared and removed (RW 0x67D3C2, RW 0x684412);
//   * a record without a template: cleared and removed, null;
//   * no record: the closest alive SUPPLY_SOURCE object within 10.0 (RW 0xA39090, 3D centres; filters RW 0xBD88B0 KindOf bit 0x56, RW 0xC10E20 alive).

#include "doctest.h"

#include "CombatTestUtil.h"

#include "Common/PlayerList.h"
#include "Common/Player.h"
#include "GameLogic/Map/TerrainTrees.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Module/FlammableUpdate.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/Object/PartitionManager.h"
#include "GameLogic/ObjectTemplateInfo.h"

#include <string>

using namespace combattest;

namespace
{
const char *const kObjects =
	"Object Local42Tree\n"
	"  Draw = W3DTreeDraw ModuleTag_01\n"
	"    ModelName = PTTree01\n"
	"  End\n"
	"  KindOf = SHRUBBERY IMMOBILE CLUB TREE SELECTABLE\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 5\n"
	"  GeometryMinorRadius = 5\n"
	"  GeometryHeight = 30\n"
	"  Behavior = FlammableUpdate ModuleTag_Flame\n"
	"    AflameDuration = 5000\n"
	"  End\n"
	"End\n"
	"Object Local42Burner\n"
	"  KindOf = STRUCTURE IMMOBILE\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 1000\n"
	"  End\n"
	"  Behavior = FlammableUpdate ModuleTag_Flame\n"
	"    AflameDuration = 20000\n"
	"  End\n"
	"  Behavior = FireSpreadUpdate ModuleTag_Spread\n"
	"    MinSpreadDelay = 400\n"
	"    MaxSpreadDelay = 400\n"
	"    SpreadTryRange = 60\n"
	"  End\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryMinorRadius = 10\n"
	"  GeometryHeight = 20\n"
	"End\n"
	"Object Local42Lumber\n"
	"  KindOf = IMMOBILE SUPPLY_SOURCE\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 5\n"
	"  GeometryMinorRadius = 5\n"
	"  GeometryHeight = 10\n"
	"End\n";

struct TreeWorld
{
	CombatWorld w{ kObjects };
	TerrainTrees &trees() { return w.logic->trees(); }
	const ThingTemplate *tree() { return w.w.things.findTemplate("Local42Tree"); }
	TreeWorld()
	{
		trees().setExtent(-300.0f, -300.0f, 3000.0f, 3000.0f);
		w.logic->partition().setRegion(0.0f, 0.0f, 3000.0f, 3000.0f); // the map's region (LiveGame sets it from the map); objects are filed in it
		REQUIRE(tree() != nullptr);
	}
	const TerrainTrees::Record *record(std::uint32_t id)
	{
		for (const TerrainTrees::Record &r : trees().records())
		{
			if (r.clientId == id)
			{
				return &r;
			}
		}
		return nullptr;
	}
};
} // namespace

TEST_CASE("LOCAL-42 tree object: the closest map tree becomes a neutral object on its spot and leaves the tree list (RW 0x68449B)")
{
	TreeWorld t;
	const Coord3D spot{ 500.0f, 500.0f, 0.0f };
	REQUIRE(t.trees().addTree(*t.tree(), spot, 7));
	REQUIRE(t.trees().addTree(*t.tree(), Coord3D{ 560.0f, 500.0f, 0.0f }, 8)); // 60 away: outside 10 + 7
	Object *made = t.trees().makeTreeObject(Coord3D{ 504.0f, 503.0f, 0.0f });
	REQUIRE(made != nullptr);
	CHECK(made->getTemplate() == t.tree());
	CHECK(made->getPosition()->x == spot.x);
	CHECK(made->getPosition()->y == spot.y);
	REQUIRE(made->getControllingPlayer() != nullptr);
	CHECK(made->getControllingPlayer() == t.w.logic->players().getNeutralPlayer()); // ThePlayerList + 0x18 = player 0
	const TerrainTrees::Record *gone = t.record(7);
	CHECK(gone == nullptr); // RW 0x67D3C2 sets the id to 0: no query finds it again
	CHECK(t.trees().findClosestTree(spot, 1.0f, false, 0) == nullptr);
	CHECK(t.record(8) != nullptr); // the far tree stays
	// a second call at the same point: no record, and the made tree is not a SUPPLY_SOURCE, so nothing comes back
	CHECK(t.trees().makeTreeObject(Coord3D{ 504.0f, 503.0f, 0.0f }) == nullptr);
}

TEST_CASE("LOCAL-42 tree object: no record within 10 answers the closest alive SUPPLY_SOURCE object (RW 0x684555 .. 0x6845AD)")
{
	TreeWorld t;
	Object *lumber = t.w.unit("Local42Lumber", 'A', 800.0f, 800.0f);
	REQUIRE(lumber != nullptr);
	// the partition files the object at its next update (a game frame runs it; here it is called directly)
	t.w.logic->partition().update();
	CHECK(t.trees().makeTreeObject(Coord3D{ 805.0f, 803.0f, 0.0f }) == lumber); // 5.8 away, 3D centres
	CHECK(t.trees().makeTreeObject(Coord3D{ 830.0f, 800.0f, 0.0f }) == nullptr); // 30 away: outside 10
	lumber->kill(DAMAGE_UNRESISTABLE, 0);
	CHECK(t.trees().makeTreeObject(Coord3D{ 805.0f, 803.0f, 0.0f }) == nullptr); // RW 0x660E71: not effectively dead
}

namespace
{
FlammableUpdate *flammableOf(Object &o)
{
	for (const auto &m : o.modules())
	{
		if (FlammableUpdate *f = dynamic_cast<FlammableUpdate *>(m.get()))
		{
			return f;
		}
	}
	return nullptr;
}
} // namespace

// FireSpreadUpdate (RW 0x88EE6E): with no flammable object within SpreadTryRange, AIData AllowForestFires (TheAI's data + 0xBC, RW 0x88EF0F) lets the fire take
// the closest standing map tree (RW 0x67F52B(position, range, 1, 0)), made an object (RW 0x88EF3E -> RW 0x68449B) and set alight (RW 0x88EF8A). Off (retail
// 2.01 and Ennorath: No) the tree stays a map tree.
TEST_CASE("LOCAL-42 fire spread: a burning object sets the closest map tree alight only with AllowForestFires (RW 0x88EF07 .. 0x88EF8A)")
{
	static const int kAflame = CombatNames::status("AFLAME");
	for (const bool allow : { false, true })
	{
		CAPTURE(allow);
		TreeWorld t;
		t.w.logic->settings().allowForestFires = allow;
		REQUIRE(t.trees().addTree(*t.tree(), Coord3D{ 1240.0f, 1200.0f, 0.0f }, 3)); // 40 from the burner: inside SpreadTryRange 60
		Object *burner = t.w.unit("Local42Burner", 'A', 1200.0f, 1200.0f);
		REQUIRE(burner != nullptr);
		FlammableUpdate *flame = flammableOf(*burner);
		REQUIRE(flame != nullptr);
		flame->tryToIgnite();
		REQUIRE(burner->testStatus((unsigned)kAflame));
		t.w.frames(10); // the spread delay is 400 ms = 2 frames
		CHECK((t.record(3) != nullptr) == !allow);
		int burningTrees = 0;
		for (Object *o = t.w.logic->getFirstObject(); o; o = o->getNextObject())
		{
			if (o->getTemplate() == t.tree() && o->testStatus((unsigned)kAflame))
			{
				++burningTrees;
			}
		}
		CHECK(burningTrees == (allow ? 1 : 0));
	}
}
