// OpenBFME unit tests. GPL-3.0.
// Lane LUMBER1, the supply warehouses (Ennorath's supply rocks; no RotWK retail object has one): ActionManager::canTransferSuppliesAt's warehouse branch
// (RW 0x82C66C: boxes left and the dock not ENEMIES with the worker) and the worker's warehouse search that depends on it (findBestSupplyWarehouse RW
// 0x8FAA51). Synthetic templates; the rules are the binary's (Common/ResourceGatheringManager.h) and ZH's ActionManager::canTransferSuppliesAt.

#include "doctest.h"
#include "MoveTestUtil.h"

#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/ResourceGatheringManager.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/DockUpdate.h"
#include "GameLogic/Module/SupplyTruckAIUpdate.h"
#include "GameLogic/Object/Object.h"

namespace
{
const char *kWarehouseObjects =
	"Object Quarryman\n"
	"  KindOf = INFANTRY\n"
	"  Body = ActiveBody B\n    MaxHealth = 10\n  End\n"
	"  Behavior = WorkerAIUpdate W\n"
	"    MaxBoxes = 1\n"
	"    SupplyCenterActionDelay = 1000\n"
	"    SupplyWarehouseActionDelay = 1000\n"
	"    SupplyWarehouseScanDistance = 500\n"
	"  End\n"
	"End\n"
	"Object SupplyRocks\n"
	"  KindOf = STRUCTURE\n"
	"  Body = ActiveBody B\n    MaxHealth = 10\n  End\n"
	"  Behavior = SupplyWarehouseCreate C\n  End\n"
	"  Behavior = SupplyWarehouseDockUpdate D\n    NumberApproachPositions = -1\n    StartingBoxes = 2\n  End\n"
	"End\n"
	"Object EmptyRocks\n"
	"  KindOf = STRUCTURE\n"
	"  Body = ActiveBody B\n    MaxHealth = 10\n  End\n"
	"  Behavior = SupplyWarehouseDockUpdate D\n    StartingBoxes = 0\n  End\n"
	"End\n"
	"Object Boulder\n"
	"  KindOf = STRUCTURE\n"
	"  Body = ActiveBody B\n    MaxHealth = 10\n  End\n"
	"End\n";
} // namespace

TEST_CASE("LUMBER1 warehouse: a worker may take boxes at a warehouse with boxes left that is not its enemy (RW 0x82C66C), and its search finds one")
{
	movetest::MoveWorld f(100, 100, kWarehouseObjects); // the AI world: WorkerAIUpdate needs one
	f.buildMap();
	Object *worker = f.spawn("Quarryman", 100.0f, 100.0f, 0.0f, f.teamOf("Alice"));
	REQUIRE(worker->getAIUpdateInterface());
	REQUIRE(worker->getAIUpdateInterface()->getSupplyTruckAIInterface());
	REQUIRE(worker->getAIUpdateInterface()->getSupplyTruckAIInterface()->getNumberBoxes() == 0);

	// a neutral warehouse (the map's supply rocks)
	Object *rocks = f.spawn("SupplyRocks", 200.0f, 100.0f, 0.0f, f.players.getNeutralPlayer()->getDefaultTeam());
	const auto *dock = dynamic_cast<const SupplyWarehouseDockUpdate *>(rocks->findModule("SupplyWarehouseDockUpdate"));
	REQUIRE(dock);
	CHECK(dock->boxesStored() == 2);
	REQUIRE(rocks->getRelationship(*worker) != ENEMIES);
	CHECK(ResourceGatheringManager::canTransferSuppliesAt(worker, rocks));

	// no boxes left: refused
	Object *empty = f.spawn("EmptyRocks", 120.0f, 100.0f, 0.0f, f.teamOf("Alice"));
	CHECK_FALSE(ResourceGatheringManager::canTransferSuppliesAt(worker, empty));

	// the worker's own and an enemy's warehouse
	Object *own = f.spawn("SupplyRocks", 300.0f, 300.0f, 0.0f, f.teamOf("Alice"));
	CHECK(ResourceGatheringManager::canTransferSuppliesAt(worker, own));
	Object *enemy = f.spawn("SupplyRocks", 300.0f, 100.0f, 0.0f, f.teamOf("Bob"));
	REQUIRE(enemy->getRelationship(*worker) == ENEMIES);
	CHECK_FALSE(ResourceGatheringManager::canTransferSuppliesAt(worker, enemy));

	// neither a warehouse nor a center: refused, as before
	Object *boulder = f.spawn("Boulder", 150.0f, 100.0f, 0.0f, f.teamOf("Alice"));
	CHECK_FALSE(ResourceGatheringManager::canTransferSuppliesAt(worker, boulder));

	// SupplyWarehouseCreate listed the rocks with Alice's manager: the search now answers the closest usable warehouse within the scan distance
	Player *alice = f.players.findPlayerWithName("Alice");
	REQUIRE(alice);
	Object *best = alice->resourceGatheringManager().findBestSupplyWarehouse(worker);
	REQUIRE(best);
	CHECK(best->getID() == rocks->getID());
}

TEST_CASE("LUMBER1 warehouse (S-286): MSG_DOCK sends the selected worker to a warehouse it may use, and not to an enemy's")
{
	movetest::MoveWorld f(100, 100, kWarehouseObjects);
	f.buildMap();
	Object *worker = f.spawn("Quarryman", 100.0f, 100.0f, 0.0f, f.teamOf("Alice"));
	Object *rocks = f.spawn("SupplyRocks", 200.0f, 100.0f, 0.0f, f.players.getNeutralPlayer()->getDefaultTeam());
	Object *enemy = f.spawn("SupplyRocks", 300.0f, 100.0f, 0.0f, f.teamOf("Bob"));
	const int alice = f.playerIndex("Alice");
	f.select(alice, { worker });
	f.frames(1);
	GameMessage toEnemy(MSG_DOCK, alice);
	toEnemy.appendObjectIDArgument(enemy->getID());
	f.send(toEnemy);
	f.frames(1);
	CHECK(f.aiCommands.stats().docks == 1);
	CHECK(f.aiCommands.stats().dockOrdersAccepted == 0);
	GameMessage toRocks(MSG_DOCK, alice);
	toRocks.appendObjectIDArgument(rocks->getID());
	f.send(toRocks);
	f.frames(1);
	CHECK(f.aiCommands.stats().docks == 2);
	CHECK(f.aiCommands.stats().dockOrdersAccepted == 1);
	CHECK(worker->getAIUpdateInterface()->lastCommandSource() == CMD_FROM_PLAYER);
	CHECK(worker->getAIUpdateInterface()->stateMachine().goalObjectID() == rocks->getID());
}
