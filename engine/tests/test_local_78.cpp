// OpenBFME tests for Local 78 (issue #576):
//   Rules in SweepEffectProbe.h covering all 138 NO-RULE patterns from the Ennorath button sweep:
//     - WeaponFireSpecialAbilityUpdate nuggets: AttributeModifierNugget (direct, warhead, multi-modifier),
//       EmotionWeaponNugget, LuaEventNugget, MetaImpactNugget, StealMoneyNugget, ParalyzeNugget,
//       SpecialModelConditionNugget, ProjectileNugget CreateObjectDie OCL, FireLogicNugget exclusion,
//       HordeAttackNugget (horde slot resolution vs non-horde caster NO-EFFECT)
//     - SpecialAbilityUpdate subclasses & enum specials: ModelConditionSpecialAbilityUpdate (+ GenerateTerror),
//       ToggleDeploySpecialAbilityUpdate, SpecialDisguiseUpdate, ReplaceObjectUpdate,
//       SPECIAL_ATHELAS / SPECIAL_ELVEN_GRACE (AutoHealBehavior), ActivateModuleSpecialPower chain
//     - SpecialPowerModule subclasses & enum specials: InvisibilitySpecialPower,
//       SpecialPowerTimerRefreshSpecialPower, SPECIAL_HARVEST
//
// Applies to both RotWK 2.01 (RW) and BFME2 1.06 rule sets unless a cited rule differs.

#include "doctest.h"
#include "CombatTestUtil.h"
#include "SweepEffectProbe.h"

#include "Common/GameEdition.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/SpecialPower.h"
#include "GameClient/FXList.h"
#include "GameLogic/AttributeModifiers.h"
#include "GameLogic/Combat/CombatState.h"
#include "GameLogic/Combat/ObjectWeapons.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/GameLogicDispatch.h"
#include "GameLogic/Module/HeroAbilityModules.h"
#include "GameLogic/Module/HeroModules.h"
#include "GameLogic/Module/ReplaceObjectUpdate.h"
#include "GameLogic/Module/SpecialAbilityModules.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Object/AttributeModifierPool.h"
#include "GameLogic/Object/Contain/HordeContainRuntime.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/ObjectCreationList.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/WeaponStores.h"

#include <string>
#include <vector>

using namespace sweep_probe;

TEST_SUITE_BEGIN("LOCAL-78");

namespace
{

struct ProbeWorld : combattest::CombatWorld
{
	SpecialPowerStore powers;
	SpecialPowerStore *savedPowers = TheSpecialPowerStore;
	FXListStore fxLists;
	FXListStore *savedFx = TheFXListStore;
	AttributeModifierStore modifiers;
	AttributeModifierStore *savedModifiers = TheAttributeModifierStore;
	ObjectCreationListStore oclStore;
	ObjectCreationListStore *savedOcl = TheObjectCreationListStore;

	explicit ProbeWorld(const char *extraIni)
		: combattest::CombatWorld("", 100)
		, oclStore(w.keys)
	{
		logic->setEdition(GameEdition::RotWK201);
		logic->combat().setAutoAcquireEnabled(false);

		TheSpecialPowerStore = &powers;
		TheFXListStore = &fxLists;
		TheAttributeModifierStore = &modifiers;
		TheObjectCreationListStore = &oclStore;

		modifiers.registerBlock(w.fx.env.blocks);
		w.fx.env.blocks.registerBlock("FXList", [](INI *ini) { ParseFXListDefinitionGlobal(ini); });
		w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
		w.fx.env.blocks.registerBlock("ObjectCreationList", [](INI *ini) { ObjectCreationListStore::parseObjectCreationListDefinitionGlobal(ini); });

		SpecialAbilityModules::registerAll(w.modules);
		SpecialPowerModules::registerAll(w.modules);
		SpecialPowerModules::installScienceHooks(*logic);
		HeroAbilityModules::registerHandlers(dispatcher);

		const std::string err = w.load(extraIni, INI_LOAD_OVERWRITE, "local78.ini");
		REQUIRE_MESSAGE(err.empty(), err);
	}

	~ProbeWorld()
	{
		logic->reset();
		TheObjectCreationListStore = savedOcl;
		TheAttributeModifierStore = savedModifiers;
		TheFXListStore = savedFx;
		TheSpecialPowerStore = savedPowers;
	}

	const SpecialPowerTemplate *powerTpl(const char *name) const
	{
		const SpecialPowerTemplate *t = powers.findSpecialPowerTemplate(name);
		REQUIRE(t != nullptr);
		return t;
	}

