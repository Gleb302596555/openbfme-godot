// OpenBFME. PORT-169 (issue #398, P-102): CommandButton toggle index (+0xFC) for UnitSpecificSound,
// SetAutoAbilityUnitSound, UnsetAutoAbilityUnitSound, TOGGLE_IMAGE_ON_WEAPONSET, TOGGLE_IMAGE_ON_FORMATION,
// and TOGGLE_NO_AUTO_ACQUIRE (RotWK 2.01 RW 0x75D22E / 0x75D082 / 0x75CDFC / 0x75CE47 / 0x75CECD /
// 0x75D0B9 / 0x75D0F8 / 0x75D137 / 0x9407D9 / 0x940848 / 0x9412F3 / 0x941367 / 0x9414E7 / 0x941A22 / 0x944603).
//
// Synthetic INI only (no game archives / ROTWK_INSTALL / OPENBFME_ENNORATH_INSTALL required).
// GPL-3.0.

#include "doctest.h"

#include "CombatTestUtil.h"

#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/SpecialPower.h"
#include "GameClient/ControlBar.h"
#include "GameClient/ControlBarCommands.h"
#include "GameClient/FXList.h"
#include "GameClient/GUI/Image.h"
#include "GameClient/HudInput.h"
#include "GameClient/TacticalView.h"
#include "GameClient/UnitVoiceResponse.h"
#include "GameLogic/AI/WeaponCommands.h"
#include "GameLogic/AttributeModifiers.h"
#include "GameLogic/GameMessage.h"
#include "GameLogic/Module/HeroAbilityModules.h"
#include "GameLogic/Module/SpecialAbilityModules.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Object/Contain/HordeContainRuntime.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/ObjectTemplateInfo.h"
#include "GameLogic/SpellCommands.h"
#include "GameLogic/WeaponSetToggle.h"

#include <memory>
#include <set>
#include <string>
#include <vector>

namespace
{
struct Port169VoiceSink : UnitVoiceResponse::Sink
{
	std::set<std::string> known;
	std::vector<std::string> sounds;
	std::vector<ObjectID> speakers;

