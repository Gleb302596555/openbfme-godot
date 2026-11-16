// OpenBFME unit tests. GPL-3.0.
// Port 204 (issue #471, S-1029 / S-1028): crush warning path gate before NotifyTargetsOfImminentProbableCrushingUpdate /
// HordeNotifyTargetsOfImminentProbableCrushingUpdate and HordeContain +0x170 member list. Synthetic INI only.
//
// Every expectation is confirmed directly in RotWK 2.01 game.dat (md5 daed3668006cd90f01c34e5a7da1901f):
//   RW 0x6950F3  Object::canCrushOrSquishNoAlly(other, testType):
//                  - 0x695101..0x69511B: crusherLevel != 0 && crushableLevel < crusherLevel
//                  - 0x69511D..0x695144: bypass quickDoesPathExist iff ai != null && ai->isMoving() (RW 0x664485)
//                    && ai->m_path != null && path->hasExplicitZ() (RW 0x5E2DF4: m_path && m_closest &&
//                    (m_closest->m_nextOpti ?: m_closest)->m_waypointID != NO_WAYPOINT)
//                  - 0x695146..0x695160: otherwise Pathfinder::quickDoesPathExist(self, &self->pos, &other->pos, nullptr)
//                    (RW 0x6F5BB0); return false when false
//   RW 0x69519A  Object::canCrushOrSquish(other, testType): calls 0x6950F3; then RAMPAGING/FLEE_OFF_MAP/CHARGING, else ENEMIES + melee weapon
//   RW 0x661305  PartitionFilter (vtable 0xC76CD8 slot 1) used by NotifyCrush mux (RW 0x8D2B21): calls 0x69519A(other, 2)
//   RW 0x8704DA  HordeContainInterface slot 0x10C: clears outList, copies m_containList (slot 0x118), then walks H+0x170
//                (interface +0x54, std::set<ObjectID> ascending order RW 0x423EA0), appending each live findObjectByID (RW 0x449681)
//   RW 0x8D3167  HordeNotifyTargetsOfImminentProbableCrushingUpdate::update: calls slot 0x10C (RW 0x8D31AF -> 0x8704DA),
//                stable-sorts ascending 2D squared distance from horde centre (RW 0x8D3131 -> 0x8D2F92, comparator RW 0x8D2EC6),
//                then calls mux (RW 0x8D2B21) on each member until one returns true (RW 0x8D3213)
//   RW 0x8743A9  HordeContain::memberPass quarrel spectator nearest neighbour calls RW 0x870C29 (m_containList then H+0x170)
//   RW 0x86EE52  HordeContain::attackedWithin scans m_containList then H+0x170 (RW 0x86EECE, ascending ObjectID order)
//   RW 0x874084  HordeContain::memberPass skips contained members whose id sits in H+0x170 (RW 0x87408B)

#include "doctest.h"

#include "MoveTestUtil.h"

