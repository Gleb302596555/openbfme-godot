// OpenBFME. PORT-315 (issue #751): RotWK 2.01 AIHuntState (0x11, RW 0x741133) attack priority info (AI + 0x70) and crate pick-up verdict.
//
// Target facts verified in RotWK 2.01 (game.dat):
//   * Attack priority info (AI + 0x70): AIUpdateInterface ctor (RW 0x66F084..0x66F0AD) resolves data + 0x44 ("DefaultAttackPriority" RW 0xC0F220) via
//     ScriptEngine::getAttackInfo (RW 0x605938); AIHuntState::findVictim (RW 0x748089..0x748177) passes AI + 0x70 to TheAI->findClosestEnemy (RW 0x701443),
//     retries without info when Player + 0x35D (PLAYER_HUNT RW 0x6AF4B1) is set, and for teamAttackCommonTarget (prototype + 0x216) returns teamTarget
//     (RW 0x7A3E36) immediately when !info or replaces it only when victimPriority > teamPriority (RW 0x748123..0x748169). Already ported in
//     AIUpdateCombat.cpp / AIHunt.cpp (lanes TARGET-1 + BFME2CAMP1).
//   * Crate pick-up (AI + 0x238 and state 0x27 AIPickUpCrateState ctor RW 0x74458E): AIUpdateInterface::checkForCrateToPickup (RW 0x66810B, DONOR BFME2
//     0x00268B79) clears AI + 0x238 (RW 0x66811B) BEFORE calling GameLogic::findObjectByID(0) (RW 0x449681) and therefore unconditionally returns nullptr;
//     its sole external writer CreateCrateDie::onDie (RW 0x888A24 / RW 0x888B47) has 0 INI uses (S-7211), and every transition to state 0x27 in game.dat
//     (including AIHuntState::update RW 0x74800C -> 0x748023) is gated on RW 0x66810B. Skirmish AI armies collect crates while hunting via
//     PickupStuffUpdate (RW 0x8957B7, S-1422), which keeps AI_HUNT in m_temporaryKeptMachine (AI + 0x34, RW 0x6630A9 / 0x6630D7).
//
// Synthetic INI only, no retail/mod files. Expectations follow the cited RotWK 2.01 addresses.

#include "doctest.h"
#include "CombatTestUtil.h"

#include "Common/INI.h"
#include "Common/Money.h"
#include "Common/Player.h"
#include "Common/Team.h"
#include "GameLogic/AI/AIHunt.h"
#include "GameLogic/Economy.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/PickupStuffUpdate.h"
#include "GameLogic/Module/SalvageCrateCollide.h"
#include "GameLogic/Object/PartitionManager.h"
#include "GameLogic/ScriptEngine/AttackPriority.h"
#include "GameLogic/ScriptEngine/ScriptEngine.h"
#include "GameLogic/SkirmishAI/SkirmishAIData.h"
#include "GameLogic/SkirmishAI/SkirmishAIManager.h"

#include <string>

using namespace combattest;

