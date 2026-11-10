// OpenBFME unit tests. GPL-3.0.
//
// PORT-98 (issue #258): PlayerTemplate LoadScreenMusic — each faction's music on the loading screen.
//
// TARGET FACTS (RotWK game.dat, caveat S-001; BFME2 1.06 decomp AptLoadScreenInit.cpp:380 is the same code):
//   * the field: PlayerTemplate table RW 0xBF8428, name "LoadScreenMusic" RW 0xBF7EA8, offset + 0x14C, parsed by RW 0x42EE5E (the port's
//     PlayerTemplate.cpp:162 / LoadScreenInfo / SkirmishSetup carry it).
//   * its reader is the players' loading screen's init, RW 0x81CB08 (AptLoadScreen; the port's AptLoadScreen::populate, LoadScreenInfo.cpp makes the
//     cards). After the cards the init takes the game argument (this + 0x88 = RW 0x81CB2D) and:
//       - the local slot: game->getLocalSlotNum (vtable + 0x34) then game->getSlot, RW 0x800B55 (RW 0x81D1B8 .. 0x81D1C5); no slot: the whole music
//         block is skipped (RW 0x81D1CA -> 0x81D2B1);
//       - the slot's template index (GameSlot + 0x18): ThePlayerTemplateStore (RW 0xDE3B10) getNthPlayerTemplate RW 0x5FCAD9; an index below 0 takes
//         the template named "FactionObserver" (RW 0xBFD484 nameToKey RW 0x5487EC, findPlayerTemplate RW 0x5FCA2E) — RW 0x81D1D2 .. 0x81D1FE;
//       - the field: `add eax, 0x14C` RW 0x81D203, copied into an AsciiString (RW 0x81D20C);
//       - TheAudio (RW 0xDE42FC) findAudioEventInfo, vslot 0x12C, RW 0x81D225; the returned info ref is null: nothing is stopped or played
//         (RW 0x81D22B -> 0x81D295);
//       - TheAudio vslot 0x8C(2, 1, 1), RW 0x81D242: the music that plays stops with a fade (the same slot and shape TheShell's music stop uses,
//         RW 0x75D9B1 = 0x8C(2, 1, fade), S-3303);
//       - AudioEventRTS(info, 2) RW 0x6DA95E (view type 2 = VIEW_ANY, stored at + 0x30), shouldFade(false) RW 0x6DA774, added by TheAudio vslot 0x64
//         RW 0x81D278;
//       - TheAudio vslot 0x28, RW 0x81D283: SubsystemInterface's update (the AudioManager table's slots 0 .. 13 keep the SubsystemInterface order and
//         index 10 is update: BFME2 decomp GameAudio.h / SubsystemInterface.h), one manager step so the track starts behind the screen.
//   * every player's card carries the music of its own faction (the screen plays the local card's only): the same card data the screen draws.
//   * source values (Ennorath 1.9.8.2 playertemplate.ini): FactionMen Shell2MusicForLoadScreenGondor, FactionDwarves Shell2MusicForLoadScreenZwerge.
//     The test's own INI is synthetic and named after the mod's values so a failure reads as the mod would.
//
// Expectations below come from the addresses above and the mod's data, never from the port's code: the card of a faction whose block has
// LoadScreenMusic = X carries X, a slot without a template carries the "FactionObserver" block's track, and the screen asks the host for exactly the
// local card's track, once, when it opens.

#include "AptPlayerTestUtil.h"

#include "GameClient/GUI/AptScreens/AptSimpleScreens.h"
#include "GameClient/GUI/LoadScreenInfo.h"
#include "GameClient/GUI/Shell/Shell.h"
#include "GameClient/GUI/ShellEnvironment.h"
#include "GameClient/GUI/ShellServices.h"
#include "GameClient/GUI/Skirmish/IniSkirmishSetupSource.h"
#include "GameClient/GUI/Skirmish/SkirmishSetup.h"
#include "GameClient/GUI/WindowManager.h"
#include "GameNetwork/GameInfo.h"

#include "doctest.h"

#include <memory>
#include <string>
#include <vector>

