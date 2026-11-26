// OpenBFME unit tests. GPL-3.0.
// PORT-283 (issue #650): HordeAIUpdate (RW 0x89DE6C..0x89E4A8) + HorseHordeContain (RW 0x879849..0x87A27E).
// Synthetic INI only; no retail game data required. Every assertion derives from RotWK 2.01 disassembly.

#include "doctest.h"
#include "MoveTestUtil.h"

#include "Common/RandomValue.h"
#include "Common/StateHash.h"
#include "Common/XferLoad.h"
#include "Common/XferSave.h"
#include "GameLogic/AI/AIMove.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/DockUpdate.h"
#include "GameLogic/Module/HordeAIUpdate.h"
#include "GameLogic/Object/Contain/HordeContainBehaviorData.h"
#include "GameLogic/Object/Contain/HordeContainCore.h"
#include "GameLogic/Object/Contain/HordeContainRuntime.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/ObjectTemplateInfo.h"

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

using namespace movetest;

struct HorseHordeContainTestAccess
{
	static void memberOrder(HorseHordeContain &contain, Object &member, const Coord3D &slotPos, float orientation, bool force)
	{
		contain.memberOrder(member, slotPos, orientation, force);
	}
};

namespace
{
const char kPort283Ini[] =
	"Locomotor P283CavalryHordeLoco\n"
	"  Surfaces = GROUND\n"
	"  TurnTime = 1000\n"
	"  TurnTimeDamaged = 1000\n"
	"  Acceleration = 500\n"
	"  Braking = 500\n"
	"  ZAxisBehavior = SURFACE_RELATIVE_HEIGHT\n"
	"  Appearance = HORDE\n"
	"  MaxTurnWithoutReform = 60\n"
	"  TurnWhileMoving = Yes\n"
	"  WaitForFormation = Yes\n"
	"  CloseEnoughDist = 2\n"
	"End\n"
	"Locomotor P283RiderLoco\n"
	"  Surfaces = GROUND\n"
	"  TurnTime = 1000\n"
	"  TurnTimeDamaged = 1000\n"
	"  Acceleration = 1000\n"
	"  Braking = 1000\n"
	"  MinSpeed = 0\n"
	"  ZAxisBehavior = SURFACE_RELATIVE_HEIGHT\n"
	"  Appearance = FOUR_WHEELS\n"
	"  CloseEnoughDist = 1\n"
	"End\n"
	"Object P283Rider\n"
	"  KindOf = CAVALRY SELECTABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 6\n"
	"  GeometryMinorRadius = 6\n"
	"  GeometryHeight = 16\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 400\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = PhysicsBehavior ModuleTag_Phys\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = P283RiderLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 100\n"
	"  End\n"
	"End\n"
	"Object P283StaticRider\n"
	"  KindOf = CAVALRY SELECTABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 6\n"
	"  GeometryMinorRadius = 6\n"
	"  GeometryHeight = 16\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 400\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"End\n"
	"Object P283CavalryHorde\n"
	"  KindOf = HORDE ARCHER LARGE_RECTANGLE_PATHFIND SELECTABLE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 25\n"
	"  GeometryMinorRadius = 25\n"
	"  GeometryHeight = 16\n"
	"  Body = ImmortalBody ModuleTag_Body\n"
	"    MaxHealth = 1\n"
	"  End\n"
	"  Behavior = HordeAIUpdate ModuleTag_AI\n"
	"  End\n"
	"  Behavior = HorseHordeContain ModuleTag_Horde\n"
	"    Slots = 4\n"
	"    InitialPayload = P283Rider 4\n"
	"    UseSlowHordeMovement = No\n"
	"    RanksToReleaseWhenAttacking = 1\n"
	"    RankInfo = RankNumber:1 UnitType:P283Rider Position:X:10 Y:-15 Position:X:10 Y:15 Position:X:-10 Y:-15 Position:X:-10 Y:15\n"
	"    AlternateFormation = P283CavalryHordeWedge\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = P283CavalryHordeLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 90\n"
	"  End\n"
	"End\n"
	"Object P283CavalryHordeWedge\n"
	"  KindOf = HORDE ARCHER LARGE_RECTANGLE_PATHFIND SELECTABLE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 25\n"
	"  GeometryMinorRadius = 25\n"
	"  GeometryHeight = 16\n"
	"  Body = ImmortalBody ModuleTag_Body\n"
	"    MaxHealth = 1\n"
	"  End\n"
	"  Behavior = HordeAIUpdate ModuleTag_AI\n"
	"  End\n"
	"  Behavior = HorseHordeContain ModuleTag_Horde\n"
	"    Slots = 4\n"
	"    ThisFormationIsTheMainFormation = No\n"
	"    UseSlowHordeMovement = No\n"
	"    RankInfo = RankNumber:1 UnitType:P283Rider Position:X:15 Y:0 Position:X:0 Y:-15 Position:X:0 Y:15 Position:X:-15 Y:0\n"
	"    AlternateFormation = P283CavalryHorde\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = P283CavalryHordeLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 90\n"
	"  End\n"
	"End\n"
	"Object P283MixedCavalryHorde\n"
	"  KindOf = HORDE MELEE_HORDE SELECTABLE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryMinorRadius = 20\n"
	"  GeometryHeight = 16\n"
	"  Body = ImmortalBody ModuleTag_Body\n"
	"    MaxHealth = 1\n"
	"  End\n"
	"  Behavior = HordeAIUpdate ModuleTag_AI\n"
	"  End\n"
	"  Behavior = HorseHordeContain ModuleTag_Horde\n"
	"    Slots = 2\n"
	"    InitialPayload = P283StaticRider 1\n"
	"    InitialPayload = P283Rider 1\n"
	"    RankInfo = RankNumber:1 UnitType:P283StaticRider Position:X:10 Y:0\n"
	"    RankInfo = RankNumber:2 UnitType:P283Rider Position:X:-10 Y:0\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = P283CavalryHordeLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 90\n"
	"  End\n"
	"End\n"
	"Object P283SpareSlotHorde\n"
	"  KindOf = HORDE MELEE_HORDE SELECTABLE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryMinorRadius = 20\n"
	"  GeometryHeight = 16\n"
	"  Body = ImmortalBody ModuleTag_Body\n"
	"    MaxHealth = 1\n"
	"  End\n"
	"  Behavior = HordeAIUpdate ModuleTag_AI\n"
	"  End\n"
	"  Behavior = HorseHordeContain ModuleTag_Horde\n"
	"    Slots = 2\n"
	"    InitialPayload = P283Rider 1\n"
	"    RankInfo = RankNumber:1 UnitType:P283Rider Position:X:12 Y:-10 Position:X:12 Y:10\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = P283CavalryHordeLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 90\n"
	"  End\n"
	"End\n"
	"Object P283InfantryHorde\n"
	"  KindOf = HORDE MELEE_HORDE SELECTABLE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryMinorRadius = 20\n"
	"  GeometryHeight = 16\n"
	"  Body = ImmortalBody ModuleTag_Body\n"
	"    MaxHealth = 1\n"
	"  End\n"
	"  Behavior = HordeAIUpdate ModuleTag_AI\n"
	"  End\n"
	"  Behavior = HordeContain ModuleTag_Horde\n"
	"    Slots = 2\n"
	"    InitialPayload = P283Rider 2\n"
	"    UseSlowHordeMovement = No\n"
	"    RankInfo = RankNumber:1 UnitType:P283Rider Position:X:10 Y:-10 Position:X:10 Y:10\n"
	"    AlternateFormation = P283PikePorcupine\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = P283CavalryHordeLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 60\n"
	"  End\n"
	"End\n"
	"Object P283PikePorcupine\n"
	"  KindOf = HORDE MELEE_HORDE SELECTABLE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryMinorRadius = 20\n"
	"  GeometryHeight = 16\n"
	"  Body = ImmortalBody ModuleTag_Body\n"
	"    MaxHealth = 1\n"
	"  End\n"
	"  Behavior = HordeAIUpdate ModuleTag_AI\n"
	"  End\n"
	"  Behavior = HordeContain ModuleTag_Horde\n"
	"    Slots = 2\n"
	"    ThisFormationIsTheMainFormation = No\n"
	"    IsPorcupineFormation = Yes\n"
	"    RankInfo = RankNumber:1 UnitType:P283Rider Position:X:10 Y:0 Position:X:-10 Y:0\n"
	"    AlternateFormation = P283InfantryHorde\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = P283CavalryHordeLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 60\n"
	"  End\n"
	"End\n"
	"Object P283ContainlessHordeAI\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 16\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 200\n"
	"  End\n"
	"  Behavior = HordeAIUpdate ModuleTag_AI\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = P283RiderLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 80\n"
	"  End\n"
	"End\n"
	"Object P283Enemy\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 16\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 2000\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"End\n";

struct Port283World : MoveWorld
{
	explicit Port283World(std::uint32_t seed = 0x283C0DE1u)
		: MoveWorld(120, 120, kPort283Ini)
	{
		logic->random().seedRandom(seed);
	}