#include "GameLogic/AI/AIAttackMelee.h"
#include "GameLogic/AI/AIPathfind.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Damage.h"
#include "GameLogic/Module/ActiveBody.h"
#include "GameLogic/Module/EmotionModules.h"
#include "GameLogic/Module/NotifyCrushModules.h"
#include "GameLogic/Object/PartitionManager.h"
#include "GameLogic/SimMath.h"
#include "GameLogic/System/EmotionSystem.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace
{
const char kPort204Nuggets[] =
	"EmotionNugget CrushBrace\n"
	"  Type = BRACE_FOR_BEING_CRUSHED\n"
	"  ModelConditions = EMOTION_BRACE_FOR_BEING_CRUSHED\n"
	"  Duration = 2000\n"
	"End\n"
	"EmotionNugget CrushCheer\n"
	"  Type = CHEER_FOR_ABOUT_TO_CRUSH\n"
	"  ModelConditions = EMOTION_CELEBRATING\n"
	"  Duration = 2000\n"
	"End\n";

const char kPort204Objects[] =
	"Locomotor CrushLoco\n"
	"  Surfaces = GROUND\n"
	"  TurnTime = 200\n"
	"  TurnTimeDamaged = 200\n"
	"  Acceleration = 1000\n"
	"  Braking = 1000\n"
	"  MinSpeed = 0\n"
	"  ZAxisBehavior = NO_Z_MOTIVE_FORCE\n"
	"  Appearance = TWO_LEGS\n"
	"  StickToGround = Yes\n"
	"  CloseEnoughDist = 1\n"
	"End\n"
	"Object Port204Crusher\n"
	"  KindOf = CAVALRY SELECTABLE\n"
	"  CrusherLevel = 2\n"
	"  CrushableLevel = 2\n"
	"  MinCrushVelocityPercent = 1\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 200\n"
	"  End\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryMinorRadius = 10\n"
	"  GeometryHeight = 20\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = EmotionTrackerUpdate ModuleTag_Emotion\n"
	"    AddEmotion = CrushCheer\n"
	"  End\n"
	"  Behavior = NotifyTargetsOfImminentProbableCrushingUpdate ModuleTag_NotifyCrush\n"
	"    TimeBetweenUpdatesMS = 200\n"
	"    ScanAheadTimeMS = 2000\n"
	"    ScanHeight = 40\n"
	"    ScanWidth = 60\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = CrushLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 100\n"
	"  End\n"
	"End\n"
	"Object Port204Victim\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  CrusherLevel = 0\n"
	"  CrushableLevel = 0\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 12\n"
	"  GeometryMinorRadius = 12\n"
	"  GeometryHeight = 20\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = EmotionTrackerUpdate ModuleTag_Emotion\n"
	"    AddEmotion = CrushBrace\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = CrushLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 50\n"
	"  End\n"
	"End\n"
	"Object Port204Rider\n"
	"  KindOf = CAVALRY SELECTABLE\n"
	"  CrusherLevel = 2\n"
	"  CrushableLevel = 2\n"
	"  MinCrushVelocityPercent = 1\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 200\n"
	"  End\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryMinorRadius = 10\n"
	"  GeometryHeight = 20\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = EmotionTrackerUpdate ModuleTag_Emotion\n"
	"    AddEmotion = CrushCheer\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = CrushLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 100\n"
	"  End\n"
	"End\n"
	"Object Port204CrushHorde\n"
	"  KindOf = HORDE CAVALRY LARGE_RECTANGLE_PATHFIND SELECTABLE\n"
	"  CrusherLevel = 2\n"
	"  CrushableLevel = 2\n"
	"  MinCrushVelocityPercent = 1\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 30\n"
	"  GeometryMinorRadius = 45\n"
	"  GeometryHeight = 20\n"
	"  Behavior = HordeAIUpdate ModuleTag_AI\n"
	"  End\n"
	"  Behavior = HordeContain ModuleTag_Contain\n"
	"    RankInfo = RankNumber:1 UnitType:Port204Rider Position:X:20 Y:0 Position:X:20 Y:30 Position:X:20 Y:-30\n"
	"    InitialPayload = Port204Rider 3\n"
	"  End\n"
	"  Behavior = HordeNotifyTargetsOfImminentProbableCrushingUpdate ModuleTag_NotifyCrush\n"
	"    TimeBetweenUpdatesMS = 200\n"
	"    ScanAheadTimeMS = 1000\n"
	"    ScanHeight = 40\n"
	"    ScanWidth = 60\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = HordeLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 100\n"
	"  End\n"
	"End\n"
	"Object Port204QuarrelMember\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 6\n"
	"  GeometryMinorRadius = 6\n"
	"  GeometryHeight = 20\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = CrushLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 150\n"
	"  End\n"
	"End\n"
	"Object Port204QuarrelHorde\n"
	"  KindOf = HORDE MELEE_HORDE LARGE_RECTANGLE_PATHFIND SELECTABLE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 30\n"
	"  GeometryMinorRadius = 45\n"
	"  GeometryHeight = 20\n"
	"  Behavior = HordeAIUpdate ModuleTag_AI\n"
	"  End\n"
	"  Behavior = HordeContain ModuleTag_Contain\n"
	"    RankInfo = RankNumber:1 UnitType:Port204QuarrelMember Position:X:20 Y:0 Position:X:20 Y:20 Position:X:20 Y:-20 Position:X:20 Y:40\n"
	"    InitialPayload = Port204QuarrelMember 4\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = HordeLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 50\n"
	"  End\n"
	"End\n";

struct Port204World : movetest::MoveWorld
{
	EmotionSystem emotions;
	EmotionSystem *savedEmotions = TheEmotionSystem;

	explicit Port204World(bool cliffWall = false, int cellsW = 60, int cellsH = 40)
		: movetest::MoveWorld(cellsW, cellsH)
	{
		TheEmotionSystem = &emotions;
		emotions.registerBlock(w.fx.env.blocks);
		std::string err = w.load(kPort204Nuggets, INI_LOAD_OVERWRITE, "port204_emotions.ini");
		REQUIRE_MESSAGE(err.empty(), err);
		err = w.load(kPort204Objects, INI_LOAD_OVERWRITE, "port204_objects.ini");
		REQUIRE_MESSAGE(err.empty(), err);
		if (cliffWall)
		{
			// Full-height impassable cliff divider across x = 200..220 (cells 20..21)
			for (int y = 0; y < cellsH; ++y)
			{
				terrain.cliff.insert({ 20, y });
				terrain.cliff.insert({ 21, y });
			}
		}
		buildMap();
		logic->partition().setRegion(0.0f, 0.0f, (float)(cellsW * 10), (float)(cellsH * 10));
		frames(1);
	}

	~Port204World()
	{
		TheEmotionSystem = savedEmotions;
	}

	static NotifyCrushUpdate *notifyOf(Object *o)
	{
		for (const auto &m : o->modules())
		{
			if (auto *n = dynamic_cast<NotifyCrushUpdate *>(m.get()))
			{
				return n;
			}
		}
		return nullptr;
	}

	static bool hasEmotion(Object *o, EmotionType type)
	{
		EmotionTrackerUpdate *et = EmotionTrackerUpdate::of(*o);
		return et && et->current() && et->current()->tmpl().type == type;
	}
};
} // namespace

