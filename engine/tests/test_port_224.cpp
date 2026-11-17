// OpenBFME. GPL-3.0.
// Port 224 (issue #534, P-143): destroying a creep lair or worker-rebuilt
// structure while it is rebuilding from its RebuildHoleBehavior hole cancels
// the build (Object::friend_onDie RW 0x698F17..0x698F41), kills the masked hole
// with DEATH_FADED (0x16) on the next RebuildHoleBehavior::update
// (RW 0x886BDC..0x886C40), suppresses CreateObjectDie (DeathTypes = ALL -FADED)
// treasure drops, awards zero bounty (Economy::awardBounty RW 0x6AC09B checks
// OBJECT_STATUS_UNDER_CONSTRUCTION), and spawns no replacement hole
// (RebuildHoleExposeDie::onDie RW 0x889B0C checks OBJECT_STATUS_UNDER_CONSTRUCTION,
// retained by BuildAssistant::buildObjectNow RW 0x797894 / 0x797951..0x797977).
#include "doctest.h"
#include "CombatTestUtil.h"

#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Module/ConstructionModules.h"
#include "GameLogic/Module/ExtraDieModules.h"
#include "GameLogic/Module/RebuildHoleModules.h"
#include "GameLogic/ObjectCreationList.h"
#include "GameLogic/ProductionSettings.h"

#include <string>
#include <vector>

using namespace combattest;

