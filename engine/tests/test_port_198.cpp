// OpenBFME. PORT-198 tests (issue #465, stop S-7444):
//   1. SpawnBehaviorInterface / SpawnBehavior / SpawnBehaviorUnion (RW vtable 0xC58D94) slots 2..6 and 10:
//      getClosestSlave (0x86292C), orderSlavesToAttackTarget (0x8637BE), orderSlavesToAttackPosition (0x863807),
//      getCanAnySlavesAttackSpecificTarget (0x862990), getCanAnySlavesUseWeaponAgainstTarget (0x8629ED), reEnslaveSlaves (0x862E4F).
//   2. RW 0x68C3A3 callers: GettingBuiltBehavior::finishConstruction (0x8569C7 -> slot 10), AIGroup::groupIdle (0x77205F -> slot 8),
//      AIGroup::groupAttackPosition (0x772369 -> slot 4), AIUpdate::privateAttackObject (0x66BEC3 -> slot 3),
//      GarrisonRules::canAttackAsNewTarget (0x82CFD4 -> slot 2 + shroud gate, no false STRUCTURE passenger error without Contain),
//      CommandTranslator::objectCanAttack / objectCanDamage / evaluateForceAttack (0x81D9DE).
//   3. ObjectWeapons::getAbleToUseWeaponAgainstTarget (RW 0x6C8652), Object::getAbleToAttackSpecificObject (0x68D6A6),
//      Object::getAbleToUseWeaponAgainstTarget (0x68B5FA): immobile aim delta, Speed = 0 locomotor, HORDE vs non-horde contain,
//      CONTESTING_BUILDING without victim, horde containment delegation, and SPAWNS_ARE_THE_WEAPONS fallback.
//   4. Player::onUnitCreated (RW 0x6AA688) -> Economy::onUnitCreated -> SkirmishAIManager::unitCreated (RW 0x6C77C9)
//      -> AIBaseBuilder::onStructureBuilt(..., false) (RW 0x8F06FF) from ProductionUpdate::completeUnit, SpawnBehavior::createSpawn,
//      and StartingBase placement.
// Synthetic data only: no retail or mod files.

#include "doctest.h"
#include "CombatTestUtil.h"
#include "ProdTestUtil.h"

#include "Common/BuildAssistant.h"
#include "Common/INI.h"
#include "Common/ModEngineFeatures.h"
#include "Common/Player.h"
#include "Common/PlayerEconomyHost.h"
#include "Common/PlayerTemplate.h"
#include "Common/Team.h"
#include "GameClient/MapChunks.h"
#include "GameClient/MessageStream/CommandXlat.h"
#include "GameLogic/Map/TerrainLogic.h"
#include "GameLogic/AI/AIGarrisonStates.h"
#include "GameLogic/AI/AIGroup.h"
#include "GameLogic/AI/AIStateMachine.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Combat/ObjectWeapons.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/ActiveBody.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/ConstructionModules.h"
#include "GameLogic/Module/ProductionUpdate.h"
#include "GameLogic/Module/SlavedUpdate.h"
#include "GameLogic/Module/SpawnBehavior.h"
#include "GameLogic/NewGame/StartingBase.h"
#include "GameLogic/Object/Contain/HordeContainRuntime.h"
#include "GameLogic/ObjectTemplateInfo.h"
#include "GameLogic/SkirmishAI/AIBaseBuilder.h"
#include "GameLogic/SkirmishAI/SkirmishAIData.h"
#include "GameLogic/SkirmishAI/SkirmishAIManager.h"
#include "GameLogic/System/ShroudManager.h"

#include <memory>
#include <string>
#include <vector>

using namespace combattest;