TEST_CASE("PORT-204 crush warning path gate: quickDoesPathExist blocks warning across a cliff and allows it on open ground (RW 0x6950F3 / 0x695159 -> 0x6F5BB0)")
{
	// Case A: cliff wall at x = 200..220 separates Crusher (moving east toward x = 190) from Victim (x = 235).
	// Velocity scan box (ScanAheadTimeMS = 1000 ms = 5 frames at speed ~100/sec = ~20/frame -> ~100 units ahead)
	// reaches past x = 235 and overlaps Victim's geometry, yet RW 0x695159 -> 0x6F5BB0 (quickDoesPathExist) returns false.
	{
		Port204World f(true);
		Object *crusher = f.spawn("Port204Crusher", 120.0f, 200.0f, 0.0f, f.teamOf("Alice"));
		Object *victim = f.spawn("Port204Victim", 235.0f, 200.0f, 0.0f, f.teamOf("Bob"));
		NotifyCrushUpdate *nc = Port204World::notifyOf(crusher);
		REQUIRE(nc != nullptr);
		REQUIRE_FALSE(f.ai->pathfinder().quickDoesPathExist(f.aiOf(crusher)->adapter(), crusher->getPosition(), victim->getPosition(), nullptr));

		f.aiOf(crusher)->aiMoveToPosition(Coord3D{ 190.0f, 200.0f, 0.0f }, CMD_FROM_PLAYER);
		bool braced = false;
		bool cheered = false;
		for (int i = 0; i < 15; ++i)
		{
			f.frames(1);
			braced = braced || Port204World::hasEmotion(victim, EMOTION_BRACE_FOR_BEING_CRUSHED);
			cheered = cheered || Port204World::hasEmotion(crusher, EMOTION_CHEER_FOR_ABOUT_TO_CRUSH);
		}
		CHECK(nc->scans() == 11u);
		CHECK(nc->warned() == 0u);
		CHECK_FALSE(braced);
		CHECK_FALSE(cheered);
	}

	// Case B (neighbouring behaviour held): identical geometry on open ground (no cliff).
	// RW 0x695159 -> 0x6F5BB0 succeeds, so the victim braces and the crusher cheers.
	{
		Port204World f(false);
		Object *crusher = f.spawn("Port204Crusher", 120.0f, 200.0f, 0.0f, f.teamOf("Alice"));
		Object *victim = f.spawn("Port204Victim", 235.0f, 200.0f, 0.0f, f.teamOf("Bob"));
		NotifyCrushUpdate *nc = Port204World::notifyOf(crusher);
		REQUIRE(nc != nullptr);
		REQUIRE(f.ai->pathfinder().quickDoesPathExist(f.aiOf(crusher)->adapter(), crusher->getPosition(), victim->getPosition(), nullptr));

		f.aiOf(crusher)->aiMoveToPosition(Coord3D{ 280.0f, 200.0f, 0.0f }, CMD_FROM_PLAYER);
		bool braced = false;
		bool cheered = false;
		for (int i = 0; i < 15; ++i)
		{
			f.frames(1);
			braced = braced || Port204World::hasEmotion(victim, EMOTION_BRACE_FOR_BEING_CRUSHED);
			cheered = cheered || Port204World::hasEmotion(crusher, EMOTION_CHEER_FOR_ABOUT_TO_CRUSH);
		}
		CHECK(nc->scans() == 14u);
		CHECK(nc->warned() == 7u);
		CHECK(braced);
		CHECK(cheered);
	}
}

