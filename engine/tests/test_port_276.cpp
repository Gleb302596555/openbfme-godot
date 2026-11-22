// OpenBFME. GPL-3.0.
// PORT-276: targetless detonation, including the dying firer's INITIAL / FINAL weapon.
// Synthetic INI only. The trigger is a self-kill, not the enemy-facing blast:
// RW 0x8961F7 (0x8962D8..0x896313) -> RW 0x6CF3D2 -> RW 0x6CEF6D -> RW 0x6CC915;
// RW 0x90E28C (0x90E603..0x90E61A) sets kill for SUICIDE against the source itself.
// The death weapon is fired at a position before destruction (RW 0x8609A8 / RW 0x860B39).

#include "doctest.h"
#include "CombatTestUtil.h"

#include "Common/SpecialPower.h"
#include "GameClient/ControlBarCommands.h"
#include "GameClient/FXList.h"
#include "GameLogic/AttributeModifiers.h"
#include "GameLogic/Module/EmotionModules.h"
#include "GameLogic/Module/HeroAbilityModules.h"
#include "GameLogic/Module/SpecialAbilityModules.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Object/AttributeModifierPool.h"
#include "GameLogic/ObjectCreationList.h"
#include "GameLogic/WeaponNugget.h"

#include <string>

namespace
{
const char kPort276Ini[] = R"ini(
SpecialPower P276Detonation
 Enum = SPECIAL_GENERAL_TARGETLESS
 ReloadTime = 60000
End
Weapon P276Igniter
 AttackRange = 500
 DelayBetweenShots = 0
 PreAttackDelay = 0
 FiringDuration = 0
 RadiusDamageAffects = ALLIES SELF SUICIDE
 DamageDealtAtSelfPosition = Yes
 DamageNugget
  Damage = 3
  Radius = 12
  DamageType = SLASH
  DeathType = BURNED
 End
End
Weapon P276Explosion
 AttackRange = 4
 RadiusDamageAffects = ENEMIES NEUTRALS SELF
 DamageNugget
  Damage = 37
  Radius = 48
  DamageType = FLAME
  DeathType = BURNED
 End
End
Object P276Trap
 KindOf = SELECTABLE IMMOBILE MINE DEPLOYED_MINE
 Geometry = SPHERE
 GeometryMajorRadius = 2
 Body = ActiveBody BodyTag
  MaxHealth = 120
 End
 Behavior = AIUpdateInterface AiTag
  AutoAcquireEnemiesWhenIdle = No
 End
 Behavior = PhysicsBehavior PhysicsTag
 End
 Behavior = SlowDeathBehavior ExplosionTag
  DeathTypes = NONE +BURNED
  DestructionDelay = 400
  Weapon = INITIAL P276Explosion
 End
 Behavior = SlowDeathBehavior OtherDeathTag
  DeathTypes = ALL -BURNED
  DestructionDelay = 400
 End
 Behavior = OCLSpecialPower PowerTag
  SpecialPowerTemplate = P276Detonation
  CreateLocation = CREATE_AT_LOCATION
 End
 Behavior = WeaponFireSpecialAbilityUpdate AbilityTag
  SpecialPowerTemplate = P276Detonation
  SpecialWeapon = P276Igniter
  WhichSpecialWeapon = 1
  UnpackTime = 600
  MustFinishAbility = Yes
  AwardXPForTriggering = 0
 End
End
Object P276Victim
 KindOf = INFANTRY IMMOBILE
 Geometry = SPHERE
 GeometryMajorRadius = 1
 Body = ActiveBody BodyTag
  MaxHealth = 200
 End
End
)ini";

struct DetonationWorld : combattest::CombatWorld
{
	SpecialPowerStore powers;
	SpecialPowerStore *savedPowers = TheSpecialPowerStore;