namespace
{
const char kObjects[] =
	"Object Port224TreasureChest\n"
	"  KindOf = PARACHUTABLE IMMOBILE\n"
	"  Geometry = CYLINDER\n  GeometryMajorRadius = 5\n  GeometryMinorRadius = 5\n  GeometryHeight = 5\n"
	"  Body = ActiveBody B\n    MaxHealth = 10\n  End\n"
	"  Behavior = DestroyDie D\n  End\n"
	"End\n"
	"Object Port224CreepLair\n"
	"  KindOf = STRUCTURE IMMOBILE SELECTABLE\n"
	"  Geometry = CYLINDER\n  GeometryMajorRadius = 15\n  GeometryMinorRadius = 15\n  GeometryHeight = 15\n"
	"  BuildCost = 300\n  BuildTime = 2.0\n  BountyValue = 75\n"
	"  ArmorSet\n    Conditions = None\n    Armor = NoArmor\n  End\n"
	"  Body = StructureBody B\n    MaxHealth = 1000\n    InitialHealth = 1000\n  End\n"
	"  Behavior = GettingBuiltBehavior G\n    SpawnTimer = 50\n    RebuildTimeSeconds = 2.0\n  End\n"
	"  Behavior = KeepObjectDie K\n  End\n"
	"  Behavior = StructureCollapseUpdate C\n"
	"    MinCollapseDelay = 0\n    MaxCollapseDelay = 0\n    CollapseDamping = 0.5\n"
	"    MaxShudder = 0\n    MinBurstDelay = 1\n    MaxBurstDelay = 1\n    BigBurstFrequency = 1\n"
	"    DestroyObjectWhenDone = Yes\n    CollapseHeight = 10\n  End\n"
	"  Behavior = RebuildHoleExposeDie E\n"
	"    HoleName = Port224CreepHole\n    HoleMaxHealth = 500\n    TransferAttackers = No\n    FadeInTimeSeconds = 2.0\n"
	"  End\n"
	"  Behavior = DestroyDie D\n  End\n"
	"End\n"
	"Object Port224CreepHole\n"
	"  KindOf = STRUCTURE IMMOBILE SELECTABLE REBUILD_HOLE\n"
	"  Geometry = CYLINDER\n  GeometryMajorRadius = 15\n  GeometryMinorRadius = 15\n  GeometryHeight = 5\n"
	"  ArmorSet\n    Conditions = None\n    Armor = NoArmor\n  End\n"
	"  Body = StructureBody B\n    MaxHealth = 500\n    InitialHealth = 500\n  End\n"
	"  Behavior = RebuildHoleBehavior R\n"
	"    WorkerRespawnDelay = 120000\n    HoleHealthRegen%PerSecond = 0.5%\n"
	"  End\n"
	"  Behavior = CreateObjectDie DropTreasure\n"
	"    CreationList = OCL_Port224LairTreasure\n"
	"    DeathTypes = ALL -FADED\n"
	"  End\n"
	"  Behavior = DestroyDie D\n  End\n"
	"End\n"
	"Object Port224WorkerStructure\n"
	"  KindOf = STRUCTURE IMMOBILE SELECTABLE\n"
	"  Geometry = CYLINDER\n  GeometryMajorRadius = 15\n  GeometryMinorRadius = 15\n  GeometryHeight = 15\n"
	"  BuildCost = 300\n  BuildTime = 2.0\n  BountyValue = 50\n"
	"  ArmorSet\n    Conditions = None\n    Armor = NoArmor\n  End\n"
	"  Body = StructureBody B\n    MaxHealth = 200\n    InitialHealth = 200\n  End\n"
	"  Behavior = GettingBuiltBehavior G\n    WorkerName = Port224Worker\n    SpawnTimer = 100\n  End\n"
	"  Behavior = KeepObjectDie K\n  End\n"
	"  Behavior = StructureCollapseUpdate C\n"
	"    MinCollapseDelay = 0\n    MaxCollapseDelay = 0\n    CollapseDamping = 0.5\n"
	"    MaxShudder = 0\n    MinBurstDelay = 1\n    MaxBurstDelay = 1\n    BigBurstFrequency = 1\n"
	"    DestroyObjectWhenDone = Yes\n    CollapseHeight = 10\n  End\n"
	"  Behavior = RebuildHoleExposeDie E\n"
	"    HoleName = Port224WorkerHole\n    HoleMaxHealth = 500\n    TransferAttackers = No\n    FadeInTimeSeconds = 0\n"
	"  End\n"
	"  Behavior = DestroyDie D\n  End\n"
	"End\n"
	"Object Port224WorkerHole\n"
	"  KindOf = STRUCTURE IMMOBILE SELECTABLE REBUILD_HOLE\n"
	"  Geometry = CYLINDER\n  GeometryMajorRadius = 15\n  GeometryMinorRadius = 15\n  GeometryHeight = 5\n"
	"  ArmorSet\n    Conditions = None\n    Armor = NoArmor\n  End\n"
	"  Body = StructureBody B\n    MaxHealth = 500\n    InitialHealth = 500\n  End\n"
	"  Behavior = RebuildHoleBehavior R\n"
	"    WorkerObjectName = Port224Worker\n    WorkerRespawnDelay = 1000\n    HoleHealthRegen%PerSecond = 0.5%\n"
	"  End\n"
	"  Behavior = CreateObjectDie DropTreasure\n"
	"    CreationList = OCL_Port224LairTreasure\n"
	"    DeathTypes = ALL -FADED\n"
	"  End\n"
	"  Behavior = DestroyDie D\n  End\n"
	"End\n"
	"Object Port224Worker\n"
	"  KindOf = INFANTRY SELECTABLE DOZER\n"
	"  Geometry = CYLINDER\n  GeometryMajorRadius = 2\n  GeometryMinorRadius = 2\n  GeometryHeight = 5\n"
	"  ArmorSet\n    Conditions = None\n    Armor = NoArmor\n  End\n"
	"  Body = ActiveBody B\n    MaxHealth = 100\n  End\n"
	"  Behavior = WorkerAIUpdate W\n    MaxBoxes = 0\n  End\n"
	"  LocomotorSet\n    Locomotor = WalkerLoco\n    Condition = SET_NORMAL\n    Speed = 1000\n  End\n"
	"  Behavior = DestroyDie D\n  End\n"
	"End\n";

const char kCreationLists[] =
	"ObjectCreationList OCL_Port224LairTreasure\n"
	"  CreateObject\n"
	"    ObjectNames = Port224TreasureChest\n"
	"  End\n"
	"End\n";

const char kProductionGameData[] =
	"GameData\n"
	"  MinLowEnergyProductionSpeed = 1\n  MaxLowEnergyProductionSpeed = 1\n  LowEnergyPenaltyModifier = 1\n  MultipleFactory = 1\n"
	"  MultiPlayUnitSpeedMult = MP1:1 MP2:1 MP3:1 MP4:1 MP5:1 MP6:1 MP7:1 MP8:1\n"
	"  MultiPlayBuildingSpeedMult = MP1:1 MP2:1 MP3:1 MP4:1 MP5:1 MP6:1 MP7:1 MP8:1\n"
	"End\n";

struct Port224World : CombatWorld
{
	ObjectCreationListStore oclStore{ w.keys };
	ObjectCreationListStore *savedOclStore = TheObjectCreationListStore;