namespace
{
const char kPort198Objects[] =
	"Weapon P198SlaveBow\n"
	"  AttackRange = 200\n"
	"  WeaponSpeed = 100\n"
	"  DelayBetweenShots = 400\n"
	"  NoVictimNeeded = Yes\n"
	"  ProjectileNugget\n"
	"    ProjectileTemplateName = Arrow\n"
	"    WarheadTemplateName = BowWarhead\n"
	"  End\n"
	"End\n"
	"Weapon P198AimBow\n"
	"  AttackRange = 200\n"
	"  WeaponSpeed = 100\n"
	"  DelayBetweenShots = 400\n"
	"  AcceptableAimDelta = 15\n"
	"  ProjectileNugget\n"
	"    ProjectileTemplateName = Arrow\n"
	"    WarheadTemplateName = BowWarhead\n"
	"  End\n"
	"End\n"
	"Object P198ImmobileSlave\n"
	"  KindOf = INFANTRY ARCHER CAN_ATTACK\n"
	"  VisionRange = 250\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 5\n"
	"  GeometryHeight = 15\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY P198SlaveBow\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 200\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"    AutoAcquireEnemiesWhenIdle = No\n"
	"  End\n"
	"  Behavior = SlavedUpdate ModuleTag_Slave\n"
	"    GuardMaxRange = 50\n"
	"    GuardWanderRange = 10\n"
	"    MarkUnselectable = Yes\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 0\n"
	"  End\n"
	"End\n"
	"Object P198MobileSlave\n"
	"  KindOf = INFANTRY ARCHER CAN_ATTACK\n"
	"  VisionRange = 250\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 5\n"
	"  GeometryHeight = 15\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY P198SlaveBow\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 200\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"    AutoAcquireEnemiesWhenIdle = No\n"
	"  End\n"
	"  Behavior = SlavedUpdate ModuleTag_Slave\n"
	"    GuardMaxRange = 50\n"
	"    GuardWanderRange = 10\n"
	"    MarkUnselectable = Yes\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n"
	"Object P198Spawner\n"
	"  KindOf = STRUCTURE IMMOBILE SELECTABLE SPAWNS_ARE_THE_WEAPONS FS_BASE_DEFENSE\n"
	"  VisionRange = 300\n"
	"  ShroudClearingRange = 300\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryHeight = 30\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 1000\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"    AutoAcquireEnemiesWhenIdle = No\n"
	"  End\n"
	"  Behavior = DefaultProductionExitUpdate ModuleTag_Exit\n"
	"    UnitCreatePoint = X:0.0 Y:0.0 Z:0.0\n"
	"    NaturalRallyPoint = X:0.0 Y:0.0 Z:0.0\n"
	"  End\n"
	"  Behavior = GettingBuiltBehavior ModuleTag_GB\n"
	"    SpawnTimer = 300\n"
	"  End\n"
	"  Behavior = SpawnBehavior ModuleTag_Spawn\n"
	"    SpawnNumber = 1\n"
	"    InitialBurst = 1\n"
	"    SpawnReplaceDelay = 10000\n"
	"    SpawnTemplateName = P198ImmobileSlave\n"
	"    OneShot = No\n"
	"  End\n"
	"End\n"
	"Object P198DualSpawner\n"
	"  KindOf = STRUCTURE IMMOBILE SELECTABLE SPAWNS_ARE_THE_WEAPONS\n"
	"  VisionRange = 300\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryHeight = 30\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 1000\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"    AutoAcquireEnemiesWhenIdle = No\n"
	"  End\n"
	"  Behavior = DefaultProductionExitUpdate ModuleTag_Exit\n"
	"    UnitCreatePoint = X:0.0 Y:0.0 Z:0.0\n"
	"    NaturalRallyPoint = X:0.0 Y:0.0 Z:0.0\n"
	"  End\n"
	"  Behavior = SpawnBehavior ModuleTag_SpawnA\n"
	"    SpawnNumber = 1\n"
	"    InitialBurst = 1\n"
	"    SpawnReplaceDelay = 10000\n"
	"    SpawnTemplateName = P198ImmobileSlave\n"
	"    OneShot = No\n"
	"  End\n"
	"  Behavior = SpawnBehavior ModuleTag_SpawnB\n"
	"    SpawnNumber = 1\n"
	"    InitialBurst = 1\n"
	"    SpawnReplaceDelay = 10000\n"
	"    SpawnTemplateName = P198MobileSlave\n"
	"    OneShot = No\n"
	"  End\n"
	"End\n"
	"Weapon P198HeroOnlyBow\n"
	"  AttackRange = 10\n"
	"  WeaponSpeed = 100\n"
	"  DelayBetweenShots = 400\n"
	"  ProjectileNugget\n"
	"    ProjectileTemplateName = Arrow\n"
	"    WarheadTemplateName = BowWarhead\n"
	"  End\n"
	"End\n"
	"Object P198WeaponSpawner\n"
	"  KindOf = STRUCTURE IMMOBILE SELECTABLE SPAWNS_ARE_THE_WEAPONS\n"
	"  VisionRange = 300\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryHeight = 30\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY P198HeroOnlyBow\n"
	"    OnlyAgainst = PRIMARY HERO\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 1000\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"    AutoAcquireEnemiesWhenIdle = No\n"
	"  End\n"
	"  Behavior = DefaultProductionExitUpdate ModuleTag_Exit\n"
	"    UnitCreatePoint = X:0.0 Y:0.0 Z:0.0\n"
	"    NaturalRallyPoint = X:0.0 Y:0.0 Z:0.0\n"
	"  End\n"
	"  Behavior = SpawnBehavior ModuleTag_Spawn\n"
	"    SpawnNumber = 1\n"
	"    InitialBurst = 1\n"
	"    SpawnReplaceDelay = 10000\n"
	"    SpawnTemplateName = P198MobileSlave\n"
	"    OneShot = No\n"
	"  End\n"
	"End\n"
	"Object P198DirectionalTower\n"
	"  KindOf = STRUCTURE IMMOBILE SELECTABLE CAN_ATTACK\n"
	"  VisionRange = 300\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryHeight = 30\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY P198AimBow\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 1000\n"
	"  End\n"
	"End\n"
	"Object P198HordeMember\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK\n"
	"  TransportSlotCount = 1\n"
	"  VisionRange = 250\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 5\n"
	"  GeometryHeight = 15\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY P198SlaveBow\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"    AutoAcquireEnemiesWhenIdle = No\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n"
	"Object P198BowHorde\n"
	"  KindOf = HORDE INFANTRY SELECTABLE CAN_ATTACK\n"
	"  TransportSlotCount = 1\n"
	"  VisionRange = 250\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 15\n"
	"  GeometryMinorRadius = 15\n"
	"  GeometryHeight = 15\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = HordeAIUpdate ModuleTag_AI\n"
	"    AutoAcquireEnemiesWhenIdle = No\n"
	"  End\n"
	"  Behavior = HordeContain ModuleTag_Horde\n"
	"    RankInfo = RankNumber:1 UnitType:P198HordeMember Position:X:0 Y:0 Position:X:0 Y:15\n"
	"    InitialPayload = P198HordeMember 2\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = HordeLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n"
	"Object P198TowerContain\n"
	"  KindOf = STRUCTURE IMMOBILE SELECTABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryHeight = 30\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 1000\n"
	"  End\n"
	"  Behavior = HordeGarrisonContain ModuleTag_Contain\n"
	"    PassengerFilter = ANY +INFANTRY\n"
	"    ContainMax = 20\n"
	"    AllowOwnPlayerInsideOverride = Yes\n"
	"    AllowAlliesInside = Yes\n"
	"  End\n"
	"End\n"
	"Object P198UnattackableDummy\n"
	"  KindOf = INFANTRY SELECTABLE UNATTACKABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"End\n"
	"Object P198WalkableWall\n"
	"  KindOf = STRUCTURE IMMOBILE SELECTABLE WALK_ON_TOP_OF_WALL\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 15\n"
	"  GeometryMinorRadius = 15\n"
	"  GeometryHeight = 20\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"End\n";

struct Port198Rig
{
	CombatWorld world{ kPort198Objects };
	ShroudManager shroud;
	PinholeView view;
	InGameUI ui;
	MessageStream stream;
	HudContext ctx{ *world.logic, world.ai.get(), view, ui, stream, nullptr, {} };
	CommandTranslator translator{ ctx };

