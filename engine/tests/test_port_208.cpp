// OpenBFME unit tests. GPL-3.0.
// PORT-208 (issue #490, stop S-2480): force attack out of garrisons and spawners, and the clicked map tree as a target.
// Synthetic INI only (CombatTestUtil's world), every expectation read from the RotWK 2.01 binary (caveat S-001) with its address:
//   * RW 0x77208A AIGroup::groupAttackObject(force, victim, source): step 1 (RW 0x77214D .. 0x7721D1) forwards the object attack to the passengers of a
//     container whose slot 0xB4 (isPassengerAllowedToFire) answers true, each passenger asked Object::getAbleToAttackSpecificObject (RW 0x68D6A6, the
//     port's ObjectWeapons::canAttackObject, S-325) and then ordered aiForceAttackObject (RW 0x771380) or aiAttackObject (RW 0x66C536); step 2 (RW
//     0x7721D3 .. 0x772235) orders an object with an AI itself.
//   * RW 0x77229A AIGroup::groupAttackPosition: the same container forward for a ground point, each passenger tested through RW 0x68B5FA ->
//     RW 0x6C8652 getAbleToUseWeaponAgainstTarget(ATTACK_NEW_TARGET, null, &pos, source), and a spawner's slaves through RW 0x863807.
//   * RW 0x6AAB85 -> 0x8DB249 -> 0x770917: a live selected object joins the AIGroup even without an AIUpdateInterface, so an AI-less garrisoned tower and
//     an AI-less SPAWNS_ARE_THE_WEAPONS lair reach those two loops.
//   * RW 0x77AF18 / RW 0x77C1C2 (MSG_DO_FORCE_ATTACK_OBJECT RW 0x77AED3 / MSG_DO_ATTACK_OBJECT RW 0x77C170): a victim of id 99999999, compared at
//     [victim+0x74]; with AIData AllowForestFires (TheAI + 0x18 + 0xBC) the tree nearest the message's location (RW 0x68449B, TerrainTrees::makeTreeObject)
//     becomes the victim and script selectable (RW 0x6719FA); without it the id resolves to null (RW 0x77AF67 / RW 0x77C211).
//   * RW 0x6913B7 .. 0x691416 Object::isAbleToAttack's tail: SPAWNS_ARE_THE_WEAPONS -> the spawn interface's canAnySlavesAttack (RW 0x862A4E).
//   * RW 0x66DE32: privateAttackPosition refuses an object outside the playable area (Object + 0x458 bit 3, Object::isOffMap).
//   * RW 0x6912E3 (Object::isAbleToAttack step 2): a rider is able to attack only while its container's ObjectStatusOfContained mask holds CAN_ATTACK,
//     which is why the mod's garrison data sets it (GondorCastleKeep, RohanBattleTower) and the synthetic tower here does too.
//   * RW 0x68D91A (Object::estimateDamage): a TREE (template + 0x113 bit 6) answers 1.0 for FLAME and 0 for every other damage type, so a made tree is
//     hurt by a fire arrow alone; the archer's weapon choice (RW 0x6C8A4E, most damage) picks the 20 FLAME arrow over the 12-effective PIERCE one.

#include "doctest.h"
#include "CombatTestUtil.h"

#include "GameLogic/AI/AIAttack.h"
#include "GameLogic/AI/AIGroup.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Combat/ObjectWeapons.h"
#include "GameLogic/Map/TerrainLogic.h"
#include "GameLogic/Map/TerrainTrees.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/SpawnBehavior.h"
#include "GameLogic/Object/Contain/GarrisonContainRuntime.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/Object/PartitionManager.h"
#include "GameEngineDevice/W3DDevice/GameClient/WorldHeightMap.h"
#include "MapTestUtil.h"
#include "MoveTestUtil.h"

#include <string>
#include <vector>

using namespace combattest;

