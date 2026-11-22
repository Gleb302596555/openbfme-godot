// PORT-275 (fork issue 642): the Galadriel mirror's WardByMirror / Dissent(Locked) buttons, which the button
// matrix finds NOT-ON-BAR, and the command-set swaps the mirror relies on.
//
// What the real mod data (ennorath-ini-1.9.8.3) shows, verified line by line in docs/ennorath/PORT275_RW_ANALYSIS.md:
//   * the mirror's sets (commandset.inc:27532 / 27540 / 27548) have NO InitialVisible — every slot is in range;
//   * every mirror button (commandbutton.inc:54359..54434) is InPalantir = Yes and Radial = Yes;
//   * the powers (specialpower.ini:3901..3925) have no RequiredSciences.
// So the base mirror shows all four buttons. The matrix's NOT-ON-BAR rows are the mod's own one-ability lock, hit
// by the sweep's slot order: the sweep presses slot 2 (Vision) first, the cast's marker object grants
// Upgrade_GaladrielMirrorUsed (attributemodifier.inc:15996) and the mirror's CommandSetUpgrade
// ModuleTag_MirrorUsedLock (galadrielmirror.ini:181) swaps the bar to GaladrielDarkMirrorUsedCommandSet — one
// passive button. Slots 3 / 4 are gone when the sweep reaches them, exactly as in RotWK for a player.
//
// The ported rules these tests pin (read with rw.py dis):
//   RW 0x97D718 (ControlBar::resetRanges): an empty range stack gets (0, InitialVisible) of the current command set
//   RW 0x71CF0A / RW 0x75CA97: the bar's placement reads the range stack's top; empty -> (0, 33)
//   RW 0x943D6F (populateCommand): command window i shows the button of slot rangeStart + i, only i < rangeCount
//   RW 0x9428A3..0x942993: a NEED_UPGRADE button without its OBJECT upgrade is restricted (the mod's "shown locked")
//   RW 0x8B7C68 (CommandSetUpgrade::upgradeImplementation): the object's command set override swaps the bar's set
// Tests on synthetic INI, no game files. GPL-3.0.

#include "doctest.h"

#include "EconGameData.h"
#include "EconTestUtil.h"
#include "IniTestUtil.h"
#include "LogicTestUtil.h"

#include "Common/INI.h"
#include "Common/SpecialPower.h"
#include "GameClient/ControlBar.h"
#include "GameClient/ControlBarCommands.h"
#include "GameClient/HudContext.h"
#include "GameClient/InGameUI.h"
#include "GameClient/MessageStream/MessageStream.h"
#include "GameClient/TacticalView.h"
#include "GameLogic/Object/Object.h"

#include <string>

namespace
{
// the buttons of the case, in the mod's shape: a passive in slot 1, the powers at slots 2..4; DissentLocked is the
// mod's "Discord shown (locked)" (commandbutton.inc:54418: NEED_UPGRADE Upgrade_Level_8); Harad's window rule is
// pinned with a tower whose set declares InitialVisible = 2 (HaradFortressCommandSet, commandset.inc:25070: 8)
const char buttons[] = R"(
CommandButton Command_P275_Passive
  Command = STOP
  Options = NONPRESSABLE
  ButtonImage = UCCommon_Passive
  TextLabel = CONTROLBAR:P275Passive
  InPalantir = Yes
End
CommandButton Command_P275_Vision
  Command = SPECIAL_POWER
  Options = NEED_TARGET_POS
  SpecialPower = SpecialAbilityP275Vision
  ButtonImage = UCCommon_Vision
  TextLabel = CONTROLBAR:P275Vision
  InPalantir = Yes
End
CommandButton Command_P275_Ward
  Command = SPECIAL_POWER
  Options = NEED_TARGET_POS
  SpecialPower = SpecialAbilityP275Ward
  ButtonImage = UCCommon_Ward
  TextLabel = CONTROLBAR:P275Ward
  InPalantir = Yes
End
CommandButton Command_P275_DissentLocked
  Command = SPECIAL_POWER
  Options = NEED_TARGET_POS NEED_UPGRADE
  SpecialPower = SpecialAbilityP275Dissent
  NeededUpgrade = Upgrade_Level_8
  ButtonImage = UCCommon_DissentLocked
  TextLabel = CONTROLBAR:P275DissentLocked
  InPalantir = Yes
End
CommandButton Command_P275_Dissent
  Command = SPECIAL_POWER
  Options = NEED_TARGET_POS
  SpecialPower = SpecialAbilityP275Dissent
  ButtonImage = UCCommon_Dissent
  TextLabel = CONTROLBAR:P275Dissent
  InPalantir = Yes
End
CommandSet P275WindowSet
  1 = Command_P275_Passive
  2 = Command_P275_Vision
  3 = Command_P275_Ward
  4 = Command_P275_DissentLocked
  InitialVisible = 2
End
CommandSet P275MirrorSet
  1 = Command_P275_Passive
  2 = Command_P275_Vision
  3 = Command_P275_Ward
  4 = Command_P275_DissentLocked
End
CommandSet P275MirrorSet8
  1 = Command_P275_Passive
  2 = Command_P275_Vision
  3 = Command_P275_Ward
  4 = Command_P275_Dissent
End
CommandSet P275MirrorUsedSet
  1 = Command_P275_Passive
End
)";

