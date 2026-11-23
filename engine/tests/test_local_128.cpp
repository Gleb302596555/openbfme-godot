// OpenBFME tests: LOCAL-128 (fork issue 812) — Ennorath 1.9.8.5 lobby map search on the profile (--mod-engine=fc32e4c4142b5c607b90972eaf448400).
// Expectations come from PYSAGE 1.9.8.5 .mapfind 0x00F33000..0x00F33534 and .mapsvar 0x00F32000..0x00F3220C, hooked at
// RW 0x49FC37 (W3DGadgetTextEntryDraw RW 0x49F7C6), RW 0x722745 (GadgetTextEntryInput), RW 0x840879 (AptMpGameSetup init RW 0x840807),
// RW 0x840906 (MpGameSetup::InitGadgets), RW 0x8465EA (AptMpGameSetup::PopulateMapList RW 0x8460B5), and RW 0x8467A8 (selectMap call in RW 0x846770).
// Off the profile, retail 2.01 behaviour holds: MapSearch is ignored and MapList is unfiltered. GPL-3.0.

#include "doctest.h"
#include "AptPlayerTestUtil.h"

#include "Common/ModEngineFeatures.h"
#include "GameClient/GUI/AptGadgetLayer.h"
#include "GameClient/GUI/AptScreens/AptLanLobby.h"
#include "GameClient/GUI/AptScreens/AptScreenFactories.h"
#include "GameClient/GUI/AptScreens/AptSkirmish.h"
#include "GameClient/GUI/GadgetDrawList.h"
#include "GameClient/GUI/Gadgets.h"
#include "GameClient/GUI/Shell/Shell.h"
#include "GameClient/GUI/ShellEnvironment.h"
#include "GameClient/GUI/ShellServices.h"
#include "GameClient/GUI/Skirmish/SkirmishGameSetup.h"
#include "GameClient/GUI/WindowManager.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace apttest;

namespace
{

struct ModEngineGuard
{
	~ModEngineGuard() { ModEngineFeatures::reset(); }
};

class Local128GameText : public GameTextSource
{
public:
	std::map<std::string, std::u16string> labels;
	bool fetch(const std::string &label, std::u16string &out) const override
	{
		auto it = labels.find(label);
		if (it == labels.end()) return false;
		out = it->second;
		return true;
	}
	void set(const std::string &label, const std::u16string &text) { labels[label] = text; }
	void setAscii(const std::string &label, const std::string &text) { labels[label] = asciiToU16(text); }
};

class Local128SetupSource : public SkirmishSetupSource
{
public:
	std::vector<SkirmishFaction> fac;
	std::vector<SkirmishColor> col;
	std::vector<MapCacheEntry> map;
	std::vector<int> cash{ 1000 };

	const std::vector<SkirmishFaction> &factions() const override { return fac; }
	const std::vector<SkirmishColor> &colors() const override { return col; }
	const std::vector<MapCacheEntry> &maps() const override { return map; }
	int defaultStartingCash() const override { return 1000; }
	const std::vector<int> &startingCashChoices() const override { return cash; }
	int mapContentsMask(const MapCacheEntry &) const override { return 1; }
	bool fileExists(const std::string &) const override { return false; }
	bool hasFileSystem() const override { return false; }

	static MapCacheEntry makeMap(const char *key, const std::u16string &display, int players, std::uint32_t crc)
	{
		MapCacheEntry m;
		m.name = key;
		m.rawName = key;
		m.numPlayers = players;
		m.isMultiplayer = true;
		m.isOfficial = true;
		m.fileCRC = crc;
		m.fileSize = crc + 1;
		m.displayName = display;
		return m;
	}