namespace
{
// The mod's shapes, reduced. P208Tower is the Ennorath RohanBattleTower / GondorBuildingGeneral* case: a garrisonable structure with NO
// AIUpdateInterface and no weapon of its own — only its passengers fire. P208Lair is the AngForStronghold case: an AI-less structure whose spawned
// archers fight for it (SPAWNS_ARE_THE_WEAPONS, RW 0x6913F5).
const char kP208Objects[] =
	// the mod's bombard bow (MordorArcherBowBombardEdain, weapon.inc): a ground attack needs a weapon that needs no victim, and the forced ground order
	// picks it through RW 0x6C8A4E's no-victim walk (the first slot whose weapon has NoVictimNeeded); the plain bow would leave the state at once
	"Weapon P208BombardBow\n"
	"  AttackRange = 300\n"
	"  MinimumAttackRange = 10\n"
	"  DelayBetweenShots = 0\n"
	"  ClipSize = 1\n"
	"  AutoReloadsClip = Yes\n"
	"  AcceptableAimDelta = 45\n"
	"  NoVictimNeeded = Yes\n"
	"  BombardType = Yes\n"
	"  ScatterRadius = 10\n"
	"  ProjectileNugget\n"
	"    ProjectileTemplateName = Arrow\n"
	"    WarheadTemplateName = BowWarhead\n"
	"  End\n"
	"End\n"
	// RW 0x68D91A Object::estimateDamage: a TREE (template + 0x113 bit 6) answers 1.0 for FLAME (6) and 0 for every other type, so only a fire arrow hurts
	// the made tree. The mod's firing arrows are this shape (a DamageNugget with DamageType = FLAME).
	"Weapon P208FireArrow\n"
	"  AttackRange = 300\n"
	"  DelayBetweenShots = 400\n"
	"  ClipSize = 1\n"
	"  AutoReloadsClip = Yes\n"
	"  DamageNugget\n"
	"    Damage = 20\n"
	"    Radius = 0\n"
	"    DamageType = FLAME\n"
	"    DeathType = BURNED\n"
	"  End\n"
	"End\n"
	"Object P208Archer\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK\n"
	"  VisionRange = 300\n"
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
	"    Weapon = PRIMARY BowWeapon\n"
	"    Weapon = SECONDARY P208FireArrow\n"
	"    Weapon = TERTIARY P208BombardBow\n"
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
	"    Speed = 40\n"
	"  End\n"
	"End\n"
	// a garrison that lets its passengers fire (GarrisonContain, slot 0xB4 true), with no AI of its own
	"Object P208Tower\n"
	"  KindOf = STRUCTURE IMMOBILE SELECTABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 25\n"
	"  GeometryMinorRadius = 25\n"
	"  GeometryHeight = 60\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 1000\n"
	"  End\n"
	"  Behavior = GarrisonContain ModuleTag_Contain\n"
	"    ContainMax = 4\n"
	// the mod's ObjectStatusOfContained (GondorCastleKeep, RohanBattleTower): without CAN_ATTACK the passenger is not able to attack (RW 0x6912E3)
	"    ObjectStatusOfContained = UNSELECTABLE CAN_ATTACK ENCLOSED\n"
	"  End\n"
	"End\n"
	// an AI-less lair whose spawned archers are its weapons
	"Object P208Lair\n"
	"  KindOf = STRUCTURE IMMOBILE SELECTABLE SPAWNS_ARE_THE_WEAPONS\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryMinorRadius = 20\n"
	"  GeometryHeight = 40\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 2000\n"
	"  End\n"
	"  Behavior = SpawnBehavior ModuleTag_Spawn\n"
	"    SpawnNumber = 2\n"
	"    SpawnReplaceDelay = 100000\n"
	"    SpawnTemplateName = P208Archer\n"
	"    SpawnedRequireSpawner = Yes\n"
	"  End\n"
	"  Behavior = QueueProductionExitUpdate ModuleTag_Exit\n"
	"    UnitCreatePoint = X:40.0 Y:0.0 Z:0.0\n"
	"    NaturalRallyPoint = X:60.0 Y:0.0 Z:0.0\n"
	"  End\n"
	"End\n"
	// the map tree a click names (LOCAL-42: TerrainTrees makes it an object)
	"Object P208Tree\n"
	"  Draw = W3DTreeDraw ModuleTag_Draw\n"
	"    ModelName = PTTree01\n"
	"  End\n"
	"  KindOf = SHRUBBERY IMMOBILE CLUB TREE SELECTABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 5\n"
	"  GeometryMinorRadius = 5\n"
	"  GeometryHeight = 30\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"End\n";

struct Port208World : CombatWorld
{
	Port208World()
		: CombatWorld(kP208Objects)
	{
		combat().setAutoAcquireEnabled(false);
		logic->settings().allowForestFires = false; // AIData AllowForestFires: No in retail 2.01 and in Ennorath
		// the map trees a click can name: the tree buffer needs an extent, the partition a region (LiveGame sets both from the map)
		logic->trees().setExtent(-500.0f, -500.0f, 3000.0f, 3000.0f);
		logic->partition().setRegion(0.0f, 0.0f, 3000.0f, 3000.0f);
	}
	unsigned long long shotsOf(Object *o)
	{
		const ObjectWeapons *weapons = o->getWeapons();
		REQUIRE(weapons);
		return weapons->stats().shotsFired;
	}
	unsigned state(Object *o)
	{
		AIUpdateInterface *ai = o->getAIUpdateInterface();
		REQUIRE(ai);
		return ai->currentStateId();
	}
	void forceObjectMessage(int player, ObjectID id, const Coord3D &at)
	{
		GameMessage m(MSG_DO_FORCE_ATTACK_OBJECT, player);
		m.appendObjectIDArgument(id);
		m.appendLocationArgument(at); // the client appends the clicked point too (CommandXlat, RW 0x81E4B5)
		send(m);
	}
	void groundMessage(int player, float x, float y)
	{
		GameMessage m(MSG_DO_FORCE_ATTACK_GROUND, player);
		m.appendLocationArgument(Coord3D{ x, y, logic->getGroundHeight(x, y) });
		send(m);
	}
	const ThingTemplate *treeTemplate()
	{
		const ThingTemplate *tt = w.things.findTemplate("P208Tree");
		REQUIRE(tt != nullptr);
		return tt;
	}
	bool addTree(const Coord3D &at, std::uint32_t clientId) { return logic->trees().addTree(*treeTemplate(), at, clientId); }
	// the one P208Tree object in the game, null for none
	Object *madeTree()
	{
		for (Object *o = logic->getFirstObject(); o; o = o->getNextObject())
		{
			if (o->getTemplate() && o->getTemplate()->getName() == "P208Tree")
			{
				return o;
			}
		}
		return nullptr;
	}
	Object *spawnedSlave(Object *lair)
	{
		SpawnBehaviorInterface *spawn = SpawnBehaviorInterface::of(*lair);
		REQUIRE(spawn);
		Object *first = nullptr;
		spawn->forEachSpawnID([&](ObjectID id) {
			if (!first)
			{
				first = logic->findObjectByID(id);
			}
		});
		return first;
	}
};

// a world with a terrain: the outside-the-playable-area refusal needs a map extent (Object::updateOffMapStatus reads TerrainLogic::getExtent)
struct Port208MapWorld : Port208World
{
	WorldHeightMap heightMap;
	MapChunks chunks;
	TerrainLogic boundaryTerrain;
	Port208MapWorld() : Port208World()
	{
		heightMap.m_width = 60;
		heightMap.m_height = 60;
		heightMap.m_data.assign(60u * 60u, 0);
		heightMap.m_boundaries = { ICoord2D{ 30, 30 } }; // TerrainLogic::getExtent(0) reads the first entry (RW 0x462637)
		boundaryTerrain.init(heightMap, chunks);
		logic->setTerrain(&boundaryTerrain);
	}
	~Port208MapWorld()
	{
		logic->reset(); // the objects leave the terrain while both are alive
		logic->setTerrain(nullptr);
	}
};
} // namespace

