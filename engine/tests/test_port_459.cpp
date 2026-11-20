// OpenBFME unit tests (lane PORT-459, issue #459, stop S-1228): the two weapon-switch modules of a hero.
//
//   * WeaponModeSpecialPowerUpdate's start (RW 0x89841E) ends with the object's own FiringTracker reset hard: RW 0x8984AF reads Object + 0x248 (the weapon
//     set) and RW 0x8984B7 calls RW 0x8E302A with the argument 1. The FiringTracker's continuous fire count and its shot victim (RW 0x8E326D's + 0x20 /
//     + 0x24) start over, so the mode's first shots do not continue the burst of the weapon the hero fired before it. Before the change the reset was
//     missing: the count and the victim of the previous fight survived the switch.
//   * The mode's InitiateSound (module data + 0x0C, RW 0x73ACEF stores it) is answered by the special power interface's slot 0x40: RW 0xC650F0's element
//     16 is RW 0x991626, which returns the module data's + 0xC. The unit voice asks exactly that slot through RW 0x691526 when a MSG_DO_SPECIAL_POWER*
//     names the power (RW 0x8DF846). The initiate itself (RW 0x89841E) posts no sound: the mode plays nothing more than the other special power modules.
//   * DualWeaponBehavior's victim without UseRealVictimRange (RW 0x85E0C2 -> RW 0x701443 with flags 0x62 = CAN_ATTACK | UNFOGGED | AUTO_ACQUIRE) is the
//     ported worker RW 0x7006D7, not an inference: a nearer enemy this object cannot attack (CAN_ATTACK's possible-to-attack filter RW 0x660CA4), a nearer
//     NOT_AUTOACQUIRABLE enemy (AUTO_ACQUIRE's 0xC1D6AC) and a caster with NO_AUTO_ACQUIRE (RW 0x7006D7's early answer) are all refused, while the closer
//     attackable enemy in the same distance wins the search. Before the change the search was an ad-hoc chain (alive + ENEMIES + canSee at 2D centres),
//     which switched the weapon for the nearer refused enemy.
//
// Synthetic INI only; every expectation comes from the addresses above, not from the port's code.

#include "doctest.h"
#include "CombatTestUtil.h"

#include "Common/Audio/AudioEntryPoints.h"
#include "Common/Player.h"
#include "Common/SpecialPower.h"
#include "Common/Thing/ThingTemplate.h"
#include "GameClient/FXList.h"
#include "GameClient/UnitVoiceResponse.h"
#include "GameLogic/AI/AIClosestEnemy.h"
#include "GameLogic/AttributeModifiers.h"
#include "GameLogic/Combat/ObjectWeapons.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/HeroAbilityModules.h"
#include "GameLogic/Module/SpecialAbilityModules.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Object/AttributeModifierPool.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/Object/PartitionManager.h"
#include "GameLogic/ObjectTemplateInfo.h"
#include "GameLogic/Weapon.h"

#include <set>
#include <string>
#include <vector>