	Port198Rig()
		: shroud(*world.logic)
	{
		world.combat().setAutoAcquireEnabled(false);
		world.players.setLocalPlayer(world.playerOf('A'));
		shroud.init(40.0f, 1);
		shroud.setExtent(0.0f, 0.0f, 2000.0f, 2000.0f);
		shroud.attach();
	}
	void select(Object *o)
	{
		ui.selectObject(o->getID());
		world.playerOf('A')->selection() = { o->getID() };
	}
};

struct SpawnerBuildFx : prodtest::ProdWorld
{
	SkirmishAIStore store{ w.keys };
	INIEnvironment env;
	pathtest::SyntheticTerrain synthTerrain{ 100, 100 };
	AIWorldConfig aiConfig;
	std::unique_ptr<AIWorld> aiWorld;
	Object *barracks = nullptr;

	SpawnerBuildFx()
	{
		load(prodtest::kBarracksObjects);
		load(prodtest::kBarracksCommands);
		load("Object P198HordeTrained\n"
			 "  BuildCost = 100\n"
			 "  BuildTime = 2.0\n"
			 "  CommandPoints = 2\n"
			 "  KindOf = SELECTABLE INFANTRY HORDE\n"
			 "  Behavior = AIUpdateInterface ModuleTag_AI\n"
			 "  End\n"
			 "End\n"
			 "Object P198UnitTrained\n"
			 "  BuildCost = 100\n"
			 "  BuildTime = 2.0\n"
			 "  CommandPoints = 2\n"
			 "  KindOf = SELECTABLE INFANTRY\n"
			 "  Behavior = AIUpdateInterface ModuleTag_AI\n"
			 "  End\n"
			 "End\n"
			 "CommandButton Command_BuildP198Unit\n"
			 "  Command = UNIT_BUILD\n"
			 "  Object = P198UnitTrained\n"
			 "  Options = CANCELABLE\n"
			 "End\n"
			 "CommandButton Command_BuildP198Horde\n"
			 "  Command = UNIT_BUILD\n"
			 "  Object = P198HordeTrained\n"
			 "  Options = CANCELABLE\n"
			 "End\n"
			 "CommandSet P198BarracksSet\n"
			 "  1 = Command_BuildP198Unit\n"
			 "  2 = Command_BuildP198Horde\n"
			 "End\n"
			 "Object P198AiBarracks\n"
			 "  KindOf = STRUCTURE IMMOBILE SELECTABLE\n"
			 "  CommandSet = P198BarracksSet\n"
			 "  Body = ActiveBody ModuleTag_Body\n"
			 "    MaxHealth = 500\n"
			 "  End\n"
			 "  Behavior = ProductionUpdate ModuleTag_PU\n"
			 "    NumDoorAnimations = 0\n"
			 "    MaxQueueEntries = 3\n"
			 "  End\n"
			 "  Behavior = QueueProductionExitUpdate ModuleTag_Q\n"
			 "    UnitCreatePoint = X:0.0 Y:-20.0 Z:0.0\n"
			 "    NaturalRallyPoint = X:30.0 Y:-50.0 Z:0.0\n"
			 "    ExitDelay = 100\n"
			 "  End\n"
			 "End\n"
			 "Object P198AiSlave\n"
			 "  KindOf = INFANTRY\n"
			 "  Behavior = AIUpdateInterface ModuleTag_AI\n"
			 "  End\n"
			 "  Behavior = SlavedUpdate ModuleTag_Slave\n"
			 "  End\n"
			 "End\n"
	"Object P198AiSpawner\n"
			 "  KindOf = STRUCTURE IMMOBILE SELECTABLE SPAWNS_ARE_THE_WEAPONS\n"
			 "  Body = ActiveBody ModuleTag_Body\n"
			 "    MaxHealth = 500\n"
			 "  End\n"
			 "  Behavior = DefaultProductionExitUpdate ModuleTag_Exit\n"
			 "    UnitCreatePoint = X:0.0 Y:0.0 Z:0.0\n"
			 "    NaturalRallyPoint = X:0.0 Y:0.0 Z:0.0\n"
			 "  End\n"
			 "  Behavior = SpawnBehavior ModuleTag_Spawn\n"
			 "    SpawnNumber = 1\n"
			 "    InitialBurst = 1\n"
			 "    SpawnReplaceDelay = 10000\n"
			 "    SpawnTemplateName = P198AiSlave\n"
			 "    OneShot = No\n"
			 "  End\n"
			 "End\n");
		store.registerBlocks(env.blocks);
		INI ini(env);
		const std::string aiIni =
			"SkirmishAIData TheSkirmishAIData\n"
			"End\n"
			"ArmyDefinition BetaArmy\n"
			"  Side = Beta\n"
			"End\n";
		ini.loadMemory("skirmishaidata.ini", std::vector<std::uint8_t>(aiIni.begin(), aiIni.end()), INI_LOAD_OVERWRITE);
		logic->skirmishAI().setStore(&store);
		bob()->setSkirmishDifficulty(2);
		REQUIRE(logic->skirmishAI().newGame(true) == 1);
		REQUIRE(logic->skirmishAI().findAI(bob()->getPlayerIndex()) != nullptr);
		logic->settings().bodyThresholdsLoaded = true;
		logic->settings().unitDamagedThreshold = 0.65f;
		logic->settings().unitReallyDamagedThreshold = 0.4f;
		aiConfig.pathfind = pathtest::testConfig();
		aiWorld = std::make_unique<AIWorld>(*logic, aiConfig, w.fx.env.macros);
		aiWorld->attach();
		aiWorld->newMap(synthTerrain);
		bob()->getMoney()->deposit(1000, false);
		barracks = make("P198AiBarracks", teamOf("Bob"));
		REQUIRE(barracks != nullptr);
		Coord3D pos{ 400.0f, 400.0f, 0.0f };
		barracks->setPosition(&pos);
	}
	~SpawnerBuildFx()
	{
		logic->reset();
		aiWorld.reset();
	}
	Player *bob() { return players.findPlayerWithName("Bob"); }
};
} // namespace

