// OpenBFME. MODGAP-1 tests, group 2 (MODULE_GAPS.md, stop S-5603): RebuildHoleExposeDie (RW 0x889AAF) and RebuildHoleBehavior (RW 0x8868C2, reset RW 0x8865F7) on
// synthetic data. The expected values are the data's own: the hole's max health, WorkerRespawnDelay in frames, the structure's paid price.
#include "doctest.h"
#include "CombatTestUtil.h"

#include "GameLogic/Damage.h"
#include "GameLogic/Module/RebuildHoleModules.h"

using namespace combattest;

namespace
{
std::string lair(const char *name, const char *holeName)
{
	return std::string("Object ") + name + "\n"
		"  KindOf = STRUCTURE\n"
		"  Geometry = CYLINDER\n"
		"  GeometryMajorRadius = 20\n"
		"  GeometryMinorRadius = 20\n"
		"  GeometryHeight = 20\n"
		"  ArmorSet\n"
		"    Conditions = None\n"
		"    Armor = NoArmor\n"
		"  End\n"
		"  Body = ActiveBody ModuleTag_Body\n"
		"    MaxHealth = 200\n"
		"  End\n"
		"  Behavior = DestroyDie ModuleTag_Destroy\n"
		"  End\n"
		"  Behavior = RebuildHoleExposeDie ModuleTag_Expose\n"
		"    HoleName = " + holeName + "\n"
		"    HoleMaxHealth = 50.0\n"
		"    FadeInTimeSeconds = 2.0\n"
		"    TransferAttackers = No\n"
		"  End\n"
		"End\n";
}

std::string hole(const char *name, const char *behaviorFields)
{
	return std::string("Object ") + name + "\n"
		"  KindOf = STRUCTURE IMMOBILE REBUILD_HOLE\n"
		"  Geometry = CYLINDER\n"
		"  GeometryMajorRadius = 25\n"
		"  GeometryMinorRadius = 25\n"
		"  GeometryHeight = 5\n"
		"  ArmorSet\n"
		"    Conditions = None\n"
		"    Armor = NoArmor\n"
		"  End\n"
		"  Body = ActiveBody ModuleTag_Body\n"
		"    MaxHealth = 9999999.9\n"
		"    InitialHealth = 9999999.9\n"
		"  End\n"
		"  Behavior = DestroyDie ModuleTag_Destroy\n"
		"  End\n"
		"  Behavior = RebuildHoleBehavior ModuleTag_Rebuild\n" + behaviorFields + "  End\n"
		"End\n";
}

const std::string kHoleObjects =
	lair("HoleLair", "LairHole") + hole("LairHole", "    WorkerRespawnDelay = 1000\n    HoleHealthRegen%PerSecond = 0\n") +
	lair("WorkerLair", "WorkerHole") + hole("WorkerHole", "    WorkerObjectName = SomeWorker\n    WorkerRespawnDelay = 1000\n    HoleHealthRegen%PerSecond = 0\n");

struct HoleWorld : CombatWorld
{
	HoleWorld()
		: CombatWorld(kHoleObjects.c_str())
	{
	}
	std::vector<Object *> alive(const char *templateName)
	{
		std::vector<Object *> out;
		for (Object *o = logic->getFirstObject(); o; o = o->getNextObject())
		{
			if (!o->isDestroyed() && !o->isEffectivelyDead() && o->getTemplate()->getName() == templateName)
			{
				out.push_back(o);
			}
		}
		return out;
	}
};
} // namespace