namespace
{
using namespace apttest;

// The synthtetic playertemplate.ini: three blocks with the mod's own track names, one block without the field (a faction that leaves the screen silent).
const char *const kPlayerTemplates =
	"PlayerTemplate FactionMen\n"
	"  Side = Men\n"
	"  PlayableSide = Yes\n"
	"  DisplayName = INI:FactionMen\n"
	"  LoadScreenMusic = Shell2MusicForLoadScreenGondor\n"
	"End\n"
	"PlayerTemplate FactionDwarves\n"
	"  Side = Dwarves\n"
	"  PlayableSide = Yes\n"
	"  DisplayName = INI:FactionDwarves\n"
	"  LoadScreenMusic = Shell2MusicForLoadScreenZwerge\n"
	"End\n"
	"PlayerTemplate FactionObserver\n"
	"  Side = Observer\n"
	"  IsObserver = Yes\n"
	"  DisplayName = INI:FactionObserver\n"
	"  LoadScreenMusic = Shell2MusicForLoadScreenGondor\n"
	"End\n"
	"PlayerTemplate FactionElves\n"
	"  Side = Elves\n"
	"  PlayableSide = Yes\n"
	"  DisplayName = INI:FactionElves\n"
	"End\n";

// The lobby's faction list over that text (IniSkirmishSetupSource::scanPlayerTemplates is the mounted-archive scan without the archives).
struct Setup : public SkirmishSetupSource
{
	std::vector<SkirmishFaction> factions_;
	std::vector<SkirmishColor> colors_;
	std::vector<MapCacheEntry> maps_;
	std::vector<int> cash_{ 0 };

	const std::vector<SkirmishFaction> &factions() const override { return factions_; }
	const std::vector<SkirmishColor> &colors() const override { return colors_; }
	const std::vector<MapCacheEntry> &maps() const override { return maps_; }
	int defaultStartingCash() const override { return 0; }
	const std::vector<int> &startingCashChoices() const override { return cash_; }
	int mapContentsMask(const MapCacheEntry &) const override { return 0; }
	bool fileExists(const std::string &) const override { return false; }
	bool hasFileSystem() const override { return false; }

	int indexOf(const std::string &templateName) const
	{
		for (int i = 0; i < (int)factions_.size(); ++i)
		{
			if (factions_[(size_t)i].templateName == templateName)
			{
				return i;
			}
		}
		return -1;
	}
};

Setup makeSetup()
{
	Setup s;
	std::string error;
	REQUIRE_MESSAGE(IniSkirmishSetupSource::scanPlayerTemplates(kPlayerTemplates, s.factions_, &error), error);
	REQUIRE(s.factions_.size() == 4);
	s.colors_.push_back(SkirmishColor{ "ColorBlue", "Color:Blue", 0x0000FF, true });
	return s;
}

// A game of two players: card 0 the local human of `localTemplate`, card 1 a medium AI of `aiTemplate`.
SkirmishGameInfo makeGame(const Setup &setup, const std::string &localTemplate, SlotState localState, const std::string &aiTemplate)
{
	SkirmishGameInfo game;
	game.mapStartCount = LEGACY_MAX_SLOTS;
	game.slots[0].state = localState;
	game.slots[0].name = u"Local";
	game.slots[0].playerTemplate = setup.indexOf(localTemplate);
	game.slots[0].color = 0;
	game.slots[1].state = SLOT_MED_AI;
	game.slots[1].name = u"Computer";
	game.slots[1].playerTemplate = aiTemplate.empty() ? -1 : setup.indexOf(aiTemplate);
	game.slots[1].color = 0;
	return game;
}

// The screen over a synthetic one-frame LoadScreen.apt: no game files are read. ShellServices records every request the screens make.
struct Fixture
{
	MemorySource source;
	RecordingShellServices services;
	ShellEnvironment env;
	AptScreenFactoryTable factories;
	LoadScreenInfo info;
	std::unique_ptr<WindowManager> wm;
	std::unique_ptr<Shell> shell;

	Fixture()
	{
		TestMovie movie;
		movie.setRootFrames({ {} });
		source.add("LoadScreen", movie);
		factories.registerFactory("LoadScreen.apt", [](AptScreenContext &c) {
			return std::make_unique<AptLoadScreen>(c.windows, c.shell, c.environment);
		});
		env.loadScreen = &info;
		env.services = &services;
		wm = std::make_unique<WindowManager>(source, services);
		shell = std::make_unique<Shell>(*wm, factories, services, env);
		wm->init();
	}

	// opens the screen for `next` (RotWK AptLoadScreen::init runs as the screen is built, RW 0x81CB08)
	void open(const LoadScreenInfo &next)
	{
		info = next;
		env.loadScreen = &info;
		shell->push("LoadScreen.apt");
		wm->update(33);
	}
};
} // namespace

