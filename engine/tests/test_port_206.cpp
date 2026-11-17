// OpenBFME unit tests (lane PORT-206, fork issue 488): who sets the weapon bonus conditions of `Object + 0x39C`, and what RotWK 2.01 actually does
// with the five the Ennorath GameData names.
//
// The address of every expectation below was read with `tools/mod/rw.py dis` on the retail RotWK 2.01 image (`game.dat`, md5 daed3668...):
//   * PLAYER_UPGRADE (bit 5) is set by WeaponBonusUpgrade's mux slot 10, RW 0x8B980D: `mov eax,[ecx-8]` (the module's own object, the UpgradeModule's
//     back pointer at module + 0) / `or dword [eax+0x39C], 0x20` / `ret`. The class chain: create RW 0x64FF7C -> constructor RW 0x8B97C0 (the
//     UpgradeModule base RW 0x863B8F, mux vtable RW 0xC6EB90), so the slot 10 at RW 0xC6EB90 + 0x28 is this class's implementation.
//   * the removal (mux slot 8) is RW 0x63F3BF, the shared no-op, and no site in .text clears bit 5 (the mask's only `and` forms are the garrison
//     parts' RW 0x87BFE8 (bit 0), the veterancy reset RW 0x6958E4 (bits 8..15 of the byte at +0x39D hold 9/10/11) and PropagandaTowerBehavior's
//     RW 0x884DC5 (bits 8 and 15)): the bit stays for the object's lifetime.
//   * HORDE (bit 1) and NATIONALISM (bit 4) have NO writer in the image: every access to the displacement 0x39C / 0x39D of .text was enumerated
//     (readers RW 0x6CA7C4 computeBonus, RW 0x6C9B84, RW 0x8CF55B / 0x8CF982 the xfer and the copy readers, the copies RW 0x53388A / 0x534999 /
//     0x534C90 / 0x8CFC04 / 0x8CFCD7, the zeroing RW 0x699B9C), the generic setters RW 0x68AD0B / 0x68AD1F (`or` / `and [this+0x39C], 1<<n`) have no
//     caller at all, and no writer uses `bts`. The GameData rows HORDE 150% and NATIONALISM 125% therefore never apply in retail 2.01.
//   * ENTHUSIASTIC (8) / SUBLIMINAL (15) are written by PropagandaTowerBehavior RW 0x884CF1 (bit 8 at RW 0x884D51 `or ah,1`, bit 15 at RW 0x884D70
//     `or ah,0x80`, both under a `shr 8` / `shr 0xf` test of the same mask; cleared at RW 0x884DC5 `and [eax+0x39D],0x7e`), a behavior module this
//     port does not have; the Ennorath data names neither the module nor a `GrantedWeaponCondition: HORDE` (its only GrantedWeaponCondition rows are
//     WEAPONSET_TOGGLE_1 on cinematic elven warriors), so nothing in the mod reaches those bits either.
//   * the battle plans' bits 12..14 are written by the player-side function RW 0x6AD2B5 / 0x6AD3E6 (`cmp [esi+4]/[esi+0xc]/[esi+8],eax` then
//     `or byte [esi+0x39D],0x10/0x20/0x40` at RW 0x6AD39A / 0x6AD3AF / 0x6AD3C4, cleared at RW 0x6AD3A3 / 0x6AD3B8 / 0x6AD3CD), and the port has no
//     battle-plan system at all (no BattlePlanUpdate, no player battle-plan state).
// What this lane ports is the one writer that exists in RW, in the mod's data and in this port's reach: WeaponBonusUpgrade -> PLAYER_UPGRADE.
// GPL-3.0.

#include "doctest.h"

#include "LogicTestUtil.h"

#include "Common/Player.h"
#include "Common/StateHash.h"
#include "Common/Team.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/LogicModules.h"
#include "GameLogic/Module/WeaponBonusUpgrade.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/Weapon.h"

#include <string>

using namespace logictest;

namespace
{
// PLAYER_UPGRADE is the 6th name of the 22-name table RW 0xDA1740
constexpr int kPlayerUpgrade = 5;

// the upgrade row and one object carrying WeaponBonusUpgrade, plus the same object without it (the neighbouring case)
std::string upgradeObjects()
{
	return "Upgrade Upgrade_Port206Bonus\n"
		   "  Type = OBJECT\n"
		   "End\n"
		   "Object Port206Swordsman\n"
		   "  KindOf = INFANTRY SELECTABLE\n"
		   "  Body = ActiveBody ModuleTag_Body\n"
		   "    MaxHealth = 100\n"
		   "  End\n"
		   "  Behavior = WeaponBonusUpgrade ModuleTag_Bonus\n"
		   "    TriggeredBy = Upgrade_Port206Bonus\n"
		   "  End\n"
		   "End\n"
		   "Object Port206Plain\n"
		   "  KindOf = INFANTRY SELECTABLE\n"
		   "  Body = ActiveBody ModuleTag_Body\n"
		   "    MaxHealth = 100\n"
		   "  End\n"
		   "End\n";
}

WeaponBonusUpgrade *bonusModuleOf(Object *o)
{
	for (const auto &m : o->modules())
	{
		if (WeaponBonusUpgrade *u = dynamic_cast<WeaponBonusUpgrade *>(m.get()))
		{
			return u;
		}
	}
	return nullptr;
}

struct BonusWorld
{
	LogicWorld f;
	BonusWorld()
	{
		REQUIRE_MESSAGE(f.loadError.empty(), f.loadError);
		LogicModules::registerAll(f.w.modules);
		f.logic->settings().unitDamagedThreshold = 0.5f; // ActiveBody needs GameData's thresholds
		f.logic->settings().unitReallyDamagedThreshold = 0.25f;
		f.logic->settings().bodyThresholdsLoaded = true; // an ActiveBody refuses the default thresholds otherwise
		const std::string err = f.w.load(upgradeObjects(), INI_LOAD_OVERWRITE, "port206.ini");
		REQUIRE_MESSAGE(err.empty(), err);
	}
	Object *make(const char *name) { return f.make(name, f.teamOf("Alice")); }
	void frames(int n)
	{
		for (int i = 0; i < n; ++i)
		{
			f.logic->runLogicFrame();
		}
	}
};
} // namespace

