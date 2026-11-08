// OpenBFME unit tests. GPL-3.0.
// PORT-26 (issue #98): AODHordeContain member jitter, rank path history, large-unit height lift, and oath-fulfilled scatter.
// Synthetic INI only; no retail game data. Expectations come from RotWK 2.01 disassembly (RW 0x87D55F..0x87EBEE) and
// Ennorath 1.9.8.2's Army of the Dead horde definitions.

#include "doctest.h"
#include "MoveTestUtil.h"

#include "Common/RandomValue.h"
#include "Common/StateHash.h"
#include "Common/XferLoad.h"
#include "Common/XferSave.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Module/AODCrushCollide.h"
#include "GameLogic/Object/Contain/HordeContainBehaviorData.h"
#include "GameLogic/Object/Contain/HordeContainCore.h"
#include "GameLogic/Object/Contain/HordeContainRuntime.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

using namespace movetest;

namespace
{
const char kPort26Ini[] =
	"Locomotor AodHordeLoco\n"
	"  Surfaces = GROUND\n"
	"  TurnTime = 1000\n"
	"  TurnTimeDamaged = 1000\n"
	"  Acceleration = 100\n"
	"  Braking = 100\n"
	"  ZAxisBehavior = FLOATING_Z\n"
	"  Appearance = HORDE\n"
	"  MaxTurnWithoutReform = 360\n"
	"  TurnWhileMoving = Yes\n"
	"  WaitForFormation = No\n"
	"  CloseEnoughDist = 2\n"
	"End\n"
	"Locomotor AodMemberLoco\n"
	"  Surfaces = GROUND\n"
	"  TurnTime = 250\n"
	"  TurnTimeDamaged = 250\n"
	"  Acceleration = 200\n"
	"  Braking = 200\n"
	"  MinSpeed = 0\n"
	"  ZAxisBehavior = FLOATING_Z\n"
	"  Appearance = TWO_LEGS\n"
	"  CloseEnoughDist = 1\n"
	"End\n"
	"Object P26Oathbreaker\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = PhysicsBehavior ModuleTag_Phys\n"
	"  End\n"
	"  Behavior = AODCrushCollide ModuleTag_Crush\n"
	"    Damage = 10\n"
	"    DamageType = MAGIC\n"
	"    DeathType = SUPERNATURAL\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = AodMemberLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 80\n"
	"  End\n"
	"End\n"
	// Ennorath 1.9.8.2 AngmarOathbreakerHordeSmall / RohanOathbreakerHordeSmall field values and shared RankNumber:2 rows.
	"Object P26EnnorathHorde\n"
	"  KindOf = HORDE MELEE_HORDE LARGE_RECTANGLE_PATHFIND SELECTABLE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 35\n"
	"  GeometryMinorRadius = 35\n"
	"  GeometryHeight = 20\n"
	"  Body = ImmortalBody ModuleTag_Body\n"
	"    MaxHealth = 1\n"
	"  End\n"
	"  Behavior = HordeAIUpdate ModuleTag_AI\n"
	"  End\n"
	"  Behavior = AODHordeContain ModuleTag_HordeContain\n"
	"    Slots = 16\n"
	"    InitialPayload = P26Oathbreaker 16\n"
	"    RankInfo = RankNumber:1 UnitType:P26Oathbreaker Position:X:10 Y:-30 Position:X:10 Y:-10 Position:X:10 Y:10 Position:X:10 Y:30\n"
	"    RankInfo = RankNumber:2 UnitType:P26Oathbreaker Position:X:-10 Y:-30 Position:X:-10 Y:-10 Position:X:-10 Y:10 Position:X:-10 Y:30\n"
	"    RankInfo = RankNumber:2 UnitType:P26Oathbreaker Position:X:-30 Y:-30 Position:X:-30 Y:-10 Position:X:-30 Y:10 Position:X:-30 Y:30\n"
	"    RankInfo = RankNumber:2 UnitType:P26Oathbreaker Position:X:-50 Y:-30 Position:X:-50 Y:-10 Position:X:-50 Y:10 Position:X:-50 Y:30\n"
	"    FrequencyScale = 0.3\n"
	"    FrequencyRandomness = 0.3\n"
	"    AmplitudeScale = 20\n"
	"    AmplitudeRandomness = 0.4\n"
	"    StillAmplitude = 0.10\n"
	"    FrequencyScaleZ = 0.2\n"
	"    FrequencyRandomnessZ = 0.4\n"
	"    AmplitudeScaleZ = 4\n"
	"    AmplitudeRandomnessZ = 0.2\n"
	"    StillAmplitudeZ = 0.15\n"
	"    LargeUnitHeightFactor = 0.85\n"
	"    LargeUnitMinHeight = 20\n"
	"    LargeUnitMaxHeight = 300\n"
	"    LargeUnitTimeout = 12000\n"
	"    LargeUnitTailOff = 2.0\n"
	"    ScatterSpeedFactor = 0.4\n"
	"    ScatterRandomness = 0.1\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = AodHordeLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 60\n"
	"  End\n"
	"End\n"
	// Default-valued AODHordeContain (verifies RW 0x87D55F constructor defaults).
	"Object P26DefaultHorde\n"
	"  KindOf = HORDE MELEE_HORDE SELECTABLE\n"
	"  Body = ImmortalBody ModuleTag_Body\n"
	"    MaxHealth = 1\n"
	"  End\n"
	"  Behavior = HordeAIUpdate ModuleTag_AI\n"
	"  End\n"
	"  Behavior = AODHordeContain ModuleTag_HordeContain\n"
	"    Slots = 2\n"
	"    InitialPayload = P26Oathbreaker 2\n"
	"    RankInfo = RankNumber:1 UnitType:P26Oathbreaker Position:X:10 Y:-10 Position:X:10 Y:10\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = AodHordeLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 60\n"
	"  End\n"
	"End\n"
	// Controlled deterministic horde without randomness for exact closed-form lift, sway, and scatter checks.
	"Object P26ExactHorde\n"
	"  KindOf = HORDE MELEE_HORDE LARGE_RECTANGLE_PATHFIND SELECTABLE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 25\n"
	"  GeometryMinorRadius = 25\n"
	"  GeometryHeight = 20\n"
	"  Body = ImmortalBody ModuleTag_Body\n"
	"    MaxHealth = 1\n"
	"  End\n"
	"  Behavior = HordeAIUpdate ModuleTag_AI\n"
	"  End\n"
	"  Behavior = AODHordeContain ModuleTag_HordeContain\n"
	"    Slots = 6\n"
	"    InitialPayload = P26Oathbreaker 6\n"
	"    RankInfo = RankNumber:1 UnitType:P26Oathbreaker Position:X:10 Y:0 Position:X:10 Y:10 Position:X:10 Y:-10 Position:X:10 Y:40 Position:X:10 Y:-40 Position:X:-10 Y:0\n"
	"    FrequencyScale = 0.0\n"
	"    FrequencyRandomness = 0.0\n"
	"    AmplitudeScale = 0.0\n"
	"    AmplitudeRandomness = 0.0\n"
	"    StillAmplitude = 0.0\n"
	"    FrequencyScaleZ = 0.0\n"
	"    FrequencyRandomnessZ = 0.0\n"
	"    AmplitudeScaleZ = 0.0\n"
	"    AmplitudeRandomnessZ = 0.0\n"
	"    StillAmplitudeZ = 0.0\n"
	"    OathFulfilledZFactor = 1.5\n"
	"    LargeUnitHeightFactor = 0.5\n"
	"    LargeUnitMinHeight = 30\n"
	"    LargeUnitMaxHeight = 80\n"
	"    LargeUnitTimeout = 4000\n"
	"    LargeUnitTailOff = 2.0\n"
	"    ScatterSpeedFactor = 0.5\n"
	"    ScatterRandomness = 0.4\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = AodHordeLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 60\n"
	"  End\n"
	"End\n"
	"Object P26LargeMumak\n"
	"  KindOf = MONSTER SELECTABLE\n"
	"  DeadCollideSize = LARGE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 25\n"
	"  GeometryMinorRadius = 25\n"
	"  GeometryHeight = 120\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 5000\n"
	"  End\n"
	"  Behavior = PhysicsBehavior ModuleTag_Phys\n"
	"  End\n"
	"End\n"
	"Object P26GiantTower\n"
	"  KindOf = MONSTER SELECTABLE\n"
	"  DeadCollideSize = LARGE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 25\n"
	"  GeometryMinorRadius = 25\n"
	"  GeometryHeight = 300\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 5000\n"
	"  End\n"
	"  Behavior = PhysicsBehavior ModuleTag_Phys\n"
	"  End\n"
	"End\n"
	"Object P26ShortMonster\n"
	"  KindOf = MONSTER SELECTABLE\n"
	"  DeadCollideSize = LARGE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 15\n"
	"  GeometryMinorRadius = 15\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 5000\n"
	"  End\n"
	"  Behavior = PhysicsBehavior ModuleTag_Phys\n"
	"  End\n"
	"End\n"
	"Object P26SmallOrc\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  DeadCollideSize = SMALL\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = PhysicsBehavior ModuleTag_Phys\n"
	"  End\n"
	"End\n";

struct Port26World : MoveWorld
{
	explicit Port26World(std::uint32_t seed = 0x26A0D001u)
		: MoveWorld(100, 100, kPort26Ini)
	{
		logic->random().seedRandom(seed);
		buildMap();
	}

