// OpenBFME. GPL-3.0.
// LOCAL-131 (fork issue 837): matrix red buttons I (23 buttons: NO-EFFECT, NO-FIRE, NO-STANCE, NO-RULE).
// Synthetic tests for SweepEffectProbe handlers (PROD_SPEED, CloudBreakSpecialPower, ToggleMounted MountedTemplate,
// BezierProjectileBehavior DetonateCallsKill, missing TriggerAttributeModifier) and RotWK MetaImpactNugget
// ShockwaveResistance threshold (RW 0x9100AA).
// Editions: both RotWK 2.01 and BFME2 1.06 (shared SweepEffectProbe / special power / projectile / meta-impact rules).

#include "doctest.h"
#include "CombatTestUtil.h"
#include "SweepEffectProbe.h"

#include "Common/SpecialPower.h"
#include "GameLogic/Combat/ObjectWeapons.h"
#include "GameLogic/Combat/WeaponDelivery.h"
#include "GameLogic/Module/LogicModules.h"
#include "GameLogic/Module/SpecialAbilityModules.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/ObjectCreationList.h"

#include <string>

namespace
{
using sweep_probe::EffectProbe;


struct Local131World : combattest::CombatWorld
{
	SpecialPowerStore powers;
	SpecialPowerStore *savedPowers = TheSpecialPowerStore;
	ObjectCreationListStore lists{ w.keys };
	ObjectCreationListStore *savedLists = TheObjectCreationListStore;
	AttributeModifierStore attrMods;
	AttributeModifierStore *savedAttrMods = TheAttributeModifierStore;

	Local131World()
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

