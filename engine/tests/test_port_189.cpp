// OpenBFME. PORT-189 (issue #455): clicking a hovered Palantir/side-bar command button drops the
// ControlBar help provider (ParasiticInGameCommandButton::Impl left/right click RW 0x9D2814 ->
// RW 0x9D282D / RW 0x9D288A -> RW 0x9D28A3 -> RW 0x8079BB -> RW 0x8076CD) and ControlBar preserves
// the click-predicted CommandButton toggle index (+0xFC, RW 0x941367 / 0x941A22 / 0x9414E7 / 0x941C16)
// across HUD frames until authoritative logic state changes or context/dirty refreshes (RW 0x71FD81),
// so hovered toggle buttons (auto-acquire, weaponset, formation, special power) immediately refresh
// their tooltip name and description after a click without leaving the button.
//
// Synthetic INI only (no game archives / ROTWK_INSTALL / OPENBFME_ENNORATH_INSTALL required).
// GPL-3.0.

#include "doctest.h"

#include "CombatTestUtil.h"

#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/SpecialPower.h"
#include "GameClient/CommandButtonHelp.h"
#include "GameClient/ControlBar.h"
#include "GameClient/ControlBarCommands.h"
#include "GameClient/FXList.h"
#include "GameClient/GameTextTableSource.h"
#include "GameClient/HudInput.h"
#include "GameClient/InGameHelpBox.h"
#include "GameClient/InGameHud.h"
#include "GameClient/TacticalView.h"
#include "GameLogic/AI/WeaponCommands.h"
#include "GameLogic/AttributeModifiers.h"
#include "GameLogic/GameMessage.h"
#include "GameLogic/Module/HeroAbilityModules.h"
#include "GameLogic/Module/SpecialAbilityModules.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Module/StancesBehavior.h"
#include "GameLogic/Object/Contain/HordeContainRuntime.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/ObjectTemplateInfo.h"
#include "GameLogic/SpellCommands.h"
#include "GameLogic/WeaponSetToggle.h"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace
{
struct Port189Text : GameTextSource
{
	std::map<std::string, std::u16string> strings;

	bool fetch(const std::string &label, std::u16string &out) const override
	{
		const auto it = strings.find(label);
		if (it == strings.end())
		{
			return false;
		}
		out = it->second;
		return true;
	}
};

const char kPort189Ini[] =
	"SpecialPower SpecialAbilitySynthToggleMounted\n"
	"  Enum = SPECIAL_TOGGLE_MOUNTED\n"
	"  ReloadTime = 200\n"
	"End\n"
	"SpecialPower SpecialAbilitySynthAutoBuff\n"
	"  Enum = SPECIAL_HERO_MODE\n"
	"  ReloadTime = 200\n"
	"End\n"
	"StanceTemplate SynthStanceTemplate\n"
	"  Stance Battle\n"
	"  End\n"
	"  Stance HoldGround\n"
	"  End\n"
	"  Stance Aggressive\n"
	"  End\n"
	"End\n"
	// Mirrors Ennorath Command_ToggleTrebAutoAcquire / Command_ToggleNoAutoAcquire
	"CommandButton Command_SynthToggleNoAutoAcquire\n"
	"  Command = TOGGLE_NO_AUTO_ACQUIRE\n"
	"  Options = TOGGLE_IMAGE_ON_WEAPON OK_FOR_MULTI_SELECT\n"
	"  ButtonImage = ImgAutoOn ImgAutoOff\n"
	"  TextLabel = CONTROLBAR:AutoAcquire CONTROLBAR:NoAutoAcquire\n"
	"  DescriptLabel = CONTROLBAR:TipAutoAcquire CONTROLBAR:TipNoAutoAcquire\n"
	"  InPalantir = Yes\n"
	"End\n"
	// Mirrors Ennorath TOGGLE_WEAPONSET (e.g. Command_ToggleEntThrowRocks)
	"CommandButton Command_SynthToggleWeaponSet\n"
	"  Command = TOGGLE_WEAPONSET\n"
	"  Options = TOGGLE_IMAGE_ON_WEAPONSET OK_FOR_MULTI_SELECT\n"
	"  FlagsUsedForToggle = WEAPONSET_TOGGLE_1\n"
	"  ButtonImage = ImgWeaponA ImgWeaponB\n"
	"  TextLabel = CONTROLBAR:WeaponModeA CONTROLBAR:WeaponModeB\n"
	"  DescriptLabel = CONTROLBAR:TipWeaponA CONTROLBAR:TipWeaponB\n"
	"  InPalantir = Yes\n"
	"End\n"
	// Mirrors Ennorath HORDE_TOGGLE_FORMATION (68 buttons in mod)
	"CommandButton Command_SynthToggleFormation\n"
	"  Command = HORDE_TOGGLE_FORMATION\n"
	"  Options = TOGGLE_IMAGE_ON_FORMATION OK_FOR_MULTI_SELECT\n"
	"  ButtonImage = ImgFormBlock ImgFormLine\n"
	"  TextLabel = CONTROLBAR:FormBlock CONTROLBAR:FormLine\n"
	"  DescriptLabel = CONTROLBAR:TipFormBlock CONTROLBAR:TipFormLine\n"
	"  InPalantir = Yes\n"
	"End\n"
	// Mirrors Ennorath SPECIAL_POWER + TOGGLE_IMAGE_ON_WEAPONSET (e.g. mount / torch with UnpackTime > 0)
	"CommandButton Command_SynthToggleMountPower\n"
	"  Command = SPECIAL_POWER\n"
	"  SpecialPower = SpecialAbilitySynthToggleMounted\n"
	"  Options = TOGGLE_IMAGE_ON_WEAPONSET ON_GROUND_ONLY\n"
	"  FlagsUsedForToggle = MOUNTED\n"
	"  ButtonImage = ImgMount ImgDismount\n"
	"  TextLabel = CONTROLBAR:MountHorse CONTROLBAR:DismountHorse\n"
	"  DescriptLabel = CONTROLBAR:TipMountHorse CONTROLBAR:TipDismountHorse\n"
	"  InPalantir = Yes\n"
	"End\n"
	// Mirrors AutoAbility right-click command button (RW 0x9D288A -> 0x9D28A3 -> 0x8079BB)
	"CommandButton Command_SynthAutoAbility\n"
	"  Command = SPECIAL_POWER\n"
	"  SpecialPower = SpecialAbilitySynthAutoBuff\n"
	"  Options = OK_FOR_MULTI_SELECT\n"
	"  AutoAbility = Yes\n"
	"  ButtonImage = ImgAutoBuff\n"
	"  TextLabel = CONTROLBAR:AutoBuff\n"
	"  DescriptLabel = CONTROLBAR:TipAutoBuff\n"
	"  InPalantir = Yes\n"
	"End\n"
	// Mirrors GUI_COMMAND_TOGGLE_STANCE (RW 0x96FAC6 / 0x96F4FA / 0x96F53C)
	"CommandButton Command_SynthToggleStance\n"
	"  Command = TOGGLE_STANCE\n"
	"  Options = OK_FOR_MULTI_SELECT\n"
	"  Stances = Battle HoldGround Aggressive\n"
	"  ButtonImage = ImgStanceBattle ImgStanceHold ImgStanceAggro\n"
	"  TextLabel = CONTROLBAR:StanceBattle CONTROLBAR:StanceHold CONTROLBAR:StanceAggro\n"
	"  DescriptLabel = CONTROLBAR:TipStanceBattle CONTROLBAR:TipStanceHold CONTROLBAR:TipStanceAggro\n"
	"  InPalantir = Yes\n"
	"End\n"
	// Side-bar (InPalantir = No) toggle button to prove both Palantir arc and side bar share the path
	"CommandButton Command_SynthSideBarToggle\n"
	"  Command = TOGGLE_NO_AUTO_ACQUIRE\n"
	"  Options = TOGGLE_IMAGE_ON_WEAPON OK_FOR_MULTI_SELECT\n"
	"  ButtonImage = ImgSideOn ImgSideOff\n"
	"  TextLabel = CONTROLBAR:AutoAcquire CONTROLBAR:NoAutoAcquire\n"
	"  DescriptLabel = CONTROLBAR:TipAutoAcquire CONTROLBAR:TipNoAutoAcquire\n"
	"  InPalantir = No\n"
	"End\n"
	"CommandSet SynthSiegeCommandSet\n"
	"  1 = Command_SynthToggleNoAutoAcquire\n"
	"  2 = Command_SynthToggleWeaponSet\n"
	"  3 = Command_SynthToggleStance\n"
	"  4 = Command_SynthAutoAbility\n"
	"  5 = Command_SynthSideBarToggle\n"
	"End\n"
	"CommandSet SynthHeroCommandSet\n"
	"  1 = Command_SynthToggleMountPower\n"
	"End\n"
	"CommandSet SynthHordeCommandSet\n"
	"  1 = Command_SynthToggleFormation\n"
	"End\n"
	"Object SynthTrebuchet\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK SIEGEENGINE\n"
	"  VisionRange = 200\n"
	"  CommandSet = SynthSiegeCommandSet\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryMinorRadius = 10\n"
	"  GeometryHeight = 20\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY BowWeapon\n"
	"  End\n"
	"  WeaponSet\n"
	"    Conditions = WEAPONSET_TOGGLE_1\n"
	"    Weapon = PRIMARY SwordWeapon\n"
	"  End\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"    InitialHealth = 100\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = StancesBehavior ModuleTag_Stances\n"
	"    StanceTemplate = SynthStanceTemplate\n"
	"  End\n"
	"  Behavior = SpecialPowerModule ModuleTag_AutoSP\n"
	"    SpecialPowerTemplate = SpecialAbilitySynthAutoBuff\n"
	"    UpdateModuleStartsAttack = Yes\n"
	"    StartsPaused = No\n"
	"  End\n"
	"  Behavior = SpecialAbilityUpdate ModuleTag_AutoUpdate\n"
	"    SpecialPowerTemplate = SpecialAbilitySynthAutoBuff\n"
	"    UnpackTime = 200\n"
	"    PackTime = 200\n"
	"  End\n"
	"  Behavior = AutoAbilityBehavior ModuleTag_Auto\n"
	"    SpecialAbility = SpecialAbilitySynthAutoBuff\n"
	"    MaxScanRange = 200\n"
	"    StartsActive = No\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 50\n"
	"  End\n"
	"End\n"
	"Object SynthMountedHero\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK HERO\n"
	"  VisionRange = 200\n"
	"  CommandSet = SynthHeroCommandSet\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY SwordWeapon\n"
	"  End\n"
	"  WeaponSet\n"
	"    Conditions = MOUNTED\n"
	"    Weapon = PRIMARY BowWeapon\n"
	"  End\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"    InitialHealth = 100\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = SpecialPowerModule ModuleTag_MountSP\n"
	"    SpecialPowerTemplate = SpecialAbilitySynthToggleMounted\n"
	"    UpdateModuleStartsAttack = Yes\n"
	"    StartsPaused = No\n"
	"  End\n"
	"  Behavior = ToggleMountedSpecialAbilityUpdate ModuleTag_MountUpdate\n"
	"    SpecialPowerTemplate = SpecialAbilitySynthToggleMounted\n"
	"    UnpackTime = 300\n"
	"    PreparationTime = 1\n"
	"    PersistentPrepTime = 200\n"
	"    PackTime = 200\n"
	"    OpacityTarget = .3\n"
	"    IgnoreFacingCheck = Yes\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 50\n"
	"  End\n"
	"End\n"
	"Object SynthHordeMember\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK\n"
	"  VisionRange = 150\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY SwordWeapon\n"
	"  End\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"    InitialHealth = 100\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 50\n"
	"  End\n"
	"End\n"
	"Object SynthHordeMain\n"
	"  KindOf = HORDE MELEE_HORDE LARGE_RECTANGLE_PATHFIND SELECTABLE CAN_ATTACK\n"
	"  VisionRange = 150\n"
	"  CommandSet = SynthHordeCommandSet\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 30\n"
	"  GeometryMinorRadius = 45\n"
	"  GeometryHeight = 20\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY HordeRangefinder\n"
	"  End\n"
	"  Body = ImmortalBody ModuleTag_Body\n"
	"    MaxHealth = 1\n"
	"  End\n"
	"  Behavior = HordeAIUpdate ModuleTag_AI\n"
	"  End\n"
	"  Behavior = HordeContain ModuleTag_Contain\n"
	"    Slots = 2\n"
	"    RankInfo = RankNumber:1 UnitType:SynthHordeMember Position:X:20 Y:10 Position:X:20 Y:-10\n"
	"    InitialPayload = SynthHordeMember 2\n"
	"    AlternateFormation = SynthHordeAlt\n"
	"    ThisFormationIsTheMainFormation = Yes\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = HordeLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 50\n"
	"  End\n"
	"End\n"
	"Object SynthHordeAlt\n"
	"  KindOf = HORDE MELEE_HORDE LARGE_RECTANGLE_PATHFIND SELECTABLE CAN_ATTACK\n"
	"  VisionRange = 150\n"
	"  CommandSet = SynthHordeCommandSet\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 30\n"
	"  GeometryMinorRadius = 45\n"
	"  GeometryHeight = 20\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY HordeRangefinder\n"
	"  End\n"
	"  Body = ImmortalBody ModuleTag_Body\n"
	"    MaxHealth = 1\n"
	"  End\n"
	"  Behavior = HordeAIUpdate ModuleTag_AI\n"
	"  End\n"
	"  Behavior = HordeContain ModuleTag_Contain\n"
	"    Slots = 2\n"
	"    RankInfo = RankNumber:1 UnitType:SynthHordeMember Position:X:10 Y:10 Position:X:10 Y:-10\n"
	"    InitialPayload = SynthHordeMember 2\n"
	"    AlternateFormation = SynthHordeMain\n"
	"    ThisFormationIsTheMainFormation = No\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = HordeLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 50\n"
	"  End\n"
	"End\n";

struct Port189Rig : combattest::CombatWorld
{
	SpecialPowerStore powers;
	SpecialPowerStore *savedPowers = TheSpecialPowerStore;
	CommandStore commandStore;
	CommandStore *savedCommands = TheCommandStore;
	FXListStore fxLists;
	FXListStore *savedFx = TheFXListStore;
	AttributeModifierStore modifiers;
	AttributeModifierStore *savedModifiers = TheAttributeModifierStore;
	StanceTemplateStore stanceTemplates;
	StanceTemplateStore *savedStances = TheStanceTemplateStore;
	SpellCommands spellCommands;
	MouseSettings mouse;
	MetaMap metaMap;
	PinholeView view{ { 0.0f, -300.0f, 400.0f }, { 100.0f, 100.0f, 0.0f }, 1024, 768, 0.7f };
	std::unique_ptr<HudInput> input;
	std::unique_ptr<ControlBar> bar;
	InGameHelpBox helpBox;
	Port189Text text;
	std::uint32_t clockMs = 1000;
	int tooltipDelayMs = 1; // retail ControlBar.wnd ButtonCommand TOOLTIPDELAY = 1
	int composeCalls = 0;

	Port189Rig()
		: combattest::CombatWorld("", 40)
	{
		TheSpecialPowerStore = &powers;
		TheCommandStore = &commandStore;
		TheFXListStore = &fxLists;
		TheAttributeModifierStore = &modifiers;
		TheStanceTemplateStore = &stanceTemplates;
		commandStore.setThingFactory(&w.things);

		modifiers.registerBlock(w.fx.env.blocks);
		stanceTemplates.registerBlock(w.fx.env.blocks);
		w.fx.env.blocks.registerBlock("FXList", [](INI *ini) { ParseFXListDefinitionGlobal(ini); });
		w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
		w.fx.env.blocks.registerBlock("CommandButton", [](INI *ini) { CommandStore::parseCommandButtonDefinitionGlobal(ini); });
		w.fx.env.blocks.registerBlock("CommandSet", [](INI *ini) { CommandStore::parseCommandSetDefinitionGlobal(ini); });

		SpecialAbilityModules::registerAll(w.modules);
		SpecialPowerModules::registerAll(w.modules);
		SpecialPowerModules::installScienceHooks(*logic);
		StancesBehavior::registerClass(w.modules);

		WeaponSetToggle::registerHandlers(dispatcher);
		HordeCommands::registerHandlers(dispatcher);
		WeaponCommands::registerHandlers(dispatcher);
		HeroAbilityModules::registerHandlers(dispatcher);
		StancesBehavior::registerHandlers(dispatcher);
		spellCommands.registerHandlers(dispatcher);

		const std::string err = w.load(kPort189Ini, INI_LOAD_OVERWRITE, "port189.ini");
		REQUIRE_MESSAGE(err.empty(), err);

		Player *alice = players.findPlayerWithName("Alice");
		REQUIRE(alice != nullptr);
		players.setLocalPlayer(alice);

		input = std::make_unique<HudInput>(*logic, ai.get(), view, commands, mouse, metaMap);
		input->context().commands = &commandStore;
		bar = std::make_unique<ControlBar>(input->context());

		text.strings = {
			{ "CONTROLBAR:AutoAcquire", u"&Auto-Acquire" },
			{ "CONTROLBAR:NoAutoAcquire", u"&Cease Fire" },
			{ "CONTROLBAR:TipAutoAcquire", u"Resume firing automatically." },
			{ "CONTROLBAR:TipNoAutoAcquire", u"Hold fire until ordered." },
			{ "CONTROLBAR:WeaponModeA", u"&Throw Rocks" },
			{ "CONTROLBAR:WeaponModeB", u"&Kick and Punch" },
			{ "CONTROLBAR:TipWeaponA", u"Switch to ranged boulders." },
			{ "CONTROLBAR:TipWeaponB", u"Switch to melee attacks." },
			{ "CONTROLBAR:FormBlock", u"&Block Formation" },
			{ "CONTROLBAR:FormLine", u"&Line Formation" },
			{ "CONTROLBAR:TipFormBlock", u"Tight defensive block." },
			{ "CONTROLBAR:TipFormLine", u"Wide offensive line." },
			{ "CONTROLBAR:MountHorse", u"&Mount Shadowfax" },
			{ "CONTROLBAR:DismountHorse", u"&Dismount Shadowfax" },
			{ "CONTROLBAR:TipMountHorse", u"Ride faster across the field." },
			{ "CONTROLBAR:TipDismountHorse", u"Fight on foot." },
			{ "CONTROLBAR:AutoBuff", u"&Auto Buff" },
			{ "CONTROLBAR:TipAutoBuff", u"Right-click to toggle autocast." },
			{ "CONTROLBAR:StanceBattle", u"&Battle Stance" },
			{ "CONTROLBAR:StanceHold", u"&Hold Ground" },
			{ "CONTROLBAR:StanceAggro", u"&Aggressive Stance" },
			{ "CONTROLBAR:TipStanceBattle", u"Balanced offense and defense." },
			{ "CONTROLBAR:TipStanceHold", u"Higher armor, holds position." },
			{ "CONTROLBAR:TipStanceAggro", u"Higher damage, lower armor." },
		};

		// Bring InGameHelpBox to state 2 (ready to show) using the exact Palantir movie handshake (RW 0x6D4F28 / 0x92E462)
		helpBox.loaded("_level0.helpBox", [](const std::string &, const std::string &, const std::vector<std::string> &) { return true; });
		auto measure = [](CommandButtonHelp &, int) { return 80; };
		helpBox.update(measure, 1.0f); // state 0 -> 1 (SampleContentWidth)
		std::vector<HelpDrawOp> ops;
		helpBox.render(0.0f, 0.0f, 260.0f, 120.0f, ops); // samples width = 260
		helpBox.update(measure, 1.0f); // state 1 -> 2 (Hide, ready)
		REQUIRE(helpBox.state() == 2);
	}

	~Port189Rig()
	{
		bar.reset();
		input.reset();
		logic->reset();
		TheSpecialPowerStore = savedPowers;
		TheCommandStore = savedCommands;
		TheFXListStore = savedFx;
		TheAttributeModifierStore = savedModifiers;
		TheStanceTemplateStore = savedStances;
	}

	Object *spawn(const char *name, float x = 100.0f, float y = 100.0f)
	{
		Object *obj = unit(name, 'A', x, y);
		REQUIRE(obj != nullptr);
		return obj;
	}

	void select(Object *obj)
	{
		input->ui().deselectAll(true);
		GameMessage sel(MSG_CREATE_SELECTED_GROUP, playerIndex("Alice"));
		sel.appendBooleanArgument(true);
		if (obj)
		{
			input->ui().selectObject(obj->getID());
			sel.appendObjectIDArgument(obj->getID());
		}
		input->submit(sel);
		input->flush();
		frames(1);
		bar->update();
	}

	const ControlBarButton *buttonNamed(const char *name) const
	{
		for (const auto *list : { &bar->palantirButtons(), &bar->sideButtons(), &bar->offBarButtons() })
		{
			for (const ControlBarButton &b : *list)
			{
				if (b.button && b.button->m_name == name)
				{
					return &b;
				}
			}
		}
		return nullptr;
	}

	// Mirrors InGameHud::update (InGameHud.cpp:692) + InGameHud::updateHud6 (InGameHudHud6.cpp:427-534):
	// bar->update() -> helpBox.beginFrame() -> helpBox.showHelp(hovered) -> helpBox.update()
	void hudFrameHover(const char *buttonName, int barKind, std::uint32_t dtMs = 16)
	{
		clockMs += dtMs;
		bar->update();
		helpBox.beginFrame();
		const ControlBarButton *hovered = buttonName ? buttonNamed(buttonName) : nullptr;
		if (hovered && hovered->button)
		{
			InGameHelpBox::Provider p;
			p.bar = barKind;
			p.slot = hovered->slot;
			p.button = hovered->button;
			Object *selection = logic->findObjectByID(input->ui().firstSelected());
			Player *local = input->context().localPlayer();
			Player *owner = selection && selection->getControllingPlayer() ? selection->getControllingPlayer() : local;
			helpBox.showHelp(p, clockMs, tooltipDelayMs, [&]() {
				++composeCalls;
				const CommandButtonHelp::Text t = CommandButtonHelp::compose(*hovered, &text, *logic, owner, local, selection, "Men", &input->ui().selected());
				return std::make_shared<CommandButtonHelp>(t, &text, nullptr, nullptr);
			});
		}
		helpBox.update([](CommandButtonHelp &, int) { return 80; }, 1.0f);
	}

	void stepLogic(int n)
	{
		input->flush();
		frames(n);
	}

	static bool hasNoAutoAcquire(const Object &o)
	{
		static const int kNoAutoAcquire = ObjectTemplateInfoBuilder::objectStatusIndex("NO_AUTO_ACQUIRE");
		return kNoAutoAcquire >= 0 && o.testStatus(kNoAutoAcquire);
	}
};
} // namespace

