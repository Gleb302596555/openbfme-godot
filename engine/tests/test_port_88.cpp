// OpenBFME unit tests: PORT-88 (issue #236) — Unit voice: VoiceMoveOverWalls / VoiceEnterStateMoveOverWalls (RW 0x8DE628, RW 0x8DECBB .. 0x8DED2F). GPL-3.0.
//
// Target facts verified against RotWK 2.01 game.dat (docs/re/rotwk/RW_0x8DE628.md):
//   * moveVoices (RW 0x8DE838 -> RW 0x8DECBB .. 0x8DED2F): after VoiceMoveToCamp / RetreatToCastle, VoiceMoveToHigherGround, and VoiceMoveWhileAttacking,
//     if !done(c) && hasPos (Coord3D != {FLT_MAX,-FLT_MAX,FLT_MAX} at RW 0x8DEA09), check row 5 (VoiceMoveOverWalls) or enter-state row 29
//     (VoiceEnterStateMoveOverWalls) on the drawable (RW 0x676B30) or crowd entry (RW 0x8DDA06); if present and moveRequiresClimbingWalls(obj, pos)
//     (RW 0x8DE628) is true, fillRow(c, obj, row) before falling back to VoiceMove / VoiceEnterStateMove (RW 0x8DED2F).
//   * moveRequiresClimbingWalls (RW 0x8DE628):
//     1. obj's curLocomotor (RW 0x68B31D) must exist and have ScalesWalls (LocomotorTemplate + 0x150) set (RW 0x8DE654).
//     2. Resolve horde via obj.getHordeObject(true) (RW 0x693A1A(1) at RW 0x8DE666); if null, use &obj (RW 0x8DE674).
//     3. Horde must have curLocomotor and AIUpdate (RW 0x8DE67B, 0x8DE6A7).
//     4. Pathfinder::quickDoesPathExist(horde, &horde->pos, &dest, nullptr) (RW 0x8DE69A -> 0x6F5BB0) must be true.
//     5. Rebuild horde's current locomotor set with ScalesWalls forced false (RW 0x5E6027 -> 0x5E80B0) and test quickDoesPathExist(..., &noScale);
//        return true iff that second query returns false (RW 0x8DE7FC .. 0x8DE809).

#include "doctest.h"

#include "MoveTestUtil.h"

#include "Common/Audio/AudioEntryPoints.h"
#include "Common/ModelState.h"
#include "GameClient/SoundSelectorClientBehavior.h"
#include "GameClient/UnitVoiceResponse.h"
#include "GameLogic/ContainParseHooks.h"
#include "GameLogic/GameMessage.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/System/ShroudManager.h"

#include <functional>
#include <memory>
#include <set>
#include <string>
#include <vector>

using namespace movetest;

namespace
{

struct RecordingSink : UnitVoiceResponse::Sink
{
	std::set<std::string> known;
	std::vector<std::string> sounds;
	std::vector<std::string> eva;
	std::vector<ObjectID> owners;

	RecordingSink()
	{
		for (const char *s : {
				 "ClimberMove", "ClimberClimb", "ClimberGo", "ClimberGoClimb", "ClimberBattleMove", "ClimberGoBattleMove",
				 "ClimberHigh", "ClimberGoHigh", "ClimberCamp", "ClimberGoCamp",
				 "NonClimberMove", "NonClimberClimb", "NonClimberGo", "NonClimberGoClimb",
				 "SilentMove", "SilentGo", "PartialMove", "PartialClimb", "PartialGo",
				 "MemberMove", "MemberClimb", "MemberGo", "MemberGoClimb",
				 "CrowdClimb", "CrowdGoClimb", "MountedClimb",
				 "StaticMove", "StaticClimb", "NoLocoMove", "NoLocoClimb",
			 })
		{
			known.insert(s);
		}
	}