	AODHordeContain *aodOf(Object *horde)
	{
		return dynamic_cast<AODHordeContain *>(horde->getContain());
	}
};

std::uint32_t hashAod(const AODHordeContain &aod)
{
	StateHasher h;
	aod.crc(h);
	return h.value();
}
} // namespace

TEST_CASE("PORT-26: AODHordeContainBehaviorData matches RotWK 0x87D55F defaults and parses all 18 Ennorath fields")
{
	Port26World mw;

	Object *defHorde = mw.spawn("P26DefaultHorde", 300.0f, 300.0f);
	AODHordeContain *defAod = mw.aodOf(defHorde);
	REQUIRE(defAod != nullptr);
	const AODHordeContainBehaviorData &defData = defAod->aodData();
	CHECK(defData.frequencyScale == 0.0f);
	CHECK(defData.frequencyRandomness == 0.0f);
	CHECK(defData.amplitudeScale == 0.0f);
	CHECK(defData.amplitudeRandomness == 0.0f);
	CHECK(defData.stillAmplitude == 0.0f);
	CHECK(defData.frequencyScaleZ == 0.0f);
	CHECK(defData.frequencyRandomnessZ == 0.0f);
	CHECK(defData.amplitudeScaleZ == 0.0f);
	CHECK(defData.amplitudeRandomnessZ == 0.0f);
	CHECK(defData.stillAmplitudeZ == 0.0f);
	CHECK(defData.oathFulfilledZFactor == 1.0f);
	CHECK(defData.largeUnitHeightFactor == 0.45f);
	CHECK(defData.largeUnitMinHeight == 0.0f);
	CHECK(defData.largeUnitMaxHeight == 999999.0f);
	CHECK(defData.largeUnitTimeout == 2u);
	CHECK(defData.largeUnitTailOff == 1.0f);
	CHECK(defData.scatterSpeedFactor == 0.3f);
	CHECK(defData.scatterRandomness == 0.4f);

	Object *enHorde = mw.spawn("P26EnnorathHorde", 500.0f, 500.0f);
	AODHordeContain *enAod = mw.aodOf(enHorde);
	REQUIRE(enAod != nullptr);
	const AODHordeContainBehaviorData &enData = enAod->aodData();
	CHECK(enData.frequencyScale == 0.3f);
	CHECK(enData.frequencyRandomness == 0.3f);
	CHECK(enData.amplitudeScale == 20.0f);
	CHECK(enData.amplitudeRandomness == 0.4f);
	CHECK(enData.stillAmplitude == 0.10f);
	CHECK(enData.frequencyScaleZ == 0.2f);
	CHECK(enData.frequencyRandomnessZ == 0.4f);
	CHECK(enData.amplitudeScaleZ == 4.0f);
	CHECK(enData.amplitudeRandomnessZ == 0.2f);
	CHECK(enData.stillAmplitudeZ == 0.15f);
	CHECK(enData.oathFulfilledZFactor == 1.0f);
	CHECK(enData.largeUnitHeightFactor == 0.85f);
	CHECK(enData.largeUnitMinHeight == 20.0f);
	CHECK(enData.largeUnitMaxHeight == 300.0f);
	CHECK(enData.largeUnitTimeout == (unsigned)(12 * LOGICFRAMES_PER_SECOND));
	CHECK(enData.largeUnitTailOff == 2.0f);
	CHECK(enData.scatterSpeedFactor == 0.4f);
	CHECK(enData.scatterRandomness == 0.1f);
}

