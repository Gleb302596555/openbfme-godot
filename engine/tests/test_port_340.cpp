// OpenBFME. GPL-3.0.
// PORT-340 (fork issue #895): Command_BlackRiderMorgulBladeForHorde never fires on the mod's MordorSchwarzeReiterHordeFoot while the
// mounted MordorSchwarzeReiterHorde fires it. What these tests pin (see docs/ennorath/night_watch.d/PORT-340.md):
//
//   * AIGroup::groupDoSpecialPowerAtObject (RW 0x77097E) is the only one of the three group casts that drops a KindOf HERO member
//     whose experience-level handle is invalid (RW 0x770A03 -> 0x689024, `je 0x770ADD`): the member is neither a candidate of the rank
//     contest nor listed for the cast, so a group of one such object casts nothing at all (no cast, no refusal). The handle is invalid
//     when the object's template has no level list at all: the tracker's key is its own template's name (RW 0x79D400..0x79D43C:
//     `add eax, 0x64` of the ThingTemplate, the NameKeyGenerator call RW 0x49F474, stored at tracker + 0x30) and the list is the map
//     entry of that key (RW 0x68931C -> the ExperienceLevel TargetNames map, RW 0x68A83E).
//   * RotWK therefore refuses the same cast on the same template: no ExperienceLevel of Ennorath 1.9.8.7 names
//     MordorSchwarzeReiterHordeFoot (only MordorSchwarzeReiterHorde and MordorSchwarzeReiter, in BlackRiderHordeLevelMod1), so the port is
//     right and the data is the cause. The second and third cases below are that pair: the same on-foot form without a level casts
//     nothing, with its name added to the level's TargetNames it casts as the mounted form does.
//   * The contest is HERO-only (RW 0x7709D3 `test byte ptr [eax + 0x113], 4`: KindOf + 0x108, bit 90, HERO): a caster that is not HERO
//     needs no level (the fourth case).
//
// The data below is the mod's, copied as values, never the archive:
//   * data/ini/object/evilfaction/units/mordor/mordorschwarzereiter.ini — MordorSchwarzeReiterHorde (here Port340Horde): KindOf HERO
//     SELECTABLE CAN_ATTACK INFANTRY HORDE MELEE_HORDE ... CAVALRY, CommandSet SchwarzeReiterCommandSet (slot 4 =
//     Command_BlackRiderMorgulBladeForHorde), HorseHordeContain (InitialPayload MordorSchwarzeReiter 5, Slots 5, FrontAngle 270,
//     FlankedDelay 500, MeleeBehavior Amoeba, AlternateFormation = the on-foot form, ThisFormationIsTheMainFormation = Yes), the
//     SpecialPowerModule starter and WeaponFireSpecialAbilityUpdate (SkipContinue, UnpackTime 250, PreparationTime 1,
//     PersistentPrepTime 500, PackTime 100, StartAbilityRange 15.0, SpecialWeapon SchwarzeReiterMorgulBlade, WhichSpecialWeapon 1); its
//     ChildObject MordorSchwarzeReiterHordeFoot (here Port340HordeFoot): CrusherLevel 0, CrushableLevel 2, the contain with
//     AttributeModifiers = NazgulMountedModifier, AlternateFormation = the mounted form, ThisFormationIsTheMainFormation = No, a slower
//     locomotor (110 against 100 in the mod's units — kept as a value, the port's locomotor store has no such names);
//   * data/ini/includes/specialpower.inc — SpecialAbilityBlackRiderMorgulBladeHorde (here Port340MorgulBlade): SPECIAL_AT_VISIBLE_OBJECT,
//     ReloadTime 60000, ObjectFilter NONE +HERO;
//   * data/ini/includes/weapon.inc — SchwarzeReiterMorgulBlade (here Port340MorgulWeapon): AttackRange 30, MeleeWeapon,
//     PreAttackType PER_SHOT, PreAttackDelay 500, FiringDuration 1000, DelayBetweenShots 1000, a 600 MAGIC DamageNugget;
//   * data/ini/includes/experiencelevels.inc — BlackRiderHordeLevelMod1: TargetNames = MordorSchwarzeReiterHorde
//     MordorSchwarzeReiter (here Port340Horde only, the value the mod has), RequiredExperience 1, Rank 1.