namespace
{

// The mode's power: a template whose ReloadTime is long enough that the mode is the only thing the test drives. The point is the FiringTracker, the sound
// name and the close range victim, all of them parsed from this block: SpecialPowerTemplate + 0x08 (RW 0x73B22F), InitiateSound + 0x0C (RW 0x73ACEF),
// LockWeaponSlot + 0x20 (RW 0x42E9B7), Duration + 0x1C (RW 0x73A429, milliseconds).
const char kPort459Ini[] =
	"SpecialPower SpecialAbilityPort459Mode\n"
	"  Enum = SPECIAL_HERO_MODE\n"
	"  ReloadTime = 30000\n"
	"End\n"
	"Weapon Port459Sword\n"
	"  AttackRange = 30.0\n"
	"  MeleeWeapon = Yes\n"
	// 200 ms = one logic frame between shots, a coast of 2000 ms = ten frames: the burst is not dropped by the coast poll (RW 0x8E30DD's
	// `now > coastEnd` reset) between the five shots of the test
	"  DelayBetweenShots = 200\n"
	"  ContinuousFireOne = 1\n"
	"  ContinuousFireTwo = 4\n"
	"  ContinuousFireCoast = 2000\n"
	"  DamageNugget\n"
	"    Damage = 6.0\n"
	"    Radius = 0.0\n"
	"    DelayTime = 0\n"
	"    DamageType = SLASH\n"
	"    DeathType = NORMAL\n"
	"  End\n"
	"End\n"
	"Weapon Port459LongSword\n"
	"  AttackRange = 60.0\n"
	"  MeleeWeapon = Yes\n"
	"  DelayBetweenShots = 1000\n"
	"  DamageNugget\n"
	"    Damage = 8.0\n"
	"    Radius = 0.0\n"
	"    DelayTime = 0\n"
	"    DamageType = SLASH\n"
	"    DeathType = NORMAL\n"
	"  End\n"
	"End\n"
	// The hero of the mode tests: two weapon sets (None -> Port459Sword, CLOSE_RANGE -> Port459LongSword), a SecondWeapon in the default set so that the mode's
	// LockWeaponSlot = SECONDARY has something to lock, and the mode. No DualWeaponBehavior: its update would rebuild the weapon set, and a rebuilt set clears
	// the mode's permanent lock (RW 0x6CA178's m_lockType = NOT_LOCKED unless the sets share the lock)
	"Object Port459Hero\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK HERO\n"
	"  VisionRange = 300.0\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY Port459Sword\n"
	"    Weapon = SECONDARY Port459LongSword\n"
	"  End\n"
	"  WeaponSet\n"
	"    Conditions = WEAPONSET_TOGGLE_1\n"
	"    Weapon = PRIMARY Port459Sword\n"
	"  End\n"
	"  WeaponSet\n"
	"    Conditions = CLOSE_RANGE\n"
	"    Weapon = PRIMARY Port459LongSword\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 600.0\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = WeaponModeSpecialPowerUpdate ModuleTag_Mode\n"
	"    SpecialPowerTemplate = SpecialAbilityPort459Mode\n"
	"    Duration = 20000\n"
	// RW 0xC87588's InitiateSound row (+ 0x0C through RW 0x73ACEF): a name-only sound, played by the unit voice through the interface's slot 0x40
	"    InitiateSound = Port459ModeSound\n"
	"    LockWeaponSlot = SECONDARY\n"
	"  End\n"
	"End\n"
	// The hero without the mode and without the second weapon: DualWeaponBehavior alone (the close range victim search).
	"Object Port459DualHero\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK HERO\n"
	"  VisionRange = 300.0\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY Port459Sword\n"
	"  End\n"
	"  WeaponSet\n"
	"    Conditions = CLOSE_RANGE\n"
	"    Weapon = PRIMARY Port459LongSword\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 600.0\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = DualWeaponBehavior ModuleTag_Dual\n"
	"    SwitchWeaponOnCloseRangeDistance = 200.0\n"
	"  End\n"
	"End\n"
	// A hero whose weapon set has no CLOSE_RANGE set: the close range flag alone turns on, the model condition follows (RW 0x691059, + 0x120 bit 0)
	"Object Port459PlainHero\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK HERO\n"
	"  VisionRange = 300.0\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY Port459Sword\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 600.0\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = DualWeaponBehavior ModuleTag_Dual\n"
	"    SwitchWeaponOnCloseRangeDistance = 200.0\n"
	"  End\n"
	"End\n"
	// The enemies of the close range test. Port459Foe is a plain infantry the hero can attack; Port459Unattackable is the same but NOT_AUTOACQUIRABLE
	// (AUTO_ACQUIRE's 0xC1D6AC filter refuses it) and Port459Airborne carries an AIRBORNE_TARGET status, which the hero's ground weapon cannot attack
	// (CAN_ATTACK's possible-to-attack filter, RW 0x660CA4). Both are placed nearer than the attackable one.
	"Object Port459Foe\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 400.0\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"End\n"
	"Object Port459Unattackable\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK NOT_AUTOACQUIRABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 400.0\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"End\n"
	"Object Port459AirborneFoe\n"
	"  KindOf = INFANTRY SELECTABLE CAN_ATTACK\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 400.0\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"End\n";

// RW 0x8E326D's continuous fire: the count of shots at the same victim (tracker + 0x20) and the frame the burst's coast ends on.
const std::uint32_t kMean = 1u << FiringTracker::CONDITION_CONTINUOUS_FIRE_MEAN;
const std::uint32_t kFast = 1u << FiringTracker::CONDITION_CONTINUOUS_FIRE_FAST;

// RW 0xDA1328's name table: the indices the module's constants name (kWeaponSetCloseRange 7 in HeroAbilityModules.cpp)
const int kCloseRangeBit = 7; // TheWeaponConditionNames[7] CLOSE_RANGE
const int kAirborneTargetStatus = 6;  // TheObjectStatusNames[6] AIRBORNE_TARGET (RW 0xD8AFF0)
const int kNoAutoAcquireStatus = 23;  // TheObjectStatusNames[23] NO_AUTO_ACQUIRE
const int kCloseRangeModelCondition = 160; // TheModelConditionNames[160] WEAPONSTATE_CLOSE_RANGE (RW 0xD9FAD8)

const WeaponModeSpecialPowerUpdate *modeOf(const Object &o)
{
	for (const std::unique_ptr<BehaviorModule> &m : o.modules())
	{
		if (const auto *w = dynamic_cast<const WeaponModeSpecialPowerUpdate *>(m.get()))
		{
			return w;
		}
	}
	return nullptr;
}

WeaponModeSpecialPowerUpdate *modeOf(Object &o)
{
	return const_cast<WeaponModeSpecialPowerUpdate *>(modeOf(static_cast<const Object &>(o)));
}

const DualWeaponBehavior *dualOf(const Object &o)
{
	for (const std::unique_ptr<BehaviorModule> &m : o.modules())
	{
		if (const auto *d = dynamic_cast<const DualWeaponBehavior *>(m.get()))
		{
			return d;
		}
	}
	return nullptr;
}

// The unit voice sink of the sound test: RW 0x8DEDBB's Sink, with the mode's own InitiateSound known to TheAudio (RW vtable + 0x7C)
struct Port459VoiceSink : UnitVoiceResponse::Sink
{
	std::set<std::string> known;
	std::vector<std::string> sounds;
	bool soundExists(const std::string &name) override { return known.count(name) != 0; }
	void playSound(const std::string &name, ObjectID, int) override { sounds.push_back(name); }
	void reportEva(const std::string &, const Coord3D *) override {}
};

// The combat fixture plus the stores the mode's INI names: SpecialPower (RW 0x7B212F), FXList, the attribute modifiers and the weapon/armour blocks the
// CombatWorld already installs. HeroAbilityModules registers WeaponModeSpecialPowerUpdate and DualWeaponBehavior (lane HERO-2).
struct Port459World : combattest::CombatWorld
{
	SpecialPowerStore powers;
	SpecialPowerStore *savedPowers = TheSpecialPowerStore;
	FXListStore fxLists;
	FXListStore *savedFx = TheFXListStore;
	AttributeModifierStore modifiers;
	AttributeModifierStore *savedModifiers = TheAttributeModifierStore;

