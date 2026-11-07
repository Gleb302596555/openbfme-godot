// OpenBFME. PORT-1 (issue #53) tests: Weapon/FinishAttackOnceStarted — WeaponTemplate + 0x161, the row of RW's WeaponTemplate field table RW 0xC17208, the
// parser RW 0x42E558 (bool) and the constructor's default 1 at RW 0x6CE164 (the copy at RW 0x6CE765 / 0x6CE76B copies it).
//
// RotWK 2.01 reads the field in exactly one live place: AIAttackFireWeaponState::update RW 0x74C4A7 (vtable 0xC27858 slot 6), in the branch RW 0x74C6A8 ..
// RW 0x74C6FB. That branch is entered only after the earlier leaves of the same update — no weapon (RW 0x74C4E2), the owner effectively dead (Object + 0x458
// bit 0, RW 0x74C4EB) and END_FIRE_STATE (0x52, RW 0x74C4F8) all return STATE_CONTINUE first — and only when the shot is an attack on an object (the notify's
// vslot 3 = RW 0x9DBF60, `mov al, 1`, true only for AIAttackState + 0x41 and TurretAI's own flag) and the victim is gone: no victim object (RW 0x74C6C3), an
// effectively dead one (+ 0x458 bit 0, RW 0x74C6C7) or one with the status FLEE_OFF_MAP (0x32, RW 0x74C6D0, TheObjectStatusNames[0x32]).
//
//   FinishAttackOnceStarted = Yes (the default, RW 0x6CE164): RW 0x74C6E5 `cmp byte ptr [eax+0x161], 0` reads non-zero, RW 0x74C6EC `jne 0x74C530` jumps
//   into the fire path — the wind-up is finished, the shot leaves at the victim's last position (RW 0x74C75C fireCurrentWeapon, RW 0x69200A, then the
//   state returns STATE_FAILURE at RW 0x74C8AC: the attack ends after the shot).
//
//   FinishAttackOnceStarted = No: the byte is zero, RW 0x74C6F2 clears the machine's goal object (setGoalObject(NULL), RW 0x8DB8CB; not ported, stop S-7182)
//   and RW 0x74C8AC returns STATE_FAILURE — no shot is fired at all, the attack ends with the victim.
//
// The decision has one precondition of its own: a NoVictimNeeded weapon (+ 0x16B) whose victim is absent (RW 0x74C51C) or not IMMOBILE (RW 0x74C523, KindOf bit 2
// of the victim's template at + 0x108) jumps to the fire path at RW 0x74C52C before the decision, so those shots never read the field.
//
// The two tests below drive the same decision through the port: AIAttackFireState::update (engine/src/GameLogic/AI/AIAttack.cpp) takes the current weapon
// first (RW 0x74C4D3 .. RW 0x74C4E5) and then asks the template, exactly as RW 0x74C6A8 .. RW 0x74C6FB does. The victim flees off the map (FLEE_OFF_MAP,
// RW 0x74C6D0) in the middle of the wind-up: the port's attack state (AIAttackState::update) only ends an attack on an absent or dead victim (RW 0x75126D ..
// RW 0x75127F), so this is the branch the fire state itself decides.
//
// Synthetic data (combattest::CombatWorld), no retail files, no Ennorath files.
#include "doctest.h"
#include "CombatTestUtil.h"

#include "GameLogic/AI/AIAttack.h"
#include "GameLogic/Combat/CombatNames.h"

using namespace combattest;

