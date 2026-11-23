// OpenBFME unit tests for PORT-331 (issue #848): Shell, online and Living World stop fixes.
//
// TARGET FACTS (RotWK 2.01 game.dat, MD5 daed3668006cd90f01c34e5a7da1901f, caveat S-001):
//   * S-923 (AptSpellStore + ControlBar::getCommandAvailability case 0x26):
//     - RW 0x71F933: PlayerTemplate + 0x13C (PurchaseScienceCommandSetMP) when TheGameLogic->isSkirmishOrMultiplayer() (RW 0x625456),
//       else + 0x138 (PurchaseScienceCommandSet).
//     - RW 0x8231D4 .. 0x823254: updatePurchaseButton checks isScienceHidden (RW 0x8227BB -> 0x822825 -> 0x6AC287) BEFORE hasScience
//       -> state 2 (_disabled_level).
//     - RW 0x823057 .. 0x823118: appends TOOLTIP:ScienceDisabled only when the first unowned science is disabled, hidden, or fails
//       playerHasPrereqsForScience (RW 0x5FED05), not when the player merely lacks spell points.
//     - RW 0x942733 case 0x26 (RW 0x943343 .. 0x943407): a spell-book power whose first owned required science is disabled or hidden
//       returns COMMAND_RESTRICTED (usable = false).
//   * S-173 (WindowManager::hoverPath):
//     - RW 0x624F7F -> RW 0xAE1270 ([gApt + 0x6C] != NULL) -> RW 0x624C68 -> RW 0xAE1410 -> RW 0xAE13A0 reads ONLY currentButton()
//       (gApt + 0x6C), never falling back to hoverClip() (gApt + 0x68).
//   * S-1922 (AptPlayerTribute):
//     - RW 0x91565C: _InitTextEntry sets flags 0x21 (digits-only | no-space, RW 0x721FA9) and maxTextLen = 5 (RW 0x81606D).
//     - RW 0x9155C2: displayed cash adds AIStatCollector::reserve() (RW 0x6A9999 -> + 0xC -> + 0x14) when TheSkirmishAIManager has
//       an AI player for that player (RW 0x6A950B).
//   * S-1364 (LinearCampaign MillisecondsAfterStartToStartFadeUp):
//     - RW 0x5EACF9 writes mission->fadeUpFrames (default 7, RW 0x5EBA4A) to TheGameLogic + 0x90; RW 0x62B620 .. 0x62B906 schedules
//       RW 0x627062 to hold black "FadeWholeScreen" while currentFrame < fadeUpFrames and then fade up.
//   * S-5016 (AptImageMode): all six RotWK modes (RW 0x815814 / 0x44704F / 0x51A44D .. 0x51A5EA).
//
// Synthetic data only: no retail files required.

#include "doctest.h"
#include "AptPlayerTestUtil.h"
#include "CombatTestUtil.h"

#include "Common/NameKeyGenerator.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/PlayerScience.h"
#include "Common/PlayerTemplate.h"
#include "Common/Science.h"
#include "Common/SpecialPower.h"
#include "Common/Team.h"
#include "GameClient/ControlBarCommands.h"
#include "GameClient/GUI/AptGadgetLayer.h"
#include "GameClient/GUI/AptScreen.h"
#include "GameClient/GUI/AptScreens/AptImageMode.h"
#include "GameClient/GUI/AptScreens/AptSimpleScreens.h"
#include "GameClient/GUI/Gadget.h"
#include "GameClient/GUI/Gadgets.h"
#include "GameClient/GUI/GameWindow.h"
#include "GameClient/GUI/GameWindowManager.h"
#include "GameClient/GUI/Shell/Shell.h"
#include "GameClient/GUI/ShellEnvironment.h"
#include "GameClient/GUI/ShellServices.h"
#include "GameClient/GUI/TributeInfo.h"
#include "GameClient/GUI/WindowManager.h"
#include "GameClient/ScriptCameraDirector.h"
#include "GameClient/SpellBookUI.h"
#include "GameLogic/Economy.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/ScriptEngine/ScriptEngine.h"
#include "GameLogic/SkirmishAI/AIStatCollector.h"
#include "GameLogic/SkirmishAI/SkirmishAIData.h"
#include "GameLogic/SkirmishAI/SkirmishAIManager.h"
#include "Libraries/Source/Apt/AptInput.h"

