// OpenBFME. PORT-213 (issue #503): Palantir multi-select context picks the highest-rank qualifying HERO.
//
// TARGET (RotWK 2.01 game.dat, caveat S-001; DONOR BFME2 ControlBarMultiSelect.cpp:427):
//   * ControlBar::evaluateContextUI (RW 0x71EBDA..0x71EF9F): single selection (RW 0x71ED9D) routes to
//     populateCommand (RW 0x943D6F) -> AptPalantir::setDrawable (RW 0x9443BD -> 0x6D4671), giving any single
//     selected object its own portrait and context. Multi-selection routes to populateMultiSelect (RW 0x94475A..0x944910).
//   * ControlBar::populateMultiSelect (RW 0x94475A..0x944839): scans selected drawables' objects for
//     KindOf HERO (+0x113 & 4, bit 90), !IGNORED_IN_GUI (+0x10D & 0x80 == 0, bit 47), and valid
//     ExperienceTracker::FindCurrentLevel (RW 0x79D12A -> 0x689417 -> 0x689024 -> currentLevelRank),
//     selecting the maximum rank (RW 0x9447FA), breaking ties by lowest signed ObjectID (RW 0x94480D).
//     If found, sets ControlBar::m_selected = best, runs populateCommand(best, false) (RW 0x944834 -> 0x943E91
//     updateToggleIndex rather than updateMultiSelectToggleIndex), and sets AptPalantir::setDrawable(best).
//     If none found, passes nullptr (INVALID_ID) to AptPalantir::setDrawable (RW 0x9448FC).
//   * PalantirCommandUI::portraitFor (RW 0x92FE33..0x92FF50) & rankInfoFor (RW 0x9305CE..0x930720): when
//     contextObject == INVALID_ID, skip IGNORED_IN_GUI drawables (RW 0x92FA83 / 0x92FAA5) when comparing
//     shared portrait/rank, falling back on mismatch (or all IGNORED_IN_GUI) to the first selected object's
//     controlling player's template MultiSelectionPortrait, else "MultiPortrait" (RW 0x92FEDD..0x92FF47).
//
// Synthetic INI only (no retail or mod archives required).
// GPL-3.0.

#include "doctest.h"

#include "AptPlayerTestUtil.h"
#include "IniTestUtil.h"
#include "MapTestUtil.h"

#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/PlayerTemplate.h"
#include "GameClient/ControlBar.h"
#include "GameClient/ControlBarCommands.h"
#include "GameClient/GUI/AptScreens/AptPalantir.h"
#include "GameClient/GUI/GameTextSource.h"
#include "GameClient/GUI/Shell/Shell.h"
#include "GameClient/GUI/ShellEnvironment.h"
#include "GameClient/GUI/ShellServices.h"
#include "GameClient/GUI/WindowManager.h"
#include "GameClient/HudInput.h"
#include "GameClient/InGameHud.h"
#include "GameClient/LiveGame.h"
#include "GameClient/PalantirCommandUI.h"
#include "GameClient/TacticalView.h"
#include "GameLogic/Economy.h"
#include "GameLogic/Object/ExperienceTracker.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/Object/RetailObjectWorld.h"
#include "GameLogic/ObjectTemplateInfo.h"
#include "Libraries/WWVegas/WW3D2/assetmgr.h"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

using namespace apttest;

namespace
{
using namespace maptest;

const char kGameData[] = R"(
GameData
  ForceModelsToFollowTimeOfDay = No
  ForceModelsToFollowWeather = No
  UnitDamagedThreshold = 0.5
  UnitReallyDamagedThreshold = 0.25
  DefaultStartingCash = 1000
  TerrainResourceCellSize = 10
  GoodCommandPoints = 10 100
  EvilCommandPoints = 10 100
  GoodCommandPointsAI = 10 100
  EvilCommandPointsAI = 10 100
  GoodCommandPointsBonus = 0
  EvilCommandPointsBonus = 0
  GoodCommandPointLimit = 100
  EvilCommandPointLimit = 100
  PowerLimit = 100
  ResourceBonusMultiplier = 1
  ResourceMultiplierLimit = 1
  MultiPlayMoneyMult = MP1:1.0
  GoodCommandPointsMP2 = 10 100
  GoodCommandPointsMP3 = 10 100
  GoodCommandPointsMP4 = 10 100
  GoodCommandPointsMP5 = 10 100
  GoodCommandPointsMP6 = 10 100
  GoodCommandPointsMP7 = 10 100
  GoodCommandPointsMP8 = 10 100
  EvilCommandPointsMP2 = 10 100
  EvilCommandPointsMP3 = 10 100
  EvilCommandPointsMP4 = 10 100
  EvilCommandPointsMP5 = 10 100
  EvilCommandPointsMP6 = 10 100
  EvilCommandPointsMP7 = 10 100
  EvilCommandPointsMP8 = 10 100
  PartitionCellSize = 10
  UnlookPersistDuration = 1000
  ShroudColor = R:0 G:0 B:0
  FogAlpha = 100
  ShroudAlpha = 200
  StealthFriendlyOpacity = 50%
  MaxPathfindCellsPerFrame = 100
  MaxCellsAdjustDestination = 100
  MaxCellsAdjustHordeMeleeDestination = 100
  MaxCellsPatchPath = 100
  MaxCellsFindPathLimit = 100
  MaxCellsFindAttackPath = 100
  MaxCellsAdjustToMeleeDestination = 100
  MaxCellsFindMeleeEngagementLocation = 100
  MaxCellsAdjustToPossibleDestination = 100
  MaxCellsToExamineTowardsGoal = 100
  MovementPenaltyDamageState = DAMAGED
End
)";

