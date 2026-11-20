// OpenBFME unit tests. LOCAL-82 (issue #706): OCLSpecialPower 26 Ennorath buttons across all 5 matrix groups.
// Both RotWK 2.01 (RW) and BFME2 1.06 rules apply unless noted.
// GPL-3.0.

#include "doctest.h"
#include "EnnorathScenario.h"

#include "Common/Player.h"
#include "Common/SpecialPower.h"
#include "Common/Upgrade.h"
#include "GameClient/ControlBar.h"
#include "GameClient/ControlBarCommands.h"
#include "GameLogic/ActionManager.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Module/ActiveBody.h"
#include "GameLogic/Module/EmotionModules.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/ObjectCreationList.h"
#include "GameLogic/System/ShroudManager.h"

#include <string>
#include <vector>

namespace
{

int countObjectsByTemplate(GameLogic &logic, const char *name)
{
	int count = 0;
	for (Object *o = logic.getFirstObject(); o; o = o->getNextObject())
	{
		if (o->getTemplate() && o->getTemplate()->getName() == name)
		{
			++count;
		}
	}
	return count;
}

void stepFrames(GameLogic &logic, int n)
{
	for (int i = 0; i < n; ++i)
	{
		for (int phase = 1; phase <= 6; ++phase) { logic.update(phase); }
	}
}

void giveUpgrade(Object &obj, const char *name)
{
	const UpgradeTemplate *ut = TheUpgradeCenter ? TheUpgradeCenter->findUpgrade(name) : nullptr;
	REQUIRE_MESSAGE(ut != nullptr, "upgrade not found: " << name);
	if (ut->getUpgradeType() == UPGRADE_TYPE_PLAYER)
	{
		obj.getControllingPlayer()->addUpgrade(ut, Player::UPGRADE_STATUS_COMPLETE);
	}
	else
	{
		obj.giveUpgrade(ut);
	}
}


struct ScopedShroud
{
	GameLogic &logic;
	ShroudManager shroud;
	explicit ScopedShroud(GameLogic &l) : logic(l), shroud(l)
	{
		shroud.init(20.0f, 0);
		shroud.setExtent(0.0f, 0.0f, 1200.0f, 1200.0f, 20.0f);
		for (int i = 0; i < 16; ++i) { shroud.revealMapForPlayerPermanently(i); }
		logic.setShroud(&shroud);
	}
	~ScopedShroud() { logic.setShroud(nullptr); }
};

} // namespace

