// OpenBFME unit tests, PORT-225 (fork issue #548): the FiringTracker's frame steps — FiringTracker::update and the shot record of
// FiringTracker::shotFired (GameLogic/WeaponState.h, GameLogic/Object/WeaponState.cpp).
// GPL-3.0.
//
// Expected values come from the retail body of the two functions, read as C++ in the BFME 2 1.06 decompilation, which is byte for
// byte that build's game.dat:
//   * FiringTracker::update        DONOR BFME2 0x004DED3B (Code/GameEngine/Source/GameLogic/Object/FiringTrackerXfer.cpp, the
//                                  unit's "retail-matched BFME2 body" column; the same function in RotWK 2.01 is RW 0x8E30DD);
//   * FiringTracker::calcTimeToSleep DONOR BFME2 0x004DEAB0 (same unit): UPDATE_SLEEP_FOREVER only when none of the three wake
//                                  frames is pending, UPDATE_SLEEP_NONE otherwise;
//   * FiringTracker::shotFired     DONOR BFME2 0x004DEECB (same unit): the shot records its frame and the owner's position before
//                                  the forced-reset branch (RW 0x8E326D).
// The steps update() runs, in retail's order: (1) the owner left the last shot position -> coolDown(true); (2) the forced
// auto-reload (tracker + 0x40); (3) the looped fire sound (+ 0x54; this port keeps that state on the audio side, stop S-712); (4) only
// with a cooldown frame pending (+ 0x3C != 0) and the frame past it -> push the frame LogicFramesPerSecond ahead (5), coolDown(false)
// and sleep that many frames. A weapon with ContinuousFireCoast = 0 leaves + 0x3C at 0 for good, so step 4 never runs for it.

#include "doctest.h"
#include "IniTestUtil.h"
#include "RetailTestMount.h"

#include "GameLogic/BitFlags.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/WeaponState.h"
#include "GameLogic/WeaponStores.h"

#include <array>
#include <cstdint>
#include <string>

using namespace initest;

namespace
{
// The weapon templates the cases below build, parsed through the real Weapon block parser (lane WEAPON-1's fixture shape).
struct Port225World
{
	Fixture fx;
	WeaponStores stores;
	Port225World()
	{
		stores.install();
		fx.env.blocks.registerBlock("Weapon", [](INI *ini) { WeaponStore::parseWeaponTemplateDefinitionGlobal(ini); });
	}
	const WeaponTemplate *make(const std::string &body)
	{
		const std::string err = loadError(fx.env, "w.ini", "Weapon W" + std::to_string(counter) + "\n" + body + "End\n");
		REQUIRE_MESSAGE(err.empty(), err);
		return stores.weapons().findWeaponTemplate("W" + std::to_string(counter++));
	}
	int counter = 0;
};

// FiringTrackerHost as ObjectWeapons supplies it: the owner's frame, position and continuous-fire bonus mask, and the reload call.
struct Port225TrackerHost : FiringTrackerHost
{
	std::uint32_t frame = 0;
	Coord3D pos{};
	std::uint32_t mask = 0;
	int reloads = 0;
	std::uint32_t currentFrame() override { return frame; }
	Coord3D ownerPosition() override { return pos; }
	std::uint32_t weaponBonusConditionMask() override { return mask; }
	void setWeaponBonusConditionMask(std::uint32_t m) override { mask = m; }
	void reloadAllWeapons() override { ++reloads; }
	// The owner's model condition bits (Object::clearAndSetModelConditionFlags, RW 0x68D607), recorded the way the object applies
	// them: the clear words first, then the set words.
	std::array<std::uint32_t, 19> conditions{};
	int conditionCalls = 0;
	void clearAndSetModelConditions(const ModelConditionBits &clr, const ModelConditionBits &set) override
	{
		++conditionCalls;
		for (std::size_t w = 0; w < conditions.size(); ++w)
		{
			conditions[w] = (conditions[w] & ~clr[w]) | set[w];
		}
	}
	bool conditionSet(int bit) const
	{
		return bit >= 0 && bit < 19 * 32 && ((conditions[(std::size_t)bit >> 5] >> (bit & 31)) & 1u) != 0;
	}
};
}

