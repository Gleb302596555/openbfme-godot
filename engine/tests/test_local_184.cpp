// OpenBFME. GPL-3.0.
// LOCAL-184 (fork issue 977): matrix red buttons — SpecialPowerModule omitted AttributeModifierAffects,
// ModelConditionSpecialAbilityUpdate emotion pulse / UsingSpecialOne Lua fear, HeroModeSpecialAbilityUpdate
// absent HeroAttributeModifier, SpecialAbilityUpdate SPECIAL_SCREECH with omitted EffectRange.
// Editions: both RotWK 2.01 and BFME2 1.06.
// Sources:
//   - SpecialPowerModuleModuleData ctor omitted AttributeModifierAffects: RW 0x0089693A -> RW 0x00763D11 (empty NONE filter)
//   - SpecialPowerModule::triggerSpecialPower modifier scan: RW 0x008977BF / RW 0x00897BAB..0x00897C47
//   - AttributeModifierPool::update expiry: RW 0x0080585F (Duration 5000 ms -> 25 frames)
//   - ModelConditionSpecialAbilityUpdate: RW 0x0089455B (startUnpacking), RW 0x008945C1 (finishAbility), RW 0x0089463A -> RW 0x00894701 (emotionPulse)
//   - HeroModeSpecialAbilityUpdate: RW 0x008958BD (startUnpacking), RW 0x008959B5 (triggerAbilityEffect -> RW 0x0068F1A8 / RW 0x0068B581)
//   - SpecialAbilityUpdate SPECIAL_SCREECH: RW 0x0085491F..0x00854977 (dispatches BeScary with m_effectRange)
//   - Ennorath INI: data/ini/object/goodfaction/units/gondor/gondorboromir.ini, gondoraragorn.ini,
//     data/ini/object/evilfaction/units/mordor/mordornazgulmarshal.ini,
//     data/ini/object/goodfaction/units/lothlorien/dorwinionnaturmagier.ini,
//     data/ini/attributemodifier.ini, data/ini/includes/attributemodifier.inc, data/scripts/scriptevents.xml, data/scripts/scripts.lua

#include "doctest.h"

#include "CombatTestUtil.h"
#include "EnnorathScenario.h"
#include "HudTestUtil.h"
#include "SweepEffectProbe.h"

#include "Common/Player.h"
#include "Common/SpecialPower.h"
#include "GameClient/FXList.h"
#include "GameLogic/AI/AIEmotionStates.h"
#include "GameLogic/AttributeModifiers.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/EmotionModules.h"
#include "GameLogic/Module/SpecialAbilityModules.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Object/AttributeModifierPool.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/System/EmotionSystem.h"

#include <string>

namespace
{
struct Local184World : combattest::CombatWorld
{
	SpecialPowerStore powers;
	SpecialPowerStore *savedPowers = TheSpecialPowerStore;
	FXListStore fxLists;
	FXListStore *savedFx = TheFXListStore;
	AttributeModifierStore modifiers;
	AttributeModifierStore *savedModifiers = TheAttributeModifierStore;
	EmotionSystem emotions;
	EmotionSystem *savedEmotions = TheEmotionSystem;