// Group 1 (NO-FIRE): NEEDS_OBJECT_FILTER (RW 0x82DEA6) refuses bare ground, accepts the matching allied structure and OCLSpecialPower creates its object.
TEST_CASE("LOCAL-82 Group 1: empowered building summons (NEEDS_OBJECT_FILTER, RW 0x82DEA6 + RW 0x8C75D8)")
{
	ennorathscenario::World w;
	if (!w.load("LOCAL-82 Group 1"))
	{
		return;
	}
	structtest::Arena arena(w.shared, "FactionMordor", "FactionMen");
	ScopedShroud shroud(arena.logic);

	// 1a. MordorGothmog_mod: SpecialAbilityGothmogEmpoweredBuildingMorgul (ObjectFilter = ANY +MordorOrcPit ALLIES) -> OCL_MorgulGothmogBuildingUpper -> GothmogAutocasterMorgul
	Object *gothmog = w.place(arena, "MordorGothmog_mod", 0, 200.0f, 200.0f);
	giveUpgrade(*gothmog, "Upgrade_Level_1");
	stepFrames(arena.logic, 2);

	const SpecialPowerTemplate *spGoth = TheSpecialPowerStore->findSpecialPowerTemplate("SpecialAbilityGothmogEmpoweredBuildingMorgul");
	REQUIRE(spGoth != nullptr);
	const Coord3D bareGround{ 320.0f, 320.0f, 0.0f };
	CHECK_FALSE(ActionManager::canDoSpecialPowerAtLocation(*gothmog, bareGround, spGoth, 0));

	Object *orcPit = w.place(arena, "MordorOrcPit", 0, 320.0f, 320.0f);
	stepFrames(arena.logic, 2);
	const Coord3D pitPos = *orcPit->getPosition();
	CHECK(ActionManager::canDoSpecialPowerAtLocation(*gothmog, pitPos, spGoth, 0));

	const int beforeGoth = countObjectsByTemplate(arena.logic, "GothmogAutocasterMorgul");
	REQUIRE(SpecialPowerModules::doSpecialPowerAtLocation(*gothmog, spGoth, pitPos, 0, false));
	stepFrames(arena.logic, 5);
	CHECK(countObjectsByTemplate(arena.logic, "GothmogAutocasterMorgul") == beforeGoth + 1);

	// 1b. AngmarGulmataur: SpecialAbilityGulmataurDefence (ObjectFilter = ANY +AngmarHallofTwilight ... ALLIES)
	Object *gulm = w.place(arena, "AngmarGulmataur", 0, 600.0f, 200.0f);
	giveUpgrade(*gulm, "Upgrade_Level_1");
	stepFrames(arena.logic, 2);
	const SpecialPowerTemplate *spGulm = TheSpecialPowerStore->findSpecialPowerTemplate("SpecialAbilityGulmataurDefence");
	REQUIRE(spGulm != nullptr);
	const Coord3D bare2{ 720.0f, 200.0f, 0.0f };
	CHECK_FALSE(ActionManager::canDoSpecialPowerAtLocation(*gulm, bare2, spGulm, 0));
	Object *hall = w.place(arena, "AngmarHallofTwilight", 0, 720.0f, 200.0f);
	stepFrames(arena.logic, 2);
	CHECK(ActionManager::canDoSpecialPowerAtLocation(*gulm, *hall->getPosition(), spGulm, 0));

	// 1c. GondorAdvisor_forFortress: SpecialAbilityAdvisorMarketMoney (ObjectFilter = NONE +GondorMarketPlace SAME_PLAYER)
	Object *advisor = w.place(arena, "GondorAdvisor_forFortress", 0, 200.0f, 600.0f);
	stepFrames(arena.logic, 2);
	const SpecialPowerTemplate *spAdv = TheSpecialPowerStore->findSpecialPowerTemplate("SpecialAbilityAdvisorMarketMoney");
	REQUIRE(spAdv != nullptr);
	const Coord3D bare3{ 320.0f, 600.0f, 0.0f };
	CHECK_FALSE(ActionManager::canDoSpecialPowerAtLocation(*advisor, bare3, spAdv, 0));
	Object *market = w.place(arena, "GondorMarketPlace", 0, 320.0f, 600.0f);
	stepFrames(arena.logic, 2);
	CHECK(ActionManager::canDoSpecialPowerAtLocation(*advisor, *market->getPosition(), spAdv, 0));
}