#include "CombatTestUtil.h"
#include "doctest.h"

#include "Common/SpecialPower.h"
#include "GameClient/ControlBarCommands.h"
#include "GameLogic/ExperienceLevels.h"
#include "GameLogic/Module/SpecialAbilityModules.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Object/ExperienceTracker.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/SpellCommands.h"

#include <string>

namespace
{
// the power's two modules, the mod's values, written once and used by every caster (the mod repeats them on the horde, on the on-foot
// ChildObject through the copy and on the member)
#define PORT340_POWER_MODULES                                                                                                                    \
	"  Behavior = SpecialPowerModule ModuleTag_MorgulBladeStarter\n"                                                                             \
	"    SpecialPowerTemplate = Port340MorgulBlade\n"                                                                                            \
	"    UpdateModuleStartsAttack = Yes\n"                                                                                                       \
	"  End\n"                                                                                                                                    \
	"  Behavior = WeaponFireSpecialAbilityUpdate ModuleTag_MorgulBladeUpdate\n"                                                                  \
	"    SpecialPowerTemplate = Port340MorgulBlade\n"                                                                                            \
	"    SkipContinue = Yes\n"                                                                                                                   \
	"    UnpackTime = 250\n"                                                                                                                     \
	"    PreparationTime = 1\n"                                                                                                                  \
	"    PersistentPrepTime = 500\n"                                                                                                             \
	"    PackTime = 100\n"                                                                                                                       \
	"    AwardXPForTriggering = 0\n"                                                                                                             \
	"    StartAbilityRange = 15\n"                                                                                                               \
	"    SpecialWeapon = Port340MorgulWeapon\n"                                                                                                  \
	"    WhichSpecialWeapon = 1\n"                                                                                                               \
	"  End\n"

// the horde's HorseHordeContain, the mod's values (the AlternateFormation line is filled in per object below)
#define PORT340_CONTAIN_FIELDS                                                                                                                   \
	"    FrontAngle = 270\n"                                                                                                                     \
	"    FlankedDelay = 500\n"                                                                                                                   \
	"    InitialPayload = Port340Rider 5\n"                                                                                                      \
	"    Slots = 5\n"                                                                                                                            \
	"    PassengerFilter = NONE +CAVALRY +Port340Rider\n"                                                                                        \
	"    ShowPips = No\n"                                                                                                                        \
	"    RandomOffset = X:1 Y:1\n"                                                                                                               \
	"    MeleeBehavior = Amoeba\n"                                                                                                               \
	"    RankInfo = RankNumber:1 UnitType:Port340Rider Position:X:25 Y:0 Position:X:5 Y:-25 Position:X:10 Y:29 Position:X:-20 Y:-18 Position:X:-24 Y:12\n"

const char kPort340Head[] = R"ini(
SpecialPower Port340MorgulBlade
 Enum = SPECIAL_AT_VISIBLE_OBJECT
 ReloadTime = 60000
 ObjectFilter = NONE +HERO
End

Weapon Port340MorgulWeapon
 AttackRange = 30
 MeleeWeapon = Yes
 PreAttackType = PER_SHOT
 PreAttackDelay = 500
 FiringDuration = 1000
 DelayBetweenShots = 1000
 DamageNugget
  Damage = 600
  Radius = 0.0
  DelayTime = 0
  DamageType = MAGIC
  DeathType = NORMAL
 End
End

ExperienceLevel Port340RiderLevel1
 TargetNames = Port340Rider
 RequiredExperience = 1
 Rank = 1
End

ExperienceLevel Port340HordeLevel1
 TargetNames = Port340Horde)ini";

const char kPort340Middle[] = R"ini(
 RequiredExperience = 1
 Rank = 1
End

Object Port340Rider
 KindOf = SELECTABLE INFANTRY CAVALRY CAN_ATTACK
 Geometry = CYLINDER
 GeometryMajorRadius = 8
 GeometryHeight = 20
 ArmorSet
  Conditions = None
  Armor = PlainArmor
 End
 WeaponSet
  Conditions = None
  Weapon = PRIMARY SwordWeapon
 End
 Body = ActiveBody ModuleTag_Body
  MaxHealth = 500
 End
 Behavior = AIUpdateInterface ModuleTag_AI
 End
 Behavior = PhysicsBehavior ModuleTag_Physics
 End
 LocomotorSet
  Locomotor = WalkerLoco
  Condition = SET_NORMAL
  Speed = 55
 End