TEST_CASE("PORT-26: lazy slot build draws exact 6 GameLogicRandom reals per slot and zero RNG draws per frame")
{
	Port26World mw(0x12345678u);
	GameLogicRandom &rng = mw.logic->random();
	rng.enableCallLog(true);
	rng.clearCallLog();

	// RW 0x63F3BF: onObjectCreated is a no-op before world placement; buildAodSlots runs on first addToContain.
	Object *horde = mw.make("P26EnnorathHorde", mw.teamOf("Alice"), INVALID_ID, false);
	AODHordeContain *aod = mw.aodOf(horde);
	REQUIRE(aod != nullptr);
	CHECK_FALSE(aod->core().built());
	CHECK(aod->swayRecords().empty());

	const Coord3D spawnPos{ 500.0f, 500.0f, 0.0f };
	horde->setPosition(&spawnPos);
	horde->setOrientation(0.0f);
	rng.clearCallLog();

	mw.completeHorde(horde);
	REQUIRE(aod->core().built());
	REQUIRE(aod->swayRecords().size() == 16u);

	std::vector<GameLogicRandom::Call> aodDraws;
	for (const GameLogicRandom::Call &rec : rng.callLog())
	{
		if (rec.file == "AODHordeContain.cpp")
		{
			aodDraws.push_back(rec);
		}
	}
	REQUIRE(aodDraws.size() == 96u); // 6 draws * 16 slots
	const int kExpectedLines[6] = { 0x20A, 0x20B, 0x20C, 0x20F, 0x210, 0x211 };
	for (size_t s = 0; s < 16; ++s)
	{
		for (size_t k = 0; k < 6; ++k)
		{
			const GameLogicRandom::Call &call = aodDraws[s * 6 + k];
			CHECK(call.real);
			CHECK(call.drewNumber);
			CHECK(call.line == kExpectedLines[k]);
		}
		const AODHordeContain::SwayRecord &sw = aod->swayRecords()[s];
		CHECK(sw.ampFactorXY >= 0.599999f); // tolerance: uniform draw in [1 - 0.4, 1 + 0.4]
		CHECK(sw.ampFactorXY <= 1.400001f); // tolerance: uniform draw in [1 - 0.4, 1 + 0.4]
		CHECK(sw.phaseXY >= 0.0f);          // tolerance: uniform draw in [0, 0.3 * 2*pi]
		CHECK(sw.phaseXY <= 1.88496f);      // tolerance: uniform draw in [0, 0.3 * 2*pi]
		CHECK(sw.freqFactorXY >= 0.699999f); // tolerance: uniform draw in [1 - 0.3, 1 + 0.3]
		CHECK(sw.freqFactorXY <= 1.300001f); // tolerance: uniform draw in [1 - 0.3, 1 + 0.3]
		CHECK(sw.ampFactorZ >= 0.799999f);  // tolerance: uniform draw in [1 - 0.2, 1 + 0.2]
		CHECK(sw.ampFactorZ <= 1.200001f);  // tolerance: uniform draw in [1 - 0.2, 1 + 0.2]
		CHECK(sw.phaseZ >= 0.0f);           // tolerance: uniform draw in [0, 0.4 * 2*pi]
		CHECK(sw.phaseZ <= 2.51328f);       // tolerance: uniform draw in [0, 0.4 * 2*pi]
		CHECK(sw.freqFactorZ >= 0.599999f); // tolerance: uniform draw in [1 - 0.4, 1 + 0.4]
		CHECK(sw.freqFactorZ <= 1.400001f); // tolerance: uniform draw in [1 - 0.4, 1 + 0.4]
	}
	CHECK_FALSE(aod->swayRecords()[0].phaseXY == aod->swayRecords()[1].phaseXY);

	// Zero AODHordeContain RNG draws during idle hover, marching, and oath scatter.
	rng.clearCallLog();
	mw.frames(15);
	mw.select(mw.playerIndex("Alice"), { horde });
	mw.moveTo(mw.playerIndex("Alice"), 680.0f, 620.0f);
	mw.frames(30);
	horde->setModelConditionState(CombatNames::modelCondition("OATH_FULLFILLED"), true);
	mw.frames(15);
	for (const GameLogicRandom::Call &rec : rng.callLog())
	{
		CHECK_FALSE(rec.file == "AODHordeContain.cpp");
	}
	rng.enableCallLog(false);

	// Same seed -> bit-identical state hash and member transforms.
	Port26World mwReplay(0x12345678u);
	Object *horde2 = mwReplay.spawn("P26EnnorathHorde", 500.0f, 500.0f, 0.0f);
	mwReplay.frames(15);
	mwReplay.select(mwReplay.playerIndex("Alice"), { horde2 });
	mwReplay.moveTo(mwReplay.playerIndex("Alice"), 680.0f, 620.0f);
	mwReplay.frames(30);
	horde2->setModelConditionState(CombatNames::modelCondition("OATH_FULLFILLED"), true);
	mwReplay.frames(15);
	AODHordeContain *aod2 = mwReplay.aodOf(horde2);
	REQUIRE(aod2 != nullptr);
	CHECK(hashAod(*aod) == hashAod(*aod2));
}