TEST_CASE("PORT-198 SpawnBehaviorInterface slots 2..6 union, getClosestSlave, reEnslaveSlaves & GarrisonRules::canAttackAsNewTarget")
{
	struct RetailFeatures
	{
		~RetailFeatures() { ModEngineFeatures::reset(); }
	} featuresGuard;
	Port198Rig r;
	Object *dual = r.world.unit("P198DualSpawner", 'A', 300.0f, 300.0f);
	Object *nearEnemy = r.world.unit("Dummy", 'B', 380.0f, 300.0f);
	Object *farEnemy = r.world.unit("Dummy", 'B', 660.0f, 300.0f);
	Object *unattackable = r.world.unit("P198UnattackableDummy", 'B', 360.0f, 300.0f);
	r.world.frames(5);
	r.shroud.update();

	// In retail mode (EdainUnchainedSpawnUnion off), SpawnBehaviorInterface::of returns only the first SpawnBehavior.
	SpawnBehaviorInterface *retailIface = SpawnBehaviorInterface::of(*dual);
	REQUIRE(retailIface != nullptr);
	std::vector<ObjectID> retailSlaveIds;
	retailIface->forEachSpawnID([&](ObjectID id) { retailSlaveIds.push_back(id); });
	REQUIRE(retailSlaveIds.size() == 1);

	// With EdainUnchainedSpawnUnion active, SpawnBehaviorInterface::of returns the union across both SpawnBehavior modules.
	std::string featErr;
	REQUIRE_MESSAGE(ModEngineFeatures::activate({ "5c31f5036cacb7690a6161b104a3efdb" }, &featErr), featErr);
	SpawnBehaviorInterface *spawnIface = SpawnBehaviorInterface::of(*dual);
	REQUIRE(spawnIface != nullptr);
	std::vector<ObjectID> slaveIds;
	spawnIface->forEachSpawnID([&](ObjectID id) { slaveIds.push_back(id); });
	REQUIRE(slaveIds.size() == 2);
	Object *immobileSlave = r.world.byId(slaveIds[0]);
	Object *mobileSlave = r.world.byId(slaveIds[1]);
	REQUIRE(immobileSlave != nullptr);
	REQUIRE(mobileSlave != nullptr);

	// Position slave B closer to nearEnemy than slave A: union slot 2 (RW 0x86292C) picks strictly closer by distSq2D.
	const Coord3D posA{ 260.0f, 300.0f, 0.0f };
	const Coord3D posB{ 360.0f, 300.0f, 0.0f };
	immobileSlave->setPosition(&posA);
	mobileSlave->setPosition(&posB);
	CHECK(spawnIface->getClosestSlave(nearEnemy->getPosition()) == mobileSlave);
	CHECK(spawnIface->getClosestSlave(&posA) == immobileSlave);

	// Union slots 5 & 6 aggregate POSSIBLE > POSSIBLE_AFTER_MOVING > INVALID_SHOT > NOT_POSSIBLE across modules:
	// at x=660, immobileSlave answers INVALID_SHOT (Speed=0 out of 200 reach), mobileSlave answers POSSIBLE_AFTER_MOVING -> union wins with POSSIBLE_AFTER_MOVING.
	CHECK(immobileSlave->getAbleToAttackSpecificObject(ATTACK_NEW_TARGET, farEnemy, CMD_FROM_PLAYER) == ATTACKRESULT_INVALID_SHOT);
	CHECK(mobileSlave->getAbleToAttackSpecificObject(ATTACK_NEW_TARGET, farEnemy, CMD_FROM_PLAYER) == ATTACKRESULT_POSSIBLE_AFTER_MOVING);
	CHECK(spawnIface->getCanAnySlavesAttackSpecificTarget(ATTACK_NEW_TARGET, farEnemy, CMD_FROM_PLAYER) == ATTACKRESULT_POSSIBLE_AFTER_MOVING);
	CHECK(spawnIface->getCanAnySlavesUseWeaponAgainstTarget(ATTACK_NEW_TARGET, farEnemy, farEnemy->getPosition(), CMD_FROM_PLAYER) == ATTACKRESULT_POSSIBLE_AFTER_MOVING);
	CHECK(spawnIface->getCanAnySlavesAttackSpecificTarget(ATTACK_NEW_TARGET, unattackable, CMD_FROM_PLAYER) == ATTACKRESULT_NOT_POSSIBLE);

	// Single spawner with GettingBuiltBehavior: test finishConstruction -> slot 10 reEnslaveSlaves (RW 0x8569C7 -> 0x862E4F)
	// and GarrisonRules::canAttackAsNewTarget (RW 0x82CFD4) including shroud veto and no spurious STRUCTURE error.
	Object *spawner = r.world.unit("P198Spawner", 'A', 1000.0f, 1000.0f);
	Object *inRangeEnemy = r.world.unit("Dummy", 'B', 1120.0f, 1000.0f);
	Object *outOfRangeEnemy = r.world.unit("Dummy", 'B', 1350.0f, 1000.0f);
	r.world.frames(5);
	r.shroud.update();

	SpawnBehaviorInterface *spawnerIface = SpawnBehaviorInterface::of(*spawner);
	REQUIRE(spawnerIface != nullptr);
	Object *spawnerSlave = spawnerIface->getClosestSlave(spawner->getPosition());
	REQUIRE(spawnerSlave != nullptr);
	SlavedUpdateInterface *slaved = SlavedUpdateInterface::of(*spawnerSlave);
	REQUIRE(slaved != nullptr);
	CHECK(slaved->getSlaverID() == spawner->getID());
	slaved->onEnslave(nullptr);
	CHECK(slaved->getSlaverID() == spawner->getID()); // RW 0x8A510B: null slaver is a no-op
	slaved->onEnslave(dual);
	CHECK(slaved->getSlaverID() == dual->getID());
	auto *gb = dynamic_cast<GettingBuiltBehavior *>(spawner->findModule("GettingBuiltBehavior"));
	REQUIRE(gb != nullptr);
	gb->finishConstruction(nullptr);
	CHECK(slaved->getSlaverID() == spawner->getID());
	slaved->onSlaverDie(nullptr);
	CHECK(slaved->getSlaverID() == INVALID_ID);
	gb->finishConstruction(nullptr);
	CHECK(slaved->getSlaverID() == spawner->getID());

	const size_t errorsBefore = r.world.logic->report().errors.size();
	CHECK(GarrisonRules::canAttackAsNewTarget(*spawner, *inRangeEnemy, CMD_FROM_PLAYER));
	CHECK_FALSE(GarrisonRules::canAttackAsNewTarget(*spawner, *outOfRangeEnemy, CMD_FROM_PLAYER));
	CHECK_FALSE(GarrisonRules::canAttackAsNewTarget(*spawner, *unattackable, CMD_FROM_PLAYER));
	CHECK(r.world.logic->report().errors.size() == errorsBefore);

	// Shrouded enemy at 1800,1800 is blocked for player orders on a SPAWNS_ARE_THE_WEAPONS spawner (S-565), allowed for scripts.
	Object *shroudedEnemy = r.world.unit("Dummy", 'B', 1800.0f, 1800.0f);
	r.shroud.update();
	REQUIRE(ShroudManager::isShroudedForAction(*spawner, *shroudedEnemy));
	const Coord3D nearShrouded{ 1700.0f, 1800.0f, 0.0f };
	spawnerSlave->setPosition(&nearShrouded);
	CHECK_FALSE(GarrisonRules::canAttackAsNewTarget(*spawner, *shroudedEnemy, CMD_FROM_PLAYER));
	CHECK(GarrisonRules::canAttackAsNewTarget(*spawner, *shroudedEnemy, CMD_FROM_SCRIPT));
}