const char kAIData[] = R"(
AIData
  EnableRepulsors = No
  RepulsedDistance = 1
  WadeWaterDepth = 1
  DeepWaterDepth = 2
  HordesWaitForHordes = No
  MeleeApproachTolerance = 1
  MeleeApproachDist = 1
  CastleSiegeStandBackDistance = 1
  AttackUsesLineOfSight = No
End
)";

const char kMultiplayer[] = R"(
MultiplayerSettings
  InitialCreditsVeryLow = 100
  InitialCreditsLow = 100
  InitialCreditsMedium = 100
  InitialCreditsHigh = 100
  InitialCreditsVeryHigh = 100
  UseShroud = No
End
)";

const char kPlayerTemplates[] = R"(
PlayerTemplate FactionSynthGood
  Side = SynthMen
  PlayableSide = Yes
  Evil = No
  StartMoney = 1000
  MultiSelectionPortrait = SynthFactionMultiPortrait
End
PlayerTemplate FactionSynthEvil
  Side = SynthMordor
  PlayableSide = Yes
  Evil = Yes
  StartMoney = 1000
  MultiSelectionPortrait = SynthEvilMultiPortrait
End
PlayerTemplate FactionSynthNoMulti
  Side = SynthPlain
  PlayableSide = Yes
  Evil = No
  StartMoney = 1000
End
)";

const char kExperienceLevels[] = R"(
ExperienceLevel SynthAlphaLvl1
  TargetNames = SynthHeroAlpha
  RequiredExperience = 1
  Rank = 1
End
ExperienceLevel SynthAlphaLvl2
  TargetNames = SynthHeroAlpha
  RequiredExperience = 50
  Rank = 2
End
ExperienceLevel SynthAlphaLvl3
  TargetNames = SynthHeroAlpha
  RequiredExperience = 100
  Rank = 3
End
ExperienceLevel SynthBetaLvl1
  TargetNames = SynthHeroBeta
  RequiredExperience = 1
  Rank = 1
End
ExperienceLevel SynthBetaLvl2
  TargetNames = SynthHeroBeta
  RequiredExperience = 50
  Rank = 2
End
ExperienceLevel SynthBetaLvl3
  TargetNames = SynthHeroBeta
  RequiredExperience = 100
  Rank = 3
End
ExperienceLevel SynthBetaLvl4
  TargetNames = SynthHeroBeta
  RequiredExperience = 200
  Rank = 4
End
ExperienceLevel SynthEnemyHeroLvl1
  TargetNames = SynthEnemyHero
  RequiredExperience = 1
  Rank = 1
End
ExperienceLevel SynthIgnoredHeroLvl1
  TargetNames = SynthIgnoredHero
  RequiredExperience = 1
  Rank = 1
End
ExperienceLevel SynthHordeLvl1
  TargetNames = SynthHordeA
  RequiredExperience = 1
  Rank = 1
End
ExperienceLevel SynthHordeLvl2
  TargetNames = SynthHordeA
  RequiredExperience = 50
  Rank = 2
End
ExperienceLevel SynthSpOnlyLvl1
  TargetNames = SynthSpOnlyHero
  RequiredExperience = 1
  Rank = 1
  SinglePlayerOnly = Yes
End
)";

const char kCommandButtons[] = R"(
CommandButton Command_SynthHeroToggle
  Command = TOGGLE_NO_AUTO_ACQUIRE
  Options = TOGGLE_IMAGE_ON_WEAPON OK_FOR_MULTI_SELECT
  ButtonImage = ImgHeroAutoOn ImgHeroAutoOff
  TextLabel = CONTROLBAR:HeroAutoOn CONTROLBAR:HeroAutoOff
  DescriptLabel = CONTROLBAR:TipHeroAutoOn CONTROLBAR:TipHeroAutoOff
  InPalantir = Yes
End
CommandButton Command_SynthBetaGuard
  Command = GUARD
  Options = OK_FOR_MULTI_SELECT
  ButtonImage = ImgBetaGuard
  TextLabel = CONTROLBAR:BetaGuard
  DescriptLabel = CONTROLBAR:TipBetaGuard
  InPalantir = Yes
End
CommandButton Command_SynthHordeToggle
  Command = TOGGLE_NO_AUTO_ACQUIRE
  Options = TOGGLE_IMAGE_ON_WEAPON OK_FOR_MULTI_SELECT
  ButtonImage = ImgHordeAutoOn ImgHordeAutoOff
  TextLabel = CONTROLBAR:HordeAutoOn CONTROLBAR:HordeAutoOff
  DescriptLabel = CONTROLBAR:TipHordeAutoOn CONTROLBAR:TipHordeAutoOff
  InPalantir = Yes