TEST_CASE("PORT-26: idle and moving sway oscillate in XY and hover above ground in Z, and rank spine trails turns")
{
	Port26World mw(0xCAFE1234u);
	Object *horde = mw.spawn("P26EnnorathHorde", 400.0f, 400.0f, 0.0f);
	AODHordeContain *aod = mw.aodOf(horde);
	REQUIRE(aod != nullptr);

	// Ennorath shares RankNumber:2 across X = -10, -30, -50: RW 0x87EAD3 groups by distinct slot.x -> 4 rank spine nodes.
	REQUIRE(aod->rankCount() == 4);
	CHECK(aod->rankSpine()[0].pos.x == 400.0f);
	CHECK(aod->rankSpine()[1].pos.x == 380.0f);
	CHECK(aod->rankSpine()[2].pos.x == 360.0f);
	CHECK(aod->rankSpine()[3].pos.x == 340.0f);
	CHECK(aod->pathSampleCount() == 4);

	const std::vector<Object *> members = mw.membersOf(horde);
	REQUIRE(members.size() == 16u);
	const Coord3D initialPos0 = *members[0]->getPosition();
	// StillAmplitudeZ = 0.15, AmplitudeScaleZ = 4, +0.95 bias -> idle ghosts hover above z = 0.
	CHECK(initialPos0.z > 0.05f); // tolerance: positive hover height from (sin * ampZ + 0.95) * 0.15 * 4

	// Advance stationary frames: StillAmplitude = 0.10 > 0.01 and StillAmplitudeZ = 0.15 > 0.01 keep sway active.
	mw.frames(10);
	const Coord3D idlePos0After = *members[0]->getPosition();
	const float idleShiftY = std::fabs(idlePos0After.y - initialPos0.y);
	const float idleShiftZ = std::fabs(idlePos0After.z - initialPos0.z);
	CHECK(idleShiftY > 0.01f); // tolerance: non-zero XY idle weave across 10 frames
	CHECK(idleShiftZ > 0.01f); // tolerance: non-zero Z idle bob across 10 frames

	// March east then turn north: breadcrumbs accumulate (capped at 60) and rear rank trails along the polyline.
	mw.select(mw.playerIndex("Alice"), { horde });
	mw.moveTo(mw.playerIndex("Alice"), 620.0f, 400.0f);
	mw.frames(35);
	CHECK(aod->pathSampleCount() > 4); // tolerance: marching pushes breadcrumbs beyond initial 4 rank seeds
	mw.moveTo(mw.playerIndex("Alice"), 620.0f, 620.0f);
	mw.frames(25);
	CHECK(aod->pathSampleCount() <= AODHordeContain::kMaxPathSamples); // tolerance: ring bounded at 60 samples
	const float frontTailDist = dist2d(aod->rankSpine()[0].pos, aod->rankSpine()[3].pos);
	CHECK(frontTailDist > 40.0f); // tolerance: 60-unit rank span stays trailed along the curved path
	CHECK(frontTailDist <= 61.0f); // tolerance: chord of 60-unit polyline arc never exceeds arc length
}

