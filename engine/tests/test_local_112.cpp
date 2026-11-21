// OpenBFME. GPL-3.0.
// LOCAL-112 (fork issue 796): WeaponFireSpecialAbilityUpdate boilerplate citations (RW 0x00895C65, RW 0x00895D15, RW 0x00895E44),
// FireLogicNugget::doEffectPosition (RW 0x009120E6 -> RW 0x6878D1), and SweepEffectProbe / matrix scene verification.
// Editions: both RotWK 2.01 and BFME2 1.06 (same WeaponFireSpecialAbilityUpdate / FireLogicNugget / OCL upgrade rules).

#include "doctest.h"
#include "CombatTestUtil.h"
#include "SweepEffectProbe.h"

#include "Common/SpecialPower.h"
#include "GameLogic/Combat/ObjectWeapons.h"
#include "GameLogic/Map/FireLogicSystem.h"
#include "GameLogic/Module/SpecialAbilityModules.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/ObjectCreationList.h"

#include <string>

namespace
{

struct Local112World : combattest::CombatWorld
{
	SpecialPowerStore powers;
	SpecialPowerStore *savedPowers = TheSpecialPowerStore;
	ObjectCreationListStore lists{ w.keys };
	ObjectCreationListStore *savedLists = TheObjectCreationListStore;

	Local112World()
	{
		TheSpecialPowerStore = &powers;
		TheObjectCreationListStore = &lists;
		w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
		w.fx.env.blocks.registerBlock("ObjectCreationList", [](INI *ini) { ObjectCreationListStore::parseObjectCreationListDefinitionGlobal(ini); });
		SpecialAbilityModules::registerAll(w.modules);
		SpecialPowerModules::registerAll(w.modules);
		combat().setAutoAcquireEnabled(false);
		logic->fireLogic().newMap(0.0f, 0.0f, 400.0f, 400.0f);
	}
	~Local112World()
	{
		TheSpecialPowerStore = savedPowers;
		TheObjectCreationListStore = savedLists;
	}

	void loadIni(const std::string &ini)
	{
		const std::string err = w.load(ini);
		REQUIRE_MESSAGE(err.empty(), err);
	}
};

} // namespace

// Expectations from RW 0x8961F7 (WeaponFireSpecialAbilityUpdate::triggerAbilityEffect) -> RW 0x6CF3D2 -> RW 0x009120E6 (FireLogicNugget::doEffectPosition, case 1 DECREASE_BURN_RATE) -> RW 0x6878D1 (FireLogicSystem::changeBurnRateInArea).
TEST_CASE("LOCAL-112: WeaponFireSpecialAbilityUpdate FireLogicNugget DECREASE_BURN_RATE extinguishes burning cells and SweepEffectProbe measures the burn-rate drop (RW 0x8961F7, RW 0x009120E6)")
{
	Local112World env;
	env.loadIni(R"(
SpecialPower SP_Local112Extinguish
	Enum = SPECIAL_EXTINGUISH_FIRE
	ReloadTime = 5000
End

Weapon W_Local112Extinguish
	AttackRange = 150.0
	FireLogicNugget
		LogicType = DECREASE_BURN_RATE
		Radius = 100.0
		Damage = 100.0
	End
End

Object O_Local112Porter
	KindOf = SELECTABLE INFANTRY
	Geometry = CYLINDER
	GeometryMajorRadius = 8
	GeometryHeight = 16
	Body = ActiveBody ModuleTag_Body
		MaxHealth = 500.0
	End
	Behavior = AIUpdateInterface ModuleTag_AI
	End
	Behavior = SpecialPowerModule ModuleTag_SP
		SpecialPowerTemplate = SP_Local112Extinguish
		UpdateModuleStartsAttack = Yes
	End
	Behavior = WeaponFireSpecialAbilityUpdate ModuleTag_WF
		SpecialPowerTemplate = SP_Local112Extinguish
		UnpackTime = 200
		StartAbilityRange = 150.0
		IgnoreFacingCheck = Yes
		SpecialWeapon = W_Local112Extinguish
	End
End
)");

	Object *porter = env.unit("O_Local112Porter", 'A', 100.0f, 100.0f);
	REQUIRE(porter != nullptr);
	const SpecialPowerTemplate *sp = TheSpecialPowerStore->findSpecialPowerTemplate("SP_Local112Extinguish");
	REQUIRE(sp != nullptr);

	// Ignite terrain cells at the target point (fuel + burn rate)
	const Coord3D firePos{ 150.0f, 100.0f, 0.0f };
	env.logic->fireLogic().changeFuelInArea(firePos, 40.0f, 1000, 100, 1, 0, false);
	env.logic->fireLogic().changeBurnRateInArea(firePos, 40.0f, 100, false);

	sweep_probe::EffectProbe before = sweep_probe::EffectProbe::take(*env.logic, porter, sp);
	REQUIRE(before.fireBurnRateSum > 0); // tolerance: midpoint disc cell sum depends on grid alignment; exact 0 asserted after extinguish
	REQUIRE(before.expects.size() == 1);
	CHECK(before.expects[0].kind == sweep_probe::EffectProbe::FIRE_LOGIC);
	CHECK(before.expects[0].fireLogicType == 1);

	auto *spm = dynamic_cast<SpecialPowerModule *>(porter->findModule("SpecialPowerModule"));
	REQUIRE(spm != nullptr);
	spm->doSpecialPowerAtLocation(firePos, 0);
	for (int i = 0; i < 20; ++i) { for (int ph = 1; ph <= 6; ++ph) { env.logic->update(ph); } }

	sweep_probe::EffectProbe after = sweep_probe::EffectProbe::take(*env.logic, porter, sp);
	CHECK(after.fireBurnRateSum == 0);
	std::string missing;
	const std::string got = before.compare(after, missing);
	CHECK(missing.empty());
	CHECK(got.find("fire burn rate") != std::string::npos);
}