TEST_CASE("PORT-198 SPAWNS_ARE_THE_WEAPONS orders (slots 3, 4, 8), CommandTranslator context & force attack")
{
	Port198Rig r;
	Object *spawner = r.world.unit("P198Spawner", 'A', 400.0f, 400.0f);
	Object *enemy = r.world.unit("Dummy", 'B', 500.0f, 400.0f);
	Object *farEnemy = r.world.unit("Dummy", 'B', 800.0f, 400.0f);
	Object *nearWall = r.world.unit("P198WalkableWall", 'B', 500.0f, 440.0f);
	Object *farWall = r.world.unit("P198WalkableWall", 'B', 800.0f, 440.0f);
	Object *ownBuilding = r.world.unit("P198TowerContain", 'A', 520.0f, 400.0f);
	Object *farOwnBuilding = r.world.unit("P198TowerContain", 'A', 850.0f, 400.0f);
	r.world.frames(5);
	r.shroud.update();

	SpawnBehaviorInterface *spawnIface = SpawnBehaviorInterface::of(*spawner);
	REQUIRE(spawnIface != nullptr);
	Object *slave = spawnIface->getClosestSlave(spawner->getPosition());
	REQUIRE(slave != nullptr);
	const Coord3D slavePos{ 410.0f, 400.0f, 0.0f };
	slave->setPosition(&slavePos);

	r.select(spawner);
	CHECK(r.translator.evaluateContextCommand(enemy, enemy->getPosition(), EvaluateType::DoHint) == MSG_DO_ATTACK_OBJECT);
	CHECK(r.ui.cursor() == std::string(MouseCursorName::AttackObj));
	CHECK(r.translator.evaluateContextCommand(farEnemy, farEnemy->getPosition(), EvaluateType::DoHint) == MSG_DO_ATTACK_OBJECT);
	CHECK(r.translator.evaluateContextCommand(nearWall, nearWall->getPosition(), EvaluateType::DoHint) == MSG_DO_ATTACK_OBJECT);
	CHECK(r.translator.evaluateContextCommand(farWall, farWall->getPosition(), EvaluateType::DoHint) == 0);

	r.ui.setForceAttackMode(true);
	CHECK(r.translator.evaluateForceAttack(ownBuilding, ownBuilding->getPosition(), EvaluateType::DoHint) == MSG_DO_FORCE_ATTACK_OBJECT);
	CHECK(r.translator.evaluateForceAttack(farOwnBuilding, farOwnBuilding->getPosition(), EvaluateType::DoHint) == 0);
	CHECK(r.ui.cursor() == std::string(MouseCursorName::GenericInvalid));
	const Coord3D forceGroundPos{ 480.0f, 400.0f, 0.0f };
	CHECK(r.translator.evaluateForceAttack(nullptr, &forceGroundPos, EvaluateType::DoHint) == MSG_DO_FORCE_ATTACK_GROUND);

	// RW 0x81D9DE: a SPAWNS_ARE_THE_WEAPONS spawner that ALSO owns a weapon still falls back to its closest slave when its own weapon cannot hit target,
	// and reports GenericInvalid once its slaves are gone.
	Object *armedSpawner = r.world.unit("P198WeaponSpawner", 'A', 400.0f, 200.0f);
	r.world.frames(5);
	r.select(armedSpawner);
	CHECK(r.translator.evaluateForceAttack(ownBuilding, ownBuilding->getPosition(), EvaluateType::DoHint) == MSG_DO_FORCE_ATTACK_OBJECT);
	Object *armedSlave = SpawnBehaviorInterface::of(*armedSpawner)->getClosestSlave(armedSpawner->getPosition());
	REQUIRE(armedSlave != nullptr);
	r.world.logic->destroyObject(armedSlave);
	r.world.frames(1);
	CHECK(r.translator.evaluateForceAttack(ownBuilding, ownBuilding->getPosition(), EvaluateType::DoHint) == 0);
	CHECK(r.ui.cursor() == std::string(MouseCursorName::GenericInvalid));
	r.select(spawner);
	r.ui.setForceAttackMode(false);

	// AIUpdateInterface::aiAttackObject rejects non-enemy/out-of-range targets and dispatches slot 3 for valid targets even when m_weapons is null.
	AIUpdateInterface *spawnerAI = spawner->getAIUpdateInterface();
	REQUIRE(spawnerAI != nullptr);
	CHECK_FALSE(spawnerAI->aiAttackObject(ownBuilding, CMD_FROM_PLAYER));
	CHECK_FALSE(spawnerAI->aiAttackObject(farEnemy, CMD_FROM_PLAYER));
	REQUIRE(spawnerAI->aiAttackObject(enemy, CMD_FROM_PLAYER));
	const float hpBefore = enemy->getBodyModule()->getHealth();
	r.world.frames(12);
	// tolerance: arrow flight and PIERCE armor fraction reduce health strictly below starting 100
	CHECK(enemy->getBodyModule()->getHealth() < hpBefore);

	// AIGroup::groupIdle dispatches slot 8 orderSlavesToGoIdle (slave returns to idle).
	AIGroup group(*r.world.logic, { spawner->getID() });
	group.groupIdle(CMD_FROM_PLAYER);
	r.world.frames(1);
	CHECK(slave->getAIUpdateInterface()->currentStateId() == (unsigned)AI_IDLE);

	// AIGroup::groupAttackPosition unconditionally dispatches slot 4 orderSlavesToAttackPosition.
	const Coord3D groundPos{ 480.0f, 430.0f, 0.0f };
	group.groupAttackPosition(groundPos, 0x7FFFFFFF, CMD_FROM_PLAYER);
	r.world.frames(1);
	CHECK(slave->getAIUpdateInterface()->currentStateId() == (unsigned)AI_ATTACK_POSITION);
}

