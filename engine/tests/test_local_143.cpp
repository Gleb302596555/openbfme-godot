// OpenBFME tests: LOCAL-143 (fork issue 860) — Ennorath 1.9.8.5 exe edits on the profile (--mod-engine=fc32e4c4142b5c607b90972eaf448400).
// Expectations come from PYSAGE 1.9.8.5 sections:
//   .obscmd 0xED8000 (hook RW 0x941BD2),
//   .cmdext 0xED6000 (hook RW 0x943E11),
//   .upg5k 0xEDF000 (mask limit 0x480 -> 0x1400, RW 0x8D2835),
//   .bldcap 0xF21000 (wrapper on RW 0x793D12, Team::countObjectsByThingTemplate RW 0x7A047A),
//   .spstore 0xED4000 (hooks RW 0x822890, RW 0x8231CA),
//   .bldgrp 0xEDA000 / .padsel 0xEDE000 / .bldext 0xEDD000 / .bldcan 0xEDC000 / .bldlvl 0xF36000
//     (hooks RW 0x625696, RW 0x71ED96, RW 0x6A286A, RW 0x6A2955, RW 0x930E21, RW 0x940975, RW 0x941BFC, RW 0x71DAB7, RW 0x940435, RW 0x942733, RW 0x944834, RW 0x9448EB).
// Off the profile, retail 2.01 behaviour holds. GPL-3.0.

#include "doctest.h"
#include "EconTestUtil.h"
#include "SyntheticHudTestUtil.h"
#include "AptTestUtil.h"

#include "Common/BuildAssistant.h"
#include "Common/ModEngineFeatures.h"
#include "Common/Player.h"
#include "Common/PlayerScience.h"
#include "Common/PlayerTemplate.h"
#include "GameLogic/ObjectTemplateInfo.h"
#include "Common/Thing/ThingFactory.h"
#include "Common/Upgrade.h"
#include "GameClient/ControlBar.h"
#include "GameClient/ControlBarCommands.h"
#include "GameClient/GUI/AptScreens/AptSpellStore.h"
#include "GameClient/GUI/Shell/Shell.h"
#include "GameClient/GUI/AptScreens/AptScreenFactories.h"
#include "GameClient/GUI/ShellEnvironment.h"
#include "GameClient/GUI/ShellServices.h"
#include "GameClient/GUI/WindowManager.h"
#include "GameClient/InGameUI.h"
#include "GameLogic/Module/LogicModules.h"
#include "GameLogic/Object/Object.h"

#include <string>
#include <vector>

namespace
{

constexpr const char *kEnnorathMd5 = "fc32e4c4142b5c607b90972eaf448400";

struct ModEngineGuard
{
	~ModEngineGuard() { ModEngineFeatures::reset(); }
};

struct CommandStoreGuard
{
	CommandStore *saved = nullptr;
	explicit CommandStoreGuard(CommandStore &s) : saved(TheCommandStore) { TheCommandStore = &s; }
	~CommandStoreGuard() { TheCommandStore = saved; }
};



} // namespace

