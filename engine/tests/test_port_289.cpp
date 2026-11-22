// PORT-289 (fork issue 677): ActionManager::canRepairObject (RW 0x82D7F2).
// Synthetic templates exercise the retail predicate and the existing porter
// repair path; no ROTWK files are required.

#include "doctest.h"
#include "CombatTestUtil.h"

#include "Common/SpecialPower.h"
#include "GameLogic/ActionManager.h"
#include "GameLogic/Module/ActiveBody.h"
#include "GameLogic/Module/DozerAIUpdate.h"
#include "GameLogic/Module/RepairSpecialPower.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Object/Object.h"

namespace
{
const char kPort289Data[] = R"(
SpecialPower SpecialRepairStructure
  Enum = SPECIAL_REPAIR_STRUCTURE
  ReloadTime = 0
End
Object Port289Dozer
  KindOf = DOZER SELECTABLE
  Geometry = CYLINDER
  GeometryMajorRadius = 8
  GeometryMinorRadius = 8
  GeometryHeight = 16
  Body = ActiveBody ModuleTag_Body
    MaxHealth = 100
  End
  Behavior = DozerAIUpdate ModuleTag_AI
    RepairHealthPercentPerSecond = 100%
  End
  Behavior = RepairSpecialPower ModuleTag_Repair
    SpecialPowerTemplate = SpecialRepairStructure
  End
End
Object MordorWorker
  KindOf = DOZER SELECTABLE
  Geometry = CYLINDER
  GeometryMajorRadius = 8
  GeometryMinorRadius = 8
  GeometryHeight = 16
  Body = ActiveBody ModuleTag_Body
    MaxHealth = 100
  End
End
Object port289mordorworker
  KindOf = DOZER SELECTABLE
  Geometry = CYLINDER
  GeometryMajorRadius = 8
  GeometryMinorRadius = 8
  GeometryHeight = 16
  Body = ActiveBody ModuleTag_Body
    MaxHealth = 100
  End
End
Object Port289Unit
  KindOf = INFANTRY SELECTABLE
  Geometry = CYLINDER
  GeometryMajorRadius = 8
  GeometryMinorRadius = 8
  GeometryHeight = 16
  Body = ActiveBody ModuleTag_Body
    MaxHealth = 100
  End
End
Object Port289Structure
  KindOf = STRUCTURE SELECTABLE
  Geometry = BOX
  GeometryMajorRadius = 20
  GeometryMinorRadius = 20
  GeometryHeight = 20
  Body = ActiveBody ModuleTag_Body
    MaxHealth = 1000
  End
End
Object Port289Bridge
  KindOf = STRUCTURE BRIDGE SELECTABLE
  Geometry = BOX
  GeometryMajorRadius = 20
  GeometryMinorRadius = 20
  GeometryHeight = 20
  Body = ActiveBody ModuleTag_Body
    MaxHealth = 1000
  End
End
Object Port289BridgeTower
  KindOf = STRUCTURE BRIDGE_TOWER SELECTABLE
  Geometry = BOX
  GeometryMajorRadius = 20
  GeometryMinorRadius = 20
  GeometryHeight = 20
  Body = ActiveBody ModuleTag_Body
    MaxHealth = 1000
  End
End
Object Port289RebuildHole
  KindOf = STRUCTURE REBUILD_HOLE SELECTABLE
  Geometry = BOX
  GeometryMajorRadius = 20
  GeometryMinorRadius = 20
  GeometryHeight = 20
  Body = ActiveBody ModuleTag_Body
    MaxHealth = 1000
  End
End
Object Port289WallUpgrade
  KindOf = STRUCTURE WALL_UPGRADE SELECTABLE
  Geometry = BOX
  GeometryMajorRadius = 20
  GeometryMinorRadius = 20
  GeometryHeight = 20
  Body = ActiveBody ModuleTag_Body
    MaxHealth = 1000
  End
End
Object Port289SelfRepair
  KindOf = STRUCTURE SELECTABLE
  Geometry = BOX
  GeometryMajorRadius = 20
  GeometryMinorRadius = 20
  GeometryHeight = 20
  Body = ActiveBody ModuleTag_Body
    MaxHealth = 1000
  End
  Behavior = GettingBuiltBehavior ModuleTag_SelfRepair
  End
End
Object Port289DeadKept
  KindOf = STRUCTURE SELECTABLE
  KeepSelectableWhenDead = Yes
  Geometry = BOX
  GeometryMajorRadius = 20
  GeometryMinorRadius = 20
  GeometryHeight = 20
  Body = ActiveBody ModuleTag_Body
    MaxHealth = 1000
  End
End
)";

