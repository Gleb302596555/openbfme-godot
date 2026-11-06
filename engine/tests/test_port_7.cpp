// OpenBFME. PORT-7 (issue #60) tests: AnimalAIUpdate/UpdateTimer — the frame gate of RW 0x882288 .. RW 0x8822A6
// (docs/re/rotwk/RW_0x882058.md).
//
// `UpdateTimer` is `AnimalAIUpdateModuleData + 0x78` (field row RW 0xC04F38, parser RW 0x42EC5E, unsigned int, the
// constructor's default at RW 0x64BB3D). Its only reader is `AnimalAIUpdate::update` (RW 0x882058):
//
//   RW 0x882288  mov ecx, [data+0x78]      UpdateTimer
//   RW 0x88228B  test ecx, ecx
//   RW 0x882292  jbe  RW 0x882508          zero -> no enemy scan at all
//   RW 0x882298  mov eax, [RW 0xDE412C]    TheGameLogic
//   RW 0x88229D  mov eax, [eax+0x40]       the logic frame counter
//   RW 0x8822A2  div ecx
//   RW 0x8822A4  test edx, edx
//   RW 0x8822A6  jne  RW 0x882508          remainder -> no enemy scan this frame
//
// RW 0x882508 is *not* a return: the update is 2,916 bytes (RW 0x882058 .. RW 0x882BBC, the same 2,916 bytes the
// BFME2 donor 0x47EDC9..0x47F92D has) and 1,716 bytes of code follow it, so both skips land inside the function —
// on the idle test, which stands *after* the gated scan. Two consequences the tests below hold:
//
//   * the gate covers the enemy scan alone. The wander dice (line 0x9a), the SALT_LICK scan and the wander orders
//     are below RW 0x882508 and run on every idle frame whatever UpdateTimer says;
//   * the scan runs before the idle test, so a critter that is already walking scans too.
//
// An enemy found inside the gated block makes the update return at once (UPDATE_SLEEP_NONE), before the dice: while
// an enemy is in FleeRange a critter draws no wander numbers. That was already the port's behaviour and stays.
//
// Synthetic data (movetest::MoveWorld), no retail files, no Ennorath files.
#include "doctest.h"
#include "MoveTestUtil.h"

#include "GameLogic/AI/AIStateMachine.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object/PartitionManager.h"

using namespace movetest;

namespace
{

// FleeRange 1000 and FleeDistance 30: the enemy of these tests stands 400 away on the critter's own y, so the flee
// destination is exactly (x - 30, y) — the direction is (-1, 0) and every step of the port's arithmetic is exact.
// WanderPercentage 0 makes the dice fail on every idle frame, so the flee is the only move order the critter can
// get and a change of the machine's goal is a flee. Port7NoScan is the same critter with UpdateTimer = 0.
const char kPort7Objects[] =
	"ChildObject Port7Critter Walker\n"
	"  Behavior = AnimalAIUpdate ModuleTag_AI\n"
	"    FleeRange = 1000\n"
	"    FleeDistance = 30\n"
	"    WanderPercentage = 0\n"
	"    MaxWanderDistance = 10\n"
	"    MaxWanderRadius = 10\n"
	"    UpdateTimer = 5\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_PANIC\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n"
	"ChildObject Port7NoScan Walker\n"
	"  Behavior = AnimalAIUpdate ModuleTag_AI\n"
	"    FleeRange = 1000\n"
	"    FleeDistance = 30\n"
	"    WanderPercentage = 0\n"
	"    MaxWanderDistance = 10\n"
	"    MaxWanderRadius = 10\n"
	"    UpdateTimer = 0\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_PANIC\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n";

const float kEnemyX = 900.0f;
const float kEnemyY = 500.0f;
const float kFleeDistance = 30.0f;

// the world a scan needs: the partition's region over the 100 x 100 cell terrain (10 units a cell), as the live
// game sets it, and the two players as enemies — Player::getRelationship answers NEUTRAL unless the relation is set
void prepareScanWorld(MoveWorld &mw)
{
	mw.logic->partition().setRegion(0.0f, 0.0f, 1000.0f, 1000.0f);
	Player *alice = mw.players.findPlayerWithName("Alice");
	Player *bob = mw.players.findPlayerWithName("Bob");
	alice->setPlayerRelationship(bob, ENEMIES);
	bob->setPlayerRelationship(alice, ENEMIES);
}

// the frames the gate opens on: RW 0x88229D reads the logic frame counter, which runLogicFrame advanced in phase 1
// before the modules ran, so the number the module divided is the one getFrame() gives back
bool gateOpen(unsigned frame, unsigned updateTimer)
{
	return updateTimer != 0 && frame % updateTimer == 0;
}

} // namespace