// Expectations: PYSAGE 1.9.8.5 .cmdext 0xED6000 (hook RW 0x943E11) raises CommandSet capacity from 33 to 64 slots.
TEST_CASE("LOCAL-143: ennorath.commandset-64 (.cmdext): 64 CommandSet slots on Ennorath 1.9.8.5 profile, 33 off")
{
	ModEngineGuard guard;
	for (const bool ennorath : { false, true })
	{
		CAPTURE(ennorath);
		std::string err;
		if (ennorath)
		{
			REQUIRE_MESSAGE(ModEngineFeatures::activate({ kEnnorathMd5 }, &err), err);
		}
		else
		{
			ModEngineFeatures::reset();
		}
		CHECK(CommandSet::slotCapacity() == (ennorath ? 64 : 33));
		econtest::Fx f("Object Dummy\n  KindOf = STRUCTURE\nEnd\n");
		CommandStore store;
		CommandStoreGuard csg(store);
		f.w.fx.env.blocks.registerBlock("CommandButton", [](INI *ini) { CommandStore::parseCommandButtonDefinitionGlobal(ini); });
		f.w.fx.env.blocks.registerBlock("CommandSet", [](INI *ini) { CommandStore::parseCommandSetDefinitionGlobal(ini); });
		REQUIRE(f.w.load("CommandButton Btn_Ext\n  Command = STOP\nEnd\n") == "");
		const std::string res = f.w.load("CommandSet ExtSet\n  1 = Btn_Ext\n  64 = Btn_Ext\nEnd\n");
		if (ennorath)
		{
			REQUIRE(res.empty());
			const CommandSet *cs = store.findCommandSet("ExtSet");
			REQUIRE(cs != nullptr);
			CHECK(cs->getCommandButton(0) != nullptr);
			CHECK(cs->getCommandButton(63) != nullptr);
		}
		else
		{
			CHECK_FALSE(res.empty());
		}
	}
}

// Expectations: PYSAGE 1.9.8.5 .upg5k 0xEDF000 raises upgrade mask limit to 5120 (0x1400 bits); without it S-5303 caps at 2048.
TEST_CASE("LOCAL-143: ennorath.upgrades-5120 (.upg5k): 5120 upgrades with profile on, 2048 cap off")
{
	ModEngineGuard guard;
	for (const bool ennorath : { false, true })
	{
		CAPTURE(ennorath);
		std::string err;
		if (ennorath)
		{
			REQUIRE_MESSAGE(ModEngineFeatures::activate({ kEnnorathMd5 }, &err), err);
		}
		else
		{
			ModEngineFeatures::reset();
		}
		econtest::Fx f("Object Dummy\n  KindOf = STRUCTURE\nEnd\n");
		std::string ini;
		// 3 veterancy upgrades already take bits 0..2; 2100 more exceeds 2048 (off) and fits in 5120 (on).
		for (int i = 0; i < 2100; ++i)
		{
			ini += "Upgrade Upg5k_" + std::to_string(i) + "\nEnd\n";
		}
		const std::string res = f.w.load(ini);
		if (!ennorath)
		{
			CHECK_FALSE(res.empty());
		}
		else
		{
			REQUIRE_MESSAGE(res.empty(), res);
			const UpgradeTemplate *last = TheUpgradeCenter->findUpgrade("Upg5k_2099");
			REQUIRE(last != nullptr);
			CHECK(last->getMaskBit() == 2108);
			CHECK(UpgradeMaskType::activeWords() == 66);
			UpgradeMaskType m;
			m.set((unsigned)last->getMaskBit());
			CHECK(m.test((unsigned)last->getMaskBit()));
			CHECK(m.testForAny(last->grantMask()));
		}
	}
}