TEST_CASE("PORT-189 clicking a hovered auto-attack toggle drops the help provider and refreshes tooltip text immediately and across logic ticks")
{
	Port189Rig rig;
	Object *treb = rig.spawn("SynthTrebuchet");
	rig.select(treb);

	// Hover the Palantir auto-acquire toggle button: starts with NO_AUTO_ACQUIRE == false -> +0xFC == 1 ("Cease Fire")
	rig.hudFrameHover("Command_SynthToggleNoAutoAcquire", 0, 16);
	REQUIRE(rig.helpBox.hasProvider());
	CHECK(rig.helpBox.state() == 2); // delay hasn't elapsed on the first hover frame
	rig.hudFrameHover("Command_SynthToggleNoAutoAcquire", 0, 16);
	REQUIRE(rig.helpBox.isShown());
	REQUIRE(rig.helpBox.help() != nullptr);
	CHECK(rig.helpBox.help()->name() == u"Cease Fire");
	CHECK(rig.helpBox.help()->longText() == u"Hold fire until ordered.");
	CHECK(rig.composeCalls == 1);

	const ControlBarButton *btn = rig.buttonNamed("Command_SynthToggleNoAutoAcquire");
	REQUIRE(btn != nullptr);
	CHECK(btn->inPalantir);

	// Left-click via the Palantir click path (RW 0x9D2814 -> 0x9D282D -> 0x8079BB -> 0x8076CD -> GBM_SELECTED):
	// drops the provider, starts hideSoon (state 4), and flips +0xFC to 0 ("Auto-Acquire") immediately.
	REQUIRE(InGameHud::dispatchPalantirButtonPress(rig.helpBox, *rig.bar, btn->slot, true));
	CHECK_FALSE(rig.helpBox.hasProvider());
	CHECK(rig.helpBox.state() == 4);
	CHECK_FALSE(rig.helpBox.isShown());
	btn = rig.buttonNamed("Command_SynthToggleNoAutoAcquire");
	REQUIRE(btn != nullptr);
	CHECK(btn->button->getToggleIndex() == 0);
	CHECK(btn->textLabel == "CONTROLBAR:AutoAcquire");
	CHECK(btn->descriptLabel == "CONTROLBAR:TipAutoAcquire");
	CHECK(btn->image == "ImgAutoOn");

	// Sub-case A: 60+ FPS HUD frames BEFORE GameLogic executes MSG_TOGGLE_NO_AUTO_ACQUIRE.
	// Without click-predicted toggle preservation (RW 0x71FD81), bar->update() would revert +0xFC to 1
	// and freeze the re-composed tooltip on the stale "Cease Fire" text.
	rig.hudFrameHover("Command_SynthToggleNoAutoAcquire", 0, 16); // reinstalls provider at new hover start
	CHECK(rig.helpBox.hasProvider());
	rig.hudFrameHover("Command_SynthToggleNoAutoAcquire", 0, 16); // now > start + 1ms -> re-composes!
	REQUIRE(rig.helpBox.isShown());
	REQUIRE(rig.helpBox.help() != nullptr);
	CHECK(rig.composeCalls == 2);
	CHECK(rig.helpBox.help()->name() == u"Auto-Acquire");
	CHECK(rig.helpBox.help()->longText() == u"Resume firing automatically.");

	// Sub-case B: Logic commits MSG_TOGGLE_NO_AUTO_ACQUIRE -> object status now matches predicted index 0.
	rig.stepLogic(2);
	REQUIRE(Port189Rig::hasNoAutoAcquire(*treb));
	rig.hudFrameHover("Command_SynthToggleNoAutoAcquire", 0, 16);
	CHECK(rig.helpBox.isShown());
	CHECK(rig.composeCalls == 2); // steady hover does not re-compose once refreshed
	CHECK(rig.helpBox.help()->name() == u"Auto-Acquire");

	// Second click while still hovering toggles back to "Cease Fire" / "Hold fire until ordered."
	btn = rig.buttonNamed("Command_SynthToggleNoAutoAcquire");
	REQUIRE(btn != nullptr);
	REQUIRE(InGameHud::dispatchPalantirButtonPress(rig.helpBox, *rig.bar, btn->slot, true));
	CHECK_FALSE(rig.helpBox.hasProvider());
	CHECK(rig.helpBox.state() == 4);
	rig.stepLogic(2);
	CHECK_FALSE(Port189Rig::hasNoAutoAcquire(*treb));
	rig.hudFrameHover("Command_SynthToggleNoAutoAcquire", 0, 16);
	rig.hudFrameHover("Command_SynthToggleNoAutoAcquire", 0, 16);
	REQUIRE(rig.helpBox.isShown());
	CHECK(rig.composeCalls == 3);
	CHECK(rig.helpBox.help()->name() == u"Cease Fire");
	CHECK(rig.helpBox.help()->longText() == u"Hold fire until ordered.");

	// Side-bar button (InPalantir = No, bar = 1, RW 0x92F165 -> ParasiticInGameCommandButton::Impl) uses the same path
	rig.hudFrameHover("Command_SynthSideBarToggle", 1, 16); // hides old helpSoon -> installs provider after hide
	// Wait out the 5-frame hideSoon when switching providers, or directly re-hover after state 2
	for (int i = 0; i < 6; ++i)
	{
		rig.hudFrameHover("Command_SynthSideBarToggle", 1, 16);
	}
	REQUIRE(rig.helpBox.isShown());
	CHECK(rig.helpBox.help()->name() == u"Cease Fire");
	const int beforeSideClick = rig.composeCalls;
	const ControlBarButton *sideBtn = rig.buttonNamed("Command_SynthSideBarToggle");
	REQUIRE(sideBtn != nullptr);
	CHECK_FALSE(sideBtn->inPalantir);
	REQUIRE(InGameHud::dispatchPalantirButtonPress(rig.helpBox, *rig.bar, sideBtn->slot, false));
	CHECK_FALSE(rig.helpBox.hasProvider());
	rig.hudFrameHover("Command_SynthSideBarToggle", 1, 16);
	rig.hudFrameHover("Command_SynthSideBarToggle", 1, 16);
	REQUIRE(rig.helpBox.isShown());
	CHECK(rig.composeCalls == beforeSideClick + 1);
	CHECK(rig.helpBox.help()->name() == u"Auto-Acquire");
}