	Port224World()
		: CombatWorld(kObjects)
	{
		TheObjectCreationListStore = &oclStore;
		w.fx.env.blocks.registerBlock("ObjectCreationList", [](INI *ini) { ObjectCreationListStore::parseObjectCreationListDefinitionGlobal(ini); });
		std::string err = w.load(kCreationLists, INI_LOAD_OVERWRITE, "objectcreationlist.ini");
		REQUIRE_MESSAGE(err.empty(), err);
		REQUIRE_MESSAGE(ProductionSettings::scan(kProductionGameData, logic->productionSettings(), &err), err);
		playerOf('A')->setBountyPercent(1.0f);
	}
	~Port224World() { TheObjectCreationListStore = savedOclStore; }

	std::vector<Object *> alive(const char *name)
	{
		std::vector<Object *> out;
		for (Object *o = logic->getFirstObject(); o; o = o->getNextObject())
		{
			if (!o->isDestroyed() && !o->isEffectivelyDead() && o->getTemplate()->getName() == name)
			{
				out.push_back(o);
			}
		}
		return out;
	}
};

DamageInfo lethalHit(Object *attacker, float amount = 5000.0f)
{
	DamageInfo d;
	d.m_input.m_sourceID = attacker ? attacker->getID() : INVALID_ID;
	d.m_input.m_amount = amount;
	d.m_input.m_damageType = DAMAGE_SLASH;
	d.m_input.m_deathType = DEATH_NORMAL;
	return d;
}

bool status(const Object &o, const char *name)
{
	const int bit = ObjectTemplateInfoBuilder::objectStatusIndex(name);
	REQUIRE(bit >= 0); // tolerance: bit index lookup in status table
	return o.testStatus((unsigned)bit);
}

bool condition(const Object &o, const char *name)
{
	return o.testModelCondition(CombatNames::modelCondition(name));
}
} // namespace