	bool soundExists(const std::string &name) override { return known.count(name) == 1; }
	void playSound(const std::string &name, ObjectID object, int) override
	{
		sounds.push_back(name);
		speakers.push_back(object);
	}
	void reportEva(const std::string &, const Coord3D *) override {}
	void clear()
	{
		sounds.clear();
		speakers.clear();
	}
};

const char kPort169Ini[] =
	"SpecialPower SpecialAbilitySynthToggleMounted\n"
	"  Enum = SPECIAL_TOGGLE_MOUNTED\n"
	"  ReloadTime = 200\n"
	"End\n"
	"SpecialPower SpecialAbilitySynthAutoBuff\n"
	"  Enum = SPECIAL_HERO_MODE\n"
	"  ReloadTime = 200\n"
	"End\n"
	// Mirrors Ennorath's Command_GandalfderWeieShadowfax_forEdoras: two identical ButtonImage entries,
	// one or two TextLabels, and two UnitSpecificSounds (mount whistle / dismount voice).
	"CommandButton Command_SynthGandalfMount\n"
	"  Command = SPECIAL_POWER\n"
	"  SpecialPower = SpecialAbilitySynthToggleMounted\n"
	"  Options = TOGGLE_IMAGE_ON_WEAPONSET ON_GROUND_ONLY\n"
	"  FlagsUsedForToggle = MOUNTED\n"
	"  ButtonImage = HSGandalfShadowfax HSGandalfShadowfax\n"
	"  TextLabel = CONTROLBAR:GandalfMountShadowfax CONTROLBAR:GandalfDismountShadowfax\n"
	"  DescriptLabel = CONTROLBAR:TipMount CONTROLBAR:TipDismount\n"
	"  UnitSpecificSound = GandalfShadowfaxWhistleMS GandalfVoiceDismountMS\n"
	"  InPalantir = Yes\n"
	"End\n"
	// Mirrors CommandButton with 2 images, 1 label, 2 sounds (e.g. Command_GandalfderWeieShadowfax_forEdoras single-label variant)
	"CommandButton Command_SynthSingleLabelTwoSounds\n"
	"  Command = SPECIAL_POWER\n"
	"  SpecialPower = SpecialAbilitySynthToggleMounted\n"
	"  Options = TOGGLE_IMAGE_ON_WEAPONSET ON_GROUND_ONLY\n"
	"  FlagsUsedForToggle = MOUNTED\n"
	"  ButtonImage = HSGandalfShadowfax HSGandalfShadowfax\n"
	"  TextLabel = CONTROLBAR:GandalfShadowfax\n"
	"  DescriptLabel = CONTROLBAR:TipShadowfax\n"
	"  UnitSpecificSound = GandalfShadowfaxWhistleMS GandalfVoiceDismountMS\n"
	"  InPalantir = Yes\n"
	"End\n"
	// Mirrors 2-image / 1-sound toggle buttons (e.g. Command_GrimbeornTransform): silent in toggled state 1
	"CommandButton Command_SynthOneSoundTwoImages\n"
	"  Command = TOGGLE_WEAPONSET\n"
	"  Options = TOGGLE_IMAGE_ON_WEAPONSET OK_FOR_MULTI_SELECT\n"
	"  FlagsUsedForToggle = WEAPONSET_TOGGLE_1\n"
	"  ButtonImage = ImgTransformA ImgTransformB\n"
	"  TextLabel = CONTROLBAR:TransformSingle\n"
	"  DescriptLabel = CONTROLBAR:TipTransformSingle\n"
	"  UnitSpecificSound = TransformOnceVoice\n"
	"  InPalantir = Yes\n"
	"End\n"
	// Mirrors 1-image / 2-sound non-toggle button (e.g. Command_SpecialAbilityTreacheryOfTheRing): stays at index 0
	"CommandButton Command_SynthOneImageTwoSounds\n"
	"  Command = TOGGLE_WEAPONSET\n"
	"  Options = TOGGLE_IMAGE_ON_WEAPONSET OK_FOR_MULTI_SELECT\n"
	"  FlagsUsedForToggle = WEAPONSET_TOGGLE_1\n"
	"  ButtonImage = ImgSingleOnly\n"
	"  TextLabel = CONTROLBAR:SingleOnlyA CONTROLBAR:SingleOnlyB\n"
	"  UnitSpecificSound = SingleSoundA SingleSoundB\n"
	"  InPalantir = Yes\n"
	"End\n"
	// TOGGLE_IMAGE_ON_WEAPONSET + TOGGLE_WEAPONSET with 2 sounds
	"CommandButton Command_SynthWeaponSetSwap\n"
	"  Command = TOGGLE_WEAPONSET\n"
	"  Options = TOGGLE_IMAGE_ON_WEAPONSET OK_FOR_MULTI_SELECT\n"
	"  FlagsUsedForToggle = WEAPONSET_TOGGLE_1\n"
	"  ButtonImage = ImgBow ImgSword\n"
	"  TextLabel = CONTROLBAR:BowMode CONTROLBAR:SwordMode\n"
	"  DescriptLabel = CONTROLBAR:TipBow CONTROLBAR:TipSword\n"
	"  UnitSpecificSound = BowModeVoice SwordModeVoice\n"
	"  InPalantir = Yes\n"
	"End\n"
	// TOGGLE_IMAGE_ON_FORMATION with 2 images, 2 labels, 2 sounds
	"CommandButton Command_SynthFormationToggle\n"
	"  Command = HORDE_TOGGLE_FORMATION\n"
	"  Options = TOGGLE_IMAGE_ON_FORMATION OK_FOR_MULTI_SELECT\n"
	"  ButtonImage = ImgShieldBlock ImgShieldLine\n"
	"  TextLabel = CONTROLBAR:ToggleBlockFormation CONTROLBAR:ToggleLineFormation\n"
	"  DescriptLabel = CONTROLBAR:TipBlock CONTROLBAR:TipLine\n"
	"  UnitSpecificSound = BlockFormationVoice LineFormationVoice\n"
	"  InPalantir = Yes\n"
	"End\n"
	// TOGGLE_IMAGE_ON_FORMATION with 0 images (commented ButtonImage pattern)
	"CommandButton Command_SynthFormationNoImage\n"
	"  Command = HORDE_TOGGLE_FORMATION\n"
	"  Options = TOGGLE_IMAGE_ON_FORMATION OK_FOR_MULTI_SELECT\n"
	"  TextLabel = CONTROLBAR:ToggleBlockFormation CONTROLBAR:ToggleLineFormation\n"
	"  UnitSpecificSound = BlockFormationVoice LineFormationVoice\n"
	"  InPalantir = Yes\n"
	"End\n"
	// TOGGLE_IMAGE_ON_WEAPON with TOGGLE_NO_AUTO_ACQUIRE
	"CommandButton Command_SynthToggleNoAutoAcquire\n"
	"  Command = TOGGLE_NO_AUTO_ACQUIRE\n"
	"  Options = TOGGLE_IMAGE_ON_WEAPON OK_FOR_MULTI_SELECT\n"
	"  ButtonImage = ImgAutoAcquireOn ImgAutoAcquireOff\n"
	"  TextLabel = CONTROLBAR:AcquireOn CONTROLBAR:AcquireOff\n"
	"  DescriptLabel = CONTROLBAR:TipAcquireOn CONTROLBAR:TipAcquireOff\n"
	"  InPalantir = Yes\n"
	"End\n"
	// AutoAbility toggle with indexed Set/UnsetAutoAbilityUnitSound + TOGGLE_IMAGE_ON_WEAPONSET
	"CommandButton Command_SynthAutoAbilityToggle\n"
	"  Command = SPECIAL_POWER\n"
	"  SpecialPower = SpecialAbilitySynthAutoBuff\n"
	"  Options = TOGGLE_IMAGE_ON_WEAPONSET\n"
	"  FlagsUsedForToggle = WEAPONSET_TOGGLE_1\n"
	"  ButtonImage = ImgAutoA ImgAutoB\n"
	"  AutoAbility = Yes\n"
	"  SetAutoAbilityUnitSound = AutoSet0 AutoSet1\n"
	"  UnsetAutoAbilityUnitSound = AutoUnset0 AutoUnset1\n"
	"  InPalantir = Yes\n"
	"End\n"
	"CommandButton Command_SynthAutoAbilityOneSound\n"
	"  Command = SPECIAL_POWER\n"
	"  SpecialPower = SpecialAbilitySynthAutoBuff\n"
	"  Options = TOGGLE_IMAGE_ON_WEAPONSET\n"
	"  FlagsUsedForToggle = WEAPONSET_TOGGLE_1\n"
	"  ButtonImage = ImgAutoA ImgAutoB\n"
	"  AutoAbility = Yes\n"
	"  SetAutoAbilityUnitSound = AutoSetOnly0\n"
	"  UnsetAutoAbilityUnitSound = AutoUnsetOnly0\n"
	"  InPalantir = Yes\n"
	"End\n"
	"CommandSet SynthGandalfCommandSet\n"
	"  1 = Command_SynthGandalfMount\n"
	"  2 = Command_SynthSingleLabelTwoSounds\n"
	"End\n"
	"CommandSet SynthWeaponSetCommandSet\n"
	"  1 = Command_SynthWeaponSetSwap\n"
	"  2 = Command_SynthOneSoundTwoImages\n"
	"  3 = Command_SynthOneImageTwoSounds\n"
	"  4 = Command_SynthToggleNoAutoAcquire\n"
	"End\n"
	"CommandSet SynthFormationCommandSet\n"
	"  1 = Command_SynthFormationToggle\n"
	"  2 = Command_SynthFormationNoImage\n"
	"End\n"
	"CommandSet SynthAutoAbilityCommandSet\n"
	"  1 = Command_SynthAutoAbilityToggle\n"
	"  2 = Command_SynthAutoAbilityOneSound\n"
	"End\n"
	"Object SynthGandalfHero\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK HERO\n"
	"  VisionRange = 200\n"
	"  CommandSet = SynthGandalfCommandSet\n"
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
	"    UnpackTime = 200\n"
	"    PreparationTime = 1\n"
	"    PersistentPrepTime = 200\n"
	"    PackTime = 200\n"
	"    OpacityTarget = .3\n"
	"    IgnoreFacingCheck = Yes\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n"
	"Object SynthSwapHordeMember\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK\n"
	"  VisionRange = 150\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
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
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n"
	"Object SynthSwapHorde\n"
	"  KindOf = HORDE MELEE_HORDE LARGE_RECTANGLE_PATHFIND SELECTABLE CAN_ATTACK\n"
	"  VisionRange = 150\n"
	"  CommandSet = SynthWeaponSetCommandSet\n"
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
	"    RankInfo = RankNumber:1 UnitType:SynthSwapHordeMember Position:X:20 Y:10 Position:X:20 Y:-10\n"
	"    InitialPayload = SynthSwapHordeMember 2\n"
	"    AlternateFormation = SynthSwapHordeAlt\n"
	"    ThisFormationIsTheMainFormation = Yes\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = HordeLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 50\n"
	"  End\n"
	"End\n"
	"Object SynthSwapHordeAlt\n"
	"  KindOf = HORDE MELEE_HORDE LARGE_RECTANGLE_PATHFIND SELECTABLE CAN_ATTACK\n"
	"  VisionRange = 150\n"
	"  CommandSet = SynthFormationCommandSet\n"
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
	"    RankInfo = RankNumber:1 UnitType:SynthSwapHordeMember Position:X:10 Y:10 Position:X:10 Y:-10\n"
	"    InitialPayload = SynthSwapHordeMember 2\n"
	"    AlternateFormation = SynthFormMainHorde\n"
	"    ThisFormationIsTheMainFormation = No\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = HordeLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 50\n"
	"  End\n"
	"End\n"
	"Object SynthFormMainHorde\n"
	"  KindOf = HORDE MELEE_HORDE LARGE_RECTANGLE_PATHFIND SELECTABLE CAN_ATTACK\n"
	"  VisionRange = 150\n"
	"  CommandSet = SynthFormationCommandSet\n"
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
	"    RankInfo = RankNumber:1 UnitType:SynthSwapHordeMember Position:X:20 Y:10 Position:X:20 Y:-10\n"
	"    InitialPayload = SynthSwapHordeMember 2\n"
	"    AlternateFormation = SynthSwapHordeAlt\n"
	"    ThisFormationIsTheMainFormation = Yes\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = HordeLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 50\n"
	"  End\n"
	"End\n"
	"Object SynthIgnoredProp\n"
	"  KindOf = INFANTRY SELECTABLE IGNORED_IN_GUI\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"End\n"
	"Object SynthAutoAbilityHero\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK HERO\n"
	"  VisionRange = 200\n"
	"  CommandSet = SynthAutoAbilityCommandSet\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY SwordWeapon\n"
	"  End\n"
	"  WeaponSet\n"
	"    Conditions = WEAPONSET_TOGGLE_1\n"
	"    Weapon = PRIMARY BowWeapon\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"    InitialHealth = 100\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
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
	"    Speed = 55\n"
	"  End\n"
	"End\n";

struct Port169Rig : combattest::CombatWorld
{
	SpecialPowerStore powers;
	SpecialPowerStore *savedPowers = TheSpecialPowerStore;
	CommandStore commandStore;
	CommandStore *savedCommands = TheCommandStore;
	FXListStore fxLists;
	FXListStore *savedFx = TheFXListStore;
	AttributeModifierStore modifiers;
	AttributeModifierStore *savedModifiers = TheAttributeModifierStore;
	SpellCommands spellCommands;
	MouseSettings mouse;
	MetaMap metaMap;
	PinholeView view{ { 0.0f, -300.0f, 400.0f }, { 100.0f, 100.0f, 0.0f }, 1024, 768, 0.7f };
	std::unique_ptr<HudInput> input;
	std::unique_ptr<ControlBar> bar;
	Port169VoiceSink sink;
	UnitVoiceResponse voice;