	void triggerAt(Object *caster, const char *powerName, Object *target, const Coord3D &loc)
	{
		const SpecialPowerTemplate *t = powerTpl(powerName);
		if (SpecialPowerUpdateInterface *up = SpecialAbilityModules::findUpdate(*caster, t))
		{
			up->initiateIntentToDoSpecialPower(t, target, &loc, 0, 0);
			return;
		}
		if (SpecialPowerModuleInterface *sp = SpecialPowerModules::findModule(*caster, t))
		{
			if (target)
			{
				sp->doSpecialPowerAtObject(target, 0);
			}
			else
			{
				sp->doSpecialPowerAtLocation(loc, 0);
			}
		}
	}
};

void trackFor(ProbeWorld &f, HitTracker &hits, int framesCount)
{
	hits.track(*f.logic);
	for (int i = 0; i < framesCount; ++i)
	{
		f.frames(1);
		hits.track(*f.logic);
	}
}

#define L78_UNIT(name, extra)                                                                                                                    \
	"Object " name "\n"                                                                                                                          \
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK HERO\n"                                                                                           \
	"  Geometry = CYLINDER\n  GeometryMajorRadius = 8\n  GeometryMinorRadius = 8\n  GeometryHeight = 20\n"                                       \
	"  Body = ActiveBody BodyTag\n    MaxHealth = 500\n    InitialHealth = 500\n  End\n"                                                         \
	"  Behavior = AIUpdateInterface AiTag\n  End\n"                                                                                              \
	"  LocomotorSet\n    Locomotor = WalkerLoco\n    Condition = SET_NORMAL\n    Speed = 55\n  End\n"                                            \
	"  Behavior = AttributeModifierPoolUpdate PoolTag\n  End\n"                                                                                  \
	"  Behavior = EmotionTrackerUpdate EmotionTag\n  End\n" extra "End\n"

const char kLocal78Ini[] =
	"ModifierList L78_ModOne\n  Category = LEADERSHIP\n  Duration = 30000\n  Modifier = ARMOR 50%\nEnd\n"
	"ModifierList L78_ModTwo\n  Category = SPELL\n  Duration = 30000\n  Modifier = DAMAGE_MULT 120%\nEnd\n"
	"Upgrade Upg_Bonus\n  Type = OBJECT\nEnd\n"
	"SpecialPower SP_UpgFilter\n  Enum = SPECIAL_AT_VISIBLE_OBJECT\n  ReloadTime = 1000\nEnd\n"
	"SpecialPower SP_OnlyMonster\n  Enum = SPECIAL_AT_VISIBLE_OBJECT\n  ReloadTime = 1000\nEnd\n"
	"Weapon W_UpgFilterBaseWarhead\n  RadiusDamageAffects = ENEMIES\n  DamageNugget\n    Damage = 50\n    DamageType = HERO\n  End\nEnd\n"
	"Weapon W_UpgFilterBonusWarhead\n  RadiusDamageAffects = ENEMIES\n  AttributeModifierNugget\n    AttributeModifier = L78_ModOne\n    Radius = 150\n  End\nEnd\n"
	"Weapon W_UpgFilter\n  AttackRange = 150\n  RadiusDamageAffects = ENEMIES\n"
	"  ProjectileNugget\n    ProjectileTemplateName = Arrow\n    WarheadTemplateName = W_UpgFilterBaseWarhead\n    ForbiddenUpgradeNames = Upg_Bonus\n  End\n"
	"  ProjectileNugget\n    ProjectileTemplateName = Arrow\n    WarheadTemplateName = W_UpgFilterBonusWarhead\n    RequiredUpgradeNames = Upg_Bonus\n  End\n"
	"  EmotionWeaponNugget\n    EmotionType = TERROR\n    Radius = 50\n    Duration = 500\n    SpecialObjectFilter = NONE +MONSTER\n  End\n"
	"End\n"
	"Weapon W_OnlyMonster\n  AttackRange = 150\n  RadiusDamageAffects = ENEMIES\n  ParalyzeNugget\n    Radius = 50\n    Duration = 1000\n    SpecialObjectFilter = NONE +MONSTER\n  End\nEnd\n"
	"ObjectCreationList L78_OCLVision\n"
	"  CreateObject\n    ObjectNames = L78_VisionDummy\n    Count = 1\n  End\n"
	"End\n"
	"SpecialPower SP_DirectMod\n  Enum = SPECIAL_GENERAL_TARGETLESS\n  ReloadTime = 1000\nEnd\n"
	"SpecialPower SP_ProjMod\n  Enum = SPECIAL_GENERAL_TARGETLESS\n  ReloadTime = 1000\nEnd\n"
	"SpecialPower SP_MultiMod\n  Enum = SPECIAL_GENERAL_TARGETLESS\n  ReloadTime = 1000\nEnd\n"
	"SpecialPower SP_Emotion\n  Enum = SPECIAL_GENERAL_TARGETLESS\n  ReloadTime = 1000\nEnd\n"
	"SpecialPower SP_Lua\n  Enum = SPECIAL_GENERAL_TARGETLESS\n  ReloadTime = 1000\nEnd\n"
	"SpecialPower SP_Meta\n  Enum = SPECIAL_GENERAL_TARGETLESS\n  ReloadTime = 1000\nEnd\n"
	"SpecialPower SP_Steal\n  Enum = SPECIAL_AT_VISIBLE_OBJECT\n  ReloadTime = 1000\nEnd\n"
	"SpecialPower SP_Paralyze\n  Enum = SPECIAL_AT_VISIBLE_OBJECT\n  ReloadTime = 1000\nEnd\n"
	"SpecialPower SP_SMC\n  Enum = SPECIAL_AT_VISIBLE_OBJECT\n  ReloadTime = 1000\nEnd\n"
	"SpecialPower SP_ProjDie\n  Enum = SPECIAL_AT_VISIBLE_OBJECT\n  ReloadTime = 1000\nEnd\n"
	"SpecialPower SP_FireLogicOnly\n  Enum = SPECIAL_GENERAL_TARGETLESS\n  ReloadTime = 1000\nEnd\n"
	"SpecialPower SP_HordeShot\n  Enum = SPECIAL_AT_VISIBLE_OBJECT\n  ReloadTime = 1000\nEnd\n"
	"SpecialPower SP_MC\n  Enum = SPECIAL_GENERAL_TARGETLESS\n  ReloadTime = 1000\nEnd\n"
	"SpecialPower SP_Deploy\n  Enum = SPECIAL_GENERAL_TARGETLESS\n  ReloadTime = 1000\nEnd\n"
	"SpecialPower SP_Disguise\n  Enum = SPECIAL_GENERAL_TARGETLESS\n  ReloadTime = 1000\nEnd\n"
	"SpecialPower SP_Replace\n  Enum = SPECIAL_SCREECH\n  ReloadTime = 1000\nEnd\n"
	"SpecialPower SP_Invis\n  Enum = SPECIAL_ARROW_STORM\n  ReloadTime = 1000\nEnd\n"
	"SpecialPower SP_Refresh\n  Enum = SPECIAL_GENERAL_TARGETLESS\n  ReloadTime = 1000\nEnd\n"
	"SpecialPower SP_Harvest\n  Enum = SPECIAL_HARVEST\n  ReloadTime = 1000\nEnd\n"
	"SpecialPower SP_Athelas\n  Enum = SPECIAL_ATHELAS\n  ReloadTime = 1000\nEnd\n"
	"SpecialPower SP_ChainStart\n  Enum = SPECIAL_ARROW_STORM\n  ReloadTime = 1000\nEnd\n"
	"SpecialPower SP_ChainSub\n  Enum = SPECIAL_GENERAL_TARGETLESS\n  ReloadTime = 1000\nEnd\n"
	"Weapon W_DirectMod\n  AttackRange = 150\n  RadiusDamageAffects = ENEMIES\n  AttributeModifierNugget\n    AttributeModifier = L78_ModOne\n    Radius = 150\n  End\nEnd\n"
	"Weapon W_ProjModWarhead\n  RadiusDamageAffects = ENEMIES\n  AttributeModifierNugget\n    AttributeModifier = L78_ModOne\n    Radius = 150\n  End\nEnd\n"
	"Weapon W_ProjMod\n  AttackRange = 150\n  WeaponSpeed = 200\n  ProjectileNugget\n    ProjectileTemplateName = Arrow\n    WarheadTemplateName = W_ProjModWarhead\n  End\nEnd\n"
	"Weapon W_MultiMod\n  AttackRange = 150\n  RadiusDamageAffects = ALLIES ENEMIES\n"
	"  AttributeModifierNugget\n    AttributeModifier = L78_ModOne\n    Radius = 150\n    SpecialObjectFilter = NONE +INFANTRY\n  End\n"
	"  AttributeModifierNugget\n    AttributeModifier = L78_ModTwo\n    Radius = 150\n    SpecialObjectFilter = NONE +STRUCTURE\n  End\nEnd\n"
	"Weapon W_Emotion\n  AttackRange = 150\n  RadiusDamageAffects = ENEMIES\n  EmotionWeaponNugget\n    EmotionType = TERROR\n    Radius = 150\n    Duration = 5000\n  End\nEnd\n"
	"Weapon W_Lua\n  AttackRange = 150\n  RadiusDamageAffects = ENEMIES\n  LuaEventNugget\n    LuaEvent = BeTerrified\n    Radius = 150\n    SendToEnemies = Yes\n  End\nEnd\n"
	"Weapon W_Meta\n  AttackRange = 150\n  RadiusDamageAffects = ENEMIES\n  MetaImpactNugget\n    ShockWaveAmount = 30\n    ShockWaveRadius = 150\n  End\nEnd\n"
	"Weapon W_Steal\n  AttackRange = 150\n  RadiusDamageAffects = ENEMIES\n  StealMoneyNugget\n    AmountStolenPerAttack = 500\n  End\nEnd\n"
	"Weapon W_Paralyze\n  AttackRange = 150\n  RadiusDamageAffects = ENEMIES\n  ParalyzeNugget\n    Radius = 150\n    Duration = 3000\n  End\nEnd\n"
	"Weapon W_SMC\n  AttackRange = 150\n  RadiusDamageAffects = ENEMIES\n  SpecialModelConditionNugget\n    ModelConditionNames = USER_1\n    ModelConditionDuration = 2000\n  End\nEnd\n"
	"Weapon W_ProjDieWarhead\n  RadiusDamageAffects = ENEMIES\nEnd\n"
	"Weapon W_ProjDie\n  AttackRange = 150\n  WeaponSpeed = 200\n  ProjectileNugget\n    ProjectileTemplateName = L78_DieProj\n    WarheadTemplateName = W_ProjDieWarhead\n  End\nEnd\n"
	"Weapon W_FireLogicOnly\n  AttackRange = 150\n  FireLogicNugget\n    LogicType = INCREASE_FUEL_ON_EXISTING_FIRE\n  End\nEnd\n"
	"Weapon W_HordeFinder\n  AttackRange = 200\n  HordeAttackNugget\n    LockWeaponSlot = QUINARY\n  End\nEnd\n"
	"Weapon W_HordeQuinary\n  AttackRange = 250\n  WeaponSpeed = 200\n  DamageNugget\n    Damage = 40\n    DamageType = PIERCE\n    DeathType = NORMAL\n  End\nEnd\n"
	"Weapon W_ChainBlast\n  AttackRange = 200\n  RadiusDamageAffects = ENEMIES\n  DamageNugget\n    Damage = 50\n    Radius = 100\n    DamageType = MAGIC\n    DeathType = NORMAL\n  End\n  MetaImpactNugget\n    ShockWaveAmount = 20\n    ShockWaveRadius = 100\n  End\nEnd\n"
	"Object L78_VisionDummy\n  KindOf = NO_COLLIDE INERT\n  Body = ActiveBody BodyTag\n    MaxHealth = 10\n  End\nEnd\n"
	"Object L78_DieProj\n  KindOf = PROJECTILE\n  Body = ActiveBody BodyTag\n    MaxHealth = 1\n  End\n  Behavior = BezierProjectileBehavior ProjTag\n    FirstHeight = 10\n    SecondHeight = 10\n    FirstPercentIndent = 30%\n    SecondPercentIndent = 70%\n  End\n  Behavior = CreateObjectDie DieTag\n    CreationList = L78_OCLVision\n  End\nEnd\n"
	L78_UNIT("L78_WeaponCaster",
		"  Behavior = SpecialPowerModule PDirectMod\n    SpecialPowerTemplate = SP_DirectMod\n    UpdateModuleStartsAttack = Yes\n  End\n"
		"  Behavior = WeaponFireSpecialAbilityUpdate UDirectMod\n    SpecialPowerTemplate = SP_DirectMod\n    SpecialWeapon = W_DirectMod\n    UnpackTime = 0\n    PreparationTime = 0\n    PackTime = 0\n    IgnoreFacingCheck = Yes\n  End\n"
		"  Behavior = SpecialPowerModule PProjMod\n    SpecialPowerTemplate = SP_ProjMod\n    UpdateModuleStartsAttack = Yes\n  End\n"
		"  Behavior = WeaponFireSpecialAbilityUpdate UProjMod\n    SpecialPowerTemplate = SP_ProjMod\n    SpecialWeapon = W_ProjMod\n    UnpackTime = 0\n    PreparationTime = 0\n    PackTime = 0\n    IgnoreFacingCheck = Yes\n  End\n"
		"  Behavior = SpecialPowerModule PMultiMod\n    SpecialPowerTemplate = SP_MultiMod\n    UpdateModuleStartsAttack = Yes\n  End\n"
		"  Behavior = WeaponFireSpecialAbilityUpdate UMultiMod\n    SpecialPowerTemplate = SP_MultiMod\n    SpecialWeapon = W_MultiMod\n    UnpackTime = 0\n    PreparationTime = 0\n    PackTime = 0\n    IgnoreFacingCheck = Yes\n  End\n"
		"  Behavior = SpecialPowerModule PEmotion\n    SpecialPowerTemplate = SP_Emotion\n    UpdateModuleStartsAttack = Yes\n  End\n"
		"  Behavior = WeaponFireSpecialAbilityUpdate UEmotion\n    SpecialPowerTemplate = SP_Emotion\n    SpecialWeapon = W_Emotion\n    UnpackTime = 0\n    PreparationTime = 0\n    PackTime = 0\n    IgnoreFacingCheck = Yes\n  End\n"
		"  Behavior = SpecialPowerModule PLua\n    SpecialPowerTemplate = SP_Lua\n    UpdateModuleStartsAttack = Yes\n  End\n"
		"  Behavior = WeaponFireSpecialAbilityUpdate ULua\n    SpecialPowerTemplate = SP_Lua\n    SpecialWeapon = W_Lua\n    UnpackTime = 0\n    PreparationTime = 0\n    PackTime = 0\n    IgnoreFacingCheck = Yes\n  End\n"
		"  Behavior = SpecialPowerModule PMeta\n    SpecialPowerTemplate = SP_Meta\n    UpdateModuleStartsAttack = Yes\n  End\n"
		"  Behavior = WeaponFireSpecialAbilityUpdate UMeta\n    SpecialPowerTemplate = SP_Meta\n    SpecialWeapon = W_Meta\n    UnpackTime = 0\n    PreparationTime = 0\n    PackTime = 0\n    IgnoreFacingCheck = Yes\n  End\n"
		"  Behavior = SpecialPowerModule PSteal\n    SpecialPowerTemplate = SP_Steal\n    UpdateModuleStartsAttack = Yes\n  End\n"
		"  Behavior = WeaponFireSpecialAbilityUpdate USteal\n    SpecialPowerTemplate = SP_Steal\n    SpecialWeapon = W_Steal\n    UnpackTime = 0\n    PreparationTime = 0\n    PackTime = 0\n    StartAbilityRange = 200\n    IgnoreFacingCheck = Yes\n  End\n"
		"  Behavior = SpecialPowerModule PParalyze\n    SpecialPowerTemplate = SP_Paralyze\n    UpdateModuleStartsAttack = Yes\n  End\n"
		"  Behavior = WeaponFireSpecialAbilityUpdate UParalyze\n    SpecialPowerTemplate = SP_Paralyze\n    SpecialWeapon = W_Paralyze\n    UnpackTime = 0\n    PreparationTime = 0\n    PackTime = 0\n    StartAbilityRange = 200\n    IgnoreFacingCheck = Yes\n  End\n"
		"  Behavior = SpecialPowerModule PSMC\n    SpecialPowerTemplate = SP_SMC\n    UpdateModuleStartsAttack = Yes\n  End\n"
		"  Behavior = WeaponFireSpecialAbilityUpdate USMC\n    SpecialPowerTemplate = SP_SMC\n    SpecialWeapon = W_SMC\n    UnpackTime = 0\n    PreparationTime = 0\n    PackTime = 0\n    StartAbilityRange = 200\n    IgnoreFacingCheck = Yes\n  End\n"
		"  Behavior = SpecialPowerModule PProjDie\n    SpecialPowerTemplate = SP_ProjDie\n    UpdateModuleStartsAttack = Yes\n  End\n"
		"  Behavior = WeaponFireSpecialAbilityUpdate UProjDie\n    SpecialPowerTemplate = SP_ProjDie\n    SpecialWeapon = W_ProjDie\n    UnpackTime = 0\n    PreparationTime = 0\n    PackTime = 0\n    StartAbilityRange = 200\n    IgnoreFacingCheck = Yes\n  End\n"
		"  Behavior = SpecialPowerModule PFire\n    SpecialPowerTemplate = SP_FireLogicOnly\n    UpdateModuleStartsAttack = Yes\n  End\n"
		"  Behavior = WeaponFireSpecialAbilityUpdate UFire\n    SpecialPowerTemplate = SP_FireLogicOnly\n    SpecialWeapon = W_FireLogicOnly\n    UnpackTime = 0\n    PreparationTime = 0\n    PackTime = 0\n    IgnoreFacingCheck = Yes\n  End\n"
		"  Behavior = SpecialPowerModule PNonHorde\n    SpecialPowerTemplate = SP_HordeShot\n    UpdateModuleStartsAttack = Yes\n  End\n"
		"  Behavior = WeaponFireSpecialAbilityUpdate UNonHorde\n    SpecialPowerTemplate = SP_HordeShot\n    SpecialWeapon = W_HordeFinder\n    UnpackTime = 0\n    PreparationTime = 0\n    PackTime = 0\n    StartAbilityRange = 200\n    IgnoreFacingCheck = Yes\n  End\n"
		"  Behavior = SpecialPowerModule PUpgFilter\n    SpecialPowerTemplate = SP_UpgFilter\n    UpdateModuleStartsAttack = Yes\n  End\n"
		"  Behavior = WeaponFireSpecialAbilityUpdate UUpgFilter\n    SpecialPowerTemplate = SP_UpgFilter\n    SpecialWeapon = W_UpgFilter\n    UnpackTime = 0\n    PreparationTime = 0\n    PackTime = 0\n    StartAbilityRange = 200\n    IgnoreFacingCheck = Yes\n  End\n"
		"  Behavior = SpecialPowerModule POnlyMon\n    SpecialPowerTemplate = SP_OnlyMonster\n    UpdateModuleStartsAttack = Yes\n  End\n"
		"  Behavior = WeaponFireSpecialAbilityUpdate UOnlyMon\n    SpecialPowerTemplate = SP_OnlyMonster\n    SpecialWeapon = W_OnlyMonster\n    UnpackTime = 0\n    PreparationTime = 0\n    PackTime = 0\n    StartAbilityRange = 200\n    IgnoreFacingCheck = Yes\n  End\n")
	"Object L78_HordeMember\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK\n"
	"  Geometry = CYLINDER\n  GeometryMajorRadius = 5\n  GeometryMinorRadius = 5\n  GeometryHeight = 15\n"
	"  WeaponSet\n    Conditions = None\n    Weapon = QUINARY W_HordeQuinary\n  End\n"
	"  Body = ActiveBody BodyTag\n    MaxHealth = 100\n    InitialHealth = 100\n  End\n"
	"  Behavior = AIUpdateInterface AiTag\n  End\n"
	"  LocomotorSet\n    Locomotor = WalkerLoco\n    Condition = SET_NORMAL\n    Speed = 55\n  End\n"
	"End\n"
	"Object L78_ArcherHorde\n"
	"  KindOf = HORDE LARGE_RECTANGLE_PATHFIND SELECTABLE CAN_ATTACK\n"
	"  VisionRange = 300\n"
	"  Geometry = BOX\n  GeometryMajorRadius = 20\n  GeometryMinorRadius = 20\n  GeometryHeight = 20\n"
	"  Body = ImmortalBody BodyTag\n    MaxHealth = 1\n  End\n"
	"  Behavior = HordeAIUpdate AiTag\n    AutoAcquireEnemiesWhenIdle = No\n  End\n"
	"  LocomotorSet\n    Locomotor = HordeLoco\n    Condition = SET_NORMAL\n    Speed = 50\n  End\n"
	"  Behavior = HordeContain ContainTag\n"
	"    Slots = 2\n    InitialPayload = L78_HordeMember 2\n"
	"    RankInfo = RankNumber:1 UnitType:L78_HordeMember Position:X:0 Y:0 Position:X:15 Y:0\n"
	"    RanksToReleaseWhenAttacking = 1\n"
	"  End\n"
	"  Behavior = SpecialPowerModule PHordeShot\n    SpecialPowerTemplate = SP_HordeShot\n    UpdateModuleStartsAttack = Yes\n  End\n"
	"  Behavior = WeaponFireSpecialAbilityUpdate UHordeShot\n    SpecialPowerTemplate = SP_HordeShot\n    SpecialWeapon = W_HordeFinder\n    UnpackTime = 0\n    PreparationTime = 0\n    PackTime = 0\n    StartAbilityRange = 300\n    IgnoreFacingCheck = Yes\n  End\n"
	"End\n"
	L78_UNIT("L78_AbilityCaster",
		"  Behavior = SpecialPowerModule PMC\n    SpecialPowerTemplate = SP_MC\n    UpdateModuleStartsAttack = Yes\n  End\n"
		"  Behavior = ModelConditionSpecialAbilityUpdate UMC\n    SpecialPowerTemplate = SP_MC\n    WhichSpecialPower = 1\n    GenerateTerror = Yes\n    EmotionPulseRadius = 150\n    UnpackTime = 400\n    PreparationTime = 0\n    PackTime = 400\n    IgnoreFacingCheck = Yes\n  End\n"
		"  Behavior = SpecialPowerModule PDisguise\n    SpecialPowerTemplate = SP_Disguise\n    UpdateModuleStartsAttack = Yes\n  End\n"
		"  Behavior = SpecialDisguiseUpdate UDisguise\n    SpecialPowerTemplate = SP_Disguise\n    DisguiseAsTemplate = L78_Victim\n    UnpackTime = 0\n    PreparationTime = 0\n    PackTime = 0\n    IgnoreFacingCheck = Yes\n  End\n"
		"  Behavior = SpecialPowerModule PReplace\n    SpecialPowerTemplate = SP_Replace\n    UpdateModuleStartsAttack = Yes\n  End\n"
		"  Behavior = ReplaceObjectUpdate UReplace\n    SpecialPowerTemplate = SP_Replace\n    ReplaceRadius = 150\n    UnpackTime = 0\n    PreparationTime = 0\n    PackTime = 0\n    IgnoreFacingCheck = Yes\n    ReplaceObject\n      TargetObjectFilter = NONE +INFANTRY ENEMIES\n      ReplacementObjectName = NoSuchReplacement\n    End\n  End\n"
		"  Behavior = InvisibilitySpecialPower PInvis\n    SpecialPowerTemplate = SP_Invis\n    BroadcastRadius = 150\n    Duration = 5000\n    ObjectFilter = ALL ALLIES\n    InvisibilityNugget\n      InvisibilityType = CAMOUFLAGE\n    End\n  End\n"
		"  Behavior = SpecialPowerTimerRefreshSpecialPower PRefresh\n    SpecialPowerTemplate = SP_Refresh\n    AttributeModifierRange = 150\n    AttributeModifierAffects = ALL ALLIES\n  End\n"
		"  Behavior = SpecialPowerModule PHarvest\n    SpecialPowerTemplate = SP_Harvest\n  End\n"
		"  Behavior = SpecialPowerModule PAthelas\n    SpecialPowerTemplate = SP_Athelas\n    UpdateModuleStartsAttack = Yes\n  End\n"
		"  Behavior = SpecialAbilityUpdate UAthelas\n    SpecialPowerTemplate = SP_Athelas\n    StartAbilityRange = 100\n    UnpackTime = 0\n    PreparationTime = 0\n    PackTime = 600\n    IgnoreFacingCheck = Yes\n  End\n"
		"  Behavior = AutoHealBehavior HealTag\n    StartsActive = Yes\n    ButtonTriggered = Yes\n    SingleBurst = Yes\n    HealingAmount = 150\n    HealingDelay = 1000\n    Radius = 150\n    HealOnlyOthers = No\n    KindOf = HERO INFANTRY\n  End\n"
		"  Behavior = SpecialPowerModule PChainStart\n    SpecialPowerTemplate = SP_ChainStart\n    UpdateModuleStartsAttack = Yes\n  End\n"
		"  Behavior = ActivateModuleSpecialPower UChainMover\n    SpecialPowerTemplate = SP_ChainStart\n    StartAbilityRange = 200\n    TriggerSpecialPower = Tag_ChainSub TARGETPOS\n  End\n"
		"  Behavior = SpecialPowerModule Tag_ChainSub\n    SpecialPowerTemplate = SP_ChainSub\n    UpdateModuleStartsAttack = No\n  End\n"
		"  Behavior = WeaponFireSpecialAbilityUpdate UChainWeapon\n    SpecialPowerTemplate = SP_ChainSub\n    SpecialWeapon = W_ChainBlast\n    UnpackTime = 0\n    PreparationTime = 0\n    PackTime = 0\n    StartAbilityRange = 200\n    IgnoreFacingCheck = Yes\n  End\n")
	"Object L78_DeployUnit\n"
	"  KindOf = INFANTRY SELECTABLE MACHINE\n"
	"  Geometry = CYLINDER\n  GeometryMajorRadius = 8\n  GeometryMinorRadius = 8\n  GeometryHeight = 20\n"
	"  Body = ActiveBody BodyTag\n    MaxHealth = 500\n  End\n"
	"  Behavior = DeployStyleAIUpdate AiTag\n    UnpackTime = 200\n    PackTime = 200\n  End\n"
	"  LocomotorSet\n    Locomotor = WalkerLoco\n    Condition = SET_NORMAL\n    Speed = 55\n  End\n"
	"  Behavior = SpecialPowerModule PDeploy\n    SpecialPowerTemplate = SP_Deploy\n    UpdateModuleStartsAttack = Yes\n  End\n"
	"  Behavior = ToggleDeploySpecialAbilityUpdate UDeploy\n    SpecialPowerTemplate = SP_Deploy\n    UnpackTime = 200\n    PackTime = 200\n    IgnoreFacingCheck = Yes\n  End\n"
	"End\n"
	L78_UNIT("L78_Victim", "")
	L78_UNIT("L78_Ally",
		"  Behavior = SpecialPowerModule PAllyPower\n    SpecialPowerTemplate = SP_DirectMod\n  End\n");

} // namespace