TEST_CASE("PORT-204 crush policy gate bypass: moving on an explicit-Z / waypoint path (RW 0x664485 / 0x5E2DF4) bypasses quickDoesPathExist (RW 0x69511D .. 0x695160)")
{
	Port204World f(true);
	Object *crusher = f.spawn("Port204Crusher", 120.0f, 200.0f, 0.0f, f.teamOf("Alice"));
	Object *victim = f.spawn("Port204Victim", 235.0f, 200.0f, 0.0f, f.teamOf("Bob"));
	AIUpdateInterface *ai = f.aiOf(crusher);
	REQUIRE(ai != nullptr);
	REQUIRE_FALSE(f.ai->pathfinder().quickDoesPathExist(ai->adapter(), crusher->getPosition(), victim->getPosition(), nullptr));

	// 1. Idle (RW 0x664485 false): quickDoesPathExist runs and rejects across the cliff.
	CHECK_FALSE(ai->isMoving());
	CHECK_FALSE(AttackCrushPolicy(*crusher, *victim));

	// 2. Moving along a normal ground path without waypoint IDs (RW 0x664485 true, RW 0x5E2DF4 false):
	// quickDoesPathExist still runs and rejects across the cliff.
	ai->aiMoveToPosition(Coord3D{ 190.0f, 200.0f, 0.0f }, CMD_FROM_PLAYER);
	f.frames(2);
	REQUIRE(ai->isMoving());
	Path *path = ai->mover().path();
	REQUIRE(path != nullptr);
	CHECK_FALSE(path->hasExplicitZ());
	CHECK_FALSE(AttackCrushPolicy(*crusher, *victim));

	// 3. Tag every node on the active path with a waypoint id (matching RW 0x5E2DF4:
	// (m_closest->m_nextOpti ?: m_closest)->m_waypointID != NO_WAYPOINT):
	// Now RW 0x69512E (isMoving) && RW 0x69513D (hasExplicitZ) hold, so RW 0x695144 jumps OVER quickDoesPathExist.
	for (PathNode *n = path->getFirstNode(); n != nullptr; n = n->getNext())
	{
		n->setWaypointID(42);
	}
	REQUIRE(path->hasExplicitZ());
	CHECK(AttackCrushPolicy(*crusher, *victim));

	// Verify the bypass also allows NotifyCrushUpdate's scan (RW 0xC76CD8 -> 0x661305 -> 0x69519A) to warn across the cliff.
	NotifyCrushUpdate *nc = Port204World::notifyOf(crusher);
	REQUIRE(nc != nullptr);
	bool warnedWithBypass = false;
	for (int i = 0; i < 6 && !warnedWithBypass; ++i)
	{
		if (Path *cur = ai->mover().path())
		{
			for (PathNode *n = cur->getFirstNode(); n != nullptr; n = n->getNext())
			{
				n->setWaypointID(42);
			}
		}
		f.frames(1);
		warnedWithBypass = nc->warned() > 0 || Port204World::hasEmotion(victim, EMOTION_BRACE_FOR_BEING_CRUSHED);
	}
	CHECK(warnedWithBypass);

	// 4. Stop the crusher (RW 0x664485 false): even if it had a path before, stopping drops the bypass and quickDoesPathExist rejects again.
	ai->aiIdle(CMD_FROM_PLAYER);
	f.frames(1);
	CHECK_FALSE(ai->isMoving());
	CHECK_FALSE(AttackCrushPolicy(*crusher, *victim));
}

