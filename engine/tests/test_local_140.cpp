// OpenBFME. GPL-3.0.
//
// LOCAL-140 (fork issue 857): tests for object-level INI field readers ported from RotWK 2.01:
//   1. Object::ShouldClearShotsOnIdle (RW 0x007489B6 / 0x007489DC..0x007489EF; DONOR BFME2 0x00346FD0):
//      AIIdleState::onEnter zeroes FiringTracker::m_count (+0x20) when ThingTemplate + 0x645 is true.
//   2. Object::Flammability::FuelFactor (RW 0x0073D711 -> RW 0x0073D0FD; DONOR BFME2 0x0033B677):
//      computes Flammability.fuel = (int)(FuelFactor * firstBodyModule->m_maxHealth), range-checked 0..65535,
//      and throws INIException when no Body module precedes Flammability.
//   3. FireLogicSystem::MaxCellsBurning (RW 0x00C11750, read RW 0x005F32FF / 0x005F3314):
//      FireLogicSystem::parseINI parses MaxCellsBurning and FuelRegenerationRate from the INI block.

#include "doctest.h"

#include "CombatTestUtil.h"
#include "IniTestUtil.h"
#include "Common/INI.h"
#include "Common/INIException.h"
#include "Common/Thing/ThingTemplate.h"
#include "GameLogic/Combat/ObjectWeapons.h"
#include "GameLogic/Map/FireLogicSystem.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/WeaponState.h"

namespace
{
struct Local140FiringHost : FiringTrackerHost
{
	std::uint32_t frame = 10;
	Coord3D pos{ 0.0f, 0.0f, 0.0f };
	std::uint32_t bonusMask = 0;
	std::uint32_t currentFrame() override { return frame; }
	Coord3D ownerPosition() override { return pos; }
	std::uint32_t weaponBonusConditionMask() override { return bonusMask; }
	void setWeaponBonusConditionMask(std::uint32_t mask) override { bonusMask = mask; }
	void reloadAllWeapons() override {}
};
} // namespace

TEST_CASE("LOCAL-140: Object ShouldClearShotsOnIdle zeroes FiringTracker shot count on AIIdleState::onEnter")
{
	// Source: RotWK 2.01 RW 0x007489B6 (0x007489D9..0x007489EF; ThingTemplate + 0x645 row RW 0xDA4888;
	// DONOR BFME2 0x00346FD0 Rva0033FE65OnEnter.cpp). Ennorath: MordorGrond (grond.ini:248).
	combattest::CombatWorld w;
	const std::string err = w.w.load(
		"Weapon Local140RamWeapon\n"
		"  AttackRange = 40.0\n"
		"  DelayBetweenShots = 200\n"
		"  PreAttackDelay = 0\n"
		"  FiringDuration = 0\n"
		"  ContinuousFireOne = 2\n"
		"  ContinuousFireCoast = 5000\n"
		"  DamageNugget\n"
		"    Damage = 10\n"
		"    DamageType = SIEGE\n"
		"  End\n"
		"End\n"
		"Object Local140ClearShots\n"
		"  KindOf = VEHICLE\n"
		"  ShouldClearShotsOnIdle = Yes\n"
		"  WeaponSet\n"
		"    Conditions = None\n"
		"    Weapon = PRIMARY Local140RamWeapon\n"
		"  End\n"
		"  Body = ActiveBody ModuleTag_Body\n"
		"    MaxHealth = 500.0\n"
		"    InitialHealth = 500.0\n"
		"  End\n"
		"  Behavior = AIUpdateInterface ModuleTag_AI\n"
		"  End\n"
		"End\n"
		"Object Local140KeepShots\n"
		"  KindOf = VEHICLE\n"
		"  WeaponSet\n"
		"    Conditions = None\n"
		"    Weapon = PRIMARY Local140RamWeapon\n"
		"  End\n"
		"  Body = ActiveBody ModuleTag_Body\n"
		"    MaxHealth = 500.0\n"
		"    InitialHealth = 500.0\n"
		"  End\n"
		"  Behavior = AIUpdateInterface ModuleTag_AI\n"
		"  End\n"
		"End\n",
		INI_LOAD_OVERWRITE,
		"local140_clear_shots.ini");
	REQUIRE_MESSAGE(err.empty(), err);

	Object *clearObj = w.unit("Local140ClearShots", 'A', 100.0f, 100.0f);
	Object *keepObj = w.unit("Local140KeepShots", 'A', 140.0f, 100.0f);
	REQUIRE(clearObj != nullptr);
	REQUIRE(keepObj != nullptr);
	REQUIRE(clearObj->getWeapons() != nullptr);
	REQUIRE(keepObj->getWeapons() != nullptr);
	REQUIRE(clearObj->getAIUpdateInterface() != nullptr);
	REQUIRE(keepObj->getAIUpdateInterface() != nullptr);

	const WeaponTemplate *wt = TheWeaponStore->findWeaponTemplate("Local140RamWeapon");
	REQUIRE(wt != nullptr);
	Weapon weapon(wt, PRIMARY_WEAPON, 10u);
	Local140FiringHost host;

	clearObj->getWeapons()->firingTracker().shotFired(host, weapon, 99, Coord3D{ 120.0f, 100.0f, 0.0f }, false);
	clearObj->getWeapons()->firingTracker().shotFired(host, weapon, 99, Coord3D{ 120.0f, 100.0f, 0.0f }, false);
	keepObj->getWeapons()->firingTracker().shotFired(host, weapon, 99, Coord3D{ 160.0f, 100.0f, 0.0f }, false);
	keepObj->getWeapons()->firingTracker().shotFired(host, weapon, 99, Coord3D{ 160.0f, 100.0f, 0.0f }, false);
	REQUIRE(clearObj->getWeapons()->firingTracker().shotCount() == 2);
	REQUIRE(keepObj->getWeapons()->firingTracker().shotCount() == 2);

	// Enter AIIdleState via aiIdle: ShouldClearShotsOnIdle = Yes zeroes m_count (+0x20), default keeps it.
	clearObj->getAIUpdateInterface()->aiIdle(CMD_FROM_AI);
	keepObj->getAIUpdateInterface()->aiIdle(CMD_FROM_AI);

	CHECK(clearObj->getWeapons()->firingTracker().shotCount() == 0);
	CHECK(keepObj->getWeapons()->firingTracker().shotCount() == 2);
}