	bool soundExists(const std::string &name) override { return known.count(name) != 0; }
	void playSound(const std::string &name, ObjectID object, int) override
	{
		sounds.push_back(name);
		owners.push_back(object);
	}
	void reportEva(const std::string &name, const Coord3D *) override { eva.push_back(name); }
	void clear()
	{
		sounds.clear();
		eva.clear();
		owners.clear();
	}
};

const char kPort88Data[] =
	"Locomotor ScaleWalkerLoco\n"
	"  Surfaces = GROUND\n"
	"  TurnTime = 500\n"
	"  TurnTimeDamaged = 500\n"
	"  Acceleration = 510\n"
	"  Braking = 510\n"
	"  MinSpeed = 0\n"
	"  ZAxisBehavior = NO_Z_MOTIVE_FORCE\n"
	"  Appearance = TWO_LEGS\n"
	"  StickToGround = Yes\n"
	"  CloseEnoughDist = 1\n"
	"  ScalesWalls = Yes\n"
	"End\n"
	"Locomotor ScaleHordeLoco\n"
	"  Surfaces = GROUND\n"
	"  TurnTime = 2000\n"
	"  TurnTimeDamaged = 2000\n"
	"  Acceleration = 500\n"
	"  Braking = 500\n"
	"  ZAxisBehavior = NO_Z_MOTIVE_FORCE\n"
	"  Appearance = HORDE\n"
	"  MaxTurnWithoutReform = 45\n"
	"  TurnWhileMoving = No\n"
	"  WaitForFormation = Yes\n"
	"  FormationPriority = MELEE1\n"
	"  CloseEnoughDist = 1\n"
	"  ScalesWalls = Yes\n"
	"End\n"
	"Object ScaleWall\n"
	"  KindOf = STRUCTURE IMMOBILE SCALEABLE_WALL\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryMinorRadius = 200\n"
	"  GeometryHeight = 30\n"
	"End\n"
	"Object PartialScaleWall\n"
	"  KindOf = STRUCTURE IMMOBILE SCALEABLE_WALL\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryMinorRadius = 60\n"
	"  GeometryHeight = 30\n"
	"End\n"
	"Object PlainWall\n"
	"  KindOf = STRUCTURE IMMOBILE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryMinorRadius = 200\n"
	"  GeometryHeight = 30\n"
	"End\n"
	"Object WallCamp\n"
	"  KindOf = STRUCTURE IMMOBILE\n"
	"  ShroudClearingRange = 40\n"
	"  CampnessValue = 10\n"
	"  CampnessValueRadius = 60\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 5\n"
	"End\n"
	"Object Climber\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  VisionRange = 200\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  MinZIncreaseForVoiceMoveToHigherGround = 10\n"
	"  VoiceMove = ClimberMove\n"
	"  VoiceMoveOverWalls = ClimberClimb\n"
	"  VoiceEnterStateMove = ClimberGo\n"
	"  VoiceEnterStateMoveOverWalls = ClimberGoClimb\n"
	"  VoiceMoveWhileAttacking = ClimberBattleMove\n"
	"  VoiceEnterStateMoveWhileAttacking = ClimberGoBattleMove\n"
	"  VoiceMoveToHigherGround = ClimberHigh\n"
	"  VoiceEnterStateMoveToHigherGround = ClimberGoHigh\n"
	"  VoiceMoveToCamp = ClimberCamp\n"
	"  VoiceEnterStateMoveToCamp = ClimberGoCamp\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = ScaleWalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n"
	"Object NonClimber\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  VoiceMove = NonClimberMove\n"
	"  VoiceMoveOverWalls = NonClimberClimb\n"
	"  VoiceEnterStateMove = NonClimberGo\n"
	"  VoiceEnterStateMoveOverWalls = NonClimberGoClimb\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n"
	"Object SilentClimber\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  VoiceMove = SilentMove\n"
	"  VoiceEnterStateMove = SilentGo\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = ScaleWalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n"
	"Object PartialVoiceClimber\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  VoiceMove = PartialMove\n"
	"  VoiceMoveOverWalls = PartialClimb\n"
	"  VoiceEnterStateMove = PartialGo\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = ScaleWalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n"
	"Object CrowdClimber\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  CrowdResponseKey = ClimbCrowd\n"
	"  VoiceMove = ClimberMove\n"
	"  VoiceMoveOverWalls = ClimberClimb\n"
	"  VoiceEnterStateMove = ClimberGo\n"
	"  VoiceEnterStateMoveOverWalls = ClimberGoClimb\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = ScaleWalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n"
	"Object ClimbMember\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  VoiceMove = MemberMove\n"
	"  VoiceMoveOverWalls = MemberClimb\n"
	"  VoiceEnterStateMove = MemberGo\n"
	"  VoiceEnterStateMoveOverWalls = MemberGoClimb\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = ScaleWalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n"
	"Object ClimbHorde\n"
	"  KindOf = HORDE MELEE_HORDE LARGE_RECTANGLE_PATHFIND SELECTABLE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryMinorRadius = 25\n"
	"  GeometryHeight = 20\n"
	"  Behavior = HordeAIUpdate ModuleTag_AI\n"
	"  End\n"
	"  Behavior = HordeContain ModuleTag_Contain\n"
	"    RankInfo = RankNumber:1 UnitType:ClimbMember Position:X:20 Y:-10 Position:X:20 Y:10\n"
	"    RankInfo = RankNumber:2 UnitType:ClimbMember Position:X:0 Y:-10 Position:X:0 Y:10\n"
	"    InitialPayload = ClimbMember 4\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = ScaleHordeLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 50\n"
	"  End\n"
	"End\n"
	"Object NoScaleHordeWithScaleMembers\n"
	"  KindOf = HORDE MELEE_HORDE LARGE_RECTANGLE_PATHFIND SELECTABLE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryMinorRadius = 25\n"
	"  GeometryHeight = 20\n"
	"  Behavior = HordeAIUpdate ModuleTag_AI\n"
	"  End\n"
	"  Behavior = HordeContain ModuleTag_Contain\n"
	"    RankInfo = RankNumber:1 UnitType:ClimbMember Position:X:20 Y:-10 Position:X:20 Y:10\n"
	"    RankInfo = RankNumber:2 UnitType:ClimbMember Position:X:0 Y:-10 Position:X:0 Y:10\n"
	"    InitialPayload = ClimbMember 4\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = HordeLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 50\n"
	"  End\n"
	"End\n"
	"Object ScaleHordeWithNoScaleMembers\n"
	"  KindOf = HORDE MELEE_HORDE LARGE_RECTANGLE_PATHFIND SELECTABLE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryMinorRadius = 25\n"
	"  GeometryHeight = 20\n"
	"  Behavior = HordeAIUpdate ModuleTag_AI\n"
	"  End\n"
	"  Behavior = HordeContain ModuleTag_Contain\n"
	"    RankInfo = RankNumber:1 UnitType:NonClimber Position:X:20 Y:-10 Position:X:20 Y:10\n"
	"    RankInfo = RankNumber:2 UnitType:NonClimber Position:X:0 Y:-10 Position:X:0 Y:10\n"
	"    InitialPayload = NonClimber 4\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = ScaleHordeLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 50\n"
	"  End\n"
	"End\n"
	"Object StaticVoice\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  VoiceMove = StaticMove\n"
	"  VoiceMoveOverWalls = StaticClimb\n"
	"End\n"
	"Object UnresolvedLocoClimber\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  VoiceMove = NoLocoMove\n"
	"  VoiceMoveOverWalls = NoLocoClimb\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = MissingLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n";

struct VoiceWallWorld
{
	MoveWorld mw;
	RecordingSink sink;
	std::unique_ptr<UnitVoiceResponse> voice;

