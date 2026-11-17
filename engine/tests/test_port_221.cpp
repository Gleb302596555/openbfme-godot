// OpenBFME. PORT-221 (issue #531, P-140): abilities and weapon toggles across multi-selected hordes
// and mixed selections, matching RotWK 2.01:
//   - ControlBar::processCommandUI case GUI_COMMAND_SPECIAL_POWER (RW 0x9414E7..0x941618) appends one
//     MSG_DO_SPECIAL_POWER { int powerID, int options, ObjectID 0, ObjectID 0 } (and one per chained
//     SPECIAL_POWER CommandTrigger at +0x188) with source = INVALID_ID (0), so SpellCommands::doSpecialPower
//     (RW 0x77A42C -> AIGroup::groupDoSpecialPower RW 0x76F5DB) activates every ready selected unit when
//     COMMAND_OPTION_OK_FOR_MULTI_EXECUTE is set (including Ennorath Imladris Noldor Mastery buffs and
//     Wind Riders' WeaponChangeSpecialPowerModule toggle).
//   - Targeted GUI_COMMAND_SPECIAL_POWER (RW 0x940877 -> GUICommandTranslator -> CommandTranslator::
//     issueSpecialPowerCommand RW 0x81E147 -> AIGroup::groupDoSpecialPowerAtLocation RW 0x76F774 /
//     groupDoSpecialPowerAtObject RW 0x77097E) and GUI_COMMAND_TOGGLE_WEAPONSET (RW 0x9412D7 ->
//     MSG_WEAPONSET_TOGGLE RW 0x77B529) activate all ready / template-equivalent selected hordes.
//   - ControlBar multi-select context (evaluateContextUI RW 0x71ECC4 -> populateMultiSelect RW 0x94475A,
//     addCommonCommands RW 0x944534, updateContextMultiSelect RW 0x944911): senior ranked HERO delegation
//     (RW 0x94478A..0x944839), common-slot OK_FOR_MULTI_SELECT intersection with GUI_COMMAND_ATTACK_MOVE
//     union (RW 0x9446DE), and enabled state whenever at least one selected unit has the command ready
//     (RW 0x944AE7..0x944B73).
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
#include "GameClient/HudInput.h"
#include "GameClient/TacticalView.h"
#include "GameLogic/AI/WeaponCommands.h"
#include "GameLogic/AttributeModifiers.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/ExperienceLevels.h"
#include "GameLogic/GameMessage.h"
#include "GameLogic/Module/HeroAbilityModules.h"
#include "GameLogic/Module/SpecialAbilityModules.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Module/StancesBehavior.h"
#include "GameLogic/Module/WeaponChangeSpecialPower.h"
#include "GameLogic/Object/Contain/HordeContainRuntime.h"
#include "GameLogic/Object/ExperienceTracker.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/SpellCommands.h"
#include "GameLogic/System/ShroudManager.h"
#include "GameLogic/WeaponSetToggle.h"

#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