#include <memory>
#include <string>
#include <vector>

namespace
{
using namespace apttest;

struct Port331SpellFixture
{
	objtest::World w;
	ScienceStore sciences;
	SpecialPowerStore specialPowers;
	CommandStore commands;
	PlayerTemplateStore templates;
	TeamFactory teams;
	PlayerList players;
	std::unique_ptr<GameLogic> logic;

	ScienceStore *prevSciences = nullptr;
	SpecialPowerStore *prevPowers = nullptr;
	CommandStore *prevCommands = nullptr;

	Port331SpellFixture()
		: sciences(w.keys)
		, templates(w.keys)
		, players(w.keys, templates, teams)
	{
		prevSciences = TheScienceStore;
		prevPowers = TheSpecialPowerStore;
		prevCommands = TheCommandStore;
		TheScienceStore = &sciences;
		TheSpecialPowerStore = &specialPowers;
		TheCommandStore = &commands;
		SpecialPowerModules::registerAll(w.modules);

		w.fx.env.blocks.registerBlock("Science", [](INI *ini) { ScienceStore::parseScienceDefinitionGlobal(ini); });
		w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
		w.fx.env.blocks.registerBlock("CommandButton", [](INI *ini) { CommandStore::parseCommandButtonDefinitionGlobal(ini); });
		w.fx.env.blocks.registerBlock("CommandSet", [](INI *ini) { CommandStore::parseCommandSetDefinitionGlobal(ini); });
		templates.registerBlock(w.fx.env.blocks);

		const char *ini =
			"Science SCIENCE_P331_TIER1\n"
			"  SciencePurchasePointCost = 5\n"
			"  SciencePurchasePointCostMP = 5\n"
			"  IsGrantable = Yes\n"
			"End\n"
			"Science SCIENCE_P331_TIER2\n"
			"  PrerequisiteSciences = SCIENCE_P331_TIER1\n"
			"  SciencePurchasePointCost = 5\n"
			"  SciencePurchasePointCostMP = 5\n"
			"  IsGrantable = Yes\n"
			"End\n"
			"SpecialPower SP_P331_BOOK\n"
			"  Enum = SPECIAL_SPELL_BOOK_HEAL\n"
			"  ReloadTime = 0\n"
			"  RequiredSciences = SCIENCE_P331_TIER1\n"
			"End\n"
			"CommandButton CB_P331_BUY_T1\n"
			"  Command = PURCHASE_SCIENCE\n"
			"  Science = SCIENCE_P331_TIER1\n"
			"End\n"
			"CommandButton CB_P331_BUY_T2\n"
			"  Command = PURCHASE_SCIENCE\n"
			"  Science = SCIENCE_P331_TIER2\n"
			"End\n"
			"CommandButton CB_P331_CAST_T1\n"
			"  Command = SPECIAL_POWER_FROM_COMMAND_CENTER\n"
			"  SpecialPower = SP_P331_BOOK\n"
			"End\n"
			"CommandSet CS_P331_STORE\n"
			"  1 = CB_P331_BUY_T1\n"
			"  2 = CB_P331_BUY_T2\n"
			"End\n"
			"CommandSet CS_P331_BOOK\n"
			"  1 = CB_P331_CAST_T1\n"
			"End\n"
			"Object P331SpellBook\n"
			"  KindOf = SPELL_BOOK IMMOBILE UNATTACKABLE\n"
			"  CommandSet = CS_P331_BOOK\n"
			"  Behavior = SpecialPowerModule ModuleTag_SP\n"
			"    SpecialPowerTemplate = SP_P331_BOOK\n"
			"  End\n"
			"End\n"
			"PlayerTemplate FactionP331\n"
			"  Side = Men\n"
			"  PlayableSide = Yes\n"
			"  SpellBook = P331SpellBook\n"
			"  SpellBookMp = P331SpellBook\n"
			"  PurchaseScienceCommandSet = CS_P331_STORE\n"
			"  PurchaseScienceCommandSetMP = CS_P331_STORE\n"
			"End\n";
		const std::string err = w.load(ini, INI_LOAD_OVERWRITE, "port331.ini");
		REQUIRE_MESSAGE(err.empty(), err);

		SkirmishSetup setup;
		SkirmishPlayer human;
		human.name = "P331Human";
		human.faction = "FactionP331";
		human.human = true;
		human.team = 0;
		human.startIndex = 1;
		setup.players.push_back(human);

		SkirmishPlayer ai;
		ai.name = "P331AI";
		ai.faction = "FactionP331";
		ai.human = false;
		ai.team = 0;
		ai.startIndex = 2;
		setup.players.push_back(ai);

		REQUIRE(players.setupSkirmish(setup).empty());
		logic = std::make_unique<GameLogic>(w.things, w.modules, players, RandomAlgorithm::ZH_CarryChain);
		SpecialPowerModules::installScienceHooks(*logic);
	}