TEST_CASE("PORT-204 horde crush warning +0x170: HordeNotifyCrushUpdate includes H+0x170 members in RW 0x8704DA / 0x8D2EC6 order (RW 0x8D3167)")
{
	// Subcase 1: Tie at equal distance from horde centre among members released into H+0x170.
	// RW 0x8704DA appends H+0x170 in ascending ObjectID order (std::set<ObjectID>, RW 0x423EA0),
	// RW 0x8D3131 -> 0x8D2F92 stable-sorts by ascending 2D squared distance from horde centre (RW 0x8D2EC6),
	// and RW 0x8D31F6..0x8D3219 breaks on the FIRST member whose mux() returns true.
	// Release larger ID first to prove order is ascending ObjectID (set order), not release order.
	{
		Port204World f(false, 80, 60);
		Object *horde = f.spawn("Port204CrushHorde", 300.0f, 300.0f, 0.0f, f.teamOf("Alice"));
		HordeContain *hc = f.hordeOf(horde);
		REQUIRE(hc != nullptr);
		NotifyCrushUpdate *nc = Port204World::notifyOf(horde);
		REQUIRE(nc != nullptr);
		std::vector<Object *> ms = f.membersOf(horde);
		REQUIRE(ms.size() == 3);
		// Sort locally by ObjectID to know mLow < mMid < mHigh
		std::sort(ms.begin(), ms.end(), [](const Object *a, const Object *b) { return a->getID() < b->getID(); });
		Object *mLow = ms[0];
		Object *mMid = ms[1];
		Object *mHigh = ms[2];

		// Release mHigh BEFORE mLow into H+0x170, and remove mMid by releasing it too (parked far away, stationary so its mux returns false).
		hc->releaseMemberForGarrison(mHigh);
		hc->releaseMemberForGarrison(mLow);
		hc->releaseMemberForGarrison(mMid);
		REQUIRE(hc->getContainCount() == 0);
		REQUIRE(hc->membersEntering().size() == 3);

		// Place mLow and mHigh at EXACTLY equal 2D squared distance from horde centre (300, 300):
		// mLow at (300, 200) -> dist^2 = 10000; mHigh at (300, 400) -> dist^2 = 10000.
		// Place mMid far away and stationary at (600, 500) -> dist^2 = 130000.
		const Coord3D pLow{ 300.0f, 200.0f, 0.0f };
		const Coord3D pHigh{ 300.0f, 400.0f, 0.0f };
		const Coord3D pMid{ 600.0f, 500.0f, 0.0f };
		mLow->setPosition(&pLow);
		mHigh->setPosition(&pHigh);
		mMid->setPosition(&pMid);

		Object *victimLow = f.spawn("Port204Victim", 360.0f, 200.0f, 0.0f, f.teamOf("Bob"));
		Object *victimHigh = f.spawn("Port204Victim", 360.0f, 400.0f, 0.0f, f.teamOf("Bob"));

		// Move both mLow and mHigh east simultaneously at identical speed.
		f.aiOf(mLow)->aiMoveToPosition(Coord3D{ 500.0f, 200.0f, 0.0f }, CMD_FROM_PLAYER);
		f.aiOf(mHigh)->aiMoveToPosition(Coord3D{ 500.0f, 400.0f, 0.0f }, CMD_FROM_PLAYER);

		// Run until the FIRST crush warning fires. On equal distance, RW 0x8704DA ascending ObjectID order
		// puts mLow before mHigh, so mLow warns victimLow and RW 0x8D3213 breaks before mHigh runs!
		for (int i = 0; i < 12 && nc->warned() == 0; ++i)
		{
			f.frames(1);
		}
		CHECK(nc->scans() == 2u);
		CHECK(nc->warned() == 1u);
		// NotifyCrushUpdate runs in PHASE_NORMAL after EmotionTrackerUpdate's PHASE_INITIAL: verify the exact
		// requested emotion + source member immediately, then advance 1 frame for the tracker to start the nugget.
		EmotionTrackerUpdate *etLow = EmotionTrackerUpdate::of(*victimLow);
		EmotionTrackerUpdate *etHigh = EmotionTrackerUpdate::of(*victimHigh);
		REQUIRE(etLow != nullptr);
		REQUIRE(etHigh != nullptr);
		CHECK(etLow->requested(EMOTION_BRACE_FOR_BEING_CRUSHED));
		CHECK(etLow->requestSource(EMOTION_BRACE_FOR_BEING_CRUSHED) == mLow->getID());
		CHECK_FALSE(etHigh->requested(EMOTION_BRACE_FOR_BEING_CRUSHED));
		f.frames(1);
		CHECK(Port204World::hasEmotion(victimLow, EMOTION_BRACE_FOR_BEING_CRUSHED));
		CHECK_FALSE(Port204World::hasEmotion(victimHigh, EMOTION_BRACE_FOR_BEING_CRUSHED));
	}

	// Subcase 2: Now place mHigh STRICTLY CLOSER to horde centre than mLow (both in H+0x170).
	// RW 0x8D2EC6 sorts mHigh before mLow, so mHigh runs first, warns victimHigh, and breaks before mLow runs.
	{
		Port204World f(false, 80, 60);
		Object *horde = f.spawn("Port204CrushHorde", 300.0f, 300.0f, 0.0f, f.teamOf("Alice"));
		HordeContain *hc = f.hordeOf(horde);
		REQUIRE(hc != nullptr);
		NotifyCrushUpdate *nc = Port204World::notifyOf(horde);
		REQUIRE(nc != nullptr);
		std::vector<Object *> ms = f.membersOf(horde);
		REQUIRE(ms.size() == 3);
		std::sort(ms.begin(), ms.end(), [](const Object *a, const Object *b) { return a->getID() < b->getID(); });
		Object *mLow = ms[0];
		Object *mMid = ms[1];
		Object *mHigh = ms[2];

		hc->releaseMemberForGarrison(mLow);
		hc->releaseMemberForGarrison(mHigh);
		hc->releaseMemberForGarrison(mMid);

		// mHigh at (300, 350) -> dist^2 = 2500; mLow at (300, 150) -> dist^2 = 22500.
		const Coord3D pLow{ 300.0f, 150.0f, 0.0f };
		const Coord3D pHigh{ 300.0f, 350.0f, 0.0f };
		const Coord3D pMid{ 600.0f, 500.0f, 0.0f };
		mLow->setPosition(&pLow);
		mHigh->setPosition(&pHigh);
		mMid->setPosition(&pMid);

		Object *victimLow = f.spawn("Port204Victim", 360.0f, 150.0f, 0.0f, f.teamOf("Bob"));
		Object *victimHigh = f.spawn("Port204Victim", 360.0f, 350.0f, 0.0f, f.teamOf("Bob"));

		f.aiOf(mLow)->aiMoveToPosition(Coord3D{ 500.0f, 150.0f, 0.0f }, CMD_FROM_PLAYER);
		f.aiOf(mHigh)->aiMoveToPosition(Coord3D{ 500.0f, 350.0f, 0.0f }, CMD_FROM_PLAYER);

		for (int i = 0; i < 12 && nc->warned() == 0; ++i)
		{
			f.frames(1);
		}
		CHECK(nc->scans() == 2u);
		CHECK(nc->warned() == 1u);
		EmotionTrackerUpdate *etLow = EmotionTrackerUpdate::of(*victimLow);
		EmotionTrackerUpdate *etHigh = EmotionTrackerUpdate::of(*victimHigh);
		REQUIRE(etLow != nullptr);
		REQUIRE(etHigh != nullptr);
		CHECK(etHigh->requested(EMOTION_BRACE_FOR_BEING_CRUSHED));
		CHECK(etHigh->requestSource(EMOTION_BRACE_FOR_BEING_CRUSHED) == mHigh->getID());
		CHECK_FALSE(etLow->requested(EMOTION_BRACE_FOR_BEING_CRUSHED));
		f.frames(1);
		CHECK(Port204World::hasEmotion(victimHigh, EMOTION_BRACE_FOR_BEING_CRUSHED));
		CHECK_FALSE(Port204World::hasEmotion(victimLow, EMOTION_BRACE_FOR_BEING_CRUSHED));
	}
}

