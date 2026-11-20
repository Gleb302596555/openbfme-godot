// OpenBFME. PORT-304 (issue #696): GiantBirdSlowDeathBehavior's CrashAvoidStrength and NeedToMaintainFlailingHeight. RotWK 2.01 parses them (rows RW 0xC59480 /
// RW 0xC59490), the data constructor RW 0x865076 writes them (0.1 from RW 0xBD83D4, 0 at RW 0x8650D9) and nothing reads them (the chain is in
// GiantBirdSlowDeathBehavior.h). The tests pin the parse and the defaults, and that a crash does not depend on them. Synthetic data.
#include "doctest.h"
#include "CombatTestUtil.h"

#include "GameLogic/Module/GiantBirdSlowDeathBehavior.h"
#include "GameLogic/Module/PhysicsBehavior.h"
#include "GameLogic/Object/PartitionManager.h"

#include <string>

using namespace combattest;

namespace
{
std::string objects(const char *deathFields)
{
	const std::string body = "  Body = ActiveBody ModuleTag_Body\n    MaxHealth = 1000\n  End\n";
	const std::string geometry = "  Geometry = CYLINDER\n  GeometryHeight = 20\n  ArmorSet\n    Conditions = None\n    Armor = NoArmor\n  End\n";
	return std::string("Object Flyer\n  KindOf = INFANTRY SELECTABLE SCORE\n  GeometryMajorRadius = 8\n  GeometryMinorRadius = 8\n") + geometry + body +
		"  Behavior = DestroyDie ModuleTag_Destroy\n  End\n"
		"  Behavior = AIUpdateInterface ModuleTag_AI\n  End\n"
		"  Behavior = PhysicsBehavior ModuleTag_Physics\n  End\n"
		"  Behavior = GiantBirdSlowDeathBehavior ModuleTag_Death\n"
		"    DeathTypes = ALL\n"
		"    DestructionDelay = 20000\n"
		"    CrashAvoidKindOfs = STRUCTURE\n"
		"    CrashAvoidRadius = 200\n" +
		std::string(deathFields) +
		"  End\n"
		"End\n"
		"Object Block\n  KindOf = STRUCTURE SELECTABLE\n  GeometryMajorRadius = 30\n  GeometryMinorRadius = 30\n" + geometry + body + "End\n";
}

const GiantBirdSlowDeathBehaviorModuleData &dataOf(Object &flyer)
{
	GiantBirdSlowDeathBehavior *death = dynamic_cast<GiantBirdSlowDeathBehavior *>(flyer.findModule("GiantBirdSlowDeathBehavior"));
	REQUIRE(death != nullptr);
	const GiantBirdSlowDeathBehaviorModuleData *data = dynamic_cast<const GiantBirdSlowDeathBehaviorModuleData *>(death->getModuleData());
	REQUIRE(data != nullptr);
	return *data;
}

Coord3D crashOf(const char *deathFields)
{
	CombatWorld w(objects(deathFields).c_str());
	w.logic->partition().setRegion(0.0f, 0.0f, 1000.0f, 1000.0f);
	Object *flyer = w.unit("Flyer", 'A', 300, 300);
	w.unit("Block", 'B', 300, 300);
	GiantBirdSlowDeathBehavior *death = dynamic_cast<GiantBirdSlowDeathBehavior *>(flyer->findModule("GiantBirdSlowDeathBehavior"));
	REQUIRE(death != nullptr);
	w.frames(1);
	flyer->kill(DAMAGE_UNRESISTABLE, DEATH_NORMAL);
	REQUIRE(death->isSlowDeathActivated());
	return death->crashTarget();
}
} // namespace

TEST_CASE("PORT-304: CrashAvoidStrength and NeedToMaintainFlailingHeight parse; unset they keep the data constructor's 0.1 and No (RW 0x865076)")
{
	{
		CombatWorld w(objects("").c_str());
		Object *flyer = w.unit("Flyer", 'A', 300, 300);
		const GiantBirdSlowDeathBehaviorModuleData &d = dataOf(*flyer);
		CHECK(d.m_crashAvoidStrength == 0.1f); // RW 0xBD83D4 -> RW 0x8650D1
		CHECK_FALSE(d.m_needToMaintainFlailingHeight); // RW 0x8650D9
	}
	{
		CombatWorld w(objects("    CrashAvoidStrength = 5.5\n    NeedToMaintainFlailingHeight = Yes\n").c_str());
		Object *flyer = w.unit("Flyer", 'A', 300, 300);
		const GiantBirdSlowDeathBehaviorModuleData &d = dataOf(*flyer);
		CHECK(d.m_crashAvoidStrength == 5.5f); // row RW 0xC59480, parser RW 0x42ED00
		CHECK(d.m_needToMaintainFlailingHeight); // row RW 0xC59490, parser RW 0x42E558
	}
}

TEST_CASE("PORT-304: the crash target does not depend on CrashAvoidStrength or NeedToMaintainFlailingHeight (RotWK reads neither; RW 0x86511C reads + 0x19C / + 0x1B8)")
{
	const Coord3D plain = crashOf("");
	const Coord3D set = crashOf("    CrashAvoidStrength = 5.5\n    NeedToMaintainFlailingHeight = Yes\n");
	CHECK(plain.y == doctest::Approx(260.0f)); // tolerance: sin / cos of a float32 -PI (the modgap1 case's ring point)
	CHECK(set.x == plain.x);
	CHECK(set.y == plain.y);
	CHECK(set.z == plain.z);
}