	explicit DetonationWorld(bool finalWeapon = false, bool suicide = true)
	{
		logic->setEdition(GameEdition::RotWK201);
		combat().setAutoAcquireEnabled(false);
		TheSpecialPowerStore = &powers;
		w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
		SpecialAbilityModules::registerAll(w.modules);
		SpecialPowerModules::registerAll(w.modules);
		std::string data(kPort276Ini);
		if (finalWeapon)
		{
			const std::string initial = "Weapon = INITIAL P276Explosion";
			data.replace(data.find(initial), initial.size(), "Weapon = FINAL P276Explosion");
		}
		if (!suicide)
		{
			const std::string mask = "ALLIES SELF SUICIDE";
			data.replace(data.find(mask), mask.size(), "ALLIES SELF");
		}
		const std::string error = w.load(data, INI_LOAD_OVERWRITE, "port276.ini");
		REQUIRE_MESSAGE(error.empty(), error);
	}

	~DetonationWorld()
	{
		logic->reset();
		TheSpecialPowerStore = savedPowers;
	}

	bool detonate(Object &trap)
	{
		const SpecialPowerTemplate *power = powers.findSpecialPowerTemplate("P276Detonation");
		REQUIRE(power);
		return SpecialPowerModules::doSpecialPower(trap, power, 0, false);
	}
};
} // namespace

TEST_CASE("PORT-276 targetless self-kill fires the trap's INITIAL or FINAL radius weapon before destroying the trap")
{
	for (bool finalWeapon : { false, true })
	{
		CAPTURE(finalWeapon);
		DetonationWorld world(finalWeapon);
		Object *trap = world.unit("P276Trap", 'A', 300, 300);
		Object *inside = world.unit("P276Victim", 'B', 330, 300);
		Object *outside = world.unit("P276Victim", 'B', 400, 300);
		Object *ally = world.unit("P276Victim", 'A', 330, 310);
		REQUIRE(trap);
		REQUIRE(inside);
		REQUIRE(outside);
		REQUIRE(ally);
		const ObjectID trapId = trap->getID();
		world.frames(2);
		CHECK(world.health(inside) == 200);
		CHECK(world.health(outside) == 200);
		CHECK(world.health(ally) == 200);
		REQUIRE(world.detonate(*trap)); // no victim and no location argument
		world.frames(20);

		// RW 0x6CC915 (0x6CCBFF..0x6CCC51) uses the firer's position, not (0,0,0),
		// then calls each nugget's position slot (0x6CCEFA..0x6CCF17).
		// RW 0x90DEF0 queries the radius; SUICIDE's BURNED death selects the explosion.
		// Exactly one 37-point blast: neither the 3-point allies-only igniter nor two shots.
		CHECK(world.health(inside) == 163);
		CHECK(world.health(outside) == 200);
		CHECK(world.health(ally) == 200);
		CHECK(world.byId(trapId) == nullptr); // never dereference a trap after its death
		const auto *body = dynamic_cast<const ActiveBody *>(inside->getBodyModule());
		REQUIRE(body);
		CHECK(body->lastDamager() == trapId); // the dead firer, not a replacement object
	}
}

TEST_CASE("PORT-276 neighbouring self-position weapon without SUICIDE only wounds the firer and does not fire a death weapon")
{
	DetonationWorld world(false, false);
	Object *trap = world.unit("P276Trap", 'A', 300, 300);
	Object *enemy = world.unit("P276Victim", 'B', 306, 300); // inside the igniter's radius, but not its relationship mask
	REQUIRE(trap);
	REQUIRE(enemy);
	const ObjectID trapId = trap->getID();
	world.frames(2);
	REQUIRE(world.detonate(*trap));
	world.frames(20);

	// RW 0x90E28C (0x90E603..0x90E61A): without bit 0x10, ordinary damage, no forced kill.
	REQUIRE(world.byId(trapId));
	CHECK_FALSE(world.byId(trapId)->isEffectivelyDead());
	CHECK(world.health(world.byId(trapId)) == 117);
	CHECK(world.health(enemy) == 200);
	const auto *ability = dynamic_cast<const WeaponFireSpecialAbilityUpdate *>(world.byId(trapId)->findModule("WeaponFireSpecialAbilityUpdate"));
	REQUIRE(ability);
	CHECK(ability->shotsFired() == 1);
}