struct Rig : combattest::CombatWorld
{
	SpecialPowerStore powers;
	SpecialPowerStore *previousPowers = TheSpecialPowerStore;

	Rig()
	{
		TheSpecialPowerStore = &powers;
		w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
		const std::string error = w.load(kPort289Data, INI_LOAD_OVERWRITE, "port289.ini");
		REQUIRE_MESSAGE(error.empty(), error);
	}

	~Rig() { TheSpecialPowerStore = previousPowers; }

	Object *place(const char *name, Team *team, float x, float y, ObjectID id = INVALID_ID)
	{
		Object *o = make(name, team, id, false);
		REQUIRE(o != nullptr);
		Coord3D p{ x, y, 0.0f };
		o->setPosition(&p);
		logictest::LogicWorld::completeHorde(o);
		return o;
	}

	Object *place(const char *name, char side = 'A', float x = 100.0f, float y = 100.0f)
	{
		return place(name, side == 'A' ? teamA() : teamB(), x, y);
	}

	static void damage(Object &object, float health)
	{
		ActiveBody *body = dynamic_cast<ActiveBody *>(object.getBodyModule());
		REQUIRE(body != nullptr);
		body->friend_setHealthRaw(health);
	}

	static void markDead(Object &object)
	{
		object.friend_setEffectivelyDead(true);
	}

	const SpecialPowerTemplate *repairPower() const
	{
		const SpecialPowerTemplate *power = TheSpecialPowerStore->findSpecialPowerTemplate("SpecialRepairStructure");
		REQUIRE(power != nullptr);
		return power;
	}
};
} // namespace

TEST_CASE("PORT-289 canRepairObject follows the native KindOf words and exact exceptions")
{
	Rig r;
	Object *dozer = r.place("Port289Dozer");
	Object *target = r.place("Port289Structure", 'A', 140.0f, 100.0f);
	r.damage(*target, 500.0f);
	REQUIRE(ActionManager::canRepairObject(dozer, target, CMD_FROM_PLAYER));

	CHECK_FALSE(ActionManager::canRepairObject(nullptr, target, CMD_FROM_PLAYER));
	CHECK_FALSE(ActionManager::canRepairObject(dozer, nullptr, CMD_FROM_PLAYER));
	r.damage(*target, 1000.0f);
	CHECK_FALSE(ActionManager::canRepairObject(dozer, target, CMD_FROM_PLAYER)); // RW 0x82D8A4: health == maxHealth
	r.damage(*target, 500.0f);
	CHECK_FALSE(ActionManager::canRepairObject(dozer, r.place("Port289Structure", 'B', 140.0f, 120.0f), CMD_FROM_PLAYER));
	CHECK_FALSE(ActionManager::canRepairObject(dozer, r.place("Port289Unit", 'A', 140.0f, 140.0f), CMD_FROM_PLAYER));

	Object *mordor = r.place("MordorWorker");
	CHECK_FALSE(ActionManager::canRepairObject(mordor, target, CMD_FROM_PLAYER)); // RW 0x82D815: StringBase::compare
	Object *differentCase = r.place("port289mordorworker");
	CHECK(ActionManager::canRepairObject(differentCase, target, CMD_FROM_PLAYER));

	for (const char *name : { "Port289Bridge", "Port289BridgeTower", "Port289RebuildHole", "Port289WallUpgrade" })
	{
		Object *special = r.place(name, 'A', 140.0f, 160.0f);
		r.damage(*special, 500.0f);
		CHECK_FALSE(ActionManager::canRepairObject(dozer, special, CMD_FROM_PLAYER));
	}

	target->setStatus(OBJECT_STATUS_UNDER_CONSTRUCTION, true);
	CHECK_FALSE(ActionManager::canRepairObject(dozer, target, CMD_FROM_PLAYER));
	target->setStatus(OBJECT_STATUS_UNDER_CONSTRUCTION, false);
	dozer->setStatus(OBJECT_STATUS_UNDER_CONSTRUCTION, true);
	CHECK_FALSE(ActionManager::canRepairObject(dozer, target, CMD_FROM_PLAYER));
	dozer->setStatus(OBJECT_STATUS_UNDER_CONSTRUCTION, false);

	Object *selfRepair = r.place("Port289SelfRepair", 'A', 140.0f, 180.0f);
	r.damage(*selfRepair, 500.0f);
	CHECK_FALSE(ActionManager::canRepairObject(dozer, selfRepair, CMD_FROM_PLAYER)); // slot-32 -> slot-9
	CHECK(ActionManager::canRepairObject(dozer, selfRepair, CMD_FROM_AI)); // the direct AI repair path keeps the existing worker-task behavior
}

