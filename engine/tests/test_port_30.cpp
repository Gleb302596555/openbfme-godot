// OpenBFME. PORT-30 (issue #102) verdict: RotWK does not read TimeFrozenFromPenalty.
// Correction 2026-11-08: row RW 0xC66B68 (table RW 0xC66B18 starts with SpecialObjectName), parser RW 0x73A429, defaults RW 0x89D1C1 zeroes +0x1C.
// RW 0x8CA12A in RW 0x8CA072 belongs to a deploy module (ctor 0x8C9B5C, vtable 0xC74910, table 0xC747E8 LowerDelay +0x18 / RaiseDelay +0x1C) reading RaiseDelay.
// OneRingPenaltyUpdate's own code (ctor 0x89D21F, vtables 0xC669C8/0xC669B8, update 0x89D647, helpers 0x89D0E3..0x89D712) reads +0xC,+0x10,+0x14,+0x18 never +0x1C.
// No other code looks up this module's data by name/interface to read +0x1C (searched .text refs to its table/maker and +0x1C via module lookup; none).
// Verdict: RotWK does not read TimeFrozenFromPenalty — emotion request RW 0x68F37F(5,none,1) is the only freeze-like thing in penalty.
// Tests pin that: TimeFrozenFromPenalty=0 or 5000 never freezes the bearer (no DISABLED_TEMPORARILY_BUSY), other penalty effects unchanged, parsing only.
#include "doctest.h"

#include "CombatTestUtil.h"
#include "Common/StateHash.h"
#include "Common/XferLoad.h"
#include "Common/XferSave.h"
#include "GameLogic/Module/InvisibilityModules.h"
#include "GameLogic/Module/OneRingPenaltyUpdate.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/SnapshotXfer.h"

using namespace combattest;

namespace
{
const char kPlainBody[] = "  Body = ActiveBody ModuleTag_Body\n    MaxHealth = 1000\n  End\n";
const char kAI[] = "  Behavior = AIUpdateInterface ModuleTag_AI\n  End\n";

std::string unitWithFrodo(const char *name, int radius, float frozenMs)
{
	std::string s;
	s += std::string("Object ") + name + "\n";
	s += "  KindOf = INFANTRY SELECTABLE SCORE\n";
	s += "  Geometry = CYLINDER\n";
	s += "  GeometryMajorRadius = " + std::to_string(radius) + "\n";
	s += "  GeometryMinorRadius = " + std::to_string(radius) + "\n";
	s += "  GeometryHeight = 20\n";
	s += "  ArmorSet\n    Conditions = None\n    Armor = NoArmor\n  End\n";
	s += kPlainBody;
	s += kAI;
	s += "  Behavior = StealthUpdate ModuleTag_Stealth\n";
	s += "    OneRingDelayOn = 200\n";
	s += "    OneRingDelayOff = 200\n";
	s += "    RingAnimTimeOn = 200\n";
	s += "    RingAnimTimeOff = 200\n";
	s += "    RingDelayAfterRemoving = 2000\n";
	s += "  End\n";
	s += "  Behavior = OneRingPenaltyUpdate ModuleTag_Ring\n";
	s += "    SpecialObjectName = TheEye\n";
	s += "    RingTimeBeforeSpawning = 400\n"; // 12 frames
	s += "    TimeSpentRoamingAround = 400\n"; // 12 frames
	s += "    TimeRingPowerSuppressed = 2000\n"; // 60 frames
	s += "    StartingDistanceFromMe = 100\n";
	s += "    TimeFrozenFromPenalty = " + std::to_string((int)frozenMs) + "\n";
	s += "  End\n";
	s += "End\n";
	return s;
}

std::string eyeObject()
{
	return std::string(
		"Object TheEye\n"
		"  KindOf = INFANTRY SELECTABLE SCORE\n"
		"  Geometry = CYLINDER\n"
		"  GeometryMajorRadius = 8\n"
		"  GeometryMinorRadius = 8\n"
		"  GeometryHeight = 20\n"
		"  ArmorSet\n    Conditions = None\n    Armor = NoArmor\n  End\n"
		"  Body = ActiveBody ModuleTag_Body\n    MaxHealth = 1000\n  End\n"
		"  Behavior = AIUpdateInterface ModuleTag_AI\n  End\n"
		"End\n");
}
const unsigned kStatusCanStealth = 0x12;
OneRingPenaltyUpdate *ringOf(Object *o) { return dynamic_cast<OneRingPenaltyUpdate *>(o->findModule("OneRingPenaltyUpdate")); }
} // namespace