// Expectations: PYSAGE 1.9.8.5 .bldcap 0xF21000 wraps RW 0x793D12 so effectively-dead objects with
// PENDING_CONSTRUCTION (0x57) + PHANTOM_STRUCTURE (0x58) count toward MaxSimultaneousOfType;
// retail RW 0x7A047A skips isEffectivelyDead(). Non-phantom effectively-dead objects never count.
TEST_CASE("LOCAL-143: ennorath.building-limit-fix (.bldcap): phantom structures count toward MaxSimultaneousOfType only with profile")
{
	ModEngineGuard guard;
	const char *ini =
		"Object LimitTower\n"
		"  KindOf = STRUCTURE SELECTABLE\n"
		"  MaxSimultaneousOfType = 1\n"
		"  BuildCost = 100\n"
		"  Body = ActiveBody B\n    MaxHealth = 100\n  End\n"
		"End\n"
		"Object Builder\n"
		"  KindOf = DOZER SELECTABLE\n"
		"  CommandSet = BuilderSet\n"
		"  Body = ActiveBody B\n    MaxHealth = 100\n  End\n"
		"End\n";
	const int kPending = ObjectTemplateInfoBuilder::objectStatusIndex("PENDING_CONSTRUCTION");
	const int kPhantom = ObjectTemplateInfoBuilder::objectStatusIndex("PHANTOM_STRUCTURE");
	REQUIRE(kPending == 87);
	REQUIRE(kPhantom == 88);

	for (const bool ennorath : { false, true })
	{
		CAPTURE(ennorath);
		std::string err;
		if (ennorath)
		{
			REQUIRE_MESSAGE(ModEngineFeatures::activate({ kEnnorathMd5 }, &err), err);
		}
		else
		{
			ModEngineFeatures::reset();
		}
		econtest::Fx f(ini);
		CommandStore store;
		CommandStoreGuard csg(store);
		store.setThingFactory(&f.logic->things());
		f.w.fx.env.blocks.registerBlock("CommandButton", [](INI *ini) { CommandStore::parseCommandButtonDefinitionGlobal(ini); });
		f.w.fx.env.blocks.registerBlock("CommandSet", [](INI *ini) { CommandStore::parseCommandSetDefinitionGlobal(ini); });
		REQUIRE(f.w.load(
			"CommandButton Btn_BuildTower\n  Command = DOZER_CONSTRUCT\n  Object = LimitTower\nEnd\n"
			"CommandSet BuilderSet\n  1 = Btn_BuildTower\nEnd\n") == "");
		Player *alice = f.player("Alice");
		REQUIRE(alice != nullptr);
		alice->getMoney()->deposit(1000);
		Object *builder = f.make("Builder", f.teamOf("Alice"));
		Object *phantom = f.make("LimitTower", f.teamOf("Alice"));
		REQUIRE(builder != nullptr);
		REQUIRE(phantom != nullptr);
		const ThingTemplate *towerTmpl = phantom->getTemplate();
		// Simulate DozerAIUpdate::construct phantom (effectively dead + PENDING_CONSTRUCTION + PHANTOM_STRUCTURE).
		phantom->friend_setEffectivelyDead(true);
		phantom->setStatus((unsigned)kPending, true);
		phantom->setStatus((unsigned)kPhantom, true);
		const CanMakeType res = BuildAssistant::canMakeUnit(*builder, towerTmpl, -1);
		if (ennorath)
		{
			CHECK(res == CANMAKE_MAXED_OUT_FOR_PLAYER);
		}
		else
		{
			CHECK(res == CANMAKE_OK);
		}
		// Neighbouring behaviour: a non-phantom effectively-dead tower is skipped even with the feature on.
		phantom->setStatus((unsigned)kPhantom, false);
		CHECK(BuildAssistant::canMakeUnit(*builder, towerTmpl, -1) == CANMAKE_OK);
	}
}