TEST_CASE("LOCAL-78: AttributeModifierNugget direct, warhead, and multi-modifier rules")
{
	ProbeWorld f(kLocal78Ini);
	Object *caster = f.unit("L78_WeaponCaster", 'A', 300, 300);
	f.frames(2);

	for (const char *spName : { "SP_DirectMod", "SP_ProjMod" })
	{
		CAPTURE(spName);
		Object *enemy = f.unit("L78_Victim", 'B', 340, 300);
		f.frames(1);
		const SpecialPowerTemplate *t = f.powerTpl(spName);
		HitTracker hits;
		const EffectProbe before = EffectProbe::take(*f.logic, caster, t);
		REQUIRE(before.unasserted.empty());
		REQUIRE(before.expects.size() == 1);
		CHECK(before.expects[0].kind == EffectProbe::MODIFIER);

		std::string missingBefore;
		before.compare(before, missingBefore, &hits);
		CHECK(missingBefore.find(": no result") != std::string::npos);

		enemy->addAttributeModifier("L78_ModOne", 50);
		f.frames(1);
		const EffectProbe after = EffectProbe::take(*f.logic, caster, t);
		std::string missingAfter;
		const std::string got = before.compare(after, missingAfter, &hits);
		CHECK_MESSAGE(missingAfter.empty(), missingAfter);
		CHECK(got.find("L78_ModOne") != std::string::npos);
		f.logic->destroyObject(enemy);
		f.frames(1);
	}

	Object *enemyMulti = f.unit("L78_Victim", 'B', 340, 300);
	f.frames(1);
	const SpecialPowerTemplate *multi = f.powerTpl("SP_MultiMod");
	const EffectProbe mbBefore = EffectProbe::take(*f.logic, caster, multi);
	REQUIRE(mbBefore.unasserted.empty());
	REQUIRE(mbBefore.expects.size() == 1);
	CHECK(mbBefore.expects[0].modifiers.count("L78_ModOne") == 1);
	CHECK(mbBefore.expects[0].modifiers.count("L78_ModTwo") == 1);
	enemyMulti->addAttributeModifier("L78_ModOne", 50);
	f.frames(1);
	std::string mbMissing;
	mbBefore.compare(EffectProbe::take(*f.logic, caster, multi), mbMissing, nullptr);
	CHECK_MESSAGE(mbMissing.empty(), mbMissing);
}

