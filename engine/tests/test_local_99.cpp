// OpenBFME. LOCAL-99 tests (fork issue 764): LifetimeUpdate::xfer (RW 0x007A7E88) preserves m_waitingForWakeUp (+0x28)
// under version (1, 2) and stays backwards-compatible with v1 snapshots.
#include "doctest.h"
#include "CombatTestUtil.h"

#include "Common/XferLoad.h"
#include "Common/XferSave.h"
#include "GameLogic/Module/StructureModules.h"

#include <cstdint>
#include <vector>

using namespace combattest;

namespace
{
const char kLifetimeWaitObjects[] =
	"Object Local99WaitLifetime\n"
	"  KindOf = STRUCTURE SELECTABLE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryMinorRadius = 10\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = LifetimeUpdate ModuleTag_Life\n"
	"    MinLifetime = 1000\n"
	"    MaxLifetime = 1000\n"
	"    WaitForWakeUp = Yes\n"
	"  End\n"
	"End\n";

struct Local99World : CombatWorld
{
	Local99World()
	{
		const std::string err = w.load(kLifetimeWaitObjects, INI_LOAD_OVERWRITE, "local99.ini");
		REQUIRE_MESSAGE(err.empty(), err);
	}
};
} // namespace

// RW 0x007A7E88: LifetimeUpdate::xfer version (1, 2) serializes m_waitingForWakeUp (+0x28) so a woken WaitForWakeUp
// module stays awake (waitingForWakeUp() == false) after save/load instead of reverting to the constructor's true.
TEST_CASE("LOCAL-99 LifetimeUpdate::xfer preserves WaitForWakeUp state across save/load (RW 0x007A7E88)")
{
	Local99World w1;
	Object *src = w1.unit("Local99WaitLifetime", 'B', 100, 100);
	REQUIRE(src != nullptr);
	auto *srcLife = dynamic_cast<LifetimeUpdate *>(src->findModule("LifetimeUpdate"));
	REQUIRE(srcLife != nullptr);
	CHECK(srcLife->waitsForWakeUp());
	CHECK(srcLife->waitingForWakeUp());

	// Wake the module: RW 0x7A7E79 clears +0x28 and schedules m_dieFrame.
	srcLife->wakeUp();
	CHECK_FALSE(srcLife->waitingForWakeUp());
	const unsigned expectedDie = srcLife->dieFrame();
	const unsigned expectedStart = srcLife->startFrame();
	CHECK(expectedDie == expectedStart + 5u);

	XferSave save;
	srcLife->xfer(save);

	// Load into a fresh object whose constructor initialized m_waitingForWakeUp = true.
	Local99World w2;
	Object *dst = w2.unit("Local99WaitLifetime", 'B', 100, 100);
	REQUIRE(dst != nullptr);
	auto *dstLife = dynamic_cast<LifetimeUpdate *>(dst->findModule("LifetimeUpdate"));
	REQUIRE(dstLife != nullptr);
	CHECK(dstLife->waitingForWakeUp());

	XferLoad load(save.data());
	dstLife->xfer(load);
	CHECK_FALSE(dstLife->waitingForWakeUp());
	CHECK(dstLife->dieFrame() == expectedDie);
	CHECK(dstLife->startFrame() == expectedStart);
}

// RW 0x007A7E88 (0x7A7ECC..0x7A7ED0): version 1 snapshots omit +0x28; loading a v1 payload must succeed and leave
// m_waitingForWakeUp at its initialized value.
TEST_CASE("LOCAL-99 LifetimeUpdate::xfer loads legacy v1 snapshot without WaitForWakeUp byte (RW 0x007A7E88)")
{
	Local99World w1;
	Object *src = w1.unit("Local99WaitLifetime", 'B', 100, 100);
	REQUIRE(src != nullptr);
	auto *srcLife = dynamic_cast<LifetimeUpdate *>(src->findModule("LifetimeUpdate"));
	REQUIRE(srcLife != nullptr);

	XferSave modProbe;
	srcLife->xfer(modProbe);
	// modProbe layout: [ver=2] [UpdateModule::xfer bytes] [u32 die] [u32 start] [u8 wait]
	REQUIRE(modProbe.data().size() >= 10u); // tolerance: UpdateModule::xfer header precedes the 9-byte LifetimeUpdate tail
	std::vector<std::uint8_t> payload;
	payload.push_back(1u); // v1
	payload.insert(payload.end(), modProbe.data().begin() + 1, modProbe.data().end() - 9);
	std::uint32_t die = 42u;
	std::uint32_t start = 12u;
	XferSave tail;
	tail.xferUnsignedInt(&die);
	tail.xferUnsignedInt(&start);
	payload.insert(payload.end(), tail.data().begin(), tail.data().end());

	Local99World w2;
	Object *dst = w2.unit("Local99WaitLifetime", 'B', 100, 100);
	REQUIRE(dst != nullptr);
	auto *dstLife = dynamic_cast<LifetimeUpdate *>(dst->findModule("LifetimeUpdate"));
	REQUIRE(dstLife != nullptr);
	CHECK(dstLife->waitingForWakeUp());

	XferLoad load(payload);
	dstLife->xfer(load);
	CHECK(dstLife->waitingForWakeUp());
	CHECK(dstLife->dieFrame() == 42u);
	CHECK(dstLife->startFrame() == 12u);
}

#include "EnnorathScenario.h"
#include "Common/ModelState.h"
#include "GameLogic/Combat/ObjectWeapons.h"
#include "GameLogic/Module/ActiveBody.h"
#include "GameLogic/Module/HeroAbilityModules.h"
#include "GameLogic/Module/HitReactionBehavior.h"
#include "GameLogic/Module/StancesBehavior.h"
#include "GameLogic/Object/PartitionManager.h"

