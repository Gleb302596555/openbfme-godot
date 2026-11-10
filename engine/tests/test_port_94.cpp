// OpenBFME. PORT-94 (issue #242) verdict: RotWK 2.01 parses SalvageCrateCollide's LevelUpRadius, ResourceChance
// and BannerChance and no reachable code reads them. Full proof with addresses in docs/ennorath/port94-salvage-crate-rw.md
// (stop S-7220). These tests pin the RW 2.01 behaviour of a picked-up crate:
//   * the roll order: exactly one real roll per crate (RW 0x8C136C pickOutcome, line 0x45 of SalvageCrateCollide.cpp);
//     roll < LevelUpChance -> outcome 2, else outcome 4 (Upgrade set) or 3 (money). Never 0, 1 or negative;
//   * outcome 2 levels the PICKING unit only (Object+0x26C -> gainExpForLevel(1, true, false), RW 0x79DA0A);
//   * outcome 3 deposits [MinResource..MaxResource] (RW 0x8C148B);
//   * the LevelUpRadius branch (RW 0x8C168D) is unreachable, so units in range never level for the crate's sake;
//   * BannerChance is never consulted anywhere.
// Synthetic INI only, no game files. Expectations are RotWK's rules, not the port's (docs/ennorath/PROCESS.md).

#include "doctest.h"
#include "CombatTestUtil.h"
#include "IniTestUtil.h"

#include "Common/Player.h"
#include "GameLogic/Economy.h"
#include "GameLogic/Module/MoneyEventModules.h"
#include "GameLogic/Module/SalvageCrateCollide.h"
#include "GameLogic/Object/ExperienceTracker.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/ExperienceLevels.h"

#include <string>
#include <vector>

using namespace combattest;