	HorseHordeContain *horseOf(Object *horde) const
	{
		return horde ? dynamic_cast<HorseHordeContain *>(horde->getContain()) : nullptr;
	}

	HordeContain *containOf(Object *horde) const
	{
		return horde ? dynamic_cast<HordeContain *>(horde->getContain()) : nullptr;
	}

	HordeAIUpdate *haiOf(Object *horde) const
	{
		return horde ? dynamic_cast<HordeAIUpdate *>(horde->getAIUpdateInterface()) : nullptr;
	}

	std::vector<Object *> membersOf(Object *horde) const
	{
		std::vector<Object *> out;
		const ContainModuleInterface *c = horde ? horde->getContain() : nullptr;
		const auto *list = c ? c->getContainedItemsList() : nullptr;
		if (list)
		{
			out.assign(list->begin(), list->end());
		}
		return out;
	}
};

std::uint32_t crcOf(const Module &m)
{
	StateHasher h;
	m.crc(h);
	return h.value();
}
} // namespace

TEST_CASE("PORT-283: HorseHordeContain distinct runtime, isPassengerAllowedToFire, beginReform, isFormationReady and formation swap")
{
	Port283World w;
	Object *cav = w.spawn("P283CavalryHorde", 400.0f, 400.0f, 0.0f);
	Object *inf = w.spawn("P283InfantryHorde", 500.0f, 400.0f, 0.0f);
	HorseHordeContain *horse = w.horseOf(cav);
	HordeContain *plain = w.containOf(inf);
	REQUIRE(horse != nullptr);
	REQUIRE(plain != nullptr);
	CHECK(w.horseOf(inf) == nullptr);

	// RW 0x9188EB (+0x20 slot 0xB4): HorseHordeContain never lets passengers fire; the infantry contain retains the shared default.
	CHECK_FALSE(plain->isPassengerAllowedToFire());
	CHECK_FALSE(horse->isPassengerAllowedToFire());

	// RW 0x8799AD (+0x11C slot 0x3C): beginReform zeroes positive horde locomotor speed (HordeContain's base is a no-op).
	Locomotor *cavLoco = cav->getAIUpdateInterface()->curLocomotor();
	Locomotor *infLoco = inf->getAIUpdateInterface()->curLocomotor();
	REQUIRE(cavLoco != nullptr);
	REQUIRE(infLoco != nullptr);
	cavLoco->setSpeed(12.0f);
	infLoco->setSpeed(12.0f);
	horse->beginReform();
	plain->beginReform();
	CHECK(cavLoco->speed() == 0.0f);
	CHECK(infLoco->speed() == 12.0f);
	cavLoco->setSpeed(-3.0f);
	horse->beginReform();
	CHECK(cavLoco->speed() == -3.0f);
	cavLoco->setSpeed(0.0f);

	// RW 0x8799CF (+0x11C slot 0x1C0): isFormationReady checks every rider against the first locomotor-bearing rider's maxTurnRate.
	std::vector<Object *> riders = w.membersOf(cav);
	REQUIRE(riders.size() == 4);
	AIUpdateInterface *r0ai = riders[0]->getAIUpdateInterface();
	const float rate = r0ai->curLocomotor()->getMaxTurnRate(r0ai->locomotorHost());
	REQUIRE(rate > 0.0f); // tolerance: TurnTime = 1000ms gives a positive per-frame turn rate
	CHECK(horse->isFormationReady(0.0f));
	riders[2]->setOrientation(rate * 0.5f);
	CHECK(horse->isFormationReady(0.0f));
	riders[2]->setOrientation(rate * 1.5f);
	CHECK_FALSE(horse->isFormationReady(0.0f));
	CHECK(plain->isFormationReady(3.14f)); // plain HordeContain is always ready (RW 0x9B501B)
	riders[2]->setOrientation(0.0f);

	// Mixed horde whose first member lacks a locomotor still caches maxTurnRate from the second member (RW 0x8799FD: while cache == 0).
	Object *mixed = w.spawn("P283MixedCavalryHorde", 600.0f, 400.0f, 0.0f);
	HorseHordeContain *mixedHorse = w.horseOf(mixed);
	REQUIRE(mixedHorse != nullptr);
	std::vector<Object *> mixedMembers = w.membersOf(mixed);
	REQUIRE(mixedMembers.size() == 2);
	mixedMembers[0]->setOrientation(0.0f);
	mixedMembers[1]->setOrientation(rate * 0.5f);
	CHECK(mixedHorse->isFormationReady(0.0f));
	mixedMembers[1]->setOrientation(rate * 1.5f);
	CHECK_FALSE(mixedHorse->isFormationReady(0.0f));

	// Alternate formation toggle preserves HorseHordeContain dynamic type.
	HordeContain *swapped = horse->toggleFormation();
	REQUIRE(swapped != nullptr);
	CHECK(dynamic_cast<HorseHordeContain *>(swapped) != nullptr);
}