// Ennorath real-unit scenario: StancesBehavior (GondorFighterHorde, menhordes.ini), HitReactionBehavior
// (GondorAnarion, arnoranarion.ini), DualWeaponBehavior (GondorBoromir), and LifetimeUpdate
// (ArnorElrond_Summoned, arnorelrond_summoned.ini, MinLifetime=MaxLifetime=90000 -> 450 frames).
TEST_CASE("LOCAL-99 ENNORATH scenario: StancesBehavior, HitReactionBehavior, DualWeaponBehavior and LifetimeUpdate")
{
	ennorathscenario::World mod;
	if (!mod.load("LOCAL-99 ENNORATH scenario"))
	{
		return;
	}
	structtest::Arena arena(mod.shared, "FactionMen", "FactionMordor", 240);
	arena.logic.partition().setRegion(0.0f, 0.0f, 2400.0f, 2400.0f);
	auto step = [&](int n) { for (int k = 0; k < n; ++k) arena.logic.runLogicFrame(); };

	// 1. StancesBehavior: GondorFighterHorde (menhordes.ini: StanceTemplate = FighterHorde)
	Object *horde = mod.place(arena, "GondorFighterHorde", 0, 400.0f, 400.0f);
	REQUIRE(horde != nullptr);
	StancesBehavior *stances = StancesBehavior::of(*horde);
	REQUIRE(stances != nullptr);
	step(2);
	CHECK(stances->setStance(STANCE_AGGRESSIVE));
	CHECK(stances->getStance() == STANCE_AGGRESSIVE);
	CHECK(stances->setStance(STANCE_HOLD_GROUND));
	CHECK(stances->getStance() == STANCE_HOLD_GROUND);

	// 2. HitReactionBehavior: GondorAnarion (arnoranarion.ini: Threshold1=0, Threshold2=25, Threshold3=50)
	Object *anarion = mod.place(arena, "GondorAnarion", 0, 700.0f, 400.0f);
	REQUIRE(anarion != nullptr);
	auto *hr = dynamic_cast<HitReactionBehavior *>(anarion->findModule("HitReactionBehavior"));
	REQUIRE(hr != nullptr);
	step(3);
	auto *body = dynamic_cast<ActiveBody *>(anarion->getBodyModule());
	REQUIRE(body != nullptr);
	const int kHitReaction = [] {
		for (int i = 0; TheModelConditionNames[i]; ++i)
		{
			if (std::string(TheModelConditionNames[i]) == "HIT_REACTION")
			{
				return i;
			}
		}
		return -1;
	}();
	REQUIRE(kHitReaction >= 0); // tolerance: index lookup in TheModelConditionNames
	body->friend_setHealthRaw(body->getHealth() - 30.0f);
	step(1);
	CHECK(anarion->testModelCondition(kHitReaction));
	CHECK(anarion->testModelCondition(kHitReaction + 2)); // HIT_LEVEL_2 (25 <= 30 < 50)

	// 3. DualWeaponBehavior: GondorArcher (gondorarcher.ini: SwitchWeaponOnCloseRangeDistance = 40) switches to CLOSE_RANGE (flag 7)
	// when an enemy is within 40 units and unfogged (RW 0x85DF88 / RW 0x85DF3D xfer).
	Object *boromir = mod.place(arena, "GondorArcher", 0, 1200.0f, 1200.0f);
	REQUIRE(boromir != nullptr);
	auto *dw = dynamic_cast<DualWeaponBehavior *>(boromir->findModule("DualWeaponBehavior"));
	REQUIRE(dw != nullptr);
	const unsigned kCloseRange = 7u;
	step(10);
	CHECK_FALSE(MaskTest(boromir->getWeaponSetFlags(), kCloseRange));
	Object *enemy = mod.place(arena, "MordorFighter", 1, 1220.0f, 1200.0f);
	REQUIRE(enemy != nullptr);
	bool switched = false;
	for (int i = 0; i < 40 && !switched; ++i)
	{
		step(1);
		switched = MaskTest(boromir->getWeaponSetFlags(), kCloseRange);
	}
	CHECK(switched);
	XferSave dwSave;
	dw->xfer(dwSave);
	XferLoad dwLoad(dwSave.data());
	dw->xfer(dwLoad);

	// 4. LifetimeUpdate: ArnorElrond_Summoned (arnorelrond_summoned.ini: MinLifetime=MaxLifetime=90000 ms = 450 frames)
	Object *summon = mod.place(arena, "ArnorElrond_Summoned", 0, 1800.0f, 400.0f);
	REQUIRE(summon != nullptr);
	auto *life = dynamic_cast<LifetimeUpdate *>(summon->findModule("LifetimeUpdate"));
	REQUIRE(life != nullptr);
	CHECK_FALSE(life->waitingForWakeUp());
	CHECK(life->dieFrame() == life->startFrame() + 450u);

	// Verify RW 0x007A7E88 xfer round-trip on the real mod unit
	XferSave save;
	life->xfer(save);
	XferLoad load(save.data());
	life->xfer(load);
	CHECK_FALSE(life->waitingForWakeUp());
	CHECK(life->dieFrame() == life->startFrame() + 450u);

	const ObjectID summonId = summon->getID();
	step(452);
	const Object *after = arena.logic.findObjectByID(summonId);
	CHECK((after == nullptr || after->isEffectivelyDead()));
}
