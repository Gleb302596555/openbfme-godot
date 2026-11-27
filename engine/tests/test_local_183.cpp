// OpenBFME. GPL-3.0.
// LOCAL-183 (fork issue 976): HordeAttackNugget range-finder abilities and SweepEffectProbe rules
// (RW 0x911A58, RW 0x911AC9, RW 0x69121A, RW 0x875221, RW 0x875550, RW 0x8C75D8, RW 0x8D0AA0, RW 0x68C26D).
// Editions: both RotWK 2.01 and BFME2 1.06 (shared HordeAttackNugget / SpecialPower dispatch / SweepEffectProbe rules).

#include "doctest.h"
#include "CombatTestUtil.h"
#include "SweepEffectProbe.h"

#include "Common/SpecialPower.h"
#include "GameLogic/Combat/ObjectWeapons.h"
#include "GameLogic/Combat/WeaponDelivery.h"
#include "GameLogic/Module/HordeAIUpdate.h"
#include "GameLogic/Module/LogicModules.h"
#include "GameLogic/Module/SpecialAbilityModules.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Object/Contain/HordeContainRuntime.h"
#include "GameLogic/ObjectCreationList.h"

#include <string>

namespace
{
using sweep_probe::EffectProbe;
using sweep_probe::HitTracker;

struct Local183World : combattest::CombatWorld
{
	SpecialPowerStore powers;
	SpecialPowerStore *savedPowers = TheSpecialPowerStore;
	ObjectCreationListStore lists{ w.keys };
	ObjectCreationListStore *savedLists = TheObjectCreationListStore;
	AttributeModifierStore attrMods;
	AttributeModifierStore *savedAttrMods = TheAttributeModifierStore;

	Local183World()
	{
		TheSpecialPowerStore = &powers;
		TheObjectCreationListStore = &lists;
		TheAttributeModifierStore = &attrMods;
		w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
		w.fx.env.blocks.registerBlock("ObjectCreationList", [](INI *ini) { ObjectCreationListStore::parseObjectCreationListDefinitionGlobal(ini); });
		SpecialAbilityModules::registerAll(w.modules);
		SpecialPowerModules::registerAll(w.modules);
		LogicModules::registerAll(w.modules);
		combat().setAutoAcquireEnabled(false);
	}

	~Local183World()
	{
		TheSpecialPowerStore = savedPowers;
		TheObjectCreationListStore = savedLists;
		TheAttributeModifierStore = savedAttrMods;
	}

