// OpenBFME. GPL-3.0.
//
// LOCAL-101 (fork issue 771): special AI modules — SiegeDockingBehavior::loadPostProcess (RW 0x0085D1CD)
// and HordeWorkerAIUpdate repair / enter-after-repair / command lifecycle (RW 0x0089E669, 0x0089E7C1,
// 0x0089E990, 0x0089E9F8, 0x0089E623, 0x0089E57C, 0x0089E884, 0x0089E5CC, 0x0082D7F2).
// Editions: both RotWK 2.01 and BFME2 1.06.
// Expectations sourced from RotWK 2.01 game.dat disassembly and Ennorath 1.9.8.3 INI (neutralunits.ini).

#include "doctest.h"

#include "CombatTestUtil.h"
#include "EnnorathScenario.h"
#include "HudTestUtil.h"

#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/StateHash.h"
#include "Common/XferLoad.h"
#include "Common/XferSave.h"
#include "GameLogic/AI/AIGarrisonStates.h"
#include "GameLogic/AI/AIStateMachine.h"
#include "GameLogic/Damage.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/ActiveBody.h"
#include "GameLogic/Module/DozerAIUpdate.h"
#include "GameLogic/Module/HordeWorkerAIUpdate.h"
#include "GameLogic/Module/SiegeModules.h"
#include "GameLogic/Object/Contain/HordeContainRuntime.h"
#include "GameLogic/Object/Object.h"

namespace
{
const char *const kLocal101Ini =
	"Object TestDockWall\n"
	"  KindOf = STRUCTURE IMMOBILE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 20.0\n"
	"  GeometryMinorRadius = 10.0\n"
	"  GeometryHeight = 30.0\n"
	"  Body = ActiveBody BodyTag\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = SiegeDockingBehavior ModuleTag_Dock\n"
	"  End\n"
	"End\n"
	"Object TestPeasantMember\n"
	"  KindOf = PRELOAD SELECTABLE INFANTRY DOZER\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 5.0\n"
	"  GeometryHeight = 15.0\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 50\n"
	"  End\n"
	"  Body = ActiveBody BodyTag\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = DozerAIUpdate ModuleTag_DozerAI\n"
	"    RepairHealthPercentPerSecond = 10%\n"
	"    BoredTime = 5000\n"
	"    BoredRange = 150\n"
	"  End\n"
	"End\n"
	"Object TestPeasantHorde\n"
	"  KindOf = PRELOAD SELECTABLE INFANTRY HORDE DOZER\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 15.0\n"
	"  GeometryMinorRadius = 15.0\n"
	"  GeometryHeight = 15.0\n"
	"  LocomotorSet\n"
	"    Locomotor = HordeLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 50\n"
	"  End\n"
	"  Body = ActiveBody BodyTag\n"
	"    MaxHealth = 200\n"
	"  End\n"
	"  Behavior = HordeWorkerAIUpdate ModuleTag_HordeWorkerAI\n"
	"  End\n"
	"  Behavior = HordeContain ModuleTag_HordeContain\n"
	"    ObjectStatusOfContained =\n"
	"    InitialPayload = TestPeasantMember 2\n"
	"    Slots = 2\n"
	"    PassengerFilter = NONE +INFANTRY\n"
	"    RankInfo = RankNumber:1 UnitType:TestPeasantMember Position:X:0 Y:-10 Position:X:0 Y:10\n"
	"  End\n"
	"End\n"
	"Object TestRepairableHouse\n"
	"  KindOf = STRUCTURE IMMOBILE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 20.0\n"
	"  GeometryHeight = 30.0\n"
	"  Body = ActiveBody BodyTag\n"
	"    MaxHealth = 200\n"
	"  End\n"
	"  Behavior = HordeGarrisonContain ModuleTag_Garrison\n"
	"    ContainMax = 2\n"
	"    AllowAlliesInside = Yes\n"
	"    AllowNeutralInside = Yes\n"
	"  End\n"
	"End\n";

void damageTo(Object &obj, float targetHealth)
{
	BodyModuleInterface *body = obj.getBodyModule();
	REQUIRE(body != nullptr);
	const float cur = body->getHealth();
	if (cur > targetHealth)
	{
		DamageInfo d;
		d.m_input.m_amount = cur - targetHealth;
		d.m_input.m_damageType = DAMAGE_UNRESISTABLE;
		body->attemptDamage(d);
	}
}

void healToFull(Object &obj)
{
	BodyModuleInterface *body = obj.getBodyModule();
	REQUIRE(body != nullptr);
	const float missing = body->getMaxHealth() - body->getHealth();
	if (missing > 0.0f)
	{
		DamageInfo h;
		h.m_input.m_amount = missing;
		h.m_input.m_damageType = DAMAGE_HEALING;
		body->attemptHealing(h);
	}
}
} // namespace