	explicit Local184World(const char *extra)
	{
		TheSpecialPowerStore = &powers;
		TheFXListStore = &fxLists;
		TheAttributeModifierStore = &modifiers;
		TheEmotionSystem = &emotions;
		emotions.registerBlock(w.fx.env.blocks);
		modifiers.registerBlock(w.fx.env.blocks);
		w.fx.env.blocks.registerBlock("FXList", [](INI *ini) { ParseFXListDefinitionGlobal(ini); });
		w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
		SpecialAbilityModules::registerAll(w.modules);
		SpecialPowerModules::registerAll(w.modules);
		const std::string err = w.load(extra);
		REQUIRE_MESSAGE(err.empty(), err);
	}
	~Local184World()
	{
		TheSpecialPowerStore = savedPowers;
		TheFXListStore = savedFx;
		TheAttributeModifierStore = savedModifiers;
		TheEmotionSystem = savedEmotions;
	}
};

template <typename T>
T *findTypedModule(Object &o)
{
	for (const std::unique_ptr<BehaviorModule> &m : o.modules())
	{
		if (T *t = dynamic_cast<T *>(m.get()))
		{
			return t;
		}
	}
	return nullptr;
}

const char kEmotionNuggets[] =
	"EmotionNugget Local184TerrorNugget\n"
	"  Type = TERROR\n"
	"  AIState = RUN_AWAY_PANIC\n"
	"  Duration = 5000\n"
	"End\n"
	"EmotionNugget Local184UncontrollableFearNugget\n"
	"  Type = UNCONTROLLABLE_FEAR\n"
	"  AIState = IDLE\n"
	"  Duration = 5000\n"
	"End\n";

const char kSpecialPowerModifierIni[] =
	"ModifierList Local184BuffMod\n"
	"  Category = BUFF\n"
	"  Modifier = DAMAGE_MULT 125%\n"
	"  Duration = 5000\n"
	"End\n"
	"ModifierList Local184EmptyCarrier\n"
	"  Duration = 5000\n"
	"End\n"
	"SpecialPower SpecialAbilityLocal184Buff\n"
	"  Enum = SPECIAL_GENERAL_TARGETLESS\n"
	"  ReloadTime = 1000\n"
	"End\n"
	"SpecialPower SpecialAbilityLocal184OmittedFilter\n"
	"  Enum = SPECIAL_GENERAL_TARGETLESS\n"
	"  ReloadTime = 1000\n"
	"End\n"
	"Object Local184Caster\n"
	"  KindOf = SELECTABLE HERO INFANTRY\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = AttributeModifierPoolUpdate ModuleTag_Pool\n"
	"  End\n"
	"  Behavior = SpecialPowerModule ModuleTag_Buff\n"
	"    SpecialPowerTemplate = SpecialAbilityLocal184Buff\n"
	"    AttributeModifier = Local184BuffMod\n"
	"    AttributeModifierRange = 150.0\n"
	"    AttributeModifierAffectsSelf = No\n"
	"    AttributeModifierAffects = ANY +INFANTRY ALLIES\n"
	"  End\n"
	"  Behavior = SpecialPowerModule ModuleTag_Omitted\n"
	"    SpecialPowerTemplate = SpecialAbilityLocal184OmittedFilter\n"
	"    UpdateModuleStartsAttack = Yes\n"
	"    AttributeModifier = Local184EmptyCarrier\n"
	"    AttributeModifierRange = 150.0\n"
	"  End\n"
	"  Behavior = ModelConditionSpecialAbilityUpdate ModuleTag_OmittedUpdate\n"
	"    SpecialPowerTemplate = SpecialAbilityLocal184OmittedFilter\n"
	"    UnpackTime = 200\n"
	"    PreparationTime = 1\n"
	"    PersistentPrepTime = 400\n"
	"    PackTime = 200\n"
	"    WhichSpecialPower = 1\n"
	"  End\n"
	"End\n"
	"Object Local184Infantry\n"
	"  KindOf = SELECTABLE INFANTRY\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 200\n"
	"  End\n"
	"  Behavior = AttributeModifierPoolUpdate ModuleTag_Pool\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"End\n"
	"Object Local184Structure\n"
	"  KindOf = SELECTABLE STRUCTURE\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = AttributeModifierPoolUpdate ModuleTag_Pool\n"
	"  End\n"
	"End\n";

const char kEmotionPulseIni[] =
	"EmotionNugget Local184TerrorNugget\n"
	"  Type = TERROR\n"
	"  AIState = RUN_AWAY_PANIC\n"
	"  Duration = 5000\n"
	"End\n"
	"EmotionNugget Local184UncontrollableFearNugget\n"
	"  Type = UNCONTROLLABLE_FEAR\n"
	"  AIState = IDLE\n"
	"  Duration = 5000\n"
	"End\n"
	"SpecialPower SpecialAbilityLocal184Terror\n"
	"  Enum = SPECIAL_GENERAL_TARGETLESS\n"
	"  ReloadTime = 1000\n"
	"End\n"
	"SpecialPower SpecialAbilityLocal184Fear\n"
	"  Enum = SPECIAL_GENERAL_TARGETLESS\n"
	"  ReloadTime = 1000\n"
	"End\n"
	"Object Local184PulseHero\n"
	"  KindOf = SELECTABLE HERO\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = EmotionTrackerUpdate ModuleTag_Emo\n"
	"    AddEmotion = Local184TerrorNugget\n"
	"    AddEmotion = Local184UncontrollableFearNugget\n"
	"  End\n"
	"  Behavior = SpecialPowerModule ModuleTag_TerrorSP\n"
	"    SpecialPowerTemplate = SpecialAbilityLocal184Terror\n"
	"    UpdateModuleStartsAttack = Yes\n"
	"  End\n"
	"  Behavior = ModelConditionSpecialAbilityUpdate ModuleTag_TerrorUpd\n"
	"    SpecialPowerTemplate = SpecialAbilityLocal184Terror\n"
	"    UnpackTime = 200\n"
	"    PreparationTime = 1\n"
	"    PersistentPrepTime = 400\n"
	"    PackTime = 200\n"
	"    WhichSpecialPower = 1\n"
	"    GenerateTerror = Yes\n"
	"    EmotionPulseRadius = 180.0\n"
	"  End\n"
	"  Behavior = SpecialPowerModule ModuleTag_FearSP\n"
	"    SpecialPowerTemplate = SpecialAbilityLocal184Fear\n"
	"    UpdateModuleStartsAttack = Yes\n"
	"  End\n"
	"  Behavior = ModelConditionSpecialAbilityUpdate ModuleTag_FearUpd\n"
	"    SpecialPowerTemplate = SpecialAbilityLocal184Fear\n"
	"    UnpackTime = 200\n"
	"    PreparationTime = 1\n"
	"    PersistentPrepTime = 400\n"
	"    PackTime = 200\n"
	"    WhichSpecialPower = 2\n"
	"    GenerateUncontrollableFear = Yes\n"
	"    EmotionPulseRadius = 180.0\n"
	"  End\n"
	"End\n"
	"Object Local184Target\n"
	"  KindOf = SELECTABLE INFANTRY\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 200\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = EmotionTrackerUpdate ModuleTag_Emo\n"
	"    AddEmotion = Local184TerrorNugget\n"
	"    AddEmotion = Local184UncontrollableFearNugget\n"
	"  End\n"
	"End\n";

const char kHeroModeIni[] =
	"Weapon Local184Sword\n"
	"  AttackRange = 20.0\n"
	"  DelayBetweenShots = 1000\n"
	"  DamageNugget\n"
	"    Damage = 10.0\n"
	"  End\n"
	"End\n"
	"ModifierList Local184ShieldMod\n"
	"  Category = SPELL\n"
	"  Modifier = ARMOR 100%\n"
	"  Duration = 5000\n"
	"End\n"
	"SpecialPower SpecialAbilityLocal184ShieldPresent\n"
	"  Enum = SPECIAL_HERO_MODE\n"
	"  ReloadTime = 1000\n"
	"End\n"
	"SpecialPower SpecialAbilityLocal184ShieldAbsent\n"
	"  Enum = SPECIAL_HERO_MODE\n"
	"  ReloadTime = 1000\n"
	"End\n"
	"Object Local184HeroPresent\n"
	"  KindOf = SELECTABLE HERO\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY Local184Sword\n"
	"  End\n"
	"  WeaponSet\n"
	"    Conditions = HERO\n"
	"    Weapon = PRIMARY Local184Sword\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = SpecialPowerModule ModuleTag_SP\n"
	"    SpecialPowerTemplate = SpecialAbilityLocal184ShieldPresent\n"
	"  End\n"
	"  Behavior = AttributeModifierPoolUpdate ModuleTag_Pool\n"
	"  End\n"
	"  Behavior = HeroModeSpecialAbilityUpdate ModuleTag_HM\n"
	"    SpecialPowerTemplate = SpecialAbilityLocal184ShieldPresent\n"
	"    HeroAttributeModifier = Local184ShieldMod\n"
	"    HeroEffectDuration = 5000\n"
	"    UnpackTime = 1\n"
	"  End\n"
	"End\n"
	"Object Local184HeroAbsent\n"
	"  KindOf = SELECTABLE HERO\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY Local184Sword\n"
	"  End\n"
	"  WeaponSet\n"
	"    Conditions = HERO\n"
	"    Weapon = PRIMARY Local184Sword\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = SpecialPowerModule ModuleTag_SP\n"
	"    SpecialPowerTemplate = SpecialAbilityLocal184ShieldAbsent\n"
	"  End\n"
	"  Behavior = AttributeModifierPoolUpdate ModuleTag_Pool\n"
	"  End\n"
	"  Behavior = HeroModeSpecialAbilityUpdate ModuleTag_HM\n"
	"    SpecialPowerTemplate = SpecialAbilityLocal184ShieldAbsent\n"
	"    HeroAttributeModifier = Local184NonExistentMod\n"
	"    HeroEffectDuration = 5000\n"
	"    UnpackTime = 1\n"
	"  End\n"
	"End\n";

const char kScreechIni[] =
	"ModifierList Local184HarvestMod\n"
	"  Category = SPELL\n"
	"  Modifier = PRODUCTION 125%\n"
	"  Duration = 5000\n"
	"End\n"
	"SpecialPower SpecialAbilityLocal184Harvest\n"
	"  Enum = SPECIAL_SCREECH\n"
	"  ReloadTime = 1000\n"
	"End\n"
	"SpecialPower SpecialAbilityLocal184RealScreech\n"
	"  Enum = SPECIAL_SCREECH\n"
	"  ReloadTime = 1000\n"
	"End\n"
	"Object Local184Tree\n"
	"  KindOf = SELECTABLE STRUCTURE\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = AttributeModifierPoolUpdate ModuleTag_Pool\n"
	"  End\n"
	"End\n"
	"Object Local184Mage\n"
	"  KindOf = SELECTABLE HERO\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = SpecialPowerModule ModuleTag_HarvestSP\n"
	"    SpecialPowerTemplate = SpecialAbilityLocal184Harvest\n"
	"    UpdateModuleStartsAttack = Yes\n"
	"    AttributeModifier = Local184HarvestMod\n"
	"    AttributeModifierRange = 300.0\n"
	"    AttributeModifierAffectsSelf = No\n"
	"    AttributeModifierAffects = ANY +Local184Tree ALLIES\n"
	"  End\n"
	"  Behavior = SpecialAbilityUpdate ModuleTag_HarvestAnim\n"
	"    SpecialPowerTemplate = SpecialAbilityLocal184Harvest\n"
	"    UnpackTime = 100\n"
	"    PreparationTime = 0\n"
	"    PersistentPrepTime = 0\n"
	"    PackTime = 100\n"
	"  End\n"
	"  Behavior = SpecialPowerModule ModuleTag_ScreechSP\n"
	"    SpecialPowerTemplate = SpecialAbilityLocal184RealScreech\n"
	"    UpdateModuleStartsAttack = Yes\n"
	"  End\n"
	"  Behavior = SpecialAbilityUpdate ModuleTag_ScreechUpd\n"
	"    SpecialPowerTemplate = SpecialAbilityLocal184RealScreech\n"
	"    EffectRange = 180.0\n"
	"  End\n"
	"End\n";
} // namespace