	Local128SetupSource()
	{
		SkirmishFaction men;
		men.templateName = "FactionMen";
		men.side = "Men";
		men.displayLabel = "INI:FactionMen";
		men.playableSide = true;
		men.startingBuilding = "MenFortress";
		fac.push_back(men);

		SkirmishColor blue;
		blue.name = "ColorBlue";
		blue.tooltipName = "Color:Blue";
		blue.rgb = 0x465B9Cu;
		col.push_back(blue);

		// Synthetic map list covering ASCII, Cyrillic with Yo/yo (U+0451), Latin-1 accented (U+00C9), and U+00D7 (must not fold to U+00F7).
		map.push_back(makeMap("maps/isengard/isengard.map", u"Isengard", 2, 0x10));
		map.push_back(makeMap("maps/mordor/mordor.map", u"Mordor Wastes", 4, 0x20));
		map.push_back(makeMap("maps/elven/elven.map", u"Тёмный Лес", 2, 0x30));
		map.push_back(makeMap("maps/vale/vale.map", u"Éowyn's Vale", 2, 0x40));
		map.push_back(makeMap("maps/cross/cross.map", u"A\u00D7B", 2, 0x50));
	}
};

struct MapSearchHarness
{
	Local128SetupSource setupSrc;
	RecordingNewGameSink games;
	Local128GameText text;
	MemorySource source;
	RecordingShellServices services;
	AptScreenFactoryTable factories;
	GadgetSkinData skins;
	WindowManager windows;
	AptGadgetLayer layer;
	ShellEnvironment environment;
	Shell shell;
	AptSkirmish *screen = nullptr;
	GameWindow *createdSearchWindow = nullptr;

	explicit MapSearchHarness(bool lanLobby = false)
		: windows(source, services), layer(windows, source, skins), shell(windows, factories, services, environment)
	{
		for (const auto &p : std::map<std::string, std::string>{
			{ "GUI:Open", "Open" }, { "GUI:Closed", "Closed" }, { "GUI:EasyAI", "Easy AI" },
			{ "GUI:MediumAI", "Medium AI" }, { "GUI:HardAI", "Hard AI" }, { "GUI:BrutalAI", "Brutal AI" },
			{ "GUI:Random", "Random" }, { "GUI:???", "???" }, { "GUI:Player", "Player" },
			{ "GUI:StartingMoneyFormat", "$%d" }, { "INI:FactionMen", "Men" },
			{ "Color:Blue", "Blue" }, { "Team:0", "Team 0" }, { "APT:MapSearch", "Search maps..." } })
		{
			text.setAscii(p.first, p.second);
		}
		environment.gameText = &text;
		environment.skirmish = &setupSrc;
		environment.newGame = &games;

		TestMovie level0;
		level0.setRootFrames({ {} });
		source.add("AptLevel0", level0);

		TestMovie movie;
		const auto shape = movie.addCharacter(movie.addShape(0, 0, 200, 24, 0));
		const auto listSprite = movie.addCharacter(movie.addSprite({ { movie.addPlaceItem(placeChar(shape, 1, "bounds")) } }));
		const auto entrySprite = movie.addCharacter(movie.addSprite({ { movie.addPlaceItem(placeChar(shape, 1, "bounds")) } }));
		movie.addExport("TestMapList", listSprite);
		movie.addExport("TestMapSearchEntry", entrySprite);

		std::vector<std::uint32_t> frame;
		// Place MapList and MapSearch as Ennorath 1.9.8.5's MpGameSetup.apt does (_Init = "MpGameSetup::InitGadgets").
		auto placeMapList = placeChar(listSprite, 1, "MapList");
		placeMapList.flags |= APT_PLACE_HASCLIPACTION;
		placeMapList.events.push_back({ APT_CLIP_CONSTRUCT, 0, program(movie, [](Asm &code) {
			code.pushString("_type").setStringVar("TestMapList");
			code.pushString("_Init").setStringVar("MpGameSetup::InitGadgets");
			code.pushString("_WindowId").setStringVar("MapList");
		}) });
		frame.push_back(movie.addPlaceItem(placeMapList));

		auto placeMapSearch = placeChar(entrySprite, 2, "MapSearch");
		placeMapSearch.flags |= APT_PLACE_HASCLIPACTION;
		placeMapSearch.events.push_back({ APT_CLIP_CONSTRUCT, 0, program(movie, [](Asm &code) {
			code.pushString("_type").setStringVar("TestMapSearchEntry");
			code.pushString("_Init").setStringVar("MpGameSetup::InitGadgets");
			code.pushString("_WindowId").setStringVar("MapSearch");
		}) });
		frame.push_back(movie.addPlaceItem(placeMapSearch));

		frame.push_back(movie.addActionItem(program(movie, [](Asm &code) {
			fscmd(code, "AptSkirmish::OnInitialized");
			code.op(APT_OP_STOP);
		})));
		movie.setRootFrames({ frame });
		const std::string movieName = lanLobby ? "LanLobby" : "Skirmish";
		source.add(movieName, movie);

		if (lanLobby)
		{
			factories.registerFactory("LanLobby.apt", [](AptScreenContext &ctx) {
				return std::make_unique<AptLanLobby>(ctx.windows, ctx.shell, ctx.environment);
			});
		}
		else
		{
			factories.registerFactory("Skirmish.apt", [](AptScreenContext &ctx) {
				return std::make_unique<AptSkirmish>(ctx.windows, ctx.shell, ctx.environment);
			});
		}

		windows.registerComponent("TestMapList", [this](AptComponentRequest &req) -> std::shared_ptr<GameWindow> {
			WinInstanceData inst;
			inst.m_style = GWS_SCROLL_LISTBOX;
			ListboxData data;
			data.columns = 5;
			data.columnWidthPercentage = { 8, 2, 70, 10, 10 };
			data.listLength = 32;
			GameWindow *w = layer.gadgets().gogoGadgetListBox(nullptr, WIN_STATUS_ENABLED, 0, 0, 300, 150, &inst, &data, nullptr, false);
			if (!w) return {};
			w->setAptInstanceName(req.instanceName);
			if (screen) w->winSetOwner(screen->ownerWindow());
			return { w, [this](GameWindow *g) { layer.gadgets().winDestroy(g); layer.gadgets().processDestroyList(); } };
		});

		windows.registerComponent("TestMapSearchEntry", [this](AptComponentRequest &req) -> std::shared_ptr<GameWindow> {
			WinInstanceData inst;
			inst.m_style = GWS_ENTRY_FIELD;
			EntryData data;
			data.maxTextLen = 64;
			GameWindow *w = layer.gadgets().gogoGadgetTextEntry(nullptr, WIN_STATUS_ENABLED, 0, 0, 200, 24, &inst, &data, nullptr, false);
			if (!w) return {};
			w->setAptInstanceName(req.instanceName);
			if (screen) w->winSetOwner(screen->ownerWindow());
			if (true)
			{
				createdSearchWindow = w;
			}
			return { w, [this](GameWindow *g) { layer.gadgets().winDestroy(g); layer.gadgets().processDestroyList(); } };
		});

		windows.init();
		shell.push(movieName + ".apt");
		screen = dynamic_cast<AptSkirmish *>(shell.top());
		REQUIRE(screen);
		for (int t = 0; t < 3; ++t) windows.update(33);
		REQUIRE(screen->mapListWindow());
		REQUIRE(createdSearchWindow);
	}