	Port459World()
		: combattest::CombatWorld("", 100)
	{
		TheSpecialPowerStore = &powers;
		TheFXListStore = &fxLists;
		TheAttributeModifierStore = &modifiers;
		modifiers.registerBlock(w.fx.env.blocks);
		w.fx.env.blocks.registerBlock("FXList", [](INI *ini) { ParseFXListDefinitionGlobal(ini); });
		w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
		SpecialPowerModules::registerAll(w.modules);
		SpecialAbilityModules::registerAll(w.modules); // HeroAbilityModules::registerAll is called from here (lane HERO-2)
		const std::string err = w.load(kPort459Ini, INI_LOAD_OVERWRITE, "port459.ini");
		REQUIRE_MESSAGE(err.empty(), err);
		// AIWorld's map does not set the partition's region: without it every search reads no cell and finds nothing (test_emotion_ai / test_cbhunt set it too)
		logic->partition().setRegion(0.0f, 0.0f, 1000.0f, 1000.0f);
		logic->combat().setAutoAcquireEnabled(false); // only the behaviour under test picks a victim
	}
	~Port459World()
	{
		logic->reset();
		TheSpecialPowerStore = savedPowers;
		TheFXListStore = savedFx;
		TheAttributeModifierStore = savedModifiers;
	}