namespace
{
const char kPort315Objects[] =
	"Object HuntHorseman\n"
	"  KindOf = CAVALRY SELECTABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  ThreatBreakdown\n"
	"    AIKindOf = CAVALRY\n"
	"  End\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = DestroyDie ModuleTag_Destroy\n"
	"  End\n"
	"End\n"
	"Object PickyHunter\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK SCORE\n"
	"  VisionRange = 150\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
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
	"    AutoAcquireEnemiesWhenIdle = No\n"
	"    AttackPriority = HuntPicky\n"
	"  End\n"
	"  Behavior = PhysicsBehavior ModuleTag_Physics\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n"
	"Object PlainPriorityHunter\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK SCORE\n"
	"  VisionRange = 150\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
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
	"    AutoAcquireEnemiesWhenIdle = No\n"
	"    AttackPriority = \"\"\n"
	"  End\n"
	"  Behavior = PhysicsBehavior ModuleTag_Physics\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n"
	"Object HuntChest\n"
	"  KindOf = CRATE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryMinorRadius = 10\n"
	"  GeometryHeight = 10\n"
	"  Behavior = SalvageCrateCollide ModuleTag_Salvage\n"
	"    ForbiddenKindOf = PROJECTILE\n"
	"    LevelUpChance = 0%\n"
	"    ResourceChance = 100%\n"
	"    MinResource = 100\n"
	"    MaxResource = 100\n"
	"    AllowAIPickup = Yes\n"
	"  End\n"
	"End\n"
	"Object HuntMember\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK SCORE\n"
	"  VisionRange = 150\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
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
	"    AutoAcquireEnemiesWhenIdle = Yes\n"
	"    MoodAttackCheckRate = 400\n"
	"  End\n"
	"  Behavior = PhysicsBehavior ModuleTag_Physics\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n"
	"Object HuntHorde\n"
	"  KindOf = HORDE MELEE_HORDE LARGE_RECTANGLE_PATHFIND SELECTABLE CAN_ATTACK\n"
	"  VisionRange = 150\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryMinorRadius = 20\n"
	"  GeometryHeight = 20\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY SwordWeapon\n"
	"  End\n"
	"  Body = ImmortalBody ModuleTag_Body\n"
	"    MaxHealth = 1\n"
	"  End\n"
	"  Behavior = HordeAIUpdate ModuleTag_AI\n"
	"    AutoAcquireEnemiesWhenIdle = Yes\n"
	"    MoodAttackCheckRate = 400\n"
	"  End\n"
	"  Behavior = HordeContain ModuleTag_Contain\n"
	"    RankInfo = RankNumber:1 UnitType:HuntMember Position:X:10 Y:0 Position:X:10 Y:15\n"
	"    InitialPayload = HuntMember 2\n"
	"    MeleeBehavior = Amoeba\n"
	"    End\n"
	"  End\n"
	"  Behavior = PickupStuffUpdate ModuleTag_Pickup\n"
	"    SkirmishAIOnly = Yes\n"
	"    StuffToPickUp = NONE +CRATE\n"
	"    ScanRange = 200\n"
	"    ScanIntervalSeconds = 0.5\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = HordeLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 50\n"
	"  End\n"
	"End\n";

void addPriorityDef(GameLogic &logic, const std::string &name, int defaultPriority, std::vector<std::pair<std::string, int>> targets = {})
{
	GameLogicSettings::AttackPriorityDef def;
	def.name = name;
	def.defaultPriority = defaultPriority;
	def.targets = std::move(targets);
	logic.settings().attackPriorityDefs.insert(logic.settings().attackPriorityDefs.begin(), std::move(def));
}
} // namespace

TEST_CASE("PORT-315: AIHuntState uses AI + 0x70 AttackPriorityInfo (RW 0x66F084 -> RW 0x605938, RW 0x74809D -> RW 0x701443) and reflects overrides")
{
	CombatWorld w(kPort315Objects);
	addPriorityDef(*w.logic, "HuntPicky", 35, { { "HuntHorseman", 39 } });
	addPriorityDef(*w.logic, "PreferDummy", 35, { { "Dummy", 42 } });

	Object *hunter = w.unit("PickyHunter", 'A', 300.0f, 300.0f);
	Object *nearDummy = w.unit("Dummy", 'B', 380.0f, 300.0f);       // distance 80 (score 35 + 100 - 64/80 = 134.2)
	Object *farHorseman = w.unit("HuntHorseman", 'B', 300.0f, 480.0f); // distance 180 (score 39 + 100 - 164/80 = 136.95 > 134.2)
	AIUpdateInterface *ai = hunter->getAIUpdateInterface();
	REQUIRE(ai);
	REQUIRE(ai->attackInfo() == w.logic->scriptEngine().attackPriorities().getAttackInfo("HuntPicky"));

	REQUIRE(ai->aiHunt(CMD_FROM_SCRIPT));
	REQUIRE(ai->currentStateId() == (unsigned)AI_HUNT);
	const AIHuntState *hunt = dynamic_cast<const AIHuntState *>(ai->stateMachine().currentState());
	REQUIRE(hunt);

	// Wait through the initial RW 0x743BF5 scan delay [0..15] frames so AIHuntState::findVictim runs.
	w.frames(16);
	REQUIRE(hunt->huntMachine());
	CHECK(hunt->huntMachine()->goalObject() == farHorseman);

	// Changing AI + 0x70 via setAttackInfo switches the hunt target on rescan (RW 0x74809D reads AI + 0x70 each scan).
	ai->setAttackInfo(w.logic->scriptEngine().attackPriorities().getAttackInfo("PreferDummy"));
	const int switchedFrames = w.runUntil([&] { return hunt->huntMachine() && hunt->huntMachine()->goalObject() == nearDummy; }, 40);
	CHECK(switchedFrames < 40); // tolerance: first rescan happens within kEnemyScanRate (15 frames), and if AIAttackState::onExit clears goalObject on transition, the next rescan within another 15 frames sets nearDummy
	CHECK(hunt->huntMachine()->goalObject() == nearDummy);
}

