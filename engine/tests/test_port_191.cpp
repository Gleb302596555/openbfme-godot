// OpenBFME. GPL-3.0.
// PORT-191 (issue #458, S-1227): AutoAbilityBehavior without a Query (RW 0x85DD3A -> RW 0x701443 with flags 0x4A),
// AI + 0x34 (RW 0x85D693 .. 0x85D6A1) and + 0x48 (RW 0x85DD69 .. 0x85DD70), vision range modifiers (RW 0x85DB30 -> RW 0x68E43B),
// and the disabled object's auto-cast exception (RW 0x855830 + RW 0x68E5A3 / 0x68E67A / 0x68E754).

#include "doctest.h"
#include "CombatTestUtil.h"

#include "Common/SpecialPower.h"
#include "GameClient/ControlBarCommands.h"
#include "GameClient/FXList.h"
#include "GameLogic/AttributeModifiers.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Combat/ObjectWeapons.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/HeroAbilityModules.h"
#include "GameLogic/Module/SpecialAbilityModules.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Object/AttributeModifierPool.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/Object/PartitionManager.h"
#include "GameLogic/ObjectTemplateInfo.h"

namespace
{

const char kPort191Ini[] =
	"ModifierList Port191Buff\n"
	"  Category = LEADERSHIP\n"
	"  Duration = 5000\n"
	"  Modifier = ARMOR 150%\n"
	"End\n"
	"ModifierList Port191VisionBoost\n"
	"  Category = SPELL\n"
	"  Duration = 10000\n"
	"  Modifier = VISION 100%\n"
	"End\n"
	"SpecialPower SpecialAbilityPort191TargetEnemy\n"
	"  Enum = SPECIAL_GENERAL_TARGETLESS\n"
	"  ReloadTime = 1000\n"
	"End\n"
	"SpecialPower SpecialAbilityPort191SelfBuff\n"
	"  Enum = SPECIAL_GENERAL_TARGETLESS\n"
	"  ReloadTime = 1000\n"
	"End\n"
	"SpecialPower SpecialAbilityPort191PosCast\n"
	"  Enum = SPECIAL_GENERAL_TARGETLESS\n"
	"  ReloadTime = 1000\n"
	"End\n"
	"SpecialPower SpecialAbilityPort191TriggerReady\n"
	"  Enum = SPECIAL_GENERAL_TARGETLESS\n"
	"  ReloadTime = 1000\n"
	"End\n"
	"SpecialPower SpecialAbilityPort191QueryCast\n"
	"  Enum = SPECIAL_GENERAL_TARGETLESS\n"
	"  ReloadTime = 1000\n"
	"End\n"
	"CommandButton Command_Port191TargetEnemy\n"
	"  Command = SPECIAL_POWER\n"
	"  SpecialPower = SpecialAbilityPort191TargetEnemy\n"
	"  Options = NEED_TARGET_ENEMY_OBJECT\n"
	"  AutoAbility = Yes\n"
	"End\n"
	"CommandButton Command_Port191SelfBuff\n"
	"  Command = SPECIAL_POWER\n"
	"  SpecialPower = SpecialAbilityPort191SelfBuff\n"
	"  AutoAbility = Yes\n"
	"End\n"
	"CommandButton Command_Port191PosCast\n"
	"  Command = SPECIAL_POWER\n"
	"  SpecialPower = SpecialAbilityPort191PosCast\n"
	"  Options = NEED_TARGET_POS\n"
	"  AutoAbility = Yes\n"
	"End\n"
	"CommandButton Command_Port191TriggerReady\n"
	"  Command = SPECIAL_POWER\n"
	"  SpecialPower = SpecialAbilityPort191TriggerReady\n"
	"  AutoAbility = Yes\n"
	"  TriggerWhenReady = Yes\n"
	"End\n"
	"CommandButton Command_Port191QueryCast\n"
	"  Command = SPECIAL_POWER\n"
	"  SpecialPower = SpecialAbilityPort191QueryCast\n"
	"  Options = NEED_TARGET_ENEMY_OBJECT\n"
	"  AutoAbility = Yes\n"
	"  NeedDamagedTarget = Yes\n"
	"End\n"
	"CommandSet Port191HeroCommandSet\n"
	"  1 = Command_Port191TargetEnemy\n"
	"  2 = Command_Port191SelfBuff\n"
	"  3 = Command_Port191PosCast\n"
	"  4 = Command_Port191TriggerReady\n"
	"  5 = Command_Port191QueryCast\n"
	"End\n"
	"Weapon Port191GroundSword\n"
	"  AttackRange = 30.0\n"
	"  DelayBetweenShots = 500\n"
	"  AntiGround = Yes\n"
	"  AntiAirborneVehicle = No\n"
	"  AntiAirborneInfantry = No\n"
	"  AntiAirborneMonster = No\n"
	"  DamageNugget\n"
	"    Damage = 10.0\n"
	"    Radius = 0.0\n"
	"    DelayTime = 0\n"
	"    DamageType = SLASH\n"
	"    DeathType = NORMAL\n"
	"  End\n"
	"End\n"
	"Object Port191Hero\n"
	"  KindOf = PRELOAD SELECTABLE CAN_ATTACK INFANTRY HERO\n"
	"  VisionRange = 200.0\n"
	"  ShroudClearingRange = 200.0\n"
	"  CommandSet = Port191HeroCommandSet\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY Port191GroundSword\n"
	"  End\n"
	"  Body = ActiveBody BodyTag\n"
	"    MaxHealth = 500.0\n"
	"  End\n"
	"  Behavior = AIUpdateInterface AiTag\n"
	"  End\n"
	"  Behavior = AttributeModifierPoolUpdate PoolTag\n"
	"  End\n"
	"  Behavior = SpecialPowerModule TargetEnemyPowerTag\n"
	"    SpecialPowerTemplate = SpecialAbilityPort191TargetEnemy\n"
	"    AttributeModifier = Port191Buff\n"
	"    AttributeModifierRange = 10.0\n"
	"    AttributeModifierAffectsSelf = Yes\n"
	"    AttributeModifierAffects = ANY +INFANTRY +HERO\n"
	"  End\n"
	"  Behavior = SpecialPowerModule SelfBuffPowerTag\n"
	"    SpecialPowerTemplate = SpecialAbilityPort191SelfBuff\n"
	"    AttributeModifier = Port191Buff\n"
	"    AttributeModifierRange = 10.0\n"
	"    AttributeModifierAffectsSelf = Yes\n"
	"    AttributeModifierAffects = ANY +INFANTRY +HERO\n"
	"  End\n"
	"  Behavior = SpecialPowerModule PosCastPowerTag\n"
	"    SpecialPowerTemplate = SpecialAbilityPort191PosCast\n"
	"    AttributeModifier = Port191Buff\n"
	"    AttributeModifierRange = 10.0\n"
	"    AttributeModifierAffectsSelf = Yes\n"
	"    AttributeModifierAffects = ANY +INFANTRY +HERO\n"
	"  End\n"
	"  Behavior = SpecialPowerModule TriggerReadyPowerTag\n"
	"    SpecialPowerTemplate = SpecialAbilityPort191TriggerReady\n"
	"    AttributeModifier = Port191Buff\n"
	"    AttributeModifierRange = 10.0\n"
	"    AttributeModifierAffectsSelf = Yes\n"
	"    AttributeModifierAffects = ANY +INFANTRY +HERO\n"
	"  End\n"
	"  Behavior = SpecialPowerModule QueryCastPowerTag\n"
	"    SpecialPowerTemplate = SpecialAbilityPort191QueryCast\n"
	"    AttributeModifier = Port191Buff\n"
	"    AttributeModifierRange = 10.0\n"
	"    AttributeModifierAffectsSelf = Yes\n"
	"    AttributeModifierAffects = ANY +INFANTRY +HERO\n"
	"  End\n"
	"  Behavior = AutoAbilityBehavior ModuleTag_QueryAutoAbility\n"
	"    SpecialAbility = SpecialAbilityPort191QueryCast\n"
	"    MaxScanRange = 250.0\n"
	"    MinScanRange = 40.0\n"
	"    AllowSelf = No\n"
	"    Query = 1 ANY ENEMIES +HERO\n"
	"  End\n"
	"  Behavior = AutoAbilityBehavior ModuleTag_NoQueryAutoAbility\n"
	"  End\n"
	"End\n"
	"Object Port191ShortSightHero\n"
	"  KindOf = PRELOAD SELECTABLE CAN_ATTACK INFANTRY HERO\n"
	"  VisionRange = 100.0\n"
	"  ShroudClearingRange = 100.0\n"
	"  CommandSet = Port191HeroCommandSet\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY Port191GroundSword\n"
	"  End\n"
	"  Body = ActiveBody BodyTag\n"
	"    MaxHealth = 500.0\n"
	"  End\n"
	"  Behavior = AIUpdateInterface AiTag\n"
	"  End\n"
	"  Behavior = AttributeModifierPoolUpdate PoolTag\n"
	"  End\n"
	"  Behavior = SpecialPowerModule TargetEnemyPowerTag\n"
	"    SpecialPowerTemplate = SpecialAbilityPort191TargetEnemy\n"
	"    AttributeModifier = Port191Buff\n"
	"    AttributeModifierRange = 10.0\n"
	"    AttributeModifierAffectsSelf = Yes\n"
	"    AttributeModifierAffects = ANY +INFANTRY +HERO\n"
	"  End\n"
	"  Behavior = AutoAbilityBehavior ModuleTag_NoQueryAutoAbility\n"
	"  End\n"
	"End\n"
	"Object Port191ValidEnemy\n"
	"  KindOf = PRELOAD SELECTABLE CAN_ATTACK INFANTRY\n"
	"  Body = ActiveBody BodyTag\n"
	"    MaxHealth = 200.0\n"
	"  End\n"
	"  Behavior = AIUpdateInterface AiTag\n"
	"  End\n"
	"End\n"
	"Object Port191EnemyHero\n"
	"  KindOf = PRELOAD SELECTABLE CAN_ATTACK INFANTRY HERO\n"
	"  Body = ActiveBody BodyTag\n"
	"    MaxHealth = 200.0\n"
	"  End\n"
	"  Behavior = AIUpdateInterface AiTag\n"
	"  End\n"
	"End\n"
	"Object Port191NotAutoAcquirableEnemy\n"
	"  KindOf = PRELOAD SELECTABLE INFANTRY NOT_AUTOACQUIRABLE\n"
	"  Body = ActiveBody BodyTag\n"
	"    MaxHealth = 200.0\n"
	"  End\n"
	"  Behavior = AIUpdateInterface AiTag\n"
	"  End\n"
	"End\n"
	"Object Port191AirborneEnemy\n"
	"  KindOf = PRELOAD SELECTABLE VEHICLE AIRCRAFT\n"
	"  Body = ActiveBody BodyTag\n"
	"    MaxHealth = 200.0\n"
	"  End\n"
	"  Behavior = AIUpdateInterface AiTag\n"
	"  End\n"
	"End\n";

struct Port191World : combattest::CombatWorld
{
	SpecialPowerStore powers;
	SpecialPowerStore *savedPowers = TheSpecialPowerStore;
	CommandStore commandStore;
	CommandStore *savedCommands = TheCommandStore;
	FXListStore fxLists;
	FXListStore *savedFx = TheFXListStore;
	AttributeModifierStore modifiers;
	AttributeModifierStore *savedModifiers = TheAttributeModifierStore;