TEST_CASE("PORT-30 OneRingPenaltyUpdate TimeFrozenFromPenalty is parsed but never freezes bearer (RW 0xC66B68 +0x1C never read: 0x89D21F/0x89D647/0x89D0E3..0x89D712 only +0xC/+0x10/+0x14/+0x18; 0x8CA072/0x8CA12A is deploy LowerDelay/RaiseDelay)")
{
	for (float frozen : {0.0f, 1000.0f, 5000.0f})
	{
		CAPTURE(frozen);
		const std::string objects = unitWithFrodo("Frodo", 8, frozen) + eyeObject();
		CombatWorld w(objects.c_str());
		Object *frodo = w.unit("Frodo", 'A', 400, 400);
		REQUIRE(frodo != nullptr);
		StealthUpdate *stealth = StealthUpdate::of(*frodo);
		OneRingPenaltyUpdate *ring = ringOf(frodo);
		REQUIRE(stealth != nullptr);
		REQUIRE(ring != nullptr);
		REQUIRE(ring->penaltyFrame() == 0);
		REQUIRE_FALSE(frodo->isDisabledByType(DISABLED_TEMPORARILY_BUSY));
		stealth->toggleRing();
		REQUIRE(stealth->ringWorn());
		for (int i = 0; i < 60 && ring->penaltyFrame() == 0; ++i) w.frames(1);
		REQUIRE(ring->penaltyFrame() != 0);
		// The field is not read: no DISABLED_TEMPORARILY_BUSY at penalty frame, nor later, regardless of value.
		CHECK_FALSE(frodo->isDisabledByType(DISABLED_TEMPORARILY_BUSY));
		for (int i = 0; i < 5; ++i)
		{
			w.frames(1);
			CHECK_FALSE(frodo->isDisabledByType(DISABLED_TEMPORARILY_BUSY));
		}
	}
}

TEST_CASE("PORT-30 OneRingPenaltyUpdate TimeFrozenFromPenalty zero and non-zero give the same penalty frame by frame (RW 0x89D41B, 0x89D3E4)")
{
	// review (Claude): the PR's version waited 12 frames for TimeRingPowerSuppressed = 2000 ms, which is 60 logic frames at 30 Hz, and was never run. The
	// verdict's claim is that + 0x1C changes nothing: two worlds that differ only in the field must agree on every frame through the penalty and its end.
	struct Run
	{
		CombatWorld w;
		Object *frodo;
		StealthUpdate *stealth;
		OneRingPenaltyUpdate *ring;
		explicit Run(float frozen) : w((unitWithFrodo("Frodo", 8, frozen) + eyeObject()).c_str())
		{
			frodo = w.unit("Frodo", 'A', 400, 400);
			REQUIRE(frodo != nullptr);
			stealth = StealthUpdate::of(*frodo);
			ring = ringOf(frodo);
			REQUIRE(stealth != nullptr);
			REQUIRE(ring != nullptr);
			stealth->toggleRing();
		}
	};
	Run a(0.0f);
	Run b(5000.0f);
	bool penaltySeen = false;
	bool penaltyEnded = false;
	for (int f = 0; f < 150; ++f)
	{
		a.w.frames(1);
		b.w.frames(1);
		CAPTURE(f);
		REQUIRE(a.ring->penaltyFrame() == b.ring->penaltyFrame());
		REQUIRE(a.ring->ringFrame() == b.ring->ringFrame());
		REQUIRE((a.ring->eye() == INVALID_ID) == (b.ring->eye() == INVALID_ID));
		REQUIRE(a.stealth->ringWorn() == b.stealth->ringWorn());
		REQUIRE(a.frodo->testStatus(kStatusCanStealth) == b.frodo->testStatus(kStatusCanStealth));
		REQUIRE(a.frodo->isDisabledByType(DISABLED_TEMPORARILY_BUSY) == b.frodo->isDisabledByType(DISABLED_TEMPORARILY_BUSY));
		if (a.ring->penaltyFrame() != 0)
		{
			penaltySeen = true;
		}
		else if (penaltySeen)
		{
			penaltyEnded = true;
		}
	}
	CHECK(penaltySeen);
	CHECK(penaltyEnded); // RW 0x89D3E4 after TimeRingPowerSuppressed (60 frames)
}

TEST_CASE("PORT-30 OneRingPenaltyUpdate TimeFrozenFromPenalty does not enter xfer/crc (S-5604, OpenBFME native)")
{
	const std::string objects = unitWithFrodo("Frodo", 8, 5000.0f) + eyeObject();
	CombatWorld w(objects.c_str());
	Object *frodo = w.unit("Frodo", 'A', 400, 400);
	StealthUpdate *stealth = StealthUpdate::of(*frodo);
	OneRingPenaltyUpdate *ring = ringOf(frodo);
	stealth->toggleRing();
	for (int i = 0; i < 60 && ring->penaltyFrame() == 0; ++i) w.frames(1);
	REQUIRE(ring->penaltyFrame() != 0);
	StateHasher h1;
	ring->crc(h1);
	auto v1 = h1.value();
	XferSave save;
	ring->xfer(save);
	CombatWorld w2(objects.c_str());
	Object *frodo2 = w2.unit("Frodo", 'A', 400, 400);
	OneRingPenaltyUpdate *ring2 = ringOf(frodo2);
	XferLoad load(save.data());
	ring2->xfer(load);
	CHECK(ring2->penaltyFrame() == ring->penaltyFrame());
	StateHasher h2;
	ring2->crc(h2);
	CHECK(h2.value() == v1);
	CHECK_FALSE(frodo2->isDisabledByType(DISABLED_TEMPORARILY_BUSY));
}
