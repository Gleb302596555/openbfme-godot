// OpenBFME. PORT-166 (issue #395, P-64 "Erkenbrand's Time to Strike: the cooldown starts on the order, no charge, slow afterwards"): the FIRE_WEAPON
// charge of a CanFireWhileCharging weapon end to end - when the slot's reload starts, what runs the charge, and what happens to the hero's speed after.
// The RotWK mechanism this lane read (the addresses at each check):
//   * the order (MSG_DO_WEAPON_AT_OBJECT, RW 0x77A34C .. 0x77A3C6): the slot locked LOCKED_TEMPORARILY (RW 0x76FAB7), groupIdle (RW 0x771F49),
//     the attack (RW 0x77208A); the attack kind 8 (RW 0x74D01B .. 0x74D02A) makes the charge machine RW 0x74523F for a weapon whose template has
//     CanFireWhileCharging (+ 0x82);
//   * the strike (RW 0x7519D4 .. 0x751A07): runThrough (RW 0x748348, the goal 50 past the victim), CHARGING on (Object + 0x11C bit 4, RW 0x7519E0),
//     chooseLocomotorSet(SET_PANIC) (RW 0x7519FE, AI vslot 0x238(4)), fireAtOwnPosition (RW 0x74D9CE -> RW 0x6920AD: the weapon fires at the unit's
//     own position) - THE RELOAD STARTS HERE, not at the order (the weapon's status RW 0x6CD142 is READY_TO_FIRE until the shot);
//   * the exit (RW 0x74D892 .. 0x74D8E4): CHARGING off (RW 0x74D8B4), chooseLocomotorSet (RW 0x74D8C8, AI vslot 0x238), releaseObjectWeaponLock
//     (RW 0x68DF11(1)); RW 0x74D8C8 pushes literal SET_NORMAL - which is exactly the set the strike displaced for the vanilla charge user (the Khamul
//     plow: mordorkhamulnew.ini SET_NORMAL FellBeastLocomotor + SET_PANIC FellBeastSwoopLocomotor), so the run is the swoop and the speed after the
//     charge is the speed before it; the fork's mounted chargers (the mod's Erkenbrand: rohanerkenbrand.ini SET_NORMAL TheodenHumanLocomotor 50 /
//     SET_MOUNTED HeroHorseLocomotor 100) entered from SET_MOUNTED, so the port's exit restores the recorded entry set instead of the literal 0
//     (fork issue 395 Q3's check: the speed after == the speed before);
//   * the speed (RW 0x5E3F49): locomotorSetSpeed (ai + 0x1F8, the set's Speed) is the movement speed, the run-through is the PANIC set's.
// The scenario (synthetic, no game files): a mounted hero with a FIRE_WEAPON charge weapon and an enemy out of range - the reload starts when RW starts
// it (not at the order), the hero charges at the charge speed and hits, and his speed afterwards equals the speed before the order. The neighbour keeps
// the same unit on a weapon without CanFireWhileCharging (RW 0x74D021 `je`): no charge, no reload at the order, no speed change.
#include "doctest.h"
#include "CombatTestUtil.h"

#include "GameLogic/AI/AIAttack.h"
#include "GameLogic/AI/WeaponCommands.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Combat/ObjectWeapons.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/SpellEffectModules.h"
#include "GameLogic/WeaponState.h"

#include <cstdio>

using namespace combattest;

