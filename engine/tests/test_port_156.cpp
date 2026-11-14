// OpenBFME unit tests. GPL-3.0.
// PORT-156 (issue #374): PlayerTemplate SpellStoreCurrentPowerLabel / SpellStoreMaximumPowerLabel,
// LoadScreenImage, ScoreScreenImage, SideIconImage — parsed and read by nobody in RotWK 2.01.
//
// BINARY VERIFICATION against RotWK 2.01 game.dat (md5 daed3668006cd90f01c34e5a7da1901f, tools/mod/rw.py):
//   * the field rows (table RW 0xBF81A8, parser RW 0x42EE5E parseAsciiString, the port's Common/PlayerTemplate.cpp:160/161/166/178/179):
//     ScoreScreenImage name RW 0xBF8408 -> +0x164, LoadScreenImage RW 0xBF8418 -> +0x168, SideIconImage RW 0xBF8468 -> +0x178,
//     SpellStoreCurrentPowerLabel RW 0xBF8528 -> +0x1C0, SpellStoreMaximumPowerLabel RW 0xBF8538 -> +0x1C4.
//   * the class's own accessors, emitted out of line next to the rest of PlayerTemplate's code:
//       RW 0x5F3FA7 `lea eax,[ecx+0x1B4]; ret` SpellBook, RW 0x5F3FAE +0x1B8 SpellBookMp, RW 0x5F3FB5 `lea eax,[ecx+0x1C0]; ret`
//       SpellStoreCurrentPowerLabel, RW 0x5F3FBC +0x1BC Evil. Not one of the four has a caller (rw.py xref: no rel32 call / jmp
//       and no address immediate anywhere in .text).
//     The image accessors, in the block RW 0x5FC90F .. 0x5FC978, are `add ecx,off; push ecx; mov ecx,[0xDE4AC0 TheMappedImageCollection];
//     call 0x6DA34C ImageCollection::findImageByName`: RW 0x5FC90F +0x16C HeadWaterMark, RW 0x5FC922 +0x170 FlagWaterMark,
//     RW 0x5FC935 +0x178 SideIconImage, RW 0x5FC948 +0x174 EnabledImage, RW 0x5FC966 +0x1D8 MultiSelectionPortrait. There is no
//     accessor for +0x164 and none for +0x168.
//     Callers (rw.py xref): +0x170 RW 0x5FC922 <- RW 0x91490A; +0x174 RW 0x5FC948 <- RW 0x9143D4 and RW 0x9145C0 (both
//     `mov ecx,[obj+0x34]` — Player + 0x34 is the PlayerTemplate, the chain RW 0x6AB16A … RW 0x6AB178 uses for +0x1A4); +0x178
//     RW 0x5FC935 <- none, and no second reader: the 200 call sites of findImageByName contain no `+0x164` / `+0x168` / `+0x178` /
//     `+0x1C0` / `+0x1C4` operand besides those five accessors.
//   * the spell book never reads the labels: the store's bind/fill RW 0x822A98, its update RW 0x822E43 and OnBttnSpell RW 0x8235A5
//     (0x822A98 .. 0x82379A) touch neither +0x1C0 nor +0x1C4 (they read the template only through RW 0x71F933, PurchaseScienceCommandSet);
//     their texts are the fixed labels RW 0xC50C4C "APT:SpellStoreSpellPoints", RW 0xC50C80 "APT:SpellHelpText",
//     RW 0xC50C68 "APT:SpellDescription", RW 0xC4F010 "TOOLTIP:ScienceDisabled", RW 0xC50C10 "APT:Spell%dCost". The in-game book's
//     code (0x92F000 .. 0x932000) touches neither field either.
//   * the loading screen reads only the music: AptLoadScreen::init RW 0x81CB08 takes the slot's template (RW 0x81D1D2 · 0x5FCAD9 /
//     0x5FCA2E) and `add eax,0x14C` RW 0x81D203 — LoadScreenMusic. No `+0x168` operand anywhere in RW 0x81C5D3 .. 0x81D4xx.
//   * every `add reg,0x1C0` in .text (7: RW 0x49D51F, 0x64928B, 0x68FAF4, 0x82609F, 0x84A771, 0x8715A6, 0x9034A8) belongs to another
//     class (a window's sound, a Coord3D copy, a float field, another template's strings) — not one is a PlayerTemplate.
//   * peers: +0x164 was already judged unused by docs/ennorath/arena-triage (batch-10 #19: the string is only initialised, copied
//     and destroyed).
//   * DONOR BFME2 1.06 (Open-BFME-2): the five names occur only in the class header (Code/GameEngine/Include/Common/PlayerTemplate.h)
//     and in the field table (Code/GameEngine/Source/GameLogic/Object/Rva001FE70FInit.cpp:73/74/79/91/92) — no reader.
//
// So RotWK parses the five fields and shows none of them: the port must not invent a use (a label in the store, an image on the
// loading / score screen). These tests pin the verdict on the port's real paths: the values reach PlayerTemplate, the loading
// screen's cards carry the faction's LoadScreenMusic and nothing of its images, and the mod's own image values never stand in for a
// missing track.

