// OpenBFME. GPL-3.0.
// LOCAL-115 (fork issue 799): shape-change abilities — ToggleMounted, ModelCondition, HeroMode special abilities.
// Editions: both RotWK 2.01 and BFME2 1.06.
// Sources:
//   - ToggleMountedSpecialAbilityUpdate::xfer: RW 0x008B1054 (version window (1, 2), m_replaceAfterPacking gated on v >= 2)
//   - ToggleMountedSpecialAbilityUpdate utility slots: RW 0x008B104E (getModuleName), RW 0x008B1097 (getModuleNameKey),
//     RW 0x008B1126 -> RW 0x008B1024 (dtor)
//   - Weapon::getAttackRange(source, dz) vs getAttackRange(source): RW 0x006CA69A -> RW 0x006CA61F (single RANGE scale)
//     vs RW 0x006CA935 -> RW 0x006CA701 (applies WeaponRangeScale twice)
//   - SpecialPowerModule::doSpecialPower empty AttributeModifierAffects: RW 0x0089693A / RW 0x00897BAB
//   - HeroModeSpecialAbilityUpdate: RW 0x008958BD (startUnpacking), RW 0x008959B5 (triggerAbilityEffect), RW 0x0068F1A8
//   - ModelConditionSpecialAbilityUpdate: RW 0x0089455B (startUnpacking), RW 0x0089463A (triggerAbilityEffect)
//   - Ennorath INI: data/ini/includes/attributemodifier.inc (LegolasElbenauge, BoromirSchildModifier),
//     data/ini/object/goodfaction/units/lothlorien/lothlorienlegolas.ini,
//     data/ini/object/goodfaction/units/gondor/gondoraragorn.ini,
//     data/ini/object/goodfaction/units/gondor/gondorboromir.ini

#include "doctest.h"

#include "CombatTestUtil.h"
#include "EnnorathScenario.h"
#include "HudTestUtil.h"
#include "SweepEffectProbe.h"