// the powers: Vision / Ward / Dissent, a short ReloadTime so the evaluation answers Enabled
const char powerIni[] = R"(
SpecialPower SpecialAbilityP275Vision
  Enum = SPECIAL_GENERAL_TARGETLESS
  ReloadTime = 10
End
SpecialPower SpecialAbilityP275Ward
  Enum = SPECIAL_GENERAL_TARGETLESS
  ReloadTime = 10
End
SpecialPower SpecialAbilityP275Dissent
  Enum = SPECIAL_GENERAL_TARGETLESS
  ReloadTime = 10
End
)";

// the upgrades of the case: the veterancy gate (the mod's Upgrade_Level_8, includes/upgrade.inc:96) and the
// used-upgrade the lock swaps on (the mod's Upgrade_GaladrielMirrorUsed, upgrade.ini:3445)
const char upgrades[] = R"(
Upgrade Upgrade_Level_8
  Type = OBJECT
End
Upgrade Upgrade_P275MirrorUsed
  Type = OBJECT
End
Upgrade Upgrade_P275Mirror8
  Type = OBJECT
End
)";

// P275WindowTower is the Harad rule in miniature: InitialVisible = 2 keeps slots 3 / 4 off the bar.
// P275Mirror is the mod's mirror (galadrielmirror.ini:10): the power modules and the used-lock CommandSetUpgrade
// (the mod's ModuleTag_MirrorUsedLock); the level-8 swap stands for the port mechanism (RW 0x8B7C68) — the mod
// itself places the level-8 mirror as the ChildObject GaladrielDarkMirror8 (galadrielmirror.ini:293), the swap
// that must bring a button onto the bar is pinned here as the task asks
const char objects[] = R"(
Object P275WindowTower
  KindOf = SELECTABLE STRUCTURE
  CommandSet = P275WindowSet
  Body = StructureBody ModuleTag_Body
    MaxHealth = 1000
  End
  Geometry = BOX
  GeometryMajorRadius = 10
  GeometryMinorRadius = 10
  GeometryHeight = 10
End
Object P275Mirror
  KindOf = SELECTABLE STRUCTURE
  CommandSet = P275MirrorSet
  Body = StructureBody ModuleTag_Body
    MaxHealth = 1000
  End
  Geometry = BOX
  GeometryMajorRadius = 10
  GeometryMinorRadius = 10
  GeometryHeight = 10
  Behavior = SpecialPowerModule ModuleTag_Vision
    SpecialPowerTemplate = SpecialAbilityP275Vision
  End
  Behavior = SpecialPowerModule ModuleTag_Ward
    SpecialPowerTemplate = SpecialAbilityP275Ward
  End
  Behavior = SpecialPowerModule ModuleTag_Dissent
    SpecialPowerTemplate = SpecialAbilityP275Dissent
  End
  Behavior = CommandSetUpgrade ModuleTag_Swap8
    TriggeredBy = Upgrade_P275Mirror8
    CommandSet = P275MirrorSet8
  End
  Behavior = CommandSetUpgrade ModuleTag_SwapUsed
    TriggeredBy = Upgrade_P275MirrorUsed
    CommandSet = P275MirrorUsedSet
  End
End
)";