	void loadIni(const std::string &ini)
	{
		const std::string err = w.load(ini);
		REQUIRE_MESSAGE(err.empty(), err);
	}
};

const char kLocal183Ini[] = R"ini(
SpecialPower L183SmitePower
	Enum = SPECIAL_AT_VISIBLE_OBJECT
	ReloadTime = 30000
End

SpecialPower L183LongShotPower
	Enum = SPECIAL_SPELL_BOOK_ARROW_VOLLEY_GOOD
	ReloadTime = 30000
	Flags = LIMIT_DISTANCE
	MaxCastRange = 500
End

SpecialPower L183DispatchPower
	Enum = SPECIAL_AT_VISIBLE_OBJECT
	ReloadTime = 30000
End

SpecialPower L183OtherPower
	Enum = SPECIAL_AT_VISIBLE_OBJECT
	ReloadTime = 30000
End

Weapon L183MemberSword
	AttackRange = 15.0
	MeleeWeapon = Yes
	DelayBetweenShots = 1000
	PreAttackDelay = 100
	FiringDuration = 200
	DamageNugget
		Damage = 5
		DamageType = SLASH
		DeathType = NORMAL
	End
End

Weapon L183MemberThrow
	AttackRange = 250.0
	AcceptableAimDelta = 180
	DelayBetweenShots = 1000
	PreAttackDelay = 100
	FiringDuration = 200
	RadiusDamageAffects = ENEMIES
	DamageNugget
		Damage = 35
		DamageType = HERO
		DeathType = NORMAL
	End
End

Weapon L183MemberDummy
	AttackRange = 99999.0
	PreAttackDelay = 100
	FiringDuration = 200
	RadiusDamageAffects = ENEMIES
End

Weapon L183MemberBlast
	AttackRange = 250.0
	DelayBetweenShots = 1000
	PreAttackDelay = 100
	FiringDuration = 200
	RadiusDamageAffects = ENEMIES
	DamageNugget
		Damage = 40
		DamageType = MAGIC
		DeathType = NORMAL
	End
End

Weapon L183RangeFinder
	AttackRange = 250.0
	AcceptableAimDelta = 180
	DelayBetweenShots = 1000
	HordeAttackNugget
		LockWeaponSlot = TERTIARY
	End
End

Weapon L183LongShotWeapon
	AttackRange = 500.0
	RadiusDamageAffects = ENEMIES
	HordeAttackNugget
		LockWeaponSlot = QUINARY
	End
End

Object L183VolleyObj
	KindOf = IMMOBILE NO_COLLIDE
	Body = ActiveBody ModuleTag_Body
		MaxHealth = 100
	End
End

ObjectCreationList OCL_L183Volley
	CreateObject
		ObjectNames = L183VolleyObj
	End
End

Object L183Target
	KindOf = SELECTABLE CAN_ATTACK INFANTRY
	VisionRange = 200.0
	Geometry = CYLINDER
	GeometryMajorRadius = 8
	GeometryMinorRadius = 8
	GeometryHeight = 20
	ArmorSet
		Conditions = None
		Armor = PlainArmor
	End
	Body = ActiveBody ModuleTag_Body
		MaxHealth = 500
	End
	Behavior = AIUpdateInterface ModuleTag_AI
	End
End

Object L183Member
	KindOf = SELECTABLE CAN_ATTACK INFANTRY
	VisionRange = 250.0
	Geometry = CYLINDER
	GeometryMajorRadius = 8
	GeometryMinorRadius = 8
	GeometryHeight = 20
	ArmorSet
		Conditions = None
		Armor = PlainArmor
	End
	WeaponSet
		Conditions = None
		Weapon = PRIMARY L183MemberSword
		Weapon = TERTIARY L183MemberThrow
		Weapon = QUINARY L183MemberDummy
		AutoChooseSources = TERTIARY FROM_PLAYER FROM_SCRIPT
		AutoChooseSources = QUINARY FROM_PLAYER FROM_SCRIPT
	End
	Body = ActiveBody ModuleTag_Body
		MaxHealth = 200
	End
	Behavior = AIUpdateInterface ModuleTag_AI
	End
	LocomotorSet
		Locomotor = WalkerLoco
		Condition = SET_NORMAL
		Speed = 55
	End
	; Copied horde ability module on member unit (as in Ennorath's IsenfurtWchter / DwarvenAxtschwinger / AngmarDarkRanger)
	Behavior = SpecialPowerModule ModuleTag_CopiedSmite
		SpecialPowerTemplate = L183SmitePower
		UpdateModuleStartsAttack = Yes
	End
	Behavior = WeaponFireSpecialAbilityUpdate ModuleTag_CopiedSmiteUpdate
		SpecialPowerTemplate = L183SmitePower
		SpecialWeapon = L183RangeFinder
		StartAbilityRange = 240.0
		UnpackTime = 0
		PreparationTime = 1
		PackTime = 100
	End
End

Object L183SmiteHorde
	KindOf = SELECTABLE CAN_ATTACK INFANTRY HORDE LARGE_RECTANGLE_PATHFIND
	VisionRange = 250.0
	Geometry = BOX
	GeometryMajorRadius = 30
	GeometryMinorRadius = 45
	GeometryHeight = 20
	WeaponSet
		Conditions = None
		Weapon = PRIMARY L183RangeFinder
	End
	Body = ActiveBody ModuleTag_Body
		MaxHealth = 500
	End
	Behavior = HordeAIUpdate ModuleTag_AI
	End
	Behavior = HordeContain ModuleTag_HordeContain
		InitialPayload = L183Member 4
		RankInfo = RankNumber:1 UnitType:L183Member Position:X:0 Y:-15 Position:X:0 Y:-5 Position:X:0 Y:5 Position:X:0 Y:15
		RanksToReleaseWhenAttacking = 1
	End
	LocomotorSet
		Locomotor = HordeLoco
		Condition = SET_NORMAL
		Speed = 50
	End
	Behavior = SpecialPowerModule ModuleTag_SmiteStarter
		SpecialPowerTemplate = L183SmitePower
		UpdateModuleStartsAttack = Yes
	End
	Behavior = WeaponFireSpecialAbilityUpdate ModuleTag_SmiteUpdate
		SpecialPowerTemplate = L183SmitePower
		SpecialWeapon = L183RangeFinder
		StartAbilityRange = 240.0
		UnpackTime = 0
		PreparationTime = 1
		PackTime = 200
	End
End

Object L183LongShotHorde
	KindOf = SELECTABLE CAN_ATTACK INFANTRY HORDE ARCHER
	VisionRange = 250.0
	Body = ActiveBody ModuleTag_Body
		MaxHealth = 500
	End
	Behavior = HordeAIUpdate ModuleTag_AI
	End
	Behavior = HordeContain ModuleTag_HordeContain
		InitialPayload = L183Member 4
		RankInfo = RankNumber:1 UnitType:L183Member Position:X:0 Y:-15 Position:X:0 Y:-5 Position:X:0 Y:5 Position:X:0 Y:15
		RanksToReleaseWhenAttacking = 1
	End
	Behavior = OCLSpecialPower ModuleTag_VolleyOCL
		SpecialPowerTemplate = L183LongShotPower
		OCL = OCL_L183Volley
		CreateLocation = CREATE_AT_LOCATION
		UpdateModuleStartsAttack = No
	End
	Behavior = WeaponFireSpecialAbilityUpdate ModuleTag_VolleyDummy
		SpecialPowerTemplate = L183LongShotPower
		SpecialWeapon = L183LongShotWeapon
		StartAbilityRange = 500.0
		UnpackTime = 0
		PreparationTime = 1
		PackTime = 100
	End
End

Object L183SingleLongShotHero
	KindOf = SELECTABLE CAN_ATTACK HERO INFANTRY
	VisionRange = 250.0
	Body = ActiveBody ModuleTag_Body
		MaxHealth = 500
	End
	Behavior = AIUpdateInterface ModuleTag_AI
	End
	Behavior = OCLSpecialPower ModuleTag_VolleyOCL
		SpecialPowerTemplate = L183LongShotPower
		OCL = OCL_L183Volley
		CreateLocation = CREATE_AT_LOCATION
		UpdateModuleStartsAttack = No
	End
	Behavior = WeaponFireSpecialAbilityUpdate ModuleTag_VolleyDummy
		SpecialPowerTemplate = L183LongShotPower
		SpecialWeapon = L183LongShotWeapon
		StartAbilityRange = 500.0
		UnpackTime = 0
		PreparationTime = 1
		PackTime = 100
	End
End

Object L183DispatchMember
	KindOf = SELECTABLE CAN_ATTACK INFANTRY
	VisionRange = 250.0
	Body = ActiveBody ModuleTag_Body
		MaxHealth = 200
	End
	Behavior = AIUpdateInterface ModuleTag_AI
	End
	Behavior = SpecialPowerModule ModuleTag_MemberDispatch
		SpecialPowerTemplate = L183DispatchPower
		UpdateModuleStartsAttack = Yes
	End
	Behavior = WeaponFireSpecialAbilityUpdate ModuleTag_MemberDispatchUpdate
		SpecialPowerTemplate = L183DispatchPower
		SpecialWeapon = L183MemberBlast
		StartAbilityRange = 240.0
		UnpackTime = 0
		PreparationTime = 1
		PackTime = 100
	End
End

Object L183DispatchHorde
	KindOf = SELECTABLE CAN_ATTACK INFANTRY HORDE
	VisionRange = 250.0
	Body = ActiveBody ModuleTag_Body
		MaxHealth = 500
	End
	Behavior = HordeAIUpdate ModuleTag_AI
	End
	Behavior = HordeContain ModuleTag_HordeContain
		InitialPayload = L183DispatchMember 2
		RankInfo = RankNumber:1 UnitType:L183DispatchMember Position:X:0 Y:-10 Position:X:0 Y:10
		RanksToReleaseWhenAttacking = 1
	End
	Behavior = HordeDispatchSpecialPower ModuleTag_Dispatch
		SpecialPowerTemplate = L183DispatchPower
	End
End

Object L183MismatchedHorde
	KindOf = SELECTABLE CAN_ATTACK INFANTRY HORDE
	VisionRange = 250.0
	Body = ActiveBody ModuleTag_Body
		MaxHealth = 500
	End
	Behavior = HordeAIUpdate ModuleTag_AI
	End
	Behavior = HordeContain ModuleTag_HordeContain
		InitialPayload = L183Member 2
		RankInfo = RankNumber:1 UnitType:L183Member Position:X:0 Y:-10 Position:X:0 Y:10
		RanksToReleaseWhenAttacking = 1
	End
	Behavior = SpecialPowerModule ModuleTag_OtherStarter
		SpecialPowerTemplate = L183OtherPower
		UpdateModuleStartsAttack = Yes
	End
End
)ini";

} // namespace

