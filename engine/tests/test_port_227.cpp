// OpenBFME unit tests. GPL-3.0.
// PORT-227 (issue #550): Ennorath backlog:
//   * WaitForOthers (RW 0x74B85F, stop S-4463)
//   * ForceSkirmishAI (RW 0x6AA450, AIData + 0x65, stop S-7228)
//   * S-1876 WALL_UPGRADE body rubble (RW 0x695338) and SUMMON_ANTI* heuristics (RW 0x9ECF7F / 0x9ECC0D / 0x9EC8A1 / 0x9EC539, RW 0x7EDAED)
//   * S-417 AITargetHeuristicTechBuilding (RW 0x97CE90)
//   * S-1300 typed ThreatBreakdown AIKindOf (RW 0x73C073 / 0x8ED505)
//   * S-4257 closed (RW 0x7A1129 uses resolved ThingTemplate::isBuildFacility)

#include "doctest.h"

#include "CombatTestUtil.h"

#include "Common/INI.h"
#include "Common/INIException.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/StateHash.h"
#include "Common/Team.h"
#include "Common/Thing/ThingFactory.h"
#include "Common/Thing/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "GameClient/ControlBarCommands.h"
#include "GameLogic/AI/AIFollowPathAsTeam.h"
#include "GameLogic/AI/AIGroup.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Damage.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Map/SidesList.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BehaviorModule.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/SkirmishAI/AIBaseBuilder.h"
#include "GameLogic/SkirmishAI/AIStatCollector.h"
#include "GameLogic/SkirmishAI/AITacticalAI.h"
#include "GameLogic/SkirmishAI/AIThreatFinder.h"
#include "GameLogic/SkirmishAI/AIUpgrade.h"
#include "GameLogic/SkirmishAI/SkirmishAIData.h"
#include "GameLogic/SkirmishAI/SkirmishAIManager.h"

#include <string>
#include <vector>

namespace
{
const char *const kWaitExtra =
	"Locomotor WaitLoco\n"
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
	"  WaitForFormation = Yes\n"
	"End\n"
	"Locomotor NoWaitLoco\n"
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
	"  WaitForFormation = No\n"
	"End\n"
	"Object WaitMarcher\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = PhysicsBehavior ModuleTag_Physics\n"
	"  End\n"
	"  Behavior = DestroyDie ModuleTag_Destroy\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WaitLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 60\n"
	"  End\n"
	"End\n"
	"Object FreeMarcher\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = PhysicsBehavior ModuleTag_Physics\n"
	"  End\n"
	"  Behavior = DestroyDie ModuleTag_Destroy\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = NoWaitLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 60\n"
	"  End\n"
	"End\n";

const char *const kSummonUnits =
	"Object P227Pike\n"
	"  KindOf = INFANTRY PIKE CAN_ATTACK SELECTABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  ThreatLevel = 10\n"
	"  ThreatBreakdown\n"
	"    AIKindOf = PIKEMAN\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"End\n"
	"Object P227Cav\n"
	"  KindOf = CAVALRY CAN_ATTACK SELECTABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  ThreatLevel = 15\n"
	"  ThreatBreakdown\n"
	"    AIKindOf = CAVALRY\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"End\n"
	"Object P227Bow\n"
	"  KindOf = INFANTRY ARCHER CAN_ATTACK SELECTABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  ThreatLevel = 12\n"
	"  ThreatBreakdown\n"
	"    AIKindOf = ARCHER\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"End\n"
	"Object P227Inf\n"
	"  KindOf = INFANTRY CAN_ATTACK SELECTABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  ThreatLevel = 10\n"
	"  ThreatBreakdown\n"
	"    AIKindOf = INFANTRY\n"
	"  End\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY SwordWeapon\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = PhysicsBehavior ModuleTag_Physics\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n"
	"Object P227Monster\n"
	"  KindOf = MONSTER CAN_ATTACK SELECTABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 12\n"
	"  GeometryHeight = 30\n"
	"  ThreatLevel = 30\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 200\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"End\n"
	"Object P227Struct\n"
	"  KindOf = STRUCTURE IMMOBILE SELECTABLE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryMinorRadius = 20\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"End\n"
	"Object P227Econ\n"
	"  KindOf = STRUCTURE IMMOBILE ECONOMY_STRUCTURE SELECTABLE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryMinorRadius = 20\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"End\n"
	"Object P227Flag\n"
	"  KindOf = STRUCTURE IMMOBILE CAPTUREFLAG ECONOMY_STRUCTURE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryMinorRadius = 10\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"End\n"
	"Object P227WallUpgrade\n"
	"  KindOf = STRUCTURE IMMOBILE WALL_UPGRADE SELECTABLE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryMinorRadius = 10\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = ProductionUpdate ModuleTag_Prod\n"
	"    MaxQueueEntries = 4\n"
	"  End\n"
	"End\n"
	"Object P227NormalProd\n"
	"  KindOf = STRUCTURE IMMOBILE SELECTABLE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryMinorRadius = 10\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = ProductionUpdate ModuleTag_Prod\n"
	"    MaxQueueEntries = 4\n"
	"  End\n"
	"End\n";

const char *const kPort227SkirmishData =
	"SkirmishAIData TheSkirmishAIData\n"
	"  DisableWallBuilding = No\n"
	"End\n"
	"ArmyDefinition AlphaArmy\n"
	"  Side = Alpha\n"
	"  TacticalAITargets = OPPORTUNITY\n"
	"  MaxTeamsPerTarget = 1\n"
	"  ArmyMemberDefinition M\n"
	"    Unit = P227Pike\n"
	"    PercentageOfArmyPhase1 = 100\n"
	"  End\n"
	"End\n"
	"ArmyDefinition BetaArmy\n"
	"  Side = Beta\n"
	"  TacticalAITargets = OPPORTUNITY\n"
	"  MaxTeamsPerTarget = 1\n"
	"  ArmyMemberDefinition M\n"
	"    Unit = P227Pike\n"
	"    PercentageOfArmyPhase1 = 100\n"
	"  End\n"
	"End\n"
	"AIDozerAssignment AlphaDozer\n"
	"  Side = Alpha\n"
	"  Unit = AlphaPorter\n"
	"End\n"
	"AIDozerAssignment BetaDozer\n"
	"  Side = Beta\n"
	"  Unit = BetaPorter\n"
	"End\n";

struct SkirmishWorld227 : combattest::CombatWorld
{
	SkirmishAIStore aiStore{ w.keys };