// the bar over a synthetic world, command store and special power store (no game files)
struct Fixture
{
	// the upgrades of the case load with the fixture's stock ones, before the objects name them in TriggeredBy
	struct World : logictest::LogicWorld
	{
		World()
			: logictest::LogicWorld(RandomAlgorithm::ZH_CarryChain, upgrades)
		{
			REQUIRE_MESSAGE(loadError.empty(), loadError);
			LogicModules::registerAll(w.modules);
			const std::string err = w.load(objects);
			REQUIRE_MESSAGE(err.empty(), err);
			logic->settings().bodyThresholdsLoaded = true;
			logic->settings().unitDamagedThreshold = 0.65f;
			logic->settings().unitReallyDamagedThreshold = 0.4f;
			std::string e;
			REQUIRE_MESSAGE(EconomySettings::scan(econtest::kGameData, logic->economy().settings(), &e), e);
			logic->economy().initAllCommandPoints();
		}
		Object *makeAt(const char *templateName, const char *owner, float x, float y)
		{
			Object *o = make(templateName, teamOf(owner), INVALID_ID, false);
			Coord3D p{ x, y, 0.0f };
			o->setPosition(&p);
			completeHorde(o);
			return o;
		}
		void frames(int n)
		{
			for (int i = 0; i < n; ++i)
			{
				logic->runLogicFrame();
			}
		}
	};
	World game;
	CommandStore store;
	SpecialPowerStore powers;
	SpecialPowerStore *savedPowers = TheSpecialPowerStore;
	PinholeView view;
	InGameUI ui;
	MessageStream stream;
	HudContext context;
	ControlBar bar;

	Fixture()
		: context{ *game.logic, nullptr, view, ui, stream, &store, {}, {} }
		, bar{ context }
	{
		TheSpecialPowerStore = &powers;
		store.setThingFactory(&game.w.things);
		TheCommandStore = &store;
		game.w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
		game.w.fx.env.blocks.registerBlock("CommandButton", [](INI *ini) { CommandStore::parseCommandButtonDefinitionGlobal(ini); });
		game.w.fx.env.blocks.registerBlock("CommandSet", [](INI *ini) { CommandStore::parseCommandSetDefinitionGlobal(ini); });
		const std::string powerErr = game.w.load(powerIni);
		REQUIRE_MESSAGE(powerErr.empty(), powerErr);
		const std::string buttonErr = game.w.load(buttons);
		REQUIRE_MESSAGE(buttonErr.empty(), buttonErr);
		game.players.setLocalPlayer(game.players.findPlayerWithName("Alice"));
		ui.setLogic(game.logic.get());
		ui.setMaxSelectCount(50);
	}
	~Fixture()
	{
		TheCommandStore = nullptr;
		TheSpecialPowerStore = savedPowers;
	}
	// the evaluated button of `name` on the arc, after a reselect and one update (null: not on the bar)
	const ControlBarButton *onBar(Object *o, const char *name)
	{
		ui.deselectAll(false);
		ui.selectObject(o->getID());
		bar.update();
		for (const ControlBarButton &b : bar.palantirButtons())
		{
			if (b.button && b.button->m_name == name)
			{
				return &b;
			}
		}
		return nullptr;
	}
	int onArcCount()
	{
		int n = 0;
		for (const ControlBarButton &b : bar.palantirButtons())
		{
			if (b.button)
			{
				++n;
			}
		}
		return n;
	}
};
} // namespace

TEST_CASE("PORT-275 a set's slots past its InitialVisible are not on the bar, as in RotWK (RW 0x97D718 -> (0, InitialVisible), RW 0x943D6F shows windows < rangeCount)")
{
	Fixture f;
	Object *tower = f.game.makeAt("P275WindowTower", "Alice", 100.0f, 100.0f);
	REQUIRE(tower);
	// the Harad rule (HaradFortressCommandSet, commandset.inc:25070: InitialVisible = 8): with the empty range stack
	// only the first InitialVisible slots are iterated (the port keeps the stack at (0, 33) and clamps by
	// InitialVisible as RW 0x97D718's (0, InitialVisible) window does)
	CHECK(f.onBar(tower, "Command_P275_Passive"));
	CHECK(f.onBar(tower, "Command_P275_Vision"));
	// slots 3 / 4 (1-based) are outside the window and there is no push button: not on the bar — the rule that keeps
	// Harad's SpyTechAI (slot 32, index 31) unreachable (its push windows end at 8-15 / 16-24 / 25-30)
	CHECK(!f.onBar(tower, "Command_P275_Ward"));
	CHECK(!f.onBar(tower, "Command_P275_DissentLocked"));
	// the neighbour that guards the rule: only the two in-range buttons are on the arc
	CHECK(f.onArcCount() == 2);
}

