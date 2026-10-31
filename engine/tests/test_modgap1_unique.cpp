// OpenBFME. MODGAP-1 tests: the classes unique to Ennorath (no retail 2.01 template uses them; tools/mod/ennorath_unique.py): FreeLifeBody (RW 0x8C5AE6, 0x8C5B86), OneRingPenaltyUpdate
// (RW 0x89D647), EntEnragedUpdate (RW 0x8B6866), GiantBirdSlowDeathBehavior (RW 0x86511C, 0x66A61A). Synthetic data; the expected values are the data's own.
#include "doctest.h"
#include "CombatTestUtil.h"

#include "GameLogic/Damage.h"
#include "GameLogic/Module/EntEnragedUpdate.h"
#include "GameLogic/Module/FreeLifeBody.h"
#include "GameLogic/Module/GiantBirdSlowDeathBehavior.h"
#include "GameLogic/Module/InvisibilityModules.h"
#include "GameLogic/Module/OneRingPenaltyUpdate.h"
#include "GameLogic/Module/PhysicsBehavior.h"
#include "GameLogic/Object/PartitionManager.h"

#include <string>

using namespace combattest;

namespace
{
std::string unitWith(const char *name, const char *kinds, int radius, const std::string &body, const std::string &fields, bool destroyDie = true)
{
	return std::string("Object ") + name + "\n"
		"  KindOf = " + kinds + "\n"
		"  Geometry = CYLINDER\n"
		"  GeometryMajorRadius = " + std::to_string(radius) + "\n"
		"  GeometryMinorRadius = " + std::to_string(radius) + "\n"
		"  GeometryHeight = 20\n"
		"  ArmorSet\n"
		"    Conditions = None\n"
		"    Armor = NoArmor\n"
		"  End\n" + body +
		(destroyDie ? "  Behavior = DestroyDie ModuleTag_Destroy\n  End\n" : "") + fields +
		"End\n";
}

const char kPlainBody[] = "  Body = ActiveBody ModuleTag_Body\n    MaxHealth = 1000\n  End\n";
const char kAI[] = "  Behavior = AIUpdateInterface ModuleTag_AI\n  End\n";
const char kUnitKinds[] = "INFANTRY SELECTABLE SCORE";

std::string freeLifeBody(const char *extra)
{
	return std::string("  Body = FreeLifeBody ModuleTag_Body\n    MaxHealth = 1000\n    FreeLifeHealthPercent = 50%\n    FreeLifeTime = 10000\n") + extra + "  End\n";
}

const std::string kFreeLifeObjects =
	unitWith("Hero", kUnitKinds, 8, freeLifeBody(""), kAI) +
	unitWith("HeroInvincible", kUnitKinds, 8, freeLifeBody("    FreeLifeInvincible = Yes\n    FreeLifeAnimAndDuration = AnimState:RESURRECTED AnimTime:2000\n"), kAI) +
	unitWith("HeroLocked", kUnitKinds, 8, freeLifeBody("    FreeLifePrerequisiteUpgrade = Upgrade_NobodyHasThis\n"), kAI);

void hit(CombatWorld &w, Object *victim, float amount)
{
	(void)w;
	DamageInfo info;
	info.m_input.m_damageType = DAMAGE_SLASH;
	info.m_input.m_deathType = DEATH_NORMAL;
	info.m_input.m_amount = amount;
	victim->attemptDamage(info);
}

const std::string kEntObjects =
	unitWith("Ent", kUnitKinds, 8, kPlainBody, std::string(kAI) +
		"  Behavior = EntEnragedUpdate ModuleTag_Rage\n"
		"    HatedObjectFilter = NONE ENEMIES +HatedMill\n"
		"    FriendlyDeadFilter = NONE +FallenFriend\n"
		"    ScanDistance = 100\n"
		"    EnragedTime = 1000\n"
		"    TimeUntilCanRageAgain = 2000\n"
		"    EnragedTransitionTime = 400\n"
		"  End\n") +
	unitWith("HatedMill", "STRUCTURE SELECTABLE", 8, kPlainBody, "") +
	// its body stays in the world after the death: what RW's dead-friend scan sees
	unitWith("FallenFriend", kUnitKinds, 8, kPlainBody, "  Behavior = SlowDeathBehavior ModuleTag_Death\n    DeathTypes = ALL\n    DestructionDelay = 20000\n  End\n", false);

const unsigned kStatusEnraged = 0x36;

EntEnragedUpdate *entOf(Object *o)
{
	return dynamic_cast<EntEnragedUpdate *>(o->findModule("EntEnragedUpdate"));
}

const std::string kRingObjects =
	unitWith("Frodo", kUnitKinds, 8, kPlainBody, std::string(kAI) +
		"  Behavior = StealthUpdate ModuleTag_Stealth\n"
		"    OneRingDelayOn = 200\n"
		"    OneRingDelayOff = 200\n"
		"    RingAnimTimeOn = 200\n"
		"    RingAnimTimeOff = 200\n"
		"    RingDelayAfterRemoving = 2000\n"
		"  End\n"
		"  Behavior = OneRingPenaltyUpdate ModuleTag_Ring\n"
		"    SpecialObjectName = TheEye\n"
		"    RingTimeBeforeSpawning = 2000\n"
		"    TimeSpentRoamingAround = 2000\n"
		"    TimeRingPowerSuppressed = 6000\n"
		"    StartingDistanceFromMe = 100\n"
		"    TimeFrozenFromPenalty = 1000\n"
		"  End\n") +
	unitWith("TheEye", kUnitKinds, 8, kPlainBody, kAI);

const unsigned kStatusCanStealth = 0x12;

const std::string kBirdObjects =
	unitWith("Flyer", kUnitKinds, 8, kPlainBody, std::string(kAI) +
		"  Behavior = PhysicsBehavior ModuleTag_Physics\n"
		"  End\n"
		"  Behavior = GiantBirdSlowDeathBehavior ModuleTag_Death\n"
		"    DeathTypes = ALL\n"
		"    DestructionDelay = 20000\n"
		"    CrashAvoidKindOfs = STRUCTURE\n"
		"    CrashAvoidRadius = 200\n"
		"  End\n") +
	unitWith("Block", "STRUCTURE SELECTABLE", 30, kPlainBody, "");
} // namespace