TEST_CASE("PORT-283: HorseHordeContain memberOrder wheels before walking, waits when ahead without UseSlowHordeMovement, and clears reform latch")
{
	Port283World w;
	w.buildMap();
	Object *cav = w.spawn("P283CavalryHorde", 400.0f, 400.0f, 0.0f);
	HorseHordeContain *horse = w.horseOf(cav);
	REQUIRE(horse != nullptr);
	std::vector<Object *> riders = w.membersOf(cav);
	REQUIRE(riders.size() == 4);

	// 1. Slow horde wheels a misaligned displaced rider in place before walking (RW 0x87A0EE .. 0x87A1AF).
	Coord3D behind = *riders[0]->getPosition();
	behind.x -= 40.0f;
	riders[0]->setPosition(&behind);
	riders[0]->setOrientation(2.4f);
	const unsigned long long walksBefore = horse->passStats().walks;
	const unsigned long long turnsBefore = horse->passStats().turns;
	horse->runMemberPass();
	CHECK(horse->passStats().turns == turnsBefore + 1);
	CHECK(horse->passStats().walks == walksBefore);

	// Above quarter max speed (RW 0x5E4CA7), the rider walks immediately instead of stopping to wheel.
	Locomotor *hordeLoco = cav->getAIUpdateInterface()->curLocomotor();
	const float maxSpeed = hordeLoco->getMaxSpeedForCondition(cav->getAIUpdateInterface()->locomotorHost());
	hordeLoco->setSpeed(maxSpeed * 0.5f);
	horse->runMemberPass();
	CHECK(horse->passStats().walks == walksBefore + 1);
	hordeLoco->setSpeed(0.0f);

	// 2. Ahead-of-slot rider stops and waits even with UseSlowHordeMovement = No (RW 0x87A1B1 .. 0x87A22B), whereas HordeContain walks it back.
	w.frames(30); // let riders[0] rejoin slot and face 0
	Coord3D slot0 = horse->getMemberFormationPosition(riders[0]);
	Coord3D ahead = slot0;
	ahead.x += 50.0f; // ahead along horde facing +X (cos 0 = 1)
	riders[0]->setPosition(&ahead);
	riders[0]->setOrientation(0.0f);
	const unsigned long long waitsBefore = horse->passStats().waits;
	const unsigned long long walksAheadBefore = horse->passStats().walks;
	HorseHordeContainTestAccess::memberOrder(*horse, *riders[0], slot0, 0.0f, false);
	CHECK(horse->passStats().waits == waitsBefore + 1);
	CHECK(horse->passStats().walks == walksAheadBefore);

	// 3. endReform sets [this + 0x121], bypassing the ahead-of-slot wait so the rider walks back, and clears once the pass settles (RW 0x87312D).
	horse->endReform();
	CHECK(horse->isHorseReforming());
	HorseHordeContainTestAccess::memberOrder(*horse, *riders[0], slot0, 0.0f, false);
	CHECK(horse->passStats().walks > walksAheadBefore); // tolerance: the exact walk count depends on deterministic locomotor step timing; require progress only.
	w.frames(40);
	CHECK_FALSE(horse->isHorseReforming());
}