TEST_CASE("PORT-204 horde emotion +0x170: quarrel spectator nearest-neighbour (RW 0x8743A9 -> 0x870C29), member-pass skip (RW 0x874078), and attackedWithin (RW 0x86EE52)")
{
	SUBCASE("quarrel spectator sidesteps a nearby H+0x170 neighbour via RW 0x870C29 during runMemberPass")
	{
		Port204World f(false, 60, 60);
		Object *horde = f.spawn("Port204QuarrelHorde", 300.0f, 300.0f, 0.0f, f.teamOf("Alice"));
		HordeContain *hc = f.hordeOf(horde);
		REQUIRE(hc != nullptr);
		std::vector<Object *> ms = f.membersOf(horde);
		REQUIRE(ms.size() == 4);

		for (size_t i = 0; i < 4; ++i)
		{
			const Coord3D p{ 300.0f + (float)i * 2.0f, 300.0f, 0.0f };
			ms[i]->setPosition(&p);
		}
		f.frames(2);

		HordeContainInterface::ConditionFlags specFlags{};
		HordeContainInterface::ConditionFlags fighterFlags{};
		const int kCelebrating = CombatNames::modelCondition("EMOTION_CELEBRATING");
		specFlags[(size_t)kCelebrating >> 5] |= 1u << (kCelebrating & 31);
		hc->beginQuarrel(50.0f, 50.0f, specFlags, fighterFlags);

		Object *fighterA = f.logic->findObjectByID(hc->quarrelFighter(0));
		Object *fighterB = f.logic->findObjectByID(hc->quarrelFighter(1));
		REQUIRE(fighterA != nullptr);
		REQUIRE(fighterB != nullptr);
		REQUIRE(fighterA != fighterB);

		std::vector<Object *> spectators;
		for (Object *m : ms)
		{
			if (m != fighterA && m != fighterB)
			{
				spectators.push_back(m);
			}
		}
		REQUIRE(spectators.size() == 2);

		// Position fighters within dist^2 = 25 < 410 (kQuarrelNear).
		const Coord3D pB{ 300.0f, 300.0f, 0.0f };
		const Coord3D pA{ 305.0f, 300.0f, 0.0f };
		fighterB->setPosition(&pB);
		fighterA->setPosition(&pA);

		// Spectator `spec` stands at (350, 300) already facing fighterB (-X, angle = pi), at exact quarrel distance 50.
		// Release the other spectator `garr` into H+0x170 and place it at (350, 304) (dist^2 = 16 < 55, on +Y side of S - B).
		// Per RW 0x8743A9 -> 0x870C29 and RW 0x8743D2..0x8744CA, `spec` steps 4 units away from `garr`, snapping to (350, 296).
		Object *spec = spectators[0];
		Object *garr = spectators[1];
		const Coord3D pSpec{ 350.0f, 300.0f, 0.0f };
		const Coord3D pGarr{ 350.0f, 304.0f, 0.0f };
		spec->setPosition(&pSpec);
		spec->setOrientation(3.14159265f);
		garr->setPosition(&pGarr);

		hc->releaseMemberForGarrison(garr);
		REQUIRE(garr->getContainedBy() == nullptr);
		REQUIRE(hc->membersEntering().size() == 1);

		hc->runMemberPass();
		CHECK(spec->testModelCondition(kCelebrating));
		// Without +0x170 in RW 0x870C29, nearest other member is fighterA at dist^2 = 2025 >= 55 -> spec stays at y == 300.
		// With +0x170 in RW 0x870C29, `garr` at (350, 304) triggers the -4 sidestep -> spec snaps to y == 296.
		CHECK(spec->getPosition()->x == 350.0f);
		CHECK(spec->getPosition()->y == 296.0f);
	}

	SUBCASE("runMemberPass skips any member in H+0x170 (RW 0x874078 .. 0x87408B)")
	{
		Port204World f(false, 60, 60);
		Object *horde = f.spawn("Port204QuarrelHorde", 300.0f, 300.0f, 0.0f, f.teamOf("Alice"));
		HordeContain *hc = f.hordeOf(horde);
		REQUIRE(hc != nullptr);
		f.frames(2);
		std::vector<Object *> ms = f.membersOf(horde);
		REQUIRE(ms.size() == 4);

		// Place ms[0] in H+0x170 while also present in m_members (the exact transient state RW 0x874078 guards),
		// move it far off its formation slot, and verify runMemberPass skips it.
		Object *detached = ms[0];
		hc->releaseMemberForGarrison(detached);
		REQUIRE(hc->addToContain(detached));
		REQUIRE(hc->membersEntering().size() == 1);
		REQUIRE(hc->getContainCount() == 4);
		const Coord3D farPos{ 460.0f, 460.0f, 0.0f };
		detached->setPosition(&farPos);
		f.aiOf(detached)->aiIdle(CMD_FROM_PLAYER);

		const unsigned long long ordersBefore = hc->passStats().orders;
		hc->runMemberPass();
		CHECK(hc->passStats().orders == ordersBefore + 3);
		CHECK(detached->getPosition()->x == 460.0f);
		CHECK(detached->getPosition()->y == 460.0f);
		CHECK_FALSE(f.aiOf(detached)->isMoving());
	}

	SUBCASE("attackedWithin scans H+0x170 in ascending ObjectID order (RW 0x86EE52 -> 0x86EECE)")
	{
		Port204World f(false, 60, 60);
		Object *horde = f.spawn("Port204QuarrelHorde", 300.0f, 300.0f, 0.0f, f.teamOf("Alice"));
		HordeContain *hc = f.hordeOf(horde);
		REQUIRE(hc != nullptr);
		f.frames(3); // frame > 1 so attackedWithin(1, ...) passes the RW 0x86EE66 frame guard

		std::vector<Object *> ms = f.membersOf(horde);
		REQUIRE(ms.size() == 4);
		std::sort(ms.begin(), ms.end(), [](const Object *a, const Object *b) { return a->getID() < b->getID(); });
		Object *mLow = ms[0];
		Object *mHigh = ms[1];

		// Release mHigh before mLow into H+0x170.
		hc->releaseMemberForGarrison(mHigh);
		hc->releaseMemberForGarrison(mLow);
		REQUIRE(hc->membersEntering().size() == 2);

		Object *enemyHigh = f.spawn("Port204Victim", 450.0f, 300.0f, 0.0f, f.teamOf("Bob"));
		Object *enemyLow = f.spawn("Port204Victim", 470.0f, 300.0f, 0.0f, f.teamOf("Bob"));

		auto hit = [](Object *victim, Object *by) {
			DamageInfo dmg;
			dmg.m_input.m_sourceID = by->getID();
			dmg.m_input.m_damageType = DAMAGE_UNRESISTABLE;
			dmg.m_input.m_amount = 5.0f;
			dmg.m_input.m_shouldPlayUnderAttackEva = false;
			victim->attemptDamage(dmg);
		};

		hit(mHigh, enemyHigh);
		ObjectID attacker = INVALID_ID;
		CHECK(hc->attackedWithin(1, attacker));
		CHECK(attacker == enemyHigh->getID());

		// Now also hit mLow: RW 0x86EECE walks H+0x170 (std::set<ObjectID>) ascending, so mLow wins over mHigh.
		hit(mLow, enemyLow);
		attacker = INVALID_ID;
		CHECK(hc->attackedWithin(1, attacker));
		CHECK(attacker == enemyLow->getID());
	}
}

TEST_CASE("PORT-204 stops closed: S-1029 (NotifyCrushUpdate) and S-1028 (HordeContain emotion) emit no open stop lines")
{
	CHECK(NotifyCrushUpdate::stopLines().empty());
	CHECK(HordeContain::emotionStops().empty());
}