// RW 0x0089693A -> RW 0x00763D11 (omitted AttributeModifierAffects = empty NONE filter),
// RW 0x00897BAB..0x00897C47 (SpecialPowerModule attribute modifier scan), RW 0x0080585F (Duration expiry: 5000 ms = 25 frames).
TEST_CASE("LOCAL-184 SpecialPowerModule AttributeModifier filter, duration, and omitted filter (RW 0x0089693A / RW 0x00897BAB)")
{
	Local184World w(kSpecialPowerModifierIni);
	Object *caster = w.unit("Local184Caster", 'A', 100.0f, 100.0f);
	Object *allyInf = w.unit("Local184Infantry", 'A', 140.0f, 100.0f);
	Object *allyStruct = w.unit("Local184Structure", 'A', 140.0f, 120.0f);
	Object *enemyInf = w.unit("Local184Infantry", 'B', 140.0f, 80.0f);
	REQUIRE(caster != nullptr);
	REQUIRE(allyInf != nullptr);
	REQUIRE(allyStruct != nullptr);
	REQUIRE(enemyInf != nullptr);

	// 1. Filtered modifier: fitting ally receives DAMAGE_MULT 1.25 for 25 frames, then expires; out-of-filter, enemy, and caster do not.
	const SpecialPowerTemplate *spBuff = TheSpecialPowerStore->findSpecialPowerTemplate("SpecialAbilityLocal184Buff");
	REQUIRE(spBuff != nullptr);
	auto *spmBuff = dynamic_cast<SpecialPowerModule *>(SpecialPowerModules::findModule(*caster, spBuff));
	REQUIRE(spmBuff != nullptr);
	spmBuff->doSpecialPower(0);
	w.frames(1);

	float dmg = 1.0f;
	CHECK(allyInf->attributeModifierProduct(ATTRIBUTE_DAMAGE_MULT, nullptr, true, dmg));
	CHECK(dmg == 1.25f);
	CHECK_FALSE(allyStruct->attributeModifierProduct(ATTRIBUTE_DAMAGE_MULT, nullptr, true, dmg));
	CHECK_FALSE(enemyInf->attributeModifierProduct(ATTRIBUTE_DAMAGE_MULT, nullptr, true, dmg));
	CHECK_FALSE(caster->attributeModifierProduct(ATTRIBUTE_DAMAGE_MULT, nullptr, true, dmg));

	w.frames(26);
	dmg = 1.0f;
	CHECK_FALSE(allyInf->attributeModifierProduct(ATTRIBUTE_DAMAGE_MULT, nullptr, true, dmg));

	// 2. Omitted AttributeModifierAffects (RW 0x0089693A -> RW 0x00763D11): 0 victims receive Local184EmptyCarrier,
	//    and SweepEffectProbe does not report a false NO-EFFECT when paired with ModelConditionSpecialAbilityUpdate.
	const SpecialPowerTemplate *spOmitted = TheSpecialPowerStore->findSpecialPowerTemplate("SpecialAbilityLocal184OmittedFilter");
	REQUIRE(spOmitted != nullptr);
	sweep_probe::EffectProbe before = sweep_probe::EffectProbe::take(*w.logic, caster, spOmitted);
	auto *spmOmitted = dynamic_cast<SpecialPowerModule *>(SpecialPowerModules::findModule(*caster, spOmitted));
	REQUIRE(spmOmitted != nullptr);
	spmOmitted->doSpecialPower(0);
	w.frames(2);
	sweep_probe::EffectProbe during = sweep_probe::EffectProbe::take(*w.logic, caster, spOmitted);
	std::string missing;
	const std::string notes = before.compare(during, missing);
	CHECK_MESSAGE(missing.empty(), "missing=" << missing << " notes=" << notes);
	if (const auto *pool = static_cast<const AttributeModifierPool *>(allyInf->findModule("AttributeModifierPoolUpdate")))
	{
		CHECK_FALSE(pool->hasList("Local184EmptyCarrier"));
	}
}