TEST_CASE("PORT-283: HorseHordeContain horseUpdatePosition clears ACCELERATE, gates DECELERATE, and settles parked riders")
{
	Port283World w;
	w.buildMap();
	Object *cav = w.spawn("P283CavalryHorde", 400.0f, 400.0f, 0.0f);
	HorseHordeContain *horse = w.horseOf(cav);
	REQUIRE(horse != nullptr);
	std::vector<Object *> riders = w.membersOf(cav);
	REQUIRE(riders.size() == 4);

	const int accelBit = AIUpdateInterface::modelConditionBit("ACCELERATE");
	const int decelBit = AIUpdateInterface::modelConditionBit("DECELERATE");
	const int movingBit = AIUpdateInterface::modelConditionBit("MOVING");
	REQUIRE(accelBit >= 0); // tolerance: status IDs are resolved by name and only need to be present in this template registry.
	REQUIRE(decelBit >= 0); // tolerance: status IDs are resolved by name and only need to be present in this template registry.
	REQUIRE(movingBit >= 0); // tolerance: status IDs are resolved by name and only need to be present in this template registry.

	// P-75 / P-156 mechanism: a rider carrying stale ACCELERATE (135) gets it cleared on the far-arm walk (RW 0x879E95 .. 0x879EAB).
	Object *r = riders[0];
	Locomotor *rLoco = r->getAIUpdateInterface()->curLocomotor();
	REQUIRE(rLoco != nullptr);
	const float maxSpeed = rLoco->getMaxSpeedForCondition(r->getAIUpdateInterface()->locomotorHost());
	Coord3D slot = horse->getMemberFormationPosition(r);

	Coord3D farPos = slot;
	farPos.x -= 40.0f;
	r->setPosition(&farPos);
	r->setOrientation(0.0f);
	r->setModelConditionState(accelBit, true);
	rLoco->setSpeed(maxSpeed * 0.4f);
	horse->runMemberPass();
	CHECK_FALSE(r->testModelCondition(accelBit));
	CHECK(r->testModelCondition(movingBit));
	CHECK_FALSE(r->testModelCondition(decelBit)); // curSpeed < dist -> not braking yet

	// Close enough that curSpeed > dist while curSpeed < 0.66 * maxSpeed and horde is parked -> sets DECELERATE (136) (RW 0x879F24 .. 0x879F78).
	const float brakeSpeed = maxSpeed * 0.5f;
	const float brakeDist = maxSpeed * 0.35f; // above snap threshold 0.2 * maxSpeed, below brakeSpeed
	Coord3D brakePos = slot;
	brakePos.x -= brakeDist;
	r->setPosition(&brakePos);
	rLoco->setSpeed(brakeSpeed);
	horse->runMemberPass();
	CHECK(r->testModelCondition(decelBit));
	CHECK(r->getAIUpdateInterface()->mover().desiredSpeed() == doctest::Approx(brakeDist)); // tolerance: 2D float length of single-axis delta

	// Snap onto parked slot -> clears both MOVING and DECELERATE (RW 0x879D80 .. 0x879DF2).
	r->setPosition(&slot);
	rLoco->setSpeed(0.0f);
	horse->runMemberPass();
	CHECK_FALSE(r->testModelCondition(movingBit));
	CHECK_FALSE(r->testModelCondition(decelBit));


}