// Group 2a (NO-EFFECT traps & mine): OCLSpecialPower spawns DetonateRangedObj AND WeaponFireSpecialAbilityUpdate kills the trap (SUICIDE + DEATH_BURNED), firing its SlowDeath weapon.
TEST_CASE("LOCAL-82 Group 2a: Saleme traps and Isengard mine detonate (OCLSpecialPower RW 0x8C73EF + SUICIDE SlowDeath)")
{
	ennorathscenario::World w;
	if (!w.load("LOCAL-82 Group 2a"))
	{
		return;
	}
	structtest::Arena arena(w.shared, "FactionMordor", "FactionMen");
	ScopedShroud shroud(arena.logic);

	const SpecialPowerTemplate *spDet = TheSpecialPowerStore->findSpecialPowerTemplate("SpecialAbilityDetonate");
	REQUIRE(spDet != nullptr);

	// 2a-1. SalemeExplodeTrap: OCL_SalemeDetonateRanged + SalemeDetonateFallen (SUICIDE, BURNED) -> SlowDeath SalemeExplodeExplosionsfalleWeapon (Radius 80, Damage 500)
	Object *trap = w.place(arena, "SalemeExplodeTrap", 0, 300.0f, 300.0f);
	Object *enemy = w.place(arena, "GondorFighter", 1, 330.0f, 300.0f);
	stepFrames(arena.logic, 2);
		const int oclBefore = countObjectsByTemplate(arena.logic, "SalemeDetonateRangedObj");

	REQUIRE(SpecialPowerModules::doSpecialPower(*trap, spDet, 0, false));
	stepFrames(arena.logic, 15);

	CHECK(countObjectsByTemplate(arena.logic, "SalemeDetonateRangedObj") == oclBefore + 1);
	CHECK(trap->isEffectivelyDead());
	CHECK(enemy->isEffectivelyDead());

	// 2a-2. SalemeBlindTrap: OCL_SalemeDetonateRanged + SalemeDetonateFallen -> SlowDeath SalemeExplodeBlendfalleWeapon (EmotionWeaponNugget UNCONTROLLABLE_FEAR, Radius 50)
	Object *btrap = w.place(arena, "SalemeBlindTrap", 0, 700.0f, 300.0f);
	Object *enemy2 = w.place(arena, "GondorFighter", 1, 725.0f, 300.0f);
	stepFrames(arena.logic, 2);
	REQUIRE(SpecialPowerModules::doSpecialPower(*btrap, spDet, 0, false));
	stepFrames(arena.logic, 15);
	CHECK(btrap->isEffectivelyDead());
	(void)enemy2;

	// 2a-3. IsengardDeployedExplosiveMine: OCL_IsengardDetonateRanged + IsegardDetonateMine (UnpackTime 12000 -> 60 frames)
	Object *mine = w.place(arena, "IsengardDeployedExplosiveMine", 0, 300.0f, 700.0f);
	Object *enemy3 = w.place(arena, "GondorFighter", 1, 330.0f, 700.0f);
	stepFrames(arena.logic, 2);
	const int mineOclBefore = countObjectsByTemplate(arena.logic, "IsengardDetonateRangedObj");
		REQUIRE(SpecialPowerModules::doSpecialPower(*mine, spDet, 0, false));
	stepFrames(arena.logic, 1);
	CHECK(countObjectsByTemplate(arena.logic, "IsengardDetonateRangedObj") == mineOclBefore + 1);
	stepFrames(arena.logic, 79);
	CHECK(mine->isEffectivelyDead());
	CHECK(enemy3->isEffectivelyDead());
}

// Group 2b (NO-EFFECT arrow volleys): OCLSpecialPower creates MenLongShotObjectGondor / DrarWarnShotObject + 10 DrarSpeerfeuerEgg.
TEST_CASE("LOCAL-82 Group 2b: Ranger LongShot and Dain Speerfeuer create their OCL objects (RW 0x8C75D8)")
{
	ennorathscenario::World w;
	if (!w.load("LOCAL-82 Group 2b"))
	{
		return;
	}
	structtest::Arena arena(w.shared, "FactionMordor", "FactionMen");
	ScopedShroud shroud(arena.logic);

	// 2b-1. GondorRangerHorde: SpecialAbilityMenLongShot -> OCL_LongShotAbilityGondor -> MenLongShotObjectGondor
	Object *rangers = w.place(arena, "GondorRangerHorde", 0, 200.0f, 200.0f);
	giveUpgrade(*rangers, "Upgrade_ObjectLevel2");
	stepFrames(arena.logic, 2);
	const SpecialPowerTemplate *spLong = TheSpecialPowerStore->findSpecialPowerTemplate("SpecialAbilityMenLongShot");
	REQUIRE(spLong != nullptr);
	const Coord3D targetPos{ 400.0f, 200.0f, 0.0f };
	const int beforeLong = countObjectsByTemplate(arena.logic, "MenLongShotObjectGondor");
	REQUIRE(SpecialPowerModules::doSpecialPowerAtLocation(*rangers, spLong, targetPos, 0, false));
	stepFrames(arena.logic, 3);
	CHECK(countObjectsByTemplate(arena.logic, "MenLongShotObjectGondor") == beforeLong + 1);

	// 2b-2. DwarvenDain_mod: SpecialAbilityDrarSpeerfeuer -> OCL_SpeerfeuerAbility -> DrarWarnShotObject + 10 DrarSpeerfeuerEgg
	Object *dain = w.place(arena, "DwarvenDain_mod", 0, 200.0f, 500.0f);
	giveUpgrade(*dain, "Upgrade_Level_10");
	stepFrames(arena.logic, 2);
	const SpecialPowerTemplate *spDain = TheSpecialPowerStore->findSpecialPowerTemplate("SpecialAbilityDrarSpeerfeuer");
	REQUIRE(spDain != nullptr);
	const Coord3D targetPos2{ 400.0f, 500.0f, 0.0f };
	const int beforeWarn = countObjectsByTemplate(arena.logic, "DrarWarnShotObject");
	const int beforeEgg = countObjectsByTemplate(arena.logic, "DrarSpeerfeuerEgg");
	REQUIRE(SpecialPowerModules::doSpecialPowerAtLocation(*dain, spDain, targetPos2, 0, false));
	stepFrames(arena.logic, 3);
	CHECK(countObjectsByTemplate(arena.logic, "DrarWarnShotObject") == beforeWarn + 1);
	CHECK(countObjectsByTemplate(arena.logic, "DrarSpeerfeuerEgg") == beforeEgg + 10);
}