// RW 0x0089455B (ModelConditionSpecialAbilityUpdate::startUnpacking), RW 0x008945C1 (finishAbility),
// RW 0x0089463A -> RW 0x00894701 (emotionPulse: ENEMIES within EmotionPulseRadius receive EMOTION_TERROR / EMOTION_UNCONTROLLABLE_FEAR).
TEST_CASE("LOCAL-184 ModelConditionSpecialAbilityUpdate emotion pulse (RW 0x0089455B / RW 0x0089463A)")
{
	Local184World w(kEmotionPulseIni);
	Object *hero = w.unit("Local184PulseHero", 'A', 100.0f, 100.0f);
	Object *ally = w.unit("Local184Target", 'A', 150.0f, 100.0f);
	Object *foe = w.unit("Local184Target", 'B', 150.0f, 120.0f);
	REQUIRE(hero != nullptr);
	REQUIRE(ally != nullptr);
	REQUIRE(foe != nullptr);

	const int kSp1 = CombatNames::modelCondition("SPECIAL_POWER_1");
	const int kSp2 = CombatNames::modelCondition("SPECIAL_POWER_2");

	// 1. WhichSpecialPower = 1 + GenerateTerror: SPECIAL_POWER_1 on hero, EMOTION_TERROR on enemy only, cleared after finish.
	const SpecialPowerTemplate *spTerror = TheSpecialPowerStore->findSpecialPowerTemplate("SpecialAbilityLocal184Terror");
	REQUIRE(spTerror != nullptr);
	auto *spmTerror = dynamic_cast<SpecialPowerModule *>(SpecialPowerModules::findModule(*hero, spTerror));
	REQUIRE(spmTerror != nullptr);
	spmTerror->doSpecialPower(0);
	bool sawSp1 = false, sawTerror = false, allyTerrified = false;
	for (int i = 0; i < 30; ++i)
	{
		w.frames(1);
		sawSp1 = sawSp1 || hero->testModelCondition(kSp1);
		if (const auto *e = findTypedModule<EmotionTrackerUpdate>(*foe))
		{
			sawTerror = sawTerror || e->requested(EMOTION_TERROR);
		}
		if (const auto *e = findTypedModule<EmotionTrackerUpdate>(*ally))
		{
			allyTerrified = allyTerrified || e->requested(EMOTION_TERROR);
		}
	}
	CHECK(sawSp1);
	CHECK(sawTerror);
	CHECK_FALSE(allyTerrified);
	CHECK_FALSE(hero->testModelCondition(kSp1));

	// 2. WhichSpecialPower = 2 + GenerateUncontrollableFear: SPECIAL_POWER_2 on hero, EMOTION_UNCONTROLLABLE_FEAR on enemy,
	//    and enemy refuses player commands while locked.
	const SpecialPowerTemplate *spFear = TheSpecialPowerStore->findSpecialPowerTemplate("SpecialAbilityLocal184Fear");
	REQUIRE(spFear != nullptr);
	auto *spmFear = dynamic_cast<SpecialPowerModule *>(SpecialPowerModules::findModule(*hero, spFear));
	REQUIRE(spmFear != nullptr);
	spmFear->doSpecialPower(0);
	bool sawSp2 = false, sawFear = false, sawLocked = false;
	for (int i = 0; i < 30; ++i)
	{
		w.frames(1);
		sawSp2 = sawSp2 || hero->testModelCondition(kSp2);
		if (const auto *e = findTypedModule<EmotionTrackerUpdate>(*foe))
		{
			sawFear = sawFear || e->requested(EMOTION_UNCONTROLLABLE_FEAR);
		}
		if (const AIUpdateInterface *ai = foe->getAIUpdateInterface())
		{
			if (!ai->allowedToRespondToCommand(CMD_FROM_PLAYER, 0))
			{
				sawLocked = true;
			}
		}
	}
	CHECK(sawSp2);
	CHECK(sawFear);
	CHECK(sawLocked);
}