namespace
{
const char kPort221Ini[] =
	"ModifierList P221MasteryBuff\n"
	"  Category = LEADERSHIP\n"
	"  Modifier = ARMOR 25%\n"
	"  Duration = 5000\n"
	"End\n"
	"ModifierList P221HeroBuff\n"
	"  Category = SPELL\n"
	"  Modifier = DAMAGE_MULT 150%\n"
	"  Duration = 5000\n"
	"End\n"
	"SpecialPower SpecialAbilityP221NoldorMastery\n"
	"  Enum = SPECIAL_HERO_MODE\n"
	"  ReloadTime = 10000\n"
	"End\n"
	"SpecialPower SpecialAbilityP221ChainedEcho\n"
	"  Enum = SPECIAL_GENERAL_TARGETLESS\n"
	"  ReloadTime = 10000\n"
	"End\n"
	"SpecialPower SpecialAbilityP221WindRiderToggle\n"
	"  Enum = SPECIAL_GENERAL_TARGETLESS\n"
	"  ReloadTime = 1000\n"
	"End\n"
	"SpecialPower SpecialAbilityP221TargetedVolley\n"
	"  Enum = SPECIAL_ARROW_STORM\n"
	"  ReloadTime = 10000\n"
	"End\n"
	// an object-targeted power: RotWK casts at an object only for the types RW 0x82DAE9 lets through (0x81 AT_VISIBLE_OBJECT; GENERAL_TARGETLESS is refused)
	"SpecialPower SpecialAbilityP221TargetedStrike\n"
	"  Enum = SPECIAL_AT_VISIBLE_OBJECT\n"
	"  ReloadTime = 10000\n"
	"End\n"
	"SpecialPower SpecialAbilityP221HeroCry\n"
	"  Enum = SPECIAL_GENERAL_TARGETLESS\n"
	"  ReloadTime = 10000\n"
	"End\n"
	"ExperienceLevel P221HeroLevel1\n"
	"  TargetNames = P221ImladrisHero\n"
	"  RequiredExperience = 1\n"
	"  Rank = 1\n"
	"End\n"
	"ExperienceLevel P221HeroLevel2\n"
	"  TargetNames = P221ImladrisHero\n"
	"  RequiredExperience = 50\n"
	"  Rank = 2\n"
	"End\n"
	"StanceTemplate P221StanceTemplate\n"
	"  Stance Battle\n"
	"  End\n"
	"  Stance HoldGround\n"
	"  End\n"
	"  Stance Aggressive\n"
	"  End\n"
	"End\n"
	"Locomotor P221FootLoco\n"
	"  Surfaces = GROUND\n"
	"  Speed = 60\n"
	"  TurnTime = 200\n"
	"  TurnTimeDamaged = 200\n"
	"  Acceleration = 500\n"
	"  Braking = 500\n"
	"  Appearance = TWO_LEGS\n"
	"  StickToGround = Yes\n"
	"End\n"
	"Weapon P221Sword\n"
	"  AttackRange = 30\n"
	"  MeleeWeapon = Yes\n"
	"  DelayBetweenShots = 800\n"
	"  DamageNugget\n"
	"    Damage = 20\n"
	"    DamageType = SLASH\n"
	"    DeathType = NORMAL\n"
	"  End\n"
	"End\n"
	"Weapon P221Bow\n"
	"  AttackRange = 300\n"
	"  DelayBetweenShots = 900\n"
	"  DamageNugget\n"
	"    Damage = 18\n"
	"    DamageType = PIERCE\n"
	"    DeathType = NORMAL\n"
	"  End\n"
	"End\n"
	"Weapon P221VolleyWeapon\n"
	"  AttackRange = 400\n"
	"  RadiusDamageAffects = ENEMIES\n"
	"  DamageNugget\n"
	"    Damage = 25\n"
	"    Radius = 40\n"
	"    DamageType = PIERCE\n"
	"    DeathType = NORMAL\n"
	"  End\n"
	"End\n"
	// Common multi-select buttons
	"CommandButton Command_P221_ToggleStance\n"
	"  Command = TOGGLE_STANCE\n"
	"  Options = OK_FOR_MULTI_SELECT\n"
	"  Stances = HoldGround Battle Aggressive\n"
	"  ButtonImage = ImgHold ImgBattle ImgAggro\n"
	"  InPalantir = Yes\n"
	"End\n"
	"CommandButton Command_P221_AttackMove\n"
	"  Command = ATTACK_MOVE\n"
	"  Options = OK_FOR_MULTI_SELECT\n"
	"  ButtonImage = ImgAttackMove\n"
	"  InPalantir = No\n"
	"End\n"
	"CommandButton Command_P221_Stop\n"
	"  Command = STOP\n"
	"  Options = OK_FOR_MULTI_SELECT\n"
	"  ButtonImage = ImgStop\n"
	"  InPalantir = No\n"
	"End\n"
	// Mirrors Ennorath Command_NoldorMasteryAbilitySword (SPECIAL_POWER, OK_FOR_MULTI_SELECT OK_FOR_MULTI_EXECUTE)
	"CommandButton Command_P221_NoldorMasterySword\n"
	"  Command = SPECIAL_POWER\n"
	"  SpecialPower = SpecialAbilityP221NoldorMastery\n"
	"  Options = OK_FOR_MULTI_SELECT OK_FOR_MULTI_EXECUTE\n"
	"  ButtonImage = ImgNoldorSword\n"
	"  InPalantir = Yes\n"
	"End\n"
	// Chained CommandTrigger SPECIAL_POWER (RW 0x94157D..0x94160F)
	"CommandButton Command_P221_ChainedEcho\n"
	"  Command = SPECIAL_POWER\n"
	"  SpecialPower = SpecialAbilityP221ChainedEcho\n"
	"  Options = OK_FOR_MULTI_SELECT OK_FOR_MULTI_EXECUTE\n"
	"  ButtonImage = ImgEcho\n"
	"  InPalantir = No\n"
	"End\n"
	"CommandButton Command_P221_MasteryWithTrigger\n"
	"  Command = SPECIAL_POWER\n"
	"  SpecialPower = SpecialAbilityP221NoldorMastery\n"
	"  Options = OK_FOR_MULTI_SELECT OK_FOR_MULTI_EXECUTE\n"
	"  CommandTrigger = Command_P221_ChainedEcho\n"
	"  ButtonImage = ImgMasteryTrig\n"
	"  InPalantir = Yes\n"
	"End\n"
	// Position-targeted SPECIAL_POWER (NEED_TARGET_POS OK_FOR_MULTI_SELECT OK_FOR_MULTI_EXECUTE)
	"CommandButton Command_P221_TargetedVolley\n"
	"  Command = SPECIAL_POWER\n"
	"  SpecialPower = SpecialAbilityP221TargetedVolley\n"
	"  Options = NEED_TARGET_POS OK_FOR_MULTI_SELECT OK_FOR_MULTI_EXECUTE\n"
	"  ButtonImage = ImgVolley\n"
	"  InPalantir = Yes\n"
	"End\n"
	// Object-targeted SPECIAL_POWER (NEED_TARGET_ENEMY_OBJECT OK_FOR_MULTI_SELECT OK_FOR_MULTI_EXECUTE)
	"CommandButton Command_P221_TargetedStrike\n"
	"  Command = SPECIAL_POWER\n"
	"  SpecialPower = SpecialAbilityP221TargetedStrike\n"
	"  Options = NEED_TARGET_ENEMY_OBJECT OK_FOR_MULTI_SELECT OK_FOR_MULTI_EXECUTE\n"
	"  ButtonImage = ImgStrike\n"
	"  InPalantir = Yes\n"
	"End\n"
	// Mirrors Ennorath Command_WindRidersWeaponToggle (SPECIAL_POWER -> WeaponChangeSpecialPowerModule)
	"CommandButton Command_P221_WindRidersWeaponToggle\n"
	"  Command = SPECIAL_POWER\n"
	"  SpecialPower = SpecialAbilityP221WindRiderToggle\n"
	"  Options = TOGGLE_IMAGE_ON_WEAPONSET OK_FOR_MULTI_SELECT OK_FOR_MULTI_EXECUTE\n"
	"  FlagsUsedForToggle = WEAPONSET_TOGGLE_1\n"
	"  ButtonImage = ImgLance ImgBow\n"
	"  InPalantir = Yes\n"
	"End\n"
	// Mirrors Ennorath Command_ToggleElvenWeaponSwitchUnit (TOGGLE_WEAPONSET)
	"CommandButton Command_P221_ToggleWatcherWeapon\n"
	"  Command = TOGGLE_WEAPONSET\n"
	"  Options = TOGGLE_IMAGE_ON_WEAPONSET OK_FOR_MULTI_SELECT\n"
	"  FlagsUsedForToggle = WEAPONSET_TOGGLE_1\n"
	"  ButtonImage = ImgWatcherBow ImgWatcherSword\n"
	"  InPalantir = Yes\n"
	"End\n"
	// Porter build / non-multi-select command
	"CommandButton Command_P221_PorterSingleOnly\n"
	"  Command = STOP\n"
	"  ButtonImage = ImgPorterOnly\n"
	"  InPalantir = Yes\n"
	"End\n"
	// Hero single-execute power (no OK_FOR_MULTI_EXECUTE)
	"CommandButton Command_P221_HeroCry\n"
	"  Command = SPECIAL_POWER\n"
	"  SpecialPower = SpecialAbilityP221HeroCry\n"
	"  ButtonImage = ImgHeroCry\n"
	"  InPalantir = Yes\n"
	"End\n"
	// Sub-range back button past InitialVisible carrying OK_FOR_MULTI_SELECT (mirrors Command_RadialBack)
	"CommandButton Command_P221_RadialBack\n"
	"  Command = POP_VISIBLE_COMMAND_RANGE\n"
	"  Options = OK_FOR_MULTI_SELECT\n"
	"  Radial = Yes\n"
	"  ButtonImage = ImgBack\n"
	"  InPalantir = No\n"
	"End\n"
	"CommandSet P221SwordsmenHordeSet\n"
	"  InitialVisible = 14\n"
	"  1 = Command_P221_ToggleStance\n"
	"  2 = Command_P221_NoldorMasterySword\n"
	"  3 = Command_P221_TargetedVolley\n"
	"  4 = Command_P221_TargetedStrike\n"
	"  5 = Command_P221_MasteryWithTrigger\n"
	"  13 = Command_P221_AttackMove\n"
	"  14 = Command_P221_Stop\n"
	"  15 = Command_P221_RadialBack\n"
	"End\n"
	"CommandSet P221WindRiderHordeSet\n"
	"  InitialVisible = 14\n"
	"  1 = Command_P221_ToggleStance\n"
	"  2 = Command_P221_WindRidersWeaponToggle\n"
	"  13 = Command_P221_AttackMove\n"
	"  14 = Command_P221_Stop\n"
	"End\n"
	"CommandSet P221WatcherHordeSet\n"
	"  InitialVisible = 14\n"
	"  1 = Command_P221_ToggleStance\n"
	"  2 = Command_P221_ToggleWatcherWeapon\n"
	"  13 = Command_P221_AttackMove\n"
	"  14 = Command_P221_Stop\n"
	"End\n"
	"CommandSet P221PorterSet\n"
	"  InitialVisible = 14\n"
	"  1 = Command_P221_Stop\n"
	"  2 = Command_P221_PorterSingleOnly\n"
	"  14 = Command_P221_Stop\n"
	"End\n"
	"CommandSet P221HeroSet\n"
	"  1 = Command_P221_ToggleStance\n"
	"  2 = Command_P221_HeroCry\n"
	"End\n"
	"Object P221SwordsmenHorde\n"
	"  KindOf = PRELOAD SELECTABLE CAN_ATTACK INFANTRY HORDE\n"
	"  VisionRange = 350\n"
	"  CommandSet = P221SwordsmenHordeSet\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 12\n"
	"  GeometryHeight = 20\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY P221Sword\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = P221FootLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 60\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 400\n"
	"    InitialHealth = 400\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = PhysicsBehavior ModuleTag_Phys\n"
	"  End\n"
	"  Behavior = StancesBehavior ModuleTag_Stance\n"
	"    StanceTemplate = P221StanceTemplate\n"
	"  End\n"
	"  Behavior = SpecialPowerModule ModuleTag_MasteryPower\n"
	"    SpecialPowerTemplate = SpecialAbilityP221NoldorMastery\n"
	"    UpdateModuleStartsAttack = No\n"
	"    AttributeModifier = P221MasteryBuff\n"
	"    AttributeModifierAffectsSelf = Yes\n"
	"  End\n"
	"  Behavior = SpecialPowerModule ModuleTag_ChainedEcho\n"
	"    SpecialPowerTemplate = SpecialAbilityP221ChainedEcho\n"
	"    UpdateModuleStartsAttack = No\n"
	"  End\n"
	"  Behavior = SpecialPowerModule ModuleTag_VolleyStarter\n"
	"    SpecialPowerTemplate = SpecialAbilityP221TargetedVolley\n"
	"    UpdateModuleStartsAttack = Yes\n"
	"  End\n"
	"  Behavior = WeaponFireSpecialAbilityUpdate ModuleTag_VolleyUpdate\n"
	"    SpecialPowerTemplate = SpecialAbilityP221TargetedVolley\n"
	"    SpecialWeapon = P221VolleyWeapon\n"
	"    StartAbilityRange = 380\n"
	"    UnpackTime = 100\n"
	"    PackTime = 100\n"
	"  End\n"
	"  Behavior = SpecialPowerModule ModuleTag_StrikeStarter\n"
	"    SpecialPowerTemplate = SpecialAbilityP221TargetedStrike\n"
	"    UpdateModuleStartsAttack = Yes\n"
	"  End\n"
	"  Behavior = WeaponFireSpecialAbilityUpdate ModuleTag_StrikeUpdate\n"
	"    SpecialPowerTemplate = SpecialAbilityP221TargetedStrike\n"
	"    SpecialWeapon = P221VolleyWeapon\n"
	"    StartAbilityRange = 380\n"
	"    UnpackTime = 100\n"
	"    PackTime = 100\n"
	"  End\n"
	"End\n"
	"Object P221WindRiderHorde\n"
	"  KindOf = PRELOAD SELECTABLE CAN_ATTACK CAVALRY HORDE\n"
	"  VisionRange = 350\n"
	"  CommandSet = P221WindRiderHordeSet\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 14\n"
	"  GeometryHeight = 22\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY P221Sword\n"
	"  End\n"
	"  WeaponSet\n"
	"    Conditions = WEAPONSET_TOGGLE_1\n"
	"    Weapon = PRIMARY P221Bow\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = P221FootLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 80\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"    InitialHealth = 500\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = PhysicsBehavior ModuleTag_Phys\n"
	"  End\n"
	"  Behavior = StancesBehavior ModuleTag_Stance\n"
	"    StanceTemplate = P221StanceTemplate\n"
	"  End\n"
	"  Behavior = WeaponChangeSpecialPowerModule ModuleTag_WeaponChange\n"
	"    SpecialPowerTemplate = SpecialAbilityP221WindRiderToggle\n"
	"    FlagsUsedForToggle = WEAPONSET_TOGGLE_1\n"
	"  End\n"
	"  Behavior = HordeContain ModuleTag_Contain\n"
	"    RankInfo = RankNumber:1 UnitType:P221WatcherFighter Position:X:0 Y:0 Position:X:10 Y:10\n"
	"    InitialPayload = P221WatcherFighter 2\n"
	"  End\n"
	"End\n"
	"Object P221WatcherFighter\n"
	"  KindOf = PRELOAD SELECTABLE CAN_ATTACK INFANTRY\n"
	"  VisionRange = 350\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 6\n"
	"  GeometryHeight = 18\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY P221Bow\n"
	"  End\n"
	"  WeaponSet\n"
	"    Conditions = WEAPONSET_TOGGLE_1\n"
	"    Weapon = PRIMARY P221Sword\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = P221FootLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 60\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"    InitialHealth = 100\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = PhysicsBehavior ModuleTag_Phys\n"
	"  End\n"
	"End\n"
	"Object P221WatcherHorde\n"
	"  KindOf = PRELOAD SELECTABLE CAN_ATTACK INFANTRY HORDE\n"
	"  VisionRange = 350\n"
	"  CommandSet = P221WatcherHordeSet\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 12\n"
	"  GeometryHeight = 20\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY P221Bow\n"
	"  End\n"
	"  WeaponSet\n"
	"    Conditions = WEAPONSET_TOGGLE_1\n"
	"    Weapon = PRIMARY P221Sword\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = P221FootLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 60\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 400\n"
	"    InitialHealth = 400\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = PhysicsBehavior ModuleTag_Phys\n"
	"  End\n"
	"  Behavior = StancesBehavior ModuleTag_Stance\n"
	"    StanceTemplate = P221StanceTemplate\n"
	"  End\n"
	"  Behavior = HordeContain ModuleTag_Contain\n"
	"    RankInfo = RankNumber:1 UnitType:P221WatcherFighter Position:X:0 Y:0 Position:X:10 Y:10\n"
	"    InitialPayload = P221WatcherFighter 2\n"
	"  End\n"
	"End\n"
	"Object P221Porter\n"
	"  KindOf = PRELOAD SELECTABLE DOZER INFANTRY\n"
	"  VisionRange = 200\n"
	"  CommandSet = P221PorterSet\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryHeight = 16\n"
	"  LocomotorSet\n"
	"    Locomotor = P221FootLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 50\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 200\n"
	"    InitialHealth = 200\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = PhysicsBehavior ModuleTag_Phys\n"
	"  End\n"
	"End\n"
	"Object P221ImladrisHero\n"
	"  KindOf = PRELOAD SELECTABLE CAN_ATTACK HERO INFANTRY\n"
	"  VisionRange = 350\n"
	"  CommandSet = P221HeroSet\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryHeight = 20\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY P221Sword\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = P221FootLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 65\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 800\n"
	"    InitialHealth = 800\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = PhysicsBehavior ModuleTag_Phys\n"
	"  End\n"
	"  Behavior = StancesBehavior ModuleTag_Stance\n"
	"    StanceTemplate = P221StanceTemplate\n"
	"  End\n"
	"  Behavior = SpecialPowerModule ModuleTag_HeroCry\n"
	"    SpecialPowerTemplate = SpecialAbilityP221HeroCry\n"
	"    UpdateModuleStartsAttack = No\n"
	"    AttributeModifier = P221HeroBuff\n"
	"    AttributeModifierAffectsSelf = Yes\n"
	"  End\n"
	"End\n"
	"Object P221EnemyTarget\n"
	"  KindOf = PRELOAD SELECTABLE CAN_ATTACK INFANTRY\n"
	"  VisionRange = 200\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 12\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 600\n"
	"    InitialHealth = 600\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = PhysicsBehavior ModuleTag_Phys\n"
	"  End\n"
	"End\n";

struct Port221Rig : combattest::CombatWorld
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
	ExperienceLevelSystem levels;
	ExperienceLevelSystem *savedLevels = TheExperienceLevelSystem;
	ShroudManager shroud{ *logic };
	SpellCommands spellCommands;
	MouseSettings mouse;
	MetaMap metaMap;
	PinholeView view{ { 150.0f, -250.0f, 420.0f }, { 150.0f, 150.0f, 0.0f }, 1024, 768, 0.75f };
	std::unique_ptr<HudInput> input;
	std::unique_ptr<ControlBar> bar;