// Expectations: PYSAGE 1.9.8.5 .obscmd 0xED8000 (hook RW 0x941BD2): an observer may press
// PUSH_VISIBLE_COMMAND_RANGE (0x37) and POP_VISIBLE_COMMAND_RANGE (0x38), while other commands are blocked.
TEST_CASE("LOCAL-143: ennorath.observer-command-range (.obscmd): observer may push/pop command range only with profile")
{
	ModEngineGuard guard;
	const char *extra =
		"Object ObsTarget\n"
		"  KindOf = STRUCTURE SELECTABLE\n"
		"  CommandSet = ObsSet\n"
		"  Body = ActiveBody B\n    MaxHealth = 100\n  End\n"
		"End\n";
	const char *cmds =
		"CommandButton Btn_Push\n"
		"  Command = PUSH_VISIBLE_COMMAND_RANGE\n"
		"  CommandRangeStart = 12\n"
		"  CommandRangeCount = 6\n"
		"End\n"
		"CommandButton Btn_Pop\n"
		"  Command = POP_VISIBLE_COMMAND_RANGE\n"
		"End\n"
		"CommandButton Btn_Stop\n"
		"  Command = STOP\n"
		"End\n"
		"CommandSet ObsSet\n"
		"  1 = Btn_Push\n"
		"  2 = Btn_Stop\n"
		"  13 = Btn_Pop\n"
		"End\n";

	for (const bool ennorath : { false, true })
	{
		CAPTURE(ennorath);
		std::string err;
		if (ennorath)
		{
			REQUIRE_MESSAGE(ModEngineFeatures::activate({ kEnnorathMd5 }, &err), err);
		}
		else
		{
			ModEngineFeatures::reset();
		}
		synthetichud::Rig r;
		CommandStore store;
		CommandStoreGuard csg(store);
		store.setThingFactory(&r.logic->things());
		r.w.fx.env.blocks.registerBlock("CommandButton", [](INI *ini) { CommandStore::parseCommandButtonDefinitionGlobal(ini); });
		r.w.fx.env.blocks.registerBlock("CommandSet", [](INI *ini) { CommandStore::parseCommandSetDefinitionGlobal(ini); });
		REQUIRE(r.w.load(extra) == "");
		REQUIRE(r.w.load(cmds) == "");
		r.input->context().commands = &store;
		Player *alice = r.players.findPlayerWithName("Alice");
		REQUIRE(alice != nullptr);
		alice->setDefeated(true); // observer / inactive local player (RW 0x6A87F5)
		Object *b = r.place("ObsTarget", 200, 200, "Alice");
		r.input->context().ui.selectObject(b->getID());
		ControlBar bar(r.input->context());
		bar.update();
		// Slot 0 = Btn_Push (PUSH_VISIBLE_COMMAND_RANGE), Slot 1 = Btn_Stop (STOP).
		CHECK(bar.pressButton(0, false));
		if (ennorath)
		{
			// With observer-command-range enabled, non-range command (STOP) is refused for an observer.
			bar.update();
			CHECK(bar.pressButton(12, false)); // POP_VISIBLE_COMMAND_RANGE works
			bar.update();
			CHECK_FALSE(bar.pressButton(1, false)); // STOP blocked for observer
		}
		else
		{
			// Retail port has no observer refusal gate (S-290).
			bar.update();
			CHECK(bar.pressButton(12, false));
			bar.update();
			CHECK(bar.pressButton(1, false));
		}
	}
}

