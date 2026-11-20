// OpenBFME unit tests. GPL-3.0.
// Port 297 (fork issue 689): the resource buildings' and supply piles' own RotWK functions. Synthetic templates, no game files.
//   * SupplyWarehouseDockUpdate::setDockCrippled (RW 0x8AB6E3, dock interface slot 17): crippling a warehouse with an active docker kills the docker inside
//     it (Object::kill(UNRESISTABLE, NORMAL), RW 0x698EC3(8, 0)) unless it moves on an airborne locomotor (RW 0x68BEED); a docker that is not inside yet and
//     has a supply truck AI goes idle (aiIdle(CMD_FROM_AI), RW 0x5E821A with 2) and is forced into its wanting state (slot 0x2C with true); then DockUpdate's
//     flag (RW 0x991727). Uncrippling (false) only clears the flag.
// TerrainResourceBehavior's own functions needed no change (GameLogic/Module/TerrainResourceBehavior.h): its income stays held by test_econ_resources.cpp.

#include "doctest.h"
#include "MoveTestUtil.h"

#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/DockUpdate.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Module/SupplyTruckAIUpdate.h"
#include "GameLogic/Object/Object.h"

namespace
{
const char *kPort297Objects =
	"Locomotor Port297WingsLoco\n"
	"  Surfaces = AIR\n"
	"  TurnTime = 500\n"
	"  Acceleration = 510\n"
	"  Braking = 510\n"
	"  ZAxisBehavior = NO_Z_MOTIVE_FORCE\n"
	"  Appearance = WINGS\n"
	"  StickToGround = No\n"
	"  CloseEnoughDist = 1\n"
	"End\n"
	"Object Port297Quarryman\n"
	"  KindOf = INFANTRY\n"
	"  Body = ActiveBody B\n    MaxHealth = 10\n  End\n"
	"  Behavior = WorkerAIUpdate W\n"
	"    MaxBoxes = 1\n"
	"    SupplyCenterActionDelay = 1000\n"
	"    SupplyWarehouseActionDelay = 1000\n"
	"    SupplyWarehouseScanDistance = 500\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n"
	"Object Port297FlyingQuarryman\n"
	"  KindOf = INFANTRY\n"
	"  Body = ActiveBody B\n    MaxHealth = 10\n  End\n"
	"  Behavior = WorkerAIUpdate W\n"
	"    MaxBoxes = 1\n"
	"    SupplyCenterActionDelay = 1000\n"
	"    SupplyWarehouseActionDelay = 1000\n"
	"    SupplyWarehouseScanDistance = 500\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = Port297WingsLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n"
	"Object Port297Walker\n"
	"  KindOf = INFANTRY\n"
	"  Body = ActiveBody B\n    MaxHealth = 10\n  End\n"
	"  Behavior = AIUpdateInterface AI\n  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n"
	"Object Port297Rocks\n"
	"  KindOf = STRUCTURE\n"
	"  Body = ActiveBody B\n    MaxHealth = 10\n  End\n"
	"  Behavior = SupplyWarehouseDockUpdate D\n    NumberApproachPositions = -1\n    StartingBoxes = 5\n  End\n"
	"End\n";

struct Port297World : movetest::MoveWorld
{
	Port297World() : movetest::MoveWorld(100, 100, kPort297Objects) { buildMap(); }