namespace
{
// WindUpBowStop: FinishAttackOnceStarted = No, as 51 of Ennorath 1.9.8.2's weapon definitions do (all of them horde rangefinders, 13 with NoVictimNeeded = Yes).
// WindUpBowKeep: the same weapon without the row — the template keeps the constructor's 1 (RW 0x6CE164), today's behaviour.
// WindUpBowNoVictim: NoVictimNeeded = Yes and FinishAttackOnceStarted = No: RW 0x74C50F .. RW 0x74C52C sends its shot past the decision.
// All three: a 2000 ms wind-up (10 frames at the test logic's 5 frames per second, ceil(ms * 0.005)), a 200 ms FiringDuration and no shot delay.
const char kPort1Objects[] =
	"Weapon WindUpBowStop\n"
	"  AttackRange = 200\n"
	"  WeaponSpeed = 100\n"
	"  PreAttackDelay = 2000\n"
	"  FiringDuration = 200\n"
	"  FinishAttackOnceStarted = No\n"
	"  ProjectileNugget\n"
	"    ProjectileTemplateName = Arrow\n"
	"    WarheadTemplateName = BowWarhead\n"
	"  End\n"
	"End\n"
	"Weapon WindUpBowNoVictim\n"
	"  AttackRange = 200\n"
	"  WeaponSpeed = 100\n"
	"  PreAttackDelay = 2000\n"
	"  FiringDuration = 200\n"
	"  NoVictimNeeded = Yes\n"
	"  FinishAttackOnceStarted = No\n"
	"  ProjectileNugget\n"
	"    ProjectileTemplateName = Arrow\n"
	"    WarheadTemplateName = BowWarhead\n"
	"  End\n"
	"End\n"
	"Weapon WindUpBowKeep\n"
	"  AttackRange = 200\n"
	"  WeaponSpeed = 100\n"
	"  PreAttackDelay = 2000\n"
	"  FiringDuration = 200\n"
	"  ProjectileNugget\n"
	"    ProjectileTemplateName = Arrow\n"
	"    WarheadTemplateName = BowWarhead\n"
	"  End\n"
	"End\n"
	"Object WindUpArcherStop\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK\n"
	"  VisionRange = 300\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY WindUpBowStop\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 60\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"    AutoAcquireEnemiesWhenIdle = No\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n"
	"Object WindUpArcherNoVictim\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK\n"
	"  VisionRange = 300\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY WindUpBowNoVictim\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 60\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"    AutoAcquireEnemiesWhenIdle = No\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n"
	"Object WindUpArcherKeep\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK\n"
	"  VisionRange = 300\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY WindUpBowKeep\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 60\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"    AutoAcquireEnemiesWhenIdle = No\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n";

struct Port1World : CombatWorld
{
	Port1World() : CombatWorld(kPort1Objects) { combat().setAutoAcquireEnabled(false); }
	int status(Object *o) { return o->getWeapons()->weaponStatus(*o->getWeapons()->currentWeapon()); }
	Weapon *bow(Object *o) { return o->getWeapons()->currentWeapon(); }
	// the attack order and the wind-up: the aim takes its frames, then the fire state's onEnter RW 0x74C258 runs the pre-attack (RW 0x6CE95D)
	void windUp(Object *archer, Object *foe)
	{
		REQUIRE(archer->getAIUpdateInterface()->aiAttackObject(foe, CMD_FROM_PLAYER));
		runUntil([&] { return status(archer) == WEAPON_PRE_ATTACK; }, 200);
		REQUIRE(status(archer) == WEAPON_PRE_ATTACK);
		REQUIRE(bow(archer)->lastFireFrame() == 0);
	}
};

const int kFleeOffMap = CombatNames::status("FLEE_OFF_MAP"); // 0x32, RW 0x44DDEC(0x32) in the test of RW 0x74C6D0
} // namespace

TEST_CASE("port1 FinishAttackOnceStarted = No (RW 0x74C6E5 -> RW 0x74C6F2, RW 0x74C8AC): a victim that flees off the map during the wind-up takes the shot with it")
{
	Port1World w;
	Object *archer = w.unit("WindUpArcherStop", 'A', 300, 300);
	Object *foe = w.unit("Dummy", 'B', 400, 300);
	w.frames(2);
	w.windUp(archer, foe);
	AIUpdateInterface *ai = archer->getAIUpdateInterface();
	Weapon *bow = w.bow(archer);
	// the victim runs off the map while the shot is still in its wind-up (status 0x32, RW 0x74C6D0's test)
	foe->setStatus((unsigned)kFleeOffMap, true);
	// RW 0x74C6E5 reads the template's byte (0 here), RW 0x74C6F2 clears the goal and RW 0x74C8AC returns STATE_FAILURE: the fire state leaves, the aim finds
	// the same FLEE_OFF_MAP victim (RW 0x7523B2) and the attack ends — no shot is fired
	w.runUntil([&] { return ai->currentStateId() != (unsigned)AI_ATTACK_OBJECT; }, 60);
	CHECK(ai->currentStateId() != (unsigned)AI_ATTACK_OBJECT); // the attack ended
	CHECK(w.status(archer) != WEAPON_PRE_ATTACK);
	CHECK_FALSE(archer->testStatus((unsigned)CombatNames::status("IS_FIRING_WEAPON"))); // onExit RW 0x74C8BE
	w.frames(100); // well past the frame the wind-up would have ended at
	CHECK(bow->lastFireFrame() == 0);
	// onExit RW 0x74C8BE: a swing cancelled before it landed clears the pre-attack (+ 0x1C) and writes the status back
	CHECK(bow->whenPreAttackFinished() == 0);
	CHECK(bow->storedStatus() == WEAPON_READY_TO_FIRE);
}