	Port169Rig()
		: combattest::CombatWorld("", 40)
		, voice(*logic, sink)
	{
		TheSpecialPowerStore = &powers;
		TheCommandStore = &commandStore;
		TheFXListStore = &fxLists;
		TheAttributeModifierStore = &modifiers;
		commandStore.setThingFactory(&w.things);

		modifiers.registerBlock(w.fx.env.blocks);
		w.fx.env.blocks.registerBlock("FXList", [](INI *ini) { ParseFXListDefinitionGlobal(ini); });
		w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
		w.fx.env.blocks.registerBlock("CommandButton", [](INI *ini) { CommandStore::parseCommandButtonDefinitionGlobal(ini); });
		w.fx.env.blocks.registerBlock("CommandSet", [](INI *ini) { CommandStore::parseCommandSetDefinitionGlobal(ini); });

		SpecialAbilityModules::registerAll(w.modules);
		SpecialPowerModules::registerAll(w.modules);
		SpecialPowerModules::installScienceHooks(*logic);

		WeaponSetToggle::registerHandlers(dispatcher);
		HordeCommands::registerHandlers(dispatcher);
		WeaponCommands::registerHandlers(dispatcher);
		HeroAbilityModules::registerHandlers(dispatcher);
		spellCommands.registerHandlers(dispatcher);

		const std::string err = w.load(kPort169Ini, INI_LOAD_OVERWRITE, "port169.ini");
		REQUIRE_MESSAGE(err.empty(), err);

		Player *alice = players.findPlayerWithName("Alice");
		REQUIRE(alice != nullptr);
		players.setLocalPlayer(alice);
		voice.setLocalPlayerIndex(alice->getPlayerIndex());

		input = std::make_unique<HudInput>(*logic, ai.get(), view, commands, mouse, metaMap);
		input->context().commands = &commandStore;
		bar = std::make_unique<ControlBar>(input->context());
		bar->setCommandVoiceHandler([this](const std::string &sound) {
			UnitVoiceResponse::Info info;
			info.commandSound = sound;
			voice.pickAndPlay(input->ui().selected(), UnitVoiceResponse::VOICE_EVENT_STANCE, &info);
		});

		sink.known = {
			"GandalfShadowfaxWhistleMS",
			"GandalfVoiceDismountMS",
			"TransformOnceVoice",
			"SingleSoundA",
			"SingleSoundB",
			"BowModeVoice",
			"SwordModeVoice",
			"BlockFormationVoice",
			"LineFormationVoice",
			"AutoSet0",
			"AutoSet1",
			"AutoUnset0",
			"AutoUnset1",
			"AutoSetOnly0",
			"AutoUnsetOnly0",
		};
	}