namespace
{

template <class T>
T *moduleOf(Object *o)
{
	for (const auto &m : o->modules())
	{
		if (T *t = dynamic_cast<T *>(m.get()))
		{
			return t;
		}
	}
	return nullptr;
}

// an ExperienceLevel chain for SalvPicker, so gainExpForLevel (RW 0x79DA0A) has a next level to grant (RW 0x68A274) and a fresh object is at rank 0:
// the first level of the chain is applied unconditionally at spawn (RW 0x693F1B .. 0x693F2F), the tracker's reset grants the same (RW 0x79DA66 -> gainLevel)
const char kSalvLevels[] =
	"ExperienceLevel SalvPickerLevel0\n"
	"  TargetNames = SalvPicker\n"
	"  RequiredExperience = 1\n"
	"  ExperienceAward = 0\n"
	"  Rank = 0\n"
	"End\n"
	"ExperienceLevel SalvPickerLevel1\n"
	"  TargetNames = SalvPicker\n"
	"  RequiredExperience = 2\n"
	"  ExperienceAward = 10\n"
	"  Rank = 1\n"
	"End\n";

struct LevelScope
{
	initest::Fixture fx;
	ExperienceLevelSystem levels;
	ExperienceLevelSystem *prev;
	LevelScope()
		: prev(TheExperienceLevelSystem)
	{
		levels.registerBlocks(fx.env.blocks);
		TheExperienceLevelSystem = &levels;
		REQUIRE(initest::loadError(fx.env, "levels.ini", kSalvLevels).empty());
	}
	~LevelScope()
	{
		TheExperienceLevelSystem = prev;
	}
};

const char kSalvageObjects[] =
	"Object SalvPicker\n"
	"  KindOf = INFANTRY SELECTABLE SCORE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = NoArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = DestroyDie ModuleTag_Destroy\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n"
	// the mod's own numbers (Ennorath crate.ini): LevelUpChance 100%, ResourceChance 20%, BannerChance 10%, LevelUpRadius 100.0, Min 25 Max 75
	"Object SalvChestMod\n"
	"  KindOf = CRATE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryMinorRadius = 10\n"
	"  GeometryHeight = 10\n"
	"  Behavior = SalvageCrateCollide ModuleTag_Salvage\n"
	"    ForbiddenKindOf = PROJECTILE\n"
	"    BannerChance = 10%\n"
	"    LevelUpChance = 100%\n"
	"    LevelUpRadius = 100.0\n"
	"    ResourceChance = 20%\n"
	"    MinResource = 25\n"
	"    MaxResource = 75\n"
	"    AllowAIPickup = Yes\n"
	"  End\n"
	"End\n"
	// the mod's other family: LevelUpRadius 0.0
	"Object SalvChestModZero\n"
	"  KindOf = CRATE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryMinorRadius = 10\n"
	"  GeometryHeight = 10\n"
	"  Behavior = SalvageCrateCollide ModuleTag_Salvage\n"
	"    ForbiddenKindOf = PROJECTILE\n"
	"    BannerChance = 0%\n"
	"    LevelUpChance = 100%\n"
	"    LevelUpRadius = 0.0\n"
	"    ResourceChance = 100%\n"
	"    MinResource = 25\n"
	"    MaxResource = 75\n"
	"    AllowAIPickup = Yes\n"
	"  End\n"
	"End\n"
	// LevelUpChance 0%, ResourceChance 100%: RW pays the money anyway (outcome 3)
	"Object SalvChestResource\n"
	"  KindOf = CRATE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryMinorRadius = 10\n"
	"  GeometryHeight = 10\n"
	"  Behavior = SalvageCrateCollide ModuleTag_Salvage\n"
	"    ForbiddenKindOf = PROJECTILE\n"
	"    BannerChance = 0%\n"
	"    LevelUpChance = 0%\n"
	"    LevelUpRadius = 100.0\n"
	"    ResourceChance = 100%\n"
	"    MinResource = 250\n"
	"    MaxResource = 250\n"
	"    AllowAIPickup = Yes\n"
	"  End\n"
	"End\n"
	// LevelUpChance 0%, ResourceChance 0%, BannerChance 100%: RW pays the money anyway (BannerChance is unread)
	"Object SalvChestNoResource\n"
	"  KindOf = CRATE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryMinorRadius = 10\n"
	"  GeometryHeight = 10\n"
	"  Behavior = SalvageCrateCollide ModuleTag_Salvage\n"
	"    ForbiddenKindOf = PROJECTILE\n"
	"    BannerChance = 100%\n"
	"    LevelUpChance = 0%\n"
	"    LevelUpRadius = 100.0\n"
	"    ResourceChance = 0%\n"
	"    MinResource = 250\n"
	"    MaxResource = 250\n"
	"    AllowAIPickup = Yes\n"
	"  End\n"
	"End\n"
	// LevelUpChance 100%, BannerChance 100%: outcome 2 (a level), no money — there is no banner outcome in RW 0x8C158B
	"Object SalvChestBanner\n"
	"  KindOf = CRATE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryMinorRadius = 10\n"
	"  GeometryHeight = 10\n"
	"  Behavior = SalvageCrateCollide ModuleTag_Salvage\n"
	"    ForbiddenKindOf = PROJECTILE\n"
	"    BannerChance = 100%\n"
	"    LevelUpChance = 100%\n"
	"    LevelUpRadius = 100.0\n"
	"    ResourceChance = 20%\n"
	"    MinResource = 250\n"
	"    MaxResource = 250\n"
	"    AllowAIPickup = Yes\n"
	"  End\n"
	"End\n"
	// LevelUpChance 0% with a PLAYER upgrade set: outcome 4, never the LevelUpRadius branch (RW 0x136C returns only 2, 3 or 4);
	// Upgrade_X is the player upgrade of the logic fixture (LogicTestUtil), so the branch grants it
	"Object SalvChestUpgrade\n"
	"  KindOf = CRATE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryMinorRadius = 10\n"
	"  GeometryHeight = 10\n"
	"  Behavior = SalvageCrateCollide ModuleTag_Salvage\n"
	"    ForbiddenKindOf = PROJECTILE\n"
	"    BannerChance = 100%\n"
	"    LevelUpChance = 0%\n"
	"    LevelUpRadius = 100.0\n"
	"    ResourceChance = 0%\n"
	"    Upgrade = Upgrade_X\n"
	"    MinResource = 250\n"
	"    MaxResource = 250\n"
	"    AllowAIPickup = Yes\n"
	"  End\n"
	"End\n"
	// nothing set at all: the RW constructor defaults (RW 0x65592C); Min/Max of the INI are absent so the 100 / 5000 defaults are what is checked
	"Object SalvChestDefault\n"
	"  KindOf = CRATE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryMinorRadius = 10\n"
	"  GeometryHeight = 10\n"
	"  Behavior = SalvageCrateCollide ModuleTag_Salvage\n"
	"    ForbiddenKindOf = PROJECTILE\n"
	"    AllowAIPickup = Yes\n"
	"  End\n"
	"End\n";

} // namespace

