// OpenBFME. GPL-3.0.
// LOCAL-96 (fork issue 761): DozerAIUpdate and WorkerAIUpdate own builder functions from RotWK 2.01 game.dat
// (md5 daed3668006cd90f01c34e5a7da1901f):
//   * DozerAIUpdate::onOwnerDie (RW 0x0088BF7A, main vslot +0x24C, called from Object::onDie RW 0x00698FB5):
//     removes an unplaced phantom structure and refunds its cost immediately on builder death (P-157).
#include "doctest.h"

#include "CombatTestUtil.h"
#include "EnnorathScenario.h"
#include "ProdTestUtil.h"

#include "Common/Player.h"
#include "GameLogic/Damage.h"
#include "GameLogic/Module/ActiveBody.h"
#include "GameLogic/Module/DozerAIUpdate.h"
#include "GameLogic/Module/WorkerAIUpdate.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/ObjectTemplateInfo.h"

#include <string>

namespace
{
const char kLocal96Ini[] =
	"Object L96Porter\n"
	"  KindOf = INFANTRY SELECTABLE DOZER\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = NoArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 200\n"
	"  End\n"
	"  Behavior = DozerAIUpdate ModuleTag_AI\n"
	"    RepairHealthPercentPerSecond = 10%\n"
	"  End\n"
	"  Behavior = SlowDeathBehavior ModuleTag_Death\n"
	"    DeathTypes = ALL\n"
	"    DestructionDelay = 30000\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 40\n"
	"  End\n"
	"End\n"
	"Object L96Worker\n"
	"  KindOf = INFANTRY SELECTABLE DOZER SWARM_DOZER\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = NoArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 200\n"
	"  End\n"
	"  Behavior = WorkerAIUpdate ModuleTag_AI\n"
	"    MaxBoxes = 1\n"
	"    RepairHealthPercentPerSecond = 10%\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 40\n"
	"  End\n"
	"End\n"
	"Object L96Structure\n"
	"  KindOf = STRUCTURE SELECTABLE IMMOBILE\n"
	"  BuildCost = 300\n"
	"  BuildTime = 10.0\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryMinorRadius = 20\n"
	"  GeometryHeight = 20\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = NoArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 400\n"
	"  End\n"
	"  Behavior = GettingBuiltBehavior ModuleTag_Build\n"
	"    RebuildTimeSeconds = 10\n"
	"  End\n"
	"End\n";

struct Local96World : combattest::CombatWorld
{
	Local96World() : CombatWorld(kLocal96Ini)
	{
		logic->productionSettings() = prodtest::defaultSettings();
		logic->settings().buildRulesLoaded = true;
		playerOf('A')->depositMoney(1000, false);
		frames(3);
	}

	Object *spawnAt(const char *name, float x, float y)
	{
		Object *o = spawn(name, x, y, 0.0f, teamA());
		REQUIRE(o != nullptr);
		return o;
	}
};
} // namespace

TEST_CASE("LOCAL-96 P-157: DozerAIUpdate::onOwnerDie (RW 0x0088BF7A) removes unplaced phantom and refunds cost on builder death")
{
	Local96World w;
	Object *porter = w.spawnAt("L96Porter", 200.0f, 200.0f);
	DozerAIUpdate *dozer = dynamic_cast<DozerAIUpdate *>(porter->getAIUpdateInterface());
	REQUIRE(dozer != nullptr);

	const ThingTemplate *bldgTmpl = w.w.things.findTemplate("L96Structure");
	REQUIRE(bldgTmpl != nullptr);

	const std::uint32_t moneyBefore = (std::uint32_t)w.cash('A');
	const Coord3D site{ 500.0f, 500.0f, 0.0f }; // far enough that the porter has not reached it yet
	Object *phantom = dozer->construct(*bldgTmpl, site, 0.0f, *w.playerOf('A'), false);
	REQUIRE(phantom != nullptr);
	const ObjectID phantomId = phantom->getID();
	REQUIRE(dozer->unplacedStructure() == phantomId);
	REQUIRE((std::uint32_t)w.cash('A') == moneyBefore - 300);

	// Kill the porter mid-march while SlowDeathBehavior keeps its corpse alive for 30s (DestructionDelay = 30000).
	porter->kill(DAMAGE_UNRESISTABLE, DEATH_NORMAL);
	// RW 0x00698FB5 -> RW 0x0088BF7A removes the phantom immediately in the death frame, before SlowDeathBehavior deletes the corpse.
	CHECK(porter->isEffectivelyDead());
	CHECK_FALSE(porter->isDestroyed());
	CHECK(dozer->unplacedStructure() == INVALID_ID);
	CHECK((std::uint32_t)w.cash('A') == moneyBefore);
	w.frames(1);
	CHECK(w.logic->findObjectByID(phantomId) == nullptr);
}

