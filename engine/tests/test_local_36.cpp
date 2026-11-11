// OpenBFME unit tests: LOCAL-36 building behaviours as retail (BaseUpgrade, DamageFieldUpdate, CivilianSpawnUpdate, CivilianSpawnCollide, issue #286).
//
// Target facts and RotWK 2.01 / BFME2 1.06 addresses:
//   * BaseUpgrade:
//     - Module registration: RW 0x65A28A (create RW 0x64F98E, createData RW 0x64F9CA) / DONOR BFME2 0x659074.
//     - Data ctor RW 0x8B7885 / DONOR BFME2 0x4B3698, buildFieldParse RW 0x8B7863 / DONOR BFME2 0x4B3670,
//       table RW 0xC6DEB8 / DONOR BFME2 0xB48288 (BuildingTemplateName +0x138, PlacementPrefix +0x13C, PlacementIndex +0x140).
//     - Runtime ctor RW 0x8B7885 / DONOR BFME2 0x4B371C, upgradeImplementation RW 0x8B7933 / DONOR BFME2 0x4B37A0 (BaseUpgrade.cpp:332),
//       xfer RW 0x8B7880 / DONOR BFME2 0x4B3640 (version 1, 1).
//   * DamageFieldUpdate:
//     - Module registration: RW 0x659082 (create RW 0x64DE32, createData RW 0x64DE6D) / DONOR BFME2 0x6577D5.
//     - Data ctor RW 0x89478E / DONOR BFME2 0x49114E, buildFieldParse RW 0x894773 / DONOR BFME2 0x491040,
//       table RW 0xC64018 / DONOR BFME2 0xB44A00 (Radius +0x10, ObjectFilter +0x14, RequiredUpgrade +0x18).
//     - Runtime ctor RW 0x894935 / DONOR BFME2 0x491220, fireExtraWeapon RW 0x89483A / DONOR BFME2 0x4911AE,
//       update RW 0x8949A2 / DONOR BFME2 0x491316 (DamageFieldUpdateRva004911AE.cpp:195),
//       xfer RW 0x894C52 / DONOR BFME2 0x49157D (version 1, 1).
//   * CivilianSpawnUpdate:
//     - Module registration: RW 0x6593F7 (create RW 0x64E245, createData RW 0x64E27D) / DONOR BFME2 0x65824C.
//     - Data ctor RW 0x882C31 / DONOR BFME2 0x47FA55, buildFieldParse RW 0x882C20 / DONOR BFME2 0x47F9C3,
//       table RW 0xC5EC68 / DONOR BFME2 0xB42570 (SpawnDelayTime +0x8, MaximumDistance +0x10, RunToFilter +0xC, Civilian +0x14).
//     - Runtime ctor RW 0x882C31 / DONOR BFME2 0x47F9D4, update RW 0x882D5B / DONOR BFME2 0x47FAFE (CivilianSpawnUpdate.cpp:306),
//       xfer RW 0x89A1AC / DONOR BFME2 0x47FA40 (version 1, 2).
//   * CivilianSpawnCollide:
//     - Module registration: RW 0x65B091 (create RW 0x650DEB, createData RW 0x650E23) / DONOR BFME2 0x659C97.
//     - Data ctor RW 0x8C17CC / DONOR BFME2 0x4BD651, buildFieldParse RW 0x8C17CC / DONOR BFME2 0x4BD683,
//       table RW 0xC71748 / DONOR BFME2 0xB4A5B8 (DeleteObjectFilter +0x8).
//     - Runtime ctor RW 0x8C17DD / DONOR BFME2 0x4BD694, onCollide RW 0x8C1823 / DONOR BFME2 0x4BD6E0 (CivilianSpawnCollide.cpp:162),
//       wouldLikeToCollideWith RW 0x63F3BF (false), xfer RW 0x8C651D.

#include "doctest.h"
#include "CombatTestUtil.h"

#include "Common/Player.h"
#include "Common/StateHash.h"
#include "Common/Upgrade.h"
#include "Common/XferLoad.h"
#include "Common/XferSave.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Combat/CombatQueries.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/BaseUpgrade.h"
#include "GameLogic/Module/CivilianSpawnCollide.h"
#include "GameLogic/Module/CivilianSpawnUpdate.h"
#include "GameLogic/Module/DamageFieldUpdate.h"
#include "GameLogic/Object/Object.h"

#include <vector>

namespace {

const char kLocal36Ini[] =
	"Upgrade UpgradeTestMordorBase\n"
	"  Type = OBJECT\n"
	"End\n"

	"Upgrade UpgradeTestDamageField\n"
	"  Type = OBJECT\n"
	"End\n"