// RW 0x008958BD (HeroModeSpecialAbilityUpdate::startUnpacking -> WEAPONSET_HERO_MODE),
// RW 0x008959B5 (triggerAbilityEffect -> RW 0x0068F1A8 addAttributeModifier + RW 0x0068B581 HERO model condition).
TEST_CASE("LOCAL-184 HeroModeSpecialAbilityUpdate present vs absent HeroAttributeModifier (RW 0x008958BD / RW 0x008959B5)")
{
	Local184World w(kHeroModeIni);
	Object *present = w.unit("Local184HeroPresent", 'A', 100.0f, 100.0f);
	Object *absent = w.unit("Local184HeroAbsent", 'A', 160.0f, 100.0f);
	REQUIRE(present != nullptr);
	REQUIRE(absent != nullptr);

	const int kHeroMc = CombatNames::modelCondition("HERO");

	// 1. Present HeroAttributeModifier: modifier lands, HERO + WEAPONSET_HERO_MODE set, SweepEffectProbe passes.
	const SpecialPowerTemplate *spPres = TheSpecialPowerStore->findSpecialPowerTemplate("SpecialAbilityLocal184ShieldPresent");
	REQUIRE(spPres != nullptr);
	sweep_probe::EffectProbe beforePres = sweep_probe::EffectProbe::take(*w.logic, present, spPres);
	auto *spmPres = dynamic_cast<SpecialPowerModule *>(SpecialPowerModules::findModule(*present, spPres));
	REQUIRE(spmPres != nullptr);
	spmPres->doSpecialPower(0);
	w.frames(5);
	CHECK(present->testModelCondition(kHeroMc));
	CHECK(((present->getWeaponSetFlags()[0] >> 0x1B) & 1u) == 1u);
	sweep_probe::EffectProbe afterPres = sweep_probe::EffectProbe::take(*w.logic, present, spPres);
	std::string missingPres;
	const std::string notesPres = beforePres.compare(afterPres, missingPres);
	CHECK_MESSAGE(missingPres.empty(), "missing=" << missingPres << " notes=" << notesPres);

	// 2. Absent HeroAttributeModifier (GondorBoromir_mod_ElrondsRat BoromirSchild shape): RW 0x0068F1A8 adds nothing,
	//    HERO + WEAPONSET_HERO_MODE still set, and SweepEffectProbe expects HERO and passes.
	CHECK(TheAttributeModifierStore->find("Local184NonExistentMod") == nullptr);
	const SpecialPowerTemplate *spAbs = TheSpecialPowerStore->findSpecialPowerTemplate("SpecialAbilityLocal184ShieldAbsent");
	REQUIRE(spAbs != nullptr);
	sweep_probe::EffectProbe beforeAbs = sweep_probe::EffectProbe::take(*w.logic, absent, spAbs);
	auto *spmAbs = dynamic_cast<SpecialPowerModule *>(SpecialPowerModules::findModule(*absent, spAbs));
	REQUIRE(spmAbs != nullptr);
	spmAbs->doSpecialPower(0);
	w.frames(5);
	CHECK(absent->testModelCondition(kHeroMc));
	CHECK(((absent->getWeaponSetFlags()[0] >> 0x1B) & 1u) == 1u);
	sweep_probe::EffectProbe afterAbs = sweep_probe::EffectProbe::take(*w.logic, absent, spAbs);
	std::string missingAbs;
	const std::string notesAbs = beforeAbs.compare(afterAbs, missingAbs);
	CHECK_MESSAGE(missingAbs.empty(), "missing=" << missingAbs << " notes=" << notesAbs);
}