// Expectations from RW 0x5F0DC5 (OCLNugget RequiredUpgrades / ForbiddenUpgrades gate) and RW 0x5D8950 (DAMAGE_HEALING).
TEST_CASE("LOCAL-112: SweepEffectProbe respects OCL CreateObject RequiredUpgrades (RW 0x5F0DC5) and classifies DAMAGE_HEALING as HEAL (RW 0x5D8950)")
{
	Local112World env;
	env.loadIni(R"(
Upgrade Upgrade_Local112Gate
	Type = PLAYER
End

Object O_Local112Egg
	KindOf = INERT IMMOBILE UNATTACKABLE
	Body = ActiveBody ModuleTag_Body
		MaxHealth = 1.0
	End
End

ObjectCreationList OCL_Local112Gated
	CreateObject
		ObjectNames = O_Local112Egg
		Count = 1
		RequiredUpgrades = Upgrade_Local112Gate
	End
End

SpecialPower SP_Local112Roar
	Enum = SPECIAL_SCREECH
	ReloadTime = 5000
End

SpecialPower SP_Local112Heal
	Enum = SPECIAL_AT_VISIBLE_OBJECT
	ReloadTime = 5000
End

Weapon W_Local112Roar
	AttackRange = 30.0
	WeaponOCLNugget
		WeaponOCLName = OCL_Local112Gated
	End
	EmotionWeaponNugget
		EmotionType = TERROR
		Radius = 100.0
		Duration = 10
	End
End

Weapon W_Local112Heal
	AttackRange = 100.0
	DamageNugget
		Damage = 500.0
		DamageType = HEALING
		DeathType = NORMAL
	End
End

Object O_Local112Caster
	KindOf = SELECTABLE INFANTRY
	Body = ActiveBody ModuleTag_Body
		MaxHealth = 500.0
	End
	Behavior = SpecialPowerModule ModuleTag_SPRoar
		SpecialPowerTemplate = SP_Local112Roar
	End
	Behavior = WeaponFireSpecialAbilityUpdate ModuleTag_WFRoar
		SpecialPowerTemplate = SP_Local112Roar
		SpecialWeapon = W_Local112Roar
	End
	Behavior = SpecialPowerModule ModuleTag_SPHeal
		SpecialPowerTemplate = SP_Local112Heal
	End
	Behavior = WeaponFireSpecialAbilityUpdate ModuleTag_WFHeal
		SpecialPowerTemplate = SP_Local112Heal
		SpecialWeapon = W_Local112Heal
	End
End
)");

	Object *caster = env.unit("O_Local112Caster", 'A', 100.0f, 100.0f);
	REQUIRE(caster != nullptr);
	const SpecialPowerTemplate *spRoar = TheSpecialPowerStore->findSpecialPowerTemplate("SP_Local112Roar");
	const SpecialPowerTemplate *spHeal = TheSpecialPowerStore->findSpecialPowerTemplate("SP_Local112Heal");
	REQUIRE(spRoar != nullptr);
	REQUIRE(spHeal != nullptr);

	// Without Upgrade_Local112Gate, OCL CreateObject is skipped (RW 0x5F0DC5): only EMOTION is expected, no SPAWN
	sweep_probe::EffectProbe pNoUpg = sweep_probe::EffectProbe::take(*env.logic, caster, spRoar);
	bool hasSpawn = false, hasEmotion = false;
	for (const auto &e : pNoUpg.expects)
	{
		hasSpawn = hasSpawn || e.kind == sweep_probe::EffectProbe::SPAWN;
		hasEmotion = hasEmotion || e.kind == sweep_probe::EffectProbe::EMOTION;
	}
	CHECK_FALSE(hasSpawn);
	CHECK(hasEmotion);

	// With Upgrade_Local112Gate on player, OCL CreateObject IS expected
	caster->giveUpgrade("Upgrade_Local112Gate");
	sweep_probe::EffectProbe pWithUpg = sweep_probe::EffectProbe::take(*env.logic, caster, spRoar);
	bool hasSpawnAfter = false;
	for (const auto &e : pWithUpg.expects)
	{
		hasSpawnAfter = hasSpawnAfter || e.kind == sweep_probe::EffectProbe::SPAWN;
	}
	CHECK(hasSpawnAfter);

	// DAMAGE_HEALING nugget maps to HEAL, not DAMAGE
	sweep_probe::EffectProbe pHeal = sweep_probe::EffectProbe::take(*env.logic, caster, spHeal);
	bool hasHeal = false, hasDamage = false;
	for (const auto &e : pHeal.expects)
	{
		hasHeal = hasHeal || e.kind == sweep_probe::EffectProbe::HEAL;
		hasDamage = hasDamage || e.kind == sweep_probe::EffectProbe::DAMAGE;
	}
	CHECK(hasHeal);
	CHECK_FALSE(hasDamage);
}