	Object *rocks()
	{
		Object *o = spawn("Port297Rocks", 200.0f, 100.0f, 0.0f, players.getNeutralPlayer()->getDefaultTeam());
		REQUIRE(o);
		return o;
	}
	static SupplyWarehouseDockUpdate *dockOf(Object *rocks)
	{
		auto *d = dynamic_cast<SupplyWarehouseDockUpdate *>(rocks->findModule("SupplyWarehouseDockUpdate"));
		REQUIRE(d);
		REQUIRE(rocks->getDockUpdateInterface() == static_cast<DockUpdateInterface *>(d));
		return d;
	}
	// the dock's own path to an active docker (RW 0x99221D reserve, RW 0x991B32 approach reached, RW 0x991F8A update promotes the first reached owner),
	// and with `inside` RW 0x991E5E (onEnterReached: + 0x84 = 1)
	static void dock(SupplyWarehouseDockUpdate *d, Object *docker, bool inside)
	{
		Coord3D pos{};
		int index = -1;
		REQUIRE(d->reserveApproachPosition(*docker, &pos, &index));
		d->onApproachReached(*docker);
		d->update();
		REQUIRE(d->activeDocker() == docker->getID());
		if (inside)
		{
			d->onEnterReached(*docker);
			REQUIRE(d->dockerInside());
		}
	}
};

bool airborne(const Object *o)
{
	const AIUpdateInterface *ai = o->getAIUpdateInterface();
	return ai && ai->curLocomotor() && (ai->curLocomotor()->getTemplate().m_surfaces & (unsigned)LOCOMOTORSURFACE_AIR) != 0;
}
} // namespace

TEST_CASE("PORT-297 SupplyWarehouseDockUpdate::setDockCrippled: crippling kills the ground docker inside the warehouse (RW 0x8AB6E3 -> RW 0x698EC3(8, 0))")
{
	Port297World f;
	Object *worker = f.spawn("Port297Quarryman", 180.0f, 100.0f, 0.0f, f.teamOf("Alice"));
	Object *rocks = f.rocks();
	SupplyWarehouseDockUpdate *d = Port297World::dockOf(rocks);
	REQUIRE_FALSE(airborne(worker));
	Port297World::dock(d, worker, true);
	REQUIRE_FALSE(worker->isEffectivelyDead());

	DockUpdateInterface *di = rocks->getDockUpdateInterface();
	di->setDockCrippled(true); // through the dock interface, as SupplyWarehouseCripplingBehavior calls it (RW 0x886E46 `call [edx + 0x44]`)

	CHECK(worker->isEffectivelyDead()); // RW 0x8AB71E: kill(DAMAGE_UNRESISTABLE, DEATH_NORMAL) inside the call
	CHECK(d->dockCrippled());           // RW 0x8AB759: DockUpdate::setDockCrippled (RW 0x991727) runs after
	// the dock itself is not hurt, keeps its boxes, and RW does not forget the docker here (no write to + 0x80 / + 0x84 in RW 0x8AB6E3)
	CHECK_FALSE(rocks->isEffectivelyDead());
	CHECK(d->boxesStored() == 5);
	CHECK(d->activeDocker() == worker->getID());
	CHECK(d->dockerInside());
	// the dead docker's removal later in the frame leaves a consistent world
	f.frames(2);
	CHECK_FALSE(rocks->isEffectivelyDead());
}

TEST_CASE("PORT-297 SupplyWarehouseDockUpdate::setDockCrippled: a docker that is not inside yet goes idle (CMD_FROM_AI) and is forced into its wanting state (RW 0x8AB725 .. 0x8AB74E)")
{
	Port297World f;
	Object *worker = f.spawn("Port297Quarryman", 180.0f, 100.0f, 0.0f, f.teamOf("Alice"));
	Object *rocks = f.rocks();
	SupplyWarehouseDockUpdate *d = Port297World::dockOf(rocks);
	Port297World::dock(d, worker, false);
	AIUpdateInterface *ai = worker->getAIUpdateInterface();
	REQUIRE(ai);
	SupplyTruckAIInterface *sup = ai->getSupplyTruckAIInterface();
	REQUIRE(sup);
	ai->setLastCommandSource(CMD_FROM_PLAYER); // the player sent it here
	REQUIRE_FALSE(sup->isForcedIntoWantingState());

	rocks->getDockUpdateInterface()->setDockCrippled(true);

	CHECK_FALSE(worker->isEffectivelyDead());          // only a docker inside dies
	CHECK(ai->lastCommandSource() == CMD_FROM_AI);     // RW 0x8AB73E .. 0x8AB743: aiIdle with source 2
	CHECK(sup->isForcedIntoWantingState());            // RW 0x8AB74A .. 0x8AB74E: slot 0x2C with 1
	CHECK(d->dockCrippled());
}