	explicit VoiceWallWorld(const char *wallTemplate = "ScaleWall")
		: mw(40, 40, kPort88Data)
	{
		if (wallTemplate)
		{
			mw.spawn(wallTemplate, 200.0f, 200.0f);
		}
		mw.buildMap();
		voice = std::make_unique<UnitVoiceResponse>(*mw.logic, sink);
		voice->setLocalPlayerIndex(mw.playerIndex("Alice"));
	}

	std::string say(ObjectID id, int msgType, Coord3D dest, bool hasPos = true)
	{
		sink.clear();
		UnitVoiceResponse::Info info;
		info.hasPosition = hasPos;
		info.position = dest;
		const bool ok = voice->pickAndPlay({ id }, msgType, &info);
		return ok && !sink.sounds.empty() ? sink.sounds.front() : std::string();
	}
};

} // namespace

TEST_CASE("PORT-88 wall-climb voice: ScalesWalls unit says VoiceMoveOverWalls and VoiceEnterStateMoveOverWalls only when a SCALEABLE_WALL path exists (RW 0x8DE628, 0x8DECBB)")
{
	VoiceWallWorld w("ScaleWall");
	const Coord3D left{ 100.0f, 200.0f, 0.0f };
	const Coord3D wallCenter{ 200.0f, 200.0f, 0.0f };
	const Coord3D right{ 300.0f, 200.0f, 0.0f };
	const Coord3D sameSide{ 140.0f, 200.0f, 0.0f };

	const PathfindCell *wallCell = w.mw.ai->pathfinder().getCell(LAYER_GROUND, &wallCenter);
	REQUIRE(wallCell != nullptr);
	REQUIRE(wallCell->getType() == PathfindCell::CELL_OBSTACLE);
	REQUIRE(wallCell->getBit22());

	Object *climber = w.mw.spawn("Climber", left.x, left.y);
	Object *walker = w.mw.spawn("NonClimber", left.x, 120.0f);
	w.mw.frames(2);
	REQUIRE(w.mw.aiOf(climber)->curLocomotor() != nullptr);
	REQUIRE(w.mw.aiOf(climber)->curLocomotor()->getTemplate().m_scalesWalls);
	REQUIRE_FALSE(w.mw.aiOf(walker)->curLocomotor()->getTemplate().m_scalesWalls);

	// Crossing the full SCALEABLE_WALL barrier requires climbing: VoiceMoveOverWalls / VoiceEnterStateMoveOverWalls
	CHECK(w.say(climber->getID(), MSG_DO_MOVETO, right) == "ClimberClimb");
	CHECK(w.say(climber->getID(), MSG_DO_MOVETO_FORMATION, right) == "ClimberClimb");
	CHECK(w.say(climber->getID(), MSG_DO_ATTACKMOVETO, right) == "ClimberClimb");
	CHECK(w.say(climber->getID(), UnitVoiceResponse::VOICE_EVENT_ENTER_STATE_MOVE, right) == "ClimberGoClimb");
	w.mw.frames(1);
	CHECK(w.say(climber->getID(), UnitVoiceResponse::VOICE_EVENT_ENTER_STATE_ATTACKMOVE, right) == "ClimberGoClimb");

	// HUD message routing: GameMessage(MSG_DO_MOVETO) carries destination location
	w.sink.clear();
	GameMessage msg(MSG_DO_MOVETO, w.mw.playerIndex("Alice"));
	msg.appendLocationArgument(right);
	PlayUnitVoiceForMessage(*w.voice, { climber->getID() }, msg);
	CHECK(w.sink.sounds == std::vector<std::string>{ "ClimberClimb" });

	// Without a destination position (RW 0x8DECC6 hasPos gate), enter-state falls back to VoiceEnterStateMove
	w.mw.frames(1);
	CHECK(w.say(climber->getID(), UnitVoiceResponse::VOICE_EVENT_ENTER_STATE_MOVE, right, false) == "ClimberGo");

	// Same side (no wall crossed) and onto the obstacle cell itself (unwalkable destination): VoiceMove
	CHECK(w.say(climber->getID(), MSG_DO_MOVETO, sameSide) == "ClimberMove");
	CHECK(w.say(climber->getID(), MSG_DO_MOVETO, wallCenter) == "ClimberMove");

	// Unit whose locomotor has ScalesWalls = No stays on VoiceMove even if it declares VoiceMoveOverWalls (RW 0x8DE654)
	CHECK(w.say(walker->getID(), MSG_DO_MOVETO, right) == "NonClimberMove");
	CHECK(w.say(walker->getID(), UnitVoiceResponse::VOICE_EVENT_ENTER_STATE_MOVE, right) == "NonClimberGo");

	// Neighbouring partial wall (open ground exists around it): non-climbing path exists, so VoiceMove plays
	{
		VoiceWallWorld partial("PartialScaleWall");
		Object *pc = partial.mw.spawn("Climber", left.x, left.y);
		partial.mw.frames(2);
		CHECK(partial.say(pc->getID(), MSG_DO_MOVETO, right) == "ClimberMove");
		CHECK(partial.say(pc->getID(), UnitVoiceResponse::VOICE_EVENT_ENTER_STATE_MOVE, right) == "ClimberGo");
	}

	// Neighbouring plain structure barrier (no SCALEABLE_WALL -> bit22 false): even ScalesWalls unit cannot cross, so VoiceMove plays
	{
		VoiceWallWorld plain("PlainWall");
		const PathfindCell *plainCell = plain.mw.ai->pathfinder().getCell(LAYER_GROUND, &wallCenter);
		REQUIRE(plainCell != nullptr);
		REQUIRE(plainCell->getType() == PathfindCell::CELL_OBSTACLE);
		REQUIRE_FALSE(plainCell->getBit22());
		Object *plc = plain.mw.spawn("Climber", left.x, left.y);
		plain.mw.frames(2);
		CHECK(plain.say(plc->getID(), MSG_DO_MOVETO, right) == "ClimberMove");
	}
}

