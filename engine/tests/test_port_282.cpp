// OpenBFME. GPL-3.0.
// Port 282 (fork issue #649, Arena task 263): the repair orders of the two builder classes of RotWK 2.01 `game.dat`
// (md5 `daed3668006cd90f01c34e5a7da1901f`), read with `tools/mod/rw.py dis` (caveat S-001). Two of the functions the
// function map left uncited were behaviour, not scaffolding, and this port adds them:
//
// 1. the dozer interface's slot 0x50, `canAcceptNewRepair` (RW 0x88CB8A). DozerAIUpdate::repair (RW 0x88BD8D, the dozer
//    interface's slot 0xB0) asks it FIRST (RW 0x88BDA2), before the ActionManager's test; WorkerAIUpdate::repair
//    (RW 0x8AD2D3, the same slot of the worker's own interface vtable RW 0xC6AC18) asks it for every worker that is not a
//    SWARM_DOZER (RW 0x8AD2F7: template byte + 0x109 bit 7). The body: the object is not there -> accept; the builder's
//    current task (interface slot 0x24) is not REPAIR -> accept; the structure the running REPAIR task holds (slot 0x1C,
//    + 0xC) is the ordered one -> refuse; two bridge towers (KindOf word + 0x108 bit 24 both, RW 0x85B874's bridge module
//    ids compared, 0 == 0 through no bridge module) -> refuse; else accept.
// 2. the worker's own repair order (RW 0x8AD2D3): the ActionManager's test, then, unless SWARM_DOZER, the slot 0x50
//    test and the sole healer test (RW 0x68B8D3), then newTask(REPAIR) - the worker does not ask the base
//    DozerAIUpdate::repair, so its module data can differ from a dozer's and its SWARM_DOZER members may share a target.
//
// The other uncited functions of the two classes are classified in `docs/ennorath/night_watch.d/PORT-282.md`: scaffolding
// (destructors, xfer, the ctor's tables, the debug string), already ported under another name (the base slots, the other
// lanes' bodies), or the client's (the bridge module, the bored wander's `AI_BORED` state entered through the AI vslot
// 0x1F0, S-1282). Synthetic INI only: no retail data, no ROTWK_INSTALL / OPENBFME_ENNORATATH_INSTALL.
#include "doctest.h"

#include "CombatTestUtil.h"
#include "ProdTestUtil.h"

#include "Common/Player.h"
#include "GameLogic/Module/ActiveBody.h"
#include "GameLogic/ObjectTemplateInfo.h"
#include "GameLogic/Module/DozerAIUpdate.h"
#include "GameLogic/Module/WorkerAIUpdate.h"
#include "GameLogic/Object/Object.h"

#include <string>

namespace
{
// A Porter of the port (KindOf DOZER) with the dozer module, and the evil factions' worker shape (KindOf DOZER SWARM_DOZER and
// the same module) whose own repair order RW 0x8AD2D3 overrides the slot. Both walk on MoveTestUtil's WalkerLoco.
const char kPort282Objects[] =
	"Object P282Porter\n"
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
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 40\n"
	"  End\n"
	"End\n"
	"Object P282Worker\n"
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
	"Object P282Building\n"
	"  KindOf = STRUCTURE SELECTABLE IMMOBILE\n"
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
	"End\n";

// A pair of bridge towers (KindOf BRIDGE_TOWER): the template's + 0x108 bit 24, the test RW 0x88CB8A's bridge branch reads.
// The port has no bridge module (S-161), so their ids, both NULL, are equal: an order for the second tower of a bridge is
// refused while the first one is being repaired, exactly as RW 0x88CBF9 compares the two NULL ids.
const char kPort282Bridges[] =
	"Object P282BridgeTowerA\n"
	"  KindOf = STRUCTURE SELECTABLE IMMOBILE BRIDGE_TOWER\n"
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
	"End\n"
	"Object P282BridgeTowerB\n"
	"  KindOf = STRUCTURE SELECTABLE IMMOBILE BRIDGE_TOWER\n"
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
	"End\n";

// the two INI blocks as one document (CombatWorld takes a C string: the buffer lives as long as the fixture)
const std::string &port282Ini()
{
	static const std::string ini = std::string(kPort282Objects) + kPort282Bridges;
	return ini;
}

struct Port282World : combattest::CombatWorld
{
	Port282World() : CombatWorld(port282Ini().c_str())
	{
		logic->productionSettings() = prodtest::defaultSettings();
		logic->settings().buildRulesLoaded = true;
		// a task ordered on frame 0 has no "most recent" order frame yet (RW 0x88BE47 compares strictly), so the world runs
		// three frames first, as the other lane fixtures do
		frames(3);
	}