End
)";

const char kCommandSets[] = R"(
CommandSet SynthHeroAlphaCS
  1 = Command_SynthHeroToggle
End
CommandSet SynthHeroBetaCS
  1 = Command_SynthBetaGuard
End
CommandSet SynthHordeCS
  1 = Command_SynthHordeToggle
End
CommandSet SynthSharedToggleCS
  1 = Command_SynthHeroToggle
End
)";

const char kObjects[] = R"(
Object SynthHeroAlpha
  KindOf = INFANTRY HERO SELECTABLE CAN_ATTACK
  IsTrainable = Yes
  SelectPortrait = PortraitHeroAlpha
  CommandSet = SynthHeroAlphaCS
  Body = ActiveBody ModuleTag_Body
    MaxHealth = 500
    InitialHealth = 500
  End
  Behavior = AIUpdateInterface ModuleTag_AI
  End
End
Object SynthHeroAlphaSharedCS
  KindOf = INFANTRY HERO SELECTABLE CAN_ATTACK
  IsTrainable = Yes
  SelectPortrait = PortraitHeroAlphaShared
  CommandSet = SynthSharedToggleCS
  Body = ActiveBody ModuleTag_Body
    MaxHealth = 500
    InitialHealth = 500
  End
  Behavior = AIUpdateInterface ModuleTag_AI
  End
End
Object SynthHeroBeta
  KindOf = INFANTRY HERO SELECTABLE CAN_ATTACK
  IsTrainable = Yes
  SelectPortrait = PortraitHeroBeta
  CommandSet = SynthHeroBetaCS
  Body = ActiveBody ModuleTag_Body
    MaxHealth = 500
    InitialHealth = 500
  End
  Behavior = AIUpdateInterface ModuleTag_AI
  End
End
Object SynthEnemyHero
  KindOf = INFANTRY HERO SELECTABLE CAN_ATTACK
  IsTrainable = Yes
  SelectPortrait = PortraitEnemyHero
  CommandSet = SynthHeroBetaCS
  Body = ActiveBody ModuleTag_Body
    MaxHealth = 500
    InitialHealth = 500
  End
  Behavior = AIUpdateInterface ModuleTag_AI
  End
End
Object SynthIgnoredHero
  KindOf = INFANTRY HERO IGNORED_IN_GUI SELECTABLE
  IsTrainable = Yes
  SelectPortrait = PortraitIgnoredHero
  CommandSet = SynthHeroBetaCS
  Body = ActiveBody ModuleTag_Body
    MaxHealth = 500
    InitialHealth = 500
  End
End
Object SynthNoExpHero
  KindOf = INFANTRY HERO SELECTABLE
  IsTrainable = Yes
  SelectPortrait = PortraitNoExpHero
  CommandSet = SynthHeroBetaCS
  Body = ActiveBody ModuleTag_Body
    MaxHealth = 500
    InitialHealth = 500
  End
End
Object SynthSpOnlyHero
  KindOf = INFANTRY HERO SELECTABLE
  IsTrainable = Yes
  SelectPortrait = PortraitSpOnlyHero
  CommandSet = SynthHeroBetaCS
  Body = ActiveBody ModuleTag_Body
    MaxHealth = 500
    InitialHealth = 500
  End
End
Object SynthHordeA
  KindOf = HORDE SELECTABLE CAN_ATTACK
  IsTrainable = Yes
  SelectPortrait = PortraitHordeA
  CommandSet = SynthSharedToggleCS
  Body = ActiveBody ModuleTag_Body
    MaxHealth = 200
    InitialHealth = 200
  End
  Behavior = AIUpdateInterface ModuleTag_AI
  End
End
Object SynthHordeB
  KindOf = HORDE SELECTABLE CAN_ATTACK
  IsTrainable = No
  SelectPortrait = PortraitHordeB
  CommandSet = SynthHordeCS
  Body = ActiveBody ModuleTag_Body
    MaxHealth = 200
    InitialHealth = 200
  End
  Behavior = AIUpdateInterface ModuleTag_AI
  End
End
Object SynthIgnoredProp
  KindOf = IGNORED_IN_GUI SELECTABLE
  IsTrainable = No
  SelectPortrait = PortraitIgnoredProp
  Body = ActiveBody ModuleTag_Body
    MaxHealth = 100
    InitialHealth = 100
  End
End
Object SynthDiscountStructure
  KindOf = SELECTABLE STRUCTURE
  IsTrainable = No
  SelectPortrait = PortraitDiscountStructure
  Behavior = CostModifierUpgrade ModuleTag_Discount
    StartsActive = Yes
    UpgradeDiscount = Yes
    LabelForPalantirString = TEST:Discount
    Percentage = -10%
  End
  Body = ActiveBody ModuleTag_Body
    MaxHealth = 500
    InitialHealth = 500
  End
End
)";