TEST_CASE("PORT-315: AIHuntState PLAYER_HUNT retries without AttackPriorityInfo when priority 0 rejects all targets (RW 0x7480E5..0x748111)")
{
	CombatWorld w(kPort315Objects);
	w.logic->partition().setRegion(0.0f, 0.0f, 1000.0f, 1000.0f);
	addPriorityDef(*w.logic, "HuntPicky", 0);

	Object *hunter = w.unit("PickyHunter", 'A', 300.0f, 300.0f);
	Object *enemy = w.unit("Dummy", 'B', 500.0f, 300.0f);
	AIUpdateInterface *ai = hunter->getAIUpdateInterface();
	REQUIRE(ai);
	REQUIRE(ai->aiHunt(CMD_FROM_SCRIPT));
	REQUIRE(ai->currentStateId() == (unsigned)AI_HUNT);

	// Without PLAYER_HUNT (+ 0x35D == 0), priority 0 yields nullptr and the hunt finishes into AI_IDLE (RW 0x7481E4..0x74820A).
	w.frames(16);
	CHECK(ai->currentStateId() == (unsigned)AI_IDLE);

	// With PLAYER_HUNT (+ 0x35D == 1, RW 0x6AF4B1), findVictim retries with info = nullptr (RW 0x7480E5) and acquires the enemy.
	w.playerOf('A')->setUnitsShouldHuntFlag(true);
	REQUIRE(ai->aiHunt(CMD_FROM_SCRIPT));
	REQUIRE(ai->currentStateId() == (unsigned)AI_HUNT);
	const AIHuntState *hunt = dynamic_cast<const AIHuntState *>(ai->stateMachine().currentState());
	REQUIRE(hunt);
	const int acquireFrames = w.runUntil([&] { return hunt->huntMachine() && hunt->huntMachine()->goalObject() == enemy; }, 40);
	CHECK(acquireFrames < 40); // tolerance: initial scan delay in AIHuntState::onEnter is logicRandom(0, 15) frames
	REQUIRE(hunt->huntMachine());
	CHECK(hunt->huntMachine()->goalObject() == enemy);
}

TEST_CASE("PORT-315: AIHuntState teamAttackCommonTarget compares AI + 0x70 priorities (RW 0x7480AE, RW 0x748123..0x748169)")
{
	CombatWorld w(kPort315Objects);
	addPriorityDef(*w.logic, "HuntPicky", 35, { { "Dummy", 30 }, { "HuntHorseman", 40 } });

	Player *alice = w.playerOf('A');
	alice->setPlayerType(PLAYER_COMPUTER, true);
	alice->setSkirmishDifficulty(1); // RW 0x79FDEA stores team + 0x114 only when computer difficulty != 0

	Dict d;
	d.setBool("teamAttackCommonTarget", true);
	TeamPrototype *proto = w.teams.initTeam("CommonHuntTeam", alice, true, &d);
	Team *commonTeam = w.teams.createTeam(proto, true);
	REQUIRE(commonTeam);

	Object *enemyLow = w.unit("Dummy", 'B', 380.0f, 300.0f);         // priority 30
	Object *enemyHigh = w.unit("HuntHorseman", 'B', 300.0f, 520.0f); // priority 40

	// (a) !info: an alive teamTarget (team + 0x114) is returned immediately without scanning (RW 0x7480AE).
	{
		Object *plain = w.spawn("PlainPriorityHunter", 300.0f, 300.0f, 0.0f, commonTeam);
		AIUpdateInterface *ai = plain->getAIUpdateInterface();
		REQUIRE(ai);
		ai->setAttackInfo(nullptr);
		CHECK(ai->attackInfo() == nullptr);
		AIHuntState::setTeamTarget(*commonTeam, enemyLow, *w.logic);
		REQUIRE(commonTeam->teamTargetId() == enemyLow->getID());
		REQUIRE(ai->aiHunt(CMD_FROM_SCRIPT));
		const AIHuntState *hunt = dynamic_cast<const AIHuntState *>(ai->stateMachine().currentState());
		REQUIRE(hunt);
		w.frames(16);
		REQUIRE(hunt->huntMachine());
		CHECK(hunt->huntMachine()->goalObject() == enemyLow);
		plain->kill(DAMAGE_UNRESISTABLE, DEATH_NORMAL);
		w.frames(1);
	}

	// (b) With info: teamTarget is kept when victimPriority <= teamPriority, and replaced when victimPriority > teamPriority (RW 0x748123..0x748169).
	{
		Object *picky = w.spawn("PickyHunter", 300.0f, 300.0f, 0.0f, commonTeam);
		AIUpdateInterface *ai = picky->getAIUpdateInterface();
		REQUIRE(ai);
		REQUIRE(ai->attackInfo() != nullptr);

		// Keep higher-priority teamTarget (Horseman = 40) even though Dummy (30) is much closer.
		AIHuntState::setTeamTarget(*commonTeam, enemyHigh, *w.logic);
		ai->setAttackInfo(w.logic->scriptEngine().attackPriorities().getAttackInfo("DefaultAttackPriority")); // both priority 35 -> victimPriority <= teamPriority keeps teamTarget
		REQUIRE(ai->aiHunt(CMD_FROM_SCRIPT));
		const AIHuntState *hunt = dynamic_cast<const AIHuntState *>(ai->stateMachine().currentState());
		REQUIRE(hunt);
		w.frames(16);
		REQUIRE(hunt->huntMachine());
		CHECK(hunt->huntMachine()->goalObject() == enemyHigh);

		// Now seed low-priority Dummy as teamTarget and switch info to HuntPicky (Horseman 40 > Dummy 30): teamTarget upgrades to Horseman.
		AIHuntState::setTeamTarget(*commonTeam, enemyLow, *w.logic);
		ai->setAttackInfo(w.logic->scriptEngine().attackPriorities().getAttackInfo("HuntPicky"));
		w.frames(16);
		CHECK(hunt->huntMachine()->goalObject() == enemyHigh);
		CHECK(commonTeam->teamTargetId() == enemyHigh->getID());
	}
}

