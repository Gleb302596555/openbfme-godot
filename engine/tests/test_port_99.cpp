// OpenBFME unit tests: PORT-99 (issue #259) — PlayerTemplate/LightPointsUpSound, "the sound when the player's power points go up".
//
// What RotWK 2.01 does (addresses read with tools/mod/rw.py, see docs/ennorath/PORT99_RW_ANALYSIS.md):
//   * the row RW 0xBF8488 of the template table RW 0xBF81A8 parses the name into PlayerTemplate + 0x1A4 (parser RW 0x73B217); the constructor
//     RW 0x5FE3F1 zeroes it (RW 0x5FE566), the copy assignment copies it (RW 0x5FDE7A with helper RW 0x4A86AC), the destructor frees it (RW 0x5FE243).
//   * the ONLY reader of + 0x1A4 in the image is the body of the script action PLAYER_GIVE_LIGHT_POINTS (RW 0x7BD2D0 -> RW 0x6AB148 -> RW 0x6AB178,
//     copied out at RW 0x6AB191 and played for the player at RW 0x6AB1B6), which adds to the light-point tracker at Player + 0x338 (RW 0x8309E3,
//     points + 0x33C) — the War of the Ring light points (spent by SpecialPower/LightPointCost, RW 0x6AA91D, stop S-531), not the skirmish spell book's
//     power points, which live in PlayerScience + 0x1C (RW 0x782764): the path RW 0x6AA980 (the Player override of the science slot 2) never reads
//     + 0x1A4, and neither does the cheat.
//   * the script action family that would reach it (RW 0x7BD2D0 / 0x7BD311 / 0x7BD34F / 0x7BD390 with TheLightPointSystem RW 0xDE8C04) is not
//     ported (S-531), so the port keeps the field parsed and unused, exactly as rotWK's skirmish paths do. These tests pin that, and they fail if
//     someone wires the field into the skirmish power-point path without a reader in RW.
//
// No game files are needed: the templates and the scores are synthetic.

#include "doctest.h"

#include "LogicTestUtil.h"

#include "Common/Audio/AudioEntryPoints.h"
#include "Common/Player.h"
#include "Common/PlayerScience.h"
#include "Common/PlayerTemplate.h"

#include <cstdint>
#include <string>

namespace
{
// the fixture's faction gets the row (an Ennorath-like name, a synthetic INI: the mod's files are never read here)
const char kWithLightPointsUpSound[] =
	"PlayerTemplate FactionA\n"
	"  LightPointsUpSound = GondorLightPointsUp\n"
	"End\n";

// the player whose template's Side is `side` (the fixture creates Alice on FactionA / Alpha and Bob on FactionB / Beta)
Player *playerOfSide(logictest::LogicWorld &lw, const char *side)
{
	for (int i = 0; i < lw.players.getPlayerCount(); ++i)
	{
		Player *p = lw.players.getNthPlayer(i);
		if (p && p->getPlayerTemplate() && p->getPlayerTemplate()->m_side == side)
		{
			return p;
		}
	}
	return nullptr;
}
} // namespace

TEST_CASE("PORT-99 LightPointsUpSound: the template keeps the row (RW 0xBF8488, + 0x1A4) and the skirmish power points go up with no sound request "
		  "(RW 0x6AA980 -> RW 0x782764 reads no + 0x1A4)")
{
	logictest::LogicWorld lw(RandomAlgorithm::ZH_CarryChain, kWithLightPointsUpSound);
	REQUIRE_MESSAGE(lw.loadError.empty(), lw.loadError);
	const PlayerTemplate *pt = lw.templates.findPlayerTemplate("FactionA");
	REQUIRE(pt);
	CHECK(pt->m_lightPointsUpSound == "GondorLightPointsUp");
	// the neighbours keep their own rows (RW 0xBF8498 / RW 0xBF84A8 are untouched by this line)
	CHECK(pt->m_objectiveAddedSound.empty());
	CHECK(pt->m_objectiveCompletedSound.empty());

	Player *alice = playerOfSide(lw, "Alpha");
	REQUIRE(alice);
	REQUIRE(alice->getPlayerTemplate());
	CHECK(alice->getPlayerTemplate()->m_lightPointsUpSound == "GondorLightPointsUp");

	AudioApi::install(nullptr);
	const int pointsBefore = alice->science().getSciencePurchasePoints();
	const std::uint64_t soundsBefore = AudioApi::callsWithoutAudio();
	alice->science().addSciencePurchasePoints(3); // RW 0x6AA980 (the Player override) -> RW 0x79DB86 -> RW 0x782764
	REQUIRE(alice->science().getSciencePurchasePoints() == pointsBefore + 3);
	// the field is not reached by the skirmish path: no sound is requested at all
	CHECK(AudioApi::callsWithoutAudio() == soundsBefore);
}

TEST_CASE("PORT-99 LightPointsUpSound: a template without the row stores nothing and neither it nor a negative amount requests a sound "
		  "(the light-point bodies RW 0x6AB173 take a positive amount only)")
{
	logictest::LogicWorld lw;
	REQUIRE_MESSAGE(lw.loadError.empty(), lw.loadError);
	const PlayerTemplate *pt = lw.templates.findPlayerTemplate("FactionB");
	REQUIRE(pt);
	CHECK(pt->m_lightPointsUpSound.empty());

	Player *bob = playerOfSide(lw, "Beta");
	REQUIRE(bob);

	AudioApi::install(nullptr);
	const std::uint64_t soundsBefore = AudioApi::callsWithoutAudio();
	bob->science().addSciencePurchasePoints(4);
	CHECK(bob->science().getSciencePurchasePoints() == 4);
	// RW 0x782764 clamps at 0; a negative amount changes no sound either (RW 0x6AB173 would skip the sound anyway)
	bob->science().addSciencePurchasePoints(-10);
	CHECK(bob->science().getSciencePurchasePoints() == 0);
	CHECK(AudioApi::callsWithoutAudio() == soundsBefore);
}