	explicit SkirmishWorld227(const char *extraObjects = kSummonUnits)
		: combattest::CombatWorld(extraObjects, 250)
	{
		aiStore.registerBlocks(w.fx.env.blocks);
		INI ini(w.fx.env);
		const std::string text = kPort227SkirmishData;
		ini.loadMemory("port227_ai.ini", std::vector<std::uint8_t>(text.begin(), text.end()), INI_LOAD_OVERWRITE);
		logic->skirmishAI().setStore(&aiStore);
		REQUIRE(logic->skirmishAI().newGame(true) == 1); // Bob (FactionB) is computer skirmish AI
	}
};

const AIFollowPathAsTeamState *marchState(const Object &o)
{
	const AIUpdateInterface *ai = o.getAIUpdateInterface();
	return ai && ai->stateMachineOrNull() ? dynamic_cast<const AIFollowPathAsTeamState *>(ai->stateMachineOrNull()->currentState()) : nullptr;
}
} // namespace

TEST_CASE("PORT-227 WaitForOthers: WaitForFormation marchers wait at waypoints 0..1 until peers align or suffer recent damage (RW 0x74B85F)")
{
	combattest::CombatWorld w(kWaitExtra, 120);
	w.frames(10); // frame >= 10 avoids AIMover::requestPath's frame < 2 initial delay (AIMove.cpp:287)

	const std::vector<Coord3D> path = {
		{ 150.0f, 200.0f, 0.0f },
		{ 300.0f, 200.0f, 0.0f },
		{ 450.0f, 200.0f, 0.0f },
		{ 600.0f, 200.0f, 0.0f },
	};

	SUBCASE("WaitForOthers = No lets fast unit continue without waiting")
	{
		Object *leader = w.unit("WaitMarcher", 'A', 135.0f, 200.0f);
		Object *laggard = w.unit("WaitMarcher", 'A', 40.0f, 200.0f);
		w.logic->settings().waitForOthers = false;
		leader->getAIUpdateInterface()->setFormationId(1);
		laggard->getAIUpdateInterface()->setFormationId(1);
		leader->getAIUpdateInterface()->aiFollowPathAsTeam(path, nullptr, 0.0f, CMD_FROM_PLAYER);
		laggard->getAIUpdateInterface()->aiFollowPathAsTeam(path, nullptr, 0.0f, CMD_FROM_PLAYER);
		w.runUntil([&]() { return leader->getPosition()->x > 165.0f; }, 40);
		const AIFollowPathAsTeamState *ls = marchState(*leader);
		REQUIRE(ls != nullptr);
		// tolerance: marcher advances past waypoint 0 (x=150) across discrete step frames; exact final coordinate depends on frame count
		CHECK(leader->getPosition()->x > 165.0f);
		CHECK(leader->getAIUpdateInterface()->currentGoalPathIndex() == 1);
		CHECK(laggard->getAIUpdateInterface()->currentGoalPathIndex() == 0);
		CHECK(ls->canMoveToNextFormationPathPoint());
	}

	SUBCASE("WaitForOthers = Yes blocks at waypoint 1 while WaitForFormation peer is at waypoint 0, ignores WaitForFormation = No peer, unblocks on damage and at waypoint > 1")
	{
		// WaitForFormation = No peer does not hold the leader.
		Object *leader = w.unit("WaitMarcher", 'A', 135.0f, 200.0f);
		Object *noWait = w.unit("FreeMarcher", 'A', 40.0f, 220.0f);
		w.logic->settings().waitForOthers = true;
		leader->getAIUpdateInterface()->setFormationId(1);
		noWait->getAIUpdateInterface()->setFormationId(1);
		leader->getAIUpdateInterface()->aiFollowPathAsTeam(path, nullptr, 0.0f, CMD_FROM_PLAYER);
		noWait->getAIUpdateInterface()->aiFollowPathAsTeam(path, nullptr, 0.0f, CMD_FROM_PLAYER);
		w.runUntil([&]() { return leader->getPosition()->x > 165.0f; }, 40);
		const AIFollowPathAsTeamState *ls = marchState(*leader);
		REQUIRE(ls != nullptr);
		// tolerance: marcher ignores WaitForFormation=No peer and advances past waypoint 0 (x=150)
		CHECK(leader->getPosition()->x > 165.0f);
		CHECK(noWait->getAIUpdateInterface()->currentGoalPathIndex() == 0);
		CHECK(ls->canMoveToNextFormationPathPoint());

		// With WaitForFormation = Yes peer still on waypoint 0, leader2 freezes at waypoint 1.
		const std::vector<Coord3D> path2 = {
			{ 150.0f, 280.0f, 0.0f },
			{ 300.0f, 280.0f, 0.0f },
			{ 450.0f, 280.0f, 0.0f },
		};
		Object *leader2 = w.unit("WaitMarcher", 'A', 135.0f, 280.0f);
		Object *laggard = w.unit("WaitMarcher", 'A', 20.0f, 280.0f);
		leader2->getAIUpdateInterface()->setFormationId(2);
		laggard->getAIUpdateInterface()->setFormationId(2);
		leader2->getAIUpdateInterface()->aiFollowPathAsTeam(path2, nullptr, 0.0f, CMD_FROM_PLAYER);
		laggard->getAIUpdateInterface()->aiFollowPathAsTeam(path2, nullptr, 0.0f, CMD_FROM_PLAYER);
		w.runUntil([&]() { return leader2->getAIUpdateInterface()->currentGoalPathIndex() == 1; }, 40);
		const AIFollowPathAsTeamState *ls2 = marchState(*leader2);
		REQUIRE(ls2 != nullptr);
		REQUIRE(leader2->getAIUpdateInterface()->currentGoalPathIndex() == 1);
		CHECK(laggard->getAIUpdateInterface()->currentGoalPathIndex() == 0);
		CHECK_FALSE(ls2->canMoveToNextFormationPathPoint());
		const float heldX = leader2->getPosition()->x;
		w.frames(3);
		CHECK(leader2->getPosition()->x == heldX);
		CHECK(leader2->getAIUpdateInterface()->currentGoalPathIndex() == 1);

		// Recent damage (within 4 seconds) on the laggard unblocks the formation (RW 0x74B8CD -> damagedWithin 4).
		DamageInfo dmg;
		dmg.m_input.m_sourceID = leader2->getID();
		dmg.m_input.m_damageType = DAMAGE_SLASH;
		dmg.m_input.m_amount = 1.0f;
		laggard->attemptDamage(dmg);
		CHECK(AIGroup::damagedWithin(*laggard, 4));
		CHECK(ls2->canMoveToNextFormationPathPoint());
		w.frames(2);
		// tolerance: after 2 frames of resumption from the hold point, x advances past heldX according to locomotor accel/speed
		CHECK(leader2->getPosition()->x > heldX);

		// After 4+ seconds (21 frames at 5 Hz), the damage grace expires and a held leader waits again; waypoint > 1 never waits (RW 0x74B87E).
		const std::vector<Coord3D> path3 = {
			{ 400.0f, 360.0f, 0.0f },
			{ 550.0f, 360.0f, 0.0f },
			{ 700.0f, 360.0f, 0.0f },
		};
		Object *leader3 = w.unit("WaitMarcher", 'A', 100.0f, 360.0f);
		Object *laggard3 = w.unit("WaitMarcher", 'A', 10.0f, 360.0f);
		leader3->getAIUpdateInterface()->setFormationId(3);
		laggard3->getAIUpdateInterface()->setFormationId(3);
		leader3->getAIUpdateInterface()->aiFollowPathAsTeam(path3, nullptr, 0.0f, CMD_FROM_PLAYER);
		laggard3->getAIUpdateInterface()->aiFollowPathAsTeam(path3, nullptr, 0.0f, CMD_FROM_PLAYER);
		laggard3->attemptDamage(dmg);
		CHECK(AIGroup::damagedWithin(*laggard3, 4));
		w.frames(21);
		CHECK_FALSE(AIGroup::damagedWithin(*laggard3, 4));
		w.runUntil([&]() { return leader3->getAIUpdateInterface()->currentGoalPathIndex() == 1; }, 40);
		const AIFollowPathAsTeamState *ls3 = marchState(*leader3);
		REQUIRE(ls3 != nullptr);
		REQUIRE(leader3->getAIUpdateInterface()->currentGoalPathIndex() == 1);
		CHECK(laggard3->getAIUpdateInterface()->currentGoalPathIndex() == 0);
		CHECK_FALSE(ls3->canMoveToNextFormationPathPoint());

		leader3->getAIUpdateInterface()->setCurrentGoalPathIndex(2);
		CHECK(ls3->canMoveToNextFormationPathPoint());
	}
}

