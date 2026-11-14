// OpenBFME unit tests — Local 45 (issue #401: P-35, P-36, P-85). GPL-3.0.
//
// Editions: both (RotWK 2.01 and BFME2 1.06).
// - P-36: hudtest::Rig (HudTestUtil.h) and mod2test::RetailGame (Mod2TestUtil.h) must enter their SharedWorld's
//   RetailObjectWorld::ContextScope for their lifetime, and shared static world loaders (test_bfme2mode0.cpp,
//   EnnorathScenario.h, StartTestUtil.cpp, P16TestUtil.h) must scope their initial load so a BFME2 1.06 world
//   (which has no Upgrade_AngmarFaction) never leaks into TheUpgradeCenter across tests when Player::xfer
//   (RW 0x6AD910, DONOR BFME2 0x6A3A80) calls TheUpgradeCenter->findUpgrade (RW 0x66F81F) during loadSnapshot.
// - P-85: AIGuardRetaliateMachine::getStdGuardRange (RW 0x94BA8C, DONOR BFME2 0x945239) queries
//   Object::getCurrentWeapon(nullptr) (RW 0x68B58C) and WeaponTemplate::isMeleeWeapon (RW 0x441B59, offset +0x125);
//   when non-zero it multiplies MaxRetaliateDistance (+0xC8 in AIData) by 1.15f (RW 0x94BACE, float at 0xC12234).
//   In both retail and Ennorath (data\ini\object\goodfaction\hordes\men\menhordes.ini, data\ini\weapon.ini),
//   GondorFighterHorde has PRIMARY NormalMeleeHordeRangefinder (MeleeWeapon = Yes -> 250 * 1.15 = 287.5),
//   whereas GondorArcherHorde has PRIMARY GoodArcherMissileHordeRangefinder (no MeleeWeapon = Yes -> 250.0).

#include "doctest.h"

#include "EnnorathScenario.h"
#include "HudTestUtil.h"
#include "Mod2TestUtil.h"
#include "RetailTestMount.h"
#include "StructureArena.h"

#include "Common/Upgrade.h"
#include "GameLogic/AI/AIGuard.h"
#include "GameLogic/Combat/ObjectWeapons.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/WeaponState.h"

namespace
{
struct ScopedBfme2World
{
	retailtest::Mount *mount = nullptr;
	std::unique_ptr<RetailObjectWorld> world;
	std::string error;

	bool ensureLoadedScoped()
	{
		mount = retailtest::bfme2Mount();
		if (!mount || !mount->fs)
		{
			return false;
		}
		if (!world)
		{
			world = std::make_unique<RetailObjectWorld>(*mount->fs);
			bool ok = false;
			{
				const auto loadContext = world->enterContext();
				ok = world->load(&error);
			}
			if (!ok)
			{
				world.reset();
				return false;
			}
		}
		return true;
	}
};

ScopedBfme2World &sharedBfme2ForLocal45()
{
	static ScopedBfme2World b2;
	return b2;
}
} // namespace

TEST_CASE("LOCAL-45 P-36: hudtest::Rig and mod2test::RetailGame select their SharedWorld context so BFME2 1.06 never breaks Angmar snapshot load")
{
	if (!hudtest::haveWorld("LOCAL-45 P-36 snapshot context"))
	{
		return;
	}
	ScopedBfme2World &b2 = sharedBfme2ForLocal45();
	if (!b2.ensureLoadedScoped())
	{
		retailtest::printSkip("LOCAL-45 P-36 snapshot context (needs BFME2_INSTALL)");
		return;
	}

	hudtest::SharedWorld &s = hudtest::shared();
	{
		const auto rotwkScope = s.world->enterContext();
		REQUIRE(s.world->upgrades().findUpgrade("Upgrade_AngmarFaction") != nullptr);
		REQUIRE(b2.world->upgrades().findUpgrade("Upgrade_AngmarFaction") == nullptr);

		// 1. Scoped BFME2 load must leave the previously active RotWK context intact.
		CHECK(RetailObjectWorld::currentContext() == s.world.get());
		CHECK(TheUpgradeCenter == &s.world->upgrades());
	}

	// 2. Even if a BFME2 1.06 world is the active context when hudtest::Rig or mod2test::RetailGame is constructed,
	// both enter s.world->enterContext() for their lifetime so Player::xfer (RW 0x6AD910 -> RW 0x66F81F) resolves
	// Upgrade_AngmarFaction from RotWK 2.01's UpgradeCenter during logic.loadSnapshot, and restore BFME2 on exit.
	const auto bfme2Active = b2.world->enterContext();
	REQUIRE(RetailObjectWorld::currentContext() == b2.world.get());
	REQUIRE(TheUpgradeCenter == &b2.world->upgrades());

	{
		mod2test::RetailGame rg(s);
		CHECK(RetailObjectWorld::currentContext() == s.world.get());
		CHECK(TheUpgradeCenter == &s.world->upgrades());
	}
	CHECK(RetailObjectWorld::currentContext() == b2.world.get());
	CHECK(TheUpgradeCenter == &b2.world->upgrades());

	{
		hudtest::Rig r(s);
		CHECK(RetailObjectWorld::currentContext() == s.world.get());
		CHECK(TheUpgradeCenter == &s.world->upgrades());

		GameLogic &logic = r.game->logic();
		bool sawAngmarUpgrade = false;
		for (int i = 0; i < r.game->players().getPlayerCount(); ++i)
		{
			if (const Player *p = r.game->players().getNthPlayer(i))
			{
				if (p->hasUpgradeComplete("Upgrade_AngmarFaction"))
				{
					sawAngmarUpgrade = true;
					break;
				}
			}
		}
		REQUIRE(sawAngmarUpgrade);

		std::vector<std::uint8_t> saved;
		std::string err;
		REQUIRE_MESSAGE(logic.saveSnapshot(saved, &err), err);
		r.frame();
		REQUIRE_MESSAGE(logic.loadSnapshot(saved, &err), err);
	}

	CHECK(RetailObjectWorld::currentContext() == b2.world.get());
	CHECK(TheUpgradeCenter == &b2.world->upgrades());
}