TEST_CASE("PORT-198 WeaponSet::getAbleToUseWeaponAgainstTarget & Object containment / fallback gates (RW 0x6C8652, 0x68D6A6, 0x68B5FA)")
{
	Port198Rig r;
	Object *tower = r.world.unit("P198DirectionalTower", 'A', 400.0f, 400.0f);
	tower->setOrientation(0.0f); // faces +X
	Object *inFront = r.world.unit("Dummy", 'B', 520.0f, 400.0f);
	Object *toSide = r.world.unit("Dummy", 'B', 400.0f, 520.0f);
	r.world.frames(2);

	// Immobile AcceptableAimDelta check: in front (0 deg) -> POSSIBLE; 90 deg off-axis (> 15 deg) -> INVALID_SHOT.
	CHECK(tower->getAbleToUseWeaponAgainstTarget(ATTACK_NEW_TARGET, inFront, inFront->getPosition(), CMD_FROM_PLAYER) == ATTACKRESULT_POSSIBLE);
	CHECK(tower->getAbleToUseWeaponAgainstTarget(ATTACK_NEW_TARGET, toSide, toSide->getPosition(), CMD_FROM_PLAYER) == ATTACKRESULT_INVALID_SHOT);

	// Immobile Speed=0 slave vs mobile slave out of 200 range:
	Object *immobileSlave = r.world.unit("P198ImmobileSlave", 'A', 400.0f, 500.0f);
	Object *mobileSlave = r.world.unit("P198MobileSlave", 'A', 400.0f, 500.0f);
	Object *farDummy = r.world.unit("Dummy", 'B', 750.0f, 500.0f);
	r.world.frames(2);
	CHECK(immobileSlave->getAbleToUseWeaponAgainstTarget(ATTACK_NEW_TARGET, farDummy, farDummy->getPosition(), CMD_FROM_PLAYER) == ATTACKRESULT_INVALID_SHOT);
	CHECK(mobileSlave->getAbleToUseWeaponAgainstTarget(ATTACK_NEW_TARGET, farDummy, farDummy->getPosition(), CMD_FROM_PLAYER) == ATTACKRESULT_POSSIBLE_AFTER_MOVING);

	// CONTESTING_BUILDING without victim returns INVALID_SHOT when attackType bit 3 is clear.
	static const int kContesting = CombatNames::status("CONTESTING_BUILDING");
	mobileSlave->setStatus((unsigned)kContesting, true);
	const Coord3D groundSpot{ 460.0f, 500.0f, 0.0f };
	CHECK(mobileSlave->getAbleToUseWeaponAgainstTarget(ATTACK_NEW_TARGET, nullptr, &groundSpot, CMD_FROM_PLAYER) == ATTACKRESULT_INVALID_SHOT);
	mobileSlave->setStatus((unsigned)kContesting, false);

	// HORDE containment: horde Object itself delegates getAbleToAttackSpecificObject to members; member inside HORDE can move to target;
	// member inside non-HORDE garrison cannot move -> INVALID_SHOT when out of range.
	Object *horde = r.world.unit("P198BowHorde", 'A', 400.0f, 700.0f);
	Object *nearTarget = r.world.unit("Dummy", 'B', 500.0f, 700.0f);
	Object *farTarget = r.world.unit("Dummy", 'B', 780.0f, 700.0f);
	r.world.frames(3);
	CHECK(horde->getAbleToAttackSpecificObject(ATTACK_NEW_TARGET, nearTarget, CMD_FROM_PLAYER) == ATTACKRESULT_POSSIBLE);
	CHECK(horde->getAbleToAttackSpecificObject(ATTACK_NEW_TARGET, farTarget, CMD_FROM_PLAYER) == ATTACKRESULT_POSSIBLE_AFTER_MOVING);

	const ContainModuleInterface::ContainedItemsList *members = horde->getContain()->getContainedItemsList();
	REQUIRE(members != nullptr);
	REQUIRE(!members->empty());
	Object *member = members->front();
	CHECK(member->getAbleToUseWeaponAgainstTarget(ATTACK_NEW_TARGET, farTarget, farTarget->getPosition(), CMD_FROM_PLAYER) == ATTACKRESULT_POSSIBLE_AFTER_MOVING);

	Object *garrison = r.world.unit("P198TowerContain", 'A', 400.0f, 900.0f);
	Object *garrisoned = r.world.unit("P198HordeMember", 'A', 400.0f, 900.0f);
	Object *farFromTower = r.world.unit("Dummy", 'B', 750.0f, 900.0f);
	r.world.frames(2);
	REQUIRE(garrison->getContain()->addToContain(garrisoned));
	CHECK(garrisoned->getAbleToUseWeaponAgainstTarget(ATTACK_NEW_TARGET, farFromTower, farFromTower->getPosition(), CMD_FROM_PLAYER) == ATTACKRESULT_INVALID_SHOT);

	// Empty WeaponSet / weaponless Object::getAbleToUseWeaponAgainstTarget returns ATTACKRESULT_INVALID_SHOT (RW 0x6C88F2: totalAnti == 0);
	// WeaponSet slot 6 fallback (RW 0x6C8A01 .. 0x6C8A3E) on IMMOBILE + SPAWNS_ARE_THE_WEAPONS promotes POSSIBLE_AFTER_MOVING -> POSSIBLE under ATTACK_TUNNEL_NETWORK_GUARD
	// when a slave returns POSSIBLE, and returns INVALID_SHOT when no slave returns POSSIBLE.
	Object *weaponlessSpawner = r.world.unit("P198Spawner", 'A', 200.0f, 1100.0f);
	Object *weaponSpawner = r.world.unit("P198WeaponSpawner", 'A', 200.0f, 1200.0f);
	Object *nearDual = r.world.unit("Dummy", 'B', 280.0f, 1200.0f);
	Object *farFromDual = r.world.unit("Dummy", 'B', 560.0f, 1200.0f);
	r.world.frames(5);
	CHECK(weaponlessSpawner->getAbleToUseWeaponAgainstTarget(ATTACK_NEW_TARGET, nearDual, nearDual->getPosition(), CMD_FROM_PLAYER) == ATTACKRESULT_INVALID_SHOT);
	CHECK(weaponSpawner->getAbleToUseWeaponAgainstTarget(ATTACK_NEW_TARGET, nearDual, nearDual->getPosition(), CMD_FROM_PLAYER) == ATTACKRESULT_INVALID_SHOT);
	CHECK(weaponSpawner->getAbleToUseWeaponAgainstTarget(ATTACK_TUNNEL_NETWORK_GUARD, nearDual, nearDual->getPosition(), CMD_FROM_PLAYER) == ATTACKRESULT_POSSIBLE);
	CHECK(weaponSpawner->getAbleToUseWeaponAgainstTarget(ATTACK_TUNNEL_NETWORK_GUARD, farFromDual, farFromDual->getPosition(), CMD_FROM_PLAYER) == ATTACKRESULT_INVALID_SHOT);
}