End

Object Port340Hero
 KindOf = HERO INFANTRY SELECTABLE CAN_ATTACK
 Geometry = CYLINDER
 GeometryMajorRadius = 8
 GeometryHeight = 20
 ArmorSet
  Conditions = None
  Armor = PlainArmor
 End
 Body = ActiveBody ModuleTag_Body
  MaxHealth = 20000
 End
 Behavior = AIUpdateInterface ModuleTag_AI
 End
 Behavior = PhysicsBehavior ModuleTag_Physics
 End
 LocomotorSet
  Locomotor = WalkerLoco
  Condition = SET_NORMAL
  Speed = 55
 End
End

Object Port340Horde
 KindOf = HERO SELECTABLE CAN_ATTACK ATTACK_NEEDS_LINE_OF_SIGHT INFANTRY HORDE MELEE_HORDE CAVALRY
 Geometry = BOX
 GeometryMajorRadius = 30
 GeometryMinorRadius = 40
 GeometryHeight = 20
 ArmorSet
  Conditions = None
  Armor = PlainArmor
 End
 WeaponSet
  Conditions = None
  Weapon = PRIMARY SwordWeapon
 End
 Body = ActiveBody ModuleTag_Body
  MaxHealth = 5000
 End
 Behavior = HordeAIUpdate ModuleTag_HordeAIUpdate
  AutoAcquireEnemiesWhenIdle = No
  MoodAttackCheckRate = 500
  MaxCowerTime = 5000
  MinCowerTime = 3000
 End
 Behavior = HordeContain ModuleTag_HordeContain
)ini" PORT340_CONTAIN_FIELDS R"ini(
  AlternateFormation = Port340HordeFoot
  ThisFormationIsTheMainFormation = Yes
  RanksToReleaseWhenAttacking = 50
  MeleeAttackLeashDistance = 1
 End
 Behavior = PhysicsBehavior ModuleTag_PhysicsBehavior
  GravityMult = 1.0
 End
)ini" PORT340_POWER_MODULES R"ini(
 LocomotorSet
  Locomotor = HordeLoco
  Condition = SET_NORMAL
  Speed = 100
 End
End

ChildObject Port340HordeFoot Port340Horde
 CrusherLevel = 0
 CrushableLevel = 2
 Behavior = HordeContain ModuleTag_HordeContain
)ini" PORT340_CONTAIN_FIELDS R"ini(
  AlternateFormation = Port340Horde
  ThisFormationIsTheMainFormation = No
  RanksToReleaseWhenAttacking = 50
  MeleeAttackLeashDistance = 1
 End
 LocomotorSet
  Locomotor = HordeLoco
  Condition = SET_NORMAL
  Speed = 110
 End
End

Object Port340PlainHorde
 KindOf = SELECTABLE CAN_ATTACK INFANTRY CAVALRY
 Geometry = CYLINDER
 GeometryMajorRadius = 8
 GeometryHeight = 20
 ArmorSet
  Conditions = None
  Armor = PlainArmor
 End
 WeaponSet
  Conditions = None
  Weapon = PRIMARY SwordWeapon
 End
 Body = ActiveBody ModuleTag_Body
  MaxHealth = 5000
 End
 Behavior = AIUpdateInterface ModuleTag_AI
 End
 Behavior = PhysicsBehavior ModuleTag_Physics
 End
)ini" PORT340_POWER_MODULES R"ini(
 LocomotorSet
  Locomotor = WalkerLoco
  Condition = SET_NORMAL
  Speed = 100
 End
End
)ini";

// nameTheFoot: the on-foot form's name in the level's TargetNames — the mod's data does not have it, its one-line fix does
std::string port340Data(bool nameTheFoot)
{
	return std::string(kPort340Head) + (nameTheFoot ? " Port340HordeFoot" : "") + kPort340Middle;
}