	Object *spawnAt(const char *name, float x, float y)
	{
		Object *o = spawn(name, x, y, 0.0f, teamA());
		REQUIRE(o != nullptr);
		return o;
	}
	DozerAIUpdate *dozer(Object *o)
	{
		DozerAIUpdate *d = dynamic_cast<DozerAIUpdate *>(o->getAIUpdateInterface());
		REQUIRE(d != nullptr);
		return d;
	}
	WorkerAIUpdate *worker(Object *o)
	{
		WorkerAIUpdate *w = dynamic_cast<WorkerAIUpdate *>(o->getAIUpdateInterface());
		REQUIRE(w != nullptr);
		return w;
	}
	// the structure is damaged but alive: the ActionManager's test (RW 0x82D7F2, stood for by the port's canRepair) accepts it
	void damage(Object *building, float fraction)
	{
		ActiveBody *body = dynamic_cast<ActiveBody *>(building->getBodyModule());
		REQUIRE(body != nullptr);
		body->internalChangeHealth(-fraction * body->getMaxHealth());
		REQUIRE(body->getHealth() < body->getMaxHealth()); // tolerance: a precondition (the damage landed at all); the last case asserts the exact heal claim below
	}
	template <typename Fn> bool until(int limit, Fn ok)
	{
		for (int i = 0; i < limit; ++i)
		{
			if (ok())
			{
				return true;
			}
			frames(1);
		}
		return ok();
	}
};
} // namespace

TEST_CASE("PORT-282 canAcceptNewRepair: with no task, or none on REPAIR, an order is accepted (RW 0x88CB9B)")
{
	Port282World w;
	Object *porter = w.spawnAt("P282Porter", 600.0f, 600.0f);
	Object *building = w.spawnAt("P282Building", 900.0f, 600.0f);
	DozerAIUpdate *d = w.dozer(porter);
	// no task at all: RW 0x88CB96 reads the current task through slot 0x24 and its 1 (REPAIR) test fails
	CHECK(d->canAcceptNewRepair(building));
	CHECK(d->getCurrentTask() == DozerAIUpdate::DOZER_TASK_INVALID);
	// a BUILD task (an unfinished structure of the builder's own player, RW 0x82C84C's stand-in): still not REPAIR, so the
	// ordered repair of any structure is accepted, the very structure being built included
	Object *unfinished = w.spawnAt("P282Building", 600.0f, 900.0f);
	unfinished->setStatus((unsigned)ObjectTemplateInfoBuilder::objectStatusIndex("UNDER_CONSTRUCTION"), true);
	REQUIRE(d->resumeConstruction(*unfinished));
	CHECK(d->getCurrentTask() == DozerAIUpdate::DOZER_TASK_BUILD);
	CHECK(d->canAcceptNewRepair(building));
	CHECK(d->canAcceptNewRepair(unfinished));
}

TEST_CASE("PORT-282 canAcceptNewRepair refuses a second order for the structure the running repair already holds (RW 0x88CBB4)")
{
	Port282World w;
	Object *porter = w.spawnAt("P282Porter", 600.0f, 600.0f);
	Object *building = w.spawnAt("P282Building", 900.0f, 600.0f);
	DozerAIUpdate *d = w.dozer(porter);
	w.damage(building, 0.5f);
	REQUIRE(d->repair(*building));
	REQUIRE(d->getCurrentTask() == DozerAIUpdate::DOZER_TASK_REPAIR);
	// the task's target is the building (slot 0x1C of the interface, + 0xC): the same structure is refused
	CHECK_FALSE(d->canAcceptNewRepair(building));
	CHECK_FALSE(d->repair(*building));
	// the running task is untouched: the porter still repairs that building
	CHECK(d->getCurrentTask() == DozerAIUpdate::DOZER_TASK_REPAIR);
	CHECK(d->getTaskTarget(DozerAIUpdate::DOZER_TASK_REPAIR) == building->getID());
}

TEST_CASE("PORT-282 canAcceptNewRepair accepts an order for another structure that is not a bridge tower (RW 0x88CBC4)")
{
	Port282World w;
	Object *porter = w.spawnAt("P282Porter", 600.0f, 600.0f);
	Object *first = w.spawnAt("P282Building", 900.0f, 600.0f);
	Object *second = w.spawnAt("P282Building", 900.0f, 900.0f);
	DozerAIUpdate *d = w.dozer(porter);
	w.damage(first, 0.5f);
	w.damage(second, 0.5f);
	REQUIRE(d->repair(*first));
	REQUIRE(d->getCurrentTask() == DozerAIUpdate::DOZER_TASK_REPAIR);
	// another building while the first is repaired: RW 0x88CBC4 (neither is a BRIDGE_TOWER), accepted
	CHECK(d->canAcceptNewRepair(second));
	CHECK(d->repair(*second));
	// the new order replaced the task (newTask RW 0x88CCFC replaces the pending target of the same task)
	CHECK(d->getTaskTarget(DozerAIUpdate::DOZER_TASK_REPAIR) == second->getID());
}