// Expectations from Ennorath 1.9.8.3 (HwaldarVerbrannteErdeWeapon 1,000,000 damage vs 40,000 HP building; GrabNugget RW 0x910C8A transient containment).
TEST_CASE("LOCAL-112: SweepEffectProbe accepts one-shot kills clamped to remaining HP and transient GrabNugget containment")
{
	Local112World env;
	env.loadIni(R"(
SpecialPower SP_Local112Nuke
	Enum = SPECIAL_AT_VISIBLE_OBJECT
	ReloadTime = 5000
End

Weapon W_Local112Nuke
	AttackRange = 50.0
	DamageNugget
		Damage = 1000000.0
		DamageType = SIEGE
		DeathType = NORMAL
	End
End

Object O_Local112Hero
	KindOf = SELECTABLE HERO
	Body = ActiveBody ModuleTag_Body
		MaxHealth = 1000.0
	End
	Behavior = SpecialPowerModule ModuleTag_SP
		SpecialPowerTemplate = SP_Local112Nuke
	End
	Behavior = WeaponFireSpecialAbilityUpdate ModuleTag_WF
		SpecialPowerTemplate = SP_Local112Nuke
		SpecialWeapon = W_Local112Nuke
	End
End

Object O_Local112Target
	KindOf = SELECTABLE STRUCTURE
	Body = ActiveBody ModuleTag_Body
		MaxHealth = 40000.0
	End
End
)");

	Object *hero = env.unit("O_Local112Hero", 'A', 100.0f, 100.0f);
	Object *target = env.unit("O_Local112Target", 'B', 120.0f, 100.0f);
	REQUIRE(hero != nullptr);
	REQUIRE(target != nullptr);
	const SpecialPowerTemplate *sp = TheSpecialPowerStore->findSpecialPowerTemplate("SP_Local112Nuke");
	REQUIRE(sp != nullptr);

	sweep_probe::EffectProbe before = sweep_probe::EffectProbe::take(*env.logic, hero, sp, {}, target);
	before.nominals(*env.logic);
	sweep_probe::HitTracker hits;
	hits.casters.insert(hero->getID());
	hits.hits.push_back({ before.frame + 1, target->getID(), 40000.0f, 0.0f });

	sweep_probe::EffectProbe after = sweep_probe::EffectProbe::take(*env.logic, hero, sp, {}, target);
	std::string missing;
	const std::string got = before.compare(after, missing, &hits);
	CHECK(missing.empty());
	CHECK(got.find("40000") != std::string::npos);

	// Transient GrabNugget check via grabbedFirstSeen
	sweep_probe::EffectProbe grabBefore;
	grabBefore.frame = 10;
	grabBefore.holders.insert(hero->getID());
	grabBefore.expects.push_back({ sweep_probe::EffectProbe::GRAB, "GrabNugget" });
	hits.grabbedFirstSeen[target->getID()] = 12;
	sweep_probe::EffectProbe grabAfter;
	std::string grabMissing;
	const std::string grabGot = grabBefore.compare(grabAfter, grabMissing, &hits);
	CHECK(grabMissing.empty());
	CHECK(grabGot.find("grabbed by the caster") != std::string::npos);
}
