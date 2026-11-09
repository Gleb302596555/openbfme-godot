// OpenBFME unit tests (Local 23, issue #178): ProductionQueueHordeContain and ProductionUpdate slot 14.
// Expected values and behavior are proven from RotWK 2.01 game.dat:
// - slot 14 (RW 0x8A03A0, DONOR BFME2 0x0049CC15) sets entry +0x3C (hordeTarget)
// - ProductionQueueHordeContain (RW 0x658518 / 0x8844D6, DONOR BFME2 0x00481343)
// - DestinationTemplate parsing (table RW 0xC5F964, parser RW 0x88494C, DONOR BFME2 0x004816F4)
// - CreateTemplate (RW 0x88460B, DONOR BFME2 0x004813B3)

#include "doctest.h"
#include "ProdTestUtil.h"

#include "GameLogic/Module/ProductionUpdate.h"

using namespace prodtest;

namespace
{
const char kLocal23Objects[] =
	"Object Soldier\n"
	"  BuildCost = 100\n"
	"  BuildTime = 10.0\n"
	"  CommandPoints = 4\n"
	"  KindOf = SELECTABLE INFANTRY\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"End\n"
	"Object Barracks\n"
	"  KindOf = STRUCTURE SELECTABLE\n"
	"  CommandSet = BarracksSet\n"
	"  Behavior = ProductionUpdate ModuleTag_PU\n"
	"    MaxQueueEntries = 5\n"
	"  End\n"
	"  Behavior = QueueProductionExitUpdate ModuleTag_Q\n"
	"    UnitCreatePoint = X:0.0 Y:-20.0 Z:0.0\n"
	"    NaturalRallyPoint = X:30.0 Y:-50.0 Z:0.0\n"
	"  End\n"
	"End\n";

struct Local23Fixture : ProdWorld
{
	Object *barracks = nullptr;
	Local23Fixture()
	{
		load(kLocal23Objects);
		load(kBarracksCommands);
		barracks = make("Barracks", teamOf("Alice"));
		REQUIRE(barracks != nullptr);
	}
};
} // namespace

TEST_CASE("LOCAL-23: ProductionUpdate slot 14 setEntryHordeTarget stores horde target by productionID")
{
	Local23Fixture fx;
	ProductionUpdateInterface *pu = fx.barracks->getProductionUpdate();
	REQUIRE(pu != nullptr);

	const ThingTemplate *soldier = fx.w.things.findTemplate("Soldier");
	REQUIRE(soldier != nullptr);

	ProductionID id1 = pu->requestUniqueUnitID();
	ProductionID id2 = pu->requestUniqueUnitID();
	CHECK(id1 != id2);

	// Queue two units
	bool q1 = pu->queueCreateUnit(soldier, -1, id1, -1, false, "", false);
	bool q2 = pu->queueCreateUnit(soldier, -1, id2, -1, false, "", false);
	CHECK(q1);
	CHECK(q2);
	CHECK(pu->getProductionCount() == 2);

	const ProductionEntry *e1 = pu->firstProduction();
	REQUIRE(e1 != nullptr);
	CHECK(e1->productionID == id1);
	CHECK(e1->hordeTarget == INVALID_ID);

	const ProductionEntry *e2 = pu->nextProduction(e1);
	REQUIRE(e2 != nullptr);
	CHECK(e2->productionID == id2);
	CHECK(e2->hordeTarget == INVALID_ID);

	// Slot 14 (RW 0x8A03A0): set horde target for id1 to 12345
	pu->setEntryHordeTarget(id1, 12345);
	CHECK(e1->hordeTarget == 12345);
	CHECK(e2->hordeTarget == INVALID_ID);

	// Slot 14: set horde target for id2 to 67890
	pu->setEntryHordeTarget(id2, 67890);
	CHECK(e1->hordeTarget == 12345);
	CHECK(e2->hordeTarget == 67890);

	// Non-existent productionID does nothing
	pu->setEntryHordeTarget(999999, 11111);
	CHECK(e1->hordeTarget == 12345);
	CHECK(e2->hordeTarget == 67890);
}
