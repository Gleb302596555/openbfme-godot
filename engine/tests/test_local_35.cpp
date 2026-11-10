// OpenBFME unit tests: LOCAL-35 DynamicShroudClearingRangeUpdate and DelayedLuaEventUpdate (issue #285; RotWK only).
//
// Target facts and RotWK 2.01 addresses (read through tools/mod/rw.py dis):
//   - DynamicShroudClearingRangeUpdate: ctor RW 0x88E6D8, update RW 0x88E9B0, xfer RW 0x88EB76, table RW 0xC62648.
//     The ctor counts down ShrinkDelay + ShrinkTime and the update grows the object's shroud clearing range by
//     nativeClearingRange / max(1, GrowTime) a frame, sustains, then shrinks by (native - FinalVision) / max(1, ShrinkTime)
//     a frame, then parks at FinalVision and sleeps. nativeClearingRange is the template's ShroudClearingRange.
//   - DelayedLuaEventUpdate: ctor RW 0x8AC560 (sleeps forever), configure RW 0x8AC5E1, update RW 0x8AC628
//     (fires the event through RW 0x7379CB on every object in range other than itself, then RW 0x62BBAB destroys it).
//     The retail caller RW 0x90DC46 passes slot 9 (LUAEVENT_DamageIncoming).

#include "doctest.h"
#include "CombatTestUtil.h"

#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/DelayedLuaEventUpdate.h"
#include "GameLogic/Module/DynamicShroudClearingRangeUpdate.h"
#include "GameLogic/Object/Object.h"

namespace
{
struct PingRig
{
	combattest::CombatWorld w;

	explicit PingRig(const char *fields)
	{
		DynamicShroudClearingRangeUpdate::registerClass(w.w.modules);
		const std::string ini = std::string(
			"Object TestPing\n"
			"  KindOf = INERT IMMOBILE\n"
			"  ShroudClearingRange = 200\n"
			"  Behavior = DynamicShroudClearingRangeUpdate ModuleTag_Ping\n") + fields + "  End\n" + "End\n";
		const std::string err = w.w.load(ini, INI_LOAD_OVERWRITE, "ping.ini");
		REQUIRE_MESSAGE(err.empty(), err);
	}
};
} // namespace

TEST_CASE("LOCAL-35 DynamicShroudClearingRangeUpdate: the ping's shroud clearing range grows to the template's ShroudClearingRange, sustains, shrinks to FinalVision and then sleeps (RW 0x88E6D8 / RW 0x88E9B0)")
{
	// ShrinkDelay 400ms (2 frames) + ShrinkTime 400ms (2 frames) = 4 total frames; GrowTime 200ms (1 frame) grows to 200 in one frame;
	// shrink (200-100)/2 = 50 a frame. ChangeInterval / GrowInterval 200ms = 1 frame.
	PingRig rig(
		"    ShrinkDelay = 400\n"
		"    ShrinkTime = 400\n"
		"    GrowDelay = 0\n"
		"    GrowTime = 200\n"
		"    FinalVision = 100\n"
		"    ChangeInterval = 200\n"
		"    GrowInterval = 200\n");

	Object *ping = rig.w.unit("TestPing", 'A', 100.0f, 100.0f);
	REQUIRE(ping != nullptr);
	DynamicShroudClearingRangeUpdate *mod = dynamic_cast<DynamicShroudClearingRangeUpdate *>(ping->findModule("DynamicShroudClearingRangeUpdate"));
	REQUIRE(mod != nullptr);

	// The first update (frame 1) grows 0 + 200 / max(1,1) = 200 and clamps to SUSTAINING, then applies the range.
	// tolerance: none; 200, 100, 50 are exact binary32, and the SimMath div/sub of these values are exact
	rig.w.frames(1);
	CHECK(ping->shroudClearingRangeOverride() == 200.0f);

	// SUSTAINING keeps 200; the change-interval countdown (1) skips the apply on the even frame.
	rig.w.frames(1);
	CHECK(ping->shroudClearingRangeOverride() == 200.0f);

	// Shrinking begins (200 - 50 = 150 applied).
	rig.w.frames(1);
	CHECK(ping->shroudClearingRangeOverride() == 150.0f);

	rig.w.frames(1);
	CHECK(ping->shroudClearingRangeOverride() == 150.0f);

	// Final frame: DONE_FOREVER parks at FinalVision (100) and the module sleeps.
	rig.w.frames(1);
	CHECK(ping->shroudClearingRangeOverride() == 100.0f);

	// A further frame changes nothing (the module is asleep).
	rig.w.frames(1);
	CHECK(ping->shroudClearingRangeOverride() == 100.0f);
}

TEST_CASE("LOCAL-35 DelayedLuaEventUpdate: fires the configured event once on the object in range and destroys itself (RW 0x8AC5E1 / RW 0x8AC628)")
{
	combattest::CombatWorld w;
	DelayedLuaEventUpdate::registerClass(w.w.modules);
	const std::string err = w.w.load(
		"Object TestWarning\n"
		"  KindOf = IMMOBILE\n"
		"  Behavior = DelayedLuaEventUpdate ModuleTag_Event\n"
		"  End\n"
		"End\n"
		"Object TestTarget\n"
		"  KindOf = IMMOBILE\n"
		"End\n",
		INI_LOAD_OVERWRITE, "warning.ini");
	REQUIRE_MESSAGE(err.empty(), err);

	Object *warning = w.unit("TestWarning", 'A', 100.0f, 100.0f);
	Object *target = w.unit("TestTarget", 'A', 100.0f, 120.0f);
	REQUIRE(warning != nullptr);
	REQUIRE(target != nullptr);

	DelayedLuaEventUpdate *mod = dynamic_cast<DelayedLuaEventUpdate *>(warning->findModule("DelayedLuaEventUpdate"));
	REQUIRE(mod != nullptr);

	const ObjectID warningId = warning->getID();
	// RW 0x90DC46 passes slot 9 (DamageIncoming); allies+enemies covers every relationship (flags 7).
	mod->configure(9, LuaEventArgs(), UPDATE_SLEEP_NONE, 100.0f, true, true);

	const unsigned long long before = w.logic->scriptEventsWithoutDispatch();
	w.frames(1);

	// The warning object was destroyed after the dispatch; the event reached the script engine once (for the target).
	CHECK(w.logic->findObjectByID(warningId) == nullptr);
	CHECK(w.logic->scriptEventsWithoutDispatch() == before + 1);
}