std::string buildTestMap(const char *localFaction)
{
	Toc t;
	W h;
	h.i32(16).i32(16).i32(0).i32(1).i32(16).i32(16).i32(256);
	for (int i = 0; i < 256; ++i)
	{
		h.u16(0);
	}
	D side1(t);
	side1.s("playerName", "Player_1")
		.b("playerIsHuman", true)
		.s("playerFaction", localFaction)
		.s("playerEnemies", "Player_2");
	D side2(t);
	side2.s("playerName", "Player_2")
		.b("playerIsHuman", false)
		.s("playerFaction", "FactionSynthEvil")
		.s("playerEnemies", "Player_1");
	W sl;
	sl.u8(1).i32(2).raw(side1.bytes()).i32(0).raw(side2.bytes()).i32(0);
	D team1(t);
	team1.s("teamName", "teamPlayer_1").s("teamOwner", "Player_1").b("teamIsSingleton", true);
	D team2(t);
	team2.s("teamName", "teamPlayer_2").s("teamOwner", "Player_2").b("teamIsSingleton", true);
	Bytes chunks = cat(chunk(t, "HeightMapData", 5, h.b), chunk(t, "WorldInfo", 1, D(t).i("weather", 0).bytes()));
	chunks = cat(chunks, chunk(t, "SidesList", 6, sl.b));
	chunks = cat(chunks, chunk(t, "Teams", 1, W().i32(2).raw(team1.bytes()).raw(team2.bytes()).b));
	Bytes map = file(t, chunks);
	return std::string(map.begin(), map.end());
}

struct Port213Text : GameTextSource
{
	bool fetch(const std::string &label, std::u16string &out) const override
	{
		if (label == "APT:RankLabel")
		{
			out = u"Rank %d";
			return true;
		}
		if (label == "APT:PalantirTimeRemaining")
		{
			out = u"Time";
			return true;
		}
		if (label == "TEST:Discount")
		{
			out = u"Discount %d%%";
			return true;
		}
		return false;
	}
};

struct Port213HudRig
{
	initest::Fixture files;
	RetailObjectWorld world;
	ArchiveW3DFileSource w3dSource;
	WW3DAssetManager assets;
	MapObjectOptions mapOptions;
	LiveGame game;
	PinholeView view;
	MetaMap meta;
	MemorySource movies;
	RecordingShellServices services;
	ShellEnvironment environment;
	AptScreenFactoryTable factories;
	WindowManager windows;
	Shell shell;
	Port213Text text;
	std::unique_ptr<InGameHud> hud;
	std::string error;
	int costShows = 0;
	int costHides = 0;

	explicit Port213HudRig(const char *localFaction = "FactionSynthGood")
		: world(files.fsys)
		, w3dSource(files.fsys)
		, assets(w3dSource)
		, game(world, files.fsys, assets, mapOptions)
		, windows(movies, services)
		, shell(windows, factories, services, environment)
	{
		files.mount({
			{ "Data\\INI\\Default\\SoundEffects.ini", "AudioEvent DefaultSoundEffect\n  Volume = 100\n  Type = ui everyone\n  Type = +DEFAULT\nEnd\n" },
			{ "Data\\INI\\Default\\SubsystemLegendExpansion1.ini",
			  "LoadSubsystem ThePlayerTemplateStore\n  InitFile = Data\\INI\\PlayerTemplate.ini\nEnd\n"
			  "LoadSubsystem TheThingFactory\n  InitFile = Data\\INI\\SyntheticObjects.ini\nEnd\n"
			  "LoadSubsystem TheExperienceLevelSystem\n  InitFile = Data\\INI\\ExperienceLevels.ini\nEnd\n" },
			{ "Data\\INI\\PlayerTemplate.ini", kPlayerTemplates },
			{ "Data\\INI\\SyntheticObjects.ini", kObjects },
			{ "Data\\INI\\ExperienceLevels.ini", kExperienceLevels },
			{ "Data\\INI\\CommandButton.ini", kCommandButtons },
			{ "Data\\INI\\CommandSet.ini", kCommandSets },
			{ "Maps\\synthetic\\synthetic.map", buildTestMap(localFaction) },
			{ "Data\\INI\\GameData.ini", kGameData },
			{ "Data\\INI\\Default\\AIData.ini", kAIData },
			{ "Data\\INI\\Multiplayer.ini", kMultiplayer },
			{ "Data\\INI\\InGameUI.ini", "InGameUI\n  SelectNearestBuilderCycleTimeOut = 1000\nEnd\n" },
			{ "Data\\Scripts\\Scripts.lua", "-- no scripts\n" },
			{ "Data\\Scripts\\ScriptEvents.xml", "<SageLuaScriptSection>\n</SageLuaScriptSection>\n" },
		});
		REQUIRE_MESSAGE(world.load(&error), error);
		LiveGame::Options opts;
		opts.mapName = "synthetic";
		opts.mapScripts = 0;
		REQUIRE_MESSAGE(game.load(opts, &error), error);

		TestMovie level0;
		level0.setRootFrames({ {} });
		movies.add("AptLevel0", level0);
		TestMovie pal;
		auto actions = program(pal, [](Asm &a) {
			for (const char *name : {
					 "SetPalantirFrameState", "SetPlayerButtonsState", "EnablePlayerMagicButton", "SetPlayerFaction",
					 "ShowCommandInterface", "HideCommandInterface",
					 "ShowRankInterface", "HideRankInterface", "ShowRankProgress", "HideRankProgress", "SetRankProgressBar",
					 "SetPlayerMagicProgress", "HighlightPlayerMagicButton", "PlayPlayerLevelUpEffect" })
			{
				a.defineFunction(name, {}, [](Asm &) {});
			}
			a.defineFunction("ShowCostModifierUpgradeInterface", {}, [](Asm &f) { fscmd(f, "Test:P213ShowCost"); });
			a.defineFunction("HideCostModifierUpgradeInterface", {}, [](Asm &f) { fscmd(f, "Test:P213HideCost"); });
			fscmd(a, "AptPalantir::OnInitialized");
		});
		pal.setRootFrames({ { pal.addActionItem(actions) } });
		movies.add("Palantir", pal);
		factories.registerFactory("Palantir.apt", [](AptScreenContext &c) { return std::make_unique<AptPalantir>(c.windows, c.shell); });
		windows.init();
		windows.registerCommand("Test:P213ShowCost", [this](const std::string &) { ++costShows; });
		windows.registerCommand("Test:P213HideCost", [this](const std::string &) { ++costHides; });
		shell.push("Palantir.apt", true);
		windows.setMinimumFrameTime(false);
		windows.update(0);
		environment.gameText = &text;
		hud = std::make_unique<InGameHud>(InGameHud::Config{ game, world, files.fsys, services, view, MouseSettings{}, meta, &text, &windows, &shell });
		REQUIRE_MESSAGE(hud->boot(&error), error);
	}