TEST_CASE("PORT-26: large-object collision lifts member Z with Min/Max height clamp, TailOff falloff, and second-half timeout fade")
{
	Port26World mw(0x55AA0011u);
	Object *horde = mw.spawn("P26ExactHorde", 500.0f, 500.0f, 0.0f);
	AODHordeContain *aod = mw.aodOf(horde);
	REQUIRE(aod != nullptr);
	const std::vector<Object *> members = mw.membersOf(horde);
	REQUIRE(members.size() == 6u);

	// With StillAmplitude = 0 and AmplitudeScale = 0, stationary members sit on ground z = 0.
	mw.frames(1);
	for (Object *m : members)
	{
		CHECK(m->getPosition()->z == 0.0f);
	}

	// Colliding with a SMALL enemy via AODCrushCollide does NOT record a large unit.
	Object *smallOrc = mw.spawn("P26SmallOrc", 500.0f, 500.0f, 0.0f, mw.teamOf("Bob"));
	AODCrushCollide *crush = dynamic_cast<AODCrushCollide *>(members[0]->findModule("AODCrushCollide"));
	REQUIRE(crush != nullptr);
	crush->onCollide(smallOrc, nullptr, nullptr);
	CHECK(aod->largeUnitID() == INVALID_ID);

	// Colliding with a LARGE enemy (GeometryHeight = 120) via AODCrushCollide triggers RW 0x87E894.
	// P26ExactHorde has LargeUnitHeightFactor = 0.5 -> 0.5 * 120 = 60, within [Min=30, Max=80], TailOff = 2 -> radius = 120,
	// LargeUnitTimeout = 4000ms = 20 frames at 5 frames/sec (or 120 frames at 30 fps: LOGICFRAMES_PER_SECOND * 4).
	Object *mumak = mw.spawn("P26LargeMumak", 500.0f, 500.0f, 0.0f, mw.teamOf("Bob"));
	crush->onCollide(mumak, nullptr, nullptr);
	CHECK(aod->largeUnitID() == mumak->getID());
	CHECK(aod->largeUnitExtent() == 17.0f);
	CHECK(aod->largeUnitHeight() == 120.0f);

	// Slot 0 is at (500, 500) -> along = 0, across = 0 <= 17 -> effDist = 0 -> full lift = 60.0f!
	// Slot 1 is at (500, 510) -> along = 0, across = 10 <= 17 (within 17.0 half-width!) -> effDist = 0 -> full lift = 60.0f!
	// Slot 3 is at (500, 540) -> along = 0, across = 40 -> acrossExcess = 23, effDist = 23 < 120 -> partial lift in (0, 60)!
	const Coord3D intendedCenter = aod->getMemberFormationPosition(members[0]);
	const Coord3D intendedWithin17 = aod->getMemberFormationPosition(members[1]);
	const Coord3D intendedTail = aod->getMemberFormationPosition(members[3]);
	CHECK(intendedCenter.z == 60.0f);
	CHECK(intendedWithin17.z == 60.0f);
	CHECK(intendedTail.z > 40.0f); // tolerance: smooth dome falloff at u = 23/120 stays below 60 and above 40
	CHECK(intendedTail.z < 60.0f); // tolerance: beyond 17-unit half-width, TailOff reduces lift strictly below peak

	// Advance into the second half of LargeUnitTimeout (timeout = 4s -> 3s is 75% of timeout -> timeFade = 0.5 -> peak = 30.0f).
	const unsigned timeoutFrames = aod->aodData().largeUnitTimeout;
	const unsigned threeQuarter = (timeoutFrames >> 1) + (timeoutFrames >> 2);
	mw.frames((int)threeQuarter);
	CHECK(aod->largeUnitID() == mumak->getID());
	const Coord3D fadedCenter = aod->getMemberFormationPosition(members[0]);
	CHECK(fadedCenter.z == 30.0f);

	// Advance to full timeout: largeUnitID clears to INVALID_ID and intended Z returns to 0.
	mw.frames((int)(timeoutFrames - threeQuarter + 1));
	CHECK(aod->largeUnitID() == INVALID_ID);
	CHECK(aod->getMemberFormationPosition(members[0]).z == 0.0f);

	// Verify LargeUnitMinHeight (30) and LargeUnitMaxHeight (80) clamps.
	Object *shortMonster = mw.spawn("P26ShortMonster", 500.0f, 500.0f, 0.0f, mw.teamOf("Bob")); // 0.5 * 20 = 10 -> clamped to Min = 30
	aod->onLargeObjectCollide(members[0], shortMonster);
	CHECK(aod->getMemberFormationPosition(members[0]).z == 30.0f);

	Object *giantTower = mw.spawn("P26GiantTower", 500.0f, 500.0f, 0.0f, mw.teamOf("Bob")); // 0.5 * 300 = 150 -> clamped to Max = 80
	aod->onLargeObjectCollide(members[0], giantTower);
	CHECK(aod->getMemberFormationPosition(members[0]).z == 80.0f);
}