	~Port169Rig()
	{
		bar.reset();
		input.reset();
		logic->reset();
		TheSpecialPowerStore = savedPowers;
		TheCommandStore = savedCommands;
		TheFXListStore = savedFx;
		TheAttributeModifierStore = savedModifiers;
	}

	void select(const std::vector<Object *> &objs)
	{
		input->ui().deselectAll(true);
		GameMessage sel(MSG_CREATE_SELECTED_GROUP, playerIndex("Alice"));
		sel.appendBooleanArgument(true);
		for (Object *o : objs)
		{
			input->ui().selectObject(o->getID());
			sel.appendObjectIDArgument(o->getID());
		}
		input->submit(sel);
		input->flush();
		frames(1);
		bar->update();
	}

	void stepAndRefresh(int n = 1)
	{
		input->flush();
		frames(n);
		bar->update();
	}

	const ControlBarButton *buttonNamed(const std::string &name) const
	{
		for (const ControlBarButton &b : bar->palantirButtons())
		{
			if (b.button && b.button->m_name == name)
			{
				return &b;
			}
		}
		return nullptr;
	}
};

constexpr int kWeaponSetMountedBit = 21; // TheWeaponConditionNames index for "MOUNTED" (RW 0xDA1328)

bool hasWeaponSetFlag(const Object &obj, int flag)
{
	const auto &f = obj.getWeaponSetFlags();
	const size_t idx = static_cast<size_t>(flag) / 32;
	const std::uint32_t mask = 1u << (static_cast<unsigned>(flag) % 32);
	return idx < f.size() && (f[idx] & mask) == mask;
}
} // namespace