TEST_CASE("PORT-7 a critter scans for enemies only on frames that are a multiple of UpdateTimer (RW 0x882288 .. 0x8822A6)")
{
	MoveWorld mw(100, 100, kPort7Objects);
	mw.buildMap();
	prepareScanWorld(mw);
	Object *critter = mw.spawn("Port7Critter", 500, kEnemyY);
	mw.spawn("Walker", kEnemyX, kEnemyY, 0.0f, mw.teamOf("Bob")); // the enemy, 400 away: inside FleeRange 1000
	mw.frames(1);
	mw.logic->partition().setRegion(0.0f, 0.0f, 1000.0f, 1000.0f);
	mw.frames(1);
	AIUpdateInterface *ai = mw.aiOf(critter);
	REQUIRE(ai != nullptr);
	REQUIRE(critter->getPosition()->x == 500.0f);

	// 20 frames: the critter runs away from (900, 500), so it stays inside FleeRange the whole window and every
	// scan finds the enemy. A flee order is the only thing that can move the machine's goal here, and the port
	// stores an AI goal verbatim (goalPositionClipped clips a player's goal only). While the critter is already
	// walking the same order also pushes the temporary AI_MOVE_TO state, so both signals are watched.
	std::vector<unsigned> fled;
	std::vector<unsigned> expected;
	Coord3D goal = ai->stateMachine().goalPosition();
	unsigned tempEnd = ai->stateMachine().temporaryStateFrameEnd();
	for (int i = 0; i < 20; ++i)
	{
		const Coord3D before = *critter->getPosition();
		mw.frames(1);
		const unsigned frame = mw.logic->getFrame();
		if (gateOpen(frame, 5))
		{
			expected.push_back(frame);
		}
		const Coord3D now = ai->stateMachine().goalPosition();
		const unsigned nowEnd = ai->stateMachine().temporaryStateFrameEnd();
		if (now.x != goal.x || now.y != goal.y || nowEnd != tempEnd)
		{
			fled.push_back(frame);
			if (fled.size() == 1)
			{
				// the first flee runs from the untouched start, so the direction is exactly (-1, 0) and the
				// destination is exact: FleeDistance 30 away from the enemy
				CHECK(before.x == 500.0f);
				CHECK(before.y == kEnemyY);
				CHECK(now.x == 500.0f - kFleeDistance);
				CHECK(now.y == kEnemyY);
			}
			const bool awayFromEnemy = now.x < before.x; // the enemy stands at x = 900
			CHECK(awayFromEnemy);
			goal = now;
			tempEnd = nowEnd;
		}
	}
	CHECK(fled.size() == 4); // 20 frames, UpdateTimer 5
	CHECK(fled == expected); // and on no other frame
}

TEST_CASE("PORT-7 UpdateTimer = 0 never scans: the critter does not flee at all (RW 0x88228B / 0x882292)")
{
	MoveWorld mw(100, 100, kPort7Objects);
	mw.buildMap();
	prepareScanWorld(mw);
	// both critters see the same enemy inside FleeRange; only UpdateTimer differs (5 and 0)
	Object *scanning = mw.spawn("Port7Critter", 300, kEnemyY);
	Object *blind = mw.spawn("Port7NoScan", 500, 700);
	mw.spawn("Walker", kEnemyX, kEnemyY, 0.0f, mw.teamOf("Bob"));
	mw.frames(1);
	const Coord3D blindStart = *blind->getPosition();
	const Coord3D blindGoal = mw.aiOf(blind)->stateMachine().goalPosition();
	mw.frames(20);
	// the critter with a timer fled, so the enemy and the range are proven live in this very world: with
	// WanderPercentage = 0 a flee is the only order it can ever get
	CHECK(scanning->getPosition()->x != 300.0f);
	const bool ranAway = mw.aiOf(scanning)->stateMachine().goalPosition().x < 300.0f;
	CHECK(ranAway);
	// the critter with UpdateTimer = 0 never ran the scan: no order, no goal, no step
	CHECK(mw.aiOf(blind)->stateMachine().goalPosition().x == blindGoal.x);
	CHECK(mw.aiOf(blind)->stateMachine().goalPosition().y == blindGoal.y);
	CHECK(mw.aiOf(blind)->isIdle());
	CHECK(blind->getPosition()->x == blindStart.x);
	CHECK(blind->getPosition()->y == blindStart.y);
	CHECK(blind->getPosition()->z == blindStart.z);
}

