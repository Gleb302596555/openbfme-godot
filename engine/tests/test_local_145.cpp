// OpenBFME tests: LOCAL-145 (fork issue 871) — Ennorath 1.9.8.6 / 1.9.8.7 exe edits on the 1.9.8.7 profile
// (--mod-engine=f222b925edf2a9553e574ad8445f2841).
// Sources of expectations:
//   - PYSAGE 1.9.8.7 .wmkeys 0xF3B000 (31-entry table: +ETKeySelectAll 0x64, +ETKeySelectAllHeroes 0xAB, +ETKeyTacticalMenu 0x99)
//   - PYSAGE 1.9.8.7 .hotkeys 0xF38490 / 0xF384D6 / 0xF38510 (hooks RW 0x92DB1F, RW 0x81F983: builder click sound Gui_PalantirCommandButtonClick, selectAllHeroes)
//   - PYSAGE 1.9.8.7 .camfix 0xF37060..0xF370E0 (hook RW 0x69E131: on-screen builder camera check)
//   - PYSAGE 1.9.8.7 .wmfeat 0xF3A000..0xF3A1E6 (hooks RW 0x65A93F, 0x694F06, 0x92D052: PortraitUpgrade Portrait / ButtonImage)
//   - PYSAGE 1.9.8.7 .wmfeat 0xF3A20D..0xF3A349 (hook RW 0x888012: FilteredHordeRespawnBehavior)
//   - PYSAGE 1.9.8.7 .wmfeat 0xF3A34C..0xF3A41F, 0xF3A5C0 (hooks RW 0x6AAFC3, 0x6AB0D0, 0x853FA0, 0x8FE8C6: KindOf CANT_EARN_POWERPOINTS bit 222)
//   - PYSAGE 1.9.8.7 .wmfeat 0xF3A940 (hook RW 0x8B1FFE in RW 0x8B1FDA: ToggleMountedSpecialAbilityUpdate at LocomotorSet Speed == 0)
//   - PYSAGE 1.9.8.7 .wmfeat 0xF3A95F / 0xF3A9B2 (hooks RW 0x87CF94, 0x8670D2: alive non-horde HERO survives STRUCTURE GarrisonContain death)
//   - PYSAGE 1.9.8.7 .wmfeat 0xF3AA56..0xF3AAB8 (exclude workers without GUI_COMMAND_DOZER_CONSTRUCT 0x35 from builder hotkey)
// Off the 1.9.8.7 profile, retail 2.01 (or 1.9.8.5) behaviour holds. GPL-3.0.

#include "doctest.h"
#include "CombatTestUtil.h"
#include "SyntheticHudTestUtil.h"

#include "Common/Audio/AudioEntryPoints.h"
#include "Common/INI.h"
#include "Common/ModEngineFeatures.h"
#include "Common/Player.h"
#include "Common/PlayerScience.h"
#include "Common/PlayerTemplate.h"
#include "Common/SpecialPower.h"
#include "Common/Thing/KindOfTokens.h"
#include "Common/Thing/ThingFactory.h"
#include "Common/Upgrade.h"
#include "GameClient/ControlBarCommands.h"
#include "GameClient/EnnorathOptions.h"
#include "GameClient/InGameHeroSelect.h"
#include "GameClient/MessageStream/CommandXlat.h"
#include "GameClient/MessageStream/MetaEvent.h"
#include "GameClient/OptionPreferences.h"
#include "GameClient/PalantirCommandUI.h"
#include "GameLogic/ExperienceWorld.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AreaScanModules.h"
#include "GameLogic/Module/LogicModules.h"
#include "GameLogic/Module/DozerAIUpdate.h"
#include "GameLogic/Module/SpecialAbilityModules.h"
#include "GameLogic/Module/UpgradeModuleClasses.h"
#include "GameLogic/Object/Contain/GarrisonContainRuntime.h"
#include "GameLogic/Object/Contain/HordeContainRuntime.h"
#include "GameLogic/Object/Object.h"

#include <algorithm>
#include <string>
#include <vector>