struct Port340World : combattest::CombatWorld
{
	SpecialPowerStore powers;
	SpecialPowerStore *savedPowers = TheSpecialPowerStore;
	ExperienceLevelSystem levels;
	ExperienceLevelSystem *savedLevels = TheExperienceLevelSystem;
	SpellCommands spells;

	explicit Port340World(bool nameTheFoot = false)
	{
		TheSpecialPowerStore = &powers;
		TheExperienceLevelSystem = &levels;
		levels.registerBlocks(w.fx.env.blocks);
		w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
		SpecialAbilityModules::registerAll(w.modules);
		SpecialPowerModules::registerAll(w.modules);
		spells.registerHandlers(dispatcher);
		combat().setAutoAcquireEnabled(false);
		const std::string error = w.load(port340Data(nameTheFoot), INI_LOAD_OVERWRITE, "port340.ini");
		REQUIRE_MESSAGE(error.empty(), error);
	}
	~Port340World()
	{
		TheSpecialPowerStore = savedPowers;
		TheExperienceLevelSystem = savedLevels;
	}

	// the press: MSG_DO_SPECIAL_POWER_AT_OBJECT with the button's options (NEED_TARGET_ENEMY_OBJECT, no OK_FOR_MULTI_EXECUTE), the power,
	// the target object, the button's options and the casting object as the source (RW 0x77A6xx's dispatcher shape)
	void cast(Object &caster, Object &victim)
	{
		GameMessage message(MSG_DO_SPECIAL_POWER_AT_OBJECT, caster.getControllingPlayer()->getPlayerIndex());
		message.appendIntegerArgument((int)powers.findSpecialPowerTemplate("Port340MorgulBlade")->getID());
		message.appendObjectIDArgument(victim.getID());
		message.appendIntegerArgument(COMMAND_OPTION_NEED_TARGET_ENEMY_OBJECT);
		message.appendObjectIDArgument(caster.getID());
		send(message);
	}
	// the ability's shots, -1 when the caster has no such module
	int shots(Object &caster)
	{
		auto *ability = dynamic_cast<const WeaponFireSpecialAbilityUpdate *>(caster.findModule("WeaponFireSpecialAbilityUpdate"));
		return ability ? (int)ability->shotsFired() : -1;
	}
	// runs the world until the ability has fired `want` shots; true when it did within `limit` frames
	bool waitForShots(Object &caster, int want, int limit)
	{
		for (int f = 0; f < limit && shots(caster) < want; ++f)
		{
			frames(1);
		}
		return shots(caster) == want;
	}
	// runs the world until the victim's health differs from `from` (the weapon nugget landed); true when it did within `limit` frames
	bool waitForHit(Object &victim, float from, int limit)
	{
		for (int f = 0; f < limit && health(victim) == from; ++f)
		{
			frames(1);
		}
		return health(victim) != from;
	}
	// the mod's weapon: a 600 MAGIC DamageNugget on the hero's PlainArmor DEFAULT 100%, so a landed hit is exactly this
	static constexpr float kHeroHealth = 20000.0f;
	static constexpr float kAfterOneHit = kHeroHealth - 600.0f;
};
} // namespace

// the mounted form: its template is named by an ExperienceLevel, so the level handle is valid, it is the (only) candidate of the rank
// contest and it casts — the mod's matrix row for MordorSchwarzeReiterHorde
TEST_CASE("PORT-340: the mounted hero horde (its template named by an ExperienceLevel) fires the Morgul blade")
{
	Port340World world;
	Object *horde = world.unit("Port340Horde", 'A', 300, 300);
	Object *victim = world.unit("Port340Hero", 'B', 340, 300);
	REQUIRE(horde);
	REQUIRE(victim);
	world.frames(2);
	// the object's first level (RW 0x693F1B: gainLevel in initObject) is the level's rank 1
	int rank = 0;
	REQUIRE(horde->getExperienceTracker());
	REQUIRE(horde->getExperienceTracker()->currentLevelRank(rank));
	CHECK(rank == 1);                        // BlackRiderHordeLevelMod1's Rank
	CHECK(world.health(victim) == Port340World::kHeroHealth);
	CHECK(world.shots(*horde) == 0);         // the module is there and has not fired yet
	world.cast(*horde, *victim);
	CHECK(world.waitForShots(*horde, 1, 300));
	CHECK(world.waitForHit(*victim, Port340World::kHeroHealth, 30));
	CHECK(world.health(victim) == Port340World::kAfterOneHit); // the 600 MAGIC nugget landed once
	CHECK(world.spells.casts() == 1);
	CHECK(world.spells.refused() == 0);
}