// RW 0x911A58 (HordeAttackNugget::doEffectObject), RW 0x69121A (setWeaponLock), RW 0x875221 (HordeContain::attackTargetNow),
// RW 0x68E67A / RW 0x8D0AA0 (horde only dispatches special powers to members via HordeDispatchSpecialPower).
TEST_CASE("LOCAL-183: HordeAttackNugget fires members TERTIARY and SweepEffectProbe skips copied member module without HordeDispatchSpecialPower (RW 0x911A58, RW 0x8D0AA0)")
{
	Local183World f;
	f.loadIni(kLocal183Ini);

	Object *horde = f.unit("L183SmiteHorde", 'A', 200.0f, 200.0f);
	Object *target = f.unit("L183Target", 'B', 320.0f, 200.0f);
	REQUIRE(horde != nullptr);
	REQUIRE(target != nullptr);
	f.frames(12);

	const SpecialPowerTemplate *sp = f.powers.findSpecialPowerTemplate("L183SmitePower");
	const WeaponTemplate *rf = TheWeaponStore ? TheWeaponStore->findWeaponTemplate("L183RangeFinder") : nullptr;
	REQUIRE(sp != nullptr);
	REQUIRE(rf != nullptr);

	EffectProbe before = EffectProbe::take(*f.logic, horde, sp, 0, target);
	before.nominals(*f.logic);
	// Without the SweepEffectProbe fix, the copied module on L183Member adds a second weaponless DAMAGE expectation ("caster has no HordeContain").
	REQUIRE(before.expects.size() == 1);
	CHECK(before.expects[0].kind == EffectProbe::DAMAGE);
	CHECK(before.expects[0].what.find("caster has no HordeContain") == std::string::npos);
	CHECK(!before.expects[0].weapons.empty());

	HitTracker hits;
	hits.casters = before.holders;
	hits.track(*f.logic);
	const float hpBefore = target->getBodyModule()->getHealth();

	ObjectWeapons::createAndFireTempWeaponAt(rf, horde, *target);
	for (int i = 0; i < 40; ++i)
	{
		f.frames(1);
		hits.track(*f.logic);
	}

	// tolerance: 4 horde members fire 35-damage TERTIARY throws across the 40-frame window
	CHECK(target->getBodyModule()->getHealth() <= hpBefore - 35.0f);
	EffectProbe after = EffectProbe::take(*f.logic, horde, sp, 0, target);
	std::string missing;
	const std::string got = before.compare(after, missing, &hits);
	CHECK(missing.empty());
	CHECK(!got.empty());
}