TEST_CASE("PORT-224 creep lair hole: destroying the lair while it rises from its hole destroys both without bounty or treasure crate")
{
	Port224World w;
	Object *attacker = w.unit("Swordsman", 'A', 300.0f, 300.0f);
	REQUIRE(attacker != nullptr);

	// Baseline check on the hole's CreateObjectDie (DeathTypes = ALL -FADED):
	// killing the hole directly before rebuild begins drops the treasure chest.
	Object *firstLair = w.unit("Port224CreepLair", 'B', 360.0f, 300.0f);
	REQUIRE(firstLair != nullptr);
	DamageInfo killFirst = lethalHit(attacker);
	firstLair->attemptDamage(killFirst);
	w.frames(1);
	std::vector<Object *> holes = w.alive("Port224CreepHole");
	REQUIRE(holes.size() == 1);
	DamageInfo killHole = lethalHit(attacker);
	holes[0]->attemptDamage(killHole);
	w.frames(1);
	CHECK(w.alive("Port224CreepHole").empty());
	CHECK(w.alive("Port224TreasureChest").size() == 1);

	// Main Port 224 scenario: the creep lair dies, spawns its hole, waits the
	// full 600-frame WorkerRespawnDelay, starts self-rising under construction,
	// and is destroyed mid-rise.
	Object *lair = w.unit("Port224CreepLair", 'B', 420.0f, 420.0f);
	REQUIRE(lair != nullptr);
	DamageInfo killLair = lethalHit(attacker);
	lair->attemptDamage(killLair);
	w.frames(1);
	holes = w.alive("Port224CreepHole");
	REQUIRE(holes.size() == 1);
	Object *hole = holes[0];
	RebuildHoleBehavior *rh = RebuildHoleBehavior::of(*hole);
	REQUIRE(rh != nullptr);
	CHECK(rh->workerWaitCounter() == 599u);

	// Wait the remaining 598 frames before creation, then step onto frame 600.
	w.frames(598);
	CHECK(rh->getReconstructedBuildingID() == INVALID_ID);
	w.frames(1);
	Object *rebuilt = w.byId(rh->getReconstructedBuildingID());
	REQUIRE(rebuilt != nullptr);
	// RW 0x797894 / 0x886994..0x886A08: buildObjectNow keeps UNDER_CONSTRUCTION,
	// RebuildHoleBehavior sets health to 1% (10 of 1000), starts GettingBuiltBehavior,
	// sets PARTIALLY_CONSTRUCTED and ACTIVELY_BEING_CONSTRUCTED, and masks the hole.
	CHECK(rebuilt->isUnderConstruction());
	CHECK(status(*rebuilt, "UNDER_CONSTRUCTION"));
	CHECK_FALSE(status(*rebuilt, "BUILD_BEING_CANCELED"));
	CHECK(condition(*rebuilt, "PARTIALLY_CONSTRUCTED"));
	CHECK(condition(*rebuilt, "ACTIVELY_BEING_CONSTRUCTED"));
	CHECK(rebuilt->getBodyModule()->getHealth() == 10.0f);
	CHECK(hole->isMasked());
	CHECK_FALSE(hole->isEffectivelyDead());

	// Advance 3 frames mid-rebuild (3 * 100 = +300 health -> 310 of 1000 = 31%).
	w.frames(3);
	CHECK(rebuilt->isUnderConstruction());
	CHECK(condition(*rebuilt, "ACTIVELY_BEING_CONSTRUCTED"));
	CHECK(rebuilt->getBodyModule()->getHealth() == 310.0f);
	CHECK_FALSE(hole->isEffectivelyDead());

	const int cashBefore = w.cash('A');
	const std::size_t chestsBefore = w.alive("Port224TreasureChest").size();
	DamageInfo killMidRebuild = lethalHit(attacker);
	rebuilt->attemptDamage(killMidRebuild);

	// RW 0x698F17..0x698F41: Object::friend_onDie sees ACTIVELY_BEING_CONSTRUCTED,
	// sets OBJECT_STATUS_BUILD_BEING_CANCELED (0x56), and clears condition 0x45.
	// RW 0x6AC09B: Economy::awardBounty skips objects with UNDER_CONSTRUCTION.
	// RW 0x889B0C: RebuildHoleExposeDie::onDie skips objects with UNDER_CONSTRUCTION.
	CHECK(rebuilt->isEffectivelyDead());
	CHECK(status(*rebuilt, "BUILD_BEING_CANCELED"));
	CHECK_FALSE(condition(*rebuilt, "ACTIVELY_BEING_CONSTRUCTED"));
	CHECK(w.cash('A') == cashBefore);
	CHECK(w.alive("Port224CreepHole").size() == 1);

	// On the next frame, RebuildHoleBehavior::update (RW 0x886BDC..0x886C40) sees
	// BUILD_BEING_CANCELED and kills the masked hole with DEATH_FADED (0x16),
	// which is excluded by CreateObjectDie's DeathTypes = ALL -FADED.
	w.frames(1);
	CHECK(hole->isEffectivelyDead());
	CHECK(w.alive("Port224CreepHole").empty());
	CHECK(w.alive("Port224CreepLair").empty());
	CHECK(w.alive("Port224TreasureChest").size() == chestsBefore);
	CHECK(w.cash('A') == cashBefore);

	// Run past collapse and another full 600-frame respawn window: nothing returns.
	w.frames(620);
	CHECK(w.alive("Port224CreepHole").empty());
	CHECK(w.alive("Port224CreepLair").empty());
	CHECK(w.alive("Port224TreasureChest").size() == chestsBefore);
	CHECK(w.cash('A') == cashBefore);
}