// Expectations: PYSAGE 1.9.8.5 .spstore 0xED4000 / 0xED4035 (hooks RW 0x822890, RW 0x8231CA):
// defers SetSpellButtonState updates for 500 ms (16 x 33 ms steps) after AptSpellStore::OnInitialized.
TEST_CASE("LOCAL-143: ennorath.spell-store-delay (.spstore): AptSpellStore defers button state updates 500 ms after OnInitialized")
{
	ModEngineGuard guard;
	for (const bool ennorath : { false, true })
	{
		CAPTURE(ennorath);
		std::string err;
		if (ennorath)
		{
			REQUIRE_MESSAGE(ModEngineFeatures::activate({ kEnnorathMd5 }, &err), err);
		}
		else
		{
			ModEngineFeatures::reset();
		}
		const char *tpls =
			"PlayerTemplate FactionMen\n"
			"  Side = Men\n"
			"  PurchaseScienceCommandSet = TestSpellStoreSet\n"
			"  PurchaseScienceCommandSetMP = TestSpellStoreSet\n"
			"End\n";
		econtest::Fx f("Object Dummy\n  KindOf = STRUCTURE\nEnd\n", tpls);
		CommandStore store;
		CommandStoreGuard csg(store);
		f.w.fx.env.blocks.registerBlock("CommandButton", [](INI *ini) { CommandStore::parseCommandButtonDefinitionGlobal(ini); });
		f.w.fx.env.blocks.registerBlock("CommandSet", [](INI *ini) { CommandStore::parseCommandSetDefinitionGlobal(ini); });
		REQUIRE(f.w.load(
			"CommandButton Btn_Sp1\n  Command = PURCHASE_SCIENCE\n  Science = SCIENCE_Sp1\nEnd\n"
			"CommandSet TestSpellStoreSet\n  1 = Btn_Sp1\nEnd\n") == "");
		Player *alice = f.player("Alice");
		REQUIRE(alice != nullptr);
		const PlayerTemplate *pt = f.templates.findPlayerTemplate("FactionMen");
		REQUIRE(pt != nullptr);
		alice->init(pt, 0);
		struct NullAptSource : AptFileSource
		{
			bool readFile(const std::string &, std::vector<std::uint8_t> &, std::string *e) override { if (e) *e = "none"; return false; }
			bool fileExists(const std::string &) override { return false; }
			std::vector<std::string> listMovies() override { return {}; }
		} src;
		struct NullShellServices : ShellServices
		{
			void playSound(const std::string &) override {}
			void setBackground(const std::string &) override {}
			void setMouseVisible(bool) override {}
			void setCursorTooltip(const std::string &) override {}
			void request(const ShellRequest &) override {}
			void applyOption(const std::string &, const std::string &) override {}
			std::uint32_t addMiscAudioEvent(const std::string &, int) override { return 0; }
			void removeAudioEvent(std::uint32_t) override {}
			bool isCurrentlyPlaying(std::uint32_t) override { return false; }
			void playLoadScreenMusic(const std::string &) override {}
			void setDisplayGamma(float) override {}
		} svc;
		ShellEnvironment env;
		AptScreenFactoryTable factories;
		WindowManager wm(src, svc);
		Shell shell(wm, factories, svc, env);
		AptSpellStore ss(wm, shell);
		std::string bindErr;
		REQUIRE_MESSAGE(ss.bind(store, *f.logic, *alice, nullptr, &bindErr), bindErr);
		wm.fscommand("AptSpellStore::OnInitialized", "");
		// First update: switches layout from 0 to multiplayer (or runs directly if layout == 0).
		// Call update() once for layout, then check button state 0.
		ss.update();
		ss.update();
		if (!ennorath)
		{
			// Retail: button state is populated immediately (no 500 ms delay).
			CHECK(ss.buttonState(0) != -1);
		}
		else
		{
			// Ennorath .spstore: button states remain -1 during the 500 ms window (< 16 steps).
			CHECK(ss.buttonState(0) == -1);
			for (int i = 0; i < 16; ++i)
			{
				ss.update();
			}
			// After 500 ms (>= 16 steps of 33 ms), button states are updated.
			CHECK(ss.buttonState(0) != -1);
			// Re-initializing resets the 500 ms timer and clears cached states to -1.
			wm.fscommand("AptSpellStore::OnInitialized", "");
			ss.update();
			CHECK(ss.buttonState(0) == -1);
		}
	}
}