// Group 3 (ORDER): NO_FORBIDDEN_OBJECTS (RW 0x82D2FD) accepts clear ground, refuses crowded ground; CarthaenSpy creates rangers.
TEST_CASE("LOCAL-82 Group 3: RohanHama NO_FORBIDDEN_OBJECTS (RW 0x82D2FD) and CarthaenSpy OCL summon (RW 0x8C73EF)")
{
	ennorathscenario::World w;
	if (!w.load("LOCAL-82 Group 3"))
	{
		return;
	}
	structtest::Arena arena(w.shared, "FactionMordor", "FactionMen");
	ScopedShroud shroud(arena.logic);

	// 3a. RohanHama: SpecialAbilityHamaRoyalGuard (NO_FORBIDDEN_OBJECTS, ForbiddenObjectRange = 100, MaxCastRange = 150)
	Object *hama = w.place(arena, "RohanHama", 0, 200.0f, 200.0f);
	giveUpgrade(*hama, "Upgrade_Level_9");
	stepFrames(arena.logic, 2);
	const SpecialPowerTemplate *spHama = TheSpecialPowerStore->findSpecialPowerTemplate("SpecialAbilityHamaRoyalGuard");
	REQUIRE(spHama != nullptr);
	const Coord3D clearSpot{ 300.0f, 200.0f, 0.0f };
	CHECK(ActionManager::canDoSpecialPowerAtLocation(*hama, clearSpot, spHama, 0));

	// Place a unit inside ForbiddenObjectRange (100): must now be refused (RW 0x82D2FD), reproducing the ORDER sweep condition.
	(void)w.place(arena, "GondorFarm", 1, 320.0f, 200.0f);
	stepFrames(arena.logic, 2);
	CHECK_FALSE(ActionManager::canDoSpecialPowerAtLocation(*hama, clearSpot, spHama, 0));

	// 3b. CarthaenSpy: SpecialAbilityCarthaenRangersCall -> OCL_CarthLoneRangersEgg -> CardolanRangerHorde_Summoned + CardolanRangerScoutKiller
	Object *carth = w.place(arena, "CarthaenSpy", 0, 600.0f, 600.0f);
	giveUpgrade(*carth, "Upgrade_ArnorBibliothekImladrisUpgrade1");
	stepFrames(arena.logic, 2);
	const SpecialPowerTemplate *spCarth = TheSpecialPowerStore->findSpecialPowerTemplate("SpecialAbilityCarthaenRangersCall");
	REQUIRE(spCarth != nullptr);
	const int beforeScout = countObjectsByTemplate(arena.logic, "CardolanRangerScoutKiller");
	const int beforeHorde = countObjectsByTemplate(arena.logic, "CardolanRangerHorde_Summoned");
	REQUIRE(SpecialPowerModules::doSpecialPower(*carth, spCarth, 0, false));
	CHECK(countObjectsByTemplate(arena.logic, "CardolanRangerScoutKiller") == beforeScout + 1);
	stepFrames(arena.logic, 5);
	CHECK(countObjectsByTemplate(arena.logic, "CardolanRangerHorde_Summoned") == beforeHorde + 1);
}