	~Port331SpellFixture()
	{
		if (logic)
		{
			logic->reset();
		}
		TheScienceStore = prevSciences;
		TheSpecialPowerStore = prevPowers;
		TheCommandStore = prevCommands;
	}
};

struct FakeTributeSource : public TributeSource
{
	std::vector<TributePlayerRow> rows;
	bool localActive() const override { return true; }
	bool transferAllowed() const override { return true; }
	std::vector<TributePlayerRow> tributePlayers() override { return rows; }
	void send(int, int, std::uint32_t) override {}
};

std::string makeEntryWnd()
{
	auto drawData = [](const char *key) {
		std::string s = std::string("  ") + key + " = ";
		for (int i = 0; i < 9; ++i)
		{
			s += (i ? ", " : "") + std::string("IMAGE: NoImage, COLOR: 255 255 255 255, BORDERCOLOR: 0 0 0 255");
		}
		return s + ";\n";
	};
	return "FILE_VERSION = 2;\n"
		   "STARTLAYOUTBLOCK\n"
		   "  LAYOUTINIT = [None];\n"
		   "  LAYOUTUPDATE = [None];\n"
		   "  LAYOUTSHUTDOWN = [None];\n"
		   "ENDLAYOUTBLOCK\n"
		   "WINDOW\n"
		   "  WINDOWTYPE = ENTRYFIELD;\n"
		   "  SCREENRECT = UPPERLEFT: 0 0, BOTTOMRIGHT: 160 25, CREATIONRESOLUTION: 800 600;\n"
		   "  NAME = \"TributeEntry\";\n"
		   "  STATUS = ENABLED+IMAGE;\n"
		   "  STYLE = ENTRYFIELD;\n"
		   "  SYSTEMCALLBACK = \"[None]\";\n"
		   "  INPUTCALLBACK = \"[None]\";\n"
		   "  TOOLTIPCALLBACK = \"[None]\";\n"
		   "  DRAWCALLBACK = \"[None]\";\n"
		   "  FONT = NAME: \"Albertus MT\", SIZE: 14, BOLD: 0;\n"
		   "  HEADERTEMPLATE = \"AptGadgets\";\n"
		   "  TOOLTIPDELAY = -1;\n"
		   "  TEXTCOLOR = ENABLED: 255 255 255 255, ENABLEDBORDER: 0 0 0 255, DISABLED: 128 128 128 255, DISABLEDBORDER: 0 0 0 255, HILITE: 255 255 255 255, HILITEBORDER: 0 0 0 255;\n"
		   "  TEXT = \"0\";\n"
		   "  TEXTENTRYDATA = MAXLEN: 40, SECRETTEXT: 0, NUMERICALONLY: 0, ALPHANUMERICALONLY: 0, ASCIIONLY: 0;\n"
		   + drawData("ENABLEDDRAWDATA") + drawData("DISABLEDDRAWDATA") + drawData("HILITEDRAWDATA") + "END\n";
}
} // namespace

