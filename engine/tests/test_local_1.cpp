// OpenBFME unit tests: LOCAL-1 GameData fields RotWK logic reads (issue #88).
// Step 1: AttributeModifierArmorMaxBonus (S-322, S-152)
//
// RotWK addresses:
//   Row RW 0xC00490 (name RW 0xC02898), parser RW 0x42EEFA (parsePercentToReal), GlobalData + 0xAE8 (float).
//   Constructor RW 0x643079: default in binary is 1.0f, retail 2.01 gamedata.ini and Ennorath set 75% (0.75f).
//   Combat logic reader: RW 0x5D893C (AdjustDamage), asm RW 0x5D8B02..0x5D8B3D:
//     Loads TheGameData (0xDE4364) + 0xAE8 (AttributeModifierArmorMaxBonus) into xmm0,
//     compares with modifier sum at ebp+0x10 (comiss xmm0, [ebp+0x10]; jbe 0x5D8B19),
//     and caps active ARMOR attribute modifiers at AttributeModifierArmorMaxBonus.
//   ActiveBody::DamageHost (RW 0x5D893C host): armorMaxBonus() supplies GameLogicSettings::attributeModifierArmorMaxBonus.

#include "doctest.h"

#include "Common/ArchiveFileSystem.h"
#include "Common/BfmeGame.h"
#include "Common/INI.h"
#include "GameLogic/AttributeModifiers.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Damage.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/ActiveBody.h"
#include "GameLogic/Module/LogicModules.h"
#include "GameLogic/Object/Object.h"
#include "LogicTestUtil.h"
#include "CombatTestUtil.h"
#include "GameLogic/Combat/WeaponDelivery.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/WeaponStores.h"

#include <cstdlib>

namespace L = GameLogicSettingsLoader;

namespace
{
const char kBaseGameData[] =
	"GameData\n"
	"  ForceModelsToFollowTimeOfDay = Yes\n"
	"  ForceModelsToFollowWeather = No\n"
	"  UnitDamagedThreshold = 0.5\n"
	"  UnitReallyDamagedThreshold = 0.25\n"
	"  DefaultStartingCash = 700\n"
	"End\n";

const char kArmorTestObjects[] =
	"ModifierList TestHeavyArmor85\n"
	"  Category = SPELL\n"
	"  Modifier = ARMOR 85%\n"
	"  Duration = 0\n"
	"End\n"
	"Object ArmorVictim\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 1000\n"
	"    InitialHealth = 1000\n"
	"    UseDefaultDamageSettings = Yes\n"
	"  End\n"
	"  Behavior = AttributeModifierPoolUpdate ModuleTag_Pool\n"
	"  End\n"
	"End\n";
} // namespace

TEST_CASE("LOCAL-1 armor max bonus: scanGameData parses AttributeModifierArmorMaxBonus into settings and clears from unapplied (S-152)")
{
	GameLogicSettings defSettings;
	std::string err;
	REQUIRE_MESSAGE(L::scanGameData(kBaseGameData, defSettings, &err), err);
	CHECK(defSettings.attributeModifierArmorMaxBonus == 0.75f);
	CHECK(defSettings.unappliedFields.count("GameData.AttributeModifierArmorMaxBonus") == 0);

	// Synthetic 50% cap
	GameLogicSettings s50;
	const std::string ini50 = std::string(kBaseGameData) +
		"GameData\n"
		"  AttributeModifierArmorMaxBonus = 50%\n"
		"End\n";
	REQUIRE_MESSAGE(L::scanGameData(ini50, s50, &err), err);
	CHECK(s50.attributeModifierArmorMaxBonus == 0.50f);
	CHECK(s50.unappliedFields.count("GameData.AttributeModifierArmorMaxBonus") == 0);

	// Synthetic 90% cap
	GameLogicSettings s90;
	const std::string ini90 = std::string(kBaseGameData) +
		"GameData\n"
		"  AttributeModifierArmorMaxBonus = 90%\n"
		"End\n";
	REQUIRE_MESSAGE(L::scanGameData(ini90, s90, &err), err);
	CHECK(s90.attributeModifierArmorMaxBonus == 0.90f);
	CHECK(s90.unappliedFields.count("GameData.AttributeModifierArmorMaxBonus") == 0);
}