	Port191World()
		: combattest::CombatWorld("", 40)
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
		HeroAbilityModules::registerHandlers(dispatcher);

		const std::string err = w.load(kPort191Ini, INI_LOAD_OVERWRITE, "port191.ini");
		REQUIRE_MESSAGE(err.empty(), err);

		logic->partition().setRegion(0.0f, 0.0f, 4000.0f, 4000.0f);
		logic->combat().setAutoAcquireEnabled(false);
	}

	~Port191World()
	{
		logic->reset();
		TheSpecialPowerStore = savedPowers;
		TheCommandStore = savedCommands;
		TheFXListStore = savedFx;
		TheAttributeModifierStore = savedModifiers;
	}

	SpecialPowerModule *powerOf(Object &o, const char *powerName)
	{
		for (const std::unique_ptr<BehaviorModule> &m : o.modules())
		{
			SpecialPowerModule *sp = dynamic_cast<SpecialPowerModule *>(m.get());
			if (sp && sp->getSpecialPowerTemplate() && sp->getSpecialPowerTemplate()->getName() == powerName)
			{
				return sp;
			}
		}
		return nullptr;
	}

	void makeReady(Object &o, const char *powerName)
	{
		SpecialPowerModule *sp = powerOf(o, powerName);
		REQUIRE(sp);
		sp->refreshTimer();
		REQUIRE(sp->getPercentReady() == 1.0f);
	}
};

} // namespace

