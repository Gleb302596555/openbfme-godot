// OpenBFME unit tests. GPL-3.0.
// PORT-65 (issue #177): SpellRechargeModifierUpgrade, the upgrade that shortens the spell book's recharge when its object is upgraded.

#include "doctest.h"

#include "IniTestUtil.h"
#include "LogicTestUtil.h"

#include "Common/INI.h"
#include "Common/Player.h"
#include "Common/PlayerScience.h"
#include "Common/SpecialPower.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/LogicModules.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Module/UpgradeModules.h"
#include "GameLogic/Object/Object.h"

#include <cstdint>
#include <string>
#include <vector>

namespace
{
const char kPort65Objects[] =
	"SpecialPower SpellBookQuick\n"
	"  Enum = SPECIAL_GENERAL_TARGETLESS\n"
	"  ReloadTime = 10000\n"
	"  Flags = RESPECT_RECHARGE_TIME_DISCOUNT\n"
	"End\n"
	"Upgrade Upgrade_LibraryBook1\n"
	"  Type = PLAYER\n"
	"End\n"
	"Upgrade Upgrade_LibraryBook2\n"
	"  Type = PLAYER\n"
	"End\n"
	"Object ElvenLibrary\n"
	"  KindOf = SELECTABLE STRUCTURE\n"
	"  Behavior = SpellRechargeModifierUpgrade ModuleTag_SpellBookRecharge\n"
	"    TriggeredBy = Upgrade_LibraryBook1 Upgrade_LibraryBook2\n"
	"    Percentage = -25%\n"
	"    Percentage = -50%\n"
	"    Percentage = -75%\n"
	"    LabelForPalantirString = GUI:SpellBookRefreshReduction\n"
	"  End\n"
	"End\n"
	"Object Erestor\n"
	"  KindOf = SELECTABLE HERO INFANTRY\n"
	"  Behavior = SpellRechargeModifierUpgrade ModuleTag_ErestorRecharge\n"
	"    StartsActive = Yes\n"
	"    Percentage = -10%\n"
	"    Percentage = -15%\n"
	"  End\n"
	"End\n"
	"Object QuickPower\n"
	"  KindOf = SELECTABLE STRUCTURE\n"
	"  Behavior = SpecialPowerModule ModuleTag_Quick\n"
	"    SpecialPowerTemplate = SpellBookQuick\n"
	"  End\n"
	"End\n";

struct Port65World : logictest::LogicWorld
{
	SpecialPowerStore powers;
	SpecialPowerStore *savedPowers = TheSpecialPowerStore;

	Port65World()
	{
		TheSpecialPowerStore = &powers;
		w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
		LogicModules::registerAll(w.modules);
		const std::string err = w.load(kPort65Objects, INI_LOAD_OVERWRITE, "port65.ini");
		REQUIRE_MESSAGE(err.empty(), err);
	}
	~Port65World() { TheSpecialPowerStore = savedPowers; }
};

// the reload the power gets: ReloadTime * (1.0 + the player's discount) (RW 0x896EE3 .. 0x896EFC; a zero result becomes one frame)
unsigned rechargeOf(Port65World &w, Object *power)
{
	auto *m = dynamic_cast<SpecialPowerModule *>(power->findModule("SpecialPowerModule"));
	REQUIRE(m != nullptr);
	m->startPowerRecharge(1.0f);
	return m->rechargeLength();
}
} // namespace

TEST_CASE("PORT-65 parse: the mod's fields of SpellRechargeModifierUpgrade reach the typed data (RW 0xC6F0D0)")
{
	Port65World w;
	const auto *library = dynamic_cast<const SpellRechargeModifierUpgradeModuleData *>(w.w.get("ElvenLibrary")->behaviorModules().find("ModuleTag_SpellBookRecharge")->data.get());
	REQUIRE(library != nullptr);
	REQUIRE(library->m_percentage.size() == 3); // RW 0x8BA26F: one fraction per Percentage line
	CHECK(library->m_percentage[0] == (float)(-25.0 * (double)0.01f));
	CHECK(library->m_percentage[1] == (float)(-50.0 * (double)0.01f));
	CHECK(library->m_percentage[2] == (float)(-75.0 * (double)0.01f));
	CHECK_FALSE(library->m_startsActive); // RW 0x8BA1F5: false in the data constructor
	CHECK(library->m_labelForPalantirString == "GUI:SpellBookRefreshReduction"); // stored only (#164 shows the label)
	CHECK(library->m_triggeredBy.size() == 2); // the upgrade table RW 0xC76AD8, the base's
	const auto *erestor = dynamic_cast<const SpellRechargeModifierUpgradeModuleData *>(w.w.get("Erestor")->behaviorModules().find("ModuleTag_ErestorRecharge")->data.get());
	REQUIRE(erestor != nullptr);
	CHECK(erestor->m_startsActive); // StartsActive = Yes (+ 0x144, RW 0x42E558)
	REQUIRE(erestor->m_percentage.size() == 2);
	CHECK(erestor->m_percentage[0] == (float)(-10.0 * (double)0.01f));
	CHECK(erestor->m_percentage[1] == (float)(-15.0 * (double)0.01f));
	CHECK(erestor->m_labelForPalantirString.empty());
}