TEST_CASE("PORT-189 clicking hovered weaponset, formation, and special-power toggles refreshes tooltip name and description")
{
	// 1. TOGGLE_WEAPONSET (RW 0x941A22 -> 0x75D22E(obj, 1))
	{
		Port189Rig rig;
		Object *treb = rig.spawn("SynthTrebuchet");
		rig.select(treb);
		rig.hudFrameHover("Command_SynthToggleWeaponSet", 0, 16);
		rig.hudFrameHover("Command_SynthToggleWeaponSet", 0, 16);
		REQUIRE(rig.helpBox.isShown());
		CHECK(rig.helpBox.help()->name() == u"Throw Rocks");
		CHECK(rig.helpBox.help()->longText() == u"Switch to ranged boulders.");

		const ControlBarButton *btn = rig.buttonNamed("Command_SynthToggleWeaponSet");
		REQUIRE(btn != nullptr);
		REQUIRE(InGameHud::dispatchPalantirButtonPress(rig.helpBox, *rig.bar, btn->slot, true));
		CHECK_FALSE(rig.helpBox.hasProvider());
		rig.hudFrameHover("Command_SynthToggleWeaponSet", 0, 16);
		rig.hudFrameHover("Command_SynthToggleWeaponSet", 0, 16);
		REQUIRE(rig.helpBox.isShown());
		CHECK(rig.helpBox.help()->name() == u"Kick and Punch");
		CHECK(rig.helpBox.help()->longText() == u"Switch to melee attacks.");

		rig.stepLogic(2);
		rig.hudFrameHover("Command_SynthToggleWeaponSet", 0, 16);
		CHECK(rig.helpBox.help()->name() == u"Kick and Punch");
	}

	// 2. HORDE_TOGGLE_FORMATION (RW 0x941A22 -> 0x75D22E(obj, 1))
	{
		Port189Rig rig;
		Object *horde = rig.spawn("SynthHordeMain");
		rig.select(horde);
		rig.hudFrameHover("Command_SynthToggleFormation", 0, 16);
		rig.hudFrameHover("Command_SynthToggleFormation", 0, 16);
		REQUIRE(rig.helpBox.isShown());
		CHECK(rig.helpBox.help()->name() == u"Block Formation");
		CHECK(rig.helpBox.help()->longText() == u"Tight defensive block.");

		const ControlBarButton *btn = rig.buttonNamed("Command_SynthToggleFormation");
		REQUIRE(btn != nullptr);
		REQUIRE(InGameHud::dispatchPalantirButtonPress(rig.helpBox, *rig.bar, btn->slot, true));
		CHECK_FALSE(rig.helpBox.hasProvider());
		// Before logic swaps the horde object, predicted toggle index refreshes the hovered help box immediately
		rig.hudFrameHover("Command_SynthToggleFormation", 0, 16);
		rig.hudFrameHover("Command_SynthToggleFormation", 0, 16);
		REQUIRE(rig.helpBox.isShown());
		CHECK(rig.helpBox.help()->name() == u"Line Formation");
		CHECK(rig.helpBox.help()->longText() == u"Wide offensive line.");
	}

	// 3. SPECIAL_POWER + TOGGLE_IMAGE_ON_WEAPONSET with non-zero UnpackTime (RW 0x9414E7 -> 0x75D22E(obj, 1))
	{
		Port189Rig rig;
		Object *hero = rig.spawn("SynthMountedHero");
		rig.select(hero);
		rig.hudFrameHover("Command_SynthToggleMountPower", 0, 16);
		rig.hudFrameHover("Command_SynthToggleMountPower", 0, 16);
		REQUIRE(rig.helpBox.isShown());
		CHECK(rig.helpBox.help()->name() == u"Mount Shadowfax");
		CHECK(rig.helpBox.help()->longText() == u"Ride faster across the field.");

		const ControlBarButton *btn = rig.buttonNamed("Command_SynthToggleMountPower");
		REQUIRE(btn != nullptr);
		REQUIRE(InGameHud::dispatchPalantirButtonPress(rig.helpBox, *rig.bar, btn->slot, true));
		CHECK_FALSE(rig.helpBox.hasProvider());

		// Step only 2 logic frames (66ms < UnpackTime 300ms): MOUNTED is not set yet on the object,
		// yet RotWK preserves click-predicted +0xFC == 1 so the re-composed tooltip already shows "Dismount Shadowfax".
		rig.stepLogic(2);
		rig.hudFrameHover("Command_SynthToggleMountPower", 0, 16);
		rig.hudFrameHover("Command_SynthToggleMountPower", 0, 16);
		REQUIRE(rig.helpBox.isShown());
		CHECK(rig.helpBox.help()->name() == u"Dismount Shadowfax");
		CHECK(rig.helpBox.help()->longText() == u"Fight on foot.");

		// Finish UnpackTime -> MOUNTED bit commits -> tooltip remains "Dismount Shadowfax"
		rig.stepLogic(25);
		rig.hudFrameHover("Command_SynthToggleMountPower", 0, 16);
		CHECK(rig.helpBox.help()->name() == u"Dismount Shadowfax");
	}
}