// Group 4 (NOT-ON-BAR) and Group 5 (GREY-BUG): GaladrielDarkMirror lock-after-use and GondorDenethor_forFortress DenethorFixes USER_9 aura.
TEST_CASE("LOCAL-82 Groups 4 & 5: GaladrielDarkMirror single-use lock and GondorDenethor_forFortress USER_9 aura")
{
	ennorathscenario::World w;
	if (!w.load("LOCAL-82 Groups 4 & 5"))
	{
		return;
	}
	structtest::Arena arena(w.shared, "FactionMordor", "FactionMen");
	ScopedShroud shroud(arena.logic);

	// Group 4: GaladrielDarkMirror starts with GaladrielDarkMirrorCommandSet (has WardByMirror); casting Vision grants Upgrade_GaladrielMirrorUsed -> GaladrielDarkMirrorUsedCommandSet.
	Object *mirror = w.place(arena, "GaladrielDarkMirror", 0, 200.0f, 200.0f);
	stepFrames(arena.logic, 2);
	CHECK(mirror->getCommandSetName() == "GaladrielDarkMirrorCommandSet");

	const SpecialPowerTemplate *spVision = TheSpecialPowerStore->findSpecialPowerTemplate("SpecialAbilityGaladrielMirrorVision");
	REQUIRE(spVision != nullptr);
	REQUIRE(SpecialPowerModules::doSpecialPowerAtLocation(*mirror, spVision, Coord3D{ 250.0f, 200.0f, 0.0f }, 0, false));
	stepFrames(arena.logic, 15);
	CHECK(mirror->getCommandSetName() == "GaladrielDarkMirrorUsedCommandSet");

	// A fresh Mirror can cast WardByMirror directly before using Vision.
	Object *mirror2 = w.place(arena, "GaladrielDarkMirror", 0, 400.0f, 200.0f);
	stepFrames(arena.logic, 2);
	const SpecialPowerTemplate *spWard = TheSpecialPowerStore->findSpecialPowerTemplate("SpecialAbilityGaladrielMirrorWardByMirror");
	REQUIRE(spWard != nullptr);
	REQUIRE(SpecialPowerModules::doSpecialPowerAtLocation(*mirror2, spWard, Coord3D{ 450.0f, 200.0f, 0.0f }, 0, false));
	stepFrames(arena.logic, 5);

	// Group 5: GondorDenethor_forFortress alone has no USER_9 and can cast SpecialAbilitySchergendesGroorks on a GondorFarm; a second Denethor applies DenethorFixes (USER_9).
	Object *den1 = w.place(arena, "GondorDenethor_forFortress", 0, 200.0f, 600.0f);
	Object *farm = w.place(arena, "GondorFarm", 0, 300.0f, 600.0f);
	giveUpgrade(*den1, "Upgrade_RaiseShield");
	stepFrames(arena.logic, 15);
	const int kUser9 = CombatNames::modelCondition("USER_9");
	CHECK_FALSE(den1->testModelCondition(kUser9));

	const SpecialPowerTemplate *spSteuer = TheSpecialPowerStore->findSpecialPowerTemplate("SpecialAbilitySchergendesGroorks");
	REQUIRE(spSteuer != nullptr);
	CHECK(ActionManager::canDoSpecialPowerAtLocation(*den1, *farm->getPosition(), spSteuer, 0));
	REQUIRE(SpecialPowerModules::doSpecialPowerAtLocation(*den1, spSteuer, *farm->getPosition(), 0, false));

	// Second Denethor triggers ModuleTag_LeadershipFix (DenethorFixes -> ModelCondition = USER_9).
	(void)w.place(arena, "GondorDenethor_forFortress", 0, 260.0f, 600.0f);
	stepFrames(arena.logic, 20);
	CHECK(den1->testModelCondition(kUser9));
}