TEST_CASE("PORT-289 dead targets, active sole healers and shroud command sources use the native gates")
{
	Rig r;
	Object *dozer = r.place("Port289Dozer");
	Object *plainDead = r.place("Port289Structure", 'A', 140.0f, 100.0f);
	r.damage(*plainDead, 500.0f);
	r.markDead(*plainDead);
	CHECK_FALSE(ActionManager::canRepairObject(dozer, plainDead, CMD_FROM_PLAYER));

	Object *keptDead = r.place("Port289DeadKept", 'A', 140.0f, 120.0f);
	r.damage(*keptDead, 500.0f);
	r.markDead(*keptDead);
	CHECK(ActionManager::canRepairObject(dozer, keptDead, CMD_FROM_PLAYER)); // +0x632 KeepSelectableWhenDead
	CHECK(ActionManager::canDoSpecialPowerAtObject(*dozer, keptDead, CMD_FROM_PLAYER, r.repairPower(), 0, true));

	Object *otherDozer = r.place("Port289Dozer", 'A', 80.0f, 100.0f);
	Object *target = r.place("Port289Structure", 'A', 140.0f, 140.0f);
	r.damage(*target, 500.0f);
	r.logic->runLogicFrame(); // a fresh object's zero expiry is still active on frame 0
	REQUIRE(target->attemptHealingFromSoleBenefactor(1.0f, otherDozer, 20));
	CHECK_FALSE(ActionManager::canRepairObject(dozer, target, CMD_FROM_PLAYER));
	for (int i = 0; i < 21; ++i)
	{
		r.logic->runLogicFrame();
	}
	CHECK(ActionManager::canRepairObject(dozer, target, CMD_FROM_PLAYER));
	CHECK(ActionManager::canRepairObject(dozer, target, CMD_FROM_SCRIPT)); // scripts also bypass the shroud helper
}

TEST_CASE("PORT-289 type 0x8A and RepairSpecialPower share canRepairObject and keep the existing healing path")
{
	Rig r;
	Object *dozer = r.place("Port289Dozer");
	Object *target = r.place("Port289Structure", 'A', 140.0f, 100.0f);
	r.damage(*target, 500.0f);
	const SpecialPowerTemplate *power = r.repairPower();
	bool unported = true;
	CHECK(ActionManager::canDoSpecialPowerAtObject(*dozer, target, CMD_FROM_PLAYER, power, 0, true, &unported));
	CHECK_FALSE(unported);

	RepairSpecialPower *repair = dynamic_cast<RepairSpecialPower *>(dozer->findModule("RepairSpecialPower"));
	DozerAIUpdate *ai = dynamic_cast<DozerAIUpdate *>(dozer->getAIUpdateInterface());
	REQUIRE(repair != nullptr);
	REQUIRE(ai != nullptr);
	const float before = target->getBodyModule()->getHealth();
	r.logic->runLogicFrame(); // frame 0 has orderFrame == 0, which the dozer's most-recent-task scan intentionally skips
	repair->doSpecialPowerAtObject(target, 0);
	CHECK(repair->repairs() == 1);
	CHECK(ai->repairing());
	for (int i = 0; i < 300 && target->getBodyModule()->getHealth() <= before; ++i)
	{
		r.logic->runLogicFrame();
	}
	// tolerance: the exact delta depends on the task's frame scheduling; positivity proves the existing repair task healed.
	CHECK(target->getBodyModule()->getHealth() > before);

	r.damage(*target, target->getBodyModule()->getMaxHealth());
	CHECK_FALSE(ActionManager::canDoSpecialPowerAtObject(*dozer, target, CMD_FROM_PLAYER, power, 0, true));
	CHECK_FALSE(ActionManager::canRepairObject(dozer, target, CMD_FROM_PLAYER));
}