// the on-foot form as the mod has it: no ExperienceLevel names MordorSchwarzeReiterHordeFoot, so the tracker has no level list, the handle
// is invalid and RW 0x770A0D drops the member: nothing casts (and nothing is refused either — the cast never reaches a module)
TEST_CASE("PORT-340: the on-foot hero horde that no ExperienceLevel names is dropped by the rank contest (RW 0x770A0D)")
{
	Port340World world;
	Object *horde = world.unit("Port340HordeFoot", 'A', 300, 300);
	Object *victim = world.unit("Port340Hero", 'B', 340, 300);
	REQUIRE(horde);
	REQUIRE(victim);
	world.frames(2);
	int rank = 0;
	REQUIRE(horde->getExperienceTracker());
	CHECK_FALSE(horde->getExperienceTracker()->currentLevelRank(rank)); // no level list for this template's name
	CHECK(TheExperienceLevelSystem->levelsFor("Port340HordeFoot") == nullptr);
	REQUIRE(TheExperienceLevelSystem->levelsFor("Port340Horde") != nullptr); // the mounted form's own list
	CHECK(world.shots(*horde) == 0);
	world.cast(*horde, *victim);
	CHECK_FALSE(world.waitForShots(*horde, 1, 300));
	CHECK(world.shots(*horde) == 0);
	CHECK(world.spells.casts() == 0);
	CHECK(world.spells.refused() == 0);      // the member never reached a module test
	CHECK(world.health(victim) == Port340World::kHeroHealth);
}

// the same on-foot form with its name added to the level's TargetNames (the mod's data fix): it carries the level, wins the contest and
// casts exactly as the mounted form — the only difference between the two rows of the matrix is that name
TEST_CASE("PORT-340: the on-foot hero horde fires once an ExperienceLevel names it (the mod's data fix)")
{
	Port340World world(/*nameTheFoot=*/true);
	Object *horde = world.unit("Port340HordeFoot", 'A', 300, 300);
	Object *victim = world.unit("Port340Hero", 'B', 340, 300);
	REQUIRE(horde);
	REQUIRE(victim);
	world.frames(2);
	int rank = 0;
	REQUIRE(horde->getExperienceTracker());
	CHECK(horde->getExperienceTracker()->currentLevelRank(rank));
	CHECK(rank == 1);
	CHECK(TheExperienceLevelSystem->levelsFor("Port340HordeFoot") != nullptr);
	world.cast(*horde, *victim);
	CHECK(world.waitForShots(*horde, 1, 300));
	CHECK(world.waitForHit(*victim, Port340World::kHeroHealth, 30));
	CHECK(world.health(victim) == Port340World::kAfterOneHit);
	CHECK(world.spells.casts() == 1);
}

// the neighbouring behaviour: the contest is HERO-only (RW 0x7709D3 tests KindOf bit 90), so a caster without that bit needs no
// ExperienceLevel and casts — the same power on a template no level names
TEST_CASE("PORT-340: a caster that is not HERO casts the same power without any ExperienceLevel")
{
	Port340World world;
	Object *caster = world.unit("Port340PlainHorde", 'A', 300, 300);
	Object *victim = world.unit("Port340Hero", 'B', 340, 300);
	REQUIRE(caster);
	REQUIRE(victim);
	world.frames(2);
	CHECK_FALSE(caster->isKindOfName("HERO"));
	int rank = 0;
	CHECK_FALSE(caster->getExperienceTracker()->currentLevelRank(rank));
	world.cast(*caster, *victim);
	CHECK(world.waitForShots(*caster, 1, 300));
	CHECK(world.waitForHit(*victim, Port340World::kHeroHealth, 30));
	CHECK(world.health(victim) == Port340World::kAfterOneHit);
	CHECK(world.spells.casts() == 1);
}