TEST_CASE("PORT-275 the mirror's base set shows all four buttons; the locked Discord is grey (RW 0x9428A3), as the mod intends")
{
	Fixture f;
	Object *mirror = f.game.makeAt("P275Mirror", "Alice", 100.0f, 100.0f);
	REQUIRE(mirror);
	// no InitialVisible on the mirror's sets (commandset.inc:27532-27550), every button InPalantir = Yes
	// (commandbutton.inc:54359-54434), no RequiredSciences (specialpower.ini:3901-3925): all four are on the bar
	CHECK(f.onBar(mirror, "Command_P275_Passive"));
	CHECK(f.onBar(mirror, "Command_P275_Vision"));
	const ControlBarButton *ward = f.onBar(mirror, "Command_P275_Ward");
	REQUIRE(ward);
	CHECK(ward->state == ButtonState::Enabled);
	// DissentLocked is on the bar and grey: NEED_UPGRADE without Upgrade_Level_8 restricts it (RW 0x9428A3..0x942993),
	// the mod's "Discord shown (locked) on the Mirror placed by a Dark Galadriel below level 8" (commandbutton.inc:54417)
	const ControlBarButton *locked = f.onBar(mirror, "Command_P275_DissentLocked");
	REQUIRE(locked);
	CHECK(locked->state == ButtonState::Restricted);
	CHECK(f.onArcCount() == 4);
}

TEST_CASE("PORT-275 the CommandSetUpgrade swap's set is on the bar at the very next update (RW 0x8B7C68)")
{
	Fixture f;
	Object *mirror = f.game.makeAt("P275Mirror", "Alice", 100.0f, 100.0f);
	REQUIRE(mirror);
	// before the swap: the base set, the locked Discord grey
	REQUIRE(f.onBar(mirror, "Command_P275_DissentLocked"));
	CHECK(!f.onBar(mirror, "Command_P275_Dissent"));
	// the level-8 upgrade lands; CommandSetUpgrade swaps the set (RW 0x8B7C68 -> Object::setCommandSetOverride)
	mirror->giveUpgrade("Upgrade_P275Mirror8");
	CHECK(mirror->getCommandSetName() == "P275MirrorSet8");
	// one update later the bar shows the swapped set: the unlocked Dissent is on the bar, the locked one is gone
	const ControlBarButton *dissent = f.onBar(mirror, "Command_P275_Dissent");
	REQUIRE(dissent);
	CHECK(dissent->state == ButtonState::Enabled);
	CHECK(!f.onBar(mirror, "Command_P275_DissentLocked"));
	CHECK(f.onBar(mirror, "Command_P275_Ward"));
}

TEST_CASE("PORT-275 the one-ability lock empties the bar — the matrix's NOT-ON-BAR rows (galadrielmirror.ini:181, attributemodifier.inc:15996)")
{
	Fixture f;
	Object *mirror = f.game.makeAt("P275Mirror", "Alice", 100.0f, 100.0f);
	REQUIRE(mirror);
	// the sweep's state before a cast: all four buttons on the bar
	REQUIRE(f.onBar(mirror, "Command_P275_Ward"));
	REQUIRE(f.onBar(mirror, "Command_P275_DissentLocked"));
	// an ability was cast: the marker object's aura grants the used-upgrade (the mod's GaladrielMirrorVisionBase /
	// GaladrielMirrorWardBase / GaladrielMirrorDissentBase -> GaladrielMirrorUsedModifier -> Upgrade_GaladrielMirrorUsed)
	mirror->giveUpgrade("Upgrade_P275MirrorUsed");
	CHECK(mirror->getCommandSetName() == "P275MirrorUsedSet");
	// the bar follows in the same update: the spent mirror offers nothing but the passive (GaladrielDarkMirrorUsedCommandSet,
	// commandset.inc:27548) — the slots the sweep had not pressed yet read NOT-ON-BAR, as they do for a player in RotWK
	CHECK(f.onBar(mirror, "Command_P275_Passive"));
	CHECK(!f.onBar(mirror, "Command_P275_Vision"));
	CHECK(!f.onBar(mirror, "Command_P275_Ward"));
	CHECK(!f.onBar(mirror, "Command_P275_DissentLocked"));
	CHECK(!f.onBar(mirror, "Command_P275_Dissent"));
	CHECK(f.onArcCount() == 1);
}