TEST_CASE("PORT-94: LevelUpRadius, ResourceChance and BannerChance parse into the module data (RW rows RW 0xC05FB0 / 0xC05FC0 / 0xC05F90)")
{
	LevelScope levels;
	CombatWorld w(kSalvageObjects);
	Object *mod = w.unit("SalvChestMod", 'A', 300, 300);
	Object *modZero = w.unit("SalvChestModZero", 'A', 500, 500);
	Object *def = w.unit("SalvChestDefault", 'A', 700, 700);
	REQUIRE(mod != nullptr);
	REQUIRE(modZero != nullptr);
	REQUIRE(def != nullptr);
	auto *modM = moduleOf<SalvageCrateCollide>(mod);
	auto *modZ = moduleOf<SalvageCrateCollide>(modZero);
	auto *modD = moduleOf<SalvageCrateCollide>(def);
	REQUIRE(modM != nullptr);
	REQUIRE(modZ != nullptr);
	REQUIRE(modD != nullptr);
	const auto *a = dynamic_cast<const SalvageCrateCollideModuleData *>(modM->getModuleData());
	const auto *b = dynamic_cast<const SalvageCrateCollideModuleData *>(modZ->getModuleData());
	const auto *c = dynamic_cast<const SalvageCrateCollideModuleData *>(modD->getModuleData());
	REQUIRE(a != nullptr);
	REQUIRE(b != nullptr);
	REQUIRE(c != nullptr);
	// the mod's values (Ennorath crate.ini): percents parse as / 100 (RW 0x42EEFA = scanPercentToReal)
	CHECK(a->m_bannerChance == doctest::Approx(0.1f));         // tolerance: 10% parses through scanReal x 0.01f under PC24 (RW 0x42EEFA)
	CHECK(a->m_levelUpChance == doctest::Approx(1.0f));        // tolerance: 100% parses through scanReal x 0.01f under PC24 (RW 0x42EEFA)
	CHECK(a->m_levelUpRadius == doctest::Approx(1.0f));        // tolerance: "100.0" through scanReal x 0.01f under PC24 (RW 0x42EEFA)
	CHECK(a->m_resourceChance == doctest::Approx(0.2f));       // tolerance: 20% parses through scanReal x 0.01f under PC24 (RW 0x42EEFA)
	CHECK(a->m_minResource == 25);
	CHECK(a->m_maxResource == 75);
	CHECK(a->m_allowAIPickup == true);
	CHECK(b->m_levelUpRadius == doctest::Approx(0.0f));        // tolerance: 0.0 through the percent parser (exact zero, kept as Approx for symmetry)
	// the module constructor defaults (RW 0x65592C): PorterChance/BannerChance 0.0, LevelUpChance 0.5,
	// LevelUpRadius 100.0 (RW 0x65595C via RW 0xBD88D8), ResourceChance 0.5 (RW 0x655961), Min 100 Max 5000
	CHECK(c->m_porterChance == doctest::Approx(0.0f));          // tolerance: exact zero default from the data constructor (RW 0x65593F)
	CHECK(c->m_bannerChance == doctest::Approx(0.0f));          // tolerance: exact zero default from the data constructor (RW 0x655944)
	CHECK(c->m_levelUpChance == doctest::Approx(0.5f));         // tolerance: 0.5f is exact in binary; kept as Approx against parser rounding
	CHECK(c->m_levelUpRadius == doctest::Approx(100.0f));       // tolerance: raw float 0xBD88D8 == 100.0f, kept as Approx for symmetry
	CHECK(c->m_resourceChance == doctest::Approx(0.5f));        // tolerance: 0.5f is exact in binary; kept as Approx for symmetry
	CHECK(c->m_minResource == 100);
	CHECK(c->m_maxResource == 5000);
}