// RW 0x85DD3A -> RW 0x701443(object, range, 0x4A, 0, 0, 0): an AutoAbilityBehavior without a Query searches via AI::findClosestEnemy with flags
// 0x4A (CAN_ATTACK | ATTACK_BUILDINGS | AUTO_ACQUIRE), skipping a nearer NOT_AUTOACQUIRABLE enemy (AutoAcquireKindFilter, 0x40) and a nearer airborne enemy
// the hero's ground weapon cannot attack (PossibleToAttackFilter, 0x2), and picks nothing while the caster itself has NO_AUTO_ACQUIRE (RW 0x7006D7)
TEST_CASE("PORT-191: an auto ability without a Query picks RW 0x701443's target with flags 0x4A, skipping nearer targets that 0x4A refuses")
{
	Port191World f;
	Object *hero = f.unit("Port191Hero", 'A', 300.0f, 300.0f);
	Object *refusedNotAuto = f.unit("Port191NotAutoAcquirableEnemy", 'B', 340.0f, 300.0f); // distance 40: refused by 0x40 (AUTO_ACQUIRE)
	Object *refusedAir = f.unit("Port191AirborneEnemy", 'B', 360.0f, 300.0f);             // distance 60: refused by 0x02 (CAN_ATTACK) when airborne
	Object *validEnemy = f.unit("Port191ValidEnemy", 'B', 420.0f, 300.0f);                // distance 120: accepted by 0x4A
	REQUIRE(hero);
	REQUIRE(refusedNotAuto);
	REQUIRE(refusedAir);
	REQUIRE(validEnemy);

	const int kAirborneTarget = ObjectTemplateInfoBuilder::objectStatusIndex("AIRBORNE_TARGET");
	REQUIRE(kAirborneTarget == 6);
	refusedAir->setStatus((unsigned)kAirborneTarget, true);
	REQUIRE(hero->getWeapons());
	REQUIRE_FALSE(hero->getWeapons()->canAttackObject(*refusedAir, CMD_FROM_AI, false));
	REQUIRE(hero->getWeapons()->canAttackObject(*validEnemy, CMD_FROM_AI, false));

	f.makeReady(*hero, "SpecialAbilityPort191TargetEnemy");
	AutoAbilityBehavior *aa = AutoAbilityBehavior::forPower(*hero, "SpecialAbilityPort191TargetEnemy");
	REQUIRE(aa);
	const CommandButton *btn = TheCommandStore->findCommandButton("Command_Port191TargetEnemy");
	REQUIRE(btn);
	aa->setAutoButton(btn);
	REQUIRE(aa->autoButton() == "Command_Port191TargetEnemy");

	// RW 0x7006D7: with 0x40 (AUTO_ACQUIRE), a caster with NO_AUTO_ACQUIRE returns nullptr before scanning
	const int kNoAutoAcquire = ObjectTemplateInfoBuilder::objectStatusIndex("NO_AUTO_ACQUIRE");
	REQUIRE(kNoAutoAcquire == 23);
	hero->setStatus((unsigned)kNoAutoAcquire, true);
	f.frames(3);
	CHECK(aa->castRecord().casts == 0);

	hero->setStatus((unsigned)kNoAutoAcquire, false);
	for (int i = 0; i < 15 && aa->castRecord().casts == 0; ++i)
	{
		f.frames(1);
	}
	REQUIRE(aa->castRecord().casts == 1);
	CHECK(aa->castRecord().target == validEnemy->getID());
}