namespace
{
struct ModGuard
{
	~ModGuard() { ModEngineFeatures::reset(); }
	static void activate1987()
	{
		ModEngineFeatures::reset();
		std::string err;
		REQUIRE_MESSAGE(ModEngineFeatures::activate({ "f222b925edf2a9553e574ad8445f2841" }, &err), err);
	}
	static void activate1985()
	{
		ModEngineFeatures::reset();
		std::string err;
		REQUIRE_MESSAGE(ModEngineFeatures::activate({ "fc32e4c4142b5c607b90972eaf448400" }, &err), err);
	}
};

void stepFrames(GameLogic &logic, int n)
{
	for (int i = 0; i < n; ++i)
	{
		logic.runLogicFrame();
	}
}
} // namespace

TEST_CASE("LOCAL-145: Ennorath 1.9.8.7 profile activates the 7 new features; 1.9.8.5 and retail do not")
{
	// Source: ModEngineFeatures.cpp (f222b925edf2a9553e574ad8445f2841 vs fc32e4c4142b5c607b90972eaf448400)
	ModGuard guard;
	CHECK_FALSE(ModEngineFeatures::has(ModEngineFeature::EnnorathOptionHotkeys1987));
	CHECK_FALSE(ModEngineFeatures::has(ModEngineFeature::EnnorathBuilderFixes));
	CHECK_FALSE(ModEngineFeatures::has(ModEngineFeature::EnnorathPortraitUpgrade));
	CHECK_FALSE(ModEngineFeatures::has(ModEngineFeature::EnnorathFilteredHordeRespawn));
	CHECK_FALSE(ModEngineFeatures::has(ModEngineFeature::EnnorathCantEarnPowerPoints));
	CHECK_FALSE(ModEngineFeatures::has(ModEngineFeature::EnnorathToggleMountedZeroSpeed));
	CHECK_FALSE(ModEngineFeatures::has(ModEngineFeature::EnnorathGarrisonHeroSurvive));

	ModGuard::activate1985();
	CHECK_FALSE(ModEngineFeatures::has(ModEngineFeature::EnnorathOptionHotkeys1987));
	CHECK_FALSE(ModEngineFeatures::has(ModEngineFeature::EnnorathBuilderFixes));
	CHECK_FALSE(ModEngineFeatures::has(ModEngineFeature::EnnorathPortraitUpgrade));
	CHECK_FALSE(ModEngineFeatures::has(ModEngineFeature::EnnorathFilteredHordeRespawn));
	CHECK_FALSE(ModEngineFeatures::has(ModEngineFeature::EnnorathCantEarnPowerPoints));
	CHECK_FALSE(ModEngineFeatures::has(ModEngineFeature::EnnorathToggleMountedZeroSpeed));
	CHECK_FALSE(ModEngineFeatures::has(ModEngineFeature::EnnorathGarrisonHeroSurvive));

	ModGuard::activate1987();
	CHECK(ModEngineFeatures::has(ModEngineFeature::EnnorathOptionHotkeys1987));
	CHECK(ModEngineFeatures::has(ModEngineFeature::EnnorathBuilderFixes));
	CHECK(ModEngineFeatures::has(ModEngineFeature::EnnorathPortraitUpgrade));
	CHECK(ModEngineFeatures::has(ModEngineFeature::EnnorathFilteredHordeRespawn));
	CHECK(ModEngineFeatures::has(ModEngineFeature::EnnorathCantEarnPowerPoints));
	CHECK(ModEngineFeatures::has(ModEngineFeature::EnnorathToggleMountedZeroSpeed));
	CHECK(ModEngineFeatures::has(ModEngineFeature::EnnorathGarrisonHeroSurvive));
}