TEST_CASE("PORT-227 ForceSkirmishAI: AIData parses ForceSkirmishAI, hashes conditionally, and upgrades non-skirmish COMPUTER players (RW 0x6AA450)")
{
	GameLogicSettings def;
	CHECK_FALSE(def.forceSkirmishAI);

	GameLogicSettings parsedNo;
	std::string err;
	REQUIRE_MESSAGE(GameLogicSettingsLoader::scanAIData("AIData\n  ForceSkirmishAI = No\nEnd\n", parsedNo, &err), err);
	CHECK_FALSE(parsedNo.forceSkirmishAI);

	GameLogicSettings parsedYes;
	REQUIRE_MESSAGE(GameLogicSettingsLoader::scanAIData("AIData\n  ForceSkirmishAI = Yes\nEnd\n", parsedYes, &err), err);
	CHECK(parsedYes.forceSkirmishAI);

	StateHasher hDef, hNo, hYes;
	def.crc(hDef);
	parsedNo.crc(hNo);
	parsedYes.crc(hYes);
	CHECK(hDef.value() == hNo.value());
	CHECK(hYes.value() != hNo.value());

	// Player::setPlayerType(PLAYER_COMPUTER, false, forceSkirmishAI): upgrades only COMPUTER players.
	logictest::LogicWorld lw;
	Player *alice = lw.players.findPlayerWithName("Alice");
	REQUIRE(alice != nullptr);
	alice->setPlayerType(PLAYER_HUMAN, false, true);
	CHECK_FALSE(alice->isSkirmishAI());
	alice->setPlayerType(PLAYER_COMPUTER, false, false);
	CHECK_FALSE(alice->isSkirmishAI());
	alice->setPlayerType(PLAYER_COMPUTER, false, true);
	CHECK(alice->isSkirmishAI());

	// PlayerList::newGame honors setForceSkirmishAI for map sides whose playerIsSkirmish is false.
	auto buildSides = []() {
		SidesList sides;
		auto makeSide = [](const std::string &name, const std::string &faction, bool human, bool skirmish) {
			SidesInfo si;
			si.dict.setAsciiString("playerName", name);
			si.dict.setAsciiString("playerFaction", faction);
			si.dict.setBool("playerIsHuman", human);
			si.dict.setBool("playerIsSkirmish", skirmish);
			si.dict.setInt("multiplayerStartIndex", 0);
			return si;
		};
		sides.sides.push_back(makeSide("HumanSide", "FactionA", true, false));
		sides.sides.push_back(makeSide("SoloAISide", "FactionB", false, false));
		Dict t1;
		t1.setAsciiString("teamName", "teamHumanSide");
		t1.setAsciiString("teamOwner", "HumanSide");
		t1.setBool("teamIsSingleton", true);
		sides.teams.push_back(t1);
		Dict t2;
		t2.setAsciiString("teamName", "teamSoloAISide");
		t2.setAsciiString("teamOwner", "SoloAISide");
		t2.setBool("teamIsSingleton", true);
		sides.teams.push_back(t2);
		return sides;
	};

	TeamFactory tfNo;
	PlayerList plNo(lw.w.keys, lw.templates, tfNo);
	plNo.setForceSkirmishAI(false);
	REQUIRE(plNo.newGame(buildSides(), 1000).empty());
	CHECK_FALSE(plNo.findPlayerWithName("HumanSide")->isSkirmishAI());
	CHECK_FALSE(plNo.findPlayerWithName("SoloAISide")->isSkirmishAI());

	TeamFactory tfYes;
	PlayerList plYes(lw.w.keys, lw.templates, tfYes);
	plYes.setForceSkirmishAI(true);
	REQUIRE(plYes.newGame(buildSides(), 1000).empty());
	CHECK_FALSE(plYes.findPlayerWithName("HumanSide")->isSkirmishAI());
	CHECK(plYes.findPlayerWithName("SoloAISide")->isSkirmishAI());
}