TEST_CASE("PORT-283: HordeAIUpdate DockUpdateInterface reserves slots, returns formation positions, and rejoins docking members")
{
	Port283World w;
	Object *horde = w.spawn("P283SpareSlotHorde", 400.0f, 400.0f, 0.0f);
	Object *recruit = w.spawn("P283Rider", 350.0f, 400.0f, 0.0f);
	Object *stranger = w.spawn("P283StaticRider", 340.0f, 400.0f, 0.0f);
	HordeAIUpdate *hai = w.haiOf(horde);
	HorseHordeContain *hc = w.horseOf(horde);
	REQUIRE(hai != nullptr);
	REQUIRE(hc != nullptr);

	DockUpdateInterface *dock = hai->getDockUpdateInterface();
	REQUIRE(dock != nullptr);
	CHECK(horde->getDockUpdateInterface() == dock);

	// Static slot answers on RW 0xC66CE8.
	Coord3D dummy{ 0.0f, 0.0f, 0.0f };
	int idx = -1;
	CHECK(dock->advanceApproachPosition(*recruit, &dummy, &idx));
	CHECK(dock->isClearToEnter(*recruit));
	CHECK_FALSE(dock->isClearToAdvance(*recruit, 0));
	CHECK(dock->isDockOpen());
	CHECK(dock->isAllowPassthroughType());
	CHECK_FALSE(dock->isRallyPointAfterDockType());
	CHECK_FALSE(dock->unknownSlot40());

	// Unregistered recruit is not clear to approach until reserveApproachPosition claims the spare slot (RW 0x89E03B / 0x89E05C -> 0x873F30).
	CHECK_FALSE(dock->isClearToApproach(*recruit));
	Coord3D approach{ 0.0f, 0.0f, 0.0f };
	idx = 77;
	CHECK(dock->reserveApproachPosition(*recruit, &approach, &idx));
	CHECK(idx == 0);
	CHECK(dock->isClearToApproach(*recruit));
	CHECK(hc->isMemberOrEntering(*recruit));
	CHECK(hc->getContainCount() == 1); // reserved, not yet contained

	// Incompatible stranger gets no slot because no free rank accepts P283StaticRider.
	CHECK_FALSE(dock->isClearToApproach(*stranger));
	Coord3D strangerPos{ 0.0f, 0.0f, 0.0f };
	CHECK(dock->reserveApproachPosition(*stranger, &strangerPos, nullptr));
	CHECK_FALSE(dock->isClearToApproach(*stranger));

	// getEnterPosition / getDockPosition / getExitPosition all match the reserved formation slot (RW 0x89E0B5 / RW 0x9F9E80).
	const Coord3D expectedSlot = hc->getMemberFormationPosition(recruit);
	Coord3D enterPos{}, dockPos{}, exitPos{};
	dock->getEnterPosition(*recruit, &enterPos);
	dock->getDockPosition(*recruit, &dockPos);
	dock->getExitPosition(*recruit, &exitPos);
	CHECK(enterPos.x == expectedSlot.x);
	CHECK(enterPos.y == expectedSlot.y);
	CHECK(dockPos.x == enterPos.x);
	CHECK(dockPos.y == enterPos.y);
	CHECK(exitPos.x == enterPos.x);
	CHECK(exitPos.y == enterPos.y);

	// action (RW 0x89E133) sets HORDE_MEMBER (status 0x26), moves recruit from entering set into contain list, and returns false.
	const int hordeMemberBit = ObjectTemplateInfoBuilder::objectStatusIndex("HORDE_MEMBER");
	REQUIRE(hordeMemberBit >= 0); // tolerance: status IDs are resolved by name and only need to be present in this template registry.
	CHECK_FALSE(recruit->testStatus((unsigned)hordeMemberBit));
	CHECK_FALSE(dock->action(*recruit, nullptr));
	CHECK(recruit->testStatus((unsigned)hordeMemberBit));
	CHECK(recruit->getContainedBy() == horde);
	CHECK(hc->getContainCount() == 2);
	CHECK(hc->allMembersEntered());
}