	"Weapon TestDamageFieldWeapon\n"
	"  AttackRange = 100.0\n"
	"  DamageNugget\n"
	"    Damage = 25\n"
	"    DamageType = MAGIC\n"
	"    DeathType = NORMAL\n"
	"  End\n"
	"End\n"

	"Object TestSpawningBase\n"
	"  KindOf = STRUCTURE SELECTABLE IMMOBILE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 30\n"
	"  GeometryMinorRadius = 30\n"
	"  GeometryHeight = 40\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 1000\n"
	"  End\n"
	"  Behavior = BaseUpgrade ModuleTag_BaseUpgrade\n"
	"    TriggeredBy = UpgradeTestMordorBase\n"
	"    BuildingTemplateName = TestSpawnedBuilding\n"
	"    PlacementPrefix = BONE_\n"
	"    PlacementIndex = 0\n"
	"  End\n"
	"End\n"

	"Object TestSpawnedBuilding\n"
	"  KindOf = STRUCTURE SELECTABLE IMMOBILE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryMinorRadius = 20\n"
	"  GeometryHeight = 30\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"End\n"

	"Object TestDamageFieldStructure\n"
	"  KindOf = STRUCTURE SELECTABLE IMMOBILE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryMinorRadius = 20\n"
	"  GeometryHeight = 30\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 1000\n"
	"  End\n"
	"  Behavior = DamageFieldUpdate ModuleTag_DamageField\n"
	"    Radius = 150.0\n"
	"    RequiredUpgrade = UpgradeTestDamageField\n"
	"    ObjectFilter = NONE +INFANTRY -STRUCTURE ENEMIES\n"
	"    FireWeaponNugget\n"
	"      WeaponName = TestDamageFieldWeapon\n"
	"      FireDelay = 0\n"
	"      OneShot = No\n"
	"    End\n"
	"  End\n"
	"End\n"

	"Object TestVictimInfantry\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"End\n"

	"Object TestCivilianHouse\n"
	"  KindOf = STRUCTURE SELECTABLE IMMOBILE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 25\n"
	"  GeometryMinorRadius = 25\n"
	"  GeometryHeight = 30\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = CivilianSpawnUpdate ModuleTag_CivilianSpawn\n"
	"    SpawnDelayTime = 100\n"
	"    MaximumDistance = 400.0\n"
	"    RunToFilter = NONE +STRUCTURE ALLIES\n"
	"    Civilian = TestFleeingCivilian\n"
	"  End\n"
	"End\n"

	"Object TestFleeingCivilian\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 5\n"
	"  GeometryMinorRadius = 5\n"
	"  GeometryHeight = 15\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 50\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"    AutoAcquireEnemiesWhenIdle = No\n"
	"  End\n"
	"  Behavior = PhysicsBehavior ModuleTag_Physics\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 40\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_PANIC\n"
	"    Speed = 70\n"
	"  End\n"
	"End\n"

	"Object TestCivilianDestination\n"
	"  KindOf = STRUCTURE SELECTABLE IMMOBILE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 25\n"
	"  GeometryMinorRadius = 25\n"
	"  GeometryHeight = 30\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = CivilianSpawnCollide ModuleTag_CivilianCollide\n"
	"    DeleteObjectFilter = NONE +INFANTRY ALLIES\n"
	"  End\n"
	"End\n";

struct Local36Rig
{
	combattest::CombatWorld world;

	Local36Rig()
		: world()
	{
		BaseUpgrade::registerClass(world.w.modules);
		DamageFieldUpdate::registerClass(world.w.modules);
		CivilianSpawnUpdate::registerClass(world.w.modules);
		CivilianSpawnCollide::registerClass(world.w.modules);

		std::string err = world.w.load(kLocal36Ini, INI_LOAD_OVERWRITE, "local36_test.ini");
		REQUIRE_MESSAGE(err.empty(), err);
	}
};

} // namespace

TEST_CASE("LOCAL-36: BaseUpgrade spawns building template upon upgrade grant (RW 0x8B7933)")
{
	Local36Rig rig;
	Object *spawner = rig.world.unit("TestSpawningBase", 'A', 100.0f, 100.0f);
	REQUIRE(spawner != nullptr);

	auto *bu = dynamic_cast<BaseUpgrade *>(spawner->findModule("BaseUpgrade"));
	REQUIRE(bu != nullptr);
	CHECK(!bu->isAlreadyUpgraded());

	// Spawner has not received UpgradeTestMordorBase yet
	const UpgradeTemplate *upg = TheUpgradeCenter ? TheUpgradeCenter->findUpgrade("UpgradeTestMordorBase") : nullptr;
	REQUIRE(upg != nullptr);
	CHECK(!spawner->hasUpgrade(upg));

	// Grant the upgrade to spawner
	spawner->giveUpgradeSelf(upg);
	CHECK(spawner->hasUpgrade(upg));
	CHECK(bu->isAlreadyUpgraded());

	// Verify that the new structure was spawned
	bool foundSpawned = false;
	for (Object *o = rig.world.logic->getFirstObject(); o; o = o->getNextObject())
	{
		if (o->getTemplate()->getName() == "TestSpawnedBuilding")
		{
			foundSpawned = true;
			CHECK(o->getProducerID() == spawner->getID());
			CHECK(o->getPosition()->x == 100.0f);
			CHECK(o->getPosition()->y == 100.0f);
			CHECK(o->getControllingPlayer() == spawner->getControllingPlayer());
			break;
		}
	}
	CHECK(foundSpawned);

	// Xfer roundtrip on BaseUpgrade
	XferSave save;
	bu->xfer(save);

	XferLoad load(save.data());
	bu->xfer(load);
	CHECK(bu->isAlreadyUpgraded());
}