TEST_CASE("LOCAL-45 P-85 neighbour: GondorFighterHorde melee rangefinder scales retaliate range by 1.15 while GondorArcherHorde missile rangefinder does not")
{
	ennorathscenario::World world;
	if (!world.load("LOCAL-45 P-85 horde rangefinder retaliate range"))
	{
		return;
	}
	// ennorathscenario::World::load must hold its own context scope while active.
	CHECK(RetailObjectWorld::currentContext() == world.shared.world.get());

	structtest::Arena arena(world.shared, "FactionMen", "FactionMordor");
	std::string err;
	REQUIRE_MESSAGE(GameLogicSettingsLoader::loadAIData(*world.merged.fs, arena.logic.settings(), &err), err);
	REQUIRE(arena.logic.settings().maxRetaliateDistance == 250.0f);

	// 1. GondorFighterHorde: Weapon = PRIMARY NormalMeleeHordeRangefinder (MeleeWeapon = Yes, RW 0x441B59 +0x125 = 1)
	// -> RW 0x94BA8C multiplies 250.0f by 1.15f (RW 0x94BACE) = 287.5f.
	Object *fighterHorde = world.place(arena, "GondorFighterHorde", 0, 500.0f, 500.0f);
	REQUIRE(fighterHorde != nullptr);
	REQUIRE(fighterHorde->getWeapons() != nullptr);
	REQUIRE(fighterHorde->getWeapons()->currentWeapon() != nullptr);
	REQUIRE(fighterHorde->getWeapons()->currentWeapon()->getTemplate() != nullptr);
	CHECK(fighterHorde->getWeapons()->currentWeapon()->getTemplate()->m_name == "NormalMeleeHordeRangefinder");
	CHECK(fighterHorde->getWeapons()->currentWeapon()->getTemplate()->m_meleeWeapon);
	CHECK(AIGuardMachine::getRetaliateRange(*fighterHorde) == 287.5f);

	// 2. Neighbour: GondorArcherHorde: Weapon = PRIMARY GoodArcherMissileHordeRangefinder (no MeleeWeapon = Yes, +0x125 = 0)
	// -> RW 0x94BA8C returns unscaled MaxRetaliateDistance = 250.0f.
	Object *archerHorde = world.place(arena, "GondorArcherHorde", 0, 600.0f, 500.0f);
	REQUIRE(archerHorde != nullptr);
	REQUIRE(archerHorde->getWeapons() != nullptr);
	REQUIRE(archerHorde->getWeapons()->currentWeapon() != nullptr);
	REQUIRE(archerHorde->getWeapons()->currentWeapon()->getTemplate() != nullptr);
	CHECK(archerHorde->getWeapons()->currentWeapon()->getTemplate()->m_name == "GoodArcherMissileHordeRangefinder");
	CHECK_FALSE(archerHorde->getWeapons()->currentWeapon()->getTemplate()->m_meleeWeapon);
	CHECK(AIGuardMachine::getRetaliateRange(*archerHorde) == 250.0f);
}