TEST_CASE("LOCAL-96 P-157 neighbour: DozerAIUpdate::onOwnerDie with no phantom leaves already-placed structure intact")
{
	Local96World w;
	Object *porter = w.spawnAt("L96Porter", 480.0f, 500.0f);
	DozerAIUpdate *dozer = dynamic_cast<DozerAIUpdate *>(porter->getAIUpdateInterface());
	REQUIRE(dozer != nullptr);

	const ThingTemplate *bldgTmpl = w.w.things.findTemplate("L96Structure");
	REQUIRE(bldgTmpl != nullptr);

	const Coord3D site{ 500.0f, 500.0f, 0.0f };
	Object *structure = dozer->construct(*bldgTmpl, site, 0.0f, *w.playerOf('A'), false);
	REQUIRE(structure != nullptr);
	const ObjectID structureId = structure->getID();

	// Advance until the porter places the phantom (m_unplacedStructure becomes INVALID_ID).
	for (int i = 0; i < 60 && dozer->unplacedStructure() != INVALID_ID; ++i)
	{
		w.frames(1);
	}
	REQUIRE(dozer->unplacedStructure() == INVALID_ID);

	porter->kill(DAMAGE_UNRESISTABLE, DEATH_NORMAL);
	w.frames(1);
	// Already-placed structure stays alive on the map.
	Object *stillThere = w.logic->findObjectByID(structureId);
	REQUIRE(stillThere != nullptr);
	CHECK_FALSE(stillThere->isEffectivelyDead());
}

TEST_CASE("LOCAL-96 onDelete (RW 0x0088CF96): deleting a dozer cancels pending tasks")
{
	Local96World w;
	Object *porter = w.spawnAt("L96Porter", 200.0f, 200.0f);
	Object *bldg = w.spawnAt("L96Structure", 500.0f, 500.0f);
	DozerAIUpdate *dozer = dynamic_cast<DozerAIUpdate *>(porter->getAIUpdateInterface());
	REQUIRE(dozer != nullptr);

	ActiveBody *body = dynamic_cast<ActiveBody *>(bldg->getBodyModule());
	REQUIRE(body != nullptr);
	body->internalChangeHealth(-100.0f);
	REQUIRE(dozer->repair(*bldg));
	REQUIRE(dozer->isTaskPending(DozerAIUpdate::DOZER_TASK_REPAIR));

	dozer->onDelete();
	CHECK_FALSE(dozer->isTaskPending(DozerAIUpdate::DOZER_TASK_REPAIR));
	CHECK(dozer->getCurrentTask() == DozerAIUpdate::DOZER_TASK_INVALID);
}

TEST_CASE("LOCAL-96 canAcceptNewRepair (RW 0x0088CB8A / RW 0x008AE0FE): null structure is refused")
{
	Local96World w;
	Object *porter = w.spawnAt("L96Porter", 200.0f, 200.0f);
	Object *workerObj = w.spawnAt("L96Worker", 240.0f, 200.0f);
	DozerAIUpdate *dozer = dynamic_cast<DozerAIUpdate *>(porter->getAIUpdateInterface());
	WorkerAIUpdate *worker = dynamic_cast<WorkerAIUpdate *>(workerObj->getAIUpdateInterface());
	REQUIRE(dozer != nullptr);
	REQUIRE(worker != nullptr);

	CHECK_FALSE(dozer->canAcceptNewRepair(nullptr));
	CHECK_FALSE(worker->canAcceptNewRepair(nullptr));
}