TEST_CASE("LOCAL-78: Emotion, LuaEvent, MetaImpact, StealMoney, Paralyze, and SpecialModelCondition nuggets")
{
	ProbeWorld f(kLocal78Ini);
	Object *caster = f.unit("L78_WeaponCaster", 'A', 300, 300);
	Object *enemy = f.unit("L78_Victim", 'B', 330, 300);
	f.frames(2);

	// EmotionWeaponNugget
	{
		const SpecialPowerTemplate *t = f.powerTpl("SP_Emotion");
		HitTracker hits;
		hits.casters.insert(caster->getID());
		const EffectProbe before = EffectProbe::take(*f.logic, caster, t);
		REQUIRE(before.unasserted.empty());
		REQUIRE(before.expects.size() == 1);
		CHECK(before.expects[0].kind == EffectProbe::EMOTION);

		std::string missing;
		before.compare(before, missing, &hits);
		CHECK(missing.find(": no result") != std::string::npos);

		f.triggerAt(caster, "SP_Emotion", nullptr, *caster->getPosition());
		trackFor(f, hits, 5);
		missing.clear();
		const std::string got = before.compare(EffectProbe::take(*f.logic, caster, t), missing, &hits);
		CHECK_MESSAGE(missing.empty(), missing);
		CHECK(got.find("emotion") != std::string::npos);
	}

	// LuaEventNugget (dispatch + fallback failure)
	{
		size_t dispatched = 0;
		f.logic->setNamedScriptEventProcs([](const std::string &n) { return n == "BeTerrified"; },
										  [&](const std::string &, Object &, const LuaEventArgs &) { ++dispatched; });
		const SpecialPowerTemplate *t = f.powerTpl("SP_Lua");
		const EffectProbe before = EffectProbe::take(*f.logic, caster, t, dispatched);
		REQUIRE(before.unasserted.empty());
		REQUIRE(before.expects.size() == 1);
		CHECK(before.expects[0].kind == EffectProbe::LUA_EVENT);

		std::string missing;
		before.compare(before, missing, nullptr);
		CHECK(missing.find(": no result") != std::string::npos);

		f.triggerAt(caster, "SP_Lua", nullptr, *caster->getPosition());
		f.frames(3);
		missing.clear();
		const std::string got = before.compare(EffectProbe::take(*f.logic, caster, t, dispatched), missing, nullptr);
		CHECK_MESSAGE(missing.empty(), missing);
		CHECK(got.find("BeTerrified") != std::string::npos);
	}

	// MetaImpactNugget
	{
		const SpecialPowerTemplate *t = f.powerTpl("SP_Meta");
		const EffectProbe before = EffectProbe::take(*f.logic, caster, t);
		REQUIRE(before.unasserted.empty());
		REQUIRE(before.expects.size() == 1);
		CHECK(before.expects[0].kind == EffectProbe::META_IMPACT);

		std::string missing;
		before.compare(before, missing, nullptr);
		CHECK(missing.find(": no result") != std::string::npos);

		f.triggerAt(caster, "SP_Meta", nullptr, *caster->getPosition());
		f.frames(3);
		missing.clear();
		const std::string got = before.compare(EffectProbe::take(*f.logic, caster, t), missing, nullptr);
		CHECK_MESSAGE(missing.empty(), missing);
		CHECK(got.find("metaImpactHits") != std::string::npos);
	}

	// StealMoneyNugget: fails when enemy has 0 money, passes when enemy has cash
	{
		Player *enemyPlayer = f.teamB()->getControllingPlayer();
		enemyPlayer->withdrawMoney(enemyPlayer->getMoney()->countMoney(), false);
		const SpecialPowerTemplate *t = f.powerTpl("SP_Steal");
		const EffectProbe beforeZero = EffectProbe::take(*f.logic, caster, t);
		REQUIRE(beforeZero.unasserted.empty());
		REQUIRE(beforeZero.expects.size() == 1);
		CHECK(beforeZero.expects[0].kind == EffectProbe::STEAL_MONEY);

		f.triggerAt(caster, "SP_Steal", enemy, *enemy->getPosition());
		f.frames(3);
		std::string missingZero;
		beforeZero.compare(EffectProbe::take(*f.logic, caster, t), missingZero, nullptr);
		CHECK(missingZero.find(": no result") != std::string::npos);

		enemyPlayer->depositMoney(5000);
		f.frames(10); // past reload
		const EffectProbe beforeCash = EffectProbe::take(*f.logic, caster, t);
		f.triggerAt(caster, "SP_Steal", enemy, *enemy->getPosition());
		f.frames(3);
		std::string missingCash;
		const std::string got = beforeCash.compare(EffectProbe::take(*f.logic, caster, t), missingCash, nullptr);
		CHECK_MESSAGE(missingCash.empty(), missingCash);
		CHECK(got.find("player money") != std::string::npos);
	}

	// ParalyzeNugget
	{
		const SpecialPowerTemplate *t = f.powerTpl("SP_Paralyze");
		HitTracker hits;
		const EffectProbe before = EffectProbe::take(*f.logic, caster, t);
		REQUIRE(before.unasserted.empty());
		REQUIRE(before.expects.size() == 1);
		CHECK(before.expects[0].kind == EffectProbe::PARALYZE);

		f.triggerAt(caster, "SP_Paralyze", enemy, *enemy->getPosition());
		trackFor(f, hits, 3);
		std::string missing;
		const std::string got = before.compare(EffectProbe::take(*f.logic, caster, t), missing, &hits);
		CHECK_MESSAGE(missing.empty(), missing);
		CHECK(got.find("disabled") != std::string::npos);
	}

	// SpecialModelConditionNugget
	{
		const SpecialPowerTemplate *t = f.powerTpl("SP_SMC");
		HitTracker hits;
		const EffectProbe before = EffectProbe::take(*f.logic, caster, t);
		REQUIRE(before.unasserted.empty());
		REQUIRE(before.expects.size() == 1);
		CHECK(before.expects[0].kind == EffectProbe::MODEL_CONDITION);

		std::string missingBefore;
		before.compare(before, missingBefore, &hits);
		CHECK(missingBefore.find(": no result") != std::string::npos);

		f.triggerAt(caster, "SP_SMC", enemy, *enemy->getPosition());
		trackFor(f, hits, 3);
		std::string missing;
		const std::string got = before.compare(EffectProbe::take(*f.logic, caster, t), missing, &hits);
		CHECK_MESSAGE(missing.empty(), missing);
		CHECK(got.find("model condition") != std::string::npos);
	}
}