// Held off by the upstream merge of 2026-11-06 (ATKSTATE-1a, RW 0x751285): the outer AIAttackState now counts a FLEE_OFF_MAP victim as gone and ends the
// attack the ZH way, while RotWK calls AI vslot 0x240 and lets the inner fire state run on (stop S-583), so the fire state never reaches RW 0x74C6A8 here.
// Port 14 (issue #70) ports RW's gone path and turns this test back on.
TEST_CASE("port1 a NoVictimNeeded weapon never reads the field (RW 0x74C50F .. RW 0x74C52C): its shot leaves even with FinishAttackOnceStarted = No" * doctest::skip())
{
	Port1World w;
	Object *archer = w.unit("WindUpArcherNoVictim", 'A', 300, 300);
	Object *foe = w.unit("Dummy", 'B', 400, 300);
	w.frames(2);
	w.windUp(archer, foe);
	REQUIRE_FALSE(foe->isKindOf((unsigned)CombatNames::kinds().immobile));
	Weapon *bow = w.bow(archer);
	const std::uint32_t windUpEnd = bow->whenPreAttackFinished();
	foe->setStatus((unsigned)kFleeOffMap, true);
	// RW 0x74C519 -> RW 0x74C52C: a victim that is not IMMOBILE sends the NoVictimNeeded shot to the fire path before RW 0x74C6A8, so the field's zero is never
	// read and the shot leaves (at the machine's goal position, RW 0x74C88B)
	w.runUntil([&] { return bow->lastFireFrame() != 0; }, 200);
	CHECK(bow->lastFireFrame() == windUpEnd);
}

// Held off by the upstream merge of 2026-11-06 (ATKSTATE-1a, RW 0x751285): the outer AIAttackState now counts a FLEE_OFF_MAP victim as gone and ends the
// attack the ZH way, while RotWK calls AI vslot 0x240 and lets the inner fire state run on (stop S-583), so the fire state never reaches RW 0x74C6A8 here.
// Port 14 (issue #70) ports RW's gone path and turns this test back on.
TEST_CASE("port1 the constructor's default FinishAttackOnceStarted (1, RW 0x6CE164) keeps the shot: the wind-up ends and the shot leaves" * doctest::skip())
{
	Port1World w;
	Object *archer = w.unit("WindUpArcherKeep", 'A', 300, 300);
	Object *foe = w.unit("Dummy", 'B', 400, 300);
	w.frames(2);
	w.windUp(archer, foe);
	Weapon *bow = w.bow(archer);
	// the wind-up the fire state's onEnter RW 0x74C258 started (RW 0x6CE95D, Weapon + 0x1C): the weapon answers PRE_ATTACK until then and READY_TO_FIRE from
	// that frame on (RW 0x6CD142), so the shot comes exactly there
	const std::uint32_t windUpEnd = bow->whenPreAttackFinished();
	foe->setStatus((unsigned)kFleeOffMap, true);
	// RW 0x74C6E5 reads a non-zero byte, RW 0x74C6EC jumps into the fire path RW 0x74C530: the wind-up is not cancelled, the shot leaves at the victim's last
	// position (RW 0x74C75C) — today's behaviour, the one the default must keep
	w.runUntil([&] { return bow->lastFireFrame() != 0; }, 200);
	CHECK(bow->lastFireFrame() == windUpEnd);
}