// Expectations: PYSAGE 1.9.8.5 .bldgrp 0xEDA000, .padsel 0xEDE000, .bldext 0xEDD000, .bldcan 0xEDC000, .bldlvl 0xF36000:
// selectMatching selects same-type structures and pairs <Prefix>ExpansionPadSide + Corner;
// ControlBar shows single-building context for a same-type building group, queues/cancels UNIT_BUILD across all active buildings,
// builds FOUNDATION_CONSTRUCT across all pads, and surfaces/applies level-2/3 OBJECT_UPGRADE across selected buildings.
TEST_CASE("LOCAL-143: ennorath.select-all-buildings (.bldgrp/.padsel/.bldext/.bldcan/.bldlvl): selection, queue/cancel, pads, and level upgrades")
{
	ModEngineGuard guard;
	const char *extra =
		"Upgrade Upgrade_BarracksLevel2\n"
		"  Type = OBJECT\n"
		"  BuildCost = 100\n"
		"End\n"
		"Object TestSoldier\n"
		"  KindOf = INFANTRY SELECTABLE\n"
		"  BuildCost = 50\n"
		"  BuildTime = 5\n"
		"End\n"
		"Object TestBarracks\n"
		"  KindOf = STRUCTURE SELECTABLE\n"
		"  CommandSet = BarracksSet\n"
		"  Behavior = ProductionUpdate P\n  End\n"
		"  Behavior = StatusBitsUpgrade L2\n    TriggeredBy = Upgrade_BarracksLevel2\n  End\n"
		"End\n"
		"Object TestTowerExp\n"
		"  KindOf = STRUCTURE SELECTABLE\n"
		"  BuildCost = 100\n"
		"End\n"
		"Object GondorExpansionPadSide\n"
		"  KindOf = STRUCTURE SELECTABLE BASE_FOUNDATION EXPANSION_PAD\n"
		"  CommandSet = PadSet\n"
		"End\n"
		"Object GondorExpansionPadCorner\n"
		"  KindOf = STRUCTURE SELECTABLE BASE_FOUNDATION EXPANSION_PAD\n"
		"  CommandSet = PadSet\n"
		"End\n"
		"Object MordorExpansionPadSide\n"
		"  KindOf = STRUCTURE SELECTABLE BASE_FOUNDATION EXPANSION_PAD\n"
		"  CommandSet = PadSet\n"
		"End\n";
	const char *cmds =
		"CommandButton Btn_Train\n"
		"  Command = UNIT_BUILD\n"
		"  Object = TestSoldier\n"
		"  Options = CANCELABLE\n"
		"End\n"
		"CommandButton Btn_Lvl2\n"
		"  Command = OBJECT_UPGRADE\n"
		"  Upgrade = Upgrade_BarracksLevel2\n"
		"End\n"
		"CommandButton Btn_BuildExp\n"
		"  Command = FOUNDATION_CONSTRUCT\n"
		"  Object = TestTowerExp\n"
		"End\n"
		"CommandSet BarracksSet\n"
		"  1 = Btn_Train\n"
		"  2 = Btn_Lvl2\n"
		"End\n"
		"CommandSet PadSet\n"
		"  1 = Btn_BuildExp\n"
		"End\n";

	for (const bool ennorath : { false, true })
	{
		CAPTURE(ennorath);
		std::string err;
		if (ennorath)
		{
			REQUIRE_MESSAGE(ModEngineFeatures::activate({ kEnnorathMd5 }, &err), err);
		}
		else
		{
			ModEngineFeatures::reset();
		}
		synthetichud::Rig r;
		LogicModules::registerAll(r.w.modules);
		CommandStore store;
		CommandStoreGuard csg(store);
		store.setThingFactory(&r.logic->things());
		r.w.fx.env.blocks.registerBlock("CommandButton", [](INI *ini) { CommandStore::parseCommandButtonDefinitionGlobal(ini); });
		r.w.fx.env.blocks.registerBlock("CommandSet", [](INI *ini) { CommandStore::parseCommandSetDefinitionGlobal(ini); });
		REQUIRE(r.w.load(extra) == "");
		REQUIRE(r.w.load(cmds) == "");
		r.input->context().commands = &store;
		Player *alice = r.players.findPlayerWithName("Alice");
		REQUIRE(alice != nullptr);
		alice->getMoney()->deposit(5000);

		// 1. selectMatching for buildings + ExpansionPad Side/Corner pairing (.bldgrp / .padsel)
		Object *b1 = r.place("TestBarracks", 100, 100, "Alice");
		Object *b2 = r.place("TestBarracks", 200, 100, "Alice");
		Object *b3 = r.place("TestBarracks", 300, 100, "Alice");
		b3->setStatus(OBJECT_STATUS_UNDER_CONSTRUCTION, true);
		Object *padSide = r.place("GondorExpansionPadSide", 400, 100, "Alice");
		Object *padCorner = r.place("GondorExpansionPadCorner", 500, 100, "Alice");
		Object *padOther = r.place("MordorExpansionPadSide", 600, 100, "Alice");

		InGameUI &ui = r.input->context().ui;
		ui.deselectAll();
		ui.selectObject(b1->getID());
		const int addedB = ui.selectMatching(r.input->context(), true);
		if (!ennorath)
		{
			CHECK(addedB == 0);
			CHECK(ui.getSelectCount() == 1);
		}
		else
		{
			CHECK(addedB == 2);
			CHECK(ui.isSelected(b2->getID()));
			CHECK(ui.isSelected(b3->getID()));
		}

		// ExpansionPad Side + Corner pair, but different prefix (Mordor) is NOT selected.
		ui.deselectAll();
		ui.selectObject(padSide->getID());
		const int addedPad = ui.selectMatching(r.input->context(), true);
		if (!ennorath)
		{
			CHECK(addedPad == 0);
		}
		else
		{
			CHECK(addedPad == 1);
			CHECK(ui.isSelected(padCorner->getID()));
			CHECK_FALSE(ui.isSelected(padOther->getID()));
		}

		if (!ennorath)
		{
			continue;
		}

		// 2. ControlBar on same-type building group: single-building context, UNIT_BUILD queues on active buildings only (skips UNDER_CONSTRUCTION b3).
		ui.deselectAll();
		ui.selectObject(b1->getID());
		ui.selectObject(b2->getID());
		ui.selectObject(b3->getID());
		ControlBar bar(r.input->context());
		bar.update();
		r.input->context().stream.propagate();
		REQUIRE(bar.pressButton(0, false)); // Btn_Train
		int queueMsgs = 0;
		for (const ClientMessage &m : r.input->context().stream.propagate())
		{
			if (m.type() == MSG_QUEUE_UNIT_CREATE)
			{
				++queueMsgs;
			}
		}
		CHECK(queueMsgs == 2); // b1 and b2 only; b3 is UNDER_CONSTRUCTION

		// 3. Right-click cancelQueued (.bldcan 0xEDC024) cancels across both active buildings.
		CHECK(b1->getProductionUpdate() != nullptr);
		CHECK(b2->getProductionUpdate() != nullptr);
		r.input->context().stream.propagate();
		REQUIRE(bar.cancelQueued(0, false, false));
		int cancelMsgs = 0;
		for (const ClientMessage &m : r.input->context().stream.propagate())
		{
			if (m.type() == MSG_CANCEL_UNIT_CREATE)
			{
				++cancelMsgs;
			}
		}
		CHECK(cancelMsgs == 2);

		// 4. Level-2 OBJECT_UPGRADE (.bldlvl 0xF366D2): give b1 the upgrade already; clicking Btn_Lvl2 queues it only for b2 (not b1, not already-upgraded).
		const UpgradeTemplate *lvl2 = TheUpgradeCenter->findUpgrade("Upgrade_BarracksLevel2");
		REQUIRE(lvl2 != nullptr);
		b1->giveUpgrade(lvl2);
		b3->setStatus(OBJECT_STATUS_UNDER_CONSTRUCTION, false);
		b3->giveUpgrade(lvl2);
		bar.update();
		r.input->context().stream.propagate();
		REQUIRE(bar.pressButton(1, false)); // Btn_Lvl2
		std::vector<ObjectID> upgradedIds;
		for (const ClientMessage &m : r.input->context().stream.propagate())
		{
			if (m.type() == MSG_QUEUE_UPGRADE && m.argumentCount() >= 1)
			{
				upgradedIds.push_back(m.arg(0).objectID);
			}
		}
		REQUIRE(upgradedIds.size() == 1);
		CHECK(upgradedIds[0] == b2->getID());

		// 5. FOUNDATION_CONSTRUCT on ExpansionPad Side + Corner group (.bldext 0xEDD020).
		ui.deselectAll();
		ui.selectObject(padSide->getID());
		ui.selectObject(padCorner->getID());
		bar.update();
		r.input->context().stream.propagate();
		REQUIRE(bar.pressButton(0, false)); // Btn_BuildExp
		int foundMsgs = 0;
		for (const ClientMessage &m : r.input->context().stream.propagate())
		{
			if (m.type() == MSG_FOUNDATION_CONSTRUCT)
			{
				++foundMsgs;
			}
		}
		CHECK(foundMsgs == 2);
	}
}
