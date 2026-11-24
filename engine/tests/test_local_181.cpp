// OpenBFME. GPL-3.0.
// LOCAL-181 (fork issue 962): synthetic INI; no retail or mod assets.
// Expectations from RotWK 2.01 game.dat:
//   - RW 0x8840D4 (SlaughterHordeContain::update), RW 0x88408A (contained horde walk, flags 1 and 0x10),
//     RW 0x884056 (member callback: if AI state != 0x38 AI_MOVE_TO_POSITION_AND_ENTER and != 0x0F AI_ENTER,
//     issues aiMoveToObject(building, CMD_FROM_AI) via RW 0x770EDE), RW 0x884244 (CitadelSlaughterHordeContain::update -> 0x8840D4).
//   - RW 0x883A83 (SlaughterHordeContain / CitadelSlaughterHordeContain shared DieModuleInterface 0xC5F2E0[0] onDie:
//     for each contained KINDOF_HORDE, walks horde members via RW 0x8704DA and calls addToContain(member) RW 0x8836D3).

#include "doctest.h"
#include "CombatTestUtil.h"
#include "GameLogic/AI/AIGarrisonStates.h"
#include "GameLogic/AI/AIStateMachine.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object/Contain/GarrisonContainRuntime.h"
#include "GameLogic/Object/Contain/HordeContainRuntime.h"

using namespace combattest;

namespace
{
const char kLocal181Objects[] =
	"Object L181Soldier\n"
	" KindOf = INFANTRY SELECTABLE\n"
	" BuildCost = 100\n"
	" BountyValue = 100\n"
	" TransportSlotCount = 1\n"
	" Geometry = CYLINDER\n GeometryMajorRadius = 5\n GeometryHeight = 10\n"
	" Body = ActiveBody Body\n MaxHealth = 100\n End\n"
	" Behavior = AIUpdateInterface AI\n End\n"
	" LocomotorSet\n Locomotor = WalkerLoco\n Condition = SET_NORMAL\n Speed = 55\n End\n"
	"End\n"
	"Object L181Horde\n"
	" KindOf = HORDE INFANTRY SELECTABLE\n"
	" BuildCost = 200\n"
	" TransportSlotCount = 1\n"
	" Body = ActiveBody Body\n MaxHealth = 1000\n End\n"
	" Behavior = HordeAIUpdate AI\n End\n"
	" Behavior = HordeContain Horde\n"
	"  RankInfo = RankNumber:1 UnitType:L181Soldier Position:X:0 Y:0 Position:X:0 Y:15\n"
	"  InitialPayload = L181Soldier 2\n"
	" End\n"
	" LocomotorSet\n Locomotor = HordeLoco\n Condition = SET_NORMAL\n Speed = 50\n End\n"
	"End\n"
	"Object L181Slaughter\n"
	" KindOf = STRUCTURE IMMOBILE SELECTABLE\n"
	" Geometry = CYLINDER\n GeometryMajorRadius = 20\n GeometryHeight = 30\n"
	" Body = ActiveBody Body\n MaxHealth = 1000\n End\n"
	" Behavior = SlaughterHordeContain Contain\n"
	"  PassengerFilter = ANY +INFANTRY\n"
	"  ContainMax = 20\n"
	"  AllowOwnPlayerInsideOverride = Yes\n"
	"  EntryOffset = X:-30 Y:0 Z:0\n"
	"  EntryPosition = X:-20 Y:0 Z:0\n"
	"  CashBackPercent = 50%\n"
	" End\n"
	"End\n"
	"Object L181Citadel\n"
	" KindOf = STRUCTURE IMMOBILE SELECTABLE COMMANDCENTER\n"
	" Geometry = CYLINDER\n GeometryMajorRadius = 20\n GeometryHeight = 30\n"
	" Body = ActiveBody Body\n MaxHealth = 1000\n End\n"
	" Behavior = CitadelSlaughterHordeContain Contain\n"
	"  PassengerFilter = ANY +INFANTRY\n"
	"  ContainMax = 20\n"
	"  AllowOwnPlayerInsideOverride = Yes\n"
	"  EntryOffset = X:-30 Y:0 Z:0\n"
	"  EntryPosition = X:-20 Y:0 Z:0\n"
	"  CashBackPercent = 50%\n"
	" End\n"
	"End\n";
}