	Port221Rig()
		: combattest::CombatWorld("", 40)
	{
		TheSpecialPowerStore = &powers;
		TheCommandStore = &commandStore;
		TheFXListStore = &fxLists;
		TheAttributeModifierStore = &modifiers;
		TheStanceTemplateStore = &stanceTemplates;
		TheExperienceLevelSystem = &levels;
		commandStore.setThingFactory(&w.things);

		modifiers.registerBlock(w.fx.env.blocks);
		stanceTemplates.registerBlock(w.fx.env.blocks);
		levels.registerBlocks(w.fx.env.blocks);
		w.fx.env.blocks.registerBlock("FXList", [](INI *ini) { ParseFXListDefinitionGlobal(ini); });
		w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
		w.fx.env.blocks.registerBlock("CommandButton", [](INI *ini) { CommandStore::parseCommandButtonDefinitionGlobal(ini); });
		w.fx.env.blocks.registerBlock("CommandSet", [](INI *ini) { CommandStore::parseCommandSetDefinitionGlobal(ini); });

		SpecialAbilityModules::registerAll(w.modules);
		SpecialPowerModules::registerAll(w.modules);
		WeaponChangeSpecialPowerModule::registerClass(w.modules);
		SpecialPowerModules::installScienceHooks(*logic);
		StancesBehavior::registerClass(w.modules);

		WeaponSetToggle::registerHandlers(dispatcher);
		HordeCommands::registerHandlers(dispatcher);
		WeaponCommands::registerHandlers(dispatcher);
		HeroAbilityModules::registerHandlers(dispatcher);
		StancesBehavior::registerHandlers(dispatcher);
		spellCommands.registerHandlers(dispatcher);

		combat().setAutoAcquireEnabled(false);

		const std::string err = w.load(kPort221Ini, INI_LOAD_OVERWRITE, "port221.ini");
		REQUIRE_MESSAGE(err.empty(), err);

		Player *alice = players.findPlayerWithName("Alice");
		REQUIRE(alice != nullptr);
		players.setLocalPlayer(alice);

		shroud.init(40.0f, 1);
		shroud.setExtent(0.0f, 0.0f, 2000.0f, 2000.0f);
		shroud.attach();
		logic->setShroud(&shroud);
		shroud.revealMapForPlayerPermanently(alice->getPlayerIndex());

		input = std::make_unique<HudInput>(*logic, ai.get(), view, commands, mouse, metaMap);
		input->context().commands = &commandStore;
		bar = std::make_unique<ControlBar>(input->context());
	}