TEST_CASE("PORT-283: HordeAIUpdate guards privateMoveToPosition and privateEnter when HordeContain is absent")
{
	Port283World w;
	Object *bare = w.spawn("P283ContainlessHordeAI", 400.0f, 400.0f, 0.0f);
	Object *ship = w.spawn("P283Enemy", 440.0f, 400.0f, 0.0f);
	HordeAIUpdate *hai = w.haiOf(bare);
	REQUIRE(hai != nullptr);
	REQUIRE(bare->getContain() == nullptr);

	// RW 0x89E0F6: move order is a no-op without a HordeContainInterface.
	const Coord3D dest{ 550.0f, 400.0f, 0.0f };
	hai->aiMoveToPosition(dest, CMD_FROM_PLAYER);
	w.frames(10);
	CHECK_FALSE(hai->isMoving());
	CHECK(bare->getPosition()->x == 400.0f);

	// RW 0x89E41F: enter order refuses without a HordeContainInterface.
	CHECK_FALSE(hai->aiEnter(ship, CMD_FROM_PLAYER));
}

TEST_CASE("PORT-283: HordeAIUpdate porcupine command tables and runtime auto-toggle / command blocking")
{
	// Exhaustive check of RW 0x89DE81 (table RW 0x89DEBD) and RW 0x89DF11 (table RW 0x89DF3F).
	const auto isAlwaysTrue = [](int c) { return c == 0x2E || c == 0x30; };
	const auto isAlwaysFalse = [](int c) {
		return c == 0x05 || c == 0x15 || c == 0x16 || c == 0x26 || c == 0x27 || c == 0x2D || c == 0x2F || c == 0x3B || c == 0x43 || c == 0x52;
	};
	for (int c = 0; c <= 0x53; ++c)
	{
		if (isAlwaysTrue(c))
		{
			CHECK(HordeAIUpdate::commandTogglesPorcupine(c, CMD_FROM_PLAYER));
			CHECK(HordeAIUpdate::commandTogglesPorcupine(c, CMD_FROM_AI));
			CHECK(HordeAIUpdate::commandTogglesPorcupine(c, CMD_FROM_SCRIPT));
		}
		else if (isAlwaysFalse(c))
		{
			CHECK_FALSE(HordeAIUpdate::commandTogglesPorcupine(c, CMD_FROM_PLAYER));
			CHECK_FALSE(HordeAIUpdate::commandTogglesPorcupine(c, CMD_FROM_AI));
		}
		else
		{
			CHECK(HordeAIUpdate::commandTogglesPorcupine(c, CMD_FROM_PLAYER));
			CHECK_FALSE(HordeAIUpdate::commandTogglesPorcupine(c, CMD_FROM_AI));
			CHECK_FALSE(HordeAIUpdate::commandTogglesPorcupine(c, CMD_FROM_SCRIPT));
		}
		const bool expectBlocked = (c != 0x0B && c != 0x52);
		CHECK(HordeAIUpdate::commandBlockedInPorcupine(c) == expectBlocked);
	}
	CHECK_FALSE(HordeAIUpdate::commandTogglesPorcupine(0x54, CMD_FROM_PLAYER));
	CHECK_FALSE(HordeAIUpdate::commandBlockedInPorcupine(0x54));

	// Runtime: AI move is blocked in porcupine; AI attack (0x0B) is allowed by the command table; player move auto-toggles out.
	Port283World w;
	w.buildMap();
	Object *pike = w.spawn("P283InfantryHorde", 400.0f, 400.0f, 0.0f);
	HordeContain *porc = w.containOf(pike)->toggleFormation();
	REQUIRE(porc != nullptr);
	REQUIRE(porc->isPorcupineFormation());

	const Coord3D marchDest{ 520.0f, 520.0f, 0.0f };
	pike->getAIUpdateInterface()->aiMoveToPosition(marchDest, CMD_FROM_AI);
	CHECK(w.containOf(pike)->isPorcupineFormation());
	CHECK_FALSE(pike->getAIUpdateInterface()->isMoving());

	CHECK_FALSE(HordeAIUpdate::commandBlockedInPorcupine(0x0B));
	CHECK(w.containOf(pike)->isPorcupineFormation());

	pike->getAIUpdateInterface()->aiMoveToPosition(marchDest, CMD_FROM_PLAYER);
	CHECK_FALSE(w.containOf(pike)->isPorcupineFormation());
	CHECK(pike->getAIUpdateInterface()->isMoving());
}