// RW 0x911A58 / RW 0x911AC9 (HordeAttackNugget no-op without HordeContain; dummy member weapon deals no damage),
// RW 0x8C75D8 (OCLSpecialPower creates the volley object).
TEST_CASE("LOCAL-183: SweepEffectProbe drops weaponless HordeAttackNugget alongside OCLSpecialPower but keeps NO-EFFECT for single unit alone (RW 0x911A58, RW 0x911AC9, RW 0x8C75D8)")
{
	Local183World f;
	f.loadIni(kLocal183Ini);

	Object *longShotHorde = f.unit("L183LongShotHorde", 'A', 200.0f, 200.0f);
	Object *singleHero = f.unit("L183SingleLongShotHero", 'A', 200.0f, 260.0f);
	Object *singleUnit = f.unit("L183Member", 'A', 200.0f, 320.0f);
	Object *target = f.unit("L183Target", 'B', 320.0f, 320.0f);
	REQUIRE(longShotHorde != nullptr);
	REQUIRE(singleHero != nullptr);
	REQUIRE(singleUnit != nullptr);
	REQUIRE(target != nullptr);
	f.frames(2);

	const SpecialPowerTemplate *longShot = f.powers.findSpecialPowerTemplate("L183LongShotPower");
	const SpecialPowerTemplate *smite = f.powers.findSpecialPowerTemplate("L183SmitePower");
	REQUIRE(longShot != nullptr);
	REQUIRE(smite != nullptr);

	// 1. Horde with OCLSpecialPower + dummy QUINARY HordeAttackNugget (as GondorRangerHorde): only SPAWN remains.
	EffectProbe hordeProbe = EffectProbe::take(*f.logic, longShotHorde, longShot);
	REQUIRE(hordeProbe.expects.size() == 1);
	CHECK(hordeProbe.expects[0].kind == EffectProbe::SPAWN);

	// 2. Single hero with OCLSpecialPower + no-op HordeAttackNugget (as DwarvenDain_mod): only SPAWN remains.
	EffectProbe heroProbe = EffectProbe::take(*f.logic, singleHero, longShot);
	REQUIRE(heroProbe.expects.size() == 1);
	CHECK(heroProbe.expects[0].kind == EffectProbe::SPAWN);

	// 3. Single unit with ONLY HordeAttackNugget (as IsenfurtWchter / IsenfurtWchterCreep): keeps NO-EFFECT placeholder and deals 0 damage (RW 0x911A58).
	EffectProbe singleProbe = EffectProbe::take(*f.logic, singleUnit, smite, 0, target);
	REQUIRE(singleProbe.expects.size() == 1);
	CHECK(singleProbe.expects[0].kind == EffectProbe::DAMAGE);
	CHECK(singleProbe.expects[0].what.find("caster has no HordeContain") != std::string::npos);

	const float hpBefore = target->getBodyModule()->getHealth();
	CHECK(SpecialPowerModules::doSpecialPowerAtObject(*singleUnit, smite, target, 0, false));
	f.frames(30);
	CHECK(target->getBodyModule()->getHealth() == hpBefore);
}