#include "Common/Player.h"
#include "Common/SpecialPower.h"
#include "Common/StateHash.h"
#include "Common/XferLoad.h"
#include "Common/XferSave.h"
#include "GameClient/FXList.h"
#include "GameLogic/AttributeModifiers.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Combat/ObjectWeapons.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/ActiveBody.h"
#include "GameLogic/Module/EmotionModules.h"
#include "GameLogic/Module/SpecialAbilityModules.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Object/AttributeModifierPool.h"
#include "GameLogic/Object/Object.h"

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace
{
const char kToggleMountedXferIni[] =
	"SpecialPower SpecialAbilityLocal115Mount\n"
	"  Enum = SPECIAL_TOGGLE_MOUNTED\n"
	"  ReloadTime = 1000\n"
	"End\n"
	"Object Local115MountedForm\n"
	"  KindOf = SELECTABLE HERO\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"End\n"
	"Object Local115Rider\n"
	"  KindOf = SELECTABLE HERO\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = SpecialPowerModule ModuleTag_MountPower\n"
	"    SpecialPowerTemplate = SpecialAbilityLocal115Mount\n"
	"    UpdateModuleStartsAttack = Yes\n"
	"  End\n"
	"  Behavior = ToggleMountedSpecialAbilityUpdate ModuleTag_MountUpdate\n"
	"    SpecialPowerTemplate = SpecialAbilityLocal115Mount\n"
	"    MountedTemplate = Local115MountedForm\n"
	"    UnpackTime = 100\n"
	"    PreparationTime = 0\n"
	"    PackTime = 200\n"
	"    OpacityTarget = 0.3\n"
	"  End\n"
	"End\n";

const char kHeroModeRangeIni[] =
	"ModifierList Local115EyeMod\n"
	"  Category = SPELL\n"
	"  Modifier = RANGE +15%\n"
	"  Duration = 30000\n"
	"End\n"
	"Weapon Local115Bow\n"
	"  AttackRange = 400.0\n"
	"  DelayBetweenShots = 1000\n"
	"  PreAttackDelay = 100\n"
	"  FireFX = None\n"
	"  DamageNugget\n"
	"    Damage = 10\n"
	"    DamageType = PIERCE\n"
	"  End\n"
	"End\n"
	"SpecialPower SpecialAbilityLocal115Eye\n"
	"  Enum = SPECIAL_HERO_MODE\n"
	"  ReloadTime = 10000\n"
	"End\n"
	"Object Local115Archer\n"
	"  KindOf = SELECTABLE HERO\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY Local115Bow\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = AttributeModifierPoolUpdate ModuleTag_Pool\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = SpecialPowerModule ModuleTag_EyePower\n"
	"    SpecialPowerTemplate = SpecialAbilityLocal115Eye\n"
	"    UpdateModuleStartsAttack = Yes\n"
	"  End\n"
	"  Behavior = HeroModeSpecialAbilityUpdate ModuleTag_EyeUpdate\n"
	"    SpecialPowerTemplate = SpecialAbilityLocal115Eye\n"
	"    HeroAttributeModifier = Local115EyeMod\n"
	"    HeroEffectDuration = 30000\n"
	"    UnpackTime = 1\n"
	"  End\n"
	"End\n";

struct Local115World : combattest::CombatWorld
{
	SpecialPowerStore powers;
	SpecialPowerStore *savedPowers = TheSpecialPowerStore;
	FXListStore fxLists;
	FXListStore *savedFx = TheFXListStore;
	AttributeModifierStore modifiers;
	AttributeModifierStore *savedModifiers = TheAttributeModifierStore;
	explicit Local115World(const char *extra)
	{
		TheSpecialPowerStore = &powers;
		TheFXListStore = &fxLists;
		TheAttributeModifierStore = &modifiers;
		modifiers.registerBlock(w.fx.env.blocks);
		w.fx.env.blocks.registerBlock("FXList", [](INI *ini) { ParseFXListDefinitionGlobal(ini); });
		w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
		SpecialAbilityModules::registerAll(w.modules);
		SpecialPowerModules::registerAll(w.modules);
		const std::string err = w.load(extra);
		REQUIRE_MESSAGE(err.empty(), err);
	}
	~Local115World()
	{
		TheSpecialPowerStore = savedPowers;
		TheFXListStore = savedFx;
		TheAttributeModifierStore = savedModifiers;
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
} // namespace

// RW 0x008B1054: ToggleMountedSpecialAbilityUpdate::xfer writes version 2 (window 1..2), delegates to
// SpecialAbilityUpdate::xfer (RW 0x00852782), and only transfers m_replaceAfterPacking (+0x8C) when version >= 2.
TEST_CASE("LOCAL-115 ToggleMountedSpecialAbilityUpdate::xfer uses version window (1, 2) and gates m_replaceAfterPacking (RW 0x008B1054)")
{
	Local115World w(kToggleMountedXferIni);
	Object *rider = w.unit("Local115Rider", 'A', 100.0f, 100.0f);
	REQUIRE(rider != nullptr);
	auto *tm = findTypedModule<ToggleMountedSpecialAbilityUpdate>(*rider);
	REQUIRE(tm != nullptr);

	// 1. Save writes version 2 (RW 0x008B106B: mov byte ptr [ebp-3], 2)
	XferSave savedV2;
	tm->xfer(savedV2);
	const std::vector<std::uint8_t> &bytesV2 = savedV2.data();
	REQUIRE_FALSE(bytesV2.empty());
	CHECK(bytesV2[0] == 2);

	// 2. Round-trip v2 preserves state
	StateHasher hBefore;
	tm->crc(hBefore);
	XferLoad loadV2(bytesV2);
	tm->xfer(loadV2);
	CHECK(loadV2.atEnd());
	StateHasher hAfterV2;
	tm->crc(hAfterV2);
	CHECK(hAfterV2.value() == hBefore.value());

	// 3. Version 1 stream (RW 0x008B107A: cmp ver, 2; jb skip +0x8C) loads cleanly without the bool byte
	XferSave baseOnly;
	tm->SpecialAbilityUpdate::xfer(baseOnly);
	std::vector<std::uint8_t> bytesV1;
	bytesV1.push_back(1); // version 1
	bytesV1.insert(bytesV1.end(), baseOnly.data().begin(), baseOnly.data().end());
	// port tail: m_toggles (u32) + m_replacement (u32)
	for (int i = 0; i < 8; ++i)
	{
		bytesV1.push_back(0);
	}
	XferLoad loadV1(bytesV1);
	tm->xfer(loadV1);
	CHECK(loadV1.atEnd());
	CHECK(bytesV2.size() == bytesV1.size() + 1);
}

// RW 0x006CA69A -> RW 0x006CA61F vs RW 0x006CA935 -> RW 0x006CA701:
// Weapon::getAttackRange(source, dz) applies RANGE (1 + sum) once; NoTarget squares it.
// SweepEffectProbe::seen.attackRange must read currentAttackRange() (single scale) to match its RW 0x006CA61F formula.
TEST_CASE("LOCAL-115 SweepEffectProbe attackRange uses single RANGE scale (RW 0x006CA69A / RW 0x006CA61F)")
{
	Local115World w(kHeroModeRangeIni);
	Object *archer = w.unit("Local115Archer", 'A', 100.0f, 100.0f);
	REQUIRE(archer != nullptr);
	const SpecialPowerTemplate *sp = TheSpecialPowerStore ? TheSpecialPowerStore->findSpecialPowerTemplate("SpecialAbilityLocal115Eye") : nullptr;
	REQUIRE(sp != nullptr);
	ObjectWeapons *ow = archer->getWeapons();
	REQUIRE(ow != nullptr);

	// RW 0x006C9F5B: AttackRange 400.0 minus RangePadding 2.5 = 397.5
	const float baseRange = 397.5f;
	CHECK(ow->currentAttackRange() == baseRange);
	CHECK(ow->currentAttackRangeNoTarget() == baseRange);

	sweep_probe::EffectProbe before = sweep_probe::EffectProbe::take(*w.logic, archer, sp);
	REQUIRE(before.seen.count(archer->getID()) == 1);
	CHECK(before.seen.at(archer->getID()).attackRange == baseRange);

	auto *spm = findTypedModule<SpecialPowerModule>(*archer);
	REQUIRE(spm != nullptr);
	spm->doSpecialPower(0);
	w.frames(10);

	// Single scale: 397.5 * 1.15 = 457.125 (RW 0x006CA69A -> RW 0x006CA61F)
	const float singleScaled = ow->currentAttackRange();
	CHECK(singleScaled == doctest::Approx(457.125f).epsilon(1e-4)); // tolerance: x87/PC24 float rounding on 397.5 * 1.15
	// Squared scale: 397.5 * 1.15 * 1.15 = 525.69375 (RW 0x006CA935 -> RW 0x006CA701)
	CHECK(ow->currentAttackRangeNoTarget() == doctest::Approx(525.69375f).epsilon(1e-4)); // tolerance: x87/PC24 float rounding on squared scale

	sweep_probe::EffectProbe after = sweep_probe::EffectProbe::take(*w.logic, archer, sp);
	REQUIRE(after.seen.count(archer->getID()) == 1);
	CHECK(after.seen.at(archer->getID()).attackRange == singleScaled);

	std::string missing;
	const std::string notes = before.compare(after, missing);
	CHECK_MESSAGE(missing.empty(), "missing=" << missing << " notes=" << notes);
	CHECK(missing.find("NO-VALUE") == std::string::npos);
}

// Ennorath INI sources:
// - LothlorienLegolas (lothlorienlegolas.ini) + LegolasElbenauge (attributemodifier.inc, RANGE +15%)
// - GondorAragorn_SummonedHunters (gondoraragorn.ini) + SpecialAbilityAragornElendil (ModelConditionSpecialAbilityUpdate GenerateTerror=Yes,
//   SpecialPowerModule AttributeModifier=AragornElendilAntiCategory with no AttributeModifierAffects -> RW 0x0089693A / RW 0x00897BAB)
// - GondorBoromir_mod_ElrondsRat vs GondorBoromir_mod (gondorboromir.ini): HeroAttributeModifier BoromirSchild (absent) vs BoromirSchildModifier (present)
TEST_CASE("LOCAL-115 ENNORATH scenario: Legolas' elven eye, Aragorn's Elendil, Boromir's brave resistance")
{
	ennorathscenario::World world;
	if (!world.load("LOCAL-115 Ennorath scenario"))
	{
		return;
	}

	hudtest::Rig game(world.shared, "map mp fall back 8p", "FactionMen");
	const int p1 = game.index();
	Player *enemyPlayer = game.game->players().findPlayerWithName("Player_2");
	REQUIRE(enemyPlayer != nullptr);
	const int p2 = enemyPlayer->getPlayerIndex();
	std::string err;

	// 1. Legolas' elven eye (LothlorienLegolas): RANGE +15% matches single-scale RW 0x006CA69A -> 0x006CA61F without NO-VALUE.
	{
		Object *legolas = game.game->createObject("LothlorienLegolas", p1, Coord3D{ 1700.0f, 2300.0f, 0.0f }, 0.0f, &err);
		REQUIRE_MESSAGE(legolas != nullptr, err);
		const SpecialPowerTemplate *sp = TheSpecialPowerStore->findSpecialPowerTemplate("SpecialAbilityAragornBladeMaster");
		REQUIRE(sp != nullptr);
		auto *spm = dynamic_cast<SpecialPowerModule *>(SpecialPowerModules::findModule(*legolas, sp));
		REQUIRE(spm != nullptr);
		while (spm->pauseCount() > 0)
		{
			spm->pauseCountdown(false);
		}
		const float rangeBefore = legolas->getWeapons()->currentAttackRange();
		REQUIRE(rangeBefore > 0.0f); // tolerance: Ennorath LegolasBow base range is mod-data-defined
		sweep_probe::EffectProbe before = sweep_probe::EffectProbe::take(game.logic(), legolas, sp);
		spm->doSpecialPower(0);
		for (int i = 0; i < 30; ++i)
		{
			game.logic().runLogicFrame();
		}
		const float rangeAfter = legolas->getWeapons()->currentAttackRange();
		CHECK(rangeAfter > rangeBefore); // tolerance: Ennorath LegolasElbenauge RANGE +15% increases range; exact ratio checked by EffectProbe::compare
		sweep_probe::EffectProbe after = sweep_probe::EffectProbe::take(game.logic(), legolas, sp);
		std::string missing;
		const std::string notes = before.compare(after, missing);
		CHECK_MESSAGE(missing.empty(), "notes=" << notes << " missing=" << missing);
	}

	// 2. Aragorn's Elendil (GondorAragorn_SummonedHunters): ModelConditionSpecialAbilityUpdate fires GenerateTerror (RW 0x0089463A)
	//    and SPECIAL_POWER_1 (RW 0x0089455B); SpecialPowerModule applies no AragornElendilAntiCategory because AttributeModifierAffects
	//    is omitted (RW 0x0089693A / RW 0x00897BAB).
	{
		Object *aragorn = game.game->createObject("GondorAragorn_SummonedHunters", p1, Coord3D{ 2100.0f, 2300.0f, 0.0f }, 0.0f, &err);
		REQUIRE_MESSAGE(aragorn != nullptr, err);
		Object *foe = game.game->createObject("GondorFighterHorde", p2, Coord3D{ 2160.0f, 2300.0f, 0.0f }, 0.0f, &err);
		REQUIRE_MESSAGE(foe != nullptr, err);
		const SpecialPowerTemplate *sp = TheSpecialPowerStore->findSpecialPowerTemplate("SpecialAbilityAragornElendil");
		REQUIRE(sp != nullptr);
		auto *spm = dynamic_cast<SpecialPowerModule *>(SpecialPowerModules::findModule(*aragorn, sp));
		REQUIRE(spm != nullptr);
		while (spm->pauseCount() > 0)
		{
			spm->pauseCountdown(false);
		}
		CHECK(spm->spData()->m_attributeModifier == "AragornElendilAntiCategory");
		CHECK(spm->spData()->m_attributeModifierAffects == nullptr);
		CHECK_FALSE(spm->spData()->m_attributeModifierAffectsSelf);
		auto *mc = findTypedModule<ModelConditionSpecialAbilityUpdate>(*aragorn);
		REQUIRE(mc != nullptr);

		spm->doSpecialPower(0);
		bool sawSp1 = false;
		bool sawTerror = false;
		const int kSp1 = CombatNames::modelCondition("SPECIAL_POWER_1");
		for (int i = 0; i < 60; ++i)
		{
			game.logic().runLogicFrame();
			if (aragorn->testModelCondition(kSp1))
			{
				sawSp1 = true;
			}
			if (const auto *emo = findTypedModule<EmotionTrackerUpdate>(*foe))
			{
				if (emo->requested(EMOTION_TERROR))
				{
					sawTerror = true;
				}
			}
		}
		CHECK(sawSp1);
		CHECK(sawTerror);
		if (const auto *pool = static_cast<const AttributeModifierPool *>(foe->findModule("AttributeModifierPoolUpdate")))
		{
			CHECK_FALSE(pool->hasList("AragornElendilAntiCategory"));
		}
	}

	// 3. Boromir's brave resistance: GondorBoromir_mod_ElrondsRat names absent BoromirSchild (RW 0x0068F1A8 adds nothing,
	//    still sets WEAPONSET_HERO_MODE RW 0x008958BD and HERO RW 0x008959B5), while GondorBoromir_mod names BoromirSchildModifier.
	{
		CHECK(TheAttributeModifierStore->find("BoromirSchild") == nullptr);
		CHECK(TheAttributeModifierStore->find("BoromirSchildModifier") != nullptr);

		Object *boromirRat = game.game->createObject("GondorBoromir_mod_ElrondsRat", p1, Coord3D{ 2500.0f, 2300.0f, 0.0f }, 0.0f, &err);
		REQUIRE_MESSAGE(boromirRat != nullptr, err);
		const SpecialPowerTemplate *sp = TheSpecialPowerStore->findSpecialPowerTemplate("SpecialAbilityAragornBladeMaster");
		REQUIRE(sp != nullptr);
		auto *spm = dynamic_cast<SpecialPowerModule *>(SpecialPowerModules::findModule(*boromirRat, sp));
		REQUIRE(spm != nullptr);
		while (spm->pauseCount() > 0)
		{
			spm->pauseCountdown(false);
		}
		spm->doSpecialPower(0);
		for (int i = 0; i < 20; ++i)
		{
			game.logic().runLogicFrame();
		}
		CHECK(((boromirRat->getWeaponSetFlags()[0] >> 0x1B) & 1u) == 1u); // WEAPONSET_HERO_MODE (RW 0x008958CD: push 0x1b)
		CHECK(boromirRat->testModelCondition(CombatNames::modelCondition("HERO")));
		if (const auto *pool = static_cast<const AttributeModifierPool *>(boromirRat->findModule("AttributeModifierPoolUpdate")))
		{
			CHECK_FALSE(pool->hasList("BoromirSchild"));
		}
	}
}