TEST_CASE("PORT-283: HordeAIUpdate update recentres empty horde, reunites idle entering members, consumes pending rally, and skips when garrisoned")
{
	Port283World w;
	w.buildMap();
	Object *horde = w.spawn("P283CavalryHorde", 400.0f, 400.0f, 0.0f);
	HorseHordeContain *hc = w.horseOf(horde);
	REQUIRE(hc != nullptr);
	std::vector<Object *> members = w.membersOf(horde);
	REQUIRE(members.size() == 4);

	// 1. Garrisoned guard (RW 0x89E2F8): IS_LEAVING_FACTORY and pending rally stay untouched while garrisoned.
	const int leavingBit = ObjectTemplateInfoBuilder::objectStatusIndex("IS_LEAVING_FACTORY");
	REQUIRE(leavingBit >= 0); // tolerance: status IDs are resolved by name and only need to be present in this template registry.
	horde->setStatus((unsigned)leavingBit, true);
	hc->setPendingRallyPoint({ 620.0f, 400.0f, 0.0f });
	hc->setGarrisoned(true);
	w.frames(12);
	CHECK(horde->testStatus((unsigned)leavingBit));
	CHECK(horde->getPosition()->x == 400.0f);

	// 2. Ungarrisoned (RW 0x89E38A .. 0x89E3E8): once no member has IS_LEAVING_FACTORY, retires status and walks to the consumed rally point.
	hc->setGarrisoned(false);
	w.frames(15);
	CHECK_FALSE(horde->testStatus((unsigned)leavingBit));
	Coord3D leftover{};
	CHECK_FALSE(hc->consumePendingRallyPoint(leftover));
	CHECK(horde->getPosition()->x > 400.0f); // tolerance: pending rally aiMoveToPosition moved the horde east

	// 3. Empty idle horde with displaced idle entering members recentres on them (RW 0x89E35A) and pulls them back into formation (RW 0x89E403).
	Object *stranded = w.spawn("P283InfantryHorde", 300.0f, 300.0f, 0.0f);
	HordeContain *shc = w.containOf(stranded);
	REQUIRE(shc != nullptr);
	std::vector<Object *> sm = w.membersOf(stranded);
	REQUIRE(sm.size() == 2);
	const Coord3D farSpot{ 700.0f, 300.0f, 0.0f };
	for (Object *m : sm)
	{
		shc->releaseMemberForGarrison(m);
		m->setPosition(&farSpot);
		m->getAIUpdateInterface()->aiIdle(CMD_FROM_AI);
	}
	REQUIRE(shc->getContainCount() == 0);
	REQUIRE(shc->hasLiveEnteringMembers());
	REQUIRE(shc->allEnteringMembersIdle());

	w.frames(8);
	CHECK(stranded->getPosition()->x == 700.0f);
	CHECK(shc->getContainCount() == 2);
	CHECK(shc->allMembersEntered());
}

