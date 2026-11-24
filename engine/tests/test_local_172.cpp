// OpenBFME. GPL-3.0.
// LOCAL-172 (fork issue 953):
//   - Binary-map audit of 449 uncited RotWK 2.01 object/module functions (part 2 of 4, 0x692BA0..0x7AEAF7).
//   - Ports ExperienceTracker::canGainLevel() (RW 0x79D11D -> RW 0x6893E2: float(next->requiredExperience) > m_experience)
//     and verifies cited counterparts (StrafeAreaUpdateModuleData RW 0x7A81E8, ObjectTypes lists RW 0x778E5D / 0x778EB8 / 0x778EFB / 0x778F8C).

#include "doctest.h"
#include "LogicTestUtil.h"

#include "GameLogic/ExperienceLevels.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/StrafeAreaUpdate.h"
#include "GameLogic/Object/ExperienceTracker.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/ScriptEngine/ScriptEngine.h"

TEST_CASE("LOCAL-172 ExperienceTracker::canGainLevel matches RW 0x79D11D -> RW 0x6893E2 float comparison")
{
	// Expectations from RW 0x79D11D -> RW 0x6893E2:
	//   currentLevel = RW 0x68931C(this); nextLevel = RW 0x689045(currentLevel);
	//   return nextLevel != nullptr && float(nextLevel->requiredExperience) > m_experience (comiss + jbe).
	struct LevelsGuard
	{
		ExperienceLevelSystem levels;
		ExperienceLevelSystem *saved = TheExperienceLevelSystem;
		LevelsGuard() { TheExperienceLevelSystem = &levels; }
		~LevelsGuard() { TheExperienceLevelSystem = saved; }
	} xpGuard;

	logictest::LogicWorld lw;
	xpGuard.levels.registerBlocks(lw.w.fx.env.blocks);
	REQUIRE(lw.w.load(
		"ExperienceLevel TestUnitL1\n"
		"  TargetNames = Local172Hero\n"
		"  RequiredExperience = 1\n"
		"  Rank = 1\n"
		"End\n"
		"ExperienceLevel TestUnitL2\n"
		"  TargetNames = Local172Hero\n"
		"  RequiredExperience = 100\n"
		"  Rank = 2\n"
		"End\n"
		"Object Local172Hero\n"
		"  KindOf = PRELOAD SELECTABLE CAN_CAST_REFLECTIONS INFANTRY HERO\n"
		"  Body = ActiveBody ModuleTag_Body\n"
		"    MaxHealth = 500\n"
		"  End\n"
		"End\n").empty());

	Object *obj = lw.make("Local172Hero");
	REQUIRE(obj != nullptr);
	ExperienceTracker *xp = obj->getExperienceTracker();
	REQUIRE(xp != nullptr);

	// At rank 1 (experience 0 < 100): can gain next level
	CHECK(xp->getRank() == 1);
	CHECK(xp->canGainLevel());

	// Fractional experience just below threshold (99.5f < 100.0f): still can gain level (RW 0x6893E2 comiss)
	xp->addExperiencePoints(98.5f, false, false, false, false);
	CHECK(xp->getRank() == 1);
	CHECK(xp->canGainLevel());

	// Reaching rank 2 (max level, no nextLevel): canGainLevel() is false (RW 0x6893FC je -> xor al, al)
	xp->addExperiencePoints(0.5f, false, false, false, false);
	CHECK(xp->getRank() == 2);
	CHECK_FALSE(xp->canGainLevel());
}

TEST_CASE("LOCAL-172 StrafeAreaUpdateModuleData defaults match RW 0x7A81E8")
{
	// Expectations from RW 0x7A81E8 (StrafeAreaUpdateModuleData::StrafeAreaUpdateModuleData):
	//   +0x0C = 150.0f (0xC041F8), +0x10 = 0.4f (0xBE0DE4), +0x14 = 100.0f (0xBD88D8),
	//   +0x18 = 50.0f (0xBD88C4), +0x1C = 0.0f (xorps).
	StrafeAreaUpdateModuleData d;
	CHECK(d.m_weaponName.empty());
	CHECK(d.m_strafeAreaRadius == 150.0f);
	CHECK(d.m_sweepFrequency == 0.4f);
	CHECK(d.m_sweepAmplitude == 100.0f);
	CHECK(d.m_slope == 50.0f);
	CHECK(d.m_initialSweepPhase == 0.0f);
}

TEST_CASE("LOCAL-172 ScriptEngine ObjectTypes lists add, dedupe, getNth, remove (RW 0x778E5D, RW 0x778EFB)")
{
	// Expectations from RW 0x759D77 / RW 0x778E5D / RW 0x778EFB / RW 0x603B6A:
	//   missing list created on add; duplicates ignored; removing last item removes list.
	logictest::LogicWorld lw;
	ScriptEngine &se = lw.logic->scriptEngine();

	CHECK(se.findObjectList("Local172List") == nullptr);
	se.objectListAdd("Local172List", "GondorFighter", true);
	se.objectListAdd("Local172List", "GondorFighter", true);
	se.objectListAdd("Local172List", "GondorArcher", true);
	REQUIRE(se.findObjectList("Local172List") != nullptr);
	CHECK(se.findObjectList("Local172List")->size() == 2);
	CHECK(se.objectListGetNth("Local172List", 0) == "GondorFighter");
	CHECK(se.objectListGetNth("Local172List", 1) == "GondorArcher");

	se.objectListAdd("Local172List", "GondorFighter", false);
	se.objectListAdd("Local172List", "GondorArcher", false);
	CHECK(se.findObjectList("Local172List") == nullptr);
}