TEST_CASE("PORT-98 load screen music (RW 0x81CB08, 0x81D1B8 .. 0x81D283): the local player's faction track plays when the players' screen opens")
{
	const Setup setup = makeSetup();
	SkirmishGameInfo game = makeGame(setup, "FactionMen", SLOT_PLAYER, "FactionDwarves");
	const LoadScreenInfo info = makeLoadScreenInfo(game, setup, nullptr, nullptr);
	REQUIRE(info.cards[0].occupied);
	REQUIRE(info.cards[1].occupied);
	// each card carries its own faction's LoadScreenMusic (the field + 0x14C of the slot's template)
	CHECK(info.cards[0].loadMusic == "Shell2MusicForLoadScreenGondor");
	CHECK(info.cards[1].loadMusic == "Shell2MusicForLoadScreenZwerge");
	CHECK(info.localCard == 0);

	Fixture fx;
	fx.open(info);
	// the local slot's template track is asked for, once: the other player's faction is not played
	REQUIRE(fx.services.loadScreenMusic.size() == 1);
	CHECK(fx.services.loadScreenMusic[0] == "Shell2MusicForLoadScreenGondor");
}

TEST_CASE("PORT-98 load screen music: a faction whose block has no LoadScreenMusic asks for nothing (RW 0x81D225: no event, nothing played)")
{
	const Setup setup = makeSetup();
	SkirmishGameInfo game = makeGame(setup, "FactionElves", SLOT_PLAYER, "FactionDwarves");
	const LoadScreenInfo info = makeLoadScreenInfo(game, setup, nullptr, nullptr);
	REQUIRE(info.cards[0].occupied);
	CHECK(info.cards[0].loadMusic.empty());
	CHECK(info.cards[1].loadMusic == "Shell2MusicForLoadScreenZwerge");

	Fixture fx;
	fx.open(info);
	CHECK(fx.services.loadScreenMusic.empty());
}

TEST_CASE("PORT-98 load screen music: the local slot's own faction is played, and a slot without a template takes FactionObserver's (RW 0x81D1E7)")
{
	const Setup setup = makeSetup();
	// the local player is the Dwarf: the Dwarf track, whichever card the faction sits on
	{
		SkirmishGameInfo game = makeGame(setup, "FactionDwarves", SLOT_PLAYER, "FactionMen");
		const LoadScreenInfo info = makeLoadScreenInfo(game, setup, nullptr, nullptr);
		REQUIRE(info.localCard == 0);
		Fixture fx;
		fx.open(info);
		REQUIRE(fx.services.loadScreenMusic.size() == 1);
		CHECK(fx.services.loadScreenMusic[0] == "Shell2MusicForLoadScreenZwerge");
	}
	// a slot without a template (GameSlot + 0x18 below 0): nameToKey("FactionObserver") -> findPlayerTemplate (RW 0x81D1E7 .. 0x81D1FE)
	{
		SkirmishGameInfo game = makeGame(setup, "", SLOT_PLAYER, "FactionDwarves");
		REQUIRE(game.slots[0].playerTemplate == -1);
		const LoadScreenInfo info = makeLoadScreenInfo(game, setup, nullptr, nullptr);
		CHECK(info.cards[0].loadMusic == "Shell2MusicForLoadScreenGondor");
		Fixture fx;
		fx.open(info);
		REQUIRE(fx.services.loadScreenMusic.size() == 1);
		CHECK(fx.services.loadScreenMusic[0] == "Shell2MusicForLoadScreenGondor");
	}
}

TEST_CASE("PORT-98 load screen music: the cards' other data is untouched — names, factions, teams, colours and the local card (neighbour)")
{
	const Setup setup = makeSetup();
	SkirmishGameInfo game = makeGame(setup, "FactionMen", SLOT_PLAYER, "FactionDwarves");
	game.slots[1].teamNumber = 1;
	const LoadScreenInfo info = makeLoadScreenInfo(game, setup, nullptr, nullptr);
	CHECK(info.localCard == 0);
	CHECK(info.cards[0].playerName == u"Local");
	CHECK(info.cards[1].playerName == u"Computer");
	CHECK(info.cards[0].human);
	CHECK_FALSE(info.cards[1].human);
	CHECK(info.cards[1].teamNumber == 1);
	CHECK(info.cards[0].color == 0x0000FF);
	CHECK_FALSE(info.cards[2].occupied);
	CHECK(info.cards[2].loadMusic.empty());
	// no services: the screen reports it instead of playing (no silent default)
	Fixture fx;
	fx.env.services = nullptr;
	fx.open(info);
	CHECK(fx.services.loadScreenMusic.empty());
}
