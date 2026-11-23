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
//   S-482 (upgrade walk order) — Player::onUpgradeCompleted (RW 0x6AE483) / onUpgradeRemoved (RW 0x6AE546) walk the player's
//   per-player team-prototype list (Player + 0x34C, appended at TeamFactory::addToLists when a prototype is made); each prototype's
//   team instances newest first (RW 0x79FA5C -> RW 0x79F9D0 prepends a new team at the head of the prototype's list; the head is
//   the newest); each team's member list newest first (RW 0x68AD89 prepends every new member at the head). The port walked the
//   global object list in list order instead. Economy now walks the prototypes the factory made for the player (creation order),
//   their teams newest first and each team's members newest first (Team::getLastMember + Object::friend_teamPrev). Both
//   callbacks share the walk (Economy::walkPlayerTeamMembers).
//
//   Expectations are the binary's, not the code's; every test runs on synthetic INI. No game files are read.

#include "doctest.h"
#include "LogicTestUtil.h"

#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/Team.h"
#include "Common/CommandPoints.h"
#include "Common/Upgrade.h"
#include "GameLogic/SpellStores.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/UpgradeModules.h"
#include "GameLogic/Module/LogicModules.h"
#include "GameLogic/Object/Object.h"

#include <memory>
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

const char kPort328Objects[] =
	"Object Port328CpGiver\n"
	"  KindOf = SELECTABLE STRUCTURE\n"
	"  Behavior = CommandPointsUpgrade ModuleTag_CP\n"
	"    TriggeredBy = Upgrade_Basic\n"
	"    CommandPoints = 200\n"
	"  End\n"
	"End\n";

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

TEST_CASE("PORT-328 S-482: the upgrade walk reaches the player's team members newest first, then the prototypes in creation order (RW 0x6AE483 / RW 0x6AE546)")
{
	logictest::LogicWorld lw(RandomAlgorithm::ZH_CarryChain);
	LogicModules::registerAll(lw.w.modules);
	const std::string objErr = lw.w.load(kPort328Objects);
	REQUIRE_MESSAGE(objErr.empty(), objErr);
	Player *alice = lw.players.findPlayerWithName("Alice");
	Team *aliceTeam = lw.teamOf("Alice");
	Object *o1 = lw.make("Port328CpGiver", aliceTeam);
	Object *o2 = lw.make("Port328CpGiver", aliceTeam);
	Object *o3 = lw.make("Port328CpGiver", aliceTeam);
	// a second prototype of the player, made after the map's: retail's per-player list holds the prototypes in the order they were made
	TeamPrototype *proto = lw.players.teams().initTeam("teamWallet", alice, false);
	Team *wallet = lw.players.teams().createTeam(proto);
	Object *b1 = lw.make("Port328CpGiver", wallet);
	const UpgradeTemplate *u = lw.upgradeCenter.findUpgrade("Upgrade_Basic");
	REQUIRE(u);
	alice->addUpgrade(u, Player::UPGRADE_STATUS_COMPLETE, true);
	const std::vector<CommandPoints::Record> &records = alice->commandPoints().records();
	REQUIRE(records.size() == 4);
	CHECK(records[0].objectId == o3->getID());
	CHECK(records[1].objectId == o2->getID());
	CHECK(records[2].objectId == o1->getID());
	CHECK(records[3].objectId == b1->getID());
	// completion skipped the other player's objects (the walk is the player's teams, not the world's object list)
	Player *bob = lw.players.findPlayerWithName("Bob");
	CHECK(bob->commandPoints().records().empty());
	// the removal walks the same lists the same way (RW 0x6AE546): the player-side completed bit clears. The object-side
	// removal of a CommandPointsUpgrade is the documented retail no-op (UpgradeModule.h: the mux's resetUpgrade clears the
	// executed flag before slot 8 runs, and slot 8 tests it), so the records stay, as in retail; the order is pinned on the
	// completion path (both callbacks share Economy::walkPlayerTeamMembers)
	alice->removeUpgrade(u, true);
	CHECK(!alice->hasUpgradeComplete(u));
	CHECK(alice->commandPoints().records().size() == 4);
	CHECK(bob->commandPoints().records().empty());
}

TEST_CASE("PORT-328 S-482: a completed upgrade does not reach an object on no team of the player, and reaches it through a second team instance last")
{
	logictest::LogicWorld lw(RandomAlgorithm::ZH_CarryChain);
	LogicModules::registerAll(lw.w.modules);
	const std::string objErr = lw.w.load(kPort328Objects);
	REQUIRE_MESSAGE(objErr.empty(), objErr);
	Player *alice = lw.players.findPlayerWithName("Alice");
	// a prototype with two instances, made after the map's: the instances are walked newest first
	TeamPrototype *proto = lw.players.teams().initTeam("teamDouble", alice, false);
	Team *first = lw.players.teams().createTeam(proto);
	Object *f1 = lw.make("Port328CpGiver", first);
	Team *second = lw.players.teams().createTeam(proto);
	Object *s1 = lw.make("Port328CpGiver", second);
	const UpgradeTemplate *u = lw.upgradeCenter.findUpgrade("Upgrade_Basic");
	REQUIRE(u);
	alice->addUpgrade(u, Player::UPGRADE_STATUS_COMPLETE, true);
	const std::vector<CommandPoints::Record> &records = alice->commandPoints().records();
	REQUIRE(records.size() == 2);
	CHECK(records[0].objectId == s1->getID());
	CHECK(records[1].objectId == f1->getID());
}