TEST_CASE("LOCAL-36: DamageFieldUpdate requires upgrade, hits entering enemies, clears exited (RW 0x8949A2)")
{
	Local36Rig rig;
	Object *fortress = rig.world.unit("TestDamageFieldStructure", 'A', 200.0f, 200.0f);
	REQUIRE(fortress != nullptr);

	auto *df = dynamic_cast<DamageFieldUpdate *>(fortress->findModule("DamageFieldUpdate"));
	REQUIRE(df != nullptr);

	// Spawn enemy infantry at (250, 200) - distance 50 from fortress (within Radius 150)
	Object *enemy = rig.world.unit("TestVictimInfantry", 'B', 250.0f, 200.0f);
	REQUIRE(enemy != nullptr);
	auto *enemyBody = dynamic_cast<ActiveBody *>(enemy->getBodyModule());
	REQUIRE(enemyBody != nullptr);
	CHECK(enemyBody->getHealth() == 100.0f);

	// Fortress lacks RequiredUpgrade ("UpgradeTestDamageField"): update must sleep and fire nothing
	UpdateSleepTime sleep = df->update();
	CHECK(sleep == UPDATE_SLEEP_NONE);
	CHECK(enemyBody->getHealth() == 100.0f);
	CHECK(df->victims().empty());

	// Grant RequiredUpgrade to fortress
	const UpgradeTemplate *upg = TheUpgradeCenter ? TheUpgradeCenter->findUpgrade("UpgradeTestDamageField") : nullptr;
	REQUIRE(upg != nullptr);
	fortress->giveUpgradeSelf(upg);
	CHECK(fortress->hasUpgrade(upg));

	// Advance one frame: enemy is newly detected within damage field and fired upon
	rig.world.frames(1);
	CHECK(enemyBody->getHealth() == 75.0f); // Took 25 magic damage (100 - 25 = 75)
	CHECK(df->victims().size() == 1);
	CHECK(df->victims()[0] == enemy->getID());

	// Advance another frame while enemy remains in range: should NOT fire again immediately (already in victims)
	rig.world.frames(1);
	CHECK(enemyBody->getHealth() == 75.0f);
	CHECK(df->victims().size() == 1);

	// Move enemy out of range (distance 250 > 150)
	Coord3D farPos{ 450.0f, 200.0f, 0.0f };
	enemy->setPosition(&farPos);

	// Advance frame: enemy exited range and is removed from victims
	rig.world.frames(1);
	CHECK(df->victims().empty());

	// Move enemy back into range (distance 50 < 150)
	Coord3D nearPos{ 250.0f, 200.0f, 0.0f };
	enemy->setPosition(&nearPos);

	// Advance frame: hits re-entering enemy again
	rig.world.frames(1);
	CHECK(enemyBody->getHealth() == 50.0f); // Took another 25 magic damage (75 - 25 = 50)
	CHECK(df->victims().size() == 1);
	CHECK(df->victims()[0] == enemy->getID());

	// Friendly unit inside field is NOT hit (filter is ENEMIES)
	Object *ally = rig.world.unit("TestVictimInfantry", 'A', 240.0f, 200.0f);
	REQUIRE(ally != nullptr);
	auto *allyBody = dynamic_cast<ActiveBody *>(ally->getBodyModule());
	REQUIRE(allyBody != nullptr);
	CHECK(allyBody->getHealth() == 100.0f);

	rig.world.frames(1);
	CHECK(allyBody->getHealth() == 100.0f);

	// StateHasher CRC and SnapshotXfer roundtrip
	StateHasher h1;
	df->crc(h1);

	XferSave save;
	df->xfer(save);

	XferLoad load(save.data());
	df->xfer(load);

	StateHasher h2;
	df->crc(h2);
	CHECK(h1.value() == h2.value());
}