// RW 0x85DD69 .. 0x85DD70: once a target is chosen, AutoAbilityBehavior writes AI + 0x48 := 2 (CMD_FROM_AI);
// RW 0x85D693 .. 0x85D6A1: while AI + 0x34 (m_temporaryKeptMachine) is non-null, canAct (RW 0x85D5EA) refuses
TEST_CASE("PORT-191: AutoAbilityBehavior checks AI + 0x34 in canAct (RW 0x85D69B) and writes AI + 0x48 := CMD_FROM_AI on a cast (RW 0x85DD70)")
{
	Port191World f;
	Object *hero = f.unit("Port191Hero", 'A', 300.0f, 300.0f);
	Object *enemy = f.unit("Port191ValidEnemy", 'B', 380.0f, 300.0f);
	REQUIRE(hero);
	REQUIRE(enemy);

	AIUpdateInterface *ai = hero->getAIUpdateInterface();
	REQUIRE(ai);
	f.makeReady(*hero, "SpecialAbilityPort191SelfBuff");

	AutoAbilityBehavior *aa = AutoAbilityBehavior::forPower(*hero, "SpecialAbilityPort191SelfBuff");
	REQUIRE(aa);
	const CommandButton *btn = TheCommandStore->findCommandButton("Command_Port191SelfBuff");
	REQUIRE(btn);
	aa->setAutoButton(btn);

	// RW 0x85D693 .. 0x85D6A1: AI + 0x34 non-null blocks canAct
	ai->beginTemporaryStateMachine();
	REQUIRE(ai->hasTemporaryStateMachine());
	ai->setLastCommandSource(CMD_FROM_PLAYER);
	f.frames(10);
	CHECK(aa->castRecord().casts == 0);
	CHECK(ai->lastCommandSource() == CMD_FROM_PLAYER);

	// Ending the temporary state machine clears AI + 0x34; the next auto ability scan picks enemy, sets AI + 0x48 := CMD_FROM_AI (2), and casts
	ai->endTemporaryStateMachine();
	REQUIRE_FALSE(ai->hasTemporaryStateMachine());
	ai->setLastCommandSource(CMD_FROM_PLAYER);
	REQUIRE(ai->lastCommandSource() == CMD_FROM_PLAYER);

	for (int i = 0; i < 15 && aa->castRecord().casts == 0; ++i)
	{
		f.frames(1);
	}
	REQUIRE(aa->castRecord().casts == 1);
	CHECK(ai->lastCommandSource() == CMD_FROM_AI);
}