// Expectations: RW 0x0085D1CD (SiegeDockingBehavior::loadPostProcess calls initializeBones RW 0x85CE65),
// RW 0x0085D08F (SiegeDockingBehavior::xfer preserves entries and m_initialized).
TEST_CASE("LOCAL-101 SiegeDockingBehavior loadPostProcess initializes uninitialized wall and preserves xfer state")
{
	combattest::CombatWorld w(kLocal101Ini);
	Object *wall = w.unit("TestDockWall", 'A', 100.0f, 100.0f);
	REQUIRE(wall != nullptr);

	SiegeDockingBehavior *dock = SiegeDockingBehavior::of(*wall);
	REQUIRE(dock != nullptr);
	CHECK_FALSE(dock->initialized());

	// RW 0x0085D1CD: loadPostProcess initializes bones when not yet initialized.
	dock->loadPostProcess();
	CHECK(dock->initialized());

	// Round-trip xfer (RW 0x0085D08F) and call loadPostProcess again: must stay initialized without duplicating entries.
	const size_t countBefore = dock->entries().size();
	XferSave save;
	dock->xfer(save);
	XferLoad load(save.data().data(), save.data().size());
	dock->xfer(load);
	dock->loadPostProcess();
	CHECK(dock->initialized());
	CHECK(dock->entries().size() == countBefore);
}