	bool closeRange(const Object &o) const
	{
		const ObjectWeapons *w = o.getWeapons();
		return w && MaskTest(w->weaponSetFlags(), (unsigned)kCloseRangeBit);
	}
};

} // namespace

// RW 0x8984AF .. 0x8984B7: the mode's start resets the object's FiringTracker hard (RW 0x8E302A(1)). The tracker's count of shots at one victim (+ 0x20)
// and its continuous fire condition bits (RW 0x8E326D -> the owner's weapon bonus mask bits 2 / 3) start over, so the first shot after the switch is the
// first shot of a new burst. Without the reset the count keeps growing through the mode's switch.
TEST_CASE("PORT-459: WeaponModeSpecialPowerUpdate's start resets the FiringTracker hard (RW 0x8984B7 -> RW 0x8E302A(1))")
{
	Port459World f;
	Object *hero = f.unit("Port459Hero", 'A', 400.0f, 400.0f);
	Object *foe = f.unit("Port459Foe", 'B', 420.0f, 400.0f);
	REQUIRE(hero);
	REQUIRE(foe);
	ObjectWeapons *weapons = hero->getWeapons();
	REQUIRE(weapons);
	WeaponModeSpecialPowerUpdate *mode = modeOf(*hero);
	REQUIRE(mode);
	REQUIRE(foe->getBodyModule());

	// The hero fires five shots through the real fire path (RW 0x69200A): its RW 0x8E326D counts them (shot 2 passes ContinuousFireOne 1 -> MEAN, shot 5
	// passes ContinuousFireTwo 4 -> FAST: bits 2 and 3 of the weapon bonus mask, RW 0x8E3174). No logic frame runs between the shots: the tracker's own
	// update (RW 0x8E30DD) resets a burst whose owner moved, and that is not this test's subject
	for (int shot = 1; shot <= 5; ++shot)
	{
		weapons->chooseBestWeaponForTarget(foe, PREFER_MOST_DAMAGE, CMD_FROM_PLAYER);
		weapons->fireCurrentWeapon(foe, nullptr); // the answer is "reloaded", false for a weapon without a ClipSize (unlimited ammo)
		CHECK(weapons->firingTracker().shotCount() == shot);
	}
	FiringTracker &tracker = weapons->firingTracker();
	CHECK(tracker.shotCount() == 5);
	CHECK((hero->weaponBonusConditionMask() & (kMean | kFast)) == kFast);

	const SpecialPowerTemplate *power = mode->getSpecialPowerTemplate();
	REQUIRE(power);
	// RW 0x9911FB: doSpecialPower runs the initiate when the countdown is not paused and the object is not disabled
	mode->doSpecialPower(0);
	REQUIRE(mode->active());
	// RW 0x8E302A(1): the count is 0 and the condition bits are cleared; a running burst is not continued by the mode's weapon
	CHECK(tracker.shotCount() == 0);
	CHECK((hero->weaponBonusConditionMask() & (kMean | kFast)) == 0u);
	// RW 0x8E326D with the tracker reset: the mode's next shot at the same victim is shot 1 of a new burst, not shot 6 of the old one
	weapons->chooseBestWeaponForTarget(foe, PREFER_MOST_DAMAGE, CMD_FROM_PLAYER);
	weapons->fireCurrentWeapon(foe, nullptr);
	CHECK(tracker.shotCount() == 1);
	// The mode's own effect is unchanged (S-1228's already ported part): LockWeaponSlot = SECONDARY is locked permanently (RW 0x69121A(slot, 2))
	CHECK(hero->getWeapons()->isCurWeaponLocked());
}