namespace
{
// Re-authored synthetic scene with the simulation values of Ennorath 1.9.8.3:
// includes/weapon.inc:37573..37687; _gamedata.inc:1355 (poison filter);
// object/evilfaction/units/harad/salemetraps.ini:1..395; armor.ini:3243;
// includes/attributemodifier.inc:11676; specialpower.ini:3061;
// includes/objectcreationlist.inc:92; object/projectile.ini:24855.
// ChildObject inheritance, zero UnpackTime, armour, invisibility, auto-ability,
// every weapon nugget and the real OCL helper are retained. Rendering/audio
// resources are not copied: the named FX lists are empty synthetic resources.
// The targets' names, health, armour and positions are scene controls, not mod data.
const char kSaleme276Ini[] = R"ini(
#define WEAK_HERO_HERORESIST 0.80
#define AFFECTED_BY_POISON_OBJECTFILTER ALL -MACHINE -STRUCTURE -SHIP -MordorBalrogAI -MordorBalrog -MordorSauron -MordorWitchKing -MordorWitchKingOnFellBeast -MordorRingWraith -MordorRingWraithNoGlow -EvilMenBlackRider -BarrowWight -RohanOathbreaker -IsengardDeployedExplosiveMine -IsengardDeployedExplosiveMineAI -MordorBalrog -TheDroppedRing -BattleWagonOilBarrelProjectile -DwarvenCatapult -AngmarWitchking_Mounted -BlackRiderMountedInterface -MordorFellBeast -MordorFellBeastInterface -MorgomirFellBeast -KhamulFellBeast -GondorGwaihir -GondorGwaihir_Summoned -WildSmaug_mod -WildSmaugNewRingFly -WildDrogothnew -DwarvenCarc -MordorWitchKingOnFellBeastSpell -GasthausKhamulonFellBeastSpell -ImladrisGwaihir -ImladrisGwaihirMod -ImladrisGwaihir_Summoned -ElvenFortressEagle -Crebain -RadagastCrebainNew -WildCaveBats -MordorFellBeast_edainmod -GasthausKhamulonFellBeast -RohanRadagastaufdemAdler_RingHero -RohanRadagastaufdemAdler -RadagastAdler_Summoned -MordorFellBeast_Summoned -ManveEagle -DrogothGround -GondorDenethor_forFortress -MordorNekromantReborn -MordorNekromantWotr
SpecialPower SpecialAbilityDetonate
 Enum = SPECIAL_GENERAL_TARGETLESS
 ReloadTime = 999999999
End
CommandButton Command_SalemeGiftfalleDetonate
 Command = SPECIAL_POWER
 SpecialPower = SpecialAbilityDetonate
 Options = OK_FOR_MULTI_SELECT OK_FOR_MULTI_EXECUTE
 InPalantir = Yes
 Radial = Yes
 AutoAbility = Yes
End
CommandButton Command_SalemeBlendfalleDetonate
 Command = SPECIAL_POWER
 SpecialPower = SpecialAbilityDetonate
 Options = OK_FOR_MULTI_SELECT OK_FOR_MULTI_EXECUTE
 InPalantir = Yes
 Radial = Yes
 AutoAbility = Yes
End
CommandButton Command_SalemeExplosionsfalleDetonate
 Command = SPECIAL_POWER
 SpecialPower = SpecialAbilityDetonate
 Options = OK_FOR_MULTI_SELECT OK_FOR_MULTI_EXECUTE
 InPalantir = Yes
 Radial = Yes
 AutoAbility = Yes
End
CommandSet SalemeGiftfalleCommandSet
 1 = Command_SalemeGiftfalleDetonate
End
CommandSet SalemeBlendfalleCommandSet
 1 = Command_SalemeBlendfalleDetonate
End
CommandSet SalemeExplosionsfalleCommandSet
 1 = Command_SalemeExplosionsfalleDetonate
End
Armor SalemeTrapsArmor
 Armor = DEFAULT 100%
 Armor = PIERCE 5%
 Armor = LOGICAL_FIRE 50000%
 Armor = FLAME 50000%
 Armor = FROST 100%
 Armor = POISON 0%
 Armor = STRUCTURAL 10%
End
FXList FX_GenericDebuff10Sek
End
FXList FX_ScorpionGiftwolke
End
FXList FX_Blendfalle
End
FXList FX_Explosionsfalle
End
FXList FX_ArrowVolleyPhaseInitialWeapon
End
ModifierList BlendfalleDebuff
 Category = DEBUFF
 Modifier = ARMOR -20%
 Modifier = SHROUD_CLEARING -100%
 Modifier = VISION -100%
 Modifier = RANGE -100%
 Duration = 10000
 FX = FX_GenericDebuff10Sek
End
Weapon SalemeDetonateFallen
 LeechRangeWeapon = Yes
 AttackRange = 9999.0
 MeleeWeapon = No
 DelayBetweenShots = 0
 PreAttackDelay = 0
 PreAttackType = PER_SHOT
 FiringDuration = 0
 RadiusDamageAffects = ALLIES SELF SUICIDE
 DamageDealtAtSelfPosition = Yes
 DamageNugget
  Damage = 1
  Radius = 20.0
  DelayTime = 0
  DamageType = SLASH
  DamageFXType = SWORD_SLASH
  DeathType = BURNED
 End
End
Weapon SalemeExplodeGiftfalleWeapon
 AttackRange = 4.0
 DelayBetweenShots = 2000
 FireFX = FX_ScorpionGiftwolke
 RadiusDamageAffects = ENEMIES NEUTRALS SELF
 DOTNugget
  Damage = 10
  AcceptDamageAdd = No
  Radius = 80.0
  DelayTime = 0
  DamageType = POISON
  DamageFXType = POISON
  DeathType = NORMAL
  DamageInterval = 1000
  DamageDuration = 15000
  SpecialObjectFilter = AFFECTED_BY_POISON_OBJECTFILTER
 End
End
Weapon SalemeExplodeGiftfalleWeaponStrong
 AttackRange = 4.0
 DelayBetweenShots = 2000
 FireFX = FX_ScorpionGiftwolke
 RadiusDamageAffects = ENEMIES NEUTRALS SELF
 DOTNugget
  Damage = 25
  AcceptDamageAdd = No
  Radius = 80.0
  DelayTime = 0
  DamageType = POISON
  DamageFXType = POISON
  DeathType = NORMAL
  DamageInterval = 1000
  DamageDuration = 15000
  SpecialObjectFilter = AFFECTED_BY_POISON_OBJECTFILTER
 End
End
Weapon SalemeExplodeBlendfalleWeapon
 AttackRange = 4.0
 DelayBetweenShots = 2000
 FireFX = FX_Blendfalle
 RadiusDamageAffects = ENEMIES NEUTRALS SELF
 EmotionWeaponNugget
  EmotionType = UNCONTROLLABLE_FEAR
  Radius = 50.0
  Duration = 10
  SpecialObjectFilter = ANY +INFANTRY +CAVALRY +MONSTER -HERO -STRUCTURE ENEMIES
 End
 AttributeModifierNugget
  AttributeModifier = BlendfalleDebuff
  Radius = 50.0
  DamageFXType = SWORD_SLASH
  SpecialObjectFilter = ANY +INFANTRY +CAVALRY +MONSTER -HERO -STRUCTURE ENEMIES
 End
End
Weapon SalemeExplodeExplosionsfalleWeapon
 AttackRange = 4.0
 DelayBetweenShots = 2000
 FireFX = FX_Explosionsfalle
 RadiusDamageAffects = ENEMIES NEUTRALS SELF
 DamageNugget
  Damage = 500
  Radius = 80
  DelayTime = 0
  DamageType = FLAME
  DamageFXType = FLAME
  DeathType = BURNED
  DamageScalar = 5000% NONE +MINE
 End
 MetaImpactNugget
  HeroResist = WEAK_HERO_HERORESIST
  ShockWaveAmount = 60.00
  ShockWaveRadius = 80
  ShockWaveTaperOff = 0.75
  ShockWaveZMult = 1.20
 End
 FireLogicNugget
  LogicType = INCREASE_BURN_RATE
  Radius = 10
  Damage = 5
 End
End
Weapon ArrowVolleyPhaseInitialWeapon
 RadiusDamageAffects = ENEMIES NEUTRALS
 FireFX = FX_ArrowVolleyPhaseInitialWeapon
End
ObjectCreationList OCL_SalemeDetonateRanged
 CreateObject
  ObjectNames = SalemeDetonateRangedObj
  OrientInSecondaryDirection = Yes
  OrientationOffset = 0
 End
End
Object SalemeDetonateRangedObj
 KindOf = NO_COLLIDE IMMOBILE UNATTACKABLE INERT
 ForceLuaRegistration = Yes
 Body = ImmortalBody ModuleTag_02
  MaxHealth = 1
  InitialHealth = 1
 End
 Behavior = DeletionUpdate ModuleTag_03
  MinLifetime = 4000
  MaxLifetime = 4000
 End
 Behavior = FireWeaponUpdate ModuleTag_DamageHandler
  FireWeaponNugget
   WeaponName = ArrowVolleyPhaseInitialWeapon
   FireDelay = 0
   OneShot = Yes
  End
 End
 Geometry = BOX
 GeometryMajorRadius = 10.0
 GeometryMinorRadius = 1.0
 GeometryHeight = 3.0
 GeometryIsSmall = No
End
Object SalemePoisonTrap
 Side = Evilmen
 EditorSorting = UNIT
 ThreatLevel = 1.0
 ThingClass = HORDE_UNIT
 CommandPoints = 0
 TransportSlotCount = 1
 ArmorSet
  Conditions = None
  Armor = SalemeTrapsArmor
 End
 VisionRange = 100.0
 CommandSet = SalemeGiftfalleCommandSet
 CrushableLevel = 2
 ShockwaveResistance = 9999
 RadarPriority = UNIT
 KindOf = PRELOAD SELECTABLE CAN_CAST_REFLECTIONS SCORE IMMOBILE NOT_AUTOACQUIRABLE MINE DEPLOYED_MINE
 Body = ActiveBody ModuleTag_02
  MaxHealth = 150
  MaxHealthDamaged = 75
 End
 Behavior = AIUpdateInterface ModuleTag_03
  AutoAcquireEnemiesWhenIdle = No
 End
 Behavior = InvisibilityUpdate ModuleTag_Camouflage
  InvisibilityNugget
   InvisibilityType = CAMOUFLAGE
   ForbiddenConditions = TAKING_DAMAGE USING_ABILITY
   DetectionRange = 50
  End
  UpdatePeriod = 1000
  StartsActive = Yes
 End
 Behavior = PhysicsBehavior ModuleTag_04
  GravityMult = 1.0
 End
 Behavior = SlowDeathBehavior ModuleTag_ExplosiveDeath
  DeathTypes = NONE +BURNED
  DeathFlags = DEATH_1
  SinkDelay = 10
  SinkRate = 4.0
  DestructionDelay = 1500
  Weapon = INITIAL SalemeExplodeGiftfalleWeapon
 End
 Behavior = SlowDeathBehavior ModuleTag_ExplosiveDeath2
  DeathTypes = ALL -BURNED
  SinkDelay = 1000
  SinkRate = 3.0
  DestructionDelay = 5000
 End
 Behavior = SquishCollide ModuleTag_06
 End
 Behavior = OCLSpecialPower ModuleTag_DetonateEffect
  SpecialPowerTemplate = SpecialAbilityDetonate
  OCL = OCL_SalemeDetonateRanged
  CreateLocation = CREATE_AT_LOCATION
 End
 Behavior = WeaponFireSpecialAbilityUpdate ModuleTag_DetonateUpdate
  SpecialPowerTemplate = SpecialAbilityDetonate
  UnpackTime = 0
  MustFinishAbility = Yes
  AwardXPForTriggering = 0
  SpecialWeapon = SalemeDetonateFallen
  WhichSpecialWeapon = 1
 End
 Behavior = AutoAbilityBehavior ModuleTag_SlamAutoAbility
  SpecialAbility = SpecialAbilityDetonate
  MaxScanRange = 80
  StartsActive = Yes
  Query = 5 ANY ENEMIES +INFANTRY +CAVALRY -STRUCTURE -WALL_UPGRADE -WALL_HUB -WALL_SEGMENT -DEFENSIVE_WALL -UNATTACKABLE -FS_BASE_DEFENSE
 End
 Geometry = CYLINDER
 GeometryMinorRadius = 8.0
 GeometryMajorRadius = 8.0
 GeometryHeight = 6.0
 GeometryIsSmall = No
End
ChildObject SalemePoisonTrapStrong SalemePoisonTrap
 Behavior = SlowDeathBehavior ModuleTag_ExplosiveDeath
  DeathTypes = NONE +BURNED
  DeathFlags = DEATH_1
  SinkDelay = 10
  SinkRate = 4.0
  DestructionDelay = 1500
  Weapon = INITIAL SalemeExplodeGiftfalleWeaponStrong
 End
End
ChildObject SalemeBlindTrap SalemePoisonTrap
 CommandSet = SalemeBlendfalleCommandSet
 Behavior = SlowDeathBehavior ModuleTag_ExplosiveDeath
  DeathTypes = NONE +BURNED
  DeathFlags = DEATH_1
  SinkDelay = 10
  SinkRate = 5.4
  DestructionDelay = 1000
  Weapon = INITIAL SalemeExplodeBlendfalleWeapon
 End
 Geometry = CYLINDER
 GeometryMinorRadius = 6.0
 GeometryMajorRadius = 6.0
 GeometryHeight = 12.0
 GeometryIsSmall = No
End
ChildObject SalemeExplodeTrap SalemePoisonTrap
 CommandSet = SalemeExplosionsfalleCommandSet
 Behavior = SlowDeathBehavior ModuleTag_ExplosiveDeath
  DeathTypes = NONE +BURNED
  DeathFlags = DEATH_1
  DestructionDelay = 1
  Weapon = FINAL SalemeExplodeExplosionsfalleWeapon
 End
 Behavior = SlowDeathBehavior ModuleTag_ExplosiveDeath2
  DeathTypes = ALL -BURNED
  DeathFlags = DEATH_1
  SinkDelay = 1
  SinkRate = 5.0
  DestructionDelay = 5000
 End
 Geometry = CYLINDER
 GeometryMinorRadius = 6.0
 GeometryMajorRadius = 6.0
 GeometryHeight = 13.0
 GeometryIsSmall = No
End
Object P276SalemeTarget
 KindOf = INFANTRY SELECTABLE
 Geometry = SPHERE
 GeometryMajorRadius = 1
 ArmorSet
  Conditions = None
  Armor = NoArmor
 End
 Body = ActiveBody BodyTag
  MaxHealth = 10000
 End
 Behavior = AIUpdateInterface AiTag
  AutoAcquireEnemiesWhenIdle = No
 End
 Behavior = AttributeModifierPoolUpdate PoolTag
 End
 Behavior = EmotionTrackerUpdate EmotionTag
 End
End
ChildObject P276SalemeExcluded P276SalemeTarget
 KindOf = MACHINE STRUCTURE
End
ChildObject P276SalemeHero P276SalemeTarget
 KindOf = INFANTRY HERO SELECTABLE
End
ChildObject BarrowWight P276SalemeTarget
End
)ini";

