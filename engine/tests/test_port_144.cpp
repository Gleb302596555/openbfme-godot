// OpenBFME. PORT-144 (issue #361, P-79: rebuilding from ruins). The goblin hole's rebuild on synthetic data that carries the module set of the Wild
// structures and of WildStructureHole: the structure has GettingBuiltBehavior with a worker and a spawn timer, KeepObjectDie, StructureCollapseUpdate with
// DestroyObjectWhenDone and RebuildHoleExposeDie; the hole has RebuildHoleBehavior with a worker, a 120000 ms delay and a 0.5 % regeneration, and a
// SlowDeathBehavior that sinks it.
//
// The expectations are RotWK 2.01's (game.dat, read with tools/mod/rw.py):
//   * RW 0x889AAF (RebuildHoleExposeDie::onDie) calls the hole's rebuild interface slot 0 (RW 0x889BF2 -> RW 0x886831), which ends in RW 0x8865F7: the
//     counter (+ 0x34) becomes ftol(WorkerRespawnDelay) when the hole is MADE (RW 0x886620 .. 0x886628). 120000 ms = 600 logic frames.
//   * RW 0x8868C2 (update): the counter goes down by one a frame while there is no worker (RW 0x88692A .. 0x886943). No branch of the function reads
//     damage: a hole that is hit keeps counting.
//   * At 0 the worker is made and constructs the structure (RW 0x8AD6A4: UNDER_CONSTRUCTION, the rebuild status, percent 0); the hole is masked (RW
//     0x68BEB1(1)), gets the statuses 3 and 0x3C and is not selectable (RW 0x886B4D .. 0x886B72). Nothing there hides or removes the hole: RW 0x68BE3C
//     -> Drawable RW 0x6719FA deselects it and clears the pick bit of its render objects (draw interface slot 0x60, RW 0x4B2D5E: collision type bit 4).
//   * The hole dies (RW 0x886C40: kill(8, 0x16)) in the first update that finds the structure no longer UNDER_CONSTRUCTION (RW 0x886BEC .. 0x886BF7).
//   * The percent over the structure is Object + 0x288 (RW 0x677CF9: UNDER_CONSTRUCTION (status 2), not SOLD (0x13), not effectively dead; the text
//     CONTROLBAR:UnderConstructionDesc RW 0xC11144; the rebuild status is not read).
#include "doctest.h"
#include "CombatTestUtil.h"

#include "GameLogic/Damage.h"
#include "GameLogic/Module/RebuildHoleModules.h"
#include "GameLogic/ObjectTemplateInfo.h"

#include <string>
#include <vector>

using namespace combattest;