TEST_CASE("PORT-88 horde resolution: getHordeObject(true) uses the horde's position and locomotor set (RW 0x8DE662 .. 0x8DE80B)")
{
	VoiceWallWorld w("ScaleWall");
	const Coord3D left{ 100.0f, 200.0f, 0.0f };
	const Coord3D right{ 300.0f, 200.0f, 0.0f };

	Object *horde = w.mw.spawn("ClimbHorde", left.x, left.y);
	w.mw.frames(2);
	const std::vector<Object *> members = w.mw.membersOf(horde);
	REQUIRE(members.size() == 4);

	// Selected horde unwraps to its 4 members; each resolves back to the horde through getHordeObject(true)
	CHECK(w.say(horde->getID(), MSG_DO_MOVETO, right) == "MemberClimb");
	CHECK(w.voice->last().count == 4);
	CHECK(w.say(horde->getID(), UnitVoiceResponse::VOICE_EVENT_ENTER_STATE_MOVE, right) == "MemberGoClimb");
	CHECK(w.voice->last().count == 4);
	// A lone HORDE_MEMBER id is skipped on enter-state (RW 0x8DD63C)
	CHECK(w.say(members.front()->getID(), UnitVoiceResponse::VOICE_EVENT_ENTER_STATE_MOVE, right).empty());

	// Prove the path test runs on the HORDE's position (RW 0x8DE662), not the member's own position:
	// put every member on the right side of the wall while the horde object stays on the left side
	for (Object *m : members)
	{
		m->setPosition(&right);
	}
	CHECK(w.say(horde->getID(), MSG_DO_MOVETO, right) == "MemberClimb");

	// Prove getHordeObject(true)'s producer branch (RW 0x693A1A(1) at RW 0x8DE662): a detached member standing at `right`
	// whose producer is `horde` at `left` still checks from `horde->getPosition()` (`left`) and chooses VoiceMoveOverWalls
	Object *detached = w.mw.spawn("ClimbMember", right.x, right.y);
	w.mw.frames(1);
	detached->setProducer(horde);
	CHECK(detached->getContainedBy() == nullptr);
	CHECK(detached->getHordeObject(true) == horde);
	CHECK(w.say(detached->getID(), MSG_DO_MOVETO, right) == "MemberClimb");

	// A horde whose locomotor has ScalesWalls = No cannot path across the wall even when its members have ScalesWalls = Yes
	Object *noScaleHorde = w.mw.spawn("NoScaleHordeWithScaleMembers", left.x, 80.0f);
	w.mw.frames(2);
	CHECK(w.say(noScaleHorde->getID(), MSG_DO_MOVETO, right) == "MemberMove");

	// A member whose locomotor has ScalesWalls = No fails RW 0x8DE654 even when its horde has ScalesWalls = Yes
	Object *noScaleMembersHorde = w.mw.spawn("ScaleHordeWithNoScaleMembers", left.x, 320.0f);
	w.mw.frames(2);
	CHECK(w.say(noScaleMembersHorde->getID(), MSG_DO_MOVETO, right) == "NonClimberMove");
}

