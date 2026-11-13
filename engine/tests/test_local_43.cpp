// OpenBFME. GPL-3.0.
// Lane LOCAL-43 (fork issue 360, PROBLEMS P-15): the context move hint's reach test. RotWK's evaluateContextCommand (RW 0x81E60B) gives the move hint
// 0xBD (Move, cursor 5) only for a point the selection can reach (RW 0x81F65E: InGameUI RW 0x69E8D0, the selected movers' quick path test RW 0x664112 ->
// RW 0x6F5BB0); a point it cannot reach is hint 0xC9 (RW 0x81F733), which createCommandHint shows as cursor 12, GenericInvalid (jump table RW 0x6A4099 entry
// 0x18 -> RW 0x6A3F87 push 0xC). The hover passes the drawable's position (RW 0x8204E9 .. 0x8204F2), so an allied building's centre (an obstacle cell of its
// footprint) is GenericInvalid while an allied unit standing on open ground keeps Move.

#include "doctest.h"

#include "HudTestUtil.h"

#include "GameClient/HudObjects.h"
#include "GameLogic/Map/TerrainLogic.h"
#include "GameLogic/Object/Object.h"

using namespace hudtest;

namespace
{
Coord3D mapCentre(Rig &r)
{
	float mx = 0, my = 0;
	REQUIRE(r.logic().terrain() != nullptr);
	REQUIRE(r.logic().terrain()->getExtent(0, mx, my));
	return Coord3D{ mx * 0.5f, my * 0.5f, 0.0f };
}
} // namespace

TEST_CASE("LOCAL-43 move hint: with own troops selected, an allied or own building is GenericInvalid (RW 0x81F733 -> RW 0x6A3F87), an allied unit and the "
		  "open ground stay Move (RW 0x81F73C)")
{
	if (!haveWorld("LOCAL-43 move hint"))
	{
		return;
	}
	SharedWorld &s = shared();
	Rig r(s, "map mp fall back 4p", "FactionMen", "FactionMen");
	Player *ally = r.game->players().findPlayerWithName("Player_3");
	REQUIRE(ally != nullptr);
	const Coord3D c0 = r.freeSpot(mapCentre(r).x, mapCentre(r).y, 420.0f);
	Object *horde = r.make("GondorFighterHorde", c0.x, c0.y);
	Object *allyBarracks = r.make("GondorBarracks", c0.x - 220.0f, c0.y, ally);
	Object *allyHero = r.make("GondorBoromir", c0.x + 120.0f, c0.y + 40.0f, ally);
	Object *ownBarracks = r.make("GondorBarracks", c0.x + 260.0f, c0.y - 60.0f);
	r.lookAt({ c0.x, c0.y, 0.0f }, 460.0f, 560.0f);
	r.frame(3);
	auto px = [&](const Object *o, float up) { return r.screenOf({ o->getPosition()->x, o->getPosition()->y, o->getPosition()->z + up }); };
	const ICoord2D ph = px(horde, 6.0f);
	r.leftDrag(ph.x - 50, ph.y - 50, ph.x + 50, ph.y + 50);
	r.frame(2);
	REQUIRE(r.local->selection() == std::vector<ObjectID>{ horde->getID() });
	const ICoord2D pa = px(allyBarracks, 30.0f), pu = px(allyHero, 10.0f), po = px(ownBarracks, 30.0f);
	const ICoord2D pg{ ph.x, ph.y + 120 };
	REQUIRE(HudObjects::pickObject(r.input->context(), pa) == allyBarracks);
	REQUIRE(HudObjects::pickObject(r.input->context(), pu) == allyHero);
	REQUIRE(HudObjects::pickObject(r.input->context(), po) == ownBarracks);
	REQUIRE(HudObjects::pickObject(r.input->context(), pg) == nullptr);
	// the reach test itself (RW 0x69E8D0): the buildings' centres are not reachable, the hero's spot and the ground are
	CHECK_FALSE(HudObjects::canSelectionReachHint(r.input->context(), *allyBarracks->getPosition()));
	CHECK(HudObjects::canSelectionReachHint(r.input->context(), *allyHero->getPosition()));

	r.move(pa.x, pa.y);
	CHECK(r.input->ui().cursor() == std::string(MouseCursorName::GenericInvalid)); // allied structure
	r.move(po.x, po.y);
	CHECK(r.input->ui().cursor() == std::string(MouseCursorName::GenericInvalid)); // own structure
	r.move(pu.x, pu.y);
	CHECK(r.input->ui().cursor() == std::string(MouseCursorName::Move)); // allied unit on open ground
	r.move(pg.x, pg.y);
	CHECK(r.input->ui().cursor() == std::string(MouseCursorName::Move)); // open ground
}

// P-15 (owner's check 2026-11-12: the yellow blinking square showed over enemy troops with nothing selected). createMouseoverHint (RW 0x69FCFC .. 0x69FD73,
// BFME2 decomp InGameUICreateMouseoverHint.cpp:538): with no selection, a selectable drawable that the local player controls (RW 0x68B749) shows Select
// (13), any other drawable and the ground show Arrow (2); with a selection the mouseover sets no cursor (RW 0x69FD0C -> 0x69FD7E), the command hint does.
TEST_CASE("P-15 mouseover cursor: nothing selected, own unit is Select, an enemy or allied unit is Arrow (RW 0x69FD53 .. 0x69FD73)")
{
	if (!haveWorld("P-15 mouseover cursor"))
	{
		return;
	}
	SharedWorld &s = shared();
	Rig r(s, "map mp fall back 4p", "FactionMen", "FactionMen");
	Player *ally = r.game->players().findPlayerWithName("Player_3");
	Player *enemy = r.game->players().findPlayerWithName("Player_2");
	REQUIRE(ally != nullptr);
	REQUIRE(enemy != nullptr);
	REQUIRE(r.local->getRelationship(enemy->getDefaultTeam()) == ENEMIES);
	const Coord3D c0 = r.freeSpot(mapCentre(r).x, mapCentre(r).y, 420.0f);
	Object *own = r.make("GondorBoromir", c0.x, c0.y);
	Object *foe = r.make("GondorBoromir", c0.x + 160.0f, c0.y, enemy);
	Object *friendHero = r.make("GondorBoromir", c0.x - 160.0f, c0.y, ally);
	r.lookAt({ c0.x, c0.y, 0.0f }, 460.0f, 560.0f);
	r.frame(3);
	r.input->ui().deselectAll(false);
	auto px = [&](const Object *o) { return r.screenOf({ o->getPosition()->x, o->getPosition()->y, o->getPosition()->z + 10.0f }); };
	const ICoord2D po = px(own), pe = px(foe), pa = px(friendHero);
	REQUIRE(HudObjects::pickObject(r.input->context(), po) == own);
	REQUIRE(HudObjects::pickObject(r.input->context(), pe) == foe);
	REQUIRE(HudObjects::pickObject(r.input->context(), pa) == friendHero);
	r.move(po.x, po.y);
	CHECK(r.input->ui().cursor() == std::string(MouseCursorName::Select));
	r.move(pe.x, pe.y);
	CHECK(r.input->ui().cursor() == std::string(MouseCursorName::Arrow)); // the symptom: Select over the enemy
	r.move(pa.x, pa.y);
	CHECK(r.input->ui().cursor() == std::string(MouseCursorName::Arrow));
}