	~Port221Rig()
	{
		bar.reset();
		input.reset();
		logic->setShroud(nullptr);
		logic->reset();
		TheSpecialPowerStore = savedPowers;
		TheCommandStore = savedCommands;
		TheFXListStore = savedFx;
		TheAttributeModifierStore = savedModifiers;
		TheStanceTemplateStore = savedStances;
		TheExperienceLevelSystem = savedLevels;
	}

	Object *spawn(const char *name, float x, float y, char side = 'A')
	{
		Object *obj = unit(name, side, x, y);
		REQUIRE(obj != nullptr);
		return obj;
	}

	void selectGroup(std::initializer_list<Object *> objs)
	{
		input->ui().setGUICommand(nullptr);
		std::vector<ObjectID> ids;
		GameMessage sel(MSG_CREATE_SELECTED_GROUP, playerIndex("Alice"));
		sel.appendBooleanArgument(true);
		for (Object *obj : objs)
		{
			if (obj)
			{
				ids.push_back(obj->getID());
				sel.appendObjectIDArgument(obj->getID());
			}
		}
		input->ui().mirrorSelection(ids);
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

	void stepLogic(int n = 1)
	{
		input->flush();
		frames(n);
		bar->update();
	}

	SpecialPowerModuleInterface *powerOf(Object &obj, const char *powerName) const
	{
		const SpecialPowerTemplate *t = powers.findSpecialPowerTemplate(powerName);
		REQUIRE(t != nullptr);
		SpecialPowerModuleInterface *sp = SpecialPowerModules::findModule(obj, t);
		REQUIRE(sp != nullptr);
		return sp;
	}

	void readyPower(Object &obj, const char *powerName)
	{
		powerOf(obj, powerName)->setReadyFrame(logic->getFrame());
	}

	static bool hasWeaponToggle1(const Object &obj)
	{
		static const int kBit = CombatNames::weaponSetBit("WEAPONSET_TOGGLE_1");
		REQUIRE(kBit != -1);
		return ((obj.getWeaponSetFlags()[(size_t)kBit >> 5] >> (kBit & 31)) & 1u) != 0;
	}

	void leftClickWorld(const Coord3D &worldPos)
	{
		ICoord2D screen{};
		REQUIRE(view.worldToScreen(worldPos, screen));
		input->mouseMove(screen.x, screen.y);
		input->mouseButton(HudInput::Button::Left, true, screen.x, screen.y, 0, 1000);
		input->mouseButton(HudInput::Button::Left, false, screen.x, screen.y, 0, 1010);
	}
};
} // namespace