// RW 0x77229A (RW 0x7722CE .. 0x772355): an AI-less garrisoned tower passes the ground force attack to its archers, which fire at the point. The tower
// itself has no AI and no weapon, so the order can only arrive through the container forward (and the tower only reaches the group through RW 0x770917).
TEST_CASE("PORT-208 force attack ground: the archers inside an AI-less garrison fire at the clicked point (RW 0x77229A, RW 0x68B5FA)")
{
	Port208World w;
	const int alice = w.playerIndex("Alice");
	Object *tower = w.unit("P208Tower", 'A', 300, 300);
	Object *archer = w.unit("P208Archer", 'A', 340, 300);
	w.frames(2);
	REQUIRE(tower->getAIUpdateInterface() == nullptr); // the Ennorath tower has no AIUpdate
	REQUIRE(tower->getContain() != nullptr);
	REQUIRE(tower->getContain()->isPassengerAllowedToFire()); // contain slot 0xB4
	REQUIRE(tower->getContain()->addToContain(archer));
	REQUIRE(archer->getContainedBy() == tower);
	// the group the click builds: a tower without an AI is a member because its contain lets passengers fire (RW 0x770917)
	AIGroup direct(*w.logic, { tower->getID() });
	CHECK(direct.size() == 1);
	w.select(alice, { tower });
	w.groundMessage(alice, 450.0f, 300.0f);
	w.frames(1);
	CHECK(w.aiCommands.stats().forceAttackGrounds == 1);
	CHECK(w.state(archer) == (unsigned)AI_ATTACK_POSITION); // aiAttackPosition from the contain forward
	// the first arrow leaves 8 frames in (PreAttackDelay of the bombard bow is 0, the state reaches its shot after the aim)
	w.runUntil([&] { return w.shotsOf(archer) != 0ull; }, 200);
	CHECK(w.shotsOf(archer) != 0ull);
}