TEST_CASE("PORT-224 worker hole neighbor: destroying a worker-rebuilt structure mid-construction cancels the build, destroys the worker, and fades the hole")
{
	Port224World w;
	Object *attacker = w.unit("Swordsman", 'A', 300.0f, 300.0f);
	Object *structure = w.unit("Port224WorkerStructure", 'B', 440.0f, 440.0f);
	REQUIRE(attacker != nullptr);
	REQUIRE(structure != nullptr);

	DamageInfo killInitial = lethalHit(attacker);
	structure->attemptDamage(killInitial);
	w.frames(1);
	std::vector<Object *> holes = w.alive("Port224WorkerHole");
	REQUIRE(holes.size() == 1);
	Object *hole = holes[0];
	RebuildHoleBehavior *rh = RebuildHoleBehavior::of(*hole);
	REQUIRE(rh != nullptr);

	// WorkerRespawnDelay = 1000 ms = 5 frames (4 remaining after the first update).
	w.frames(4);
	Object *rebuilt = w.byId(rh->getReconstructedBuildingID());
	Object *worker = w.byId(rh->getWorkerID());
	REQUIRE(rebuilt != nullptr);
	REQUIRE(worker != nullptr);
	CHECK(rebuilt->isUnderConstruction());
	CHECK(status(*rebuilt, "RECONSTRUCTING"));
	CHECK(condition(*rebuilt, "ACTIVELY_BEING_CONSTRUCTED"));
	CHECK(hole->isMasked());

	// Step 2 frames into worker reconstruction, then destroy the rebuilding structure.
	w.frames(2);
	REQUIRE(rebuilt->isUnderConstruction());
	const int cashBefore = w.cash('A');
	DamageInfo killMidRebuild = lethalHit(attacker);
	rebuilt->attemptDamage(killMidRebuild);

	CHECK(rebuilt->isEffectivelyDead());
	CHECK(status(*rebuilt, "BUILD_BEING_CANCELED"));
	CHECK_FALSE(condition(*rebuilt, "ACTIVELY_BEING_CONSTRUCTED"));
	CHECK(w.cash('A') == cashBefore);

	// Next frame: RebuildHoleBehavior::update (RW 0x886C20..0x886C40) destroys the
	// hole's worker and fades the masked hole without treasure.
	w.frames(1);
	CHECK(hole->isEffectivelyDead());
	CHECK(w.alive("Port224Worker").empty());
	CHECK(w.alive("Port224WorkerHole").empty());
	CHECK(w.alive("Port224WorkerStructure").empty());
	CHECK(w.alive("Port224TreasureChest").empty());

	// Run past structure collapse and respawn delay: hole stays gone.
	w.frames(30);
	CHECK(w.alive("Port224Worker").empty());
	CHECK(w.alive("Port224WorkerHole").empty());
	CHECK(w.alive("Port224WorkerStructure").empty());
	CHECK(w.alive("Port224TreasureChest").empty());
}

TEST_CASE("PORT-224 completed rebuild neighbor: a creep lair that finishes rising fades its old hole without treasure and leaves a new hole when killed later")
{
	Port224World w;
	Object *attacker = w.unit("Swordsman", 'A', 300.0f, 300.0f);
	Object *lair = w.unit("Port224CreepLair", 'B', 460.0f, 460.0f);
	REQUIRE(attacker != nullptr);
	REQUIRE(lair != nullptr);
	lair->setName("CreepLairScriptName");

	DamageInfo killInitial = lethalHit(attacker);
	lair->attemptDamage(killInitial);
	w.frames(1);
	std::vector<Object *> holes = w.alive("Port224CreepHole");
	REQUIRE(holes.size() == 1);
	Object *hole = holes[0];
	RebuildHoleBehavior *rh = RebuildHoleBehavior::of(*hole);
	REQUIRE(rh != nullptr);

	// Advance 599 frames to spawn the self-rising lair, then 10 frames for the
	// 2.0-second self-rise to reach 1000/1000 health and 1 frame for the hole to fade.
	w.frames(599);
	Object *rebuilt = w.byId(rh->getReconstructedBuildingID());
	REQUIRE(rebuilt != nullptr);
	CHECK(rebuilt->isUnderConstruction());
	w.frames(10);
	CHECK_FALSE(rebuilt->isUnderConstruction());
	CHECK(rebuilt->getBodyModule()->getHealth() == 1000.0f);
	w.frames(1);
	CHECK(hole->isEffectivelyDead());
	CHECK(w.alive("Port224CreepHole").empty());
	CHECK(w.alive("Port224TreasureChest").empty());
	CHECK(rebuilt->getName() == "CreepLairScriptName");

	// Killing the completed lair awards its BountyValue (75) and spawns a fresh hole.
	const int cashBefore = w.cash('A');
	DamageInfo killCompleted = lethalHit(attacker);
	rebuilt->attemptDamage(killCompleted);
	w.frames(1);
	CHECK(w.cash('A') == cashBefore + 75);
	CHECK(w.alive("Port224CreepHole").size() == 1);
}