namespace
{
// the mod's numbers where they are the subject (rohanerkenbrand.ini / _gamedata.inc: 50 / 100 / 120), the charge speed 200 of the tests' own so the
// run-through is unmistakable
const char kP166Locomotors[] =
	"Locomotor P166FootLoco\n"
	"  Surfaces = GROUND\n"
	"  TurnTime = 500\n"
	"  TurnTimeDamaged = 500\n"
	"  Acceleration = 510\n"
	"  Braking = 510\n"
	"  MinSpeed = 0\n"
	"  ZAxisBehavior = NO_Z_MOTIVE_FORCE\n"
	"  Appearance = TWO_LEGS\n"
	"  StickToGround = Yes\n"
	"  CloseEnoughDist = 1\n"
	"End\n"
	"Locomotor P166HorseLoco\n"
	"  Surfaces = GROUND\n"
	"  TurnTime = 500\n"
	"  TurnTimeDamaged = 500\n"
	"  Acceleration = 510\n"
	"  Braking = 510\n"
	"  MinSpeed = 0\n"
	"  ZAxisBehavior = NO_Z_MOTIVE_FORCE\n"
	"  Appearance = TWO_LEGS\n"
	"  StickToGround = Yes\n"
	"  CloseEnoughDist = 1\n"
	"End\n";

const char kP166Weapons[] =
	// the command's weapon: the mod's ErkenbrandChargeWeapon shape (AttackRange 300, DelayBetweenShots 40000, FiringDuration 0, PER_SHOT,
	// CanFireWhileCharging), a token DamageNugget
	"Weapon P166ChargeWeapon\n"
	"  RadiusDamageAffects = ENEMIES\n"
	"  AttackRange = 300.0\n"
	"  DelayBetweenShots = 40000\n"
	"  FiringDuration = 0\n"
	"  PreAttackType = PER_SHOT\n"
	"  CanFireWhileCharging = Yes\n"
	"  AcceptableAimDelta = 15\n"
	"  DamageNugget\n"
	"    Damage = 1.0\n"
	"    Radius = 1.0\n"
	"    DamageType = CRUSH\n"
	"    DeathType = NORMAL\n"
	"  End\n"
	"End\n"
	// the same weapon without the flag: the neighbour (RW 0x74D021)
	"Weapon P166PlainWeapon\n"
	"  RadiusDamageAffects = ENEMIES\n"
	"  AttackRange = 300.0\n"
	"  DelayBetweenShots = 2000\n"
	"  FiringDuration = 0\n"
	"  PreAttackType = PER_SHOT\n"
	"  AcceptableAimDelta = 15\n"
	"  DamageNugget\n"
	"    Damage = 1.0\n"
	"    Radius = 1.0\n"
	"    DamageType = CRUSH\n"
	"    DeathType = NORMAL\n"
	"  End\n"
	"End\n"
	// what hurts on the way (the mod's FireWeaponUpdate ChargingModeTrigger chain, ErkenbrandHeroEffect's shape)
	"Weapon P166ChargeEffect\n"
	"  RadiusDamageAffects = ENEMIES NEUTRALS\n"
	"  AttackRange = 20.0\n"
	"  DelayBetweenShots = 500\n"
	"  DamageNugget\n"
	"    Damage = 50\n"
	"    Radius = 15.0\n"
	"    DamageType = CRUSH\n"
	"    DeathType = NORMAL\n"
	"  End\n"
	"End\n";

#define P166_BODY(weapon) \
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK\n" \
	"  VisionRange = 400\n" \
	"  Geometry = CYLINDER\n" \
	"  GeometryMajorRadius = 8\n" \
	"  GeometryMinorRadius = 8\n" \
	"  GeometryHeight = 20\n" \
	"  ArmorSet\n" \
	"    Conditions = None\n" \
	"    Armor = PlainArmor\n" \
	"  End\n" \
	"  WeaponSet\n" \
	"    Conditions = None\n" \
	"    Weapon = PRIMARY " weapon "\n" \
	"    Weapon = SECONDARY P166PlainWeapon\n" \
	"  End\n" \
	"  Body = ActiveBody ModuleTag_Body\n" \
	"    MaxHealth = 600\n" \
	"  End\n" \
	"  Behavior = AIUpdateInterface ModuleTag_AI\n" \
	"    AutoAcquireEnemiesWhenIdle = No\n" \
	"  End\n" \
	"  Behavior = FireWeaponUpdate ModuleTag_Charge\n" \
	"    FireWeaponNugget\n" \
	"      WeaponName = P166ChargeEffect\n" \
	"      FireDelay = 0\n" \
	"      OneShot = No\n" \
	"    End\n" \
	"    ChargingModeTrigger = Yes\n" \
	"    AliveOnly = Yes\n" \
	"  End\n" \
	"  LocomotorSet\n" \
	"    Locomotor = P166FootLoco\n" \
	"    Condition = SET_NORMAL\n" \
	"    Speed = 50\n" \
	"  End\n" \
	"  LocomotorSet\n" \
	"    Locomotor = P166HorseLoco\n" \
	"    Condition = SET_MOUNTED\n" \
	"    Speed = 100\n" \
	"  End\n" \
	"  LocomotorSet\n" \
	"    Locomotor = P166HorseLoco\n" \
	"    Condition = SET_PANIC\n" \
	"    Speed = 200\n" \
	"  End\n"

const char kP166Objects[] =
	"Object P166Hero\n" P166_BODY("P166ChargeWeapon") "End\n"
	"Object P166PlainHero\n" P166_BODY("P166PlainWeapon") "End\n"
	"Object P166Victim\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100000\n"
	"  End\n"
	"End\n";

struct TimeToStrikeWorld : CombatWorld
{
	TimeToStrikeWorld() : CombatWorld()
	{
		combat().setAutoAcquireEnabled(false);
		SpellEffectModules::registerAll(w.modules); // FireWeaponUpdate
		WeaponCommands::registerHandlers(dispatcher); // the FIRE_WEAPON messages (MSG_DO_WEAPON_AT_OBJECT), which MoveWorld does not register
		std::string err = w.load(kP166Locomotors, INI_LOAD_OVERWRITE, "p166loco.ini");
		REQUIRE_MESSAGE(err.empty(), err);
		err = w.load(kP166Weapons, INI_LOAD_OVERWRITE, "p166weapon.ini");
		REQUIRE_MESSAGE(err.empty(), err);
		err = w.load(kP166Objects, INI_LOAD_OVERWRITE, "p166.ini");
		REQUIRE_MESSAGE(err.empty(), err);
	}
	static bool charging(const Object *o) { return o->testModelCondition(CombatNames::modelCondition("CHARGING")); }
	static float health(const Object *o) { return o->getBodyModule()->getHealth(); }
	// RW 0x8B17C9 .. 0x8B1812 (ToggleMounted's mount): the MOUNTED model condition (Object + 0x124 bit 22) and AI vslot 0x238(7)
	void mount(Object &o)
	{
		const int mounted = CombatNames::modelCondition("MOUNTED");
		REQUIRE(mounted >= 0); // tolerance: the model-condition index is the registry's counter, not a fixed number (-1 = unregistered)
		if (!o.testModelCondition(mounted))
		{
			o.setModelConditionState(mounted, true);
		}
		AIUpdateInterface *ai = o.getAIUpdateInterface();
		REQUIRE(ai);
		REQUIRE(ai->chooseLocomotorSet(LOCOMOTORSET_MOUNTED));
	}
	// the player's game path: the selection and MSG_DO_WEAPON_AT_OBJECT (RW 0x77A34C), the press's slot and shots as the ControlBar sends them
	void orderThruMessage(Object *who, Object *victim)
	{
		const int alice = playerIndex("Alice");
		select(alice, { who });
		GameMessage m(MSG_DO_WEAPON_AT_OBJECT, alice);
		m.appendIntegerArgument(PRIMARY_WEAPON);
		m.appendObjectIDArgument(victim->getID());
		m.appendIntegerArgument(0x7FFFFFFF);
		send(m);
	}
	unsigned state(Object *o) { return o->getAIUpdateInterface()->currentStateId(); }
	int weaponStatus(Object *o)
	{
		ObjectWeapons *ow = o->getWeapons();
		Weapon *w = ow ? ow->currentWeapon() : nullptr;
		return w ? (int)ow->weaponStatus(*w) : -1;
	}
	// runs until the attack state is left (at most `limit` frames); the frame CHARGING first showed in (-1: never) and the set/speed at the charge
	struct Run
	{
		int frames = 0, chargeFrame = -1, chargeEndFrame = -1;
		int setAtCharge = -1;
		float speedAtCharge = -1.0f, speedAfter = -1.0f;
		int setAfter = -1;
		float healthAtCharge = -1.0f, healthAfter = -1.0f;
	};
	Run run(Object *who, Object *victim, int limit)
	{
		Run r;
		AIUpdateInterface *ai = who->getAIUpdateInterface();
		r.healthAfter = health(victim);
		for (; r.frames < limit; ++r.frames)
		{
			const bool before = charging(who);
			const float hBefore = health(victim);
			frames(1);
			if (!before && charging(who))
			{
				r.chargeFrame = r.frames;
				r.healthAtCharge = hBefore;
				r.setAtCharge = ai->curLocomotorSet();
				r.speedAtCharge = ai->locomotorSetSpeed();
			}
			if (before && !charging(who) && r.chargeEndFrame < 0)
			{
				r.chargeEndFrame = r.frames;
			}
			if (state(who) != (unsigned)AI_ATTACK_OBJECT)
			{
				break;
			}
		}
		r.healthAfter = health(victim);
		r.setAfter = ai->curLocomotorSet();
		r.speedAfter = ai->locomotorSetSpeed();
		return r;
	}
};
} // namespace