TEST_CASE("PORT-227 S-1300: ThreatBreakdown AIKindOf is typed-parsed into ThingTemplate, copied, overridden, and validates case-sensitively (RW 0x73C073 / 0x8ED505)")
{
	objtest::World w;
	const std::string err = w.load(
		"Object TbNone\n"
		"  KindOf = INFANTRY\n"
		"End\n"
		"Object TbCav\n"
		"  KindOf = CAVALRY\n"
		"  ThreatBreakdown\n"
		"    AIKindOf = CAVALRY\n"
		"  End\n"
		"End\n"
		"ChildObject TbChild TbCav\n"
		"End\n"
		"ChildObject TbChildOverride TbCav\n"
		"  ThreatBreakdown\n"
		"    AIKindOf = PIKEMAN\n"
		"  End\n"
		"End\n",
		INI_LOAD_OVERWRITE, "tb_valid.ini");
	REQUIRE_MESSAGE(err.empty(), err);
	const ThingTemplate *tNone = w.things.findTemplate("TbNone");
	const ThingTemplate *tCav = w.things.findTemplate("TbCav");
	const ThingTemplate *tChild = w.things.findTemplate("TbChild");
	const ThingTemplate *tChildOv = w.things.findTemplate("TbChildOverride");
	REQUIRE(tNone != nullptr);
	REQUIRE(tCav != nullptr);
	REQUIRE(tChild != nullptr);
	REQUIRE(tChildOv != nullptr);
	CHECK(tNone->threatCategory() == -1);
	CHECK(tCav->threatCategory() == 3);
	CHECK(tChild->threatCategory() == 3);
	CHECK(tChildOv->threatCategory() == 2);
	CHECK(AIThreatFinder::threatCategory(*tCav) == 3);
	CHECK(AIThreatFinder::threatCategory(*tChildOv) == 2);
	CHECK(tCav->rawBlocks().size() == 1);

	// Unknown or wrong-case AIKindOf raises at load time (RW 0x8ED505).
	CHECK(w.load("Object TbBad\n  ThreatBreakdown\n    AIKindOf = NOT_A_KIND\n  End\nEnd\n").find("invalid AI_KINDOF") != std::string::npos);
	CHECK(w.load("Object TbLower\n  ThreatBreakdown\n    AIKindOf = cavalry\n  End\nEnd\n").find("invalid AI_KINDOF") != std::string::npos);
}