TEST_CASE("LOCAL-96 WorkerAIUpdate::newTask (RW 0x008AE4A9) and internalTaskCompleteOrCancelled (RW 0x008AE17C) toggle WORKER_REPAIRING (0x61)")
{
	Local96World w;
	Object *workerObj = w.spawnAt("L96Worker", 200.0f, 200.0f);
	Object *bldg = w.spawnAt("L96Structure", 500.0f, 500.0f);
	WorkerAIUpdate *worker = dynamic_cast<WorkerAIUpdate *>(workerObj->getAIUpdateInterface());
	REQUIRE(worker != nullptr);

	const int kWorkerRepairing = ObjectTemplateInfoBuilder::objectStatusIndex("WORKER_REPAIRING");
	REQUIRE(kWorkerRepairing == 0x61);
	CHECK_FALSE(workerObj->testStatus((unsigned)kWorkerRepairing));

	ActiveBody *body = dynamic_cast<ActiveBody *>(bldg->getBodyModule());
	REQUIRE(body != nullptr);
	body->internalChangeHealth(-100.0f);

	// Ordering a repair enters WorkerAIUpdate::newTask (RW 0x008AE4A9) and sets WORKER_REPAIRING (0x61).
	REQUIRE(worker->repair(*bldg));
	CHECK(workerObj->testStatus((unsigned)kWorkerRepairing));

	// Cancelling the task enters WorkerAIUpdate::cancelTask (RW 0x008ACEE3) -> internalCancelTask (RW 0x008AE74B)
	// -> internalTaskCompleteOrCancelled (RW 0x008AE17C) and clears WORKER_REPAIRING (0x61).
	worker->cancelTask();
	CHECK_FALSE(workerObj->testStatus((unsigned)kWorkerRepairing));
	CHECK_FALSE(worker->isTaskPending(DozerAIUpdate::DOZER_TASK_REPAIR));
}

TEST_CASE("LOCAL-96 ENNORATH scenario: MenPorter killed after placing GondorFarm removes phantom immediately and refunds cost (P-157, RW 0x0088BF7A)")
{
	ennorathscenario::World world;
	if (!world.load("LOCAL-96 ENNORATH scenario"))
	{
		return;
	}
	hudtest::Rig game(world.shared);
	const int player1 = game.index();
	Player *p1 = game.game->players().getNthPlayer(player1);
	REQUIRE(p1 != nullptr);
	p1->depositMoney(5000, false);

	std::string error;
	Object *porter = game.game->createObject("MenPorter", player1, Coord3D{ 1200.0f, 1200.0f, 0.0f }, 0.0f, &error);
	REQUIRE_MESSAGE(porter != nullptr, error);
	DozerAIUpdate *dozer = dynamic_cast<DozerAIUpdate *>(porter->getAIUpdateInterface());
	REQUIRE(dozer != nullptr);

	const ThingTemplate *farmTmpl = world.shared.world->things().findTemplate("GondorFarm");
	REQUIRE(farmTmpl != nullptr);

	game.logic().runLogicFrame();
	game.logic().runLogicFrame();
	game.logic().runLogicFrame();

	// Pick a legal build site ~200..400 units away from the porter so it has to walk there.
	Coord3D site{ 0.0f, 0.0f, 0.0f };
	bool foundSite = false;
	for (float dx : { 250.0f, -250.0f, 320.0f, -320.0f, 0.0f })
	{
		for (float dy : { 0.0f, 250.0f, -250.0f, 320.0f, -320.0f })
		{
			if (dx == 0.0f && dy == 0.0f) continue;
			Coord3D cand{ 1200.0f + dx, 1200.0f + dy, 0.0f };
			if (dozer->siteIsLegal(*farmTmpl, cand, 0.0f, *p1))
			{
				site = cand;
				foundSite = true;
				break;
			}
		}
		if (foundSite) break;
	}
	REQUIRE(foundSite);

	const std::uint32_t moneyBefore = p1->getMoney()->countMoney();
	Object *phantom = dozer->construct(*farmTmpl, site, 0.0f, *p1, false);
	REQUIRE(phantom != nullptr);
	const ObjectID phantomId = phantom->getID();
	REQUIRE(dozer->unplacedStructure() == phantomId);
	const std::uint32_t moneyAfterOrder = p1->getMoney()->countMoney();
	REQUIRE(moneyAfterOrder < moneyBefore); // tolerance: precondition that GondorFarm build cost was charged before checking exact refund to moneyBefore

	// Kill MenPorter while walking to the site: SlowDeathBehavior keeps the corpse alive,
	// so without RW 0x0088BF7A the phantom would linger until SlowDeathBehavior deletes the corpse.
	porter->kill(DAMAGE_UNRESISTABLE, DEATH_NORMAL);
	CHECK(porter->isEffectivelyDead());
	CHECK_FALSE(porter->isDestroyed());
	CHECK(dozer->unplacedStructure() == INVALID_ID);
	CHECK(p1->getMoney()->countMoney() == moneyBefore);
	game.logic().runLogicFrame();
	CHECK(game.logic().findObjectByID(phantomId) == nullptr);
}