TEST_CASE("LOCAL-78: Projectile CreateObjectDie OCL, FireLogicNugget exclusion, and HordeAttackNugget rules")
{
	ProbeWorld f(kLocal78Ini);
	Object *caster = f.unit("L78_WeaponCaster", 'A', 300, 300);
	Object *horde = f.unit("L78_ArcherHorde", 'A', 300, 360);
	Object *enemy = f.unit("L78_Victim", 'B', 380, 340);
	f.frames(2);

	// ProjectileNugget -> Projectile CreateObjectDie -> OCL_Vision
	{
		const SpecialPowerTemplate *t = f.powerTpl("SP_ProjDie");
		const EffectProbe before = EffectProbe::take(*f.logic, caster, t);
		REQUIRE(before.unasserted.empty());
		REQUIRE(before.expects.size() == 1);
		CHECK(before.expects[0].kind == EffectProbe::SPAWN);
		CHECK(before.expects[0].templates.count("L78_VisionDummy") == 1);

		Object *dummy = f.unit("L78_VisionDummy", 'A', 380, 340);
		REQUIRE(dummy != nullptr);
		std::string missing;
		const std::string got = before.compare(EffectProbe::take(*f.logic, caster, t), missing, nullptr);
		CHECK_MESSAGE(missing.empty(), missing);
		CHECK(got.find("L78_VisionDummy") != std::string::npos);
	}

	// FireLogicNugget derives from DamageNugget: MUST NOT emit DAMAGE (emits FIRE_LOGIC -> NO-EFFECT, not NO-RULE)
	{
		const SpecialPowerTemplate *t = f.powerTpl("SP_FireLogicOnly");
		const EffectProbe before = EffectProbe::take(*f.logic, caster, t);
		CHECK(before.unasserted.empty());
		REQUIRE(before.expects.size() == 1);
		CHECK(before.expects[0].kind == EffectProbe::FIRE_LOGIC);
		std::string missing;
		before.compare(before, missing, nullptr);
		CHECK(missing.find(": no result (FireLogicNugget)") != std::string::npos);
		CHECK(missing.find("is asserted") == std::string::npos);
	}

	// HordeAttackNugget on single unit -> expects DAMAGE (no HordeContain -> NO-EFFECT, not NO-RULE)
	{
		const SpecialPowerTemplate *t = f.powerTpl("SP_HordeShot");
		const EffectProbe singleBefore = EffectProbe::take(*f.logic, caster, t);
		CHECK(singleBefore.unasserted.empty());
		REQUIRE(singleBefore.expects.size() == 1);
		CHECK(singleBefore.expects[0].kind == EffectProbe::DAMAGE);
		CHECK(singleBefore.expects[0].what.find("no HordeContain") != std::string::npos);
		std::string singleMissing;
		singleBefore.compare(singleBefore, singleMissing, nullptr);
		CHECK(singleMissing.find(": no result") != std::string::npos);
		CHECK(singleMissing.find("is asserted") == std::string::npos);
	}

	// HordeAttackNugget on real horde -> resolves QUINARY slot W_HordeQuinary and verifies hit
	{
		const SpecialPowerTemplate *t = f.powerTpl("SP_HordeShot");
		HitTracker hits;
		hits.casters.insert(horde->getID());
		for (Object *m : *horde->getContain()->getContainedItemsList())
		{
			hits.casters.insert(m->getID());
		}
		hits.track(*f.logic);
		f.frames(2);
		hits.track(*f.logic);

		EffectProbe hordeBefore = EffectProbe::take(*f.logic, horde, t);
		hordeBefore.nominals(*f.logic);
		REQUIRE(hordeBefore.unasserted.empty());
		REQUIRE(hordeBefore.expects.size() == 1);
		CHECK(hordeBefore.expects[0].kind == EffectProbe::DAMAGE);
		CHECK(hordeBefore.hitNominal.count(enemy->getID()) == 1);

		f.triggerAt(horde, "SP_HordeShot", enemy, *enemy->getPosition());
		trackFor(f, hits, 15);
		std::string missing;
		const std::string got = hordeBefore.compare(EffectProbe::take(*f.logic, horde, t), missing, &hits);
		CHECK_MESSAGE(missing.empty(), missing);
		CHECK(got.find("first hit after the cast") != std::string::npos);
	}
}