TEST_CASE("LOCAL-145: ennorath.option-hotkeys-1987: 31-entry .wmkeys table (ETKeySelectAll, ETKeySelectAllHeroes, ETKeyTacticalMenu) and 0x5F / 0xAB dispatch")
{
	// Source: PYSAGE 1.9.8.7 .wmkeys 0xF3B1D4 / 0xF3B1F8 / 0xF3B21C, .hotkeys 0xF38490 / 0xF38510 (hook RW 0x81F983)
	ModGuard guard;

	// 1.9.8.5 profile: 28 entries; 1.9.8.7 hotkey gadgets not exposed
	ModGuard::activate1985();
	CHECK(EnnorathOptions::activeEntryCount() == 28);
	CHECK(EnnorathOptions::findEntryByGadget("ETKeySelectAll") == -1);
	CHECK(EnnorathOptions::findEntryByGadget("ETKeySelectAllHeroes") == -1);
	CHECK(EnnorathOptions::findEntryByGadget("ETKeyTacticalMenu") == -1);

	// 1.9.8.7 profile: 31 entries; all 3 new hotkeys load, save, and apply to MetaMap
	ModGuard::activate1987();
	CHECK(EnnorathOptions::activeEntryCount() == 31);
	CHECK(EnnorathOptions::findEntryByGadget("ETKeySelectAll") == EnnorathOptions::kEntrySelectAll);
	CHECK(EnnorathOptions::findEntryByGadget("ETKeySelectAllHeroes") == EnnorathOptions::kEntrySelectAllHeroes);
	CHECK(EnnorathOptions::findEntryByGadget("ETKeyTacticalMenu") == EnnorathOptions::kEntryTacticalMenu);

	OptionPreferences prefs;
	prefs.set("ETKeySelectAll", "1");        // A
	prefs.set("ETKeySelectAllHeroes", "8");  // H
	prefs.set("ETKeyTacticalMenu", "16");    // P

	MetaMap map;
	applyEnnorathHotkeys(map, &prefs);
	const MetaMapRec *rAll = map.find(CMSG_META_SELECT_ALL);
	const MetaMapRec *rHeroes = map.find(CMSG_META_SELECT_ALL_HEROES);
	const MetaMapRec *rPlan = map.find(CMSG_META_TOGGLE_PLANNING_MODE);
	REQUIRE(rAll != nullptr);
	REQUIRE(rHeroes != nullptr);
	REQUIRE(rPlan != nullptr);
	CHECK(rAll->key == KEY_A);
	CHECK(rHeroes->key == KEY_H);
	CHECK(rPlan->key == KEY_P);

	// CommandTranslator dispatch of CMSG_META_SELECT_NEXT_WORKER (0x5F) and CMSG_META_SELECT_ALL_HEROES (0xAB)
	synthetichud::Rig rig;
	int builderCalls = 0, heroCalls = 0;
	rig.input->context().selectNearestBuilder = [&](bool) { ++builderCalls; };
	rig.input->context().selectAllHeroes = [&]() { ++heroCalls; };
	CommandTranslator xlat(rig.input->context());

	ClientMessage mBuilder(CMSG_META_SELECT_NEXT_WORKER);
	ClientMessage mHeroes(CMSG_META_SELECT_ALL_HEROES);
	CHECK(xlat.translate(mBuilder) == MessageDisposition::Destroy);
	CHECK(xlat.translate(mHeroes) == MessageDisposition::Destroy);
	CHECK(builderCalls == 1);
	CHECK(heroCalls == 1);

	// Off profile (retail 2.01): both remain retail no-ops (Keep)
	ModEngineFeatures::reset();
	CHECK(xlat.translate(mBuilder) == MessageDisposition::Keep);
	CHECK(xlat.translate(mHeroes) == MessageDisposition::Keep);
	CHECK(builderCalls == 1);
	CHECK(heroCalls == 1);
}