// The same message with an empty tower: nothing to order, no shot. And the object form for a tower whose archer is inside: step 1 of RW 0x77208A.
TEST_CASE("PORT-208 force attack: an empty AI-less garrison orders nothing; with passengers it attacks the clicked enemy (RW 0x77214D .. 0x7721D1)")
{
	Port208World w;
	const int alice = w.playerIndex("Alice");
	Object *tower = w.unit("P208Tower", 'A', 300, 300);
	Object *archer = w.unit("P208Archer", 'A', 340, 300);
	Object *foe = w.unit("Dummy", 'B', 480, 300);
	w.frames(2);
	w.select(alice, { tower });
	w.groundMessage(alice, 450.0f, 300.0f);
	w.frames(2);
	CHECK(w.shotsOf(archer) == 0);
	CHECK(w.state(archer) == (unsigned)AI_IDLE);
	// now the archer is a passenger: Ctrl+click on the enemy reaches it through the contain
	REQUIRE(tower->getContain()->addToContain(archer));
	w.select(alice, { tower }, false);
	w.forceObjectMessage(alice, foe->getID(), *foe->getPosition());
	w.frames(1);
	CHECK(w.aiCommands.stats().forceAttacks == 1);
	CHECK(w.state(archer) == (unsigned)AI_FORCE_ATTACK_OBJECT);
	CHECK(archer->getAIUpdateInterface()->currentVictimId() == foe->getID());
	w.runUntil([&] { return w.health(foe) != 100.0f; }, 300);
	CHECK(w.health(foe) == 80.0f); // the choice's most damage: the 20 FLAME fire arrow (PlainArmor 100%) beats 15 PIERCE at 80% (12)
}

// RW 0x77229A's second half (RW 0x772357 .. 0x772375 -> RW 0x863807): an AI-less SPAWNS_ARE_THE_WEAPONS lair orders its slaves to the point, and
// RW 0x6913B7 .. 0x691416 (canAnySlavesAttack, RW 0x862A4E) is what lets the lair count as an attacker at all.
TEST_CASE("PORT-208 force attack ground: an AI-less spawner's slaves fire at the clicked point (RW 0x863807, RW 0x6913F5)")
{
	Port208World w;
	const int alice = w.playerIndex("Alice");
	Object *lair = w.unit("P208Lair", 'A', 300, 300);
	w.frames(2); // the spawner's first updates make its archers
	REQUIRE(lair->getAIUpdateInterface() == nullptr);
	REQUIRE(lair->isKindOf((unsigned)CombatNames::kinds().spawnsAreTheWeapons));
	CHECK(lair->isAbleToAttack()); // RW 0x6913B7: through canAnySlavesAttack
	Object *slave = w.spawnedSlave(lair);
	REQUIRE(slave != nullptr);
	AIGroup direct(*w.logic, { lair->getID() }); // the group filter RW 0x770917 lets the AI-less lair in
	CHECK(direct.size() == 1);
	// the spawn walks out of the lair first (QueueProductionExitUpdate) and reaches the exit; the order arrives after that
	w.runUntil([&] { return w.state(slave) == (unsigned)AI_IDLE; }, 200);
	REQUIRE(w.state(slave) == (unsigned)AI_IDLE);
	w.select(alice, { lair });
	w.groundMessage(alice, 450.0f, 300.0f);
	w.frames(1);
	CHECK(w.state(slave) == (unsigned)AI_ATTACK_POSITION);
	w.runUntil([&] { return w.shotsOf(slave) != 0ull; }, 200);
	CHECK(w.shotsOf(slave) != 0ull);
}