TEST_CASE("PORT-88 rule priority, CrowdResponse and SoundSelectors: higher rules win; missing rows fall back; crowd and selectors work (RW 0x8DECBB .. 0x8DED3D)")
{
	VoiceWallWorld w("ScaleWall");
	ShroudManager sm(*w.mw.logic);
	sm.init(40.0f, 1);
	sm.setExtent(0.0f, 0.0f, 400.0f, 400.0f);
	sm.attach();

	Object *camp = w.mw.spawn("WallCamp", 300.0f, 200.0f);
	Object *climber = w.mw.spawn("Climber", 100.0f, 200.0f);
	Object *silent = w.mw.spawn("SilentClimber", 100.0f, 120.0f);
	Object *partial = w.mw.spawn("PartialVoiceClimber", 100.0f, 160.0f);
	w.mw.frames(2);
	sm.update();
	(void)camp;

	const Coord3D rightCamp{ 300.0f, 200.0f, 0.0f };
	const Coord3D rightNoCamp{ 300.0f, 340.0f, 0.0f };
	const Coord3D rightHigh{ 300.0f, 340.0f, 20.0f };
	const Coord3D rightLowZ{ 300.0f, 340.0f, 5.0f };

	// 1. VoiceMoveToCamp outranks VoiceMoveOverWalls when moving into own camp
	w.voice->setMoveToCampThresholds(5, 10);
	CHECK(w.say(climber->getID(), MSG_DO_MOVETO, rightCamp) == "ClimberCamp");
	CHECK(w.say(climber->getID(), UnitVoiceResponse::VOICE_EVENT_ENTER_STATE_MOVE, rightCamp) == "ClimberGoCamp");
	CHECK(w.say(climber->getID(), MSG_DO_MOVETO, rightNoCamp) == "ClimberClimb");

	// 2. VoiceMoveToHigherGround outranks VoiceMoveOverWalls when dz > MinZIncreaseForVoiceMoveToHigherGround (10)
	CHECK(w.say(climber->getID(), MSG_DO_MOVETO, rightHigh) == "ClimberHigh");
	CHECK(w.say(climber->getID(), UnitVoiceResponse::VOICE_EVENT_ENTER_STATE_MOVE, rightHigh) == "ClimberGoHigh");
	CHECK(w.say(climber->getID(), MSG_DO_MOVETO, rightLowZ) == "ClimberClimb");

	// 3. VoiceMoveWhileAttacking outranks VoiceMoveOverWalls when ENGAGED with an enemy in VisionRange (RW 0x8DEC2A -> 0x8DECBB)
	Object *enemy = w.mw.spawn("Climber", 180.0f, 200.0f, 0.0f, w.mw.teamOf("Bob"));
	(void)enemy;
	const int engaged = ModelCondition::indexOf("ENGAGED");
	REQUIRE(engaged != -1);
	climber->setModelConditionState(engaged, true);
	CHECK(w.say(climber->getID(), MSG_DO_MOVETO, rightNoCamp) == "ClimberBattleMove");
	CHECK(w.say(climber->getID(), UnitVoiceResponse::VOICE_EVENT_ENTER_STATE_MOVE, rightNoCamp) == "ClimberGoBattleMove");
	climber->setModelConditionState(engaged, false);
	CHECK(w.say(climber->getID(), MSG_DO_MOVETO, rightNoCamp) == "ClimberClimb");

	// 4. Units without VoiceMoveOverWalls / VoiceEnterStateMoveOverWalls fall back to VoiceMove / VoiceEnterStateMove
	CHECK(w.say(silent->getID(), MSG_DO_MOVETO, rightNoCamp) == "SilentMove");
	CHECK(w.say(silent->getID(), UnitVoiceResponse::VOICE_EVENT_ENTER_STATE_MOVE, rightNoCamp) == "SilentGo");
	CHECK(w.say(partial->getID(), MSG_DO_MOVETO, rightNoCamp) == "PartialClimb");
	CHECK(w.say(partial->getID(), UnitVoiceResponse::VOICE_EVENT_ENTER_STATE_MOVE, rightNoCamp) == "PartialGo");

	// 5. CrowdResponse VoiceMoveOverWalls / VoiceEnterStateMoveOverWalls plays alongside primary voice across the wall
	REQUIRE(w.voice->parseCrowdResponses(
		"CrowdResponse ClimbCrowd\n"
		"  Threshold 2\n"
		"    VoiceMoveOverWalls = CrowdClimb\n"
		"    VoiceEnterStateMoveOverWalls = CrowdGoClimb\n"
		"  End\n"
		"  Weight = 100\n"
		"End\n",
		nullptr));
	Object *cc1 = w.mw.spawn("CrowdClimber", 90.0f, 200.0f);
	Object *cc2 = w.mw.spawn("CrowdClimber", 110.0f, 200.0f);
	w.mw.frames(1);
	UnitVoiceResponse::Info dest;
	dest.hasPosition = true;
	dest.position = rightNoCamp;
	w.sink.clear();
	CHECK(w.voice->pickAndPlay({ cc1->getID(), cc2->getID() }, MSG_DO_MOVETO, &dest));
	CHECK(w.sink.sounds == (std::vector<std::string>{ "ClimberClimb", "CrowdClimb" }));
	CHECK(w.voice->stats().crowdSounds == 1);
	w.sink.clear();
	CHECK(w.voice->pickAndPlay({ cc1->getID(), cc2->getID() }, UnitVoiceResponse::VOICE_EVENT_ENTER_STATE_MOVE, &dest));
	CHECK(w.sink.sounds == (std::vector<std::string>{ "ClimberGoClimb", "CrowdGoClimb" }));
	CHECK(w.voice->stats().crowdSounds == 2);
	dest.position = Coord3D{ 130.0f, 200.0f, 0.0f };
	w.sink.clear();
	CHECK(w.voice->pickAndPlay({ cc1->getID(), cc2->getID() }, MSG_DO_MOVETO, &dest));
	CHECK(w.sink.sounds == std::vector<std::string>{ "ClimberMove" });
	CHECK(w.voice->stats().crowdSounds == 2);

	// 6. ModelConditionSoundSelectorClientBehavior overrides VoiceMoveOverWalls (RW 0x676B30 -> 0x676A8A)
	{
		VoiceWallWorld sw("ScaleWall");
		SoundSelectorClientBehavior::registerClasses(sw.mw.w.modules);
		const std::function<bool(const std::string &)> savedHook = TheContainParseHooks().audioEventExists;
		struct RestoreHook
		{
			std::function<bool(const std::string &)> prev;
			~RestoreHook() { TheContainParseHooks().audioEventExists = prev; }
		} restore{ savedHook };
		TheContainParseHooks().audioEventExists = [](const std::string &n) { return n == "MountedClimb"; };
		const std::string err = sw.mw.w.load(
			"Object SelectorClimber\n"
			"  KindOf = INFANTRY SELECTABLE\n"
			"  Geometry = CYLINDER\n"
			"  GeometryMajorRadius = 8\n"
			"  GeometryMinorRadius = 8\n"
			"  GeometryHeight = 20\n"
			"  VoiceMove = ClimberMove\n"
			"  VoiceMoveOverWalls = ClimberClimb\n"
			"  Behavior = AIUpdateInterface ModuleTag_AI\n"
			"  End\n"
			"  ClientBehavior = ModelConditionSoundSelectorClientBehavior ModuleTag_MCS\n"
			"    SoundState = MOUNTED\n"
			"      VoiceMoveOverWalls = MountedClimb\n"
			"    End\n"
			"  End\n"
			"  LocomotorSet\n"
			"    Locomotor = ScaleWalkerLoco\n"
			"    Condition = SET_NORMAL\n"
			"    Speed = 55\n"
			"  End\n"
			"End\n",
			INI_LOAD_OVERWRITE, "selector_climber.ini");
		REQUIRE_MESSAGE(err.empty(), err);
		Object *sc = sw.mw.spawn("SelectorClimber", 100.0f, 200.0f);
		sw.mw.frames(1);
		const int mounted = ModelCondition::indexOf("MOUNTED");
		REQUIRE(mounted != -1);
		CHECK(sw.say(sc->getID(), MSG_DO_MOVETO, rightNoCamp) == "ClimberClimb");
		sc->setModelConditionState(mounted, true);
		CHECK(sw.say(sc->getID(), MSG_DO_MOVETO, rightNoCamp) == "MountedClimb");
	}
}