// RW 0x8D0AA0 (HordeDispatchSpecialPower forwards the power to contained members).
TEST_CASE("LOCAL-183: SweepEffectProbe still inspects member modules when HordeDispatchSpecialPower is present (RW 0x8D0AA0)")
{
	Local183World f;
	f.loadIni(kLocal183Ini);

	Object *horde = f.unit("L183DispatchHorde", 'A', 200.0f, 200.0f);
	Object *target = f.unit("L183Target", 'B', 320.0f, 200.0f);
	REQUIRE(horde != nullptr);
	REQUIRE(target != nullptr);
	f.frames(2);

	const SpecialPowerTemplate *sp = f.powers.findSpecialPowerTemplate("L183DispatchPower");
	REQUIRE(sp != nullptr);

	EffectProbe probe = EffectProbe::take(*f.logic, horde, sp, 0, target);
	REQUIRE(probe.expects.size() == 1);
	CHECK(probe.expects[0].kind == EffectProbe::DAMAGE);
	CHECK(probe.expects[0].what.find("L183MemberBlast") != std::string::npos);
}

// RW 0x68C26D (Object::findSpecialPowerModuleInterface returns nullptr when horde's modules name a different template, as IsenfurtWchterHordeHerold).
TEST_CASE("LOCAL-183: horde with mismatched SpecialPowerTemplate refuses dispatch (RW 0x68C26D)")
{
	Local183World f;
	f.loadIni(kLocal183Ini);

	Object *horde = f.unit("L183MismatchedHorde", 'A', 200.0f, 200.0f);
	Object *target = f.unit("L183Target", 'B', 320.0f, 200.0f);
	REQUIRE(horde != nullptr);
	REQUIRE(target != nullptr);
	const SpecialPowerTemplate *smite = f.powers.findSpecialPowerTemplate("L183SmitePower");
	const SpecialPowerTemplate *other = f.powers.findSpecialPowerTemplate("L183OtherPower");
	REQUIRE(smite != nullptr);
	REQUIRE(other != nullptr);

	CHECK_FALSE(SpecialPowerModules::doSpecialPowerAtObject(*horde, smite, target, 0, false));
	CHECK(SpecialPowerModules::doSpecialPowerAtObject(*horde, other, target, 0, false));
}