TEST_CASE("PORT-283: HorseHordeContain and HordeContain v3 xfer + crc round-trip and mutation coverage")
{
	Port283World wA(0x2835A7E1u);
	Object *cavA = wA.spawn("P283CavalryHorde", 400.0f, 400.0f, 0.0f);
	HorseHordeContain *hcA = wA.horseOf(cavA);
	HordeAIUpdate *haiA = wA.haiOf(cavA);
	REQUIRE(hcA != nullptr);
	REQUIRE(haiA != nullptr);

	const std::uint32_t baseCrc = crcOf(*hcA);
	hcA->endReform();
	const std::uint32_t reformCrc = crcOf(*hcA);
	CHECK(reformCrc != baseCrc);

	hcA->setPendingRallyPoint({ 555.0f, 444.0f, 12.0f });
	const std::uint32_t rallyCrc = crcOf(*hcA);
	CHECK(rallyCrc != reformCrc);

	XferSave savedContain;
	hcA->xfer(savedContain);
	XferSave savedAI;
	haiA->xfer(savedAI);
	const std::uint32_t aiCrc = crcOf(*haiA);

	// Fresh world -> load snapshot -> exact CRC, pending rally, reform latch and re-serialized bytes match.
	Port283World wB(0x2835A7E1u);
	Object *cavB = wB.spawn("P283CavalryHorde", 400.0f, 400.0f, 0.0f);
	HorseHordeContain *hcB = wB.horseOf(cavB);
	HordeAIUpdate *haiB = wB.haiOf(cavB);
	REQUIRE(hcB != nullptr);
	REQUIRE(haiB != nullptr);
	CHECK_FALSE(hcB->isHorseReforming());
	CHECK(crcOf(*hcB) != rallyCrc);

	XferLoad loadContain(savedContain.data());
	hcB->xfer(loadContain);
	CHECK(loadContain.atEnd());
	CHECK(hcB->isHorseReforming());
	CHECK(crcOf(*hcB) == rallyCrc);

	XferSave reSaved;
	hcB->xfer(reSaved);
	CHECK(reSaved.data() == savedContain.data());

	Coord3D restoredRally{};
	CHECK(hcB->consumePendingRallyPoint(restoredRally));
	CHECK(restoredRally.x == 555.0f);
	CHECK(restoredRally.y == 444.0f);
	CHECK(restoredRally.z == 12.0f);

	XferLoad loadAI(savedAI.data());
	haiB->xfer(loadAI);
	CHECK(loadAI.atEnd());
	CHECK(crcOf(*haiB) == aiCrc);

	// Corrupt HorseHordeContain version byte is rejected.
	std::vector<std::uint8_t> corrupt = savedContain.data();
	corrupt[0] = 99;
	XferLoad badLoad(corrupt);
	CHECK_THROWS_AS(hcB->xfer(badLoad), XferException);
}