TEST_CASE("LOCAL-145: ennorath.builder-fixes: builder click sound, non-DOZER_CONSTRUCT worker exclusion, on-screen camera check, and >2 builders")
{
	// Source: PYSAGE 1.9.8.7 .hotkeys 0xF384D6 (hook RW 0x92DB1F -> RW 0x815772 "Gui_PalantirCommandButtonClick"),
	// .wmfeat 0xF3AA56..0xF3AAB8 (CommandSet has GUI_COMMAND_DOZER_CONSTRUCT 0x35), .camfix 0xF37060 (hook RW 0x69E131)
	ModGuard guard;

	combattest::CombatWorld c;
	DozerAIUpdate::registerClass(c.w.modules);
	CommandStore cmdStore;
	CommandStore *savedStore = TheCommandStore;
	TheCommandStore = &cmdStore;
	c.w.fx.env.blocks.registerBlock("CommandButton", [](INI *ini) { CommandStore::parseCommandButtonDefinitionGlobal(ini); });
	c.w.fx.env.blocks.registerBlock("CommandSet", [](INI *ini) { CommandStore::parseCommandSetDefinitionGlobal(ini); });
	REQUIRE(c.w.load(
		"CommandButton Cmd_BuildBarracks\n"
		"  Command = DOZER_CONSTRUCT\n"
		"End\n"
		"CommandButton Cmd_StopWorker\n"
		"  Command = STOP\n"
		"End\n"
		"CommandSet RealBuilderCS\n"
		"  1 = Cmd_BuildBarracks\n"
		"End\n"
		"CommandSet MillWorkerCS\n"
		"  1 = Cmd_StopWorker\n"
		"End\n"
		"Object RealBuilder\n"
		"  KindOf = PRELOAD SELECTABLE CAN_CAST_REFLECTIONS INFANTRY DOZER PORTER\n"
		"  ButtonImage = BGBarracks\n"
		"  CommandSet = RealBuilderCS\n"
		"  Geometry = CYLINDER\n  GeometryMajorRadius = 5\n  GeometryMinorRadius = 5\n  GeometryHeight = 10\n"
		"  Body = ActiveBody BodyTag\n    MaxHealth = 100\n  End\n"
		"  Behavior = DozerAIUpdate AiTag\n  End\n"
		"End\n"
		"Object MillWorker\n"
		"  KindOf = PRELOAD SELECTABLE CAN_CAST_REFLECTIONS INFANTRY DOZER PORTER\n"
		"  ButtonImage = BGBarracks\n"
		"  CommandSet = MillWorkerCS\n"
		"  Geometry = CYLINDER\n  GeometryMajorRadius = 5\n  GeometryMinorRadius = 5\n  GeometryHeight = 10\n"
		"  Body = ActiveBody BodyTag\n    MaxHealth = 100\n  End\n"
		"  Behavior = DozerAIUpdate AiTag\n  End\n"
		"End\n") == "");

	c.players.setLocalPlayer(c.players.findPlayerWithName("Bob"));
	Object *b1 = c.unit("RealBuilder", 'B', 100.0f, 100.0f);
	Object *b2 = c.unit("RealBuilder", 'B', 120.0f, 100.0f);
	Object *b3 = c.unit("RealBuilder", 'B', 1900.0f, 1900.0f); // off screen (view is 1024x768)
	Object *worker = c.unit("MillWorker", 'B', 110.0f, 100.0f);
	REQUIRE(b1 != nullptr);
	REQUIRE(b2 != nullptr);
	REQUIRE(b3 != nullptr);
	REQUIRE(worker != nullptr);

	synthetichud::PixelView view;
	InGameUI ui;
	MessageStream stream;
	HudContext ctx{ *c.logic, c.ai.get(), view, ui, stream, &cmdStore, MouseSettings{}, {}, {} };

	// Retail 2.01: MillWorker passes isReadyBuilder; selectNearestBuilder emits no UI click sound
	ModEngineFeatures::reset();
	CHECK(InGameHeroSelect::isReadyBuilder(ctx, *worker));
	CHECK(InGameHeroSelect::isReadyBuilder(ctx, *b1));
	{
		const std::uint64_t before = AudioApi::callsWithoutAudio();
		InGameHeroSelect hs(ctx, 5000);
		hs.trackObjects();
		hs.selectNearestBuilder(false);
		CHECK(AudioApi::callsWithoutAudio() == before);
	}

	// Ennorath 1.9.8.7: MillWorker excluded; all 3 builders cycle (>2 builders); click sound plays; camera jumps only when off screen
	ModGuard::activate1987();
	CHECK_FALSE(InGameHeroSelect::isReadyBuilder(ctx, *worker));
	CHECK(InGameHeroSelect::isReadyBuilder(ctx, *b1));

	ui.deselectAll(false);
	view.lookAt(Coord3D{ 100.0f, 100.0f, 0.0f });
	const Coord3D startCam = view.position();

	const std::uint64_t soundsBefore = AudioApi::callsWithoutAudio();
	InGameHeroSelect hs(ctx, 5000);
	hs.trackObjects();
	std::vector<ObjectID> picked;
	for (int i = 0; i < 3; ++i)
	{
		hs.selectNearestBuilder(false);
		REQUIRE(ui.selected().size() == 1);
		picked.push_back(ui.selected().front());
		if (picked.back() == b1->getID() || picked.back() == b2->getID())
		{
			CHECK(view.position().x == startCam.x);
			CHECK(view.position().y == startCam.y);
		}
	}
	CHECK(std::find(picked.begin(), picked.end(), b1->getID()) != picked.end());
	CHECK(std::find(picked.begin(), picked.end(), b2->getID()) != picked.end());
	CHECK(std::find(picked.begin(), picked.end(), b3->getID()) != picked.end());
	CHECK(std::find(picked.begin(), picked.end(), worker->getID()) == picked.end());
	CHECK(view.position().x == 1900.0f);
	CHECK(view.position().y == 1900.0f);
	CHECK(AudioApi::callsWithoutAudio() - soundsBefore == 3);
	TheCommandStore = savedStore;
}