// Expectations: RW 0x0089E9F8 (aiDoCommand), RW 0x0089E884 (moveToRepairTarget), RW 0x0089E669 (update),
// RW 0x0089E7C1 (privateRepair), RW 0x0089E57C (cancelRepair), RW 0x0089E623 (clearRepairFlagIfTargetsGone),
// RW 0x0082D7F2 (canRepairObject).
TEST_CASE("LOCAL-101 HordeWorkerAIUpdate repair lifecycle, duplicate 0x13 ignore, cancelRepair, and target death")
{
	combattest::CombatWorld w(kLocal101Ini);
	Object *horde = w.unit("TestPeasantHorde", 'A', 100.0f, 100.0f);
	Object *house = w.unit("TestRepairableHouse", 'A', 140.0f, 100.0f);
	REQUIRE(horde != nullptr);
	REQUIRE(house != nullptr);
	damageTo(*house, 100.0f);

	// Step once so HordeContain spawns its InitialPayload members.
	w.logic->runLogicFrame();

	auto *ai = dynamic_cast<HordeWorkerAIUpdate *>(horde->getAIUpdateInterface());
	REQUIRE(ai != nullptr);
	ContainModuleInterface *c = horde->getContain();
	REQUIRE(c != nullptr);
	HordeContainInterface *hc = c->getHordeContainInterface();
	REQUIRE(hc != nullptr);
	REQUIRE(c->getContainCount() == 2u);

	CHECK(HordeWorkerAIUpdate::canRepairObject(*horde, *house));

	// 1. Command 0x13 (repair): RW 0x0089E9F8 -> RW 0x0089E884 sets pending/active target and +0x3CA.
	ai->commandAccepted(CMD_FROM_PLAYER, 0x13, house);
	CHECK(ai->pendingRepairTarget() == house->getID());
	CHECK(ai->activeRepairTarget() == house->getID());
	CHECK(ai->repairMoveFlag());

	// 2. Duplicate 0x13 for the same active target is ignored (RW 0x89EA13..0x89EA28).
	ai->commandAccepted(CMD_FROM_PLAYER, 0x13, house);
	CHECK(ai->pendingRepairTarget() == house->getID());
	CHECK(ai->activeRepairTarget() == house->getID());

	// 3. When horde becomes idle, update() (RW 0x0089E669) runs privateRepair (RW 0x0089E7C1):
	//    releases DOZER members (slot +0xA8 RW 0x0086EBF4), orders DozerAIUpdate::repair, clears pending target.
	ai->aiIdle(CMD_FROM_AI);
	// Restore pending/active after aiIdle if aiIdle went through commandAccepted:
	// instead, directly test arrival by driving frames until idle.
	ai->commandAccepted(CMD_FROM_PLAYER, 0x13, house);
	for (int i = 0; i < 60 && ai->pendingRepairTarget() != 0; ++i)
	{
		w.logic->runLogicFrame();
	}
	CHECK(ai->pendingRepairTarget() == 0);
	CHECK(ai->activeRepairTarget() == house->getID());
	CHECK_FALSE(hc->membersEntering().empty());

	// 4. Non-repair command cancels repair (RW 0x0089E57C): returns released members to formation and clears targets/+0x3CA.
	ai->commandAccepted(CMD_FROM_PLAYER, 0x05, nullptr);
	CHECK(ai->pendingRepairTarget() == 0);
	CHECK(ai->activeRepairTarget() == 0);
	CHECK_FALSE(ai->repairMoveFlag());
	CHECK(hc->membersEntering().empty());

	// 5. Full health completion in update() (RW 0x89E738..0x89E784): returns members to formation and clears active target.
	ai->privateRepair(house, CMD_FROM_AI);
	CHECK(ai->activeRepairTarget() == house->getID());
	CHECK_FALSE(hc->membersEntering().empty());
	healToFull(*house);
	ai->update();
	CHECK(ai->activeRepairTarget() == 0);
	CHECK(hc->membersEntering().empty());

	// 6. Target destroyed while pending: update() clears pending and RW 0x0089E623 clears repairMoveFlag when all targets gone.
	damageTo(*house, 100.0f);
	ai->commandAccepted(CMD_FROM_PLAYER, 0x13, house);
	CHECK(ai->repairMoveFlag());
	w.logic->destroyObject(house);
	w.logic->runLogicFrame();
	ai->update(); // RW 0x89E6A1..0x89E725: clears pendingRepairTarget and idles
	ai->update(); // RW 0x89E738..0x89E784: clears activeRepairTarget and RW 0x0089E623 clears repairMoveFlag
	CHECK(ai->pendingRepairTarget() == 0);
	CHECK(ai->activeRepairTarget() == 0);
	CHECK_FALSE(ai->repairMoveFlag());
}

// Expectations: RW 0x0089E990 (bfmePrivateCommand42: damaged -> repair then enter; healthy -> enter directly),
// RW 0x0089E669 (update: after active repair completes, m_command42Target enters via RW 0x89E41F -> 0x66D85F),
// RW 0x0089E5CC (xfer v2 preserves m_repairMoveFlag and all three ObjectIDs).
TEST_CASE("LOCAL-101 HordeWorkerAIUpdate command 0x42 enters after repair and xfer round-trip")
{
	combattest::CombatWorld w(kLocal101Ini);
	Object *horde = w.unit("TestPeasantHorde", 'A', 100.0f, 100.0f);
	Object *house = w.unit("TestRepairableHouse", 'A', 140.0f, 100.0f);
	REQUIRE(horde != nullptr);
	REQUIRE(house != nullptr);
	w.logic->runLogicFrame();

	auto *ai = dynamic_cast<HordeWorkerAIUpdate *>(horde->getAIUpdateInterface());
	REQUIRE(ai != nullptr);

	// 1. Healthy house: command 0x42 skips repair and enters directly (RW 0x89E9A7..0x89E9EC).
	ai->commandAccepted(CMD_FROM_PLAYER, 0x42, house);
	CHECK(ai->command42Target() == 0);
	CHECK(ai->activeRepairTarget() == 0);
	CHECK(ai->currentStateId() == AI_MOVE_TO_POSITION_AND_ENTER);

	// 2. Damaged house: command 0x42 remembers command42Target and starts repair approach (RW 0x89E9C7..0x89E9D7).
	damageTo(*house, 100.0f);
	ai->commandAccepted(CMD_FROM_PLAYER, 0x42, house);
	CHECK(ai->command42Target() == house->getID());
	CHECK(ai->pendingRepairTarget() == house->getID());
	CHECK(ai->activeRepairTarget() == house->getID());
	CHECK(ai->repairMoveFlag());

	// 3. Xfer v2 + CRC round-trip (RW 0x0089E5CC).
	StateHasher hBefore;
	ai->crc(hBefore);
	XferSave save;
	ai->xfer(save);
	ai->cancelRepair();
	CHECK_FALSE(ai->repairMoveFlag());
	XferLoad load(save.data().data(), save.data().size());
	ai->xfer(load);
	CHECK(ai->command42Target() == house->getID());
	CHECK(ai->pendingRepairTarget() == house->getID());
	CHECK(ai->activeRepairTarget() == house->getID());
	CHECK(ai->repairMoveFlag());
	StateHasher hAfter;
	ai->crc(hAfter);
	CHECK(hBefore.value() == hAfter.value());

	// 4. Finish approach & repair, then heal house to full: update() releases formation, next update() enters house!
	for (int i = 0; i < 60 && ai->pendingRepairTarget() != 0; ++i)
	{
		w.logic->runLogicFrame();
	}
	REQUIRE(ai->pendingRepairTarget() == 0);
	REQUIRE(ai->activeRepairTarget() == house->getID());
	healToFull(*house);
	ai->update(); // clears activeRepairTarget and calls returnToFormation(true)
	CHECK(ai->activeRepairTarget() == 0);
	CHECK(ai->command42Target() == house->getID());
	ai->update(); // RW 0x89E786..0x89E7B1: enters house and clears command42Target
	CHECK(ai->command42Target() == 0);
	CHECK(ai->currentStateId() == AI_MOVE_TO_POSITION_AND_ENTER);
}