// the full contact path (AIWorld::processCrateContacts): the crate is destroyed inside a logic frame and the object is
// deleted at the frame's destroy list (processDestroyList), so nothing of the crate's module is touched after the frames —
// the assertions read only the picker, the players and findObjectByID.
TEST_CASE("PORT-94: LevelUpChance = 100% with ResourceChance = 100%: the picker gains a level, no money is given (RW 0x8C136C -> 0x8C1625, RW 0x8C158B)")
{
	LevelScope levels;
	CombatWorld w(kSalvageObjects);
	Object *chest = w.unit("SalvChestModZero", 'A', 300, 300); // LevelUpChance 100%, ResourceChance 100%
	const ObjectID chestId = chest->getID();
	Object *picker = w.unit("SalvPicker", 'A', 305, 300);
	const int before = w.cash('A');
	w.frames(4);
	CHECK(w.byId(chestId) == nullptr); // the crate was used up
	REQUIRE(picker->getExperienceTracker() != nullptr);
	CHECK(picker->getExperienceTracker()->getRank() == 1); // the PICKER gained the level
	CHECK(w.cash('A') == before);                          // the money path never ran
}

// the outcome code and the money are read through a direct executeCrateBehavior call (what CrateCollide::onCollide does
// after the validity checks): the crate stays alive, so the module can be inspected afterwards
TEST_CASE("PORT-94: the outcome codes: LevelUpChance = 100% -> 2 (no money), LevelUpChance = 0% -> 3 (money), LevelUpChance = 0% with Upgrade -> 4")
{
	LevelScope levels;
	SUBCASE("LevelUpChance = 100%, ResourceChance = 100%: outcome 2, lastMoney stays 0")
	{
		CombatWorld w(kSalvageObjects);
		Object *chest = w.unit("SalvChestModZero", 'A', 300, 300);
		Object *picker = w.unit("SalvPicker", 'A', 305, 300);
		const int before = w.cash('A');
		auto *module = moduleOf<SalvageCrateCollide>(chest);
		REQUIRE(module != nullptr);
		CHECK(module->executeCrateBehavior(*picker) == true);
		CHECK(module->lastOutcome() == SalvageCrateCollide::OUTCOME_LEVEL_UP); // RW returns 2
		CHECK(module->lastMoney() == 0);
		CHECK(picker->getExperienceTracker()->getRank() == 1);
		CHECK(w.cash('A') == before);
	}
	SUBCASE("LevelUpChance = 0%, ResourceChance = 0%, BannerChance = 100%: outcome 3, money 250")
	{
		CombatWorld w(kSalvageObjects);
		Object *chest = w.unit("SalvChestNoResource", 'A', 300, 300);
		Object *picker = w.unit("SalvPicker", 'A', 305, 300);
		const int before = w.cash('A');
		auto *module = moduleOf<SalvageCrateCollide>(chest);
		REQUIRE(module != nullptr);
		CHECK(module->executeCrateBehavior(*picker) == true);
		CHECK(module->lastOutcome() == SalvageCrateCollide::OUTCOME_MONEY); // RW returns 3
		CHECK(module->lastMoney() == 250);
		CHECK(picker->getExperienceTracker()->getRank() == 0);
		CHECK(w.cash('A') == before + 250);
	}
	SUBCASE("LevelUpChance = 0%, BannerChance = 100%, ResourceChance = 100%: outcome 3 anyway (ResourceChance is unread)")
	{
		CombatWorld w(kSalvageObjects);
		Object *chest = w.unit("SalvChestResource", 'A', 300, 300);
		Object *picker = w.unit("SalvPicker", 'A', 305, 300);
		const int before = w.cash('A');
		auto *module = moduleOf<SalvageCrateCollide>(chest);
		REQUIRE(module != nullptr);
		CHECK(module->executeCrateBehavior(*picker) == true);
		CHECK(module->lastOutcome() == SalvageCrateCollide::OUTCOME_MONEY); // RW returns 3
		CHECK(module->lastMoney() == 250);
		CHECK(w.cash('A') == before + 250);
	}
	SUBCASE("LevelUpChance = 0% with a PLAYER upgrade set: outcome 4, never the LevelUpRadius branch (RW 0x136C returns only 2, 3 or 4)")
	{
		CombatWorld w(kSalvageObjects);
		Object *chest = w.unit("SalvChestUpgrade", 'A', 300, 300);
		Object *picker = w.unit("SalvPicker", 'A', 305, 300);
		const int before = w.cash('A');
		auto *module = moduleOf<SalvageCrateCollide>(chest);
		REQUIRE(module != nullptr);
		REQUIRE_FALSE(w.playerOf('A')->hasUpgradeComplete("Upgrade_X"));
		CHECK(module->executeCrateBehavior(*picker) == true);
		CHECK(module->lastOutcome() == SalvageCrateCollide::OUTCOME_UPGRADE); // RW returns 4 (the player upgrade is granted)
		CHECK(module->lastMoney() == 0);
		CHECK(w.cash('A') == before);
		CHECK(w.playerOf('A')->hasUpgradeComplete("Upgrade_X"));
	}
}