	Object *spawn(const char *tmpl, const char *playerName, float x, float y)
	{
		Player *p = game.players().findPlayerWithName(playerName);
		REQUIRE(p != nullptr);
		Object *obj = game.createObject(tmpl, p->getPlayerIndex(), Coord3D{ x, y, 0.0f }, 0.0f, &error);
		REQUIRE_MESSAGE(obj != nullptr, error);
		return obj;
	}

	void selectObjects(std::initializer_list<Object *> objs)
	{
		hud->input().ui().deselectAll(false);
		// InGameUI::selectObject prepends (ZH push_front), so insert in reverse to preserve caller list order.
		for (auto it = std::rbegin(objs); it != std::rend(objs); ++it)
		{
			Object *o = *it;
			REQUIRE(o != nullptr);
			hud->input().ui().selectObject(o->getID());
		}
		hud->update(0.033);
		hud->update(0.033);
	}

	void stepFrames(int n = 2)
	{
		for (int i = 0; i < n; ++i)
		{
			game.advance(0.033);
			hud->update(0.033);
		}
	}
};
} // namespace

TEST_CASE("PORT-213 single object keeps its own portrait, rank, and command context (RW 0x71ED9D -> 0x9443BD)")
{
	Port213HudRig r;
	Object *hero = r.spawn("SynthHeroAlpha", "Player_1", 10.0f, 10.0f);
	Object *horde = r.spawn("SynthHordeA", "Player_1", 30.0f, 10.0f);
	Object *noExpHero = r.spawn("SynthNoExpHero", "Player_1", 50.0f, 10.0f);
	Object *ignoredHero = r.spawn("SynthIgnoredHero", "Player_1", 70.0f, 10.0f);

	r.selectObjects({ hero });
	REQUIRE(r.hud->palantir() != nullptr);
	CHECK(r.hud->controlBar().contextObject() == hero->getID());
	CHECK(r.hud->controlBar().sourceObject() == hero->getID());
	CHECK(r.hud->controlBar().commandSetName() == "SynthHeroAlphaCS");
	CHECK(r.hud->palantir()->portraitShown() == "PortraitHeroAlpha");
	CHECK(r.hud->palantir()->rankInterfaceShown());
	REQUIRE(r.windows.aptText("APT:HeroRank") != nullptr);
	CHECK(*r.windows.aptText("APT:HeroRank") == "Rank 1");

	r.selectObjects({ horde });
	CHECK(r.hud->controlBar().contextObject() == horde->getID());
	CHECK(r.hud->controlBar().sourceObject() == horde->getID());
	CHECK(r.hud->palantir()->portraitShown() == "PortraitHordeA");

	// Even a HERO lacking ExperienceLevel or marked IGNORED_IN_GUI shows its own portrait when selected alone.
	r.selectObjects({ noExpHero });
	CHECK(r.hud->controlBar().contextObject() == noExpHero->getID());
	CHECK(r.hud->palantir()->portraitShown() == "PortraitNoExpHero");

	r.selectObjects({ ignoredHero });
	CHECK(r.hud->controlBar().contextObject() == ignoredHero->getID());
	CHECK(r.hud->palantir()->portraitShown() == "PortraitIgnoredHero");
}