	~MapSearchHarness() { shell.clear(); }

	void typeText(GameWindow *entry, const std::u16string &s)
	{
		for (char16_t ch : s)
		{
			layer.gadgets().winSendInputMsg(entry, GWM_IME_CHAR, (WindowMsgData)ch, 0);
		}
	}

	void pressBackspace(GameWindow *entry, int count)
	{
		for (int i = 0; i < count; ++i)
		{
			layer.gadgets().winSendInputMsg(entry, GWM_CHAR, (WindowMsgData)KEY_BACKSPACE, (WindowMsgData)KEY_STATE_DOWN);
		}
	}
};

} // namespace

TEST_CASE("LOCAL-128: ennorath.lobby-map-search filters MapList live, folds ASCII/Latin-1/Cyrillic/Yo, trims spaces, and preserves selection when hidden")
{
	// Expectations from PYSAGE 1.9.8.5 .mapfind 0x00F33000..0x00F33534 (.mapsvar 0x00F32000..0x00F3220C),
	// hooked at RW 0x49FC37, RW 0x722745, RW 0x840879, RW 0x840906, RW 0x8465EA, RW 0x8467A8.
	ModEngineGuard guard;

	SUBCASE("profile off (retail 2.01): MapSearch is ignored and MapList stays unfiltered")
	{
		ModEngineFeatures::reset();
		MapSearchHarness fx;
		CHECK(fx.screen->mapSearchWindow() == nullptr);
		CHECK(fx.screen->mapListKeys().size() == 5);
		fx.typeText(fx.createdSearchWindow, u"mordor");
		CHECK(fx.screen->mapListKeys().size() == 5);
	}

	SUBCASE("profile on (Ennorath 1.9.8.5): live substring filter, folding, trimming, colours, and preserved map selection")
	{
		std::string err;
		REQUIRE_MESSAGE(ModEngineFeatures::activate({ "fc32e4c4142b5c607b90972eaf448400" }, &err), err);
		REQUIRE(ModEngineFeatures::has(ModEngineFeature::EnnorathLobbyMapSearch));

		MapSearchHarness fx;
		GameWindow *search = fx.screen->mapSearchWindow();
		REQUIRE(search != nullptr);
		CHECK(search == fx.createdSearchWindow);

		// Colours from PYSAGE 1.9.8.5 .mapfind 0x00F3318C..0x00F331D7 (RW 0x7156B3, 0x7156DB, 0x715703, 0x71572B).
		CHECK(search->winGetEnabledTextColor() == 0xFFEEEAE1u);
		CHECK(search->winGetEnabledTextBorderColor() == 0xFF101419u);
		CHECK(search->winGetDisabledTextColor() == 0xFFAEB8BFu);
		CHECK(search->winGetDisabledTextBorderColor() == 0xFF101419u);
		CHECK(search->winGetHiliteTextColor() == 0xFFF5F1E9u);
		CHECK(search->winGetHiliteTextBorderColor() == 0xFF101419u);
		CHECK(search->winGetIMECompositeTextColor() == 0xFFB6D8E8u);
		CHECK(search->winGetIMECompositeBorderColor() == 0xFF101419u);

		// All 5 maps initially; pick Isengard explicitly to test PYSAGE 1.9.8.5 .mapfind 0x00F334C3 (RW 0x8467A8).
		REQUIRE(fx.screen->mapListKeys().size() == 5);
		REQUIRE(fx.screen->setup());
		// Select Isengard via MapList (GLM_SELECTED)
		const auto &initialKeys = fx.screen->mapListKeys();
		auto isenIt = std::find(initialKeys.begin(), initialKeys.end(), "maps/isengard/isengard.map");
		REQUIRE(isenIt != initialKeys.end());
		const int isenRow = static_cast<int>(isenIt - initialKeys.begin());
		fx.layer.gadgets().winSendSystemMsg(fx.screen->mapListWindow()->winGetOwner(), GLM_SELECTED, reinterpret_cast<WindowMsgData>(fx.screen->mapListWindow()), static_cast<WindowMsgData>(isenRow));
		CHECK(fx.screen->setup()->info().mapName == "maps/isengard/isengard.map");

		// Live filter with leading/trailing spaces and uppercase ASCII (.mapfind 0x00F3335B..0x00F3339F, 0x00F330CA, 0x00F33416).
		fx.typeText(search, u"  MOR  ");
		CHECK(fx.screen->mapListKeys() == std::vector<std::string>{ "maps/mordor/mordor.map" });
		// Selected map is hidden by filter -> selection in game info must NOT be overwritten (.mapfind 0x00F334C3).
		CHECK(fx.screen->setup()->info().mapName == "maps/isengard/isengard.map");

		// Clearing with Backspace restores the full list and re-selects the preserved map in MapList.
		fx.pressBackspace(search, 7);
		CHECK(fx.screen->mapListKeys().size() == 5);
		int selectedRow = -1;
		GadgetListBoxGetSelected(fx.screen->mapListWindow(), &selectedRow);
		REQUIRE(selectedRow == isenRow);
		CHECK(fx.screen->mapListKeys()[static_cast<std::size_t>(selectedRow)] == "maps/isengard/isengard.map");

		// Cyrillic case folding + Yo/yo -> ye (.mapfind 0x00F330BE: U+0401/U+0451 -> U+0435, U+0410..U+042F + 0x20).
		fx.typeText(search, u"ТЕМН");
		CHECK(fx.screen->mapListKeys() == std::vector<std::string>{ "maps/elven/elven.map" });
		fx.pressBackspace(search, 4);

		// Latin-1 folding + NBSP (U+00A0) and tab trimming (.mapfind 0x00F330D6, 0x00F33368).
		fx.typeText(search, u"\t\u00A0éowyn ");
		CHECK(fx.screen->mapListKeys() == std::vector<std::string>{ "maps/vale/vale.map" });
		fx.pressBackspace(search, 8);

		// Multiplication sign U+00D7 must NOT fold to division sign U+00F7 (.mapfind 0x00F330E2).
		fx.typeText(search, u"\u00F7");
		CHECK(fx.screen->mapListKeys().empty());
		fx.pressBackspace(search, 1);
		CHECK(fx.screen->mapListKeys().size() == 5);
	}
}