TEST_CASE("PORT-297 SupplyWarehouseDockUpdate::setDockCrippled: an airborne docker inside survives; a docker without a supply truck AI is left alone (RW 0x8AB70F, RW 0x8AB738)")
{
	Port297World f;
	Object *flier = f.spawn("Port297FlyingQuarryman", 180.0f, 100.0f, 0.0f, f.teamOf("Alice"));
	Object *rocks = f.rocks();
	SupplyWarehouseDockUpdate *d = Port297World::dockOf(rocks);
	REQUIRE(airborne(flier));
	Port297World::dock(d, flier, true);
	rocks->getDockUpdateInterface()->setDockCrippled(true);
	CHECK_FALSE(flier->isEffectivelyDead()); // RW 0x8AB714 `jne`: an airborne locomotor skips the kill
	CHECK(d->dockCrippled());

	// a plain unit (no supply truck interface) approaching another warehouse: nothing but the flag
	Object *walker = f.spawn("Port297Walker", 300.0f, 300.0f, 0.0f, f.teamOf("Alice"));
	Object *rocks2 = f.spawn("Port297Rocks", 320.0f, 300.0f, 0.0f, f.players.getNeutralPlayer()->getDefaultTeam());
	SupplyWarehouseDockUpdate *d2 = Port297World::dockOf(rocks2);
	Port297World::dock(d2, walker, false);
	AIUpdateInterface *ai = walker->getAIUpdateInterface();
	REQUIRE(ai);
	REQUIRE_FALSE(ai->getSupplyTruckAIInterface());
	ai->setLastCommandSource(CMD_FROM_PLAYER);
	rocks2->getDockUpdateInterface()->setDockCrippled(true);
	CHECK_FALSE(walker->isEffectivelyDead());
	CHECK(ai->lastCommandSource() == CMD_FROM_PLAYER); // no aiIdle without the supply truck interface
	CHECK(d2->dockCrippled());
}

TEST_CASE("PORT-297 SupplyWarehouseDockUpdate::setDockCrippled(false) and a dock without an active docker only set DockUpdate's flag (RW 0x8AB6EB, RW 0x8AB6F2)")
{
	Port297World f;
	Object *worker = f.spawn("Port297Quarryman", 180.0f, 100.0f, 0.0f, f.teamOf("Alice"));
	Object *rocks = f.rocks();
	SupplyWarehouseDockUpdate *d = Port297World::dockOf(rocks);
	DockUpdateInterface *di = rocks->getDockUpdateInterface();

	// no active docker: crippled, nobody touched
	di->setDockCrippled(true);
	CHECK(d->dockCrippled());
	CHECK(d->activeDocker() == INVALID_ID);
	CHECK_FALSE(worker->isEffectivelyDead());
	// a crippled dock promotes no reached docker (DockUpdate::update RW 0x991F8A, unchanged)
	Coord3D pos{};
	int index = -1;
	REQUIRE(d->reserveApproachPosition(*worker, &pos, &index));
	d->onApproachReached(*worker);
	d->update();
	CHECK(d->activeDocker() == INVALID_ID);

	// uncrippling with a docker inside: the docker lives, only the flag clears
	di->setDockCrippled(false);
	CHECK_FALSE(d->dockCrippled());
	d->update();
	REQUIRE(d->activeDocker() == worker->getID());
	d->onEnterReached(*worker);
	REQUIRE(d->dockerInside());
	worker->getAIUpdateInterface()->setLastCommandSource(CMD_FROM_PLAYER);
	di->setDockCrippled(false);
	CHECK_FALSE(worker->isEffectivelyDead());
	CHECK(worker->getAIUpdateInterface()->lastCommandSource() == CMD_FROM_PLAYER);
	CHECK_FALSE(worker->getAIUpdateInterface()->getSupplyTruckAIInterface()->isForcedIntoWantingState());
	CHECK_FALSE(d->dockCrippled());
}