TEST_CASE("LOCAL-1 armor max bonus: AdjustDamage caps ARMOR modifiers at settings AttributeModifierArmorMaxBonus (S-322)")
{
	logictest::LogicWorld lw;
	REQUIRE(lw.loadError.empty());
	LogicModules::registerAll(lw.w.modules);
	AttributeModifierStore modifierStore;
	AttributeModifierStore *prevModStore = TheAttributeModifierStore;
	TheAttributeModifierStore = &modifierStore;
	modifierStore.registerBlock(lw.w.fx.env.blocks);
	const std::string err = lw.w.load(kArmorTestObjects);
	REQUIRE_MESSAGE(err.empty(), err);
	lw.logic->settings().bodyThresholdsLoaded = true;
	lw.logic->settings().unitDamagedThreshold = 0.5f;
	lw.logic->settings().unitReallyDamagedThreshold = 0.25f;

	const ThingTemplate *tt = lw.w.get("ArmorVictim");
	REQUIRE(tt != nullptr);

	Object *victim = lw.logic->newObject(tt, nullptr, ObjectStatusMaskType{});
	REQUIRE(victim != nullptr);
	auto *body = dynamic_cast<ActiveBody *>(victim->getBodyModule());
	REQUIRE(body != nullptr);
	lw.logic->update(1);

	const ModifierListTemplate *direct = modifierStore.find("TestHeavyArmor85");
	REQUIRE(direct != nullptr);
	float dv = 0.0f;
	CHECK(direct->value(ATTRIBUTE_ARMOR, "SLASH", dv));
	CHECK(dv == doctest::Approx(0.85f)); // tolerance: float division in INI percent parser (85 / 100.0f)
	CHECK(direct->mayHave(ATTRIBUTE_ARMOR));

	REQUIRE(victim->addAttributeModifier("TestHeavyArmor85", 0));
	float sum = 0.0f;
	CHECK(victim->attributeModifierSum(ATTRIBUTE_MODIFIER_ARMOR, TheDamageNames[DAMAGE_SLASH], sum));
	CHECK(sum == doctest::Approx(0.85f)); // tolerance: float division in INI percent parser (85 / 100.0f)

	DamageInfoInput in;
	in.m_damageType = DAMAGE_SLASH;
	in.m_amount = 100.0f;
	in.m_sourceID = 0;

	// Case 1: Cap = 50% (0.50f). 85% modifier is clamped to 50%.
	// Damage dealt = 100 * (1 - 0.50) = 50.
	// On old code (hardcoded 0.75f), damage would be 100 * (1 - 0.75) = 25.
	lw.logic->settings().attributeModifierArmorMaxBonus = 0.50f;
	float dmg50 = body->estimateDamage(in);
	CHECK(dmg50 == 50.0f);

	// Case 2: Cap = 90% (0.90f). 85% modifier is NOT clamped.
	// Damage dealt = 100 * (1 - 0.85) = 15.
	// On old code (hardcoded 0.75f), damage would be 25.
	lw.logic->settings().attributeModifierArmorMaxBonus = 0.90f;
	float dmg90 = body->estimateDamage(in);
	CHECK(dmg90 == doctest::Approx(15.0f)); // tolerance: float division in INI percent parser (100 * (1 - 0.85))

	// Case 3: Cap = 75% (0.75f, retail standard). 85% modifier is clamped to 75%.
	// Damage dealt = 100 * (1 - 0.75) = 25.
	lw.logic->settings().attributeModifierArmorMaxBonus = 0.75f;
	float dmg75 = body->estimateDamage(in);
	CHECK(dmg75 == 25.0f);

	// Also verify attemptDamage deals exactly the same amount
	DamageInfo hit;
	hit.m_input = in;
	body->attemptDamage(hit);
	CHECK(hit.m_output.m_actualDamageDealt == 25.0f);

	TheAttributeModifierStore = prevModStore;
}

TEST_CASE("LOCAL-1 splash minimum: scanGameData parses DamageRadiusMinimumForSplash into settings and clears from unapplied (S-152)")
{
	GameLogicSettings defSettings;
	std::string err;
	REQUIRE_MESSAGE(L::scanGameData(kBaseGameData, defSettings, &err), err);
	CHECK(defSettings.damageRadiusMinimumForSplash == 4.0f);
	CHECK(defSettings.unappliedFields.count("GameData.DamageRadiusMinimumForSplash") == 0);

	GameLogicSettings s15;
	const std::string ini15 = std::string(kBaseGameData) +
		"GameData\n"
		"  DamageRadiusMinimumForSplash = 15.0\n"
		"End\n";
	REQUIRE_MESSAGE(L::scanGameData(ini15, s15, &err), err);
	CHECK(s15.damageRadiusMinimumForSplash == 15.0f);
	CHECK(s15.unappliedFields.count("GameData.DamageRadiusMinimumForSplash") == 0);
}

TEST_CASE("LOCAL-1 splash minimum: WeaponDelivery radiusDamage respects damageRadiusMinimumForSplash (RW 0x90E24F)")
{
	const char kSplashWeapon[] =
		"Weapon Splash10\n"
		"  AttackRange = 50.0\n"
		"  DamageNugget\n"
		"    Damage = 20\n"
		"    Radius = 10.0\n"
		"    DamageType = SLASH\n"
		"    DeathType = NORMAL\n"
		"  End\n"
		"End\n";

	// Case 1: Default threshold 4.0 < radius 10.0 -> splash is active, both victims take damage
	{
		combattest::CombatWorld w(kSplashWeapon);
		Object *attacker = w.unit("Swordsman", 'A', 280.0f, 300.0f);
		Object *v1 = w.unit("Dummy", 'B', 300.0f, 300.0f);
		Object *v2 = w.unit("Dummy", 'B', 305.0f, 300.0f);
		w.frames(1);
		const WeaponTemplate *splash = TheWeaponStore->findWeaponTemplate("Splash10");
		REQUIRE(splash != nullptr);

		w.logic->settings().damageRadiusMinimumForSplash = 4.0f;
		DeliverNuggets(*w.logic, attacker->getID(), *splash, WeaponBonus{}, v1, v1->getPosition(), false, nullptr);
		CHECK(w.health(v1) == 90.0f);
		CHECK(w.health(v2) == 90.0f);
	}

	// Case 2: Threshold 15.0 >= radius 10.0 -> splash is suppressed after first victim (RW 0x90E24F breaks)
	{
		combattest::CombatWorld w(kSplashWeapon);
		Object *attacker = w.unit("Swordsman", 'A', 280.0f, 300.0f);
		Object *v1 = w.unit("Dummy", 'B', 300.0f, 300.0f);
		Object *v2 = w.unit("Dummy", 'B', 305.0f, 300.0f);
		w.frames(1);
		const WeaponTemplate *splash = TheWeaponStore->findWeaponTemplate("Splash10");
		REQUIRE(splash != nullptr);

		w.logic->settings().damageRadiusMinimumForSplash = 15.0f;
		DeliverNuggets(*w.logic, attacker->getID(), *splash, WeaponBonus{}, v1, v1->getPosition(), false, nullptr);
		CHECK(w.health(v1) == 90.0f);
		CHECK(w.health(v2) == 100.0f);
	}
}