TEST_CASE("PORT-169: TOGGLE_IMAGE_ON_WEAPONSET special power (Gandalf Shadowfax) plays UnitSpecificSound[0] on mount and [1] on dismount")
{
	Port169Rig rig;
	Object *gandalf = rig.unit("SynthGandalfHero", 'A', 100.0f, 100.0f);
	REQUIRE(gandalf != nullptr);
	rig.frames(4);
	rig.select({ gandalf });

	const ControlBarButton *btn = rig.buttonNamed("Command_SynthGandalfMount");
	REQUIRE(btn != nullptr);
	REQUIRE(btn->state == ButtonState::Enabled);
	CHECK(btn->image == "HSGandalfShadowfax");
	CHECK(btn->textLabel == "CONTROLBAR:GandalfMountShadowfax");
	CHECK(btn->descriptLabel == "CONTROLBAR:TipMount");
	CHECK_FALSE(hasWeaponSetFlag(*gandalf, kWeaponSetMountedBit));

	// First press (unmounted, +0xFC == 0): plays mount whistle once, then sets +0xFC = 1 immediately
	const unsigned versionUnmounted = rig.bar->version();
	rig.sink.clear();
	REQUIRE(rig.bar->pressButton(btn->slot, true));
	CHECK(rig.sink.sounds == std::vector<std::string>{ "GandalfShadowfaxWhistleMS" });
	CHECK(btn->button->getToggleIndex() == 1);

	rig.stepAndRefresh(10);
	REQUIRE(hasWeaponSetFlag(*gandalf, kWeaponSetMountedBit));
	btn = rig.buttonNamed("Command_SynthGandalfMount");
	REQUIRE(btn != nullptr);
	CHECK(btn->button->getToggleIndex() == 1);
	CHECK(btn->textLabel == "CONTROLBAR:GandalfDismountShadowfax");
	CHECK(btn->descriptLabel == "CONTROLBAR:TipDismount");
	// ControlBar::version() must bump even when both ButtonImage names are identical ("HSGandalfShadowfax")
	CHECK(rig.bar->version() != versionUnmounted);

	// Second press (mounted, +0xFC == 1): plays dismount voice once, then resets +0xFC = 0 immediately
	rig.sink.clear();
	REQUIRE(rig.bar->pressButton(btn->slot, true));
	CHECK(rig.sink.sounds == std::vector<std::string>{ "GandalfVoiceDismountMS" });
	CHECK(btn->button->getToggleIndex() == 0);

	rig.stepAndRefresh(10);
	REQUIRE_FALSE(hasWeaponSetFlag(*gandalf, kWeaponSetMountedBit));
	btn = rig.buttonNamed("Command_SynthGandalfMount");
	REQUIRE(btn != nullptr);
	CHECK(btn->textLabel == "CONTROLBAR:GandalfMountShadowfax");

	// Third press (back on foot): plays mount whistle again
	rig.sink.clear();
	REQUIRE(rig.bar->pressButton(btn->slot, true));
	CHECK(rig.sink.sounds == std::vector<std::string>{ "GandalfShadowfaxWhistleMS" });

	// Rapid second click in the same frame before logic completes: already sees inverted +0xFC == 1
	rig.sink.clear();
	REQUIRE(rig.bar->pressButton(btn->slot, true));
	CHECK(rig.sink.sounds == std::vector<std::string>{ "GandalfVoiceDismountMS" });

	// Single TextLabel with two ButtonImages and two UnitSpecificSounds (as Command_GandalfderWeieShadowfax_forEdoras):
	// label clamps cleanly to the single label while +0xFC still switches sound [0] <-> [1].
	rig.stepAndRefresh(10);
	const ControlBarButton *singleLbl = rig.buttonNamed("Command_SynthSingleLabelTwoSounds");
	REQUIRE(singleLbl != nullptr);
	CHECK(singleLbl->textLabel == "CONTROLBAR:GandalfShadowfax");
	CHECK(singleLbl->descriptLabel == "CONTROLBAR:TipShadowfax");
	rig.sink.clear();
	REQUIRE(rig.bar->pressButton(singleLbl->slot, true));
	const std::string firstSound = rig.sink.sounds.size() == 1 ? rig.sink.sounds.front() : std::string();
	rig.stepAndRefresh(10);
	singleLbl = rig.buttonNamed("Command_SynthSingleLabelTwoSounds");
	REQUIRE(singleLbl != nullptr);
	CHECK(singleLbl->textLabel == "CONTROLBAR:GandalfShadowfax");
	rig.sink.clear();
	REQUIRE(rig.bar->pressButton(singleLbl->slot, true));
	const std::string secondSound = rig.sink.sounds.size() == 1 ? rig.sink.sounds.front() : std::string();
	CHECK(firstSound != secondSound);
	CHECK(((firstSound == "GandalfShadowfaxWhistleMS" && secondSound == "GandalfVoiceDismountMS")
		|| (firstSound == "GandalfVoiceDismountMS" && secondSound == "GandalfShadowfaxWhistleMS")));
}