TEST_CASE("PORT-88 guards and S-704 stops: no AI, null locomotor or unready map fall back safely; S-704 omits VoiceMoveOverWalls")
{
	// 1. Unbuilt pathfinder map (!world->mapReady()) falls back to VoiceMove cleanly
	{
		MoveWorld mw(40, 40, kPort88Data);
		REQUIRE_FALSE(mw.ai->mapReady());
		RecordingSink sink;
		UnitVoiceResponse voice(*mw.logic, sink);
		voice.setLocalPlayerIndex(mw.playerIndex("Alice"));
		Object *climber = mw.spawn("Climber", 100.0f, 200.0f);
		UnitVoiceResponse::Info dest;
		dest.hasPosition = true;
		dest.position = Coord3D{ 300.0f, 200.0f, 0.0f };
		CHECK(voice.pickAndPlay({ climber->getID() }, MSG_DO_MOVETO, &dest));
		CHECK(sink.sounds == std::vector<std::string>{ "ClimberMove" });
	}

	// 2. Unit without AIUpdate and unit whose curLocomotor() is null fall back cleanly
	{
		VoiceWallWorld w("ScaleWall");
		Object *noAi = w.mw.spawn("StaticVoice", 100.0f, 200.0f);
		Object *noLoco = w.mw.spawn("UnresolvedLocoClimber", 100.0f, 160.0f);
		w.mw.frames(1);
		REQUIRE(noAi->getAIUpdateInterface() == nullptr);
		REQUIRE(w.mw.aiOf(noLoco) != nullptr);
		REQUIRE(w.mw.aiOf(noLoco)->curLocomotor() == nullptr);
		const Coord3D right{ 300.0f, 200.0f, 0.0f };
		CHECK(w.say(noAi->getID(), MSG_DO_MOVETO, right) == "StaticMove");
		CHECK(w.say(noLoco->getID(), MSG_DO_MOVETO, right) == "NoLocoMove");
	}

	// 3. Acceptance stops preserve the six [S-700]..[S-705] lines and S-704 no longer lists VoiceMoveOverWalls
	const std::vector<std::string> stops = UnitVoiceResponse::acceptanceStops();
	REQUIRE(stops.size() == 6);
	for (int i = 0; i < 6; ++i)
	{
		CHECK(stops[(size_t)i].rfind("[S-70" + std::to_string(i) + "] unit voice:", 0) == 0);
	}
	CHECK(stops[4].find("VoiceMoveOverWalls") == std::string::npos);
}