TEST_CASE("PORT-213 multi-select of two heroes chooses highest rank, breaking ties by lowest signed ObjectID (RW 0x94475A..0x944839)")
{
	Port213HudRig r;
	Object *heroA = r.spawn("SynthHeroAlpha", "Player_1", 10.0f, 10.0f);
	Object *heroB = r.spawn("SynthHeroBeta", "Player_1", 30.0f, 10.0f);
	// tolerance: verify lower-ObjectID precondition for equal-rank tie-break against RW 0x94480D
	REQUIRE(static_cast<std::int32_t>(heroA->getID()) < static_cast<std::int32_t>(heroB->getID()));
	REQUIRE(heroA->getExperienceTracker() != nullptr);
	REQUIRE(heroB->getExperienceTracker() != nullptr);

	// Raise heroB to rank 3 while heroA stays rank 1: heroB must win in both selection orders.
	heroB->getExperienceTracker()->gainLevels(2, false);
	REQUIRE(heroA->getExperienceTracker()->getRank() == 1);
	REQUIRE(heroB->getExperienceTracker()->getRank() == 3);

	r.selectObjects({ heroA, heroB });
	CHECK(r.hud->controlBar().contextObject() == heroB->getID());
	CHECK(r.hud->controlBar().sourceObject() == heroB->getID());
	CHECK(r.hud->controlBar().commandSetName() == "SynthHeroBetaCS");
	CHECK(r.hud->palantir()->portraitShown() == "PortraitHeroBeta");
	CHECK(r.hud->palantir()->rankInterfaceShown());
	REQUIRE(r.windows.aptText("APT:HeroRank") != nullptr);
	CHECK(*r.windows.aptText("APT:HeroRank") == "Rank 3");

	r.selectObjects({ heroB, heroA });
	CHECK(r.hud->controlBar().contextObject() == heroB->getID());
	CHECK(r.hud->palantir()->portraitShown() == "PortraitHeroBeta");

	// Equal rank (both rank 3): lowest signed ObjectID (heroA) wins even when heroB was selected first (RW 0x94480D).
	heroA->getExperienceTracker()->gainLevels(2, false);
	REQUIRE(heroA->getExperienceTracker()->getRank() == 3);
	r.selectObjects({ heroB, heroA });
	CHECK(r.hud->controlBar().contextObject() == heroA->getID());
	CHECK(r.hud->controlBar().sourceObject() == heroA->getID());
	CHECK(r.hud->controlBar().commandSetName() == "SynthHeroAlphaCS");
	CHECK(r.hud->palantir()->portraitShown() == "PortraitHeroAlpha");
	CHECK(*r.windows.aptText("APT:HeroRank") == "Rank 3");

	// Live level-up while both stay selected: heroB reaches rank 4 and immediately takes over Palantir context.
	heroB->getExperienceTracker()->gainLevels(1, false);
	REQUIRE(heroB->getExperienceTracker()->getRank() == 4);
	r.stepFrames(2);
	CHECK(r.hud->controlBar().contextObject() == heroB->getID());
	CHECK(r.hud->controlBar().sourceObject() == heroB->getID());
	CHECK(r.hud->controlBar().commandSetName() == "SynthHeroBetaCS");
	CHECK(r.hud->palantir()->portraitShown() == "PortraitHeroBeta");
	CHECK(*r.windows.aptText("APT:HeroRank") == "Rank 4");
}