TEST_CASE("LOCAL-128: MapSearch draws APT:MapSearch placeholder only when empty on the Ennorath 1.9.8.5 profile")
{
	// Expectations from PYSAGE 1.9.8.5 .mapfind 0x00F3305E -> 0x00F3320B (hooked at W3DGadgetTextEntryDraw RW 0x49FC37).
	ModEngineGuard guard;

	auto drawnTexts = [](GameWindowManager &gwm, GameWindow *w) {
		GadgetDrawList dl;
		gwm.setDrawList(&dl);
		gwm.drawWindow(w);
		gwm.setDrawList(nullptr);
		std::vector<std::pair<UnicodeString, Color>> out;
		for (const GadgetDrawCommand &cmd : dl.commands)
		{
			if (cmd.kind == GadgetDrawCommand::Kind::Text && !cmd.text.empty())
			{
				out.emplace_back(cmd.text, cmd.color);
			}
		}
		return out;
	};

	SUBCASE("profile off: no placeholder drawn")
	{
		ModEngineFeatures::reset();
		MapSearchHarness fx;
		CHECK(drawnTexts(fx.layer.gadgets(), fx.createdSearchWindow).empty());
	}

	SUBCASE("profile on: placeholder drawn when empty, hidden when non-empty, restored when cleared")
	{
		std::string err;
		REQUIRE_MESSAGE(ModEngineFeatures::activate({ "fc32e4c4142b5c607b90972eaf448400" }, &err), err);
		MapSearchHarness fx;
		GameWindow *search = fx.screen->mapSearchWindow();
		REQUIRE(search != nullptr);

		auto texts = drawnTexts(fx.layer.gadgets(), search);
		REQUIRE(texts.size() == 1);
		CHECK(texts[0].first == u"Search maps...");
		CHECK(texts[0].second == 0xFFB6CBD3u);

		fx.typeText(search, u"a");
		texts = drawnTexts(fx.layer.gadgets(), search);
		REQUIRE(texts.size() == 1);
		CHECK(texts[0].first == u"a");
		CHECK(texts[0].second == 0xFFEEEAE1u);

		fx.pressBackspace(search, 1);
		texts = drawnTexts(fx.layer.gadgets(), search);
		REQUIRE(texts.size() == 1);
		CHECK(texts[0].first == u"Search maps...");
		CHECK(texts[0].second == 0xFFB6CBD3u);
	}
}