// RW 0x77AF18 / RW 0x77C1C2 with AIData AllowForestFires: the message names the stand-in 99999999 and the point; the tree nearest that point becomes an
// object (RW 0x68449B, script selectable RW 0x6719FA) and is attacked. Without the setting the id resolves to null (RW 0x77AF67) and nothing happens.
TEST_CASE("PORT-208 force attack: a clicked tree is a target only with AllowForestFires (RW 0x77AF18, RW 0x77AF67, RW 0x68449B)")
{
	for (const bool allow : { false, true })
	{
		CAPTURE(allow);
		Port208World w;
		const int alice = w.playerIndex("Alice");
		Object *archer = w.unit("P208Archer", 'A', 300, 300);
		w.frames(2);
		const Coord3D at{ 420.0f, 300.0f, 0.0f };
		REQUIRE(w.addTree(at, 77));
		w.logic->settings().allowForestFires = allow;
		w.select(alice, { archer });
		w.forceObjectMessage(alice, 99999999u, at);
		w.frames(1);
		CHECK(w.aiCommands.stats().forceAttacks == 1);
		if (!allow)
		{
			CHECK(w.state(archer) == (unsigned)AI_IDLE); // the null victim of RW 0x77AF67
			CHECK(w.logic->trees().records().size() == 1); // the tree is still a map tree
			CHECK(w.madeTree() == nullptr);
			continue;
		}
		REQUIRE(w.madeTree() != nullptr); // RW 0x684528: the tree is an object now
		Object *made = w.madeTree();
		CHECK(made->isScriptSelectable()); // RW 0x77AF4C .. 0x77AF65 -> RW 0x6719FA(1)
		CHECK(w.logic->trees().records().at(0).clientId == 0u); // RW 0x67D3C2: the id is cleared, the record is no tree any more
		CHECK(w.state(archer) == (unsigned)AI_FORCE_ATTACK_OBJECT);
		CHECK(archer->getAIUpdateInterface()->currentVictimId() == made->getID());
		CHECK(archer->getWeapons()->canAttackObject(*made, CMD_FROM_PLAYER, true)); // RW 0x68D6A6: the tree is a legal victim
		// the fire arrow is what hurts it (RW 0x68D91A), so the archer locks SECONDARY as the mod's button does and fires at the made tree
		archer->getWeapons()->setWeaponLock(SECONDARY_WEAPON, LOCKED_TEMPORARILY);
		w.runUntil([&] { return w.health(made) < 100.0f; }, 400);
		CHECK(w.health(made) == 80.0f); // one fire arrow of 20 FLAME on a 100 health tree (RW 0x68D91A: a tree takes FLAME alone)
	}
}

// A stand-in with no tree under the point: RW 0x77AF44 / RW 0x77C1EE answer null and the command ends (no victim, no state). A garrisoned AI-less tower
// reaches the same branch, so a Ctrl+click on a tree from inside a tower works (RW 0x77214D forwards the made tree like any other victim).
TEST_CASE("PORT-208 force attack: no tree at the point is no order; an AI-less garrison attacks the made tree (RW 0x77AF44, RW 0x77214D)")
{
	Port208World w;
	const int alice = w.playerIndex("Alice");
	w.logic->settings().allowForestFires = true;
	Object *tower = w.unit("P208Tower", 'A', 300, 300);
	Object *archer = w.unit("P208Archer", 'A', 340, 300);
	w.frames(2);
	REQUIRE(tower->getContain()->addToContain(archer));
	w.select(alice, { tower });
	w.forceObjectMessage(alice, 99999999u, Coord3D{ 700.0f, 700.0f, 0.0f }); // no tree within 10 of it
	w.frames(1);
	CHECK(w.state(archer) == (unsigned)AI_IDLE);
	CHECK(w.madeTree() == nullptr);
	const Coord3D at{ 430.0f, 300.0f, 0.0f };
	REQUIRE(w.addTree(at, 78));
	w.forceObjectMessage(alice, 99999999u, at);
	w.frames(1);
	REQUIRE(w.madeTree() != nullptr);
	CHECK(w.state(archer) == (unsigned)AI_FORCE_ATTACK_OBJECT);
	CHECK(archer->getAIUpdateInterface()->currentVictimId() == w.madeTree()->getID());
}

// RW 0x66DE32: privateAttackPosition refuses an object outside the playable area (Object + 0x458 bit 3), so no state starts; back inside the map the
// same order is taken. The map's active boundary is 30 cells = 300 world units (RW 0x6936B2: lo < x < hi and lo < y < hi).
TEST_CASE("PORT-208 off the playable area: aiAttackPosition refuses an off-map object (RW 0x66DE32)")
{
	Port208MapWorld w;
	Object *archer = w.unit("P208Archer", 'A', 100, 100);
	w.frames(2);
	REQUIRE_FALSE(archer->isOffMap());
	Coord3D far{ 400.0f, 400.0f, 0.0f }; // beyond the 300 x 300 boundary
	archer->setPosition(&far);
	REQUIRE(archer->isOffMap());
	const Coord3D spot{ 200.0f, 200.0f, 0.0f };
	CHECK_FALSE(archer->getAIUpdateInterface()->aiAttackPosition(spot, 1, CMD_FROM_PLAYER));
	CHECK(w.state(archer) == (unsigned)AI_IDLE);
	Coord3D near{ 150.0f, 150.0f, 0.0f };
	archer->setPosition(&near);
	REQUIRE_FALSE(archer->isOffMap());
	CHECK(archer->getAIUpdateInterface()->aiAttackPosition(spot, 1, CMD_FROM_PLAYER));
	CHECK(w.state(archer) == (unsigned)AI_ATTACK_POSITION);
}