TEST_CASE("LOCAL-145: ennorath.portrait-upgrade: PortraitUpgrade changes Portrait and ButtonImage on upgrade")
{
	// Source: PYSAGE 1.9.8.7 .wmfeat 0xF3A000..0xF3A1E6 (hooks RW 0x65A93F, 0x694F06, 0x92D052)
	ModGuard guard;
	const char kIni[] =
		"Upgrade Upgrade_NewLook\n"
		"  Type = OBJECT\n"
		"End\n"
		"Object PortraitHero\n"
		"  KindOf = PRELOAD SELECTABLE HERO\n"
		"  SelectPortrait = OrigPortrait\n"
		"  ButtonImage = OrigButton\n"
		"  Geometry = CYLINDER\n  GeometryMajorRadius = 5\n  GeometryMinorRadius = 5\n  GeometryHeight = 10\n"
		"  Body = ActiveBody BodyTag\n    MaxHealth = 100\n  End\n"
		"  Behavior = PortraitUpgrade ModuleTag_Port\n"
		"    TriggeredBy = Upgrade_NewLook\n"
		"    Portrait = UpgradedPortrait\n"
		"    ButtonImage = UpgradedButton\n"
		"  End\n"
		"End\n";

	// Off profile: PortraitUpgrade is not registered
	ModEngineFeatures::reset();
	{
		combattest::CombatWorld c;
		CHECK(c.w.modules.findModuleTemplate("PortraitUpgrade", MODULETYPE_BEHAVIOR) == nullptr);
	}

	// On 1.9.8.7 profile: before upgrade -> original; after upgrade -> UpgradedPortrait / UpgradedButton
	ModGuard::activate1987();
	combattest::CombatWorld c(kIni);
	Object *hero = c.unit("PortraitHero", 'B', 100, 100);
	REQUIRE(hero != nullptr);
	CHECK(PalantirCommandUI::objectPortrait(*hero) == "OrigPortrait");
	CHECK(InGameHeroSelect::buttonImage(*hero) == "OrigButton");

	const UpgradeTemplate *u = TheUpgradeCenter->findUpgrade("Upgrade_NewLook");
	REQUIRE(u != nullptr);
	hero->giveUpgrade(u);
	CHECK(PalantirCommandUI::objectPortrait(*hero) == "UpgradedPortrait");
	CHECK(InGameHeroSelect::buttonImage(*hero) == "UpgradedButton");
}