TEST_CASE("LOCAL-140: Object Flammability FuelFactor computes fuel from Body MaxHealth")
{
	// Source: RotWK 2.01 RW 0x0073D711 (ThingTemplate::parseFlammability) -> RW 0x0073D0FD (parseFuelFactor:
	// first behavior Body via vslot 7 RW 0x0073D12D, factor * BodyModuleData::m_maxHealth RW 0x0073D185..0x0073D188,
	// range 0..0xFFFF RW 0x0073D18D; DONOR BFME2 0x0033B677). Ennorath: IsengardUrukPit (urukpit.ini:306, FuelFactor = 0.5).
	combattest::CombatWorld w;
	const std::string err = w.w.load(
		"Object Local140FuelFactorPit\n"
		"  KindOf = STRUCTURE\n"
		"  Body = ActiveBody ModuleTag_Body\n"
		"    MaxHealth = 2000.0\n"
		"    InitialHealth = 2000.0\n"
		"  End\n"
		"  Flammability\n"
		"    FuelFactor = 0.5\n"
		"    MaxBurnRate = 20\n"
		"    Decay = 5\n"
		"    Resistance = 10\n"
		"  End\n"
		"End\n"
		"Object Local140DirectFuel\n"
		"  KindOf = STRUCTURE\n"
		"  Body = ActiveBody ModuleTag_Body\n"
		"    MaxHealth = 2000.0\n"
		"    InitialHealth = 2000.0\n"
		"  End\n"
		"  Flammability\n"
		"    Fuel = 350\n"
		"    MaxBurnRate = 15\n"
		"  End\n"
		"End\n",
		INI_LOAD_OVERWRITE,
		"local140_flammability.ini");
	REQUIRE_MESSAGE(err.empty(), err);

	Object *pitObj = w.unit("Local140FuelFactorPit", 'A', 100.0f, 100.0f);
	Object *directObj = w.unit("Local140DirectFuel", 'A', 140.0f, 100.0f);
	REQUIRE(pitObj != nullptr);
	REQUIRE(directObj != nullptr);
	const ThingTemplate *pit = pitObj->getTemplate();
	const ThingTemplate *direct = directObj->getTemplate();
	REQUIRE(pit != nullptr);
	REQUIRE(direct != nullptr);

	CHECK(pit->flammability().fuel == 1000);
	CHECK(pit->flammability().maxBurnRate == 20);
	CHECK(pit->flammability().decay == 5);
	CHECK(pit->flammability().resistance == 10);
	CHECK(direct->flammability().fuel == 350);
	CHECK(direct->flammability().maxBurnRate == 15);
}

TEST_CASE("LOCAL-140: Object Flammability FuelFactor without Body reports error")
{
	// Source: RotWK 2.01 RW 0x0073D0FD (RW 0x0073D153: "Can't use FuelFactor without defining Body first").
	combattest::CombatWorld w;
	const std::string err = w.w.load(
		"Object Local140NoBodyFuelFactor\n"
		"  KindOf = STRUCTURE\n"
		"  Flammability\n"
		"    FuelFactor = 0.5\n"
		"  End\n"
		"End\n",
		INI_LOAD_OVERWRITE,
		"local140_nobody.ini");
	CHECK(!err.empty());
	CHECK(err.find("FuelFactor") != std::string::npos);
}

TEST_CASE("LOCAL-140: FireLogicSystem::parseINI reads MaxCellsBurning and FuelRegenerationRate")
{
	// Source: RotWK 2.01 FireLogicSystem FieldParse table RW 0x00C11750 (MaxCellsBurning +0x90,
	// FuelRegenerationRate +0x94; MaxCellsBurning read by FireLogicSystem and ParticleSystemManager RW 0x005F32FF / 0x005F3314).
	combattest::CombatWorld w;
	const std::string err = w.w.load(
		"Object Local140Dummy\n"
		"  KindOf = STRUCTURE\n"
		"  Body = ActiveBody ModuleTag_Body\n"
		"    MaxHealth = 100.0\n"
		"    InitialHealth = 100.0\n"
		"  End\n"
		"End\n",
		INI_LOAD_OVERWRITE,
		"local140_dummy.ini");
	REQUIRE_MESSAGE(err.empty(), err);

	Object *dummy = w.unit("Local140Dummy", 'A', 100.0f, 100.0f);
	REQUIRE(dummy != nullptr);
	FireLogicSystem &fire = dummy->logic().fireLogic();
	CHECK(fire.maxCellsBurning() == 1000);

	INIEnvironment env;
	env.blocks.registerBlock("FireLogicSystem", [&fire](INI *p) { fire.parseINI(p); });
	INI ini(env);
	const std::string iniText =
		"FireLogicSystem\n"
		"  MaxCellsBurning = 42\n"
		"  FuelRegenerationRate = 7\n"
		"End\n";
	ini.loadMemory("local140_fire.ini", initest::toBytes(iniText), INI_LOAD_OVERWRITE);

	CHECK(fire.maxCellsBurning() == 42);
}