#include "AptPlayerTestUtil.h"

#include "GameClient/GUI/AptScreens/AptSimpleScreens.h"
#include "LogicTestUtil.h"

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
// The mod's own values (field audit v6, playertemplate.ini:66/67/684/708/711): a faction that names both spell-book labels and all
// three screen images, and one that names only the images.
const char *const kPlayerTemplates =
	"PlayerTemplate FactionPortMen\n"
	"  Side = Men\n"
	"  PlayableSide = Yes\n"
	"  DisplayName = INI:FactionPortMen\n"
	"  ScoreScreenImage = MutiPlayer_ScoreScreen\n"
	"  LoadScreenImage = Mp_Load\n"
	"  SideIconImage = GameinfoOBSRVR\n"
	"  LoadScreenMusic = Shell2MusicForLoadScreenGondor\n"
	"  PurchaseScienceCommandSet = GondorSpellStore\n"
	"  PurchaseScienceCommandSetMP = GondorSpellStoreMP\n"
	"  SpellBook = SpellBookGondor\n"
	"  SpellBookMp = SpellBookGondorMP\n"
	"  SpellStoreCurrentPowerLabel = APT:SpellStoreCurrentEvenstarPower\n"
	"  SpellStoreMaximumPowerLabel = APT:SpellStoreMaximumEvenstarPower\n"
	"End\n"
	"PlayerTemplate FactionPortRing\n"
	"  Side = Mordor\n"
	"  PlayableSide = Yes\n"
	"  DisplayName = INI:FactionPortRing\n"
	"  ScoreScreenImage = MutiPlayer_ScoreScreen\n"
	"  LoadScreenImage = Mp_Load\n"
	"  SideIconImage = GameinfoOBSRVR\n"
	"  SpellStoreCurrentPowerLabel = APT:SpellStoreCurrentRingPower\n"
	"  SpellStoreMaximumPowerLabel = APT:SpellStoreMaximumRingPower\n"
	"End\n";

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
	s.colors_.push_back(SkirmishColor{ "ColorBlue", "Color:Blue", 0x0000FF, true });
	return s;
}

// One human on card 0 with `localTemplate` and a medium AI on card 1 with `aiTemplate`.
SkirmishGameInfo makeGame(const Setup &setup, const std::string &localTemplate, const std::string &aiTemplate)
{
	SkirmishGameInfo game;
	game.mapStartCount = LEGACY_MAX_SLOTS;
	game.slots[0].state = SLOT_PLAYER;
	game.slots[0].name = u"Local";
	game.slots[0].playerTemplate = setup.indexOf(localTemplate);
	game.slots[0].color = 0;
	game.slots[1].state = SLOT_MED_AI;
	game.slots[1].name = u"Computer";
	game.slots[1].playerTemplate = setup.indexOf(aiTemplate);
	game.slots[1].color = 0;
	return game;
}

// The retail one-frame LoadScreen.apt: ShellServices records what the screen asks for (no game files are read).
struct Fixture
{
	apttest::MemorySource source;
	RecordingShellServices services;
	ShellEnvironment env;
	AptScreenFactoryTable factories;
	LoadScreenInfo info;
	std::unique_ptr<WindowManager> wm;
	std::unique_ptr<Shell> shell;

	Fixture()
	{
		apttest::TestMovie movie;
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

	void open(const LoadScreenInfo &next)
	{
		info = next;
		env.loadScreen = &info;
		shell->push("LoadScreen.apt");
		wm->update(33);
	}
};
} // namespace