// RW 0x0085491F..0x00854977: SpecialAbilityUpdate SPECIAL_SCREECH dispatches BeScary(self, m_effectRange).
// When EffectRange is omitted (0.0f, DorwinionNaturmagier shape), SweepEffectProbe must not require TERROR.
TEST_CASE("LOCAL-184 SpecialAbilityUpdate SPECIAL_SCREECH EffectRange > 0 vs omitted EffectRange (RW 0x0085491F)")
{
	Local184World w(kScreechIni);
	Object *mage = w.unit("Local184Mage", 'A', 100.0f, 100.0f);
	Object *tree = w.unit("Local184Tree", 'A', 160.0f, 100.0f);
	REQUIRE(mage != nullptr);
	REQUIRE(tree != nullptr);

	// 1. Omitted EffectRange (0.0f): SweepEffectProbe expects only SpecialPowerModule's Local184HarvestMod, not TERROR.
	const SpecialPowerTemplate *spHarvest = TheSpecialPowerStore->findSpecialPowerTemplate("SpecialAbilityLocal184Harvest");
	REQUIRE(spHarvest != nullptr);
	sweep_probe::EffectProbe before = sweep_probe::EffectProbe::take(*w.logic, mage, spHarvest);
	for (const auto &ex : before.expects)
	{
		CHECK(ex.kind != sweep_probe::EffectProbe::TERROR);
	}
	auto *spm = dynamic_cast<SpecialPowerModule *>(SpecialPowerModules::findModule(*mage, spHarvest));
	REQUIRE(spm != nullptr);
	spm->doSpecialPower(0);
	w.frames(10);
	sweep_probe::EffectProbe after = sweep_probe::EffectProbe::take(*w.logic, mage, spHarvest);
	std::string missing;
	const std::string notes = before.compare(after, missing);
	CHECK_MESSAGE(missing.empty(), "missing=" << missing << " notes=" << notes);

	// 2. Positive EffectRange (180.0f): SweepEffectProbe still expects TERROR.
	const SpecialPowerTemplate *spScreech = TheSpecialPowerStore->findSpecialPowerTemplate("SpecialAbilityLocal184RealScreech");
	REQUIRE(spScreech != nullptr);
	sweep_probe::EffectProbe screechProbe = sweep_probe::EffectProbe::take(*w.logic, mage, spScreech);
	bool hasTerror = false;
	for (const auto &ex : screechProbe.expects)
	{
		if (ex.kind == sweep_probe::EffectProbe::TERROR)
		{
			hasTerror = true;
		}
	}
	CHECK(hasTerror);
}