TEST_CASE("PORT-169: TOGGLE_WEAPONSET and 1-sound/1-image edge cases follow RW 0x75D22E and 0x75D0B9")
{
	Port169Rig rig;
	Object *horde = rig.unit("SynthSwapHorde", 'A', 120.0f, 120.0f);
	REQUIRE(horde != nullptr);
	rig.frames(4);
	rig.select({ horde });

	// 2 images, 2 labels, 2 sounds: swaps image, label, and sound together
	const ControlBarButton *swapBtn = rig.buttonNamed("Command_SynthWeaponSetSwap");
	REQUIRE(swapBtn != nullptr);
	CHECK(swapBtn->image == "ImgBow");
	CHECK(swapBtn->textLabel == "CONTROLBAR:BowMode");
	rig.sink.clear();
	REQUIRE(rig.bar->pressButton(swapBtn->slot, true));
	CHECK(rig.sink.sounds == std::vector<std::string>{ "BowModeVoice" });

	rig.stepAndRefresh(2);
	REQUIRE(hasWeaponSetFlag(*horde, WeaponSetToggle::WEAPONSET_TOGGLE_1));
	swapBtn = rig.buttonNamed("Command_SynthWeaponSetSwap");
	REQUIRE(swapBtn != nullptr);
	CHECK(swapBtn->image == "ImgSword");
	CHECK(swapBtn->textLabel == "CONTROLBAR:SwordMode");
	CHECK(swapBtn->descriptLabel == "CONTROLBAR:TipSword");

	// 1 sound with 2 images (e.g. Command_GrimbeornTransform): silent in state 1, label clamps to back()
	const ControlBarButton *oneSound = rig.buttonNamed("Command_SynthOneSoundTwoImages");
	REQUIRE(oneSound != nullptr);
	CHECK(oneSound->image == "ImgTransformB");
	CHECK(oneSound->textLabel == "CONTROLBAR:TransformSingle");
	CHECK(oneSound->descriptLabel == "CONTROLBAR:TipTransformSingle");
	rig.sink.clear();
	REQUIRE(rig.bar->pressButton(oneSound->slot, true));
	CHECK(rig.sink.sounds.empty());

	// Flush so WEAPONSET_TOGGLE_1 toggles back to state 0, then verify 1-sound 2-image plays TransformOnceVoice in state 0
	rig.stepAndRefresh(2);
	REQUIRE_FALSE(hasWeaponSetFlag(*horde, WeaponSetToggle::WEAPONSET_TOGGLE_1));
	oneSound = rig.buttonNamed("Command_SynthOneSoundTwoImages");
	REQUIRE(oneSound != nullptr);
	CHECK(oneSound->image == "ImgTransformA");
	rig.sink.clear();
	REQUIRE(rig.bar->pressButton(oneSound->slot, true));
	CHECK(rig.sink.sounds == std::vector<std::string>{ "TransformOnceVoice" });

	// Flush so WEAPONSET_TOGGLE_1 is back on (state 1); verify 1 image with 2 sounds keeps +0xFC == 0 and plays SingleSoundA
	rig.stepAndRefresh(2);
	REQUIRE(hasWeaponSetFlag(*horde, WeaponSetToggle::WEAPONSET_TOGGLE_1));
	const ControlBarButton *oneImg = rig.buttonNamed("Command_SynthOneImageTwoSounds");
	REQUIRE(oneImg != nullptr);
	CHECK(oneImg->button->getToggleIndex() == 0);
	CHECK(oneImg->image == "ImgSingleOnly");
	CHECK(oneImg->textLabel == "CONTROLBAR:SingleOnlyA");
	rig.sink.clear();
	REQUIRE(rig.bar->pressButton(oneImg->slot, true));
	CHECK(rig.sink.sounds == std::vector<std::string>{ "SingleSoundA" });

	// Flush back to state 0, toggle to state 1 with BowModeVoice, then press in state 1 -> SwordModeVoice
	rig.stepAndRefresh(2);
	REQUIRE_FALSE(hasWeaponSetFlag(*horde, WeaponSetToggle::WEAPONSET_TOGGLE_1));
	swapBtn = rig.buttonNamed("Command_SynthWeaponSetSwap");
	REQUIRE(swapBtn != nullptr);
	rig.sink.clear();
	REQUIRE(rig.bar->pressButton(swapBtn->slot, true));
	CHECK(rig.sink.sounds == std::vector<std::string>{ "BowModeVoice" });

	rig.stepAndRefresh(2);
	REQUIRE(hasWeaponSetFlag(*horde, WeaponSetToggle::WEAPONSET_TOGGLE_1));
	swapBtn = rig.buttonNamed("Command_SynthWeaponSetSwap");
	REQUIRE(swapBtn != nullptr);
	rig.sink.clear();
	REQUIRE(rig.bar->pressButton(swapBtn->slot, true));
	CHECK(rig.sink.sounds == std::vector<std::string>{ "SwordModeVoice" });
}