TEST_CASE("PORT-189 right-clicking a hovered Palantir button drops the help provider (RW 0x9D288A -> 0x9D28A3 -> 0x8079BB)")
{
	Port189Rig rig;
	Object *treb = rig.spawn("SynthTrebuchet");
	rig.select(treb);

	rig.hudFrameHover("Command_SynthAutoAbility", 0, 16);
	rig.hudFrameHover("Command_SynthAutoAbility", 0, 16);
	REQUIRE(rig.helpBox.isShown());
	CHECK(rig.composeCalls == 1);
	CHECK(rig.helpBox.help()->name() == u"Auto Buff");

	const ControlBarButton *btn = rig.buttonNamed("Command_SynthAutoAbility");
	REQUIRE(btn != nullptr);
	REQUIRE(InGameHud::dispatchPalantirRightButtonPress(rig.helpBox, *rig.bar, btn->slot, true, false));
	CHECK_FALSE(rig.helpBox.hasProvider());
	CHECK(rig.helpBox.state() == 4);
	CHECK_FALSE(rig.helpBox.isShown());

	// Staying hovered re-installs the provider and re-composes after tooltipDelayMs
	rig.hudFrameHover("Command_SynthAutoAbility", 0, 16);
	rig.hudFrameHover("Command_SynthAutoAbility", 0, 16);
	REQUIRE(rig.helpBox.isShown());
	CHECK(rig.composeCalls == 2);
	CHECK(rig.helpBox.help()->name() == u"Auto Buff");
}