TEST_CASE("PORT-315: hunting skirmish AI horde collects SalvageCrateCollide via PickupStuffUpdate (RW 0x8957B7) and resumes AIHuntState; no AI + 0x238 / state 0x27")
{
	CombatWorld w(kPort315Objects);
	SkirmishAIStore aiStore{ w.w.keys };
	{
		INIEnvironment env;
		aiStore.registerBlocks(env.blocks);
		INI ini(env);
		const std::string text = "SkirmishAIData TheSkirmishAIData\nEnd\n";
		ini.loadMemory("skirmishaidata.ini", std::vector<std::uint8_t>(text.begin(), text.end()), INI_LOAD_OVERWRITE);
	}
	w.logic->skirmishAI().setStore(&aiStore);
	Player *bob = w.playerOf('B');
	bob->setPlayerType(PLAYER_COMPUTER, true);
	bob->setSkirmishDifficulty(1);
	REQUIRE(w.logic->skirmishAI().newGame(true) == 1);
	REQUIRE(w.logic->skirmishAI().findAI(bob->getPlayerIndex()) != nullptr);

	Object *horde = w.unit("HuntHorde", 'B', 200.0f, 200.0f);
	Object *chest = w.spawn("HuntChest", 300.0f, 200.0f, 0.0f, w.players.getNeutralPlayer()->getDefaultTeam());
	Object *farEnemy = w.unit("Dummy", 'A', 650.0f, 200.0f);
	REQUIRE(horde);
	REQUIRE(chest);
	REQUIRE(farEnemy);
	const ObjectID chestId = chest->getID();
	const ObjectID enemyId = farEnemy->getID();
	const unsigned cashBefore = bob->getMoney()->countMoney();

	PickupStuffUpdate *pickup = dynamic_cast<PickupStuffUpdate *>(horde->findModule("PickupStuffUpdate"));
	AIUpdateInterface *ai = horde->getAIUpdateInterface();
	REQUIRE(pickup);
	REQUIRE(ai);

	// Enter AI_HUNT (state 0x11, RW 0x6644E6): PickupStuffUpdate keeps AI_HUNT in AI + 0x34 (RW 0x6630A9), collects the crate (RW 0x8C06B6),
	// restores AI_HUNT (RW 0x6630D7), and the horde continues hunting and destroys the distant enemy.
	REQUIRE(ai->aiHunt(CMD_FROM_SCRIPT));
	REQUIRE(ai->currentStateId() == (unsigned)AI_HUNT);

	const int pickupFrames = w.runUntil([&] { return pickup->pickups() > 0; }, 40);
	CHECK(pickupFrames < 40); // tolerance: first scan runs after ScanIntervalSeconds = 0.5 s (3 frames)
	CHECK(ai->hasTemporaryStateMachine());

	const int collectedFrames = w.runUntil([&] {
		const Object *c = w.byId(chestId);
		return !c || c->isDestroyed();
	}, 400);
	CHECK(collectedFrames < 400); // tolerance: horde walks 100 units at speed 50 (about 10 frames) to touch the crate
	CHECK(bob->getMoney()->countMoney() == cashBefore + 100u);

	const int resumedFrames = w.runUntil([&] { return !pickup->pickingUp(); }, 60);
	CHECK(resumedFrames < 60); // tolerance: endTemporaryStateMachine runs on the next idle tick after pickup
	CHECK_FALSE(ai->hasTemporaryStateMachine());
	CHECK(ai->currentStateId() == (unsigned)AI_HUNT);

	const int killedFrames = w.runUntil([&] {
		const Object *e = w.byId(enemyId);
		return !e || e->isDestroyed();
	}, 800);
	CHECK(killedFrames < 800); // tolerance: horde walks ~450 units at speed 50 and kills the 100 HP Dummy
}