// RW 0xC650F0[16] = RW 0x991626: the special power interface's slot 0x40 returns the module data's + 0x0C (the mode's InitiateSound, RW 0x73ACEF). The unit
// voice asks that slot (RW 0x8DF846 through RW 0x691526) for a MSG_DO_SPECIAL_POWER that names the mode's power, so the mode's InitiateSound is what the hero
// says when it switches. The initiate posts nothing itself: the port's sound count for the cast stays 0.
TEST_CASE("PORT-459: the mode's InitiateSound is the special power interface's slot 0x40 answer (RW 0x991626, read by the unit voice at RW 0x8DF846)")
{
	Port459World f;
	Object *hero = f.unit("Port459Hero", 'A', 400.0f, 400.0f);
	REQUIRE(hero);
	WeaponModeSpecialPowerUpdate *mode = modeOf(*hero);
	REQUIRE(mode);
	const SpecialPowerTemplate *power = mode->getSpecialPowerTemplate();
	REQUIRE(power);
	// the parsed data (+ 0x0C): the INI's InitiateSound
	REQUIRE(mode->getInitiateSound() == "Port459ModeSound");

	// RW 0x691526: the interface of the module that drives the power is found on the hero; its slot 0x40 answers the module data's + 0x0C
	const SpecialPowerModuleInterface *iface = hero->findSpecialPowerModuleInterface(power->getSpecialPowerType());
	REQUIRE(iface);
	CHECK(&iface->getInitiateSound() == &mode->getInitiateSound());

	// RW 0x8DF846: MSG_DO_SPECIAL_POWER with the mode's template plays that sound through the voice, exactly once
	Port459VoiceSink sink;
	sink.known.insert("Port459ModeSound");
	UnitVoiceResponse voice(*f.logic, sink);
	voice.setLocalPlayerIndex(f.playerOf('A')->getPlayerIndex());
	UnitVoiceResponse::Info info;
	info.powerTemplate = power;
	CHECK(voice.pickAndPlay({ hero->getID() }, MSG_DO_SPECIAL_POWER, &info));
	REQUIRE(sink.sounds.size() == 1u);
	CHECK(sink.sounds[0] == "Port459ModeSound");

	// the initiate itself posts no sound (RW 0x89841E calls no audio): one click, one sound
	REQUIRE(AudioApi::current() == nullptr);
	const std::uint64_t before = AudioApi::callsWithoutAudio();
	mode->doSpecialPower(0);
	CHECK(mode->active());
	CHECK(AudioApi::callsWithoutAudio() == before);
}

// RW 0x85E0C2: without UseRealVictimRange the victim is RW 0x701443(hero, distance, 0x62, 0, 0, 0) — flags CAN_ATTACK | UNFOGGED | AUTO_ACQUIRE of
// RW 0x7006D7. A nearer enemy the hero cannot attack (an AIRBORNE_TARGET for its ground weapon; the possible-to-attack filter of RW 0x660CA4) and a nearer
// NOT_AUTOACQUIRABLE enemy (0xC1D6AC) are both skipped, so the search answers nothing while only they are within the distance: the weapon set stays on the far
// one. Without this the ad-hoc chain took the nearest living enemy and the CLOSE_RANGE set was switched on for a target the hero would never fight.
TEST_CASE("PORT-459: DualWeaponBehavior's victim is RW 0x701443 with flags 0x62, a nearer refused enemy does not switch the weapon (RW 0x85E0C2)")
{
	Port459World f;
	Object *hero = f.unit("Port459DualHero", 'A', 400.0f, 400.0f);
	REQUIRE(hero);
	// the two refused objects stand between the hero (x 400) and the accepted one: distances 40, 60 and 300 (the distance is 200)
	Object *airborne = f.unit("Port459AirborneFoe", 'B', 440.0f, 400.0f);
	Object *notAuto = f.unit("Port459Unattackable", 'B', 460.0f, 400.0f);
	Object *attackable = f.unit("Port459Foe", 'B', 700.0f, 400.0f);
	REQUIRE(airborne);
	REQUIRE(notAuto);
	REQUIRE(attackable);
	// RW 0xD8AFF0's name table: the index the status bit has in retail
	REQUIRE(ObjectTemplateInfoBuilder::objectStatusIndex("AIRBORNE_TARGET") == kAirborneTargetStatus);
	airborne->setStatus((unsigned)kAirborneTargetStatus, true);
	REQUIRE_FALSE(hero->getWeapons()->canAttackObject(*airborne, CMD_FROM_AI, false)); // RW 0x68D6A6: the possible-to-attack answer
	REQUIRE(hero->getWeapons()->canAttackObject(*attackable, CMD_FROM_AI, false));
	const DualWeaponBehavior *dual = dualOf(*hero);
	REQUIRE(dual);
	f.frames(1); // the partition takes the placed objects in its update (RW 0xA3B4E0)

	// nothing the hero could attack is within the distance: the search answers null and the far weapon stays
	CHECK(AI::findClosestEnemy(*hero, 200.0f, AI::CAN_ATTACK | AI::UNFOGGED | AI::AUTO_ACQUIRE, nullptr, nullptr, 0) == nullptr);
	bool closeSeen = false;
	for (int i = 0; i < 30; ++i) // the module sleeps 5 frames between its updates
	{
		f.frames(1);
		closeSeen = closeSeen || f.closeRange(*hero);
	}
	CHECK_FALSE(closeSeen);

	// the attackable enemy steps inside the distance: the search answers it (not the nearer refused two) and the weapon switches
	Coord3D near = *attackable->getPosition();
	near.x = 520.0f;
	attackable->setPosition(&near);
	f.frames(1);
	CHECK(AI::findClosestEnemy(*hero, 200.0f, AI::CAN_ATTACK | AI::UNFOGGED | AI::AUTO_ACQUIRE, nullptr, nullptr, 0) == attackable);
	for (int i = 0; i < 40 && !closeSeen; ++i)
	{
		f.frames(1);
		closeSeen = f.closeRange(*hero);
	}
	CHECK(closeSeen);
	// the two refused objects are still alive and nearer: the switch is the victim search's answer, not a distance of the nearest object
	CHECK_FALSE(airborne->isEffectivelyDead());
	CHECK_FALSE(notAuto->isEffectivelyDead());
	CHECK_FALSE(attackable->isEffectivelyDead());
}