TEST_CASE("PORT-227 S-1876: WALL_UPGRADE legality refuses BODY_RUBBLE (RW 0x695338) and SUMMON_ANTI* + dominantCategory evaluate retail rules (RW 0x7EDAED, 0x9ECF7F..0x9EC539)")
{
	SUBCASE("dominantCategory returns 0 when empty, keeps highest positive category, and breaks ties to lower index (RW 0x7EDAED)")
	{
		AIThreatFinder::ThreatRecord empty{};
		CHECK(AIThreatFinder::dominantCategory(empty) == 0);

		AIThreatFinder::ThreatRecord tied{};
		tied.v[0] = 50.0f;
		tied.v[1 + 1] = 25.0f; // ARCHER
		tied.v[1 + 3] = 25.0f; // CAVALRY (same value: strict > keeps lower index 1)
		CHECK(AIThreatFinder::dominantCategory(tied) == 1);

		AIThreatFinder::ThreatRecord cav{};
		cav.v[0] = 50.0f;
		cav.v[1 + 1] = 20.0f;
		cav.v[1 + 3] = 30.0f;
		CHECK(AIThreatFinder::dominantCategory(cav) == 3);
	}

	SUBCASE("WALL_UPGRADE BODY_RUBBLE legality check (RW 0x695338)")
	{
		SkirmishWorld227 w;
		Object *wall = w.unit("P227WallUpgrade", 'B', 100.0f, 100.0f);
		Object *normal = w.unit("P227NormalProd", 'B', 200.0f, 100.0f);
		CommandButton btn;
		btn.m_name = "Cmd_WallTest";
		btn.m_command = GUI_COMMAND_PLAYER_UPGRADE;
		btn.m_upgradeName = "Upgrade_Basic";

		CHECK(AIUpgrade::legality(*w.logic, *wall, &btn) == 0);
		CHECK(AIUpgrade::legality(*w.logic, *normal, &btn) == 0);

		DamageInfo rubble;
		rubble.m_input.m_damageType = DAMAGE_UNRESISTABLE;
		rubble.m_input.m_amount = 200.0f;
		wall->attemptDamage(rubble);
		normal->attemptDamage(rubble);
		REQUIRE(wall->getBodyModule()->getDamageState() == BODY_RUBBLE);
		REQUIRE(normal->getBodyModule()->getDamageState() == BODY_RUBBLE);
		CHECK(AIUpgrade::legality(*w.logic, *wall, &btn) == 9);
		CHECK(AIUpgrade::legality(*w.logic, *normal, &btn) == 0);
	}

	SUBCASE("SUMMON_ANTI* heuristics (4..7) in Rush, victim, attackedAt, nearby enemy, ANTICAVALRY team target, and fallback draw")
	{
		SkirmishWorld227 w;
		w.frames(12);
		Player *bob = w.playerOf('B');
		AISkirmishPlayer *ai = w.logic->skirmishAI().findAI(bob->getPlayerIndex());
		REQUIRE(ai != nullptr);

		// Rush (phase 0): empty army -> 2 for all four; 1 counter unit (ratio 1.0 >= 0.25) -> 3, 4 other units (ratio 1/5 = 0.2 < 0.25) -> 2.
		ai->build.phase = 0;
		Object *emptyProd = w.unit("P227Struct", 'B', 400.0f, 500.0f);
		w.frames(1);
		AIBuildRequest emptyReq;
		emptyReq.producer = emptyProd->getID();
		for (int h : { AIUpgrade::HEURISTIC_SUMMON_ANTICAVALRY, AIUpgrade::HEURISTIC_SUMMON_ANTIARCHER,
					   AIUpgrade::HEURISTIC_SUMMON_ANTISPECIAL, AIUpgrade::HEURISTIC_SUMMON_ANTIINFANTRY })
		{
			emptyReq.heuristicType = h;
			CHECK(AIUpgrade::evaluate(*w.logic, emptyReq, *bob) == 2); // empty army() -> 2
		}

		Object *prod = w.unit("P227Inf", 'B', 500.0f, 500.0f);
		w.frames(1);

		// Missing producer / HORDE_MEMBER / NO_ATTACK_FROM_AI -> 3.
		AIBuildRequest noProd;
		noProd.heuristicType = AIUpgrade::HEURISTIC_SUMMON_ANTICAVALRY;
		CHECK(AIUpgrade::evaluate(*w.logic, noProd, *bob) == 3);

		AIBuildRequest req;
		req.producer = prod->getID();

		prod->setStatus((unsigned)CombatNames::statuses().hordeMember, true);
		req.heuristicType = AIUpgrade::HEURISTIC_SUMMON_ANTIARCHER;
		CHECK(AIUpgrade::evaluate(*w.logic, req, *bob) == 3);
		prod->setStatus((unsigned)CombatNames::statuses().hordeMember, false);

		prod->setStatus((unsigned)CombatNames::statuses().noAttackFromAI, true);
		req.heuristicType = AIUpgrade::HEURISTIC_SUMMON_ANTISPECIAL;
		CHECK(AIUpgrade::evaluate(*w.logic, req, *bob) == 3);
		prod->setStatus((unsigned)CombatNames::statuses().noAttackFromAI, false);

		req.heuristicType = AIUpgrade::HEURISTIC_SUMMON_ANTISPECIAL;
		CHECK(AIUpgrade::evaluate(*w.logic, req, *bob) == 3); // 1 INFANTRY / 1 army = 1.0 >= 0.25
		req.heuristicType = AIUpgrade::HEURISTIC_SUMMON_ANTIARCHER;
		CHECK(AIUpgrade::evaluate(*w.logic, req, *bob) == 2); // 0 CAVALRY / 1 army = 0 < 0.25

		Object *pikeOwn = w.unit("P227Pike", 'B', 100.0f, 100.0f);
		w.frames(1);
		(void)pikeOwn;
		req.heuristicType = AIUpgrade::HEURISTIC_SUMMON_ANTICAVALRY;
		CHECK(AIUpgrade::evaluate(*w.logic, req, *bob) == 3); // 1 PIKE / 2 army = 0.5 >= 0.25
		for (int i = 0; i < 3; ++i)
		{
			w.unit("P227Cav", 'B', 120.0f + 20.0f * (float)i, 100.0f);
		}
		w.frames(1);
		CHECK(AIUpgrade::evaluate(*w.logic, req, *bob) == 2); // 1 PIKE / 5 army = 0.2 < 0.25

		// Phase > 0: currentVictim check (non-STRUCTURE counter -> 2).
		ai->build.phase = 1;
		Object *enemyCav = w.unit("P227Cav", 'A', 540.0f, 500.0f);
		REQUIRE(prod->getAIUpdateInterface()->aiAttackObject(enemyCav, CMD_FROM_PLAYER, 1));
		CHECK(AIUpgrade::evaluate(*w.logic, req, *bob) == 2);

		// STRUCTURE victim is ignored; without nearby enemy or attackedAt, falls through to RNG draw (lines 0xA1 / 0x97 / 0x98 / 0x97).
		Object *enemyStruct = w.unit("P227Struct", 'A', 1200.0f, 1200.0f);
		w.logic->destroyObject(enemyCav);
		w.logic->processDestroyList();
		REQUIRE(prod->getAIUpdateInterface()->aiAttackObject(enemyStruct, CMD_FROM_PLAYER, 1));

		w.logic->random().enableCallLog(true);
		w.logic->random().clearCallLog();
		req.heuristicType = AIUpgrade::HEURISTIC_SUMMON_ANTICAVALRY;
		CHECK(AIUpgrade::evaluate(*w.logic, req, *bob) == 3);
		REQUIRE(w.logic->random().callLog().size() == 1);
		CHECK(w.logic->random().callLog()[0].file == "AIUpgradeHeuristicSummonAntiCavalry.cpp");
		CHECK(w.logic->random().callLog()[0].line == 0xA1);
		w.logic->random().clearCallLog();
		req.heuristicType = AIUpgrade::HEURISTIC_SUMMON_ANTIARCHER;
		CHECK(AIUpgrade::evaluate(*w.logic, req, *bob) == 3);
		REQUIRE(w.logic->random().callLog().size() == 1);
		CHECK(w.logic->random().callLog()[0].file == "AIUpgradeHeuristicSummonAntiArcher.cpp");
		CHECK(w.logic->random().callLog()[0].line == 0x97);
		w.logic->random().clearCallLog();
		req.heuristicType = AIUpgrade::HEURISTIC_SUMMON_ANTISPECIAL;
		CHECK(AIUpgrade::evaluate(*w.logic, req, *bob) == 3);
		REQUIRE(w.logic->random().callLog().size() == 1);
		CHECK(w.logic->random().callLog()[0].file == "AIUpgradeHeuristicSummonAntiSpecial.cpp");
		CHECK(w.logic->random().callLog()[0].line == 0x98);
		w.logic->random().clearCallLog();
		req.heuristicType = AIUpgrade::HEURISTIC_SUMMON_ANTIINFANTRY;
		CHECK(AIUpgrade::evaluate(*w.logic, req, *bob) == 3);
		REQUIRE(w.logic->random().callLog().size() == 1);
		CHECK(w.logic->random().callLog()[0].file == "AIUpgradeHeuristicSummonAntiInfantry.cpp");
		CHECK(w.logic->random().callLog()[0].line == 0x97);
		w.logic->random().enableCallLog(false);

		// Nearby enemy within 400: ANTIARCHER accepts MONSTER; ANTISPECIAL accepts PIKE; ANTIINFANTRY accepts INFANTRY.
		prod->getAIUpdateInterface()->aiIdle(CMD_FROM_PLAYER);
		Object *nearMonster = w.unit("P227Monster", 'A', 650.0f, 500.0f);
		req.heuristicType = AIUpgrade::HEURISTIC_SUMMON_ANTIARCHER;
		CHECK(AIUpgrade::evaluate(*w.logic, req, *bob) == 2);
		w.logic->destroyObject(nearMonster);
		w.logic->processDestroyList();

		Object *nearPike = w.unit("P227Pike", 'A', 650.0f, 500.0f);
		req.heuristicType = AIUpgrade::HEURISTIC_SUMMON_ANTISPECIAL;
		CHECK(AIUpgrade::evaluate(*w.logic, req, *bob) == 2);

		// attackedAt within 600 short-circuits without falling through to nearby enemy.
		AIAttackedAt rec;
		rec.position = Coord3D{ 520.0f, 500.0f, 0.0f };
		rec.enemies.v[0] = 50.0f;
		rec.enemies.v[1 + 0] = 50.0f; // INFANTRY dominantCategory = 0
		ai->brain.attackedAt.push_back(rec);
		req.heuristicType = AIUpgrade::HEURISTIC_SUMMON_ANTIINFANTRY;
		CHECK(AIUpgrade::evaluate(*w.logic, req, *bob) == 2);
		req.heuristicType = AIUpgrade::HEURISTIC_SUMMON_ANTISPECIAL; // expects PIKEMAN (2), dominant is 0 -> returns 3 despite nearPike!
		CHECK(AIUpgrade::evaluate(*w.logic, req, *bob) == 3);

		ai->brain.attackedAt.clear();
		w.logic->destroyObject(nearPike);
		w.logic->processDestroyList();

		// ANTICAVALRY team target STRUCTURE + victim within 600 -> 2.
		Object *farEnemyInf = w.unit("P227Inf", 'A', 950.0f, 500.0f); // 450 from prod (> 400 PartitionManager, < 600 teamTarget victim)
		prod->getTeam()->setTeamTargetId(enemyStruct->getID());
		REQUIRE(prod->getAIUpdateInterface()->aiAttackObject(farEnemyInf, CMD_FROM_PLAYER, 1));
		req.heuristicType = AIUpgrade::HEURISTIC_SUMMON_ANTICAVALRY;
		CHECK(AIUpgrade::evaluate(*w.logic, req, *bob) == 2);
	}
}