// Expectations: Ennorath 1.9.8.3 data/ini/object/neutral/neutralunits.ini (RohanPeasantHorde has DOZER + HordeWorkerAIUpdate),
// RW 0x0089E9F8 / 0x0089E884 / 0x0089E669 / 0x0089E7C1 / 0x0089E990.
TEST_CASE("LOCAL-101 ENNORATH scenario: RohanPeasantHorde repairs damaged GondorFarm and returns to formation")
{
	ennorathscenario::World world;
	if (!world.load("LOCAL-101 ENNORATH scenario"))
	{
		return;
	}
	hudtest::Rig game(world.shared);
	const int player1 = game.index();

	std::string error;
	Object *horde = game.game->createObject("RohanPeasantHorde", player1, Coord3D{ 1200.0f, 1200.0f, 0.0f }, 0.0f, &error);
	REQUIRE_MESSAGE(horde != nullptr, error);
	Object *farm = game.game->createObject("GondorFarm", player1, Coord3D{ 1260.0f, 1200.0f, 0.0f }, 0.0f, &error);
	REQUIRE_MESSAGE(farm != nullptr, error);

	// Step so HordeContain spawns its peasant members.
	game.logic().runLogicFrame();
	game.logic().runLogicFrame();

	auto *ai = dynamic_cast<HordeWorkerAIUpdate *>(horde->getAIUpdateInterface());
	REQUIRE(ai != nullptr);

	// Healthy farm cannot be repaired (RW 0x0082D7F2).
	CHECK_FALSE(HordeWorkerAIUpdate::canRepairObject(*horde, *farm));

	// Damaged farm can be repaired by RohanPeasantHorde (DOZER).
	damageTo(*farm, farm->getBodyModule()->getMaxHealth() * 0.5f);
	CHECK(HordeWorkerAIUpdate::canRepairObject(*horde, *farm));

	ai->commandAccepted(CMD_FROM_PLAYER, 0x13, farm);
	CHECK(ai->pendingRepairTarget() == farm->getID());
	CHECK(ai->activeRepairTarget() == farm->getID());
	CHECK(ai->repairMoveFlag());

	// Drive frames until the horde arrives and dispatches privateRepair (RW 0x0089E7C1).
	for (int i = 0; i < 90 && ai->pendingRepairTarget() != 0; ++i)
	{
		game.logic().runLogicFrame();
	}
	CHECK(ai->pendingRepairTarget() == 0);
	CHECK(ai->activeRepairTarget() == farm->getID());

	// Once repaired to full health, update() returns members to formation (RW 0x0089E669).
	healToFull(*farm);
	ai->update();
	CHECK(ai->activeRepairTarget() == 0);
}