TEST_CASE("PORT-156: PlayerTemplate keeps the spell-book labels and the screen images, and no reader takes them (RW 0x5F3FB5, 0x5FC935)")
{
	// The five rows of RW 0xBF81A8 land in the port's members: the parse is retail's, and these tests fail if a row is dropped
	// (field audit: parsed-unused; the verdict is that nothing else may be built on them).
	logictest::LogicWorld lw;
	REQUIRE(lw.loadError.empty());
	REQUIRE_MESSAGE(lw.w.load(kPlayerTemplates, INI_LOAD_OVERWRITE, "port156.ini").empty(), "the mod's five rows must parse");
	const PlayerTemplate *men = lw.templates.findPlayerTemplate("FactionPortMen");
	REQUIRE(men);
	CHECK(men->m_scoreScreenImage == "MutiPlayer_ScoreScreen");   // + 0x164
	CHECK(men->m_loadScreenImage == "Mp_Load");                   // + 0x168
	CHECK(men->m_sideIconImage == "GameinfoOBSRVR");              // + 0x178
	CHECK(men->m_spellStoreCurrentPowerLabel == "APT:SpellStoreCurrentEvenstarPower"); // + 0x1C0
	CHECK(men->m_spellStoreMaximumPowerLabel == "APT:SpellStoreMaximumEvenstarPower"); // + 0x1C4
	// the neighbours that RotWK does read: LoadScreenMusic + 0x14C (RW 0x81D203) and the store's command set (RW 0x822AB3)
	CHECK(men->m_loadScreenMusic == "Shell2MusicForLoadScreenGondor");
	CHECK(men->m_purchaseScienceCommandSetMP == "GondorSpellStoreMP");
	CHECK(men->m_spellBookMp == "SpellBookGondorMP");
	// a block without any of them: the members stay empty (retail's constructor RW 0x5FE3F1 leaves the strings empty)
	const PlayerTemplate *ring = lw.templates.findPlayerTemplate("FactionPortRing");
	REQUIRE(ring);
	CHECK(ring->m_loadScreenMusic.empty());
	CHECK(ring->m_spellStoreCurrentPowerLabel == "APT:SpellStoreCurrentRingPower");
}

TEST_CASE("PORT-156: the loading screen's cards carry the faction's LoadScreenMusic and nothing of its images (RW 0x81D203, no + 0x168)")
{
	// RW 0x81CB08 reads the slot's template and only `add eax,0x14C`: the faction's LoadScreenImage (+ 0x168), ScoreScreenImage
	// (+ 0x164) and SideIconImage (+ 0x178) never reach the screen, so a faction that names all three and no track is silent.
	const Setup setup = makeSetup();
	const SkirmishGameInfo game = makeGame(setup, "FactionPortMen", "FactionPortRing");
	const LoadScreenInfo info = makeLoadScreenInfo(game, setup, nullptr, nullptr);
	CHECK(info.error.empty());
	REQUIRE(info.cards[0].occupied);
	REQUIRE(info.cards[1].occupied);
	CHECK(info.cards[0].loadMusic == "Shell2MusicForLoadScreenGondor");
	CHECK(info.cards[1].loadMusic.empty()); // images present, no music: empty, not "Mp_Load" and not the other faction's track
	CHECK(info.cards[0].levelImage.empty()); // no image lookup was passed: no rank icon, and LoadScreenImage is not one
	CHECK(info.cards[1].levelImage.empty());
}

TEST_CASE("PORT-156: the loading screen asks for the local faction's track once and for nothing when it has none (RW 0x81D1B8 .. 0x81D283)")
{
	// The screen's own path (AptLoadScreen over LoadScreen.apt): the local card's LoadScreenMusic is the only track asked for; a
	// faction whose block has the three images and no track asks for no music at all.
	{
		const Setup setup = makeSetup();
		Fixture fx;
		fx.open(makeLoadScreenInfo(makeGame(setup, "FactionPortMen", "FactionPortRing"), setup, nullptr, nullptr));
		REQUIRE(fx.services.loadScreenMusic.size() == 1);
		CHECK(fx.services.loadScreenMusic[0] == "Shell2MusicForLoadScreenGondor");
	}
	{
		const Setup setup = makeSetup();
		Fixture fx;
		fx.open(makeLoadScreenInfo(makeGame(setup, "FactionPortRing", "FactionPortMen"), setup, nullptr, nullptr));
		CHECK(fx.services.loadScreenMusic.empty());
	}
}