TEST_CASE("LOCAL-145: ennorath.filtered-horde-respawn: FilteredHordeRespawnBehavior respawns missing member on matching allied HORDE")
{
	// Source: PYSAGE 1.9.8.7 .wmfeat 0xF3A000, 0xF3A20D..0xF3A349 (hook RW 0x888012)
	ModGuard guard;

	// Off profile: FilteredHordeRespawnBehavior is not registered
	ModEngineFeatures::reset();
	{
		combattest::CombatWorld c;
		CHECK(c.w.modules.findModuleTemplate("FilteredHordeRespawnBehavior", MODULETYPE_BEHAVIOR) == nullptr);
	}

	const char kIni[] =
		"Object RespawnMember\n"
		"  KindOf = PRELOAD SELECTABLE INFANTRY\n"
		"  Geometry = CYLINDER\n  GeometryMajorRadius = 5\n  GeometryMinorRadius = 5\n  GeometryHeight = 10\n"
		"  Body = ActiveBody BodyTag\n    MaxHealth = 100\n  End\n"
		"  Behavior = AIUpdateInterface AiTag\n  End\n"
		"End\n"
		"Object CavMember\n"
		"  KindOf = PRELOAD SELECTABLE CAVALRY\n"
		"  Geometry = CYLINDER\n  GeometryMajorRadius = 5\n  GeometryMinorRadius = 5\n  GeometryHeight = 10\n"
		"  Body = ActiveBody BodyTag\n    MaxHealth = 100\n  End\n"
		"  Behavior = AIUpdateInterface AiTag\n  End\n"
		"End\n"
		"Object RespawnHorde\n"
		"  KindOf = PRELOAD SELECTABLE HORDE INFANTRY\n"
		"  Geometry = BOX\n  GeometryMajorRadius = 20\n  GeometryMinorRadius = 20\n  GeometryHeight = 10\n"
		"  Body = ImmortalBody BodyTag\n    MaxHealth = 1\n  End\n"
		"  Behavior = AIUpdateInterface AiTag\n  End\n"
		"  Behavior = HordeContain ModuleTag_Horde\n"
		"    ObjectStatusOfContained =\n"
		"    InitialPayload = RespawnMember 1\n"
		"    Slots = 2\n"
		"    PassengerFilter = NONE +INFANTRY\n"
		"    RankInfo = RankNumber:1 UnitType:RespawnMember Position:X:0 Y:0 Position:X:10 Y:0\n"
		"  End\n"
		"End\n"
		"Object CavHorde\n"
		"  KindOf = PRELOAD SELECTABLE HORDE CAVALRY\n"
		"  Geometry = BOX\n  GeometryMajorRadius = 20\n  GeometryMinorRadius = 20\n  GeometryHeight = 10\n"
		"  Body = ImmortalBody BodyTag\n    MaxHealth = 1\n  End\n"
		"  Behavior = AIUpdateInterface AiTag\n  End\n"
		"  Behavior = HordeContain ModuleTag_Horde\n"
		"    ObjectStatusOfContained =\n"
		"    InitialPayload = CavMember 1\n"
		"    Slots = 2\n"
		"    PassengerFilter = NONE +CAVALRY\n"
		"    RankInfo = RankNumber:1 UnitType:CavMember Position:X:0 Y:0 Position:X:10 Y:0\n"
		"  End\n"
		"End\n"
		"Object RespawnShrine\n"
		"  KindOf = PRELOAD STRUCTURE\n"
		"  Geometry = CYLINDER\n  GeometryMajorRadius = 10\n  GeometryMinorRadius = 10\n  GeometryHeight = 20\n"
		"  Body = ActiveBody BodyTag\n    MaxHealth = 500\n  End\n"
		"  Behavior = FilteredHordeRespawnBehavior ModuleTag_Respawn\n"
		"    Radius = 200\n"
		"    RespawnDelay = 200\n"
		"    ObjectFilter = ANY +INFANTRY\n"
		"    RespawnFXList = FX_TestRespawn\n"
		"  End\n"
		"End\n";

	ModGuard::activate1987();
	combattest::CombatWorld c(kIni);
	Object *horde = c.unit("RespawnHorde", 'B', 120, 100);
	Object *cavHorde = c.unit("CavHorde", 'B', 140, 100);
	REQUIRE(horde != nullptr);
	REQUIRE(cavHorde != nullptr);
	HordeContain *hc = dynamic_cast<HordeContain *>(horde->getContain());
	HordeContain *cc = dynamic_cast<HordeContain *>(cavHorde->getContain());
	REQUIRE(hc != nullptr);
	REQUIRE(cc != nullptr);
	REQUIRE(hc->payloadCount() == 1);
	REQUIRE(cc->payloadCount() == 1);

	Object *shrine = c.unit("RespawnShrine", 'B', 100, 100);
	REQUIRE(shrine != nullptr);
	stepFrames(*c.logic, 5);
	CHECK(hc->payloadCount() == 2);
	CHECK(cc->payloadCount() == 1); // filtered out by ObjectFilter = ANY +INFANTRY
	const auto *beh = dynamic_cast<const FilteredHordeRespawnBehavior *>(shrine->findModule("FilteredHordeRespawnBehavior"));
	REQUIRE(beh != nullptr);
	CHECK(beh->respawns() == 1);
}