// BannerChance = 100%: both outcomes stay the same — RW 0x8C158B has no banner branch at all
TEST_CASE("PORT-94: BannerChance is never consulted: the outcome does not change with BannerChance = 100% (RW 0x8C158B has no banner branch)")
{
	LevelScope levels;
	SUBCASE("LevelUpChance = 100%, BannerChance = 100%: outcome 2, no money")
	{
		CombatWorld w(kSalvageObjects);
		Object *chest = w.unit("SalvChestBanner", 'A', 300, 300);
		Object *picker = w.unit("SalvPicker", 'A', 305, 300);
		const int before = w.cash('A');
		auto *module = moduleOf<SalvageCrateCollide>(chest);
		REQUIRE(module != nullptr);
		CHECK(module->executeCrateBehavior(*picker) == true);
		CHECK(module->lastOutcome() == SalvageCrateCollide::OUTCOME_LEVEL_UP);
		CHECK(module->lastMoney() == 0);
		CHECK(picker->getExperienceTracker()->getRank() == 1);
		CHECK(w.cash('A') == before);
	}
	SUBCASE("LevelUpChance = 0%, BannerChance = 100%: outcome 3, money")
	{
		CombatWorld w(kSalvageObjects);
		Object *chest = w.unit("SalvChestNoResource", 'A', 300, 300);
		Object *picker = w.unit("SalvPicker", 'A', 305, 300);
		const int before = w.cash('A');
		auto *module = moduleOf<SalvageCrateCollide>(chest);
		REQUIRE(module != nullptr);
		CHECK(module->executeCrateBehavior(*picker) == true);
		CHECK(module->lastOutcome() == SalvageCrateCollide::OUTCOME_MONEY);
		CHECK(w.cash('A') == before + 250);
	}
}

TEST_CASE("PORT-94: LevelUpRadius is not read: a bystander of the picker's player within the radius gains no level (RW 0x8C16BF sits in the unreachable outcome <= 1 branch, RW 0x8C168D)")
{
	LevelScope levels;
	CombatWorld w(kSalvageObjects);
	Object *chest = w.unit("SalvChestMod", 'A', 300, 300); // LevelUpRadius = 100.0 (parses to 1.0), LevelUpChance = 100%
	const ObjectID chestId = chest->getID();
	Object *picker = w.unit("SalvPicker", 'A', 305, 300);
	Object *bystander = w.unit("SalvPicker", 'A', 330, 300); // same player, 30 away, inside any radius, does not touch the crate
	Object *enemy = w.unit("SalvPicker", 'B', 330, 320);      // the other player
	w.frames(4);
	CHECK(w.byId(chestId) == nullptr);
	REQUIRE(picker->getExperienceTracker() != nullptr);
	REQUIRE(bystander->getExperienceTracker() != nullptr);
	REQUIRE(enemy->getExperienceTracker() != nullptr);
	CHECK(picker->getExperienceTracker()->getRank() == 1);
	CHECK(bystander->getExperienceTracker()->getRank() == 0); // no radius level-up, for anyone
	CHECK(enemy->getExperienceTracker()->getRank() == 0);
}

// the money through the full contact path (Min == Max, no random range): outcome 3 deposits 250
TEST_CASE("PORT-94: LevelUpChance = 0% through the contact path: the picker's player is paid (RW 0x8C148B)")
{
	LevelScope levels;
	CombatWorld w(kSalvageObjects);
	Object *chest = w.unit("SalvChestResource", 'A', 300, 300); // LevelUpChance = 0%, ResourceChance = 100%
	const ObjectID chestId = chest->getID();
	Object *picker = w.unit("SalvPicker", 'A', 305, 300);
	const int before = w.cash('A');
	w.frames(4);
	CHECK(w.byId(chestId) == nullptr);
	CHECK(picker->getExperienceTracker()->getRank() == 0);
	CHECK(w.cash('A') == before + 250);
}
