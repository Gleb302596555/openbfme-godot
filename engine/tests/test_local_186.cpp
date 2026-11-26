// OpenBFME. GPL-3.0.
// LOCAL-186 (fork issue 979): matrix red buttons — SweepEffectProbe rule for DevastateSpecialPower (RW 0x8CC73E..0x8CC8E2)
// and neighboring empty-FireWeapon case (RW 0x8CC8BB).
// Editions: both RotWK 2.01 and BFME2 1.06 (shared SweepEffectProbe / DevastateSpecialPower FireWeapon rule).

#include "doctest.h"
#include "CombatTestUtil.h"
#include "SweepEffectProbe.h"

#include "Common/SpecialPower.h"
#include "GameLogic/Combat/ObjectWeapons.h"
#include "GameLogic/Module/LogicModules.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Module/SpellBookPowers.h"
#include "GameLogic/ObjectCreationList.h"

#include <string>

namespace
{
using sweep_probe::EffectProbe;

struct Local186World : combattest::CombatWorld
{
	SpecialPowerStore powers;
	SpecialPowerStore *savedPowers = TheSpecialPowerStore;
	ObjectCreationListStore lists{ w.keys };
	ObjectCreationListStore *savedLists = TheObjectCreationListStore;
	AttributeModifierStore attrMods;
	AttributeModifierStore *savedAttrMods = TheAttributeModifierStore;

	Local186World()
	{
		TheSpecialPowerStore = &powers;
		TheObjectCreationListStore = &lists;
		TheAttributeModifierStore = &attrMods;
		w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
		w.fx.env.blocks.registerBlock("ObjectCreationList", [](INI *ini) { ObjectCreationListStore::parseObjectCreationListDefinitionGlobal(ini); });
		SpecialPowerModules::registerAll(w.modules);
		SpellBookPowers::registerAll(w.modules);
		LogicModules::registerAll(w.modules);
		combat().setAutoAcquireEnabled(false);
	}

	~Local186World()
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

// Expectations from RotWK 2.01 RW 0x8CC73E..0x8CC8E2 (DevastateSpecialPower::doSpecialPowerAtLocation fires FireWeapon via
// TheWeaponStore->createAndFireTempWeapon at RW 0x8CC8DD -> RW 0x6CF530) and Ennorath data/ini/object/system/system.ini:314
// (SpellBookDevastation -> FireWeapon = DevastationEntWeaponMod -> WeaponOCLNugget OCL_IsenDevastationEgg -> IsengardDevastationPing).
TEST_CASE("LOCAL-186 SweepEffectProbe inspects DevastateSpecialPower FireWeapon (RW 0x8CC73E..0x8CC8E2)")
{
	Local186World env;
	env.loadIni(
		"SpecialPower TestDevastate\n"
		"  Enum = SPECIAL_SPELL_BOOK_DEVASTATION\n"
		"  ReloadTime = 1000\n"
		"  RadiusCursorRadius = 150\n"
		"End\n"
		"Object TestDevastatePing\n"
		"  KindOf = NO_COLLIDE IMMOBILE UNATTACKABLE INERT\n"
		"  Body = ActiveBody ModuleTag_Body\n"
		"    MaxHealth = 10\n"
		"  End\n"
		"End\n"
		"ObjectCreationList OCL_TestDevastatePing\n"
		"  CreateObject\n"
		"    ObjectNames = TestDevastatePing\n"
		"    Count = 1\n"
		"  End\n"
		"End\n"
		"Weapon TestDevastateWeapon\n"
		"  WeaponOCLNugget\n"
		"    WeaponOCLName = OCL_TestDevastatePing\n"
		"  End\n"
		"End\n"
		"Object TestSpellBook\n"
		"  KindOf = IMMOBILE\n"
		"  Body = ActiveBody ModuleTag_Body\n"
		"    MaxHealth = 100\n"
		"  End\n"
		"  Behavior = DevastateSpecialPower ModuleTag_Devastate\n"
		"    SpecialPowerTemplate = TestDevastate\n"
		"    Radius = 170\n"
		"    TreeValueMultiplier = 50%\n"
		"    TreeValueTotalCap = 1500\n"
		"    FireWeapon = TestDevastateWeapon\n"
		"  End\n"
		"End\n");

	Object *book = env.spawn("TestSpellBook", 100.0f, 100.0f, 'A');
	REQUIRE(book != nullptr);
	const SpecialPowerTemplate *sp = env.powers.findSpecialPowerTemplate("TestDevastate");
	REQUIRE(sp != nullptr);

	EffectProbe before = EffectProbe::take(*env.logic, book, sp);
	CHECK(before.unasserted.empty());
	REQUIRE(before.expects.size() == 1);
	CHECK(before.expects[0].kind == EffectProbe::SPAWN);
	CHECK(before.expects[0].templates.count("TestDevastatePing") == 1);

	const Coord3D loc{ 200.0f, 200.0f, 0.0f };
	REQUIRE(SpecialPowerModules::doSpecialPowerAtLocation(*book, sp, loc, 0u, true));
	env.frames(2);

	EffectProbe after = EffectProbe::take(*env.logic, book, sp);
	std::string missing;
	const std::string effect = before.compare(after, missing);
	CHECK(missing.empty());
	CHECK(effect.find("TestDevastatePing") != std::string::npos);
}

// Neighboring behavior from RotWK 2.01 RW 0x8CC8BB: DevastateSpecialPower with no FireWeapon skips createAndFireTempWeapon
// and must not crash or invent bogus expects in SweepEffectProbe.
TEST_CASE("LOCAL-186 SweepEffectProbe handles DevastateSpecialPower without FireWeapon (RW 0x8CC8BB)")
{
	Local186World env;
	env.loadIni(
		"SpecialPower TestDevastateBare\n"
		"  Enum = SPECIAL_SPELL_BOOK_DEVASTATION\n"
		"  ReloadTime = 1000\n"
		"  RadiusCursorRadius = 150\n"
		"End\n"
		"Object TestSpellBookBare\n"
		"  KindOf = IMMOBILE\n"
		"  Body = ActiveBody ModuleTag_Body\n"
		"    MaxHealth = 100\n"
		"  End\n"
		"  Behavior = DevastateSpecialPower ModuleTag_Devastate\n"
		"    SpecialPowerTemplate = TestDevastateBare\n"
		"    Radius = 170\n"
		"  End\n"
		"End\n");

	Object *book = env.spawn("TestSpellBookBare", 100.0f, 100.0f, 'A');
	REQUIRE(book != nullptr);
	const SpecialPowerTemplate *sp = env.powers.findSpecialPowerTemplate("TestDevastateBare");
	REQUIRE(sp != nullptr);

	EffectProbe probe = EffectProbe::take(*env.logic, book, sp);
	CHECK(probe.unasserted.empty());
	CHECK(probe.expects.empty());
}