TEST_CASE("PORT-221: no-target SPECIAL_POWER activates all two and three selected hordes and starts every reload (RW 0x9414E7..0x941618 -> RW 0x76F5DB)")
{
	Port221Rig r;
	Object *h1 = r.spawn("P221SwordsmenHorde", 100.0f, 100.0f);
	Object *h2 = r.spawn("P221SwordsmenHorde", 130.0f, 100.0f);
	Object *h3 = r.spawn("P221SwordsmenHorde", 160.0f, 100.0f);

	for (Object *h : { h1, h2, h3 })
	{
		r.readyPower(*h, "SpecialAbilityP221NoldorMastery");
		r.readyPower(*h, "SpecialAbilityP221ChainedEcho");
	}

	SUBCASE("two identical hordes: single press sends one MSG_DO_SPECIAL_POWER with source INVALID_ID and starts both reloads")
	{
		r.selectGroup({ h1, h2 });
		const ControlBarButton *btn = r.buttonNamed("Command_P221_NoldorMasterySword");
		REQUIRE(btn != nullptr);
		CHECK(btn->state == ButtonState::Enabled);
		// Sub-range button past InitialVisible (slot 15) must not leak into multi-select
		CHECK(r.buttonNamed("Command_P221_RadialBack") == nullptr);

		REQUIRE(r.bar->pressButton(btn->slot, btn->inPalantir));
		r.input->flush();
		// Check exact RW 0x94154F..0x941578 wire contract before logic consumes CommandList: 1 message, 4 args, source=0, toggled=0
		const auto &queued = r.commands.messages();
		REQUIRE(queued.size() == 1);
		CHECK(queued[0].getType() == MSG_DO_SPECIAL_POWER);
		CHECK(queued[0].getArgumentCount() == 4);
		if (queued[0].getArgumentCount() >= 3)
		{
			CHECK(queued[0].getArgument(2)->objectID == INVALID_ID);
		}
		if (queued[0].getArgumentCount() >= 4)
		{
			CHECK(queued[0].getArgument(3)->objectID == INVALID_ID);
		}

		r.stepLogic(2);
		CHECK_FALSE(r.powerOf(*h1, "SpecialAbilityP221NoldorMastery")->isReady());
		CHECK_FALSE(r.powerOf(*h2, "SpecialAbilityP221NoldorMastery")->isReady());
		CHECK(r.powerOf(*h3, "SpecialAbilityP221NoldorMastery")->isReady());

		btn = r.buttonNamed("Command_P221_NoldorMasterySword");
		REQUIRE(btn != nullptr);
		CHECK(btn->state == ButtonState::NotReady);
	}

	SUBCASE("three identical hordes and chained CommandTrigger: all three hordes fire both powers")
	{
		r.selectGroup({ h1, h2, h3 });
		const ControlBarButton *btn = r.buttonNamed("Command_P221_MasteryWithTrigger");
		REQUIRE(btn != nullptr);
		CHECK(btn->state == ButtonState::Enabled);

		REQUIRE(r.bar->pressButton(btn->slot, btn->inPalantir));
		r.input->flush();
		const auto &queued = r.commands.messages();
		REQUIRE(queued.size() == 2);
		for (const GameMessage &m : queued)
		{
			CHECK(m.getType() == MSG_DO_SPECIAL_POWER);
			CHECK(m.getArgumentCount() == 4);
			if (m.getArgumentCount() >= 3)
			{
				CHECK(m.getArgument(2)->objectID == INVALID_ID);
			}
			if (m.getArgumentCount() >= 4)
			{
				CHECK(m.getArgument(3)->objectID == INVALID_ID);
			}
		}

		r.stepLogic(2);
		for (Object *h : { h1, h2, h3 })
		{
			CHECK_FALSE(r.powerOf(*h, "SpecialAbilityP221NoldorMastery")->isReady());
			CHECK_FALSE(r.powerOf(*h, "SpecialAbilityP221ChainedEcho")->isReady());
		}
	}

	SUBCASE("partial readiness (RW 0x944911..0x944B73): when h1 is cooling down and h2 is ready, button stays Enabled and fires h2")
	{
		// Put h1 on cooldown first
		r.selectGroup({ h1 });
		const ControlBarButton *singleBtn = r.buttonNamed("Command_P221_NoldorMasterySword");
		REQUIRE(singleBtn != nullptr);
		REQUIRE(r.bar->pressButton(singleBtn->slot, singleBtn->inPalantir));
		r.stepLogic(2);
		REQUIRE_FALSE(r.powerOf(*h1, "SpecialAbilityP221NoldorMastery")->isReady());
		REQUIRE(r.powerOf(*h2, "SpecialAbilityP221NoldorMastery")->isReady());

		// Now multi-select {h1 (cooling down), h2 (ready)}: RW updateContextMultiSelect enables slot since availCount == 1 > 0
		r.selectGroup({ h1, h2 });
		const ControlBarButton *multiBtn = r.buttonNamed("Command_P221_NoldorMasterySword");
		REQUIRE(multiBtn != nullptr);
		CHECK(multiBtn->state == ButtonState::Enabled);
		REQUIRE(r.bar->pressButton(multiBtn->slot, multiBtn->inPalantir));
		r.stepLogic(2);
		CHECK_FALSE(r.powerOf(*h1, "SpecialAbilityP221NoldorMastery")->isReady());
		CHECK_FALSE(r.powerOf(*h2, "SpecialAbilityP221NoldorMastery")->isReady());
	}
}