// A victim whose height difference is above 0.5 * the distance is none (RW 0x85E0CD .. 0x85E0EE: x87 fabs of the z difference against 0.5 * the distance):
// the enemy 100 away and 150 below is inside the distance but past the cutoff (0.5 * 200 = 100), so the far weapon comes back; 50 below is inside it.
TEST_CASE("PORT-459: DualWeaponBehavior's height cutoff drops a victim 150 below for a distance of 200 (RW 0x85E0CD)")
{
	Port459World f;
	Object *hero = f.unit("Port459DualHero", 'A', 400.0f, 400.0f);
	Object *foe = f.unit("Port459Foe", 'B', 500.0f, 400.0f); // 100 away, level with the hero
	REQUIRE(hero);
	REQUIRE(foe);
	// the switch happens for the level enemy (the module sleeps 5 frames between its updates)
	bool closeSeen = false;
	for (int i = 0; i < 30 && !closeSeen; ++i)
	{
		f.frames(1);
		closeSeen = f.closeRange(*hero);
	}
	REQUIRE(closeSeen);

	// 150 below: the search still finds the enemy (its 3D distance is 180 of 200), the module's cutoff drops it and the far weapon comes back
	Coord3D below = *foe->getPosition();
	below.z = -150.0f;
	foe->setPosition(&below);
	REQUIRE(AI::findClosestEnemy(*hero, 200.0f, AI::CAN_ATTACK | AI::UNFOGGED | AI::AUTO_ACQUIRE, nullptr, nullptr, 0) == foe);
	bool farSeen = false;
	for (int i = 0; i < 30 && !farSeen; ++i)
	{
		f.frames(1);
		farSeen = !f.closeRange(*hero);
	}
	CHECK(farSeen);

	// 50 below: inside the cutoff, the CLOSE_RANGE set turns on again
	below.z = -50.0f;
	foe->setPosition(&below);
	closeSeen = false;
	for (int i = 0; i < 30 && !closeSeen; ++i)
	{
		f.frames(1);
		closeSeen = f.closeRange(*hero);
	}
	CHECK(closeSeen);
}