namespace
{
const char *kPort144Objects =
	"Object Port144Worker\n"
	"  KindOf = INFANTRY DOZER UNATTACKABLE NOT_AUTOACQUIRABLE\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = WorkerAIUpdate ModuleTag_AI\n"
	"    RepairHealthPercentPerSecond = 0.2%\n"
	"    BoredTime = 5000\n"
	"  End\n"
	"End\n"
	"Object Port144Cave\n"
	"  KindOf = STRUCTURE SELECTABLE IMMOBILE\n"
	"  BuildCost = 300\n"
	"  BuildTime = 10.0\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryMinorRadius = 20\n"
	"  GeometryHeight = 20\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = NoArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 1000\n"
	"  End\n"
	"  Behavior = GettingBuiltBehavior ModuleTag_GetBuilt\n"
	"    WorkerName = Port144Worker\n"
	"    SpawnTimer = 100\n"
	"  End\n"
	"  Behavior = KeepObjectDie ModuleTag_IWantRubble\n"
	"  End\n"
	"  Behavior = StructureCollapseUpdate ModuleTag_Collapse\n"
	"    MinCollapseDelay = 000\n"
	"    MaxCollapseDelay = 000\n"
	"    CollapseDamping = .5\n"
	"    MaxShudder = 0.6\n"
	"    MinBurstDelay = 250\n"
	"    MaxBurstDelay = 800\n"
	"    BigBurstFrequency = 4\n"
	"    DestroyObjectWhenDone = Yes\n"
	"    CollapseHeight = 0\n"
	"  End\n"
	"  Behavior = RebuildHoleExposeDie ModuleTag_ExposeDie\n"
	"    ExemptStatus = SOLD\n"
	"    HoleName = Port144Hole\n"
	"    HoleMaxHealth = 500.0\n"
	"    FadeInTimeSeconds = 2.0\n"
	"  End\n"
	"End\n"
	"Object Port144Hole\n"
	"  KindOf = STRUCTURE SELECTABLE IMMOBILE REBUILD_HOLE NOT_AUTOACQUIRABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 25\n"
	"  GeometryMinorRadius = 25\n"
	"  GeometryHeight = 5\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = NoArmor\n"
	"  End\n"
	"  Body = StructureBody ModuleTag_Body\n"
	"    MaxHealth = 9999999.9\n"
	"    InitialHealth = 9999999.9\n"
	"  End\n"
	"  Behavior = RebuildHoleBehavior ModuleTag_RebuildHole\n"
	"    WorkerObjectName = Port144Worker\n"
	"    WorkerRespawnDelay = 120000\n"
	"    HoleHealthRegen%PerSecond = 0.5%\n"
	"  End\n"
	"  Behavior = SlowDeathBehavior ModuleTag_Die\n"
	"    SinkDelay = 0\n"
	"    SinkRate = 4.8\n"
	"    DestructionDelay = 4000\n"
	"  End\n"
	"End\n";

struct Port144World : CombatWorld
{
	Port144World()
		: CombatWorld(kPort144Objects)
	{
	}
	// every object of the template that is still in the logic's list, dead or not
	std::vector<Object *> all(const char *templateName)
	{
		std::vector<Object *> out;
		for (Object *o = logic->getFirstObject(); o; o = o->getNextObject())
		{
			if (!o->isDestroyed() && o->getTemplate()->getName() == templateName)
			{
				out.push_back(o);
			}
		}
		return out;
	}
	std::vector<Object *> alive(const char *templateName)
	{
		std::vector<Object *> out;
		for (Object *o : all(templateName))
		{
			if (!o->isEffectivelyDead())
			{
				out.push_back(o);
			}
		}
		return out;
	}
	// a cave of Alice's that died: its hole
	Object *holeOfDeadCave()
	{
		Object *cave = unit("Port144Cave", 'A', 400, 400);
		REQUIRE(cave != nullptr);
		REQUIRE_FALSE(cave->isUnderConstruction());
		cave->kill(DAMAGE_UNRESISTABLE, DEATH_NORMAL);
		frames(1);
		const std::vector<Object *> holes = alive("Port144Hole");
		REQUIRE(holes.size() == 1);
		return holes[0];
	}
};

bool status(const Object &o, const char *name)
{
	return o.testStatus((unsigned)ObjectTemplateInfoBuilder::objectStatusIndex(name));
}
} // namespace

TEST_CASE("PORT-144 hole delay: the counter is WorkerRespawnDelay from the hole's creation and nothing is rebuilt before it ends (RW 0x886831 -> 0x8865F7, 0x8868C2)")
{
	Port144World w;
	Object *hole = w.holeOfDeadCave();
	RebuildHoleBehavior *rb = RebuildHoleBehavior::of(*hole);
	REQUIRE(rb != nullptr);
	CHECK(hole->getBodyModule()->getMaxHealth() == 500.0f);
	// RW 0x886628: 120000 ms = 600 frames, set when the hole is made; one update has run
	CHECK(rb->workerWaitCounter() == 599);
	CHECK(rb->getWorkerID() == INVALID_ID);
	CHECK_FALSE(hole->isMasked());
	w.frames(590);
	CHECK(rb->workerWaitCounter() == 9);
	CHECK(w.alive("Port144Worker").empty());
	CHECK(w.alive("Port144Cave").empty()); // not one frame of a rebuild before the delay is over
	CHECK(rb->getReconstructedBuildingID() == INVALID_ID);
	w.frames(9);
	CHECK(rb->workerWaitCounter() == 0);
	CHECK(rb->getWorkerID() != INVALID_ID);
	CHECK(w.alive("Port144Cave").size() == 1);
}