TEST_CASE("PORT-221: position-targeted and object-targeted SPECIAL_POWER activate all two and three selected hordes (RW 0x81E147 -> RW 0x76F774 / 0x77097E)")
{
	Port221Rig r;
	Object *h1 = r.spawn("P221SwordsmenHorde", 100.0f, 100.0f);
	Object *h2 = r.spawn("P221SwordsmenHorde", 130.0f, 100.0f);
	Object *h3 = r.spawn("P221SwordsmenHorde", 160.0f, 100.0f);
	Object *enemy = r.spawn("P221EnemyTarget", 150.0f, 220.0f, 'B');

	for (Object *h : { h1, h2, h3 })
	{
		r.readyPower(*h, "SpecialAbilityP221TargetedVolley");
		r.readyPower(*h, "SpecialAbilityP221TargetedStrike");
	}

	SUBCASE("two and three hordes: NEED_TARGET_POS starts every selected horde")
	{
		r.selectGroup({ h1, h2 });
		const ControlBarButton *btn = r.buttonNamed("Command_P221_TargetedVolley");
		REQUIRE(btn != nullptr);
		REQUIRE(r.bar->pressButton(btn->slot, btn->inPalantir));
		REQUIRE(r.input->ui().getGUICommand() == btn->button);

		r.leftClickWorld(Coord3D{ 130.0f, 220.0f, 0.0f });
		r.stepLogic(10);
		CHECK_FALSE(r.powerOf(*h1, "SpecialAbilityP221TargetedVolley")->isReady());
		CHECK_FALSE(r.powerOf(*h2, "SpecialAbilityP221TargetedVolley")->isReady());
		CHECK(r.powerOf(*h3, "SpecialAbilityP221TargetedVolley")->isReady());

		// Reset and verify with all three hordes
		for (Object *h : { h1, h2, h3 })
		{
			r.readyPower(*h, "SpecialAbilityP221TargetedVolley");
		}
		r.selectGroup({ h1, h2, h3 });
		btn = r.buttonNamed("Command_P221_TargetedVolley");
		REQUIRE(btn != nullptr);
		REQUIRE(r.bar->pressButton(btn->slot, btn->inPalantir));
		r.leftClickWorld(Coord3D{ 140.0f, 220.0f, 0.0f });
		r.stepLogic(10);
		for (Object *h : { h1, h2, h3 })
		{
			CHECK_FALSE(r.powerOf(*h, "SpecialAbilityP221TargetedVolley")->isReady());
		}
	}

	SUBCASE("two and three hordes: NEED_TARGET_ENEMY_OBJECT starts every selected horde")
	{
		r.selectGroup({ h1, h2, h3 });
		const ControlBarButton *btn = r.buttonNamed("Command_P221_TargetedStrike");
		REQUIRE(btn != nullptr);
		REQUIRE(r.bar->pressButton(btn->slot, btn->inPalantir));
		REQUIRE(r.input->ui().getGUICommand() == btn->button);

		r.leftClickWorld(*enemy->getPosition());
		r.stepLogic(10);
		for (Object *h : { h1, h2, h3 })
		{
			CHECK_FALSE(r.powerOf(*h, "SpecialAbilityP221TargetedStrike")->isReady());
		}
	}
}