TEST_CASE("LOCAL-145: ennorath.cant-earn-powerpoints: KindOf CANT_EARN_POWERPOINTS blocks kill and loss skill points")
{
	// Source: PYSAGE 1.9.8.7 .wmfeat 0xF3A5C0 (bit 222), 0xF3A34C..0xF3A41F (hooks RW 0x6AAFC3, 0x6AB0D0, 0x853FA0, 0x8FE8C6)
	ModGuard guard;

	ModEngineFeatures::reset();
	CHECK(KindOfTokens::indexOf("CANT_EARN_POWERPOINTS") == -1);

	ModGuard::activate1987();
	CHECK(KindOfTokens::indexOf("CANT_EARN_POWERPOINTS") == 222);

	const char kIni[] =
		"Object NoPpUnit\n"
		"  KindOf = PRELOAD SELECTABLE CAN_ATTACK INFANTRY CANT_EARN_POWERPOINTS\n"
		"  Geometry = CYLINDER\n  GeometryMajorRadius = 5\n  GeometryMinorRadius = 5\n  GeometryHeight = 10\n"
		"  Body = ActiveBody BodyTag\n    MaxHealth = 100\n  End\n"
		"End\n"
		"Object NormalUnit\n"
		"  KindOf = PRELOAD SELECTABLE CAN_ATTACK INFANTRY\n"
		"  Geometry = CYLINDER\n  GeometryMajorRadius = 5\n  GeometryMinorRadius = 5\n  GeometryHeight = 10\n"
		"  Body = ActiveBody BodyTag\n    MaxHealth = 100\n  End\n"
		"End\n";

	combattest::CombatWorld c(kIni);
	Object *noPp = c.unit("NoPpUnit", 'B', 100, 100);
	Object *enemy = c.unit("NormalUnit", 'R', 120, 100);
	REQUIRE(noPp != nullptr);
	REQUIRE(enemy != nullptr);
	CHECK(ExperienceWorld::cantEarnPowerPoints(noPp));
	CHECK_FALSE(ExperienceWorld::cantEarnPowerPoints(enemy));

	CHECK_FALSE(c.logic->experience().awardSkillPointsForDamage(*noPp, *enemy, 1.0f));
	Player *pB = noPp->getControllingPlayer();
	REQUIRE(pB != nullptr);
	CHECK_FALSE(c.logic->experience().awardSkillPointsForLoss(*pB, enemy, *noPp));
}

TEST_CASE("LOCAL-145: ennorath.toggle-mounted-zero-speed: ToggleMountedSpecialAbilityUpdate fires when LocomotorSet Speed == 0")
{
	// Source: PYSAGE 1.9.8.7 .wmfeat 0xF3A940 (hook RW 0x8B1FFE in RW 0x8B1FDA)
	ModGuard guard;
	const char kIni[] =
		"SpecialPower SP_MountZero\n"
		"  Enum = SPECIAL_TOGGLE_MOUNTED\n"
		"  ReloadTime = 1000\n"
		"End\n"
		"Object ZeroSpeedMountUnit\n"
		"  KindOf = PRELOAD SELECTABLE HERO\n"
		"  Geometry = CYLINDER\n  GeometryMajorRadius = 5\n  GeometryMinorRadius = 5\n  GeometryHeight = 10\n"
		"  Body = ActiveBody BodyTag\n    MaxHealth = 100\n  End\n"
		"  Behavior = AIUpdateInterface AiTag\n  End\n"
		"  LocomotorSet\n    Locomotor = WalkerLoco\n    Condition = SET_NORMAL\n    Speed = 0\n  End\n"
		"  Behavior = SpecialPowerModule ModuleTag_SP\n"
		"    SpecialPowerTemplate = SP_MountZero\n"
		"  End\n"
		"  Behavior = ToggleMountedSpecialAbilityUpdate ModuleTag_Mount\n"
		"    SpecialPowerTemplate = SP_MountZero\n"
		"    MountedTemplate = ZeroSpeedMountUnit\n"
		"    TriggerAttributeModifier = None\n"
		"  End\n"
		"End\n";

	auto runOne = [&](bool expectActive) {
		combattest::CombatWorld c;
		SpecialPowerStore spStore;
		SpecialPowerStore *saved = TheSpecialPowerStore;
		TheSpecialPowerStore = &spStore;
		c.w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
		REQUIRE(c.w.load(kIni) == "");
		Object *u = c.unit("ZeroSpeedMountUnit", 'B', 100, 100);
		REQUIRE(u != nullptr);
		auto *mod = dynamic_cast<ToggleMountedSpecialAbilityUpdate *>(u->findModule("ToggleMountedSpecialAbilityUpdate"));
		REQUIRE(mod != nullptr);
		const SpecialPowerTemplate *sp = spStore.findSpecialPowerTemplate("SP_MountZero");
		REQUIRE(sp != nullptr);
		mod->initiateIntentToDoSpecialPower(sp, nullptr, nullptr, 0u, 0);
		CHECK(mod->isActive() == expectActive);
		TheSpecialPowerStore = saved;
	};

	ModEngineFeatures::reset();
	runOne(false);

	ModGuard::activate1987();
	runOne(true);
}