// fork issue 395 Q1: the reload starts at the strike, not at the order - RW 0x751A07 fires the weapon (RW 0x6920AD) inside the strike; before that the
// status is READY_TO_FIRE (RW 0x6CD142). The scenario: a mounted hero (SET_MOUNTED, speed 100) with a FIRE_WEAPON charge weapon and an enemy out of
// range (450 away, AttackRange 300): the reload starts when RW starts it, the hero charges at the charge speed (SET_PANIC, speed 200, RW 0x7519FE)
// and hits, and his speed afterwards equals the speed before the order (the exit's set restore, RW 0x74D8C8)
TEST_CASE("PORT-166 time to strike: the reload starts at the strike, the run is the PANIC set's speed and the speed after the charge is the speed before the order")
{
	TimeToStrikeWorld w;
	Object *hero = w.unit("P166Hero", 'A', 300, 300);
	Object *victim = w.unit("P166Victim", 'B', 750, 300);
	w.frames(2);
	w.mount(*hero);
	w.frames(2);
	AIUpdateInterface *ai = hero->getAIUpdateInterface();
	REQUIRE(ai->curLocomotorSet() == (int)LOCOMOTORSET_MOUNTED);
	const float speedBefore = ai->locomotorSetSpeed();
	REQUIRE(speedBefore == 100.0f);
	const float healthBefore = TimeToStrikeWorld::health(victim);
	w.orderThruMessage(hero, victim);
	w.frames(1); // the dispatch frame: the order is in, the attack started
	// Q1: the reload has not started at the order (the enemy is out of range: no strike yet)
	CHECK(w.weaponStatus(hero) == (int)WEAPON_READY_TO_FIRE);
	CHECK_FALSE(TimeToStrikeWorld::charging(hero));
	const TimeToStrikeWorld::Run r = w.run(hero, victim, 600);
	std::printf("  info: PORT-166: CHARGING from frame %d to %d, attack over after %d frames; set/speed at the charge %d / %.0f, after %d / %.0f (before %d / %.0f), victim %.0f -> %.0f (at the charge %.0f)\n",
		r.chargeFrame, r.chargeEndFrame, r.frames, r.setAtCharge, r.speedAtCharge, r.setAfter, r.speedAfter, (int)LOCOMOTORSET_MOUNTED, speedBefore,
		healthBefore, r.healthAfter, r.healthAtCharge);
	REQUIRE(r.chargeFrame == 8); // tolerance: the walk frame can shift by one on another compiler's float rounding (450 away, AttackRange 300)
	CHECK(r.healthAtCharge == healthBefore);        // the walk hurts nobody: the effect weapon fires only with CHARGING (RW 0x7519E0 -> the trigger)
	CHECK(r.setAtCharge == (int)LOCOMOTORSET_PANIC); // RW 0x7519FE: the run-through is the PANIC set's
	CHECK(r.speedAtCharge == 200.0f);               // the charge speed: the set's Speed (ai + 0x1F8)
	CHECK(r.healthAfter <= healthBefore - 50.0f); // tolerance: the run-through can stay in the radius 15 for more than one 500ms effect shot
	CHECK(r.frames < 600); // tolerance: the catch-all "ended by itself" guard; the exact count is the path's, 21 here
	CHECK(r.setAfter == (int)LOCOMOTORSET_MOUNTED); // RW 0x74D8C8: the exit restores the set the strike displaced
	CHECK(r.speedAfter == speedBefore);             // his speed afterwards equals the speed before the order
	CHECK_FALSE(TimeToStrikeWorld::charging(hero));
	CHECK_FALSE(hero->getWeapons()->isCurWeaponLocked()); // RW 0x68DF11(1) at the exit
}