TEST_CASE("PORT-221: weapon toggle switches all two and three selected hordes for both WeaponChangeSpecialPowerModule and TOGGLE_WEAPONSET")
{
	Port221Rig r;

	SUBCASE("Imladris Wind Riders pattern (SPECIAL_POWER -> WeaponChangeSpecialPowerModule): two and three hordes all switch")
	{
		Object *w1 = r.spawn("P221WindRiderHorde", 100.0f, 120.0f);
		Object *w2 = r.spawn("P221WindRiderHorde", 130.0f, 120.0f);
		Object *w3 = r.spawn("P221WindRiderHorde", 160.0f, 120.0f);
		for (Object *w : { w1, w2, w3 })
		{
			r.readyPower(*w, "SpecialAbilityP221WindRiderToggle");
			REQUIRE_FALSE(Port221Rig::hasWeaponToggle1(*w));
		}

		// Two hordes: switch Lance -> Bow
		r.selectGroup({ w1, w2 });
		const ControlBarButton *btn = r.buttonNamed("Command_P221_WindRidersWeaponToggle");
		REQUIRE(btn != nullptr);
		CHECK(btn->state == ButtonState::Enabled);
		REQUIRE(r.bar->pressButton(btn->slot, btn->inPalantir));
		r.stepLogic(2);
		CHECK(Port221Rig::hasWeaponToggle1(*w1));
		CHECK(Port221Rig::hasWeaponToggle1(*w2));
		CHECK_FALSE(Port221Rig::hasWeaponToggle1(*w3));
		CHECK_FALSE(r.powerOf(*w1, "SpecialAbilityP221WindRiderToggle")->isReady());
		CHECK_FALSE(r.powerOf(*w2, "SpecialAbilityP221WindRiderToggle")->isReady());

		// Ready all three in same initial state (off) and toggle three hordes on, then off
		r.readyPower(*w1, "SpecialAbilityP221WindRiderToggle");
		r.readyPower(*w2, "SpecialAbilityP221WindRiderToggle");
		r.selectGroup({ w1, w2 });
		btn = r.buttonNamed("Command_P221_WindRidersWeaponToggle");
		REQUIRE(btn != nullptr);
		REQUIRE(r.bar->pressButton(btn->slot, btn->inPalantir));
		r.stepLogic(2);
		for (Object *w : { w1, w2, w3 })
		{
			r.readyPower(*w, "SpecialAbilityP221WindRiderToggle");
			REQUIRE_FALSE(Port221Rig::hasWeaponToggle1(*w));
		}

		r.selectGroup({ w1, w2, w3 });
		btn = r.buttonNamed("Command_P221_WindRidersWeaponToggle");
		REQUIRE(btn != nullptr);
		REQUIRE(r.bar->pressButton(btn->slot, btn->inPalantir));
		r.stepLogic(2);
		for (Object *w : { w1, w2, w3 })
		{
			CHECK(Port221Rig::hasWeaponToggle1(*w));
			CHECK_FALSE(r.powerOf(*w, "SpecialAbilityP221WindRiderToggle")->isReady());
		}
	}

	SUBCASE("Lindon Watchers pattern (GUI_COMMAND_TOGGLE_WEAPONSET -> MSG_WEAPONSET_TOGGLE RW 0x77B529): two and three hordes all switch")
	{
		Object *v1 = r.spawn("P221WatcherHorde", 100.0f, 160.0f);
		Object *v2 = r.spawn("P221WatcherHorde", 130.0f, 160.0f);
		Object *v3 = r.spawn("P221WatcherHorde", 160.0f, 160.0f);

		r.selectGroup({ v1, v2, v3 });
		const ControlBarButton *btn = r.buttonNamed("Command_P221_ToggleWatcherWeapon");
		REQUIRE(btn != nullptr);
		REQUIRE(r.bar->pressButton(btn->slot, btn->inPalantir));
		r.stepLogic(2);
		for (Object *v : { v1, v2, v3 })
		{
			CHECK(Port221Rig::hasWeaponToggle1(*v));
		}

		btn = r.buttonNamed("Command_P221_ToggleWatcherWeapon");
		REQUIRE(btn != nullptr);
		REQUIRE(r.bar->pressButton(btn->slot, btn->inPalantir));
		r.stepLogic(2);
		for (Object *v : { v1, v2, v3 })
		{
			CHECK_FALSE(Port221Rig::hasWeaponToggle1(*v));
		}
	}
}

