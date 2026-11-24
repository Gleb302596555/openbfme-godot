// OpenBFME unit tests: PORT-328 (issue #845) — the stops of Player, PlayerTemplate, GameInfo, SidesList and GlobalData.
//
// The mechanisms these tests hold, with the RotWK 2.01 evidence they were derived from (read with `rw.py dis` on game.dat):
//
//   S-521 (intrinsic sciences) — Player::addUpgrade with a player-type upgrade logs nothing by itself; the PlayerTemplate
//   parsers do the check. Retail's parseScienceVector (RW 0x73B4A0) resolves every science name against TheScienceStore right
//   there and raises the INI error of RW 0x73A386 for an unknown one (RW 0x73B4B6 .. 0x73B4C6); PlayerScience::resetSciences
//   (RW 0x6AED4B) then only copies the already-resolved sciences (RW 0x6AED5C..0x6AED81), it looks nothing up. The port stored
//   the names at parse time and resolved them at reset, so an unknown name was a runtime error at game start instead of the
//   INI error. The PlayerTemplate field now looks the name up at parse time when TheScienceStore is installed (lane SPELL-1:
//   it loads before the PlayerTemplate store); without a store (unit fixtures that never install one) the name is kept and the
//   reset reports it, as before.
//
//   NOT PORTED in this PR: S-482 (the upgrade walk order, RW 0x6AE483 / RW 0x6AE546 — retail walks the player's per-player
//   team-prototype list, each prototype's teams newest first, each team's members newest first; the port walks the global object
//   list). A port of that walk was tried under this issue and moved the skirmish games (the Linux GCC run of
//   test_skirmish_ai_vs_ai.cpp failed its pin at line 1006, `CHECK(run.defeatFrame == g.defeatFrame)`); the values are engine
//   pins that stay as they are on main and the retail binary did not survive the session, so the walk is not ported here.
//   Expectations are the binary's, not the code's; every test runs on synthetic INI. No game files are read.

#include "doctest.h"
#include "LogicTestUtil.h"

#include "Common/Player.h"
#include "Common/PlayerTemplate.h"
#include "GameLogic/SpellStores.h"
#include "GameLogic/GameLogic.h"

#include <string>
#include <vector>

using namespace logictest;

namespace
{

// the three stores of the spell book with two sciences and one rank (test_audio4.cpp's Sciences pattern, lane SPELL-1): the real
// parsers of `Science` / `Rank`, installed around the assertions
struct ScienceEnv
{
	initest::Fixture fx;
	NameKeyGenerator keys;
	SpellStores stores;
	ScienceEnv()
		: stores(keys)
	{
		keys.init();
		stores.install();
		SpellStores::registerBlocks(fx.env.blocks);
		const std::string err = initest::loadError(fx.env, "science.ini",
			"Science SCIENCE_Known\nEnd\n"
			"Science SCIENCE_AlsoKnown\nEnd\n"
			"Rank 1\n  SkillPointsNeededDefault = 0\nEnd\n");
		REQUIRE_MESSAGE(err.empty(), err);
	}
	~ScienceEnv() { stores.uninstall(); }
};

} // namespace

TEST_CASE("PORT-328 S-521: an unknown IntrinsicSciences name is the INI error at load, not a reset-time error (RW 0x73B4A0 / 0x73A386)")
{
	ScienceEnv sci;
	logictest::LogicWorld lw(RandomAlgorithm::ZH_CarryChain);
	// the names retail rejects with the INI error: an unknown science in IntrinsicSciences and in IntrinsicSciencesMP
	const std::string bad = lw.w.load("PlayerTemplate PT_Bad\n  IntrinsicSciences = SCIENCE_Known SCIENCE_NoSuchScience\nEnd\n", INI_LOAD_OVERWRITE, "pt_bad.ini");
	CHECK_MESSAGE(!bad.empty(), "an unknown IntrinsicSciences science loaded");
	CHECK(bad.find("SCIENCE_NoSuchScience") != std::string::npos);
	const std::string badMp = lw.w.load("PlayerTemplate PT_BadMP\n  IntrinsicSciencesMP = SCIENCE_NoSuchScienceMP\nEnd\n", INI_LOAD_OVERWRITE, "pt_bad_mp.ini");
	CHECK(badMp.find("SCIENCE_NoSuchScienceMP") != std::string::npos);
	// the known ones load cleanly (the neighbor of the rejection)
	const std::string good = lw.w.load("PlayerTemplate PT_Good\n  IntrinsicSciences = SCIENCE_Known SCIENCE_AlsoKnown\nEnd\n", INI_LOAD_OVERWRITE, "pt_good.ini");
	CHECK_MESSAGE(good.empty(), good);
	const PlayerTemplate *t = lw.templates.findPlayerTemplate("PT_Good");
	REQUIRE(t);
	CHECK(t->m_intrinsicSciences == std::vector<std::string>{ "SCIENCE_Known", "SCIENCE_AlsoKnown" });
	// "None" (stricmp) clears the list and ends the line, as in RW (RW 0x73B4B6)
	const std::string none = lw.w.load("PlayerTemplate PT_None\n  IntrinsicSciences = None\nEnd\n", INI_LOAD_OVERWRITE, "pt_none.ini");
	CHECK_MESSAGE(none.empty(), none);
	const PlayerTemplate *nt = lw.templates.findPlayerTemplate("PT_None");
	REQUIRE(nt);
	CHECK(nt->m_intrinsicSciences.empty());
}