TEST_CASE("modgap1 FreeLifeBody: a lethal hit restores FreeLifeHealthPercent of the maximum; another within FreeLifeTime kills (RW 0x8C5B86)")
{
	CombatWorld w(kFreeLifeObjects.c_str());
	Object *hero = w.unit("Hero", 'A', 400, 400);
	REQUIRE(hero != nullptr);
	FreeLifeBody *body = dynamic_cast<FreeLifeBody *>(hero->getBodyModule());
	REQUIRE(body != nullptr);
	CHECK_FALSE(body->used());
	hit(w, hero, 5000.0f);
	CHECK(body->used());
	CHECK_FALSE(hero->isEffectivelyDead());
	CHECK(hero->getBodyModule()->getHealth() == 500.0f);
	w.frames(3);
	hit(w, hero, 5000.0f); // 3 frames later, FreeLifeTime is 50
	CHECK(hero->isEffectivelyDead());
}

TEST_CASE("modgap1 FreeLifeBody: the second free life comes once FreeLifeTime has passed (RW 0x8C5C07 .. 0x8C5C5C)")
{
	CombatWorld w(kFreeLifeObjects.c_str());
	Object *hero = w.unit("Hero", 'A', 400, 400);
	hit(w, hero, 5000.0f);
	w.frames(51); // FreeLifeTime 10000 ms = 50 frames: strictly more
	hit(w, hero, 5000.0f);
	CHECK_FALSE(hero->isEffectivelyDead());
	CHECK(hero->getBodyModule()->getHealth() == 500.0f);
}

TEST_CASE("modgap1 FreeLifeBody: without the prerequisite upgrade the hit kills (RW 0x8C5BCD)")
{
	CombatWorld w(kFreeLifeObjects.c_str());
	Object *hero = w.unit("HeroLocked", 'A', 400, 400);
	hit(w, hero, 5000.0f);
	CHECK(hero->isEffectivelyDead());
}

TEST_CASE("modgap1 FreeLifeBody: FreeLifeInvincible ignores hits while the animation's condition holds (RW 0x8C5AE6)")
{
	CombatWorld w(kFreeLifeObjects.c_str());
	Object *hero = w.unit("HeroInvincible", 'A', 400, 400);
	hit(w, hero, 5000.0f);
	REQUIRE(hero->getBodyModule()->getHealth() == 500.0f);
	hit(w, hero, 100.0f); // the condition RESURRECTED lasts AnimTime 2000 ms
	CHECK(hero->getBodyModule()->getHealth() == 500.0f);
	w.frames(12);
	hit(w, hero, 100.0f);
	CHECK(hero->getBodyModule()->getHealth() == 400.0f);
}