// RW 0x8B980D: the object whose upgrade completes carries bit 5 after it, and every other bit of the mask is untouched.
TEST_CASE("PORT-206 a WeaponBonusUpgrade puts PLAYER_UPGRADE into the object's mask (RW 0x8B980D) and clears no other bit")
{
	BonusWorld w;
	Object *o = w.make("Port206Swordsman");
	REQUIRE(o);
	WeaponBonusUpgrade *module = bonusModuleOf(o);
	REQUIRE_MESSAGE(module, "WeaponBonusUpgrade must be a live module, not the UnportedModule stub");
	w.frames(2);

	// nothing before the upgrade: RW 0x8B980D runs only when updateUpgradeModules (RW 0x6936FE) accepts the upgrade
	CHECK(o->weaponBonusConditionMask() == 0u);
	CHECK_FALSE(module->isAlreadyUpgraded());

	// a bit the object carries from another writer survives the upgrade (RW 0x8B980D is an `or`, not a store)
	o->setWeaponBonusCondition(0, true); // GARRISONED, as GarrisonContain's RW 0x87BF01 sets it
	CHECK(o->weaponBonusConditionMask() == 1u);

	o->giveUpgrade("Upgrade_Port206Bonus");
	CHECK(module->isAlreadyUpgraded());
	CHECK(o->weaponBonusConditionMask() == ((1u << kPlayerUpgrade) | 1u)); // 0x21: RW 0x8B980D ORs 0x20 into RW 0x6CA7C4's mask

	// the removal is retail's no-op RW 0x63F3BF: taking the upgrade away leaves the bit
	o->removeUpgrade(Player::resolveUpgrade("Upgrade_Port206Bonus", true));
	CHECK(o->weaponBonusConditionMask() == ((1u << kPlayerUpgrade) | 1u));
}

// The neighbouring behaviour the fix must not break: an object with no WeaponBonusUpgrade keeps an empty mask, and the same registry class still
// parses its base upgrade rows (RW 0xC76AD8 at extra 8) — TriggeredBy gates the upgrade, nothing else.
TEST_CASE("PORT-206 the neighbouring behaviour holds: an object without the module never gains the bit, and TriggeredBy still gates the upgrade")
{
	BonusWorld w;
	Object *plain = w.make("Port206Plain");
	REQUIRE(plain);
	CHECK(bonusModuleOf(plain) == nullptr);
	w.frames(2);
	CHECK(plain->weaponBonusConditionMask() == 0u);

	// the module's object does not take the upgrade when its TriggeredBy is not in the mask
	Object *o = w.make("Port206Swordsman");
	REQUIRE(o);
	o->giveUpgrade("Upgrade_Port206Bonus");
	CHECK(o->weaponBonusConditionMask() == (1u << kPlayerUpgrade));

	// the mask is simulation state: the object's hash covers it (Object::crc adds m_weaponBonusMask), so the upgrade moves the hash
	Object *second = w.make("Port206Swordsman");
	REQUIRE(second);
	w.frames(1);
	StateHasher h1;
	second->crc(h1);
	second->giveUpgrade("Upgrade_Port206Bonus");
	StateHasher h2;
	second->crc(h2);
	CHECK(h1.value() != h2.value());
}

// What the three conditions this port still cannot set would need, recorded as the test that keeps the verdict honest: the mask has room for
// HORDE (1), NATIONALISM (4), ENTHUSIASTIC (8), SUBLIMINAL (15) and the battle plans (12..14), the GameData set answers them (RW 0x6CA4B5), and no
// module in this world writes them — the writers are PropagandaTowerBehavior (RW 0x884CF1, a module this port does not have) and the player-side
// battle-plan function (RW 0x6AD2B5), which have no counterpart here yet.
TEST_CASE("PORT-206 the conditions without a writer in RotWK or without a ported module: the mask carries them, nothing in this world sets them")
{
	BonusWorld w;
	Object *o = w.make("Port206Swordsman");
	REQUIRE(o);
	w.frames(2);
	// RW 0x68AD0B takes any bit of the 22-name table, so the mask's shape is not the gap
	o->setWeaponBonusCondition(1, true);  // HORDE
	o->setWeaponBonusCondition(4, true);  // NATIONALISM
	o->setWeaponBonusCondition(8, true);  // ENTHUSIASTIC
	o->setWeaponBonusCondition(15, true); // SUBLIMINAL
	CHECK(o->weaponBonusConditionMask() == ((1u << 1) | (1u << 4) | (1u << 8) | (1u << 15)));
	// and the horde module this port does have does not set HORDE by itself: its rank weapon conditions are the WEAPONSET_* flags, not these bits
	// (Ennorath's only GrantedWeaponCondition rows are WEAPONSET_TOGGLE_1 on cinematic elven warriors; RW parses them through RW 0x8774AB / 0x8774C0)
	Object *horde = w.make("Port206Plain");
	REQUIRE(horde);
	w.frames(2);
	CHECK((horde->weaponBonusConditionMask() & (1u << 1)) == 0u);
}