TEST_CASE("PORT-189 adjacent guards: steady hover, hotkey without provider drop, stance submenu switch, and selection change")
{
	Port189Rig rig;
	Object *trebA = rig.spawn("SynthTrebuchet", 100.0f, 100.0f);
	Object *trebB = rig.spawn("SynthTrebuchet", 160.0f, 100.0f);
	rig.select(trebA);

	// 1. Steady hover without clicking keeps the shown help box without re-composing (RW 0x8078CA)
	rig.hudFrameHover("Command_SynthToggleNoAutoAcquire", 0, 16);
	rig.hudFrameHover("Command_SynthToggleNoAutoAcquire", 0, 16);
	REQUIRE(rig.helpBox.isShown());
	CHECK(rig.composeCalls == 1);
	for (int i = 0; i < 10; ++i)
	{
		rig.hudFrameHover("Command_SynthToggleNoAutoAcquire", 0, 16);
	}
	CHECK(rig.composeCalls == 1);
	CHECK(rig.helpBox.help()->name() == u"Cease Fire");

	// 2. Direct ControlBar::pressButton (the hotkey path RW 0x944498 / radial path RW 0x7298A3) does NOT call
	// ControlBar::hideHelp (RW 0x8079BB): the provider stays installed while the button flips +0xFC and image.
	const ControlBarButton *btn = rig.buttonNamed("Command_SynthToggleNoAutoAcquire");
	REQUIRE(btn != nullptr);
	REQUIRE(rig.bar->pressButton(btn->slot, true));
	CHECK(rig.helpBox.hasProvider());
	CHECK(rig.helpBox.isShown());
	btn = rig.buttonNamed("Command_SynthToggleNoAutoAcquire");
	REQUIRE(btn != nullptr);
	CHECK(btn->button->getToggleIndex() == 0);
	CHECK(btn->image == "ImgAutoOn");
	rig.hudFrameHover("Command_SynthToggleNoAutoAcquire", 0, 16);
	CHECK(rig.composeCalls == 1); // unchanged because provider was not dropped

	// 3. Switching selection clears pending toggle predictions so trebB shows its own real state (NO_AUTO_ACQUIRE == false -> +0xFC == 1)
	rig.select(trebB);
	btn = rig.buttonNamed("Command_SynthToggleNoAutoAcquire");
	REQUIRE(btn != nullptr);
	CHECK(btn->button->getToggleIndex() == 1);
	CHECK(btn->textLabel == "CONTROLBAR:NoAutoAcquire");
	CHECK(btn->image == "ImgAutoOff");

	// 4. Stance toggle submenu (RW 0x96FAC6 / 0x96F4FA / 0x96F53C):
	// hovering the stance toggle shows Battle Stance; moving to a stance radial item switches provider (bar = 5);
	// sendChangeStance updates +0xFC (applyToggleStance); returning to the stance toggle re-composes Hold Ground.
	rig.helpBox.dropProvider();
	rig.hudFrameHover("Command_SynthToggleStance", 0, 16);
	rig.hudFrameHover("Command_SynthToggleStance", 0, 16);
	REQUIRE(rig.helpBox.isShown());
	CHECK(rig.helpBox.help()->name() == u"Battle Stance");
	CHECK(rig.helpBox.help()->longText() == u"Balanced offense and defense.");

	// Hover a stance radial item (bar = 5, object = STANCE_HOLD_GROUND) until it becomes the active provider, then pick it
	InGameHelpBox::Provider stanceItem;
	stanceItem.bar = 5;
	stanceItem.slot = 1;
	stanceItem.button = rig.buttonNamed("Command_SynthToggleStance")->button;
	stanceItem.object = static_cast<std::uint32_t>(STANCE_HOLD_GROUND);
	for (int i = 0; i < 7; ++i)
	{
		rig.clockMs += 16;
		rig.helpBox.beginFrame();
		rig.helpBox.showHelp(stanceItem, rig.clockMs, rig.tooltipDelayMs, [&]() {
			++rig.composeCalls;
			return std::make_shared<CommandButtonHelp>(CommandButtonHelp::simple(u"Hold Ground Item", u"Stance radial item."));
		});
		rig.helpBox.update([](CommandButtonHelp &, int) { return 80; }, 1.0f);
	}
	REQUIRE(rig.helpBox.hasProvider());
	CHECK(rig.helpBox.provider().bar == 5);
	REQUIRE(rig.bar->sendChangeStance(STANCE_HOLD_GROUND, "PORT-189:stance-test"));
	rig.stepLogic(2);
	// Returning the pointer to the stance toggle button (bar = 0) switches providers and re-composes Hold Ground
	for (int i = 0; i < 8; ++i)
	{
		rig.hudFrameHover("Command_SynthToggleStance", 0, 16);
	}
	REQUIRE(rig.helpBox.isShown());
	CHECK(rig.helpBox.help()->name() == u"Hold Ground");
	CHECK(rig.helpBox.help()->longText() == u"Higher armor, holds position.");
}