struct Saleme276World : DetonationWorld
{
	CommandStore commands;
	CommandStore *savedCommands = TheCommandStore;
	AttributeModifierStore modifiers;
	AttributeModifierStore *savedModifiers = TheAttributeModifierStore;
	FXListStore fxLists;
	FXListStore *savedFx = TheFXListStore;
	ObjectCreationListStore ocls;
	ObjectCreationListStore *savedOcls = TheObjectCreationListStore;

	Saleme276World() : ocls(w.keys)
	{
		TheCommandStore = &commands;
		w.fx.env.blocks.registerBlock("CommandButton", [](INI *ini) { CommandStore::parseCommandButtonDefinitionGlobal(ini); });
		w.fx.env.blocks.registerBlock("CommandSet", [](INI *ini) { CommandStore::parseCommandSetDefinitionGlobal(ini); });
		TheAttributeModifierStore = &modifiers;
		TheFXListStore = &fxLists;
		TheObjectCreationListStore = &ocls;
		modifiers.registerBlock(w.fx.env.blocks);
		w.fx.env.blocks.registerBlock("FXList", [](INI *ini) { ParseFXListDefinitionGlobal(ini); });
		w.fx.env.blocks.registerBlock("ObjectCreationList", [](INI *ini) { ObjectCreationListStore::parseObjectCreationListDefinitionGlobal(ini); });
		HeroAbilityModules::registerAll(w.modules);
		const std::string error = w.load(kSaleme276Ini, INI_LOAD_OVERWRITE, "port276-saleme.ini");
		REQUIRE_MESSAGE(error.empty(), error);
	}