TEST_CASE("modgap1 RebuildHole: a dead structure of a live player leaves its hole with HoleMaxHealth, the template and the paid price (RW 0x889AAF, 0x886831)")
{
	HoleWorld w;
	Object *l = w.unit("HoleLair", 'A', 400, 400);
	l->setBuildCostPaid(300.0f);
	l->setName("TheLair");
	const ObjectID lairId = l->getID();
	const ThingTemplate *lairTemplate = l->getTemplate();
	l->kill(DAMAGE_UNRESISTABLE, DEATH_NORMAL);
	w.frames(1);
	const std::vector<Object *> holes = w.alive("LairHole");
	REQUIRE(holes.size() == 1);
	Object *h = holes[0];
	CHECK(h->getPosition()->x == doctest::Approx(400.0f));
	CHECK(h->getPosition()->y == doctest::Approx(400.0f));
	CHECK(h->getBodyModule()->getMaxHealth() == doctest::Approx(50.0f));
	CHECK(h->getBodyModule()->getHealth() == doctest::Approx(50.0f));
	CHECK(h->getName() == "TheLair"); // the script name moves to the hole (RW 0x759467)
	RebuildHoleBehavior *rb = RebuildHoleBehavior::of(*h);
	REQUIRE(rb != nullptr);
	CHECK(rb->rebuildTemplate() == lairTemplate);
	CHECK(rb->countdown() <= 5);  // WorkerRespawnDelay 1000 ms = 5 frames
	CHECK(rb->countdown() >= 3);
	(void)lairId;
}

TEST_CASE("modgap1 RebuildHole: after WorkerRespawnDelay the structure stands again, complete, with its price, and the hole is gone without treasure (RW 0x8868C2, 0x886BD8)")
{
	HoleWorld w;
	Object *l = w.unit("HoleLair", 'A', 400, 400);
	l->setBuildCostPaid(300.0f);
	l->setName("TheLair");
	l->kill(DAMAGE_UNRESISTABLE, DEATH_NORMAL);
	w.frames(3);
	CHECK(w.alive("HoleLair").empty()); // still counting down
	w.frames(20);
	const std::vector<Object *> lairs = w.alive("HoleLair");
	REQUIRE(lairs.size() == 1);
	Object *rebuilt = lairs[0];
	CHECK(rebuilt->getPosition()->x == doctest::Approx(400.0f));
	CHECK(rebuilt->getBuildCostPaid() == doctest::Approx(300.0f));
	CHECK_FALSE(rebuilt->isUnderConstruction());
	CHECK(rebuilt->getName() == "TheLair");
	CHECK(w.alive("LairHole").empty()); // the hole faded away
	// the rebuilt lair is a live structure again: it leaves a hole when it dies
	rebuilt->kill(DAMAGE_UNRESISTABLE, DEATH_NORMAL);
	w.frames(1);
	CHECK(w.alive("LairHole").size() == 1);
}

TEST_CASE("modgap1 RebuildHole: a hole destroyed before the countdown ends is not rebuilt (RW 0x8868C2)")
{
	HoleWorld w;
	Object *l = w.unit("HoleLair", 'A', 400, 400);
	l->kill(DAMAGE_UNRESISTABLE, DEATH_NORMAL);
	w.frames(1);
	const std::vector<Object *> holes = w.alive("LairHole");
	REQUIRE(holes.size() == 1);
	holes[0]->kill(DAMAGE_UNRESISTABLE, DEATH_NORMAL); // the player's units broke the ruin
	w.frames(40);
	CHECK(w.alive("LairHole").empty());
	CHECK(w.alive("HoleLair").empty());
}

TEST_CASE("modgap1 RebuildHole: a structure still UNDER_CONSTRUCTION leaves no hole (RW 0x889B03: object + 0x94 bit 2)")
{
	HoleWorld w;
	Object *l = w.unit("HoleLair", 'A', 400, 400);
	l->setStatus(OBJECT_STATUS_UNDER_CONSTRUCTION, true);
	l->kill(DAMAGE_UNRESISTABLE, DEATH_NORMAL);
	w.frames(2);
	CHECK(w.alive("LairHole").empty());
}

TEST_CASE("modgap1 RebuildHole: a hole with a WorkerObjectName waits (the worker branch is S-5603)")
{
	HoleWorld w;
	Object *l = w.unit("WorkerLair", 'A', 400, 400);
	l->kill(DAMAGE_UNRESISTABLE, DEATH_NORMAL);
	w.frames(30);
	CHECK(w.alive("WorkerHole").size() == 1);
	CHECK(w.alive("WorkerLair").empty());
}

TEST_CASE("modgap1 RebuildHole: the stop line names S-5603")
{
	const std::string line = RebuildHoleExposeDie::stopLine();
	CHECK(line.compare(0, 8, "[S-5603]") == 0);
	CHECK(line.find("WorkerObjectName") != std::string::npos);
}