TEST_CASE("PORT-213 multi-select of hero and horde chooses hero regardless of order, horde rank, toggle vote, or cost modifier (RW 0x94475A / 0x943E91)")
{
	Port213HudRig r;
	Object *horde1 = r.spawn("SynthHordeA", "Player_1", 10.0f, 10.0f);
	Object *horde2 = r.spawn("SynthHordeA", "Player_1", 20.0f, 10.0f);
	Object *hero = r.spawn("SynthHeroAlpha", "Player_1", 40.0f, 10.0f);
	Object *discount = r.spawn("SynthDiscountStructure", "Player_1", 60.0f, 10.0f);
	discount->friend_onBuildComplete();

	// Even when the horde is selected first and outranks the hero (rank 2 > rank 1), the HERO wins (RW 0x9447AF).
	horde1->getExperienceTracker()->gainLevels(1, false);
	REQUIRE(horde1->getExperienceTracker()->getRank() == 2);
	REQUIRE(hero->getExperienceTracker()->getRank() == 1);

	r.selectObjects({ horde1, hero });
	CHECK(r.hud->controlBar().contextObject() == hero->getID());
	CHECK(r.hud->controlBar().sourceObject() == hero->getID());
	CHECK(r.hud->controlBar().commandSetName() == "SynthHeroAlphaCS");
	CHECK(r.hud->palantir()->portraitShown() == "PortraitHeroAlpha");
	CHECK(r.hud->palantir()->rankInterfaceShown());
	REQUIRE(r.windows.aptText("APT:HeroRank") != nullptr);
	CHECK(*r.windows.aptText("APT:HeroRank") == "Rank 1");

	r.selectObjects({ hero, horde1 });
	CHECK(r.hud->controlBar().contextObject() == hero->getID());
	CHECK(r.hud->palantir()->portraitShown() == "PortraitHeroAlpha");

	// RW 0x94482E -> 0x943E91: when populateMultiSelect picks a hero, toggle buttons read that hero alone
	// via updateToggleIndex (NO_AUTO_ACQUIRE clear -> toggle index 1, ImgHeroAutoOff) rather than
	// updateMultiSelectToggleIndex reading the first non-ignored object (horde1 has NO_AUTO_ACQUIRE -> index 0, ImgHeroAutoOn).
	const int noAutoIdx = ObjectTemplateInfoBuilder::objectStatusIndex("NO_AUTO_ACQUIRE");
	REQUIRE(noAutoIdx >= 0); // tolerance: objectStatusIndex returns -1 only if NO_AUTO_ACQUIRE is missing
	const unsigned noAuto = static_cast<unsigned>(noAutoIdx);
	horde1->setStatus(noAuto, true);
	horde2->setStatus(noAuto, false);
	hero->setStatus(noAuto, false);
	r.selectObjects({ horde1, horde2, hero });
	REQUIRE(r.hud->controlBar().contextObject() == hero->getID());
	REQUIRE(r.hud->controlBar().palantirButtons().size() == 1);
	CHECK(r.hud->controlBar().palantirButtons().front().image == "ImgHeroAutoOff");

	// Selecting only the two hordes drops back to m_contextSource == INVALID_ID and uses multi-select toggle (RW 0x944603 -> horde1).
	r.selectObjects({ horde1 });
	const unsigned vSingleHorde = r.hud->controlBar().version();
	CHECK(r.hud->controlBar().contextObject() == horde1->getID());
	CHECK(r.hud->controlBar().sourceObject() == horde1->getID());
	r.selectObjects({ horde1, horde2 });
	CHECK(r.hud->controlBar().contextObject() == INVALID_ID);
	CHECK(r.hud->controlBar().sourceObject() == horde1->getID());
	CHECK(r.hud->controlBar().version() == vSingleHorde + 1u);
	REQUIRE(r.hud->controlBar().palantirButtons().size() == 1);
	CHECK(r.hud->controlBar().palantirButtons().front().image == "ImgHeroAutoOn");

	// CostModifierUpgrade shows on single structure, and hides when co-selected with hero because contextObject becomes hero (RW 0x930970).
	r.selectObjects({ discount });
	REQUIRE(r.windows.aptText("APT:CostModifierUpgrade") != nullptr);
	CHECK(*r.windows.aptText("APT:CostModifierUpgrade") == "Discount -10%");
	CHECK(r.costShows == 1);
	CHECK(r.costHides == 0);
	r.selectObjects({ discount, hero });
	CHECK(r.hud->controlBar().contextObject() == hero->getID());
	CHECK(r.hud->palantir()->portraitShown() == "PortraitHeroAlpha");
	CHECK(r.hud->palantir()->rankInterfaceShown());
	REQUIRE(r.windows.aptText("APT:HeroRank") != nullptr);
	CHECK(*r.windows.aptText("APT:HeroRank") == "Rank 1");
	CHECK(r.costHides == 1);
}