TEST_CASE("PORT-331 S-923: SpellStoreModel hidden science state, tooltip disabled warning, and InGameSpellBookModel disabled/hidden restriction")
{
	Port331SpellFixture fx;
	Player *p = fx.players.findPlayerWithName("P331Human");
	REQUIRE(p != nullptr);
	const ScienceType t1 = fx.sciences.getScienceFromInternalName("SCIENCE_P331_TIER1");
	const ScienceType t2 = fx.sciences.getScienceFromInternalName("SCIENCE_P331_TIER2");
	REQUIRE(t1 != SCIENCE_INVALID);
	REQUIRE(t2 != SCIENCE_INVALID);

	// 1. Player has 0 spell points: T1 has met prereqs (lacks only points) -> STATE_LOCKED with NO disabled warning (RW 0x823057 .. 0x823118);
	//    T2 lacks T1 prerequisite -> STATE_LOCKED WITH disabled warning.
	p->science().addSciencePurchasePoints(-p->science().getSciencePurchasePoints());
	CHECK(p->science().getSciencePurchasePoints() == 0);
	SpellStoreModel store;
	std::string err;
	REQUIRE_MESSAGE(store.open(fx.commands, *fx.logic, *p, &err), err);
	REQUIRE(store.buttons().size() == 2u);
	CHECK(store.buttons()[0].state == SpellStoreModel::STATE_LOCKED);
	CHECK(store.buttons()[0].showDisabledWarning == false);
	CHECK(store.buttons()[1].state == SpellStoreModel::STATE_LOCKED);
	CHECK(store.buttons()[1].showDisabledWarning == true);

	// 2. Disable T1 while unowned: showDisabledWarning becomes true (RW 0x823057).
	p->setScienceAvailability(t1, 1);
	store.refresh();
	CHECK(store.buttons()[0].state == SpellStoreModel::STATE_LOCKED);
	CHECK(store.buttons()[0].showDisabledWarning == true);
	p->setScienceAvailability(t1, 0);

	// 3. Give points: T1 is STATE_AVAILABLE with no warning.
	p->science().addSciencePurchasePoints(10);
	store.refresh();
	CHECK(store.buttons()[0].state == SpellStoreModel::STATE_AVAILABLE);
	CHECK(store.buttons()[0].showDisabledWarning == false);

	// 4. Hide T1 while unowned -> STATE_HIDDEN (_disabled_level, RW 0x8231D4 -> 0x8227BB -> 0x6AC287) with warning.
	p->setScienceAvailability(t1, 2);
	store.refresh();
	CHECK(store.buttons()[0].state == SpellStoreModel::STATE_HIDDEN);
	CHECK(store.buttons()[0].showDisabledWarning == true);

	// 5. Grant T1 while still hidden: hidden check runs BEFORE hasScience (RW 0x8231D4) -> STILL STATE_HIDDEN.
	p->science().grantScience(t1);
	store.refresh();
	CHECK(store.buttons()[0].state == SpellStoreModel::STATE_HIDDEN);
	CHECK(store.buttons()[0].showDisabledWarning == false);

	// 6. Unhide T1: now owned -> STATE_PURCHASED; T2 now has prereq met and points -> STATE_AVAILABLE.
	p->setScienceAvailability(t1, 0);
	store.refresh();
	CHECK(store.buttons()[0].state == SpellStoreModel::STATE_PURCHASED);
	CHECK(store.buttons()[1].state == SpellStoreModel::STATE_AVAILABLE);

	// 7. InGameSpellBookModel (RW 0x942733 case 0x26, RW 0x943343 .. 0x943407): disabled or hidden owned science sets usable = false.
	REQUIRE(SpecialPowerModules::createSpellBook(*fx.logic, *p));
	InGameSpellBookModel book;
	REQUIRE(book.refresh(fx.commands, *fx.logic, *p));
	REQUIRE(book.buttons().size() == 1u);
	CHECK(book.buttons()[0].owned == true);
	CHECK(book.buttons()[0].usable == true);

	p->setScienceAvailability(t1, 1);
	REQUIRE(book.refresh(fx.commands, *fx.logic, *p));
	CHECK(book.buttons()[0].owned == true);
	CHECK(book.buttons()[0].usable == false);

	p->setScienceAvailability(t1, 2);
	REQUIRE(book.refresh(fx.commands, *fx.logic, *p));
	CHECK(book.buttons()[0].owned == true);
	CHECK(book.buttons()[0].usable == false);

	p->setScienceAvailability(t1, 0);
	REQUIRE(book.refresh(fx.commands, *fx.logic, *p));
	CHECK(book.buttons()[0].usable == true);
}