TEST_CASE("LOCAL-181 SlaughterHordeContain::update re-orders stray horde members (RW 0x8840D4 / 0x88408A / 0x884056)")
{
	// Expectations from RW 0x8840D4 -> 0x88408A -> 0x884056 (DONOR BFME2 SlaughterHordeContainUpdate.cpp 0x480E72).
	CombatWorld world(kLocal181Objects);
	Object *building = world.unit("L181Slaughter", 'A', 400, 400);
	Object *horde = world.unit("L181Horde", 'A', 200, 400);
	REQUIRE(building != nullptr);
	REQUIRE(horde != nullptr);

	auto *slaughter = dynamic_cast<SlaughterHordeContain *>(building->getContain());
	auto *hc = dynamic_cast<HordeContain *>(horde->getContain());
	REQUIRE(slaughter != nullptr);
	REQUIRE(hc != nullptr);

	std::vector<Object *> members = hc->getMembersWithEntering();
	REQUIRE(members.size() == 2);
	Object *stray = members[0];
	Object *entering = members[1];

	// Admit horde parent into SlaughterHordeContain and release both members as approaching (m_garrisonEntering).
	REQUIRE(slaughter->addToContain(horde));
	hc->releaseMemberForGarrison(stray);
	hc->releaseMemberForGarrison(entering);

	AIUpdateInterface *strayAi = stray->getAIUpdateInterface();
	AIUpdateInterface *enteringAi = entering->getAIUpdateInterface();
	REQUIRE(strayAi != nullptr);
	REQUIRE(enteringAi != nullptr);

	// Put stray member in AI_IDLE (state != 0x38 and != 0x0F) and entering member in AI_ENTER (0x0F).
	strayAi->aiIdle(CMD_FROM_AI);
	enteringAi->aiEnter(building, CMD_FROM_AI);
	REQUIRE(strayAi->currentStateId() == (unsigned)AI_IDLE);
	REQUIRE(enteringAi->currentStateId() == (unsigned)AI_MOVE_TO_POSITION_AND_ENTER);

	// RW 0x8840D4: update re-orders stray via aiMoveToObject(building, CMD_FROM_AI) and leaves AI_ENTER untouched.
	slaughter->update();

	CHECK(strayAi->currentStateId() != (unsigned)AI_IDLE);
	CHECK(strayAi->stateMachineOrNull()->goalObject() == building);
	CHECK(enteringAi->currentStateId() == (unsigned)AI_MOVE_TO_POSITION_AND_ENTER);
}

TEST_CASE("LOCAL-181 CitadelSlaughterHordeContain::update chains to SlaughterHordeContain::update (RW 0x884244 -> 0x8840D4)")
{
	// Expectations from RW 0x884244 (CitadelSlaughterHordeContain::update) tail-calling RW 0x8840D4.
	CombatWorld world(kLocal181Objects);
	Object *citadel = world.unit("L181Citadel", 'A', 400, 400);
	Object *horde = world.unit("L181Horde", 'A', 200, 400);
	REQUIRE(citadel != nullptr);
	REQUIRE(horde != nullptr);

	auto *contain = dynamic_cast<CitadelSlaughterHordeContain *>(citadel->getContain());
	auto *hc = dynamic_cast<HordeContain *>(horde->getContain());
	REQUIRE(contain != nullptr);
	REQUIRE(hc != nullptr);

	std::vector<Object *> members = hc->getMembersWithEntering();
	REQUIRE(members.size() == 2);
	Object *stray = members[0];

	REQUIRE(contain->addToContain(horde));
	hc->releaseMemberForGarrison(stray);

	AIUpdateInterface *strayAi = stray->getAIUpdateInterface();
	REQUIRE(strayAi != nullptr);
	strayAi->aiIdle(CMD_FROM_AI);
	REQUIRE(strayAi->currentStateId() == (unsigned)AI_IDLE);

	contain->update();

	CHECK(strayAi->currentStateId() != (unsigned)AI_IDLE);
	CHECK(strayAi->stateMachineOrNull()->goalObject() == citadel);
}

TEST_CASE("LOCAL-181 SlaughterHordeContain::onDie slaughters entering horde members (RW 0x883A83)")
{
	// Expectations from RW 0x883A83 (shared DieModuleInterface 0xC5F2E0[0] of SlaughterHordeContain and
	// CitadelSlaughterHordeContain): for each contained KINDOF_HORDE, feeds members (RW 0x8704DA) to addToContain (RW 0x8836D3).
	CombatWorld world(kLocal181Objects);
	Object *citadel = world.unit("L181Citadel", 'A', 400, 400);
	Object *horde = world.unit("L181Horde", 'A', 200, 400);
	REQUIRE(citadel != nullptr);
	REQUIRE(horde != nullptr);

	auto *contain = dynamic_cast<CitadelSlaughterHordeContain *>(citadel->getContain());
	auto *hc = dynamic_cast<HordeContain *>(horde->getContain());
	REQUIRE(contain != nullptr);
	REQUIRE(hc != nullptr);

	std::vector<Object *> members = hc->getMembersWithEntering();
	REQUIRE(members.size() == 2);
	const ObjectID m0 = members[0]->getID();
	const ObjectID m1 = members[1]->getID();

	REQUIRE(contain->addToContain(horde));
	hc->releaseMemberForGarrison(members[0]);
	hc->releaseMemberForGarrison(members[1]);

	Player *player = world.playerOf('A');
	const unsigned moneyBefore = player->getMoney()->countMoney();

	// Killing the citadel runs RW 0x883A83, which slaughters the entering members and deposits cashback (50% of 100 each = 100).
	citadel->kill(DAMAGE_UNRESISTABLE, DEATH_NORMAL);

	// 50% of BountyValue=100 for each of the 2 entering members = +100, and the parent horde is destroyed once all members enter.
	CHECK(player->getMoney()->countMoney() == moneyBefore + 100u);
	CHECK(horde->isDestroyed());
	(void)m0;
	(void)m1;
}

TEST_CASE("LOCAL-181 SlaughterHordeContain::onDie with no contained horde is a no-op (RW 0x883A83)")
{
	// Neighboring behavior from RW 0x883A83: empty contained list returns cleanly with no payout.
	CombatWorld world(kLocal181Objects);
	Object *slaughter = world.unit("L181Slaughter", 'A', 400, 400);
	REQUIRE(slaughter != nullptr);

	Player *player = world.playerOf('A');
	const unsigned moneyBefore = player->getMoney()->countMoney();
	slaughter->kill(DAMAGE_UNRESISTABLE, DEATH_NORMAL);
	CHECK(player->getMoney()->countMoney() == moneyBefore);
}