TEST_CASE("PORT-65 recharge: an applied SpellRechargeModifierUpgrade shortens the recharge by the percentage of its count (RW 0x8BA139)")
{
	Port65World w;
	Player *alice = w.players.findPlayerWithName("Alice");
	Player *bob = w.players.findPlayerWithName("Bob");
	Object *library = w.make("ElvenLibrary", w.teamOf("Alice"));
	Object *power = w.make("QuickPower", w.teamOf("Alice"));
	REQUIRE(library->findModule("SpellRechargeModifierUpgrade") != nullptr);
	CHECK(alice->science().rechargeDiscount() == 0.0f);           // RW 0x6B045B
	CHECK(alice->science().spellRechargeModifierCount() == 0);    // RW 0x6B0463
	const unsigned plain = rechargeOf(w, power);
	REQUIRE(plain == 10000u); // ReloadTime with a discount of 0

	// Upgrade_LibraryBook1 completes for Alice: Object::updateUpgradeModules -> the mux -> the implementation
	alice->addCompletedUpgrade("Upgrade_LibraryBook1");
	library->updateUpgradeModules();
	CHECK(alice->science().spellRechargeModifierCount() == 1);
	CHECK(alice->science().rechargeDiscount() == (float)(-25.0 * (double)0.01f));
	CHECK(bob->science().rechargeDiscount() == 0.0f); // the player's, not the object's
	CHECK(rechargeOf(w, power) == (unsigned)((double)plain * (1.0 + (double)(float)(-25.0 * (double)0.01f)))); // 7500

	alice->addCompletedUpgrade("Upgrade_LibraryBook2");
	library->updateUpgradeModules();
	CHECK(alice->science().spellRechargeModifierCount() == 2);
	CHECK(alice->science().rechargeDiscount() == (float)(-50.0 * (double)0.01f));
	CHECK(rechargeOf(w, power) == (unsigned)((double)plain * (1.0 + (double)(float)(-50.0 * (double)0.01f)))); // 5000

	library->removeUpgrade(TheUpgradeCenter->findUpgrade("Upgrade_LibraryBook2"));
	CHECK(alice->science().spellRechargeModifierCount() == 1);
	CHECK(alice->science().rechargeDiscount() == (float)(-25.0 * (double)0.01f));
	library->removeUpgrade(TheUpgradeCenter->findUpgrade("Upgrade_LibraryBook1"));
	CHECK(alice->science().spellRechargeModifierCount() == 0);
	CHECK(alice->science().rechargeDiscount() == (float)(-25.0 * (double)0.01f)); // the index stays 0 (RW 0x8BA018)
}

TEST_CASE("PORT-65 neighbour: StartsActive, a capture and an uncompleted upgrade (RW 0x8B9FB6, RW 0x8BA05E)")
{
	Port65World w;
	Player *alice = w.players.findPlayerWithName("Alice");
	Player *bob = w.players.findPlayerWithName("Bob");
	Object *erestor = w.make("Erestor", w.teamOf("Alice")); // StartsActive: onBuildComplete runs it
	CHECK(alice->science().spellRechargeModifierCount() == 1);
	CHECK(alice->science().rechargeDiscount() == (float)(-10.0 * (double)0.01f));
	auto *module = dynamic_cast<SpellRechargeModifierUpgrade *>(erestor->findModule("SpellRechargeModifierUpgrade"));
	REQUIRE(module != nullptr);
	CHECK(module->isAlreadyUpgraded());
	CHECK(module->friend_flag20());
	erestor->setTeam(w.teamOf("Bob"));
	CHECK(alice->science().spellRechargeModifierCount() == 0);
	CHECK(bob->science().spellRechargeModifierCount() == 1);
	CHECK(bob->science().rechargeDiscount() == (float)(-10.0 * (double)0.01f));
}

TEST_CASE("PORT-65 state: the counter is player state and travels in the snapshot (RW 0x8B9D39)")
{
	Port65World w;
	Player *alice = w.players.findPlayerWithName("Alice");
	Object *library = w.make("ElvenLibrary", w.teamOf("Alice"));
	const std::uint32_t before = w.logic->computeStateHash();
	alice->addCompletedUpgrade("Upgrade_LibraryBook1");
	library->updateUpgradeModules();
	CHECK(w.logic->computeStateHash() != before);
	std::string error;
	std::vector<std::uint8_t> saved;
	REQUIRE_MESSAGE(w.logic->saveSnapshot(saved, &error), error);
	alice->addCompletedUpgrade("Upgrade_LibraryBook2");
	library->updateUpgradeModules();
	REQUIRE(alice->science().spellRechargeModifierCount() == 2);
	REQUIRE_MESSAGE(w.logic->loadSnapshot(saved, &error), error);
	alice = w.players.findPlayerWithName("Alice");
	REQUIRE(alice != nullptr);
	CHECK(alice->science().spellRechargeModifierCount() == 1);
	CHECK(alice->science().rechargeDiscount() == (float)(-25.0 * (double)0.01f));
}
