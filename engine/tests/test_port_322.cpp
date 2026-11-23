// OpenBFME. Port 322 (issue: the open stops of the game loop, object lifetime and the logic frame - GameLogic), lane PORT-322.
//
// The two mechanisms this file pins, both read from the RotWK 2.01 disassembly (`game.dat`, RW = a VA in that image; no retail file is needed to
// run these cases: every world is synthetic INI):
//
//  1. The AI row of phase 5. GameLogic::update phase 5 calls TheAI->update (RW 0x62EB63 .. 0x62EB71, vtbl slot 0x28 of 0xDE4B40 = 0x6FEC63),
//     which does two things: it processes the pathfinder queue once more (RW 0x6F2364 on [TheAI + 0x10]) and then calls ThePlayerList->update
//     (RW 0xDE4928, vslot 0x28 = RW 0x6A84DB), whose 20-slot walk runs Player::update (RW 0x6AF269) per player. Player::update records the
//     per-frame score statistics (ScoreKeeper::recordPerFrameStats RW 0x79F704 from RW 0x6AF3AD with TheGameLogic + 0x40). The port used to run
//     that per-player work at the end of phase 6 as an inference (stop S-1061); these cases pin retail's place: phase 5, once per logic frame,
//     after the module vectors and before the deferred-entries drain (RW 0x62EB74) and the destroy list (RW 0x62EBB1).
//  2. The end-of-frame pass of phase 1 (RW 0x62EC55 .. 0x62ECAA): per object, the disabled expiry (RW 0x690A42), IGNORE_AI_COMMAND (RW 0x690AB9),
//     NO_COLLISIONS (RW 0x690AE5) and then, for the same object, updatePendingDamage (RW 0x62EC9B -> 0x697EB6), unguarded. These cases hold the
//     neighbouring behaviour (the disabled expiry and a due queued hit both still happen inside that pass).

#include "doctest.h"

#include "CombatTestUtil.h"
#include "FlatTerrainTestUtil.h"
#include "LogicTestUtil.h"

#include "Common/Player.h"
#include "Common/ScoreKeeper.h"

#include <string>
#include <vector>

namespace
{
// the templates the phase cases need: a plain object with one update module (no body, no combat)
const char kPort322Objects[] =
	"Object Plain\n"
	"End\n"
	"Object One\n"
	"  Behavior = LifetimeUpdate ModuleTag_L\n"
	"  End\n"
	"End\n";

// the phase work row `row` is still reported as unported work by `logic` (GameLogic::unportedPhaseWork, one line per row with its source)
bool unported(const GameLogic &logic, const char *row)
{
	const std::string needle = std::string(" ") + row + ": ";
	for (const std::string &line : logic.unportedPhaseWork())
	{
		if (line.find(needle) != std::string::npos)
		{
			return true;
		}
	}
	return false;
}

struct Port322Fx : logictest::LogicWorld
{
	Port322Fx()
	{
		REQUIRE_MESSAGE(loadError.empty(), loadError);
		const std::string err = w.load(kPort322Objects);
		REQUIRE_MESSAGE(err.empty(), err);
	}
};

// ScoreKeeper::recordPerFrameStats (RW 0x79F704) marks the entry of the frame it recorded (ScoreKeeper.h PerFrameStats::recorded); the vector grows
// up to frame + 1, so an entry is the recorded flag of that frame, absent = the player did not record it.
bool recorded(const ScoreKeeper &keeper, unsigned frame)
{
	const std::vector<ScoreKeeper::PerFrameStats> &perFrame = keeper.perFrameStats();
	return frame < perFrame.size() && perFrame[frame].recorded;
}
} // namespace

// RW 0x62EB71 -> TheAI::update RW 0x6FEC63 -> ThePlayerList->update RW 0x6A84DB -> Player::update RW 0x6AF269, whose step 4 is
// ScoreKeeper::recordPerFrameStats (RW 0x6AF3AD, RW 0x79F704). The frame's record exists once phase 5 has run and phase 6 never makes one:
// with the old phase-6 placement, phase 5 recorded nothing (the first CHECK fails) and a lone phase 6 recorded the frame (the last CHECK fails).
TEST_CASE("PORT-322: the player list's frame update runs in phase 5 through the AI row, not at the end of phase 6 (RW 0x6FEC63 -> 0x6A84DB)")
{
	Port322Fx f;
	Player *alice = f.players.findPlayerWithName("Alice"); // FactionA: PlayableSide = Yes, so recordPerFrameStats records her (RW 0x6AAC66)
	REQUIRE(alice != nullptr);
	ScoreKeeper &keeper = alice->getScoreKeeper();

	// phase 1 advances the frame to 1 (RW 0x62E577); phases 2 .. 4 do not reach the player list
	f.logic->update(1);
	for (int phase = 2; phase <= 4; ++phase)
	{
		f.logic->update(phase);
	}
	CHECK(f.logic->getFrame() == 1);
	CHECK_FALSE(recorded(keeper, 1));

	f.logic->update(5);
	CHECK(recorded(keeper, 1)); // RW 0x62EB71: the player list's update is part of phase 5

	// phase 6 adds nothing: the frame was recorded once, by phase 5
	const size_t entries = keeper.perFrameStats().size();
	f.logic->update(6);
	CHECK(keeper.perFrameStats().size() == entries);

	// a frame whose phase 5 did not run records nothing even though phase 6 runs (the old placement recorded it there)
	Port322Fx g;
	Player *bob = g.players.findPlayerWithName("Bob");
	REQUIRE(bob != nullptr);
	g.logic->update(1);
	g.logic->update(6);
	CHECK_FALSE(recorded(bob->getScoreKeeper(), 1));
}