TEST_CASE("PORT-7 the wander dice stays outside the gate: UpdateTimer does not thin it (RW 0x882508)")
{
	// no enemy in this world: the gated scan finds nothing and the update reaches the dice on every idle frame.
	// WanderPercentage = 0 fails the dice, so the critter never moves and stays idle — one draw of line 0x9a per
	// frame. Had the gate covered the whole decision, 20 frames with UpdateTimer 5 would give 4 draws, not 20.
	MoveWorld mw(100, 100, kPort7Objects);
	mw.buildMap();
	Object *critter = mw.spawn("Port7Critter", 500, kEnemyY);
	mw.frames(1);
	REQUIRE(mw.aiOf(critter)->isIdle());
	mw.logic->random().clearCallLog();
	mw.logic->random().enableCallLog(true);
	mw.frames(20);
	mw.logic->random().enableCallLog(false);
	int rolls = 0, angles = 0, distances = 0;
	for (const auto &c : mw.logic->random().callLog())
	{
		rolls += c.file == "AnimalAIUpdate.cpp" && c.line == 0x9a;
		angles += c.file == "AnimalAIUpdate.cpp" && (c.line == 0xbc || c.line == 0xf2);
		distances += c.file == "AnimalAIUpdate.cpp" && (c.line == 0xbf || c.line == 0xf5);
	}
	CHECK(rolls == 20); // every frame, not every fifth
	CHECK(angles == 0); // the dice failed each time, so no wander numbers were drawn
	CHECK(distances == 0);
	CHECK(mw.aiOf(critter)->isIdle());
}

TEST_CASE("PORT-7 the scan stands before the idle test: a walking critter still flees on a gate frame (RW 0x882508)")
{
	MoveWorld mw(100, 100, kPort7Objects);
	mw.buildMap();
	prepareScanWorld(mw);
	Object *critter = mw.spawn("Port7Critter", 500, kEnemyY);
	mw.spawn("Walker", kEnemyX, kEnemyY, 0.0f, mw.teamOf("Bob"));
	mw.frames(1);
	AIUpdateInterface *ai = mw.aiOf(critter);

	// a player's order sets it walking along the enemy's own y, so the flee destination stays exact; the walk is
	// long enough to still be running on the next gate frame
	ai->aiMoveToPosition(Coord3D{ 105.0f, kEnemyY, 0.0f }, CMD_FROM_PLAYER);
	while (!gateOpen(mw.logic->getFrame() + 1, 5))
	{
		mw.frames(1);
		REQUIRE_FALSE(ai->isIdle()); // walking, and no gate frame passed on the way
	}
	REQUIRE_FALSE(ai->isIdle());
	const Coord3D before = *critter->getPosition();
	const Coord3D walkGoal = ai->stateMachine().goalPosition();
	CHECK(walkGoal.x == 105.0f);
	mw.frames(1); // the gate frame, reached while the critter is not idle
	const Coord3D now = ai->stateMachine().goalPosition();
	CHECK(gateOpen(mw.logic->getFrame(), 5));
	const bool replacedTheWalk = now.x != walkGoal.x || now.y != walkGoal.y;
	CHECK(replacedTheWalk);
	const bool awayFromEnemy = now.x < before.x; // the enemy stands at x = 900
	CHECK(awayFromEnemy);
}