// RW 0x7006D7 with AUTO_ACQUIRE (0x40): a caster whose own status carries NO_AUTO_ACQUIRE answers null before the scan, so DualWeaponBehavior's weapon
// stays on the far set even with an attackable enemy inside the distance. Clearing the status lets the next update find it.
TEST_CASE("PORT-459: with NO_AUTO_ACQUIRE the 0x62 search returns nothing, so DualWeaponBehavior keeps the far weapon (RW 0x7006D7)")
{
	Port459World f;
	Object *hero = f.unit("Port459DualHero", 'A', 400.0f, 400.0f);
	Object *foe = f.unit("Port459Foe", 'B', 500.0f, 400.0f);
	REQUIRE(hero);
	REQUIRE(foe);
	REQUIRE(ObjectTemplateInfoBuilder::objectStatusIndex("NO_AUTO_ACQUIRE") == kNoAutoAcquireStatus);
	hero->setStatus((unsigned)kNoAutoAcquireStatus, true);
	CHECK(AI::findClosestEnemy(*hero, 200.0f, AI::CAN_ATTACK | AI::UNFOGGED | AI::AUTO_ACQUIRE, nullptr, nullptr, 0) == nullptr);

	bool everClose = false;
	for (int i = 0; i < 15; ++i)
	{
		f.frames(1);
		everClose = everClose || f.closeRange(*hero);
	}
	CHECK_FALSE(everClose);

	// the status is gone: the next update switches for the same enemy
	hero->setStatus((unsigned)kNoAutoAcquireStatus, false);
	bool closeSeen = false;
	for (int i = 0; i < 40 && !closeSeen; ++i)
	{
		f.frames(1);
		closeSeen = f.closeRange(*hero);
	}
	CHECK(closeSeen);
}

// The neighbouring behaviour, kept: without an enemy in SwitchWeaponOnCloseRangeDistance the weapon set flag 7 stays off (RW 0x85DF88's empty victim
// branch, RW 0x691106 clears it). A hero with no CLOSE_RANGE weapon set still gets the flag and the WEAPONSTATE_CLOSE_RANGE model condition (RW 0x691059
// and Object + 0x120 bit 0 / RW 0x68B53C).
TEST_CASE("PORT-459: no victim in range leaves the close range set off; a weapon set without CLOSE_RANGE still switches (RW 0x85DF88)")
{
	Port459World f;
	Object *hero = f.unit("Port459DualHero", 'A', 400.0f, 400.0f);
	REQUIRE(hero);
	Object *far = f.unit("Port459Foe", 'B', 700.0f, 400.0f); // 300 away, the distance is 200
	REQUIRE(far);
	for (int i = 0; i < 30; ++i)
	{
		f.frames(1);
		CHECK_FALSE(f.closeRange(*hero));
	}
	// the same hero with a nearer enemy: the flag turns on
	Coord3D near = *far->getPosition();
	near.x = 500.0f;
	far->setPosition(&near);
	bool closeSeen = false;
	for (int i = 0; i < 40 && !closeSeen; ++i)
	{
		f.frames(1);
		closeSeen = f.closeRange(*hero);
	}
	CHECK(closeSeen);

	// a weapon set that names no CLOSE_RANGE condition: RW 0x691059 still sets the flag, and the model condition WEAPONSTATE_CLOSE_RANGE (160) follows
	Object *plain = f.unit("Port459PlainHero", 'A', 1000.0f, 400.0f);
	Object *plainFoe = f.unit("Port459Foe", 'B', 1100.0f, 400.0f);
	REQUIRE(plain);
	REQUIRE(plainFoe);
	static const int kCloseCondition = kCloseRangeModelCondition; // RW 0xD9FAD8's 160th name
	CHECK_FALSE(plain->testModelCondition(kCloseCondition));
	bool plainClose = false;
	for (int i = 0; i < 40 && !plainClose; ++i)
	{
		f.frames(1);
		plainClose = f.closeRange(*plain);
	}
	CHECK(plainClose);
	CHECK(plain->testModelCondition(kCloseCondition));
	// the weapon the flag selects is the CLOSE_RANGE set's weapon; the plain hero has none, so it keeps its only weapon (RW 0x68B58C chooses by flag)
	REQUIRE(plain->getWeapons());
	CHECK(plain->getWeapons()->currentWeapon() != nullptr);
}