// Ennorath INI sources:
// - GondorBoromir_mod, GondorBoromir_OneRing, GondorBoromir_mod_ElrondsRat (gondorboromir.ini)
// - MordorNazgulMarshal (mordornazgulmarshal.ini)
// - GondorAragorn_SummonedHunters (gondoraragorn.ini)
// - DorwinionNaturmagier (dorwinionnaturmagier.ini)
TEST_CASE("LOCAL-184 ENNORATH scenario: 7 red buttons (Boromir horns, Nazgul Marshal, Elendil, Boromir shield, harvest blessing)")
{
	ennorathscenario::World world;
	if (!world.load("LOCAL-184 Ennorath scenario"))
	{
		return;
	}

	hudtest::Rig game(world.shared, "map mp fall back 8p", "FactionMen");
	const int p1 = game.index();
	Player *enemyPlayer = game.game->players().findPlayerWithName("Player_2");
	REQUIRE(enemyPlayer != nullptr);
	const int p2 = enemyPlayer->getPlayerIndex();
	std::string err;

	auto unpauseAndFire = [&](Object *u, const char *spName) -> const SpecialPowerTemplate * {
		const SpecialPowerTemplate *sp = TheSpecialPowerStore->findSpecialPowerTemplate(spName);
		REQUIRE(sp != nullptr);
		auto *spm = dynamic_cast<SpecialPowerModule *>(SpecialPowerModules::findModule(*u, sp));
		REQUIRE(spm != nullptr);
		while (spm->pauseCount() > 0)
		{
			spm->pauseCountdown(false);
		}
		spm->doSpecialPower(0);
		return sp;
	};

	// 1-4. Boromir horns & Nazgul Marshal screech: ModelConditionSpecialAbilityUpdate sets SPECIAL_POWER_1 (RW 0x0089455B),
	//      Lua ModelConditionEvent UsingSpecialOne broadcasts BeUncontrollablyAfraid -> EMOTION_UNCONTROLLABLE_FEAR on enemies,
	//      and SweepEffectProbe passes (omitted AttributeModifierAffects RW 0x0089693A).
	const char *hornUnits[] = { "GondorBoromir_mod", "GondorBoromir_OneRing", "GondorBoromir_mod_ElrondsRat", "MordorNazgulMarshal" };
	for (int idx = 0; idx < 4; ++idx)
	{
		const float baseX = 1400.0f + idx * 350.0f;
		Object *hero = game.game->createObject(hornUnits[idx], p1, Coord3D{ baseX, 2300.0f, 0.0f }, 0.0f, &err);
		REQUIRE_MESSAGE(hero != nullptr, err);
		Object *foe = game.game->createObject("GondorFighterHorde", p2, Coord3D{ baseX + 60.0f, 2300.0f, 0.0f }, 0.0f, &err);
		REQUIRE_MESSAGE(foe != nullptr, err);
		const SpecialPowerTemplate *sp = TheSpecialPowerStore->findSpecialPowerTemplate("SpecialAbilityHornOfGondor02");
		REQUIRE(sp != nullptr);
		sweep_probe::EffectProbe before = sweep_probe::EffectProbe::take(game.logic(), hero, sp);
		unpauseAndFire(hero, "SpecialAbilityHornOfGondor02");

		bool sawSp1 = false, sawFear = false, probePassed = false;
		const int kSp1 = CombatNames::modelCondition("SPECIAL_POWER_1");
		for (int f = 0; f < 30; ++f)
		{
			game.logic().runLogicFrame();
			sawSp1 = sawSp1 || hero->testModelCondition(kSp1);
			if (const auto *emo = findTypedModule<EmotionTrackerUpdate>(*foe))
			{
				sawFear = sawFear || emo->requested(EMOTION_UNCONTROLLABLE_FEAR);
			}
			if (!probePassed)
			{
				sweep_probe::EffectProbe after = sweep_probe::EffectProbe::take(game.logic(), hero, sp);
				std::string missing;
				before.compare(after, missing);
				if (missing.empty())
				{
					probePassed = true;
				}
			}
		}
		CHECK_MESSAGE(sawSp1, hornUnits[idx]);
		CHECK_MESSAGE(sawFear, hornUnits[idx]);
		CHECK_MESSAGE(probePassed, hornUnits[idx]);
	}

	// 5. Aragorn Elendil (GondorAragorn_SummonedHunters): GenerateTerror + SPECIAL_POWER_1 pass SweepEffectProbe.
	{
		Object *aragorn = game.game->createObject("GondorAragorn_SummonedHunters", p1, Coord3D{ 1400.0f, 2650.0f, 0.0f }, 0.0f, &err);
		REQUIRE_MESSAGE(aragorn != nullptr, err);
		Object *foe = game.game->createObject("GondorFighterHorde", p2, Coord3D{ 1460.0f, 2650.0f, 0.0f }, 0.0f, &err);
		REQUIRE_MESSAGE(foe != nullptr, err);
		const SpecialPowerTemplate *sp = TheSpecialPowerStore->findSpecialPowerTemplate("SpecialAbilityAragornElendil");
		REQUIRE(sp != nullptr);
		sweep_probe::EffectProbe before = sweep_probe::EffectProbe::take(game.logic(), aragorn, sp);
		unpauseAndFire(aragorn, "SpecialAbilityAragornElendil");
		bool probePassed = false;
		for (int f = 0; f < 40; ++f)
		{
			game.logic().runLogicFrame();
			sweep_probe::EffectProbe after = sweep_probe::EffectProbe::take(game.logic(), aragorn, sp);
			std::string missing;
			before.compare(after, missing);
			if (missing.empty())
			{
				probePassed = true;
				break;
			}
		}
		CHECK(probePassed);
	}

	// 6. Boromir shield on GondorBoromir_mod_ElrondsRat: BoromirSchild is absent, HERO + WEAPONSET_HERO_MODE pass SweepEffectProbe.
	{
		Object *boromir = game.game->createObject("GondorBoromir_mod_ElrondsRat", p1, Coord3D{ 1800.0f, 2650.0f, 0.0f }, 0.0f, &err);
		REQUIRE_MESSAGE(boromir != nullptr, err);
		const SpecialPowerTemplate *sp = TheSpecialPowerStore->findSpecialPowerTemplate("SpecialAbilityAragornBladeMaster");
		REQUIRE(sp != nullptr);
		sweep_probe::EffectProbe before = sweep_probe::EffectProbe::take(game.logic(), boromir, sp);
		unpauseAndFire(boromir, "SpecialAbilityAragornBladeMaster");
		for (int f = 0; f < 10; ++f)
		{
			game.logic().runLogicFrame();
		}
		sweep_probe::EffectProbe after = sweep_probe::EffectProbe::take(game.logic(), boromir, sp);
		std::string missing;
		const std::string notes = before.compare(after, missing);
		CHECK_MESSAGE(missing.empty(), "missing=" << missing << " notes=" << notes);
	}

	// 7. DorwinionNaturmagier harvest blessing: NaturmagierErntesegenModifier lands on allied ElvenMallornTree, SweepEffectProbe passes.
	{
		Object *mage = game.game->createObject("DorwinionNaturmagier", p1, Coord3D{ 2200.0f, 2650.0f, 0.0f }, 0.0f, &err);
		REQUIRE_MESSAGE(mage != nullptr, err);
		Object *tree = game.game->createObject("ElvenMallornTree", p1, Coord3D{ 2260.0f, 2650.0f, 0.0f }, 0.0f, &err);
		REQUIRE_MESSAGE(tree != nullptr, err);
		const SpecialPowerTemplate *sp = TheSpecialPowerStore->findSpecialPowerTemplate("SpecialAbilityNaturmagierErntesegen");
		REQUIRE(sp != nullptr);
		sweep_probe::EffectProbe before = sweep_probe::EffectProbe::take(game.logic(), mage, sp);
		unpauseAndFire(mage, "SpecialAbilityNaturmagierErntesegen");
		for (int f = 0; f < 15; ++f)
		{
			game.logic().runLogicFrame();
		}
		sweep_probe::EffectProbe after = sweep_probe::EffectProbe::take(game.logic(), mage, sp);
		std::string missing;
		const std::string notes = before.compare(after, missing);
		CHECK_MESSAGE(missing.empty(), "missing=" << missing << " notes=" << notes);
		const auto *pool = static_cast<const AttributeModifierPool *>(tree->findModule("AttributeModifierPoolUpdate"));
		REQUIRE(pool != nullptr);
		CHECK(pool->hasList("NaturmagierErntesegenModifier"));
	}
}