TEST_CASE("modgap1 EntEnragedUpdate: nothing near, no rage; a hated enemy building near starts it, it ends after EnragedTime and comes back only after TimeUntilCanRageAgain (RW 0x8B6866)")
{
	CombatWorld w(kEntObjects.c_str());
	w.logic->partition().setRegion(0.0f, 0.0f, 1000.0f, 1000.0f);
	Object *ent = w.unit("Ent", 'A', 400, 400);
	REQUIRE(ent != nullptr);
	EntEnragedUpdate *rage = entOf(ent);
	REQUIRE(rage != nullptr);
	w.unit("HatedMill", 'B', 800, 800); // out of ScanDistance
	w.frames(3);
	CHECK_FALSE(rage->enraged());
	Object *mill = w.unit("HatedMill", 'B', 450, 400);
	REQUIRE(mill != nullptr);
	w.frames(2);
	REQUIRE(rage->enraged());
	CHECK(ent->testStatus(kStatusEnraged));
	mill->kill(DAMAGE_UNRESISTABLE, DEATH_NORMAL);
	w.frames(40); // past EnragedTime (5 frames)
	CHECK_FALSE(rage->enraged());
	CHECK_FALSE(ent->testStatus(kStatusEnraged));
}

TEST_CASE("modgap1 EntEnragedUpdate: below half health it rages; suppressed it does not, forced it does (RW 0x8B697C, 0x8B64B8, 0x8B649F)")
{
	CombatWorld w(kEntObjects.c_str());
	w.logic->partition().setRegion(0.0f, 0.0f, 1000.0f, 1000.0f);
	Object *ent = w.unit("Ent", 'A', 400, 400);
	EntEnragedUpdate *rage = entOf(ent);
	REQUIRE(rage != nullptr);
	rage->setSuppressed(true);
	hit(w, ent, 600.0f);
	w.frames(3);
	CHECK_FALSE(rage->enraged());
	rage->setSuppressed(false);
	w.frames(3);
	CHECK(rage->enraged());

	Object *other = w.unit("Ent", 'A', 700, 700);
	EntEnragedUpdate *forced = entOf(other);
	w.frames(2);
	REQUIRE_FALSE(forced->enraged());
	forced->setForced(true, false);
	w.frames(2);
	CHECK(forced->enraged());
}

TEST_CASE("modgap1 EntEnragedUpdate: a dead friend near starts the rage (RW 0x8B66CE)")
{
	CombatWorld w(kEntObjects.c_str());
	w.logic->partition().setRegion(0.0f, 0.0f, 1000.0f, 1000.0f);
	Object *ent = w.unit("Ent", 'A', 400, 400);
	Object *friendUnit = w.unit("FallenFriend", 'A', 420, 400);
	EntEnragedUpdate *rage = entOf(ent);
	REQUIRE(rage != nullptr);
	w.frames(2);
	REQUIRE_FALSE(rage->enraged());
	friendUnit->kill(DAMAGE_UNRESISTABLE, DEATH_NORMAL);
	REQUIRE(friendUnit->isEffectivelyDead());
	w.frames(2);
	CHECK(rage->enraged());
}

TEST_CASE("modgap1 OneRingPenaltyUpdate: the ring worn too long calls the Eye, then the penalty takes the ring off and suppresses it for a while (RW 0x89D647, 0x89D2AC, 0x89D41B)")
{
	CombatWorld w(kRingObjects.c_str());
	w.logic->partition().setRegion(0.0f, 0.0f, 1000.0f, 1000.0f);
	Object *frodo = w.unit("Frodo", 'A', 400, 400);
	REQUIRE(frodo != nullptr);
	StealthUpdate *stealth = StealthUpdate::of(*frodo);
	OneRingPenaltyUpdate *ring = dynamic_cast<OneRingPenaltyUpdate *>(frodo->findModule("OneRingPenaltyUpdate"));
	REQUIRE(stealth != nullptr);
	REQUIRE(ring != nullptr);
	stealth->toggleRing();
	REQUIRE(stealth->ringWorn());
	ObjectID eye = INVALID_ID;
	for (int i = 0; i < 40 && eye == INVALID_ID; ++i)
	{
		w.frames(1);
		eye = ring->eye();
	}
	REQUIRE(eye != INVALID_ID);
	Object *eyeObject = w.byId(eye);
	REQUIRE(eyeObject != nullptr);
	CHECK(eyeObject->getTemplate()->getName() == "TheEye");
	for (int i = 0; i < 60 && ring->penaltyFrame() == 0; ++i)
	{
		w.frames(1);
	}
	REQUIRE(ring->penaltyFrame() != 0);
	CHECK(ring->eye() == INVALID_ID);
	CHECK_FALSE(frodo->testStatus(kStatusCanStealth));
	w.frames(10); // OneRingDelayOff, then the module's next look (every 5 frames): the ring is off (RW 0x77735D)
	CHECK_FALSE(stealth->ringWorn());
	CHECK(ring->ringFrame() == 0);
	for (int i = 0; i < 40 && ring->penaltyFrame() != 0; ++i)
	{
		w.frames(1);
	}
	CHECK(ring->penaltyFrame() == 0); // TimeRingPowerSuppressed over: RW 0x89D3E4
	CHECK(frodo->testStatus(kStatusCanStealth));
}