TEST_CASE("LOCAL-145: ennorath.garrison-hero-survive: alive non-horde HERO survives STRUCTURE GarrisonContain deletion")
{
	// Source: PYSAGE 1.9.8.7 .wmfeat 0xF3A95F / 0xF3A9B2 (hooks RW 0x87CF94, 0x8670D2)
	ModGuard guard;
	const char kIni[] =
		"Object GarrisonHero\n"
		"  KindOf = PRELOAD SELECTABLE HERO\n"
		"  Geometry = CYLINDER\n  GeometryMajorRadius = 5\n  GeometryMinorRadius = 5\n  GeometryHeight = 10\n"
		"  Body = ActiveBody BodyTag\n    MaxHealth = 100\n  End\n"
		"End\n"
		"Object GarrisonTower\n"
		"  KindOf = PRELOAD SELECTABLE STRUCTURE\n"
		"  Geometry = CYLINDER\n  GeometryMajorRadius = 20\n  GeometryMinorRadius = 20\n  GeometryHeight = 40\n"
		"  Body = ActiveBody BodyTag\n    MaxHealth = 500\n  End\n"
		"  Behavior = GarrisonContain ModuleTag_Contain\n"
		"    ContainMax = 2\n"
		"    AllowAlliesInside = Yes\n"
		"    EjectPassengersOnDeath = No\n"
		"    KillPassengersOnDeath = No\n"
		"  End\n"
		"End\n";

	ModEngineFeatures::reset();
	{
		combattest::CombatWorld c(kIni);
		Object *tower = c.unit("GarrisonTower", 'B', 100, 100);
		Object *hero = c.unit("GarrisonHero", 'B', 100, 100);
		REQUIRE(tower != nullptr);
		REQUIRE(hero != nullptr);
		tower->getContain()->addToContain(hero);
		REQUIRE(hero->getContainedBy() == tower);
		const ObjectID heroId = hero->getID();
		tower->kill(DAMAGE_UNRESISTABLE, 0);
		stepFrames(*c.logic, 1);
		CHECK(c.logic->findObjectByID(heroId) == nullptr);
	}

	ModGuard::activate1987();
	{
		combattest::CombatWorld c(kIni);
		Object *tower = c.unit("GarrisonTower", 'B', 100, 100);
		Object *hero = c.unit("GarrisonHero", 'B', 100, 100);
		REQUIRE(tower != nullptr);
		REQUIRE(hero != nullptr);
		tower->getContain()->addToContain(hero);
		REQUIRE(hero->getContainedBy() == tower);
		const ObjectID heroId = hero->getID();
		tower->kill(DAMAGE_UNRESISTABLE, 0);
		stepFrames(*c.logic, 1);
		Object *survived = c.logic->findObjectByID(heroId);
		REQUIRE(survived != nullptr);
		CHECK_FALSE(survived->isEffectivelyDead());
		CHECK(survived->getContainedBy() == nullptr);
	}
}

TEST_CASE("LOCAL-145: limit check: PlayerTemplateStore has no 14-faction cap (127+ factions supported natively)")
{
	// Source: PROCESS 15.16 limit check (PlayerTemplateStore::m_templates is an unbounded std::vector)
	combattest::CombatWorld c;
	const int baseCount = c.templates.getPlayerTemplateCount();
	std::string iniText;
	for (int i = 0; i < 130; ++i)
	{
		iniText += "PlayerTemplate FactionTest" + std::to_string(i) + "\n";
		iniText += "  Side = Side" + std::to_string(i) + "\n";
		iniText += "  PlayableSide = Yes\n";
		iniText += "End\n";
	}
	REQUIRE(c.w.load(iniText) == "");
	CHECK(c.templates.getPlayerTemplateCount() == baseCount + 130);
	CHECK(c.templates.findPlayerTemplate("FactionTest126") != nullptr);
}
