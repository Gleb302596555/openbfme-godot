// OpenBFME unit tests (Local 179, fork issue 960):
//   1. Script v>=4 LivingWorldTurnPhase bitmask parsing (RW 0x7B636C ReadScriptPlayerMask_Rva003B5918,
//      RW 0x7B616D token loop, RW 0x7B5038 BitFlags::parseSingleBit against RW 0xDB7ABC 7-bit table:
//      Planning=0, MoveArmies=1, ResolveBattles=2, FakePhaseOne=3, PlanRetreats=4, RetreatArmies=5, Complete=6).
//   2. ScriptEngine::shouldEvaluate LivingWorld turn-phase gating (RW 0x603878 -> RW 0x6038E4..0x603901).
// Synthetic tests only: no retail or mod files are required.
// GPL-3.0.

#include "doctest.h"
#include "ScriptTestUtil.h"

#include "GameLogic/ScriptEngine/ScriptEngine.h"
#include "GameLogic/ScriptEngine/Scripts.h"

#include <cstdint>
#include <stdexcept>
#include <string>

using namespace scripttest;

TEST_CASE("LOCAL-179 Script turnPhaseMask parsing matches RW 0x7B636C / 0x7B616D / 0x7B5038")
{
	// RW 0x7B638E: exact "ALL" -> 7-bit all-phases mask (0x7F)
	CHECK(parseScriptTurnPhaseMask("ALL") == 0x7Fu);

	// RW 0x7B504A: case-insensitive "NONE" -> 0
	CHECK(parseScriptTurnPhaseMask("NONE") == 0u);
	CHECK(parseScriptTurnPhaseMask("none") == 0u);

	// RW 0x7B5144 against RW 0xDB7ABC: plain tokens set bits (case-insensitive)
	CHECK(parseScriptTurnPhaseMask("Planning") == (1u << 0));
	CHECK(parseScriptTurnPhaseMask("planning ResolveBattles Complete") == ((1u << 0) | (1u << 2) | (1u << 6)));

	// RW 0x7B5081 / 0x7B50C1: '+' sets bit, '-' clears bit
	CHECK(parseScriptTurnPhaseMask("+MoveArmies +PlanRetreats") == ((1u << 1) | (1u << 4)));
	CHECK(parseScriptTurnPhaseMask("+Planning +MoveArmies -Planning") == (1u << 1));

	// RW 0x7B505B / 0x7B508B / 0x7B5101: mixing plain and +/- or NONE with others throws
	CHECK_THROWS_AS(parseScriptTurnPhaseMask("Planning +MoveArmies"), std::invalid_argument);
	CHECK_THROWS_AS(parseScriptTurnPhaseMask("+Planning MoveArmies"), std::invalid_argument);
	CHECK_THROWS_AS(parseScriptTurnPhaseMask("Planning NONE"), std::invalid_argument);
	CHECK_THROWS_AS(parseScriptTurnPhaseMask("NoSuchPhase"), std::invalid_argument);

	// Default-constructed Script (RW 0x7B4CF8) has all phases enabled (0x7F)
	Script def;
	CHECK(def.turnPhaseMask == kScriptAllTurnPhasesMask);
}

TEST_CASE("LOCAL-179 ScriptEngine shouldEvaluate gates LivingWorld scripts by turnPhaseMask (RW 0x603878)")
{
	// RW 0x6038E4..0x603901: when in LivingWorld phase P, script runs only if bit P is set in Script+0x24.
	auto makeScript = [](const char *name, const char *flagName, std::uint32_t mask) {
		auto s = script(name, { { cond("CONDITION_TRUE", {}) } }, { act("SET_FLAG", { sp(13, flagName), ip(4, 1) }) }, {}, true, true);
		s->turnPhaseMask = mask;
		return item(std::move(s));
	};

	ScriptGame g;
	g.lists[1].items.push_back(makeScript("OnlyResolve", "FlagResolve", parseScriptTurnPhaseMask("ResolveBattles")));
	g.lists[1].items.push_back(makeScript("OnlyPlanning", "FlagPlanning", parseScriptTurnPhaseMask("Planning")));
	g.lists[1].items.push_back(makeScript("NoPhases", "FlagNone", parseScriptTurnPhaseMask("NONE")));

	// LivingWorld phase 2 = ResolveBattles (RW 0xDB7ABC[2])
	g.start();
	g.engine().setLivingWorldTurnPhase(2);
	g.engine().update();

	const bool *fResolve = g.engine().findFlag("Alice/FlagResolve");
	const bool *fPlanning = g.engine().findFlag("Alice/FlagPlanning");
	const bool *fNone = g.engine().findFlag("Alice/FlagNone");
	CHECK((fResolve != nullptr && *fResolve));
	CHECK((fPlanning == nullptr || !*fPlanning));
	CHECK((fNone == nullptr || !*fNone));

	// Neighboring behavior: outside LivingWorld (phase = -1, RW 0x6038D6 not taken), turnPhaseMask is not checked
	g.engine().setLivingWorldTurnPhase(-1);
	g.engine().update();
	fPlanning = g.engine().findFlag("Alice/FlagPlanning");
	fNone = g.engine().findFlag("Alice/FlagNone");
	CHECK((fPlanning != nullptr && *fPlanning));
	CHECK((fNone != nullptr && *fNone));
}