TEST_CASE("LOCAL-128: AptLanLobby inherits MapSearch filtering on the Ennorath 1.9.8.5 profile and stays retail when off")
{
	// Expectations from PYSAGE 1.9.8.5 .mapfind 0x00F33000..0x00F334C3; AptLanLobby inherits AptSkirmish's MpGameSetup handling.
	ModEngineGuard guard;

	SUBCASE("profile off")
	{
		ModEngineFeatures::reset();
		MapSearchHarness fx(true);
		CHECK(fx.screen->mapSearchWindow() == nullptr);
		CHECK(fx.screen->mapListKeys().size() == 5);
		fx.typeText(fx.createdSearchWindow, u"isengard");
		CHECK(fx.screen->mapListKeys().size() == 5);
	}

	SUBCASE("profile on")
	{
		std::string err;
		REQUIRE_MESSAGE(ModEngineFeatures::activate({ "fc32e4c4142b5c607b90972eaf448400" }, &err), err);
		MapSearchHarness fx(true);
		REQUIRE(fx.screen->mapSearchWindow() != nullptr);
		CHECK(fx.screen->mapListKeys().size() == 5);
		fx.typeText(fx.screen->mapSearchWindow(), u"isengard");
		CHECK(fx.screen->mapListKeys() == std::vector<std::string>{ "maps/isengard/isengard.map" });
	}
}