TEST_CASE("LOCAL-36: CivilianSpawnUpdate spawns panicking civilian and directs to destination (RW 0x882D5B)")
{
	Local36Rig rig;
	Object *house = rig.world.unit("TestCivilianHouse", 'A', 100.0f, 100.0f);
	REQUIRE(house != nullptr);

	Object *dest = rig.world.unit("TestCivilianDestination", 'A', 200.0f, 100.0f);
	REQUIRE(dest != nullptr);

	auto *csu = dynamic_cast<CivilianSpawnUpdate *>(house->findModule("CivilianSpawnUpdate"));
	REQUIRE(csu != nullptr);
	CHECK(csu->spawnFrame() == 1);

	// Count initial civilians
	int initialCivilians = 0;
	for (Object *o = rig.world.logic->getFirstObject(); o; o = o->getNextObject())
	{
		if (o->getTemplate()->getName() == "TestFleeingCivilian")
		{
			++initialCivilians;
		}
	}
	CHECK(initialCivilians == 0);

	// Update spawns a civilian and schedules next spawn at SpawnDelayTime (100 ms = 1 frame)
	UpdateSleepTime sleep = csu->update();
	CHECK(csu->spawnFrame() == 1);
	CHECK(sleep == (UpdateSleepTime)1);

	Object *spawnedCivilian = nullptr;
	for (Object *o = rig.world.logic->getFirstObject(); o; o = o->getNextObject())
	{
		if (o->getTemplate()->getName() == "TestFleeingCivilian")
		{
			spawnedCivilian = o;
			break;
		}
	}
	REQUIRE(spawnedCivilian != nullptr);

	// Check model condition PANICKING
	static const int kPanicking = CombatNames::modelCondition("PANICKING");
	CHECK(spawnedCivilian->testModelCondition(kPanicking));

	// Check AI has panic locomotor set 4
	AIUpdateInterface *ai = spawnedCivilian->getAIUpdateInterface();
	REQUIRE(ai != nullptr);

	// SnapshotXfer roundtrip
	XferSave save;
	csu->xfer(save);

	XferLoad load(save.data());
	csu->xfer(load);
}

TEST_CASE("LOCAL-36: CivilianSpawnCollide destroys colliding objects matching DeleteObjectFilter (RW 0x8C1823)")
{
	Local36Rig rig;
	Object *dest = rig.world.unit("TestCivilianDestination", 'A', 150.0f, 150.0f);
	REQUIRE(dest != nullptr);

	auto *csc = dynamic_cast<CivilianSpawnCollide *>(dest->findModule("CivilianSpawnCollide"));
	REQUIRE(csc != nullptr);
	CHECK(csc->getCollide() != nullptr);
	CHECK(!csc->wouldLikeToCollideWith(*dest));

	// Create friendly civilian (matches DeleteObjectFilter: +INFANTRY ALLIES)
	Object *civilian = rig.world.unit("TestFleeingCivilian", 'A', 150.0f, 150.0f);
	REQUIRE(civilian != nullptr);
	CHECK(!civilian->isDestroyed());

	// Collide: civilian should be destroyed
	csc->onCollide(civilian, nullptr, nullptr);
	CHECK(civilian->isDestroyed());

	// Create enemy unit (team B: does not match DeleteObjectFilter ALLIES)
	Object *enemy = rig.world.unit("TestVictimInfantry", 'B', 150.0f, 150.0f);
	REQUIRE(enemy != nullptr);
	CHECK(!enemy->isDestroyed());

	// Collide: enemy must NOT be destroyed
	csc->onCollide(enemy, nullptr, nullptr);
	CHECK(!enemy->isDestroyed());

	// SnapshotXfer roundtrip
	XferSave save;
	csc->xfer(save);

	XferLoad load(save.data());
	csc->xfer(load);
}

TEST_CASE("LOCAL-36: ModuleFactory registration and module coverage counts")
{
	Local36Rig rig;
	ModuleFactory &factory = rig.world.w.modules;

	const auto *bu = factory.findModuleTemplate("BaseUpgrade", MODULETYPE_BEHAVIOR);
	REQUIRE(bu != nullptr);
	CHECK(bu->portedModule);
	CHECK(bu->typed);

	const auto *df = factory.findModuleTemplate("DamageFieldUpdate", MODULETYPE_BEHAVIOR);
	REQUIRE(df != nullptr);
	CHECK(df->portedModule);
	CHECK(df->typed);

	const auto *csu = factory.findModuleTemplate("CivilianSpawnUpdate", MODULETYPE_BEHAVIOR);
	REQUIRE(csu != nullptr);
	CHECK(csu->portedModule);
	CHECK(csu->typed);

	const auto *csc = factory.findModuleTemplate("CivilianSpawnCollide", MODULETYPE_BEHAVIOR);
	REQUIRE(csc != nullptr);
	CHECK(csc->portedModule);
	CHECK(csc->typed);
}