TEST_CASE("PORT-331 S-173: WindowManager::hoverPath reads currentButton only, never falling back to hoverClip")
{
	MemorySource source;
	RecordingShellServices services;
	TestMovie level0;
	level0.setRootFrames({ {} });
	source.add("AptLevel0", level0);

	TestMovie m;
	m.addCharacter(0);
	const std::uint32_t shape = m.addShape(0, 0, 100, 100, 1);
	const std::uint32_t shapeId = m.addCharacter(shape);
	const std::uint32_t clip = m.addSprite({ { m.addPlaceItem(placeChar(shapeId, 1)) } });
	const std::uint32_t clipId = m.addCharacter(clip);
	const std::uint32_t btn = m.addButton({ { 1, shapeId }, { 2, shapeId }, { 4, shapeId }, { 8, shapeId } }, {});
	const std::uint32_t btnId = m.addCharacter(btn);

	TestMovie::Place placeClip = placeChar(clipId, 1, "NonButtonClip");
	placeClip.flags |= APT_PLACE_HASMATRIX | APT_PLACE_HASCLIPACTION;
	placeClip.translation[0] = 20;
	placeClip.translation[1] = 20;
	placeClip.events.push_back({ APT_CLIP_ROLLOVER, 0, program(m, [](Asm &a) { fscmd(a, "clipOver"); }) });

	TestMovie::Place placeBtn = placeChar(btnId, 2, "RealButton");
	placeBtn.flags |= APT_PLACE_HASMATRIX;
	placeBtn.translation[0] = 200;
	placeBtn.translation[1] = 20;

	m.setRootFrames({ { m.addPlaceItem(placeClip), m.addPlaceItem(placeBtn) } });
	source.add("HoverTest", m);

	WindowManager wm(source, services);
	wm.init();
	const int lvl = wm.loadAptWindow("", "HoverTest.apt", true, 0, 1);
	REQUIRE(lvl == 1);
	wm.update(33);

	// Pointer over non-button clip: hoverClip() is non-null, currentButton() is null -> hoverPath() MUST be empty (RW 0xAE1270 / 0xAE1410).
	wm.apt().input().postMouseMove(50.0f, 50.0f);
	wm.update(33);
	CHECK(wm.apt().input().hoverClip() != nullptr);
	CHECK(wm.apt().input().currentButton() == nullptr);
	CHECK(wm.hoverPath().empty());

	// Pointer over actual button (addButton hit quad is 10x10 at translation (200, 20)): currentButton() is non-null -> "HoverTest/RealButton".
	wm.apt().input().postMouseMove(205.0f, 25.0f);
	wm.update(33);
	CHECK(wm.apt().input().currentButton() != nullptr);
	CHECK(wm.hoverPath() == "HoverTest/RealButton");
}