// the reload really starts at the strike: the second charge of the same weapon follows its DelayBetweenShots, and the frame CHARGING shows the status
// is already off READY (the shot left at that frame, RW 0x7519D4 .. 0x751A07)
TEST_CASE("PORT-166 time to strike: the status at the first CHARGING frame is the shot's (RW 0x6920AD), not the order's")
{
	TimeToStrikeWorld w;
	Object *hero = w.unit("P166Hero", 'A', 300, 300);
	Object *victim = w.unit("P166Victim", 'B', 750, 300);
	w.frames(2);
	w.mount(*hero);
	w.frames(2);
	w.orderThruMessage(hero, victim);
	int statusAtCharge = -1;
	for (int f = 0; f < 600; ++f)
	{
		const bool before = TimeToStrikeWorld::charging(hero);
		w.frames(1); // the order dispatches inside the first frame, so the loop must not gate its first iteration on the attack state
		if (!before && TimeToStrikeWorld::charging(hero))
		{
			statusAtCharge = w.weaponStatus(hero);
			break;
		}
		if (f > 0 && w.state(hero) != (unsigned)AI_ATTACK_OBJECT)
		{
			break;
		}
	}
	std::printf("  info: PORT-166 status at the charge: %d (0 = READY, 2 = BETWEEN_FIRING_SHOTS)\n", statusAtCharge);
	REQUIRE(statusAtCharge == (int)WEAPON_BETWEEN_FIRING_SHOTS); // RW 0x751A07: the shot was made at the strike; -1 means no charge was seen
	// the world tears down cleanly only with the attack finished (a mid-attack teardown is a harness hazard outside this mechanism)
	w.frames(60);
}