TEST_CASE("LOCAL-78: ModelCondition, ToggleDeploy, SpecialDisguise, and ReplaceObjectUpdate rules")
{
	ProbeWorld f(kLocal78Ini);
	Object *caster = f.unit("L78_AbilityCaster", 'A', 300, 300);
	Object *deploy = f.unit("L78_DeployUnit", 'A', 300, 360);
	Object *enemy = f.unit("L78_Victim", 'B', 340, 300);
	f.frames(2);

	// ModelConditionSpecialAbilityUpdate (SPECIAL_POWER_1 + GenerateTerror)
	{
		const SpecialPowerTemplate *t = f.powerTpl("SP_MC");
		HitTracker hits;
		hits.casters.insert(caster->getID());
		const EffectProbe before = EffectProbe::take(*f.logic, caster, t);
		REQUIRE(before.unasserted.empty());
		REQUIRE(before.expects.size() == 2);

		std::string missingBefore;
		before.compare(before, missingBefore, &hits);
		CHECK(missingBefore.find("SPECIAL_POWER_1): no result") != std::string::npos);
		CHECK(missingBefore.find("GenerateTerror): no result") != std::string::npos);

		f.triggerAt(caster, "SP_MC", nullptr, *caster->getPosition());
		trackFor(f, hits, 3);
		std::string missing;
		before.compare(EffectProbe::take(*f.logic, caster, t), missing, &hits);
		CHECK_MESSAGE(missing.empty(), missing);
	}

	// ToggleDeploySpecialAbilityUpdate
	{
		const SpecialPowerTemplate *t = f.powerTpl("SP_Deploy");
		HitTracker hits;
		const EffectProbe before = EffectProbe::take(*f.logic, deploy, t);
		REQUIRE(before.unasserted.empty());
		REQUIRE(before.expects.size() == 1);
		CHECK(before.expects[0].kind == EffectProbe::MODEL_CONDITION);

		f.triggerAt(deploy, "SP_Deploy", nullptr, *deploy->getPosition());
		trackFor(f, hits, 5);
		std::string missing;
		before.compare(EffectProbe::take(*f.logic, deploy, t), missing, &hits);
		CHECK_MESSAGE(missing.empty(), missing);
	}

	// SpecialDisguiseUpdate
	{
		const SpecialPowerTemplate *t = f.powerTpl("SP_Disguise");
		HitTracker hits;
		const EffectProbe before = EffectProbe::take(*f.logic, caster, t);
		REQUIRE(before.unasserted.empty());
		REQUIRE(before.expects.size() == 1);
		CHECK(before.expects[0].kind == EffectProbe::MODEL_CONDITION);

		f.triggerAt(caster, "SP_Disguise", nullptr, *caster->getPosition());
		trackFor(f, hits, 3);
		std::string missing;
		before.compare(EffectProbe::take(*f.logic, caster, t), missing, &hits);
		CHECK_MESSAGE(missing.empty(), missing);
	}

	// ReplaceObjectUpdate (even when replacement Racoon_Replacement is missing from INI: victim destroyed)
	{
		const SpecialPowerTemplate *t = f.powerTpl("SP_Replace");
		HitTracker hits;
		const EffectProbe before = EffectProbe::take(*f.logic, caster, t);
		REQUIRE(before.unasserted.empty());
		REQUIRE(before.expects.size() == 1);
		CHECK(before.expects[0].kind == EffectProbe::REPLACE);

		std::string missingBefore;
		before.compare(before, missingBefore, &hits);
		CHECK(missingBefore.find(": no result") != std::string::npos);

		f.triggerAt(caster, "SP_Replace", nullptr, *enemy->getPosition());
		trackFor(f, hits, 3);
		std::string missing;
		const std::string got = before.compare(EffectProbe::take(*f.logic, caster, t), missing, &hits);
		CHECK_MESSAGE(missing.empty(), missing);
		CHECK(got.find("replaced") != std::string::npos);
	}
}