TEST_CASE("PORT-213 multi-select of hordes and filtered objects matches RW portrait and rank fallback rules (RW 0x9448FC / 0x92FA83 / 0x92FE33)")
{
	{
		Port213HudRig r;
		Object *hordeA1 = r.spawn("SynthHordeA", "Player_1", 10.0f, 10.0f);
		Object *hordeA2 = r.spawn("SynthHordeA", "Player_1", 20.0f, 10.0f);
		Object *hordeB = r.spawn("SynthHordeB", "Player_1", 30.0f, 10.0f);
		Object *ignoredHero = r.spawn("SynthIgnoredHero", "Player_1", 40.0f, 10.0f);
		Object *noExpHero = r.spawn("SynthNoExpHero", "Player_1", 50.0f, 10.0f);
		Object *spOnlyHero = r.spawn("SynthSpOnlyHero", "Player_1", 60.0f, 10.0f);
		Object *ignoredProp1 = r.spawn("SynthIgnoredProp", "Player_1", 70.0f, 10.0f);
		Object *ignoredProp2 = r.spawn("SynthIgnoredProp", "Player_1", 80.0f, 10.0f);

		// Two hordes with different portraits -> MultiSelectionPortrait
		r.selectObjects({ hordeA1, hordeB });
		CHECK(r.hud->controlBar().contextObject() == INVALID_ID);
		CHECK(r.hud->palantir()->portraitShown() == "SynthFactionMultiPortrait");
		CHECK_FALSE(r.hud->palantir()->rankInterfaceShown());

		// Two hordes with the same portrait & same rank -> shared portrait and shared rank (RW 0x92FE6D / 0x9305CE)
		r.selectObjects({ hordeA1, hordeA2 });
		CHECK(r.hud->controlBar().contextObject() == INVALID_ID);
		CHECK(r.hud->palantir()->portraitShown() == "PortraitHordeA");
		CHECK(r.hud->palantir()->rankInterfaceShown());
		REQUIRE(r.windows.aptText("APT:HeroRank") != nullptr);
		CHECK(*r.windows.aptText("APT:HeroRank") == "Rank 1");

		// Rank mismatch hides rank while keeping shared portrait (RW 0x92F5D5)
		hordeA2->getExperienceTracker()->gainLevels(1, false);
		r.stepFrames(2);
		CHECK(r.hud->palantir()->portraitShown() == "PortraitHordeA");
		CHECK_FALSE(r.hud->palantir()->rankInterfaceShown());

		// IGNORED_IN_GUI skip in portraitFor / rankInfoFor (RW 0x92FA83 / 0x92FAA5)
		Object *hordeA3 = r.spawn("SynthHordeA", "Player_1", 90.0f, 10.0f);
		r.selectObjects({ ignoredProp1, hordeA1, hordeA3 });
		CHECK(r.hud->controlBar().contextObject() == INVALID_ID);
		CHECK(r.hud->palantir()->portraitShown() == "PortraitHordeA");
		CHECK(r.hud->palantir()->rankInterfaceShown());
		CHECK(*r.windows.aptText("APT:HeroRank") == "Rank 1");

		// All selected are IGNORED_IN_GUI -> MultiSelectionPortrait fallback (RW 0x92FEDD)
		r.selectObjects({ ignoredProp1, ignoredProp2 });
		CHECK(r.hud->controlBar().contextObject() == INVALID_ID);
		CHECK(r.hud->palantir()->portraitShown() == "SynthFactionMultiPortrait");
		CHECK_FALSE(r.hud->palantir()->rankInterfaceShown());

		// HERO + IGNORED_IN_GUI is skipped by both populateMultiSelect (RW 0x9447A6) and portraitFor (RW 0x92FA83).
		r.selectObjects({ ignoredHero, hordeA1, hordeA3 });
		CHECK(r.hud->controlBar().contextObject() == INVALID_ID);
		CHECK(r.hud->palantir()->portraitShown() == "PortraitHordeA");

		// HERO without ExperienceLevel does not win populateMultiSelect (RW 0x9447DA); because it is not IGNORED_IN_GUI,
		// portraitFor still compares its portrait against hordeA1 and falls back to MultiSelectionPortrait.
		r.selectObjects({ noExpHero, hordeA1, hordeA3 });
		CHECK(r.hud->controlBar().contextObject() == INVALID_ID);
		CHECK(r.hud->palantir()->portraitShown() == "SynthFactionMultiPortrait");

		// HERO whose current level is SinglePlayerOnly = Yes qualifies in MODE_SINGLE_PLAYER (RW 0x9447D3),
		// then loses its valid level when evaluated in MODE_SKIRMISH.
		r.game.logic().economy().context().gameMode = EconomyContext::MODE_SINGLE_PLAYER;
		spOnlyHero->getExperienceTracker()->gainLevel(false);
		REQUIRE(spOnlyHero->getExperienceTracker()->getLevelName() == "SynthSpOnlyLvl1");
		r.selectObjects({ spOnlyHero, hordeA1, hordeA3 });
		CHECK(r.hud->controlBar().contextObject() == spOnlyHero->getID());
		CHECK(r.hud->palantir()->portraitShown() == "PortraitSpOnlyHero");

		r.game.logic().economy().context().gameMode = EconomyContext::MODE_SKIRMISH;
		r.selectObjects({ spOnlyHero, hordeA1, hordeA3 });
		CHECK(r.hud->controlBar().contextObject() == INVALID_ID);
		CHECK(r.hud->palantir()->portraitShown() == "SynthFactionMultiPortrait");
	}

	// Empty MultiSelectionPortrait on PlayerTemplate falls back to "MultiPortrait" (RW 0x92FF47)
	{
		Port213HudRig rNoMulti("FactionSynthNoMulti");
		Object *hordeA = rNoMulti.spawn("SynthHordeA", "Player_1", 10.0f, 10.0f);
		Object *hordeB = rNoMulti.spawn("SynthHordeB", "Player_1", 20.0f, 10.0f);
		rNoMulti.selectObjects({ hordeA, hordeB });
		CHECK(rNoMulti.hud->controlBar().contextObject() == INVALID_ID);
		CHECK(rNoMulti.hud->palantir()->portraitShown() == "MultiPortrait");
	}
}

TEST_CASE("PORT-213 foreign selection keeps foreignView and avoids falsely pinning non-hero context (RW 0x71EBDA / 0x9448FC -> 0x94397F)")
{
	Port213HudRig r;
	Object *enemyHorde = r.spawn("SynthHordeB", "Player_2", 10.0f, 10.0f);
	Object *enemyHero = r.spawn("SynthEnemyHero", "Player_2", 30.0f, 10.0f);

	// Single foreign hero (SelectionTranslator limits normal enemy selection to 1 unit): drives contextObject, foreignView, and portrait.
	r.selectObjects({ enemyHero });
	CHECK(r.hud->controlBar().contextObject() == enemyHero->getID());
	CHECK(r.hud->controlBar().sourceObject() == enemyHero->getID());
	CHECK(r.hud->controlBar().foreignView());
	CHECK(r.hud->controlBar().commandSetName() == "SynthHeroBetaCS");
	CHECK(r.hud->palantir()->portraitShown() == "PortraitEnemyHero");

	// Foreign multi-selection stays outside locally-controlled CB_CONTEXT_MULTI_SELECT (PORT-221): contextObject must
	// still be INVALID_ID rather than pinning enemyHorde as a single-object Palantir context.
	r.selectObjects({ enemyHorde, enemyHero });
	CHECK(r.hud->controlBar().contextObject() == INVALID_ID);
	CHECK(r.hud->controlBar().foreignView());
	CHECK(r.hud->palantir()->portraitShown() == "SynthEvilMultiPortrait");
}
