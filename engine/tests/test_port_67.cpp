// OpenBFME unit tests: PORT-67 (issue #181) — Weapon/CanSwoop and the Locomotor Swoop rows (the Fell Beast's dive).
// GPL-3.0.
//
// ROTWK VERDICT (game.dat 2.01, md5 daed3668006cd90f01c34e5a7da1901f, caveat S-001): both are parsed and never read.
//   Weapon: `CanSwoop` is the byte at weapon + 0x83 of the field table RW 0xC16DD8 (parser RW 0x42E558, the bool
//   parser the neighbours HoldDuringReload +0x80 / CanFireWhileMoving +0x81 / CanFireWhileCharging +0x82 share).
//   Its getter RW 0x6CA152 (`mov al, byte ptr [ecx + 0x83]; ret`) has no caller, unlike its neighbours RW 0x6CA144
//   (+0x126, 3 sites) and RW 0x6CA14B (+0x81, 4 sites); of the nine byte reads of disp 0x83 in the whole .text none
//   is a weapon template read (docs/ennorath/port67-swoop-rw.md lists all nine with their classes).
//   Locomotor: `SwoopStandoffRadius` +0x124, `SwoopStandoffHeight` +0x128, `SwoopTerminalVelocity` +0x12C,
//   `SwoopAccelerationRate` +0x130 (and `SwoopSpeedTuningFactor` +0x134, absent from the audit) of the table
//   RW 0xBF4478 are touched only by the template's ctor RW 0x5E4326, its copy ctor RW 0x5E4653 and its assignment
//   RW 0x5E31DF; the six functions that consume a LocomotorTemplate through the Locomotor's +4 pointer
//   (RW 0x5E357E, 0x5E372D, 0x5E3796, 0x5E3C56, 0x5E40AD, 0x5F2F55) read Speed +0x20, TurnTime +0x2C,
//   Acceleration +0x44 and MinTurnSpeed +0x54 and never the swoop rows, and the giant-bird movers RW 0x5E91E2 /
//   RW 0x5E4CD3 (ported in LocomotorGiantBird.cpp) take PreferredHeight +0x48, Lift +0xA0 and the lift limit.
// The tests below therefore pin what RotWK has: the parse, the ctor constants and the fact that a template that
// differs only in these fields is otherwise identical (a reader wired in later breaks them).

#include "doctest.h"
#include "IniTestUtil.h"

#include "GameLogic/Locomotor.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/WeaponStores.h"

#include <cmath>
#include <string>

using namespace initest;

namespace
{
struct World
{
	Fixture fx;
	WeaponStores stores;
	LocomotorStore locomotors;
	LocomotorStore *savedLocomotors;

	World()
	{
		stores.install();
		savedLocomotors = TheLocomotorStore;
		TheLocomotorStore = &locomotors;
		fx.env.blocks.registerBlock("Weapon", [](INI *ini) { WeaponStore::parseWeaponTemplateDefinitionGlobal(ini); });
		fx.env.blocks.registerBlock("Locomotor", [](INI *ini) { LocomotorStore::parseLocomotorTemplateDefinitionGlobal(ini); });
	}
	~World() { TheLocomotorStore = savedLocomotors; }

	std::string load(const std::string &text) { return loadError(fx.env, "port67.ini", text); }
	const WeaponTemplate *weapon(const char *name)
	{
		const WeaponTemplate *t = stores.weapons().findWeaponTemplate(name);
		REQUIRE_MESSAGE(t != nullptr, name);
		return t;
	}
	const LocomotorTemplate *locomotor(const char *name)
	{
		const LocomotorTemplate *t = locomotors.findLocomotorTemplate(name);
		REQUIRE_MESSAGE(t != nullptr, name);
		return t;
	}
};

const char *const kLocomotorCommon = "  Surfaces = GROUND\n  TurnTime = 500\n  Acceleration = 510\n  Braking = 510\n"
                                     "  ZAxisBehavior = NO_Z_MOTIVE_FORCE\n  CloseEnoughDist = 10\n  Appearance = GIANT_BIRD\n";
}