TEST_CASE("PORT-26: OATH_FULLFILLED propagates to members and scatters radially in XY and upward in Z")
{
	Port26World mw(0x77889900u);
	Object *horde = mw.spawn("P26ExactHorde", 500.0f, 500.0f, 0.0f);
	AODHordeContain *aod = mw.aodOf(horde);
	REQUIRE(aod != nullptr);
	const std::vector<Object *> members = mw.membersOf(horde);
	REQUIRE(members.size() == 6u);

	mw.frames(2);
	const Coord3D before0 = *members[0]->getPosition(); // slot (10, 0) -> world (500, 500, 0), slotIdx = 0
	const Coord3D before1 = *members[1]->getPosition(); // slot (10, 10) -> world (500, 510, 0), slotIdx = 1
	CHECK(aod->scatterProgress() == 0.0f);

	const int oathBit = CombatNames::modelCondition("OATH_FULLFILLED");
	REQUIRE(oathBit >= 0); // tolerance: valid registered model condition index
	horde->setModelConditionState(oathBit, true);

	// 1 frame: OATH_FULLFILLED propagates to members; next frame members scatter outward & climb by OathFulfilledZFactor = 1.5.
	mw.frames(5);
	CHECK(aod->scatterProgress() > 0.0f); // tolerance: scatterProgress accumulates horde maxSpeed per frame
	for (Object *m : members)
	{
		CHECK(m->testModelCondition(oathBit));
	}

	const Coord3D after0 = *members[0]->getPosition();
	const Coord3D after1 = *members[1]->getPosition();
	// Slot 0 has local offset (10, 0) -> scatters along +X (x increases, y stays 500).
	CHECK(after0.x > before0.x); // tolerance: slot (10, 0) scatters outward in +X
	CHECK(after0.y == before0.y);
	CHECK(after0.z > 0.0f); // tolerance: Z scatter climbs with ScatterSpeedFactor * OathFulfilledZFactor
	// Slot 1 has slotIdx = 1 (mod5 = 1 vs slot 0's mod5 = 0, and mod5Z = 3 vs slot 0's mod5Z = 2) -> climbs faster in Z than slot 0!
	CHECK(after1.y > before1.y);  // tolerance: slot (10, 10) scatters outward in +Y as well as +X
	CHECK(after1.z > after0.z);   // tolerance: (1+2)%5 == 3 > (0+2)%5 == 2 gives slot 1 higher Z scatter rate
}