TEST_CASE("PORT-169: TOGGLE_IMAGE_ON_FORMATION updates image, label, multi-select vote, MappedImageCollection gate, and UnitSpecificSound")
{
	Port169Rig rig;
	Object *h1 = rig.unit("SynthFormMainHorde", 'A', 100.0f, 100.0f);
	Object *h2 = rig.unit("SynthFormMainHorde", 'A', 160.0f, 100.0f);
	Object *h3 = rig.unit("SynthFormMainHorde", 'A', 220.0f, 100.0f);
	Object *ignored = rig.unit("SynthIgnoredProp", 'A', 260.0f, 100.0f);
	REQUIRE((h1 && h2 && h3 && ignored));
	rig.frames(4);

	// 1. Single-select main -> alt -> main
	rig.select({ h1 });
	const ControlBarButton *fbtn = rig.buttonNamed("Command_SynthFormationToggle");
	const ControlBarButton *noImg = rig.buttonNamed("Command_SynthFormationNoImage");
	REQUIRE((fbtn && noImg));
	CHECK(fbtn->image == "ImgShieldBlock");
	CHECK(fbtn->textLabel == "CONTROLBAR:ToggleBlockFormation");
	CHECK(noImg->image.empty());
	CHECK(noImg->button->getToggleIndex() == 0);

	rig.sink.clear();
	REQUIRE(rig.bar->pressButton(fbtn->slot, true));
	CHECK(rig.sink.sounds == std::vector<std::string>{ "BlockFormationVoice" });
	CHECK(fbtn->button->getToggleIndex() == 1);

	rig.stepAndRefresh(2);
	fbtn = rig.buttonNamed("Command_SynthFormationToggle");
	noImg = rig.buttonNamed("Command_SynthFormationNoImage");
	REQUIRE((fbtn && noImg));
	CHECK(fbtn->image == "ImgShieldLine");
	CHECK(fbtn->textLabel == "CONTROLBAR:ToggleLineFormation");
	CHECK(fbtn->descriptLabel == "CONTROLBAR:TipLine");
	// 0-image formation button remains at +0xFC == 0
	CHECK(noImg->button->getToggleIndex() == 0);

	rig.sink.clear();
	REQUIRE(rig.bar->pressButton(noImg->slot, true));
	CHECK(rig.sink.sounds == std::vector<std::string>{ "BlockFormationVoice" });

	// Bring h1 back into main formation for the multi-select vote checks
	rig.stepAndRefresh(2);
	fbtn = rig.buttonNamed("Command_SynthFormationToggle");
	REQUIRE(fbtn != nullptr);
	CHECK(fbtn->image == "ImgShieldBlock");

	// Put h3 alone into alternate formation (h1=main, h2=main, h3=alt)
	rig.select({ h3 });
	fbtn = rig.buttonNamed("Command_SynthFormationToggle");
	REQUIRE(fbtn != nullptr);
	REQUIRE(rig.bar->pressButton(fbtn->slot, true));
	rig.stepAndRefresh(2);

	// 2. Multi-select vote RW 0x944603: 2 main (-2) + 1 alt (+1) => vote = -1 < 0 => +0xFC == 0
	rig.select({ h1, h2, h3 });
	fbtn = rig.buttonNamed("Command_SynthFormationToggle");
	REQUIRE(fbtn != nullptr);
	CHECK(fbtn->button->getToggleIndex() == 0);
	CHECK(fbtn->image == "ImgShieldBlock");
	rig.sink.clear();
	REQUIRE(rig.bar->pressButton(fbtn->slot, true));
	CHECK(rig.sink.sounds == std::vector<std::string>{ "BlockFormationVoice" });

	// Now put h2 + h3 in alt, h1 in main, then test 1 main + 1 alt tie (vote = 0 >= 0 => +0xFC == 1)
	rig.stepAndRefresh(2);
	// After group formation toggle, all 3 are in alt; put h1 alone back into main
	rig.select({ h1 });
	fbtn = rig.buttonNamed("Command_SynthFormationToggle");
	REQUIRE(fbtn != nullptr);
	REQUIRE(rig.bar->pressButton(fbtn->slot, true));
	rig.stepAndRefresh(2);

	// Mark h3 as SOLD (status 0x13) and include IGNORED_IN_GUI prop: RW 0x94463D..0x944651 ignores both,
	// so selecting [h1 (main), ignored, h3 (SOLD alt)] votes -1 => +0xFC == 0 (vs +1 without SOLD)!
	const int soldBit = ObjectTemplateInfoBuilder::objectStatusIndex("SOLD");
	REQUIRE(soldBit != -1);
	h3->setStatus(static_cast<unsigned>(soldBit), true);
	rig.select({ h1, ignored, h3 });
	fbtn = rig.buttonNamed("Command_SynthFormationToggle");
	REQUIRE(fbtn != nullptr);
	CHECK(fbtn->button->getToggleIndex() == 0);
	h3->setStatus(static_cast<unsigned>(soldBit), false);

	// 1 main (h1) + 1 alt (h2): vote == 0 >= 0 => +0xFC == 1 => LineFormationVoice
	rig.select({ h1, h2 });
	fbtn = rig.buttonNamed("Command_SynthFormationToggle");
	REQUIRE(fbtn != nullptr);
	CHECK(fbtn->button->getToggleIndex() == 1);
	CHECK(fbtn->image == "ImgShieldLine");
	rig.sink.clear();
	REQUIRE(rig.bar->pressButton(fbtn->slot, true));
	CHECK(rig.sink.sounds == std::vector<std::string>{ "LineFormationVoice" });

	// 3. MappedImageCollection filtering (RW 0x75D3E9): when only 1 of 2 ButtonImage entries resolves, +0xFC stays 0 even on alt h3
	MappedImageCollection partialImages;
	Image img;
	img.name = "ImgShieldBlock";
	partialImages.addImage(img);
	rig.input->context().images = &partialImages;
	rig.select({ h3 });
	fbtn = rig.buttonNamed("Command_SynthFormationToggle");
	REQUIRE(fbtn != nullptr);
	CHECK(fbtn->button->getToggleIndex() == 0);
	CHECK(fbtn->image == "ImgShieldBlock");
	rig.sink.clear();
	REQUIRE(rig.bar->pressButton(fbtn->slot, true));
	CHECK(rig.sink.sounds == std::vector<std::string>{ "BlockFormationVoice" });
	rig.input->context().images = nullptr;
}