TEST_CASE("LOCAL-78: Invisibility, TimerRefresh, SPECIAL_HARVEST, SPECIAL_ATHELAS, and ActivateModule chain rules")
{
	ProbeWorld f(kLocal78Ini);
	Object *caster = f.unit("L78_AbilityCaster", 'A', 300, 300);
	Object *ally = f.unit("L78_Ally", 'A', 340, 300);
	Object *enemy = f.unit("L78_Victim", 'B', 350, 300);
	f.frames(2);

	// InvisibilitySpecialPower
	{
		const SpecialPowerTemplate *t = f.powerTpl("SP_Invis");
		HitTracker hits;
		const EffectProbe before = EffectProbe::take(*f.logic, caster, t);
		REQUIRE(before.unasserted.empty());
		REQUIRE(before.expects.size() == 1);
		CHECK(before.expects[0].kind == EffectProbe::MODEL_CONDITION);

		std::string missingBefore;
		before.compare(before, missingBefore, &hits);
		CHECK(missingBefore.find(": no result") != std::string::npos);

		f.triggerAt(caster, "SP_Invis", nullptr, *ally->getPosition());
		trackFor(f, hits, 3);
		std::string missing;
		before.compare(EffectProbe::take(*f.logic, caster, t), missing, &hits);
		CHECK_MESSAGE(missing.empty(), missing);
	}

	// SpecialPowerTimerRefreshSpecialPower
	{
		const SpecialPowerTemplate *t = f.powerTpl("SP_Refresh");
		// No ally power recharging yet -> fails
		const EffectProbe beforeReady = EffectProbe::take(*f.logic, caster, t);
		REQUIRE(beforeReady.unasserted.empty());
		REQUIRE(beforeReady.expects.size() == 1);
		CHECK(beforeReady.expects[0].kind == EffectProbe::TIMER_REFRESH);
		f.triggerAt(caster, "SP_Refresh", nullptr, *ally->getPosition());
		f.frames(2);
		std::string missingReady;
		beforeReady.compare(EffectProbe::take(*f.logic, caster, t), missingReady, nullptr);
		CHECK(missingReady.find(": no result") != std::string::npos);

		// Put ally power on cooldown, then refresh
		f.frames(10);
		SpecialPowerModule *allySp = dynamic_cast<SpecialPowerModule *>(ally->findModule("SpecialPowerModule"));
		REQUIRE(allySp != nullptr);
		allySp->setReadyFrame(f.logic->getFrame() + 100);
		const EffectProbe beforeCd = EffectProbe::take(*f.logic, caster, t);
		f.triggerAt(caster, "SP_Refresh", nullptr, *ally->getPosition());
		f.frames(2);
		std::string missingCd;
		const std::string got = beforeCd.compare(EffectProbe::take(*f.logic, caster, t), missingCd, nullptr);
		CHECK_MESSAGE(missingCd.empty(), missingCd);
		CHECK(got.find("refreshed") != std::string::npos);
	}

	// SPECIAL_HARVEST
	{
		const SpecialPowerTemplate *t = f.powerTpl("SP_Harvest");
		HitTracker hits;
		const EffectProbe before = EffectProbe::take(*f.logic, caster, t);
		REQUIRE(before.unasserted.empty());
		REQUIRE(before.expects.size() == 1);
		CHECK(before.expects[0].kind == EffectProbe::HARVEST);

		std::string missingBefore;
		before.compare(before, missingBefore, &hits);
		CHECK(missingBefore.find(": no result") != std::string::npos);

		caster->setModelConditionState(CombatNames::modelCondition("HARVEST_ACTION"), true);
		f.frames(1);
		hits.track(*f.logic);
		std::string missing;
		before.compare(EffectProbe::take(*f.logic, caster, t), missing, &hits);
		CHECK_MESSAGE(missing.empty(), missing);
		caster->setModelConditionState(CombatNames::modelCondition("HARVEST_ACTION"), false);
		f.frames(1);
	}

	// SPECIAL_ATHELAS -> AutoHealBehavior::triggerByButton
	{
		const SpecialPowerTemplate *t = f.powerTpl("SP_Athelas");
		ActiveBody *body = dynamic_cast<ActiveBody *>(caster->getBodyModule());
		REQUIRE(body != nullptr);
		body->setInitialHealthPercent(50.0f);
		const EffectProbe before = EffectProbe::take(*f.logic, caster, t);
		REQUIRE(before.unasserted.empty());
		REQUIRE(before.expects.size() == 1);
		CHECK(before.expects[0].kind == EffectProbe::HEAL);

		f.triggerAt(caster, "SP_Athelas", nullptr, *caster->getPosition());
		f.frames(8);
		AutoHealBehavior *ah = dynamic_cast<AutoHealBehavior *>(caster->findModule("AutoHealBehavior"));
		INFO("health=" << body->getHealth() << " heals=" << (ah ? (int)ah->heals() : -1));
		std::string missing;
		const std::string got = before.compare(EffectProbe::take(*f.logic, caster, t), missing, nullptr);
		CHECK_MESSAGE(missing.empty(), missing);
		CHECK(got.find("healed") != std::string::npos);
	}

	// ActivateModuleSpecialPower -> SpecialPowerModule sibling -> WeaponFireSpecialAbilityUpdate (DAMAGE + META_IMPACT)
	{
		const SpecialPowerTemplate *t = f.powerTpl("SP_ChainStart");
		HitTracker hits;
		hits.casters.insert(caster->getID());
		hits.track(*f.logic);
		f.frames(2);
		hits.track(*f.logic);

		EffectProbe before = EffectProbe::take(*f.logic, caster, t);
		before.nominals(*f.logic);
		REQUIRE(before.unasserted.empty());
		REQUIRE(before.expects.size() == 2);

		f.triggerAt(caster, "SP_ChainStart", nullptr, *enemy->getPosition());
		trackFor(f, hits, 5);
		std::string missing;
		before.compare(EffectProbe::take(*f.logic, caster, t), missing, &hits);
		CHECK_MESSAGE(missing.empty(), missing);
	}
}