TEST_CASE("modgap1 OneRingPenaltyUpdate: taking the ring off in time removes the Eye and pays no penalty (RW 0x89D6B4)")
{
	CombatWorld w(kRingObjects.c_str());
	w.logic->partition().setRegion(0.0f, 0.0f, 1000.0f, 1000.0f);
	Object *frodo = w.unit("Frodo", 'A', 400, 400);
	StealthUpdate *stealth = StealthUpdate::of(*frodo);
	OneRingPenaltyUpdate *ring = dynamic_cast<OneRingPenaltyUpdate *>(frodo->findModule("OneRingPenaltyUpdate"));
	REQUIRE(ring != nullptr);
	stealth->toggleRing();
	for (int i = 0; i < 40 && ring->eye() == INVALID_ID; ++i)
	{
		w.frames(1);
	}
	REQUIRE(ring->eye() != INVALID_ID);
	stealth->toggleRing();
	for (int i = 0; i < 30 && ring->eye() != INVALID_ID; ++i)
	{
		w.frames(1);
	}
	CHECK(ring->eye() == INVALID_ID);
	CHECK(ring->penaltyFrame() == 0);
}

TEST_CASE("modgap1 GiantBirdSlowDeathBehavior: the first clear point is on the ring of the two radii, x from the sine (RW 0x66A61A)")
{
	CombatWorld w(kBirdObjects.c_str());
	w.logic->partition().setRegion(0.0f, 0.0f, 1000.0f, 1000.0f);
	Object *flyer = w.unit("Flyer", 'A', 300, 300);
	w.unit("Block", 'B', 300, 300);
	w.frames(1);
	KindOfMaskType kinds{};
	const int structure = ObjectTemplateInfoBuilder::kindOfIndex("STRUCTURE");
	REQUIRE(structure >= 0);
	kinds[(size_t)structure / 32] |= 1u << (structure % 32);
	const Coord3D in{ 300.0f, 300.0f, 0.0f };
	Coord3D out = in;
	// the flyer's own radius 8 is below RW's minimum 10: rings 10, 20, 30 are inside 30 + 10, ring 40 is not strictly inside
	REQUIRE(GiantBirdSlowDeathBehavior::findClearPoint(*flyer, 200.0f, in, out, kinds));
	CHECK(out.y == doctest::Approx(260.0f)); // tolerance: sin / cos of a float32 -PI
	CHECK(out.x == doctest::Approx(300.0f)); // tolerance: sin / cos of a float32 -PI
	Coord3D unchanged = in;
	CHECK_FALSE(GiantBirdSlowDeathBehavior::findClearPoint(*flyer, 30.0f, in, unchanged, kinds)); // no ring within the radius is clear
	CHECK(unchanged.y == in.y);
}

TEST_CASE("modgap1 GiantBirdSlowDeathBehavior: a killed flyer starts its crash flight to a point clear of CrashAvoidKindOfs (RW 0x86511C)")
{
	CombatWorld w(kBirdObjects.c_str());
	w.logic->partition().setRegion(0.0f, 0.0f, 1000.0f, 1000.0f);
	Object *flyer = w.unit("Flyer", 'A', 300, 300);
	w.unit("Block", 'B', 300, 300);
	GiantBirdSlowDeathBehavior *death = dynamic_cast<GiantBirdSlowDeathBehavior *>(flyer->findModule("GiantBirdSlowDeathBehavior"));
	REQUIRE(death != nullptr);
	w.frames(1);
	flyer->kill(DAMAGE_UNRESISTABLE, DEATH_NORMAL);
	REQUIRE(death->isSlowDeathActivated());
	CHECK(death->crashTarget().y == doctest::Approx(260.0f)); // tolerance: sin / cos of a float32 -PI
	PhysicsBehavior *physics = flyer->physicsModule();
	REQUIRE(physics != nullptr);
	CHECK(physics->isFlying());
	CHECK(physics->speed() == 30.0f); // RW 0xBDAE54
}