// RW 0x85DB30 -> RW 0x68E43B: when MaxScanRange and PresetRange are 0, AutoAbilityBehavior uses Object::getVisionRange (including VISION attribute modifiers)
TEST_CASE("PORT-191: AutoAbilityBehavior uses Object::getVisionRange with VISION attribute modifiers (RW 0x85DB30 -> RW 0x68E43B)")
{
	Port191World f;
	Object *hero = f.unit("Port191ShortSightHero", 'A', 300.0f, 300.0f); // base VisionRange = 100
	Object *enemy = f.unit("Port191ValidEnemy", 'B', 460.0f, 300.0f);    // distance 160: outside 100, inside 200
	REQUIRE(hero);
	REQUIRE(enemy);

	f.makeReady(*hero, "SpecialAbilityPort191TargetEnemy");
	AutoAbilityBehavior *aa = AutoAbilityBehavior::forPower(*hero, "SpecialAbilityPort191TargetEnemy");
	REQUIRE(aa);
	const CommandButton *btn = TheCommandStore->findCommandButton("Command_Port191TargetEnemy");
	REQUIRE(btn);
	aa->setAutoButton(btn);

	// Without a VISION modifier, VisionRange is 100 < 160: no target in range
	CHECK(AIUpdateInterface::objectVisionRangeOf(*hero) == 100.0f);
	f.frames(10);
	CHECK(aa->castRecord().casts == 0);

	// Apply Port191VisionBoost (VISION 200%): Object::getVisionRange (RW 0x68E43B) becomes 200 >= 160, so the auto ability finds enemy and casts
	auto *pool = dynamic_cast<AttributeModifierPool *>(hero->findModule("AttributeModifierPoolUpdate"));
	REQUIRE(pool);
	REQUIRE(pool->add("Port191VisionBoost", -1));
	CHECK(AIUpdateInterface::objectVisionRangeOf(*hero) == 200.0f);

	for (int i = 0; i < 15 && aa->castRecord().casts == 0; ++i)
	{
		f.frames(1);
	}
	REQUIRE(aa->castRecord().casts == 1);
	CHECK(aa->castRecord().target == enemy->getID());
}