TEST_CASE("PORT-198 Player::onUnitCreated -> SkirmishAIManager::unitCreated -> AIBaseBuilder::onStructureBuilt(..., false)")
{
	SpawnerBuildFx f;
	AISkirmishPlayer *ai = f.logic->skirmishAI().findAI(f.bob()->getPlayerIndex());
	REQUIRE(ai != nullptr);
	REQUIRE_FALSE(ai->disabled);

	// 1. ProductionUpdate::completeUnit: non-HORDE completes request (state 1 -> 2), HORDE is skipped (RW 0x8F0703).
	auto reqUnit = std::make_shared<AIBuildRequest>();
	reqUnit->serial = 1;
	reqUnit->templateName = "P198UnitTrained";
	reqUnit->producer = f.barracks->getID();
	reqUnit->state = 1;
	ai->build.inProgress.push_back(reqUnit);

	auto reqHorde = std::make_shared<AIBuildRequest>();
	reqHorde->serial = 2;
	reqHorde->templateName = "P198HordeTrained";
	reqHorde->producer = f.barracks->getID();
	reqHorde->state = 1;
	ai->build.inProgress.push_back(reqHorde);

	ProductionUpdateInterface *pu = f.barracks->getProductionUpdate();
	REQUIRE(pu != nullptr);
	const ThingTemplate *unitTt = f.w.get("P198UnitTrained");
	const ThingTemplate *hordeTt = f.w.get("P198HordeTrained");
	REQUIRE(unitTt != nullptr);
	REQUIRE(hordeTt != nullptr);
	REQUIRE(pu->queueCreateUnit(unitTt, -1, pu->requestUniqueUnitID(), -1, false, std::string(), false));
	f.frames(25);
	CHECK(reqUnit->state == 2);

	REQUIRE(pu->queueCreateUnit(hordeTt, -1, pu->requestUniqueUnitID(), -1, false, std::string(), false));
	f.frames(25);
	CHECK(reqHorde->state == 1);

	// 2. SpawnBehavior::createSpawn fires Player::onUnitCreated(spawner, slave).
	Object *spawner = f.make("P198AiSpawner", f.teamOf("Bob"));
	REQUIRE(spawner != nullptr);
	auto reqSlave = std::make_shared<AIBuildRequest>();
	reqSlave->serial = 3;
	reqSlave->templateName = "P198AiSlave";
	reqSlave->producer = spawner->getID();
	reqSlave->state = 1;
	ai->build.inProgress.push_back(reqSlave);
	f.frames(4);
	CHECK(reqSlave->state == 2);

	// 3. StartingBase / null factory guard does not crash and respects disabled AI.
	Object *orphanUnit = f.make("P198UnitTrained", f.teamOf("Bob"));
	REQUIRE(orphanUnit != nullptr);
	f.bob()->onUnitCreated(nullptr, orphanUnit);
	ai->disabled = true;
	auto reqDisabled = std::make_shared<AIBuildRequest>();
	reqDisabled->serial = 4;
	reqDisabled->templateName = "P198UnitTrained";
	reqDisabled->producer = f.barracks->getID();
	reqDisabled->state = 1;
	ai->build.inProgress.push_back(reqDisabled);
	f.bob()->onUnitCreated(f.barracks, orphanUnit);
	CHECK(reqDisabled->state == 1);

	// 4. StartingBase::placeForPlayer calls Player::onUnitCreated(nullptr, unit) for starting units (RW 0x62B147) and skips it for structures.
	struct SpyHost : PlayerEconomyHost
	{
		PlayerEconomyHost &inner;
		std::vector<std::pair<Object *, Object *>> calls;
		explicit SpyHost(PlayerEconomyHost &i) : inner(i) {}
		bool hasObjectMatching(const Player &p, const ObjectFilter &flt, bool c) const override { return inner.hasObjectMatching(p, flt, c); }
		int commandPointLimit(const Player &p) const override { return inner.commandPointLimit(p); }
		float productionCostChange(const Player &p, const ThingTemplate &t, bool s) const override { return inner.productionCostChange(p, t, s); }
		int applyHandicapMoney(const Player &p, int a) const override { return inner.applyHandicapMoney(p, a); }
		bool canAffordCommandPoints(const Player &p, const ThingTemplate &t) const override { return inner.canAffordCommandPoints(p, t); }
		void updateUpgradeModulesOfPlayer(const Player &p) override { inner.updateUpgradeModulesOfPlayer(p); }
		void removeUpgradeFromObjectsOfPlayer(const Player &p, const UpgradeTemplate &u) override { inner.removeUpgradeFromObjectsOfPlayer(p, u); }
		void reportUpgradeEva(const Player &p, const UpgradeTemplate &u, bool g) override { inner.reportUpgradeEva(p, u, g); }
		std::optional<float> brutalReduction(const Player &p, bool t) const override { return inner.brutalReduction(p, t); }
		void onUnitCreated(const Player &p, Object *factory, Object &unit) override
		{
			calls.emplace_back(factory, &unit);
			inner.onUnitCreated(p, factory, unit);
		}
	};
	WorldHeightMap hmap;
	hmap.m_width = 100;
	hmap.m_height = 100;
	hmap.m_borderSize = 0;
	hmap.m_data.assign(100u * 100u, 0);
	hmap.m_boundaries.push_back({ 100, 100 });
	MapChunks chunks;
	MapObject wpObj;
	wpObj.m_isWaypoint = true;
	wpObj.m_location = { 500.0f, 500.0f, 0.0f };
	wpObj.m_properties.setInt("waypointID", 1);
	wpObj.m_properties.setAsciiString("waypointName", "Player_1_Start");
	chunks.objects.push_back(wpObj);
	TerrainLogic terrain;
	terrain.init(hmap, chunks);
	f.logic->setTerrain(&terrain);

	SpyHost spy(f.economy());
	f.bob()->setEconomyHost(&spy);
	PlayerTemplate pt;
	pt.m_startingBuilding = "Barracks";
	pt.m_startingUnit[0] = "P198UnitTrained";
	pt.m_startingUnitOffset[0] = { 50.0f, 0.0f, 0.0f };
	SkirmishGameSlot slot;
	slot.startPos = 0;
	StartingBase::Result startResult;
	StartingBase::placeForPlayer(*f.logic, 1, slot, *f.bob(), pt, startResult);
	f.bob()->setEconomyHost(&f.economy());
	f.logic->setTerrain(nullptr);
	REQUIRE(startResult.errors.empty());
	REQUIRE(startResult.placed.size() == 2);
	REQUIRE(spy.calls.size() == 1);
	CHECK(spy.calls[0].first == nullptr);
	CHECK(spy.calls[0].second->getTemplate()->getName() == "P198UnitTrained");
}