TEST_CASE("PORT-282 canAcceptNewRepair refuses the other tower of a bridge while its pair is repaired (RW 0x88CBF9, both bridge ids NULL)")
{
	Port282World w;
	Object *porter = w.spawnAt("P282Porter", 600.0f, 600.0f);
	Object *towerA = w.spawnAt("P282BridgeTowerA", 900.0f, 600.0f);
	Object *towerB = w.spawnAt("P282BridgeTowerB", 900.0f, 900.0f);
	REQUIRE(towerA->isKindOfName("BRIDGE_TOWER"));
	REQUIRE(towerB->isKindOfName("BRIDGE_TOWER"));
	DozerAIUpdate *d = w.dozer(porter);
	w.damage(towerA, 0.5f);
	w.damage(towerB, 0.5f);
	REQUIRE(d->repair(*towerA));
	REQUIRE(d->getCurrentTask() == DozerAIUpdate::DOZER_TASK_REPAIR);
	// two bridge towers whose bridge module ids are equal (no bridge module in the port: NULL == NULL): refused
	CHECK_FALSE(d->canAcceptNewRepair(towerB));
	CHECK_FALSE(d->repair(*towerB));
	CHECK(d->getTaskTarget(DozerAIUpdate::DOZER_TASK_REPAIR) == towerA->getID());
	// ... while a non-bridge structure is still accepted (the bridge branch alone decides)
	Object *building = w.spawnAt("P282Building", 1200.0f, 600.0f);
	w.damage(building, 0.5f);
	CHECK(d->canAcceptNewRepair(building));
}

TEST_CASE("PORT-282 the worker's repair order (RW 0x8AD2D3) takes the structure, and a SWARM_DOZER skips the two gates (RW 0x8AD2F7)")
{
	Port282World w;
	Object *worker = w.spawnAt("P282Worker", 600.0f, 600.0f);
	REQUIRE(worker->isKindOfName("SWARM_DOZER"));
	Object *building = w.spawnAt("P282Building", 900.0f, 600.0f);
	WorkerAIUpdate *wr = w.worker(worker);
	w.damage(building, 0.5f);
	// the worker's own override runs (not the dozer's 0x88BD8D): the repair task holds the building
	REQUIRE(wr->repair(*building));
	CHECK(wr->getCurrentTask() == DozerAIUpdate::DOZER_TASK_REPAIR);
	CHECK(wr->getTaskTarget(DozerAIUpdate::DOZER_TASK_REPAIR) == building->getID());
	CHECK(wr->repairing());
	// a SWARM_DOZER skips the slot 0x50 and sole healer gates, so a second order for another structure is taken even though the
	// first repair runs; and the sole healer of the first structure does NOT block it
	Object *second = w.spawnAt("P282Building", 900.0f, 900.0f);
	w.damage(second, 0.5f);
	CHECK(wr->repair(*second));
	CHECK(wr->getTaskTarget(DozerAIUpdate::DOZER_TASK_REPAIR) == second->getID());
}

TEST_CASE("PORT-282 a non-swarm builder refuses the repair when the structure has another sole healer (RW 0x8AD31B / 0x88BDBE)")
{
	Port282World w;
	Object *porter = w.spawnAt("P282Porter", 600.0f, 600.0f);
	Object *other = w.spawnAt("P282Porter", 640.0f, 600.0f);
	Object *building = w.spawnAt("P282Building", 900.0f, 600.0f);
	DozerAIUpdate *d = w.dozer(porter);
	w.damage(building, 0.5f);
	// the other builder heals it first: the sole healer (RW 0x690584's expiry, two frames of the repair) is the other builder
	ActiveBody *body = dynamic_cast<ActiveBody *>(building->getBodyModule());
	REQUIRE(body != nullptr);
	CHECK(building->attemptHealingFromSoleBenefactor(0.01f, other, 2)); // RW 0x690584 through the repair rate's gate
	REQUIRE(building->soleHealingBenefactor() == other->getID());
	// the first builder's order is refused: RW 0x88BDBE (dozer) / 0x8AD31B (worker) compare the healer with itself
	CHECK_FALSE(d->repair(*building));
	CHECK_FALSE(d->repairing());
	// ... and the same structure is accepted once the claim runs out (RW: the expiry, two frames after the claim)
	// tolerance: the claim's expiry is the frame + 2 (RW 0x690584's duration), so the loop is a wait, not an expected value
	CHECK(w.until(6, [&] { return building->logic().getFrame() > building->soleHealingBenefactorExpiry(); }));
	CHECK(d->repair(*building));
	CHECK(d->repairing());
}