TEST_CASE("PORT-331 S-1922: AptPlayerTribute InitTextEntry sets numericalOnly + maxTextLen=5 and tributeDisplayCash adds AI reserve")
{
	// Part A: tributeDisplayCash adds AIStatCollector::reserve() for AI players only (RW 0x9155C2 -> RW 0x6A950B / 0x6A9999).
	Port331SpellFixture fx;
	Player *human = fx.players.findPlayerWithName("P331Human");
	Player *ai = fx.players.findPlayerWithName("P331AI");
	REQUIRE(human != nullptr);
	REQUIRE(ai != nullptr);
	SkirmishAIStore aiStore(fx.w.keys);
	INIEnvironment aiEnv;
	aiStore.registerBlocks(aiEnv.blocks);
	INI aiIni(aiEnv);
	const std::string aiText = "SkirmishAIData TheSkirmishAIData\nEnd\n";
	aiIni.loadMemory("skirmishaidata.ini", std::vector<std::uint8_t>(aiText.begin(), aiText.end()), INI_LOAD_OVERWRITE);
	fx.logic->skirmishAI().setStore(&aiStore);
	REQUIRE(fx.logic->skirmishAI().newGame(true) == 1);
	REQUIRE(fx.logic->skirmishAI().findAI(human->getPlayerIndex()) == nullptr);
	REQUIRE(fx.logic->skirmishAI().findAI(ai->getPlayerIndex()) != nullptr);

	AIStatCollector *humanRec = fx.logic->skirmishAI().record(human->getPlayerIndex());
	AIStatCollector *aiRec = fx.logic->skirmishAI().record(ai->getPlayerIndex());
	REQUIRE(humanRec != nullptr);
	REQUIRE(aiRec != nullptr);
	humanRec->setReserve(300u);
	aiRec->setReserve(450u);

	const std::uint32_t humanMoney = human->getMoney()->countMoney();
	const std::uint32_t aiMoney = ai->getMoney()->countMoney();
	CHECK(tributeDisplayCash(*fx.logic, *human) == humanMoney);
	CHECK(tributeDisplayCash(*fx.logic, *ai) == aiMoney + 450u);

	// Part B: AptPlayerTribute _InitTextEntry sets numericalOnly = true (RW 0x721FA9 0x21) and maxTextLen = 5 (RW 0x81606D).
	MemorySource source;
	RecordingShellServices services;
	GadgetSkinData skins;
	for (const char *name : { "EntLeft", "EntRight", "EntCenter", "EntSmall" })
	{
		Image img;
		img.name = name;
		img.imageSize.x = 8;
		img.imageSize.y = 16;
		skins.images.addImage(img);
	}
	HeaderTemplate h;
	h.name = "AptGadgets";
	h.fontName = "Albertus MT";
	h.point = 14;
	skins.headers.addTemplate(h);
	const std::string wnd = makeEntryWnd();
	source.files["window/apt/textentry.wnd"] = std::vector<std::uint8_t>(wnd.begin(), wnd.end());

	TestMovie level0;
	level0.setRootFrames({ {} });
	source.add("AptLevel0", level0);

	TestMovie lib;
	lib.addCharacter(0);
	const std::uint32_t shape = lib.addShape(0, 0, 160, 25, 1);
	const std::uint32_t shapeId = lib.addCharacter(shape);
	const std::uint32_t sprite = lib.addSprite({ { lib.addPlaceItem(placeChar(shapeId, 1)) } });
	const std::uint32_t spriteId = lib.addCharacter(sprite);
	lib.addExport("TextEntry", spriteId);
	source.add("GameWindowGadgets", lib);

	TestMovie trib;
	trib.addCharacter(0);
	trib.addCharacter(0);
	trib.addImport("GameWindowGadgets", "TextEntry", 1);
	TestMovie::Place entryPlace = placeChar(1, 1, "AmountEntry");
	entryPlace.flags |= APT_PLACE_HASMATRIX;
	entryPlace.translation[0] = 20;
	entryPlace.translation[1] = 20;
	const std::uint32_t varsCode = program(trib, [&](Asm &a) {
		a.pushString("_type").setStringVar("TextEntry");
		a.pushString("_Init").setStringVar("_level1.Row1_InitTextEntry");
		a.pushString("_Load").setStringVar("Apt/TextEntry.wnd");
	});
	entryPlace.flags |= APT_PLACE_HASCLIPACTION;
	entryPlace.events.push_back({ APT_CLIP_CONSTRUCT, 0, varsCode });
	trib.setRootFrames({ { trib.addPlaceItem(entryPlace) } });
	source.add("PlayerTribute", trib);

	WindowManager wm(source, services);
	AptGadgetLayer layer(wm, source, skins);
	layer.registerComponents();
	wm.init();

	AptScreenFactoryTable factories;
	FakeTributeSource tributeSrc;
	TributePlayerRow localRow;
	localRow.playerIndex = 0;
	localRow.name = "Local";
	localRow.cash = 1000;
	localRow.local = true;
	TributePlayerRow allyRow;
	allyRow.playerIndex = 1;
	allyRow.name = "Ally";
	allyRow.cash = 500;
	tributeSrc.rows = { localRow, allyRow };

	ShellEnvironment env;
	env.services = &services;
	env.tribute = &tributeSrc;
	Shell shell(wm, factories, services, env);

	AptPlayerTribute screen(wm, shell, services, env);
	screen.runInit();
	wm.update(33);
	const std::string pfx = "_level" + std::to_string(screen.level());
	REQUIRE(wm.invokeCallback(pfx + "_OnPageLoaded", "name=" + pfx + ".Page&type=TributePage"));
	REQUIRE(wm.invokeCallback(pfx + ".Page_OnEnabledContentLoaded", "name=" + pfx + ".Content"));
	REQUIRE(wm.invokeCallback(pfx + ".Content_OnRowShown", "index=1&name=" + pfx + ".Row1"));
	wm.update(33);
	wm.update(33);
	REQUIRE(layer.errors().empty());
	GameWindow *entryWin = layer.windowOf(pfx + ".AmountEntry");
	REQUIRE(entryWin != nullptr);
	EntryData *entryData = static_cast<EntryData *>(entryWin->winGetUserData());
	REQUIRE(entryData != nullptr);
	CHECK(entryData->numericalOnly == true);
	CHECK(entryData->maxTextLen == 5);

	// Verify digits-only (rejects letters and spaces) and 5-digit cap (up to 99999).
	GadgetTextEntrySetText(entryWin, UnicodeString());
	CHECK(GadgetTextEntryInsertCharacter(entryWin, u'a') == false);
	CHECK(GadgetTextEntryInsertCharacter(entryWin, u' ') == false);
	for (char16_t d : std::u16string(u"12345"))
	{
		CHECK(GadgetTextEntryInsertCharacter(entryWin, d) == true);
	}
	CHECK(GadgetTextEntryInsertCharacter(entryWin, u'6') == false);
	CHECK(GadgetTextEntryGetText(entryWin) == u"12345");
}