TEST_CASE("LOCAL-78: SweepEffectProbe respects nugget upgrade gates, SpecialObjectFilter, and skippedNotes")
{
	ProbeWorld f(kLocal78Ini);
	Object *caster = f.unit("L78_WeaponCaster", 'A', 300, 300);
	Object *enemy = f.unit("L78_Victim", 'B', 330, 300);
	f.frames(2);

	const SpecialPowerTemplate *spUpg = f.powerTpl("SP_UpgFilter");
	const SpecialPowerTemplate *spMon = f.powerTpl("SP_OnlyMonster");
	REQUIRE(spUpg != nullptr);
	REQUIRE(spMon != nullptr);

	// 1. Without Upg_Bonus and with an INFANTRY target: RequiredUpgradeNames skips BonusWarhead,
	//    SpecialObjectFilter NONE +MONSTER skips EmotionWeaponNugget with a skippedNote, leaving only DAMAGE.
	{
		const EffectProbe probeInf = EffectProbe::take(*f.logic, caster, spUpg, 0, enemy);
		REQUIRE(probeInf.unasserted.empty());
		REQUIRE(probeInf.expects.size() == 1);
		CHECK(probeInf.expects[0].kind == EffectProbe::DAMAGE);
		REQUIRE(probeInf.skippedNotes.size() == 1);
		CHECK(probeInf.skippedNotes[0].find("SpecialObjectFilter") != std::string::npos);
	}

	// 2. With target == nullptr: EmotionWeaponNugget +MONSTER is not pre-filtered out
	{
		const EffectProbe probeNone = EffectProbe::take(*f.logic, caster, spUpg, 0, nullptr);
		REQUIRE(probeNone.expects.size() == 2);
		CHECK(probeNone.skippedNotes.empty());
	}

	// 3. When all nuggets of a weapon are filtered out by SpecialObjectFilter, compare() reports failure instead of silent PASS
	{
		const EffectProbe beforeMon = EffectProbe::take(*f.logic, caster, spMon, 0, enemy);
		CHECK(beforeMon.expects.empty());
		REQUIRE(beforeMon.skippedNotes.size() == 1);
		std::string missing;
		beforeMon.compare(beforeMon, missing, nullptr);
		CHECK(missing.find("SpecialObjectFilter") != std::string::npos);
	}

	// 4. Grant Upg_Bonus to caster -> BonusWarhead MODIFIER becomes active, ForbiddenUpgradeNames skips BaseWarhead
	{
		const UpgradeTemplate *up = TheUpgradeCenter ? TheUpgradeCenter->findUpgrade("Upg_Bonus") : nullptr;
		REQUIRE(up != nullptr);
		caster->giveUpgrade(up);
		const EffectProbe probeUp = EffectProbe::take(*f.logic, caster, spUpg, 0, enemy);
		REQUIRE(probeUp.expects.size() == 1);
		CHECK(probeUp.expects[0].kind == EffectProbe::MODIFIER);
		CHECK(probeUp.expects[0].modifiers.count("L78_ModOne") == 1u);
		REQUIRE(probeUp.skippedNotes.size() == 1);
	}
}

TEST_SUITE_END();