TEST_CASE("PORT-144 hole delay: a hole that takes damage keeps counting, and a hole that stops taking damage does not start early (RW 0x8868C2 reads no damage)")
{
	Port144World w;
	Object *hole = w.holeOfDeadCave();
	RebuildHoleBehavior *rb = RebuildHoleBehavior::of(*hole);
	REQUIRE(rb != nullptr);
	for (int i = 0; i < 50; ++i)
	{
		DamageInfo hit;
		hit.m_input.m_amount = 1.0f;
		hit.m_input.m_damageType = DAMAGE_UNRESISTABLE;
		hit.m_input.m_deathType = DEATH_NORMAL;
		hole->getBodyModule()->attemptDamage(hit);
		w.frames(1);
	}
	REQUIRE_FALSE(hole->isEffectivelyDead());
	CHECK(rb->workerWaitCounter() == 549); // 599 - 50: the hits did not hold the counter back
	// the hits stop: 30 seconds later (150 frames) nothing has started
	w.frames(150);
	CHECK(rb->workerWaitCounter() == 399);
	CHECK(w.alive("Port144Worker").empty());
	CHECK(w.alive("Port144Cave").empty());
	// the hole regenerates meanwhile (RW 0x886B77 .. 0x886BD5), up to HoleMaxHealth
	CHECK(hole->getBodyModule()->getHealth() == 500.0f);
}

TEST_CASE("PORT-144 rebuild: the structure under the hole's worker is UNDER_CONSTRUCTION with a percent that grows from 0 (RW 0x8AD6A4, the reader RW 0x677CF9)")
{
	Port144World w;
	Object *hole = w.holeOfDeadCave();
	RebuildHoleBehavior *rb = RebuildHoleBehavior::of(*hole);
	REQUIRE(rb != nullptr);
	w.frames(599);
	const std::vector<Object *> caves = w.alive("Port144Cave");
	REQUIRE(caves.size() == 1);
	Object *cave = caves[0];
	CHECK(rb->getReconstructedBuildingID() == cave->getID());
	// what RW 0x677CF9 reads before it draws CONTROLBAR:UnderConstructionDesc
	CHECK(cave->isUnderConstruction());
	CHECK_FALSE(status(*cave, "SOLD"));
	CHECK_FALSE(cave->isEffectivelyDead());
	CHECK(status(*cave, "RECONSTRUCTING"));
	const float first = cave->getConstructionPercent();
	CHECK(first >= 0.0f); // tolerance: the reader shows any value but the -1 of an object that is not being built; the worker may have built a frame already
	CHECK(first < 100.0f); // tolerance: the rebuild has only started (BuildTime 10 s)
	// BuildTime 10 s = 50 frames: half way the percent is between the ends and above the first value
	w.frames(20);
	REQUIRE(cave->isUnderConstruction());
	const float mid = cave->getConstructionPercent();
	CHECK(mid > first); // tolerance: the percent grows while the worker builds; the frame the worker reaches the site is not RW data of this test
	CHECK(mid < 100.0f); // tolerance: 20 of the 50 build frames
	// the hole during the rebuild (RW 0x886B4D .. 0x886B72): masked, the statuses 3 and 0x3C, alive and in the world
	CHECK_FALSE(hole->isEffectivelyDead());
	CHECK(hole->isMasked());
	CHECK(status(*hole, "UNSELECTABLE"));
	CHECK(status(*hole, "UNATTACKABLE"));
	CHECK(w.all("Port144Hole").size() == 1);
}

TEST_CASE("PORT-144 rebuild: the hole dies when the structure is complete and its SlowDeathBehavior removes it (RW 0x886BEC .. 0x886C40)")
{
	Port144World w;
	Object *hole = w.holeOfDeadCave();
	const ObjectID holeId = hole->getID();
	w.frames(599);
	REQUIRE(w.alive("Port144Cave").size() == 1);
	Object *cave = w.alive("Port144Cave")[0];
	int guard = 0;
	while (cave->isUnderConstruction() && guard < 400)
	{
		CHECK_FALSE(hole->isEffectivelyDead()); // alive for the whole rebuild
		w.frames(1);
		++guard;
	}
	REQUIRE_FALSE(cave->isUnderConstruction());
	CHECK(cave->getBodyModule()->getHealth() == 1000.0f);
	// the first update of the hole after that kills it (RW 0x886C40); the worker goes with it (RW 0x886C2C)
	w.frames(1);
	Object *h = w.logic->findObjectByID(holeId);
	CHECK((h == nullptr || h->isEffectivelyDead()));
	CHECK(w.alive("Port144Worker").empty());
	// DestructionDelay 4000 ms = 20 frames: then the hole is gone from the world
	w.frames(25);
	CHECK(w.all("Port144Hole").empty());
	CHECK(w.alive("Port144Cave").size() == 1);
	CHECK_FALSE(cave->isEffectivelyDead());
}