// the neighbour (RW 0x74D021 `je`): the same unit and order with a weapon without CanFireWhileCharging keeps the generic attack - no CHARGING, no run
// through the victim, no reload at the order, and the locomotor set (the speed) untouched from the order to the end
TEST_CASE("PORT-166 neighbour: a weapon without CanFireWhileCharging neither charges nor touches the locomotor set (RW 0x74D021)")
{
	TimeToStrikeWorld w;
	Object *hero = w.unit("P166PlainHero", 'A', 300, 300);
	Object *victim = w.unit("P166Victim", 'B', 750, 300);
	w.frames(2);
	w.mount(*hero);
	w.frames(2);
	AIUpdateInterface *ai = hero->getAIUpdateInterface();
	REQUIRE(ai->curLocomotorSet() == (int)LOCOMOTORSET_MOUNTED);
	const float speedBefore = ai->locomotorSetSpeed();
	w.orderThruMessage(hero, victim);
	bool charged = false;
	int statusAtOrder = -1;
	w.frames(1);
	statusAtOrder = w.weaponStatus(hero);
	for (int f = 0; f < 300 && w.state(hero) == (unsigned)AI_ATTACK_OBJECT; ++f)
	{
		w.frames(1);
		charged = charged || TimeToStrikeWorld::charging(hero);
	}
	std::printf("  info: PORT-166 neighbour: charged %d, status at the order %d, set/speed after %d / %.0f (before %d / %.0f)\n", charged ? 1 : 0,
		statusAtOrder, ai->curLocomotorSet(), ai->locomotorSetSpeed(), (int)LOCOMOTORSET_MOUNTED, speedBefore);
	CHECK_FALSE(charged);
	CHECK(statusAtOrder == (int)WEAPON_READY_TO_FIRE);          // no shot at the order: the reload has not started
	CHECK(ai->curLocomotorSet() == (int)LOCOMOTORSET_MOUNTED);  // no set switch anywhere in the attack
	CHECK(ai->locomotorSetSpeed() == speedBefore);
}