TEST_CASE("PORT-169: TOGGLE_NO_AUTO_ACQUIRE image toggle and rightPressButton Set/UnsetAutoAbilityUnitSound follow +0xFC")
{
	Port169Rig rig;

	// 1. TOGGLE_IMAGE_ON_WEAPON + TOGGLE_NO_AUTO_ACQUIRE (RW 0x75D2CF / 0x941367)
	Object *horde = rig.unit("SynthSwapHorde", 'A', 120.0f, 120.0f);
	REQUIRE(horde != nullptr);
	rig.frames(4);
	rig.select({ horde });

	const ControlBarButton *acq = rig.buttonNamed("Command_SynthToggleNoAutoAcquire");
	REQUIRE(acq != nullptr);
	CHECK(acq->image == "ImgAutoAcquireOn");
	CHECK(acq->textLabel == "CONTROLBAR:AcquireOn");
	REQUIRE(rig.bar->pressButton(acq->slot, true));
	CHECK(acq->button->getToggleIndex() == 1);

	rig.stepAndRefresh(2);
	acq = rig.buttonNamed("Command_SynthToggleNoAutoAcquire");
	REQUIRE(acq != nullptr);
	CHECK(acq->image == "ImgAutoAcquireOff");
	CHECK(acq->textLabel == "CONTROLBAR:AcquireOff");
	CHECK(acq->descriptLabel == "CONTROLBAR:TipAcquireOff");

	// 2. rightPressButton SetAutoAbilityUnitSound (RW 0x75D0F8) and UnsetAutoAbilityUnitSound (RW 0x75D137)
	Object *hero = rig.unit("SynthAutoAbilityHero", 'A', 200.0f, 200.0f);
	REQUIRE(hero != nullptr);
	rig.frames(4);
	rig.select({ hero });

	const ControlBarButton *autoTwo = rig.buttonNamed("Command_SynthAutoAbilityToggle");
	const ControlBarButton *autoOne = rig.buttonNamed("Command_SynthAutoAbilityOneSound");
	REQUIRE((autoTwo && autoOne));
	CHECK_FALSE(autoTwo->autoAbility);

	// State 0: Set -> AutoSet0, Unset -> AutoUnset0
	rig.sink.clear();
	REQUIRE(rig.bar->rightPressButton(autoTwo->slot, true));
	CHECK(rig.sink.sounds == std::vector<std::string>{ "AutoSet0" });
	rig.stepAndRefresh(2);
	autoTwo = rig.buttonNamed("Command_SynthAutoAbilityToggle");
	REQUIRE(autoTwo != nullptr);
	CHECK(autoTwo->autoAbility);
	rig.sink.clear();
	REQUIRE(rig.bar->rightPressButton(autoTwo->slot, true));
	CHECK(rig.sink.sounds == std::vector<std::string>{ "AutoUnset0" });
	rig.stepAndRefresh(2);

	// Flip hero WEAPONSET_TOGGLE_1 so +0xFC == 1
	hero->setWeaponSetFlag(WeaponSetToggle::WEAPONSET_TOGGLE_1, true);
	rig.stepAndRefresh(2);
	autoTwo = rig.buttonNamed("Command_SynthAutoAbilityToggle");
	autoOne = rig.buttonNamed("Command_SynthAutoAbilityOneSound");
	REQUIRE((autoTwo && autoOne));
	CHECK(autoTwo->button->getToggleIndex() == 1);
	CHECK(autoOne->button->getToggleIndex() == 1);

	// State 1: Set -> AutoSet1, Unset -> AutoUnset1; 1-sound button stays silent at +0xFC == 1
	rig.sink.clear();
	REQUIRE(rig.bar->rightPressButton(autoTwo->slot, true));
	CHECK(rig.sink.sounds == std::vector<std::string>{ "AutoSet1" });
	rig.stepAndRefresh(2);
	autoTwo = rig.buttonNamed("Command_SynthAutoAbilityToggle");
	REQUIRE(autoTwo != nullptr);
	CHECK(autoTwo->autoAbility);
	rig.sink.clear();
	REQUIRE(rig.bar->rightPressButton(autoTwo->slot, true));
	CHECK(rig.sink.sounds == std::vector<std::string>{ "AutoUnset1" });
	rig.stepAndRefresh(2);

	autoOne = rig.buttonNamed("Command_SynthAutoAbilityOneSound");
	REQUIRE(autoOne != nullptr);
	rig.sink.clear();
	REQUIRE(rig.bar->rightPressButton(autoOne->slot, true));
	CHECK(rig.sink.sounds.empty());
}