	~Saleme276World()
	{
		logic->reset();
		TheObjectCreationListStore = savedOcls;
		TheFXListStore = savedFx;
		TheAttributeModifierStore = savedModifiers;
		TheCommandStore = savedCommands;
	}

	bool detonate(Object &trap)
	{
		const SpecialPowerTemplate *power = powers.findSpecialPowerTemplate("SpecialAbilityDetonate");
		REQUIRE(power);
		return SpecialPowerModules::doSpecialPower(trap, power, 0, false);
	}

	bool blinded(Object &target) const
	{
		const auto *pool = dynamic_cast<const AttributeModifierPool *>(target.attributeModifierPoolModule());
		REQUIRE(pool);
		return pool->hasList("BlendfalleDebuff");
	}
};
} // namespace

TEST_CASE("PORT-276 Saleme mod values ignite all four inherited traps and deliver their filtered INITIAL or FINAL effects")
{
	struct TrapCase
	{
		const char *name;
		float firstDamage;
		bool poison;
		bool blind;
	};
	for (const TrapCase &c : { TrapCase{ "SalemePoisonTrap", 10, true, false }, { "SalemePoisonTrapStrong", 25, true, false },
	                          { "SalemeBlindTrap", 0, false, true }, { "SalemeExplodeTrap", 500, false, false } })
	{
		CAPTURE(c.name);
		Saleme276World world;
		Object *trap = world.unit(c.name, 'A', 300, 300);
		Object *inside = world.unit("P276SalemeTarget", 'B', 330, 300);
		Object *outside = world.unit("P276SalemeTarget", 'B', 450, 300);
		Object *betweenRadii = world.unit("P276SalemeTarget", 'B', 370, 300); // inside radius 80, outside blind's 50
		Object *ally = world.unit("P276SalemeTarget", 'A', 330, 330);
		Object *excluded = world.unit("P276SalemeExcluded", 'B', 330, 310);
		Object *hero = world.unit("P276SalemeHero", 'B', 330, 290);
		Object *namedImmune = world.unit("BarrowWight", 'B', 320, 320);
		REQUIRE(trap);
		REQUIRE(inside);
		REQUIRE(outside);
		REQUIRE(betweenRadii);
		REQUIRE(ally);
		REQUIRE(excluded);
		REQUIRE(hero);
		REQUIRE(namedImmune);
		const ObjectID trapId = trap->getID();
		world.frames(2);
		trap = world.byId(trapId); // StartsActive must not auto-detonate this below-Query scene; never inspect a stale caster.
		REQUIRE(trap);
		CHECK(world.health(trap) == 150);
		CHECK(world.health(inside) == 10000);
		CHECK_FALSE(world.blinded(*inside));
		// The mod's StartsActive auto ability really has its command button.
		// Four matching enemies (below Query = 5) keep the explicit press isolated.
		const auto *autoAbility = AutoAbilityBehavior::forPower(*trap, "SpecialAbilityDetonate");
		REQUIRE(autoAbility);
		const char *buttonName = c.blind ? "Command_SalemeBlendfalleDetonate"
		                                : c.poison ? "Command_SalemeGiftfalleDetonate" : "Command_SalemeExplosionsfalleDetonate";
		const CommandButton *button = world.commands.findCommandButton(buttonName);
		REQUIRE(button);
		CHECK(autoAbility->isAutoButton(*button));
		const auto *ocl = dynamic_cast<const OCLSpecialPower *>(trap->findModule("OCLSpecialPower"));
		REQUIRE(ocl);
		REQUIRE(world.detonate(*trap)); // same targetless power, no direct weapon/damage/death call
		// OCL is not replaced with the generic fixture's empty list: it makes its real FX-only helper.
		// Observe the world, not the caster's module: the trigger is allowed to kill its caster.
		unsigned helperCount = 0;
		for (const Object *o = world.logic->getFirstObject(); o; o = o->getNextObject())
		{
			if (o->getTemplate()->getName() == "SalemeDetonateRangedObj")
			{
				++helperCount;
			}
		}
		CHECK(helperCount == 1);
		const WeaponTemplate *helperWeapon = TheWeaponStore->findWeaponTemplate("ArrowVolleyPhaseInitialWeapon");
		REQUIRE(helperWeapon);
		CHECK(helperWeapon->m_nuggets.empty());

		// RW 0x90E28C, 0x90E603..0x90E61A: the one-point allies-only SLASH
		// forces a BURNED self-kill; the enemy effect is the selected death weapon.
		world.runUntil([&]() { return c.blind ? world.blinded(*inside) : world.health(inside) != 10000; }, 20);
		CHECK(world.health(inside) == 10000 - c.firstDamage);
		CHECK(world.blinded(*inside) == c.blind);
		if (c.blind)
		{
			// weapon.inc:37646..37657: fear + modifier, not ordinary HP damage.
			const EmotionTrackerUpdate *emotions = EmotionTrackerUpdate::of(*inside);
			REQUIRE(emotions);
			// RW 0x911707 -> RW 0x68F3CB -> RW 0x8B4EB1 forces (not requests) the emotion for 10 seconds.
			CHECK(emotions->forcedType() == EMOTION_UNCONTROLLABLE_FEAR);
		}

		// RW 0x911407 registers DOT before its initial hit; RW 0x820EF0
		// continues the saved record after the trap is gone: 1000 ms = 5 frames.
		world.frames(10);
		CHECK(world.byId(trapId) == nullptr);
		CHECK(world.health(inside) == 10000 - c.firstDamage * (c.poison ? 3 : 1));
		CHECK(world.health(betweenRadii) == 10000 - c.firstDamage * (c.poison ? 3 : 1));
		CHECK_FALSE(world.blinded(*betweenRadii));
		CHECK(world.health(outside) == 10000);
		CHECK(world.health(ally) == 10000);
		CHECK(world.health(excluded) == (c.poison || c.blind ? 10000 : 9500));
		CHECK(world.health(hero) == (c.blind ? 10000 : 10000 - c.firstDamage * (c.poison ? 3 : 1)));
		CHECK(world.health(namedImmune) == (c.poison || c.blind ? 10000 : 9500)); // _gamedata.inc:1355 excludes BarrowWight by name
		CHECK(world.blinded(*namedImmune) == c.blind); // the blind filter has no such name exclusion
		CHECK_FALSE(world.blinded(*outside));
		CHECK_FALSE(world.blinded(*ally));
		CHECK_FALSE(world.blinded(*excluded));
		CHECK_FALSE(world.blinded(*hero));

		// No RequiredUpgradeNames / ForbiddenUpgradeNames occur in these blocks.
		// Keep that absence explicit rather than supplying a made-up upgrade.
		for (const char *name : { "SalemeDetonateFallen", "SalemeExplodeGiftfalleWeapon", "SalemeExplodeGiftfalleWeaponStrong",
		                          "SalemeExplodeBlendfalleWeapon", "SalemeExplodeExplosionsfalleWeapon" })
		{
			const WeaponTemplate *weapon = TheWeaponStore->findWeaponTemplate(name);
			REQUIRE(weapon);
			for (const auto &nugget : weapon->m_nuggets)
			{
				CHECK(nugget->m_requiredUpgradeNames.empty());
				CHECK(nugget->m_forbiddenUpgradeNames.empty());
			}
		}
	}
}