TEST_CASE("PORT-225 FiringTracker: a weapon with no ContinuousFireCoast keeps its consecutive shot count over updates")
{
	// No ContinuousFireCoast line: the tracker's cooldown frame (+ 0x3C) stays 0 (DONOR 0x004DEECB's `if (coast) ... else 0`).
	Port225World w;
	const WeaponTemplate *t = w.make("  ContinuousFireOne = 10\n  ContinuousFireTwo = 20\n  DelayBetweenShots = 200\n");
	REQUIRE(t != nullptr);
	CHECK(t->m_continuousFireCoastFrames == 0);
	Weapon weapon(t, 0, 0);
	weapon.setWhenWeCanFireAgain(3);
	Port225TrackerHost host;
	FiringTracker tracker;
	for (std::uint32_t f = 1; f <= 3; ++f)
	{
		host.frame = f;
		tracker.shotFired(host, weapon, 7, Coord3D{ 1, 2, 3 }, false);
	}
	CHECK(tracker.shotCount() == 3);
	CHECK(tracker.coastEnd() == 0u);
	// The owner stands still for ten frames. Retail's update leaves the tracker alone while no cooldown frame is pending
	// (DONOR 0x004DED3B, the `m_frameToStartCooldown != 0` gate) and then reports UPDATE_SLEEP_FOREVER (DONOR 0x004DEAB0).
	host.pos = tracker.lastOwnerPosition();
	for (std::uint32_t f = 4; f <= 13; ++f)
	{
		host.frame = f;
		const std::uint32_t sleep = tracker.update(host);
		CHECK(sleep == 0x3FFFFFFFu);
	}
	CHECK(tracker.shotCount() == 3);
	CHECK(tracker.getShotsAtTarget(true, 7, Coord3D{}) == 3);
	CHECK(host.reloads == 0);
}

TEST_CASE("PORT-225 FiringTracker: a pending cooldown polls in five frames, and the auto-reload keeps the module awake")
{
	// 2000 ms at the logic frame rate of 5: ten frames for both the coast and the idle auto-reload (PLAN rule 2, ms -> frames).
	Port225World w;
	const WeaponTemplate *t = w.make("  ContinuousFireCoast = 2000\n  AutoReloadWhenIdle = 2000\n  DelayBetweenShots = 200\n");
	REQUIRE(t != nullptr);
	CHECK(t->m_continuousFireCoastFrames == 10);
	CHECK(t->m_autoReloadWhenIdle == 10);
	Weapon weapon(t, 0, 0);
	weapon.setWhenWeCanFireAgain(3);
	Port225TrackerHost host;
	FiringTracker tracker;
	host.frame = 5;
	tracker.shotFired(host, weapon, 7, Coord3D{ 1, 2, 3 }, false);
	CHECK(tracker.coastEnd() == 13u);          // next shot (3) + coast (10)
	CHECK(tracker.autoReloadDeadline() == 15u); // this frame (5) + AutoReloadWhenIdle (10)
	host.pos = tracker.lastOwnerPosition();
	const std::uint32_t mean = 1u << FiringTracker::CONDITION_CONTINUOUS_FIRE_MEAN;
	// Before the cooldown frame: the reload is still pending, so the module may sleep one frame only.
	host.frame = 10;
	CHECK(tracker.update(host) == 1u);
	CHECK(tracker.shotCount() == 1);
	CHECK(host.reloads == 0);
	// Past the cooldown frame with a continuous-fire condition on: the cooldown runs and the module wakes after five frames
	// (DONOR 0x004DED3B's UPDATE_SLEEP(LogicFramesPerSecond)); coolDown(false)'s else arm keeps the frame it just pushed ahead.
	host.mask = mean;
	host.frame = 14;
	CHECK(tracker.update(host) == 5u);
	CHECK(tracker.coastEnd() == 19u);
	CHECK(tracker.shotCount() == 0);
	CHECK(host.mask == 0u);
	// The reload frame: the reload runs first (DONOR's step 2) and the cooldown step still ends the update.
	host.frame = 15;
	CHECK(tracker.update(host) == 1u);
	CHECK(host.reloads == 1);
	CHECK(tracker.autoReloadDeadline() == 0u);
	// A pending cooldown frame is a timer calcTimeToSleep must see: FOREVER here would leave the poll hanging (DONOR 0x004DEAB0).
	host.mask = mean;
	host.frame = 20;
	CHECK(tracker.update(host) == 5u);
	CHECK(tracker.coastEnd() == 25u);
}

TEST_CASE("PORT-225 FiringTracker: a forced (hard) shot records its frame like a normal one")
{
	Port225World w;
	const WeaponTemplate *t = w.make("  DelayBetweenShots = 200\n");
	REQUIRE(t != nullptr);
	Weapon weapon(t, 0, 0);
	weapon.setWhenWeCanFireAgain(3);
	Port225TrackerHost host;
	FiringTracker tracker;
	host.frame = 42;
	// DONOR 0x004DEECB writes the frame and the owner position before the forced-reset branch; the forced shot leaves the count at 1
	// and then the reset zeroes it, exactly as here.
	tracker.shotFired(host, weapon, 7, Coord3D{ 1, 2, 3 }, true);
	CHECK(tracker.lastShotFrame() == 42u);
	CHECK(tracker.shotCount() == 0);
}