TEST_CASE("PORT-227 S-417: AITargetHeuristicTechBuilding picks closest captured non-allied non-CAPTUREFLAG tech building within 1000 (RW 0x97CE90)")
{
	SkirmishWorld227 w;
	w.frames(12);
	Player *bob = w.playerOf('B');
	AISkirmishPlayer *ai = w.logic->skirmishAI().findAI(bob->getPlayerIndex());
	REQUIRE(ai != nullptr);
	ai->build.startPosition = Coord3D{ 100.0f, 100.0f, 0.0f };
	ai->build.phase = 1;

	// Candidates registered in captureFlags():
	Object *alliedCaptured = w.unit("P227Econ", 'B', 120.0f, 100.0f);       // ALLIES -> skipped
	Object *uncapturedFlag = w.unit("P227Flag", 'A', 130.0f, 100.0f);       // still has CAPTUREFLAG -> skipped
	Object *tooFarEcon = w.unit("P227Econ", 'A', 1200.0f, 100.0f);          // distance 1100 > 1000 -> skipped
	Object *nearNonEcon = w.unit("P227Struct", 'A', 140.0f, 100.0f);        // non-ECONOMY_STRUCTURE -> skipped only while attackedAt non-empty
	Object *nearEcon = w.unit("P227Econ", 'A', 400.0f, 100.0f);             // distance 300 <= 1000 -> wins while attackedAt non-empty

	for (Object *o : { alliedCaptured, uncapturedFlag, tooFarEcon, nearNonEcon, nearEcon })
	{
		w.logic->skirmishAI().addCaptureFlagForTest(o->getID());
	}

	AIAttackedAt atk;
	atk.position = ai->build.startPosition;
	ai->brain.attackedAt.push_back(atk);

	AITarget t;
	AITacticalAI::techBuildingHeuristicForTest(*w.logic, *ai, t);
	CHECK(t.object == nearEcon->getID());
	CHECK(ai->brain.unportedTypes == 0);

	// With empty attackedAt, non-ECONOMY_STRUCTURE captured flags are eligible too (RW 0x97CF3F).
	ai->brain.attackedAt.clear();
	AITarget t2;
	AITacticalAI::techBuildingHeuristicForTest(*w.logic, *ai, t2);
	CHECK(t2.object == nearNonEcon->getID());
}

TEST_CASE("PORT-227 S-4257: acquireEnemy emits no S-4257 stop")
{
	SkirmishWorld227 w;
	w.frames(12);
	w.unit("P227Struct", 'A', 100.0f, 100.0f);
	w.unit("P227Inf", 'A', 120.0f, 100.0f);
	Player *bob = w.playerOf('B');
	AISkirmishPlayer *ai = w.logic->skirmishAI().findAI(bob->getPlayerIndex());
	REQUIRE(ai != nullptr);
	(void)AITacticalAI::enemyPlayer(*w.logic, *ai);
	bool hasS4257 = false;
	for (const std::string &line : w.logic->report().stops)
	{
		hasS4257 = hasS4257 || line.rfind("[S-4257]", 0) == 0;
	}
	CHECK_FALSE(hasS4257);
}