	~Local131World()
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

} // namespace

// Expectations from RW 0x8C71B7 (ProductionSpeedBonus::doSpecialPower) -> RW 0x6AF3C8 (Player::addProductionTimeChange)
// and RW 0x6AF48A (Player::getProductionTimeChange).
TEST_CASE("LOCAL-131: ProductionSpeedBonus changes Player::getProductionTimeChange and SweepEffectProbe detects PROD_SPEED (RW 0x8C71B7, RW 0x6AF3C8)")
{
	Local131World env;
	env.loadIni(R"(
SpecialPower SP_Local131ProdSpeed
	Enum = SPECIAL_SPELL_BOOK_CALL_THE_HORDE
	ReloadTime = 5000
End

Object Local131Trainee
	KindOf = SELECTABLE INFANTRY
	Body = ActiveBody ModuleTag_Body
		MaxHealth = 100.0
	End
End

Object Local131Wulfgar
	KindOf = SELECTABLE HERO
	Body = ActiveBody ModuleTag_Body
		MaxHealth = 500.0
	End
	Behavior = ProductionSpeedBonus ModuleTag_ProdSpeed
		SpecialPowerTemplate = SP_Local131ProdSpeed
		NumberOfFrames = 600
		SpeedMulitplier = 2.0
		Type = Local131Trainee
	End
End
)");

	Object *caster = env.spawn("Local131Wulfgar", 100.0f, 100.0f, 'A');
	REQUIRE(caster != nullptr);
	const SpecialPowerTemplate *spt = env.powers.findSpecialPowerTemplate("SP_Local131ProdSpeed");
	REQUIRE(spt != nullptr);

	EffectProbe before = EffectProbe::take(*env.logic, caster, spt);
	REQUIRE(before.expects.size() == 1);
	CHECK(before.expects[0].kind == EffectProbe::PROD_SPEED);
	CHECK(before.expects[0].templates.count("Local131Trainee") == 1);

	SpecialPowerModuleInterface *spm = SpecialPowerModules::findModule(*caster, spt);
	REQUIRE(spm != nullptr);
	spm->doSpecialPower(0);
	env.frames(2);

	EffectProbe after = EffectProbe::take(*env.logic, caster, spt);
	CHECK(env.playerOf('A')->getProductionTimeChange("Local131Trainee") == -0.5f);
	std::string missing;
	const std::string detail = before.compare(after, missing);
	CHECK(missing.empty());
	CHECK(detail.find("ProductionSpeedBonus") != std::string::npos);
}

// Expectations from RW 0x8C8A1A (CloudBreakSpecialPower::doSpecialPowerAtLocation): RW 0x8C8B57 spawns SunbeamObject when
// SunbeamObject is set and ObjectSpacing > 0; RW 0x8C8A3B applies EMOTION_UNCONTROLLABLE_FEAR otherwise.
TEST_CASE("LOCAL-131: CloudBreakSpecialPower expects SunbeamObject spawn when configured and EMOTION_UNCONTROLLABLE_FEAR otherwise (RW 0x8C8B57, RW 0x8C8A3B)")
{
	Local131World env;
	env.loadIni(R"(
SpecialPower SP_Local131CloudBreakBeam
	Enum = SPECIAL_SPELL_BOOK_CLOUD_BREAK
	ReloadTime = 5000
End

SpecialPower SP_Local131CloudBreakFear
	Enum = SPECIAL_SPELL_BOOK_CLOUD_BREAK
	ReloadTime = 5000
End

Object Local131Sunbeam
	KindOf = INERT
End

Object Local131SpellBook
	KindOf = IMMOBILE
	Behavior = CloudBreakSpecialPower ModuleTag_CBB
		SpecialPowerTemplate = SP_Local131CloudBreakBeam
		SunbeamObject = Local131Sunbeam
		ObjectSpacing = 100
	End
	Behavior = CloudBreakSpecialPower ModuleTag_CBF
		SpecialPowerTemplate = SP_Local131CloudBreakFear
	End
End
)");

	Object *book = env.spawn("Local131SpellBook", 100.0f, 100.0f, 'A');
	REQUIRE(book != nullptr);

	const SpecialPowerTemplate *spBeam = env.powers.findSpecialPowerTemplate("SP_Local131CloudBreakBeam");
	const SpecialPowerTemplate *spFear = env.powers.findSpecialPowerTemplate("SP_Local131CloudBreakFear");
	REQUIRE(spBeam != nullptr);
	REQUIRE(spFear != nullptr);

	EffectProbe pBeam = EffectProbe::take(*env.logic, book, spBeam);
	REQUIRE(pBeam.expects.size() == 1);
	CHECK(pBeam.expects[0].kind == EffectProbe::SPAWN);
	CHECK(pBeam.expects[0].templates.count("Local131Sunbeam") == 1);

	EffectProbe pFear = EffectProbe::take(*env.logic, book, spFear);
	REQUIRE(pFear.expects.size() == 1);
	CHECK(pFear.expects[0].kind == EffectProbe::EMOTION);
	CHECK(pFear.expects[0].emotions.count((int)EMOTION_UNCONTROLLABLE_FEAR) == 1);
}

// Expectations from RW 0x8B2180 (ToggleMountedSpecialAbilityUpdate::triggerAbilityEffect): when MountedTemplate is set,
// finishing PackTime creates the MountedTemplate replacement object and destroys the old caster.
TEST_CASE("LOCAL-131: SweepEffectProbe MOUNT detects MountedTemplate replacement object created after PackTime (RW 0x8B2180)")
{
	Local131World env;
	env.loadIni(R"(
SpecialPower SP_Local131Submerge
	Enum = SPECIAL_TOGGLE_MOUNTED
	ReloadTime = 5000
End

Object Local131Submerged
	KindOf = SELECTABLE MONSTER
	Body = ActiveBody ModuleTag_Body
		MaxHealth = 400.0
	End
End

Object Local131Scorpion
	KindOf = SELECTABLE MONSTER
	Body = ActiveBody ModuleTag_Body
		MaxHealth = 400.0
	End
	Behavior = SpecialPowerModule ModuleTag_SPM
		SpecialPowerTemplate = SP_Local131Submerge
		UpdateModuleStartsAttack = Yes
	End
	Behavior = ToggleMountedSpecialAbilityUpdate ModuleTag_Mount
		SpecialPowerTemplate = SP_Local131Submerge
		MountedTemplate = Local131Submerged
		PackTime = 100
		UnpackTime = 100
		IgnoreFacingCheck = Yes
	End
End
)");

	Object *caster = env.spawn("Local131Scorpion", 100.0f, 100.0f, 'A');
	REQUIRE(caster != nullptr);
	const SpecialPowerTemplate *spt = env.powers.findSpecialPowerTemplate("SP_Local131Submerge");
	REQUIRE(spt != nullptr);

	EffectProbe before = EffectProbe::take(*env.logic, caster, spt);
	REQUIRE(before.expects.size() == 1);
	CHECK(before.expects[0].kind == EffectProbe::MOUNT);
	CHECK(before.expects[0].modifier == "Local131Submerged");

	// Simulate the MountedTemplate object appearing after PackTime while the old caster is replaced.
	Object *replacement = env.spawn("Local131Submerged", 100.0f, 100.0f, 'A');
	REQUIRE(replacement != nullptr);

	EffectProbe after = EffectProbe::take(*env.logic, nullptr, spt);
	std::string missing;
	const std::string detail = before.compare(after, missing);
	CHECK(missing.empty());
	CHECK(detail.find("Local131Submerged") != std::string::npos);
}

// Expectations from RW 0x85F21B..0x85F235 (BezierProjectileBehavior::detonate): when WarheadTemplate is set and
// DetonateCallsKill is No (default), detonation calls destroyObject instead of kill, so CreateObjectDie does not run.
TEST_CASE("LOCAL-131: SweepEffectProbe ProjectileNugget skips CreateObjectDie when WarheadTemplate is set and DetonateCallsKill is No (RW 0x85F21B)")
{
	Local131World env;
	env.loadIni(R"(
SpecialPower SP_Local131BombNoKill
	Enum = SPECIAL_GENERAL_TARGETLESS
	ReloadTime = 5000
End

SpecialPower SP_Local131BombKill
	Enum = SPECIAL_GENERAL_TARGETLESS
	ReloadTime = 5000
End

Object Local131Crater
	KindOf = INERT
End

ObjectCreationList OCL_Local131Crater
	CreateObject
		ObjectNames = Local131Crater
		Count = 1
	End
End

Weapon W_Local131Warhead
	DamageNugget
		Damage = 50.0
		Radius = 20.0
		DamageType = STRUCTURAL
	End
End

Object Local131ProjNoKill
	KindOf = PROJECTILE
	Behavior = BezierProjectileBehavior ModuleTag_BPB
		DetonateCallsKill = No
	End
	Behavior = CreateObjectDie ModuleTag_COD
		CreationList = OCL_Local131Crater
	End
End

Object Local131ProjKill
	KindOf = PROJECTILE
	Behavior = BezierProjectileBehavior ModuleTag_BPB
		DetonateCallsKill = Yes
	End
	Behavior = CreateObjectDie ModuleTag_COD
		CreationList = OCL_Local131Crater
	End
End

Weapon W_Local131ThrowNoKill
	AttackRange = 150.0
	ProjectileNugget
		ProjectileTemplateName = Local131ProjNoKill
		WarheadTemplateName = W_Local131Warhead
	End
End

Weapon W_Local131ThrowKill
	AttackRange = 150.0
	ProjectileNugget
		ProjectileTemplateName = Local131ProjKill
		WarheadTemplateName = W_Local131Warhead
	End
End

Object Local131Thrower
	KindOf = SELECTABLE HERO
	Body = ActiveBody ModuleTag_Body
		MaxHealth = 500.0
	End
	Behavior = SpecialPowerModule ModuleTag_SPM1
		SpecialPowerTemplate = SP_Local131BombNoKill
		UpdateModuleStartsAttack = Yes
	End
	Behavior = WeaponFireSpecialAbilityUpdate ModuleTag_WF1
		SpecialPowerTemplate = SP_Local131BombNoKill
		SpecialWeapon = W_Local131ThrowNoKill
	End
	Behavior = SpecialPowerModule ModuleTag_SPM2
		SpecialPowerTemplate = SP_Local131BombKill
		UpdateModuleStartsAttack = Yes
	End
	Behavior = WeaponFireSpecialAbilityUpdate ModuleTag_WF2
		SpecialPowerTemplate = SP_Local131BombKill
		SpecialWeapon = W_Local131ThrowKill
	End
End
)");

	Object *caster = env.spawn("Local131Thrower", 100.0f, 100.0f, 'A');
	REQUIRE(caster != nullptr);

	const SpecialPowerTemplate *spNoKill = env.powers.findSpecialPowerTemplate("SP_Local131BombNoKill");
	const SpecialPowerTemplate *spKill = env.powers.findSpecialPowerTemplate("SP_Local131BombKill");
	REQUIRE(spNoKill != nullptr);
	REQUIRE(spKill != nullptr);

	EffectProbe pNoKill = EffectProbe::take(*env.logic, caster, spNoKill);
	bool hasSpawnNoKill = false;
	for (const EffectProbe::Expect &e : pNoKill.expects)
	{
		if (e.kind == EffectProbe::SPAWN && e.templates.count("Local131Crater"))
		{
			hasSpawnNoKill = true;
		}
	}
	CHECK_FALSE(hasSpawnNoKill);

	EffectProbe pKill = EffectProbe::take(*env.logic, caster, spKill);
	bool hasSpawnKill = false;
	for (const EffectProbe::Expect &e : pKill.expects)
	{
		if (e.kind == EffectProbe::SPAWN && e.templates.count("Local131Crater"))
		{
			hasSpawnKill = true;
		}
	}
	CHECK(hasSpawnKill);
}

// Expectations from RW 0x852580 (SpecialAbilityUpdate::triggerAbilityEffect): TriggerAttributeModifier is looked up in
// TheAttributeModifierStore and silently skipped when absent; SweepEffectProbe must not expect a nonexistent modifier.
TEST_CASE("LOCAL-131: SweepEffectProbe skips SpecialAbilityUpdate TriggerAttributeModifier missing from AttributeModifierStore (RW 0x852580)")
{
	Local131World env;
	env.loadIni(R"(
SpecialPower SP_Local131Charge
	Enum = SPECIAL_CHARGE_ATTACK
	ReloadTime = 5000
End

Object Local131Charger
	KindOf = SELECTABLE INFANTRY
	Body = ActiveBody ModuleTag_Body
		MaxHealth = 300.0
	End
	Behavior = SpecialPowerModule ModuleTag_SPM
		SpecialPowerTemplate = SP_Local131Charge
		UpdateModuleStartsAttack = Yes
	End
	Behavior = SpecialAbilityUpdate ModuleTag_SAU
		SpecialPowerTemplate = SP_Local131Charge
		TriggerAttributeModifier = NonExistentLocal131Modifier
	End
End
)");

	Object *caster = env.spawn("Local131Charger", 100.0f, 100.0f, 'A');
	REQUIRE(caster != nullptr);
	const SpecialPowerTemplate *spt = env.powers.findSpecialPowerTemplate("SP_Local131Charge");
	REQUIRE(spt != nullptr);

	EffectProbe probe = EffectProbe::take(*env.logic, caster, spt);
	bool hasMissingMod = false;
	for (const EffectProbe::Expect &e : probe.expects)
	{
		if (e.kind == EffectProbe::MODIFIER && e.modifier == "NonExistentLocal131Modifier")
		{
			hasMissingMod = true;
		}
	}
	CHECK_FALSE(hasMissingMod);
}

// Expectations from RW 0x9100AA (MetaImpactNugget::shouldDeliver): rejects targets where
// ShockwaveResistance * 0.2 >= ShockWaveAmount (parsed as VelocityReal = value / LOGICFRAMES_PER_SECOND (5)), while knocking back 0-resistance targets.
TEST_CASE("LOCAL-131: MetaImpactNugget rejects high ShockwaveResistance heroes and hits 0-resistance targets (RW 0x9100AA)")
{
	Local131World env;
	env.loadIni(R"(
Weapon W_Local131Whip
	AttackRange = 240.0
	RadiusDamageAffects = ALLIES ENEMIES NEUTRALS
	MetaImpactNugget
		HeroResist = 0.0
		ShockWaveAmount = 50.0
		ShockWaveRadius = 25.0
		ShockWaveTaperOff = 1.0
		ShockWaveZMult = 1.20
		InvertShockWave = Yes
	End
End

Object Local131Balrog
	KindOf = SELECTABLE MONSTER
	Body = ActiveBody ModuleTag_Body
		MaxHealth = 5000.0
	End
End

Object Local131ResistantHero
	KindOf = SELECTABLE HERO
	ShockwaveResistance = 74.0
	Body = ActiveBody ModuleTag_Body
		MaxHealth = 5000.0
	End
End

Object Local131ZeroResistHero
	KindOf = SELECTABLE HERO
	Body = ActiveBody ModuleTag_Body
		MaxHealth = 5000.0
	End
	Behavior = PhysicsBehavior ModuleTag_Phys
		GravityMult = 1.0
	End
End
)");

	Object *balrog = env.spawn("Local131Balrog", 100.0f, 100.0f, 'A');
	Object *resHero = env.spawn("Local131ResistantHero", 150.0f, 100.0f, 'B');
	Object *zeroHero = env.spawn("Local131ZeroResistHero", 250.0f, 100.0f, 'B');
	REQUIRE(balrog != nullptr);
	REQUIRE(resHero != nullptr);
	REQUIRE(zeroHero != nullptr);

	env.frames(1);
	const WeaponTemplate *w = TheWeaponStore->findWeaponTemplate("W_Local131Whip");
	REQUIRE(w != nullptr);

	const unsigned long long beforeRes = env.combat().counters().metaImpactHits;
	DeliverNuggets(*env.logic, balrog->getID(), *w, WeaponBonus{}, resHero, resHero->getPosition(), false, nullptr);
	CHECK(env.combat().counters().metaImpactHits == beforeRes); // 74 * 0.2 = 14.8 >= 50/5 = 10.0 -> resisted

	const unsigned long long beforeZero = env.combat().counters().metaImpactHits;
	DeliverNuggets(*env.logic, balrog->getID(), *w, WeaponBonus{}, zeroHero, zeroHero->getPosition(), false, nullptr);
	CHECK(env.combat().counters().metaImpactHits == beforeZero + 2); // 0 * 0.2 = 0 < 10.0 -> direct + radius hit
}