// RW 0x855830 (getDisabledTypesToProcess = 1 << DISABLED_HELD) and RW 0x68E5A3 / 0x68E67A / 0x68E754:
// a DISABLED_HELD object runs AutoAbilityBehavior::update and may cast via doSpecialPower / doSpecialPowerAtObject only when AUTO_ABILITY_TRIGGERED
// (0x20000000) is set; doSpecialPowerAtLocation (RW 0x68E754), TriggerWhenReady (RW 0x85DB26: fromAuto = 0), manual casts without 0x20000000, and other
// disabled types (DISABLED_PARALYZED) are refused
TEST_CASE("PORT-191: a DISABLED_HELD object auto-casts via RW 0x68E5A3 / 0x68E67A (with 0x20000000), while RW 0x68E754, TriggerWhenReady, manual casts, and DISABLED_PARALYZED are refused")
{
	Port191World f;
	Object *hero = f.unit("Port191Hero", 'A', 300.0f, 300.0f);
	Object *enemy = f.unit("Port191ValidEnemy", 'B', 360.0f, 300.0f);
	REQUIRE(hero);
	REQUIRE(enemy);

	AutoAbilityBehavior *aa = AutoAbilityBehavior::forPower(*hero, "SpecialAbilityPort191TargetEnemy");
	REQUIRE(aa);
	CHECK(aa->getDisabledTypesToProcess() == (DisabledMaskType)(1u << DISABLED_HELD));

	const SpecialPowerTemplate *targetEnemyT = TheSpecialPowerStore->findSpecialPowerTemplate("SpecialAbilityPort191TargetEnemy");
	const SpecialPowerTemplate *selfBuffT = TheSpecialPowerStore->findSpecialPowerTemplate("SpecialAbilityPort191SelfBuff");
	const SpecialPowerTemplate *posCastT = TheSpecialPowerStore->findSpecialPowerTemplate("SpecialAbilityPort191PosCast");
	REQUIRE(targetEnemyT);
	REQUIRE(selfBuffT);
	REQUIRE(posCastT);

	f.makeReady(*hero, "SpecialAbilityPort191TargetEnemy");
	f.makeReady(*hero, "SpecialAbilityPort191SelfBuff");
	f.makeReady(*hero, "SpecialAbilityPort191PosCast");
	f.makeReady(*hero, "SpecialAbilityPort191TriggerReady");

	// Hold the hero with DISABLED_HELD (bit 3)
	hero->setDisabled(DISABLED_HELD, f.logic->getFrame() + 1000u);
	REQUIRE(hero->getDisabledMask() == (1u << DISABLED_HELD));

	// Manual casts (without 0x20000000) on a DISABLED_HELD object are refused by RW 0x68E5A3 / 0x68E67A
	CHECK_FALSE(SpecialPowerModules::doSpecialPower(*hero, selfBuffT, 0x40000u, false));
	CHECK_FALSE(SpecialPowerModules::doSpecialPowerAtObject(*hero, targetEnemyT, enemy, 0x40000u, false));

	// TriggerWhenReady (RW 0x85DB26) calls doCommand with fromAuto = false (no 0x20000000), so it does not cast while DISABLED_HELD
	const CommandButton *trigBtn = TheCommandStore->findCommandButton("Command_Port191TriggerReady");
	REQUIRE(trigBtn);
	aa->setAutoButton(trigBtn);
	f.frames(5);
	CHECK(f.powerOf(*hero, "SpecialAbilityPort191TriggerReady")->applied() == 0);

	// NEED_TARGET_POS (RW 0x6979D9 -> RW 0x68E754 doSpecialPowerAtLocation) has no 0x20000000 disabled exception, so it does not cast while DISABLED_HELD
	const CommandButton *posBtn = TheCommandStore->findCommandButton("Command_Port191PosCast");
	REQUIRE(posBtn);
	aa->setAutoButton(posBtn);
	f.frames(5);
	CHECK(aa->castRecord().casts == 0);
	CHECK(f.powerOf(*hero, "SpecialAbilityPort191PosCast")->applied() == 0);

	// NEED_TARGET_ENEMY_OBJECT (RW 0x697890 -> RW 0x68E67A doSpecialPowerAtObject with 0x20000000) DOES auto-cast while DISABLED_HELD
	const CommandButton *enemyBtn = TheCommandStore->findCommandButton("Command_Port191TargetEnemy");
	REQUIRE(enemyBtn);
	aa->setAutoButton(enemyBtn);
	for (int i = 0; i < 15 && aa->castRecord().casts == 0; ++i)
	{
		f.frames(1);
	}
	REQUIRE(aa->castRecord().casts == 1);
	CHECK(aa->castRecord().target == enemy->getID());
	CHECK(f.powerOf(*hero, "SpecialAbilityPort191TargetEnemy")->applied() == 1);

	// Targetless auto ability (RW 0x696FD2 -> RW 0x68E5A3 doSpecialPower with 0x20000000) also auto-casts while DISABLED_HELD
	const CommandButton *selfBtn = TheCommandStore->findCommandButton("Command_Port191SelfBuff");
	REQUIRE(selfBtn);
	aa->setAutoButton(selfBtn);
	for (int i = 0; i < 15 && aa->castRecord().casts < 2; ++i)
	{
		f.frames(1);
	}
	REQUIRE(aa->castRecord().casts == 2);
	CHECK(f.powerOf(*hero, "SpecialAbilityPort191SelfBuff")->applied() == 1);

	// Another disabled bit (DISABLED_PARALYZED, bit 4) is NOT in getDisabledTypesToProcess (RW 0x855830): neither update nor RW 0x68E5A3 allows a cast
	hero->clearDisabled(DISABLED_HELD);
	f.makeReady(*hero, "SpecialAbilityPort191SelfBuff");
	hero->setDisabled(DISABLED_PARALYZED, f.logic->getFrame() + 1000u);
	CHECK_FALSE(SpecialPowerModules::doSpecialPower(*hero, selfBuffT, 0x40000u | 0x20000000u, false));
	f.frames(10);
	CHECK(aa->castRecord().casts == 2);
}