// RW 0xC16DD8 puts CanSwoop in the byte after CanFireWhileCharging (+0x82) and before the float WeaponRecoil (+0x84).
// A slot mistake (bool written over the float, or one byte off) shows up here.
TEST_CASE("PORT-67 weapon: CanSwoop is weapon + 0x83 (table RW 0xC16DD8, parser RW 0x42E558) and leaves its neighbours alone")
{
	World w;
	const std::string err = w.load(
		"Weapon Port67NoSwoop\n"
		"  AttackRange = 100\n"
		"  HoldDuringReload = Yes\n"
		"  CanFireWhileMoving = No\n"
		"  CanFireWhileCharging = No\n"
		"  WeaponRecoil = 2.5\n"
		"End\n"
		"Weapon Port67Swoop\n"
		"  AttackRange = 100\n"
		"  HoldDuringReload = Yes\n"
		"  CanFireWhileMoving = No\n"
		"  CanFireWhileCharging = No\n"
		"  CanSwoop = Yes\n"
		"  WeaponRecoil = 2.5\n"
		"End\n");
	REQUIRE_MESSAGE(err.empty(), err);

	const WeaponTemplate *plain = w.weapon("Port67NoSwoop");
	const WeaponTemplate *swoop = w.weapon("Port67Swoop");
	CHECK_FALSE(plain->m_canSwoop); // absent: false, the bool was not written
	CHECK(swoop->m_canSwoop);       // `CanSwoop = Yes` lands in the field
	// the byte slots the table puts around +0x83 and the float that follows it
	CHECK(swoop->m_holdDuringReload == plain->m_holdDuringReload);
	CHECK(swoop->m_holdDuringReload);
	CHECK(swoop->m_canFireWhileMoving == plain->m_canFireWhileMoving);
	CHECK_FALSE(swoop->m_canFireWhileMoving);
	CHECK(swoop->m_canFireWhileCharging == plain->m_canFireWhileCharging);
	CHECK_FALSE(swoop->m_canFireWhileCharging);
	CHECK(swoop->m_weaponRecoil == 2.5f); // +0x84 still the recoil float, not a bool
	CHECK(plain->m_weaponRecoil == 2.5f);
	CHECK(swoop->m_attackRange == 100.0f);
}

// RW 0x5E4326 (the LocomotorTemplate ctor) stores RW 0xBE4170 (200.0) into +0x124 and +0x128, RW 0xBF4A2C (0.07) into
// +0x12C, RW 0xBF4A28 (0.003) into +0x130 and RW 0xBD1908 (1.0) into +0x134. An INI without the rows keeps them.
TEST_CASE("PORT-67 locomotor: the Swoop rows +0x124..+0x134 default to the ctor's constants (RW 0x5E4326)")
{
	World w;
	const std::string err = w.load(std::string("Locomotor Port67Plain\n") + kLocomotorCommon + "End\n");
	REQUIRE_MESSAGE(err.empty(), err);

	const LocomotorTemplate *t = w.locomotor("Port67Plain");
	CHECK(t->m_swoopStandoffRadius == 200.0f);   // RW 0xBE4170
	CHECK(t->m_swoopStandoffHeight == 200.0f);   // RW 0xBE4170
	CHECK(t->m_swoopTerminalVelocity == 0.07f);  // RW 0xBF4A2C
	CHECK(t->m_swoopAccelerationRate == 0.003f); // RW 0xBF4A28
	CHECK(t->m_swoopSpeedTuningFactor == 1.0f);  // RW 0xBD1908
}

// The four rows of the issue (plus the fifth row the audit does not list) parse where RW 0xBF4478 puts them, and a
// template that differs only in them carries every other field unchanged: nothing in the port consults them, as in RW.
TEST_CASE("PORT-67 locomotor: the Swoop rows parse at RW 0xBF4478's offsets and nothing else moves")
{
	World w;
	const std::string err = w.load(
		std::string("Locomotor Port67Plain2\n") + kLocomotorCommon + "End\n" +
		std::string("Locomotor Port67Swoop\n") + kLocomotorCommon +
		"  SwoopStandoffRadius = 111\n"
		"  SwoopStandoffHeight = 222\n"
		"  SwoopTerminalVelocity = 0.5\n"
		"  SwoopAccelerationRate = 0.25\n"
		"  SwoopSpeedTuningFactor = 3\n"
		"End\n");
	REQUIRE_MESSAGE(err.empty(), err);

	const LocomotorTemplate *plain = w.locomotor("Port67Plain2");
	const LocomotorTemplate *swoop = w.locomotor("Port67Swoop");
	CHECK(swoop->m_swoopStandoffRadius == 111.0f);   // +0x124
	CHECK(swoop->m_swoopStandoffHeight == 222.0f);   // +0x128
	CHECK(swoop->m_swoopTerminalVelocity == 0.5f);   // +0x12C
	CHECK(swoop->m_swoopAccelerationRate == 0.25f);  // +0x130
	CHECK(swoop->m_swoopSpeedTuningFactor == 3.0f);  // +0x134

	// the neighbours of the row block (RW: +0x120 AeleronCorrectionRate and +0x138 BackingUpSpeed) and the fields the
	// movers actually read (SW: +0x20 Speed, +0x2C TurnTime, +0x44 Acceleration, +0x54 MinTurnSpeed, +0x48
	// PreferredHeight, +0xA0 Lift) are identical: the swoop rows move no motion value.
	CHECK(swoop->m_aeleronCorrectionRate == plain->m_aeleronCorrectionRate);
	CHECK(swoop->m_backingUpSpeed == plain->m_backingUpSpeed);
	CHECK(swoop->m_maxSpeed == plain->m_maxSpeed); // +0x20 Speed
	CHECK(swoop->m_turnTime == plain->m_turnTime);
	CHECK(swoop->m_acceleration == plain->m_acceleration);
	CHECK(swoop->m_minTurnSpeed == plain->m_minTurnSpeed);
	CHECK(swoop->m_preferredHeight == plain->m_preferredHeight);
	CHECK(swoop->m_lift == plain->m_lift);
	CHECK(swoop->m_appearance == plain->m_appearance);
	CHECK(swoop->m_zAxisBehavior == plain->m_zAxisBehavior);
}