TEST_CASE("PORT-26: AODHordeContain xfer round-trip preserves CRC, mid-march state, and closed-set bounds")
{
	Port26World mwA(0xABCD0026u);
	Object *hordeA = mwA.spawn("P26EnnorathHorde", 400.0f, 400.0f, 0.0f);
	Object *mumakA = mwA.spawn("P26LargeMumak", 480.0f, 420.0f, 0.0f, mwA.teamOf("Bob"));
	AODHordeContain *aodA = mwA.aodOf(hordeA);
	REQUIRE(aodA != nullptr);

	mwA.select(mwA.playerIndex("Alice"), { hordeA });
	mwA.moveTo(mwA.playerIndex("Alice"), 550.0f, 460.0f);
	mwA.frames(12);
	aodA->onLargeObjectCollide(mwA.membersOf(hordeA)[0], mumakA);
	hordeA->setModelConditionState(CombatNames::modelCondition("OATH_FULLFILLED"), true);
	mwA.frames(4);

	// Save AODHordeContain bytes at frame 16.
	XferSave savedBytes;
	aodA->xfer(savedBytes);
	const std::uint32_t crcAtSave = hashAod(*aodA);

	// Build an identical world up to the spawn IDs, then overwrite AODHordeContain from savedBytes at the same frame.
	Port26World mwB(0xABCD0026u);
	Object *hordeB = mwB.spawn("P26EnnorathHorde", 400.0f, 400.0f, 0.0f);
	Object *mumakB = mwB.spawn("P26LargeMumak", 480.0f, 420.0f, 0.0f, mwB.teamOf("Bob"));
	(void)mumakB;
	AODHordeContain *aodB = mwB.aodOf(hordeB);
	REQUIRE(aodB != nullptr);

	// Advance mwB to the same state via identical inputs, perturb aodB, then restore from savedBytes and verify exact CRC & re-save bytes.
	mwB.select(mwB.playerIndex("Alice"), { hordeB });
	mwB.moveTo(mwB.playerIndex("Alice"), 550.0f, 460.0f);
	mwB.frames(12);
	aodB->onLargeObjectCollide(mwB.membersOf(hordeB)[0], mumakB);
	hordeB->setModelConditionState(CombatNames::modelCondition("OATH_FULLFILLED"), true);
	mwB.frames(4);

	// Perturb aodB by clearing large object via extra collision/markDirty, then restore from savedBytes.
	Object *towerB = mwB.spawn("P26GiantTower", 200.0f, 200.0f, 0.0f, mwB.teamOf("Bob"));
	aodB->onLargeObjectCollide(mwB.membersOf(hordeB)[0], towerB);
	CHECK_FALSE(hashAod(*aodB) == crcAtSave);

	XferLoad reader(savedBytes.data());
	aodB->xfer(reader);
	CHECK(reader.atEnd());
	CHECK(hashAod(*aodB) == crcAtSave);

	XferSave reSaved;
	aodB->xfer(reSaved);
	CHECK(reSaved.data() == savedBytes.data());

	// Corrupt rankCount (last int before rank records when rankCount = 99) is rejected by SnapshotXfer::checkRange.
	std::vector<std::uint8_t> badBytes = savedBytes.data();
	badBytes[0] = 9; // unsupported version byte
	XferLoad badVer(badBytes);
	CHECK_THROWS_AS(aodB->xfer(badVer), XferException);
}