TEST_CASE("PORT-225 FiringTracker: a moving owner still hard-resets the tracker and drops the continuous-fire bonus")
{
	// The neighbouring behaviour the update rewrite must keep: movement is step 1 and still clears the continuous-fire conditions.
	Port225World w;
	// One and ten keep the continuous-fire table out of the way: one shot is below Two, so no speed-up and no reset.
	const WeaponTemplate *t = w.make("  ContinuousFireCoast = 2000\n  ContinuousFireOne = 1\n  ContinuousFireTwo = 10\n  DelayBetweenShots = 200\n");
	REQUIRE(t != nullptr);
	Weapon weapon(t, 0, 0);
	weapon.setWhenWeCanFireAgain(3);
	Port225TrackerHost host;
	FiringTracker tracker;
	const std::uint32_t mean = 1u << FiringTracker::CONDITION_CONTINUOUS_FIRE_MEAN;
	host.frame = 5;
	host.mask = mean;
	tracker.shotFired(host, weapon, 7, Coord3D{ 1, 2, 3 }, false);
	CHECK(tracker.shotCount() == 1);
	host.frame = 6;
	host.pos = Coord3D{ 50, 50, 50 }; // the owner walked away from the last shot position
	const std::uint32_t sleep = tracker.update(host);
	CHECK(tracker.shotCount() == 0);
	CHECK(host.mask == 0u);
	// The hard reset also clears the cooldown frame (coolDown(true)'s else arm), so nothing is pending: the module may sleep
	// until it is woken (DONOR 0x004DEAB0).
	CHECK(sleep == 0x3FFFFFFFu);
}

TEST_CASE("PORT-225 FiringTracker: the model condition name table carries the continuous-fire bits the tracker drives")
{
	// The tracker's coolDown / speedUp write bits 115 / 116 / 117 of the binary's model condition table (DONOR BFME2's
	// ModelConditionFlagType); the port's own name table (RW 0xD9FAD8) must agree, or the animations of every unit would be driven
	// by the wrong bits.
	CHECK(std::string(TheModelConditionNames[FiringTracker::MODELCOND_CONTINUOUS_FIRE_SLOW]) == "CONTINUOUS_FIRE_SLOW");
	CHECK(std::string(TheModelConditionNames[FiringTracker::MODELCOND_CONTINUOUS_FIRE_MEAN]) == "CONTINUOUS_FIRE_MEAN");
	CHECK(std::string(TheModelConditionNames[FiringTracker::MODELCOND_CONTINUOUS_FIRE_FAST]) == "CONTINUOUS_FIRE_FAST");
}

TEST_CASE("PORT-225 FiringTracker: speedUp and coolDown set and clear the owner's continuous-fire model conditions")
{
	Port225World w;
	// One and one: the second shot is already past ContinuousFireTwo, so the third shot speeds the owner up again.
	const WeaponTemplate *t = w.make("  ContinuousFireOne = 1\n  ContinuousFireTwo = 1\n  ContinuousFireCoast = 2000\n  DelayBetweenShots = 200\n");
	REQUIRE(t != nullptr);
	Weapon weapon(t, 0, 0);
	weapon.setWhenWeCanFireAgain(3);
	Port225TrackerHost host;
	FiringTracker tracker;
	const int slow = (int)FiringTracker::MODELCOND_CONTINUOUS_FIRE_SLOW;
	const int mean = (int)FiringTracker::MODELCOND_CONTINUOUS_FIRE_MEAN;
	const int fast = (int)FiringTracker::MODELCOND_CONTINUOUS_FIRE_FAST;

	host.frame = 1;
	tracker.shotFired(host, weapon, 7, Coord3D{ 1, 2, 3 }, false);
	// One shot is not past ContinuousFireOne yet, so the tracker leaves the owner's conditions alone.
	CHECK(!host.conditionSet(mean));
	CHECK(host.conditionCalls == 0);

	host.frame = 2;
	tracker.shotFired(host, weapon, 7, Coord3D{ 1, 2, 3 }, false);
	// DONOR 0x004DEDD2, the none -> MEAN arm: MEAN set, FAST and SLOW cleared.
	CHECK(host.conditionSet(mean));
	CHECK(!host.conditionSet(fast));
	CHECK(!host.conditionSet(slow));

	host.frame = 3;
	tracker.shotFired(host, weapon, 7, Coord3D{ 1, 2, 3 }, false);
	// DONOR 0x004DEDD2, the MEAN -> FAST arm: FAST set, MEAN and SLOW cleared.
	CHECK(host.conditionSet(fast));
	CHECK(!host.conditionSet(mean));
	CHECK(!host.conditionSet(slow));

	// DONOR 0x004DEC88, the `!forceReset && (FAST || MEAN)` arm: the owner drops to CONTINUOUS_FIRE_SLOW.
	tracker.reset(host, false);
	CHECK(host.conditionSet(slow));
	CHECK(!host.conditionSet(mean));
	CHECK(!host.conditionSet(fast));

	// DONOR 0x004DEC88, the else arm (a forced reset, or no continuous-fire condition): all three cleared.
	tracker.reset(host, true);
	CHECK(!host.conditionSet(slow));
	CHECK(!host.conditionSet(mean));
	CHECK(!host.conditionSet(fast));
}