// RW 0x62EB63 .. 0x62EB71: the aiUpdate row is retail's phase-5 slot of TheAI::update. It runs exactly once per logic frame, in phase 5; before the
// port had the row here it never ran at all (the row was reported as unported work).
TEST_CASE("PORT-322: the aiUpdate row runs once per logic frame, inside phase 5 (RW 0x62EB63 .. 0x62EB71)")
{
	Port322Fx f;
	int calls = 0;
	int lastPhase = -1;
	REQUIRE(f.logic->installPhaseWork("aiUpdate", [&] {
		++calls;
		lastPhase = (int)f.logic->getLastPhase();
	}));

	for (int phase = 1; phase <= 4; ++phase)
	{
		f.logic->update(phase);
	}
	CHECK(calls == 0);
	f.logic->update(5);
	CHECK(calls == 1);
	CHECK(lastPhase == 5);
	f.logic->update(6);
	CHECK(calls == 1);
	f.logic->runLogicFrame();
	CHECK(calls == 2); // once in the second frame too
}

// RW 0x6FEC63: TheAI::update's first call is the pathfinder queue (RW 0x6F2364 on TheAI + 0x10). The AI world owns that half of the row: attaching
// it installs the row, detaching it takes the row's function out again (the logic keeps the frame running without an AI).
TEST_CASE("PORT-322: the AI world installs the queue half of the aiUpdate row and drops it on detach (RW 0x6FEC63)")
{
	Port322Fx f;
	CHECK(unported(*f.logic, "aiUpdate"));
	{
		flatterrain::Flat flat(f, 8); // a flat 8x8 map: the AI world attaches its rows and builds the grid
		CHECK_FALSE(unported(*f.logic, "aiUpdate"));
		flat.ai->detach();
		CHECK(unported(*f.logic, "aiUpdate"));
	}
	f.logic->reset(); // nothing of the map outlives the world (the AI world is gone already)
}

// RW 0x62EC55 .. 0x62ECAA (the neighbouring behaviour of the end-of-frame pass): an object's disabled type whose expiry frame arrived is cleared by
// the phase-1 pass (RW 0x690A42 behind RW 0x9325B4 on Object + 0x1C8), a type whose frame is still ahead stays. The pass keeps running for every
// object of the list after the pending damage moved into it (RW 0x62EC9B).
TEST_CASE("PORT-322: the phase-1 end-of-frame pass still clears an arrived disabled expiry and keeps a future one (RW 0x62EC55 .. 0x62ECAA)")
{
	Port322Fx f;
	Object *due = f.make("One");    // registered at frame 0, so its disabled expiry can be frame 1
	Object *later = f.make("Plain");
	REQUIRE(due != nullptr);
	REQUIRE(later != nullptr);
	due->setDisabled(DISABLED_HELD, 1);   // RW 0x6907F1: expires at frame 1, which phase 1 reaches first
	later->setDisabled(DISABLED_HELD, 3); // still disabled at frame 1
	CHECK(due->isDisabledByType(DISABLED_HELD));
	CHECK(later->isDisabledByType(DISABLED_HELD));

	f.logic->update(1);
	for (int phase = 2; phase <= 6; ++phase)
	{
		f.logic->update(phase);
	}
	CHECK(f.logic->getFrame() == 1);
	CHECK_FALSE(due->isDisabledByType(DISABLED_HELD));
	CHECK(later->isDisabledByType(DISABLED_HELD));
}

// RW 0x62EC9B (-> 0x697EB6) is the same pass: a queued hit (Object::attemptDamage puts a hit with a delay into the pending list, RW 0x695031) whose
// delay runs out is applied by phase 1's pass. Delay 0.5 is below one frame, so one pass applies it (PendingDamageDue: delay -= 1, due when 0 > delay).
TEST_CASE("PORT-322: a queued hit whose delay ran out is applied by the phase-1 end-of-frame pass (RW 0x62EC9B -> 0x697EB6)")
{
	combattest::CombatWorld w;
	Object *dummy = w.unit("Dummy", 'B', 400, 300); // 100 health, the body the hit needs
	REQUIRE(dummy != nullptr);
	REQUIRE(w.health(dummy) == 100.0f);
	DamageInfo hit;
	hit.m_input.m_sourceID = INVALID_ID;
	hit.m_input.m_amount = 20.0f;
	hit.m_input.m_damageType = DAMAGE_UNRESISTABLE;
	hit.m_input.m_delay = 0.5f;
	dummy->attemptDamage(hit);
	REQUIRE(dummy->pendingDamageCount() == 1); // the delay put it into the pending list, not applied yet
	CHECK(w.health(dummy) == 100.0f);

	w.logic->update(1); // phase 1 only: the end-of-frame pass
	CHECK(dummy->pendingDamageCount() == 0);
	CHECK(w.health(dummy) == 80.0f);
}