TEST_CASE("PORT-221: mixed selection with one object lacking the button matches RotWK HUD intersection, senior-hero delegation, and logic filtering")
{
	Port221Rig r;
	Object *s1 = r.spawn("P221SwordsmenHorde", 100.0f, 100.0f);
	Object *s2 = r.spawn("P221SwordsmenHorde", 130.0f, 100.0f);
	Object *w1 = r.spawn("P221WatcherHorde", 160.0f, 100.0f);
	Object *porter = r.spawn("P221Porter", 190.0f, 100.0f);
	r.readyPower(*s1, "SpecialAbilityP221NoldorMastery");
	r.readyPower(*s2, "SpecialAbilityP221NoldorMastery");

	SUBCASE("HUD addCommonCommands (RW 0x944534): unshared ability/toggle and non-OK_FOR_MULTI_SELECT buttons hide in either selection order")
	{
		// {SwordsmenHorde, Porter}: Swordsmen's slot-2 ability and Porter's non-OK_FOR_MULTI_SELECT slot-2 button are both absent;
		// shared slot-14 Command_P221_Stop remains enabled.
		r.selectGroup({ s1, porter });
		CHECK(r.buttonNamed("Command_P221_NoldorMasterySword") == nullptr);
		CHECK(r.buttonNamed("Command_P221_PorterSingleOnly") == nullptr);
		const ControlBarButton *stopA = r.buttonNamed("Command_P221_Stop");
		REQUIRE(stopA != nullptr);
		CHECK(stopA->state == ButtonState::Enabled);

		// Reverse order {Porter, SwordsmenHorde}: unshared abilities remain absent, shared slot-14 Stop remains, and
		// GUI_COMMAND_ATTACK_MOVE at slot 13 is unioned in from the second object (RW 0x9446DE).
		r.selectGroup({ porter, s1 });
		CHECK(r.buttonNamed("Command_P221_NoldorMasterySword") == nullptr);
		CHECK(r.buttonNamed("Command_P221_PorterSingleOnly") == nullptr);
		CHECK(r.buttonNamed("Command_P221_Stop") != nullptr);
		CHECK(r.buttonNamed("Command_P221_AttackMove") != nullptr);

		// {SwordsmenHorde, WatcherHorde}: both share slot-1 ToggleStance, slot-13 AttackMove, slot-14 Stop, while slot-2 differs
		r.selectGroup({ s1, w1 });
		CHECK(r.buttonNamed("Command_P221_NoldorMasterySword") == nullptr);
		CHECK(r.buttonNamed("Command_P221_ToggleWatcherWeapon") == nullptr);
		CHECK(r.buttonNamed("Command_P221_ToggleStance") != nullptr);
		CHECK(r.buttonNamed("Command_P221_AttackMove") != nullptr);
		CHECK(r.buttonNamed("Command_P221_Stop") != nullptr);
	}

	SUBCASE("HUD populateMultiSelect senior hero rule (RW 0x94478A..0x944839): highest-rank HERO wins regardless of selection order")
	{
		Object *heroLow = r.spawn("P221ImladrisHero", 220.0f, 100.0f);
		Object *heroHigh = r.spawn("P221ImladrisHero", 250.0f, 100.0f);
		heroLow->getExperienceTracker()->setExperienceAndLevel(1.0f, false);   // rank 1
		heroHigh->getExperienceTracker()->setExperienceAndLevel(50.0f, false); // rank 2
		r.readyPower(*heroLow, "SpecialAbilityP221HeroCry");
		r.readyPower(*heroHigh, "SpecialAbilityP221HeroCry");

		// Select {porter, s1, heroLow, heroHigh}: senior hero (rank 2) populates the bar even though porter was selected first
		r.selectGroup({ porter, s1, heroLow, heroHigh });
		CHECK(r.bar->sourceObject() == heroHigh->getID());
		const ControlBarButton *cry = r.buttonNamed("Command_P221_HeroCry");
		REQUIRE(cry != nullptr);
		CHECK(cry->state == ButtonState::Enabled);

		// Without OK_FOR_MULTI_EXECUTE, pressing the hero button casts on the senior hero only (RW 0x76F60A..0x76F693)
		REQUIRE(r.bar->pressButton(cry->slot, cry->inPalantir));
		r.stepLogic(2);
		CHECK_FALSE(r.powerOf(*heroHigh, "SpecialAbilityP221HeroCry")->isReady());
		CHECK(r.powerOf(*heroLow, "SpecialAbilityP221HeroCry")->isReady());
	}

	SUBCASE("logic group execution over mixed selection {s1, porter, s2} and {w1, porter}: matching hordes execute, lacking unit is skipped")
	{
		// Group MSG_DO_SPECIAL_POWER (OK_FOR_MULTI_EXECUTE) over {s1, porter, s2}
		r.selectGroup({ s1, porter, s2 });
		const SpecialPowerTemplate *mastery = r.powers.findSpecialPowerTemplate("SpecialAbilityP221NoldorMastery");
		REQUIRE(mastery != nullptr);
		GameMessage cast(MSG_DO_SPECIAL_POWER, r.playerIndex("Alice"));
		cast.appendIntegerArgument((int)mastery->getID());
		cast.appendIntegerArgument((int)(COMMAND_OPTION_OK_FOR_MULTI_SELECT | COMMAND_OPTION_OK_FOR_MULTI_EXECUTE));
		cast.appendObjectIDArgument(INVALID_ID);
		cast.appendObjectIDArgument(INVALID_ID);
		r.input->submit(cast);
		r.stepLogic(2);
		CHECK_FALSE(r.powerOf(*s1, "SpecialAbilityP221NoldorMastery")->isReady());
		CHECK_FALSE(r.powerOf(*s2, "SpecialAbilityP221NoldorMastery")->isReady());
		CHECK_FALSE(porter->isDestroyed());

		// Group MSG_WEAPONSET_TOGGLE over {w1, s1, porter}: only template-equivalent Watcher horde switches (RW 0x77B529 -> 0x73D5C2)
		Object *w2 = r.spawn("P221WatcherHorde", 280.0f, 100.0f);
		r.selectGroup({ w1, s1, porter, w2 });
		GameMessage ws(MSG_WEAPONSET_TOGGLE, r.playerIndex("Alice"));
		ws.appendObjectIDArgument(w1->getID());
		r.input->submit(ws);
		r.stepLogic(2);
		CHECK(Port221Rig::hasWeaponToggle1(*w1));
		CHECK(Port221Rig::hasWeaponToggle1(*w2));
		CHECK_FALSE(Port221Rig::hasWeaponToggle1(*s1));
		CHECK_FALSE(Port221Rig::hasWeaponToggle1(*porter));
	}
}