TEST_CASE("PORT-331 S-1364: ScriptCameraDirector mission start fade-up holds black for fadeUpFrames logic frames")
{
	ScriptCameraDirector dir;
	// Default: 7 logic frames (RW 0x5EBA4A / RW 0x625502)
	dir.startMissionFadeUp();
	CHECK(dir.missionFadeUpWaiting() == true);
	CHECK(dir.missionFadeUpTargetFrame() == 7u);
	dir.update(33.0, 0, 0.5);
	CHECK(dir.missionFadeUpWaiting() == true);
	CHECK(dir.fadeType() == ScriptCameraDirector::FADE_SUBTRACT);
	CHECK(dir.fade() == 1.0f);
	dir.update(33.0, 6, 0.5);
	CHECK(dir.missionFadeUpWaiting() == true);
	CHECK(dir.fade() == 1.0f);
	dir.update(33.0, 7, 0.0);
	CHECK(dir.missionFadeUpWaiting() == false);
	CHECK(dir.fadeType() == ScriptCameraDirector::FADE_NONE);
	CHECK(dir.fade() == 0.0f);

	// Custom mission delay (e.g. MAP ANG Rhudaur: 2000 ms = 10 logic frames)
	dir.startMissionFadeUp(10);
	dir.update(33.0, 9, 1.0);
	CHECK(dir.missionFadeUpWaiting() == true);
	CHECK(dir.fade() == 1.0f);
	dir.update(33.0, 10, 0.0);
	CHECK(dir.missionFadeUpWaiting() == false);
	CHECK(dir.fadeType() == ScriptCameraDirector::FADE_NONE);
	CHECK(dir.fade() == 0.0f);

	// Explicit CAMERA_FADE_* supersedes mission-start fade hold
	dir.startMissionFadeUp(10);
	ScriptClientRequest req;
	req.action = "CAMERA_FADE_ADD";
	req.frame = 2;
	for (int i = 0; i < 5; ++i)
	{
		ScriptParameter sp;
		sp.type = i < 2 ? 1 : 0;
		sp.realValue = i == 1 ? 0.5f : 0.0f;
		sp.intValue = 2;
		req.params.push_back(sp);
	}
	dir.apply(req, Coord3D{ 0.0f, 0.0f, 0.0f }, 0.0f);
	CHECK(dir.missionFadeUpWaiting() == false);
	CHECK(dir.fadeType() == ScriptCameraDirector::FADE_ADD);
}

TEST_CASE("PORT-331 S-5016: AptImageMode parses all six RotWK modes and preserves stop pin")
{
	CHECK(parseAptImageMode("SOLID") == APT_IMAGE_SOLID);
	CHECK(parseAptImageMode("GRAYSCALE") == APT_IMAGE_GRAYSCALE);
	CHECK(parseAptImageMode("ALPHA") == APT_IMAGE_ALPHA);
	CHECK(parseAptImageMode("ADDITIVE") == APT_IMAGE_ADDITIVE);
	CHECK(parseAptImageMode("SOLID_GRAYSCALE") == APT_IMAGE_SOLID_GRAYSCALE);
	CHECK(parseAptImageMode("SOLID_TEXTURE") == APT_IMAGE_SOLID_TEXTURE);
	CHECK(parseAptImageMode("unknown") == APT_IMAGE_ALPHA);
	CHECK(aptImageModeShader(APT_IMAGE_ALPHA).empty());
	CHECK(aptImageModeShader(APT_IMAGE_ADDITIVE).find("blend_add") != std::string::npos);
	CHECK(aptImageModeShader(APT_IMAGE_SOLID_TEXTURE).find("pixel.a = 1.0") != std::string::npos);
	CHECK(aptImageModeStops().size() == 1u);
}