// RW 0x85DBDA .. 0x85DD16 / RW 0x85D4B5: a module with a Query keeps its partition scan + pickTarget (AllowSelf, NeedDamagedTarget <= 0.8, MinScanRange)
// and does not fall back to RW 0x701443
TEST_CASE("PORT-191: an AutoAbilityBehavior with a Query keeps its Query scan and pickTarget (RW 0x85D4B5)")
{
	Port191World f;
	Object *hero = f.unit("Port191Hero", 'A', 300.0f, 300.0f);
	Object *nearInfantry = f.unit("Port191ValidEnemy", 'B', 360.0f, 300.0f);   // not HERO: filtered out by Query
	Object *tooCloseHero = f.unit("Port191EnemyHero", 'B', 320.0f, 300.0f);    // distance 20 < MinScanRange 40: skipped by pickTarget
	Object *fullHealthHero = f.unit("Port191EnemyHero", 'B', 380.0f, 300.0f);  // health ratio 1.0 > 0.8: skipped by NeedDamagedTarget
	Object *damagedHero = f.unit("Port191EnemyHero", 'B', 420.0f, 300.0f);     // distance 120 >= 40, damaged to 50% <= 80%: picked
	REQUIRE(hero);
	REQUIRE(nearInfantry);
	REQUIRE(tooCloseHero);
	REQUIRE(fullHealthHero);
	REQUIRE(damagedHero);

	DamageInfo hurt;
	hurt.m_input.m_damageType = DAMAGE_UNRESISTABLE;
	hurt.m_input.m_amount = 100.0f;
	tooCloseHero->attemptDamage(hurt);
	damagedHero->attemptDamage(hurt);
	REQUIRE(damagedHero->getBodyModule()->getHealth() == 100.0f);

	f.makeReady(*hero, "SpecialAbilityPort191QueryCast");
	AutoAbilityBehavior *aa = AutoAbilityBehavior::forPower(*hero, "SpecialAbilityPort191QueryCast");
	REQUIRE(aa);
	REQUIRE(aa->data()->m_specialAbility == "SpecialAbilityPort191QueryCast");
	const CommandButton *btn = TheCommandStore->findCommandButton("Command_Port191QueryCast");
	REQUIRE(btn);
	aa->setAutoButton(btn);

	const unsigned long long closestBefore = AutoAbilityBehavior::stats().closestEnemyQueries;
	for (int i = 0; i < 15 && aa->castRecord().casts == 0; ++i)
	{
		f.frames(1);
	}
	REQUIRE(aa->castRecord().casts == 1);
	CHECK(aa->castRecord().target == damagedHero->getID());
	CHECK(AutoAbilityBehavior::stats().closestEnemyQueries == closestBefore);
}
