// OpenBFME. PORT-140 (issue #342, PROBLEMS.md P-11): a weapon that throws what its owner carries is ready only while the container
// holds an item its ProjectileFilterInContainer accepts.
//
// The owner-reported symptom: the creep troll of a lair on a map (Ennorath `CaveTroll_Slaved`, spawned by `CaveTrollLair`) plays the
// throw animation, no rock is there and nothing takes damage. The mod's default WeaponSet of that object is
//   WeaponSet / Conditions = None / PRIMARY MordorCaveTrollRockThrow / SECONDARY MordorCaveTrollPunch / TERTIARY MordorCaveTrollBash
// with `PreferredAgainst = PRIMARY STRUCTURE BLOCKING_GATE`, and the rock weapon carries
//   ProjectileFilterInContainer = ANY +ROCK +INFANTRY   ; "We don't make the projectile, it is in our Contain module."
//   ProjectileNugget / WarheadTemplateName = MordorCaveTrollRockThrowWarhead   (no ProjectileTemplateName)
// so the rock it throws is the object the troll's TransportContain holds (a rock, or an orc it picked up) — retail's own words.
// The troll reaches that set without any upgrade because the mod re-specifies the whole WeaponSet on the child; the "don't start with
// a rock" intent of the stock data sits in `AILuaEventsList = CreepTrollFunctions`, whose OnCreated grants no
// Upgrade_SwitchToRockThrowing (scripts.lua:36-39 hides Trunk01 and ROCK only).
//
// What RW does — the mechanism the port was missing (S-001 caveat on every address):
//   * Weapon::getRemainingAmmo RW 0x6CD298: `mov eax,[esi+4]` (the WeaponTemplate), `lea ecx,[eax+0x120]` (ProjectileFilterInContainer),
//     `call 0x762977` (the filter has a rule); with one, the owner's contain `[Object+0x258]` (RW 0x68B678 reads +0x31C of the Object
//     and jumps to RW 0x79FD6F, `mov eax,[ecx+0x30]` -> the module at +0x30) is asked `push &filter; call [eax+0x114]`.
//   * contain slot 0x114 = OpenContain RW 0x865CE3: `cmp [ebp+8],ebx` (no filter) -> `mov eax,[edi+0x38]` (the contained count);
//     with a filter, walk the list (`call [eax+0x118]`) and `inc ebx` for every item `call 0x7640C1` accepts. RW 0x7640C1 ->
//     ObjectFilter::testTemplate RW 0x763543 with the item's and the container's players (`call 0x68B678` = getControllingPlayer;
//     RW 0x763543 tests +4 of the item's template against the filter and the relationship to the container's player).
//     TransportContain RW 0x86B14D and GarrisonContain RW 0x87B8A9 hold their own bodies of the same slot.
//   * Weapon::getStatus RW 0x6CD142 reads that number before it answers: `cmp [esi+0x78],0 / jl` splits IdleAfterFiringDelay, in both
//     halves `call 0x6CD298` decides, so the status is READY_TO_FIRE only while the container holds a matching item and OUT_OF_AMMO
//     when it does not. That is what makes the troll fall back to the punch/bash of its set while it carries no rock.
//   * Weapon host method RW 0x6CD11F (the second accessor, `ownerHasContainAmmo`) -> RW 0x68B634 -> RW 0x8F1149
//     (`mov eax,[ecx+0x24]; ret`, the containment filter's rule at Object+0x370) is the same question through the template's own filter.
//
// The port already had the Weapon half (Weapon::getRemainingAmmo / getStatus read the host), but the live host — ObjectWeapons::Host —
// answered "no contain", so every filtered weapon looked loaded for ever: the rock weapon was ready, its ProjectileNugget has no
// ProjectileTemplateName and launches nothing, and the melee weapons were never chosen. The engine change (ObjectWeapons.cpp) answers
// the two host methods from ContainModuleInterface / ObjectFilterMatch; these tests hold it and the neighbour that must not move.
//
// Synthetic data only: no retail / Ennorath / mod file, value or byte. Every test name begins PORT-140 so the lane's narrow filter is
// -tc="PORT-140*".

#include "doctest.h"

#include "CombatTestUtil.h"

#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Combat/ObjectWeapons.h"
#include "GameLogic/Module/TransportContain.h"
#include "GameLogic/Object/Contain/TransportContainRuntime.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/Weapon.h"

#include <string>

using namespace combattest;

namespace
{
// The mod's shape, reduced to what the choice needs: a melee weapon (MordorCaveTrollPunch's MeleeWeapon / STANDARD_MELEE_ATTACK_RANGE
// and DamageDealtAtSelfPosition kept) and a rock weapon with ProjectileFilterInContainer, HitPercentage 0 and a ProjectileNugget whose
// ProjectileTemplateName is absent — exactly what makes a wrongly-ready rock weapon do nothing at all.
const char *const kPort140Weapons =
	"Weapon Port140Punch\n"
	"  MeleeWeapon = Yes\n"
	"  AttackRange = 11.5\n"
	"  DelayBetweenShots = 1000\n"
	"  PreAttackDelay = 200\n"
	"  PreAttackType = PER_SHOT\n"
	"  DamageDealtAtSelfPosition = Yes\n"
	"  DamageNugget\n"
	"    Damage = 10\n"
	"    Radius = 57\n"
	"    DamageArc = 45\n"
	"    DamageType = SIEGE\n"
	"    DeathType = NORMAL\n"
	"  End\n"
	"End\n"
	"Weapon Port140Rock\n"
	"  AttackRange = 200\n"
	"  WeaponSpeed = 240\n"
	"  HitPercentage = 0\n"
	"  ProjectileFilterInContainer = ANY +ROCK +INFANTRY\n"
	"  DelayBetweenShots = 2000\n"
	"  PreAttackDelay = 400\n"
	"  PreAttackType = PER_SHOT\n"
	"  ProjectileNugget\n"
	"    WarheadTemplateName = Port140RockWarhead\n"
	"  End\n"
	"End\n"
	"Weapon Port140RockWarhead\n"
	"  DamageNugget\n"
	"    Damage = 40\n"
	"    Radius = 16\n"
	"    DamageType = SIEGE\n"
	"    DeathType = NORMAL\n"
	"  End\n"
	"End\n";

// The troll: the set's default block is the melee punch, the conditional block (WEAPONSET_TOGGLE_1) is the rock. That is the shape the
// mod's EnemyObject templates have (GoblinCaveTroll's own default is melee with the bash against structures), and it keeps the test
// about the weapon-status question rather than about the set's Conditions.
const std::string kPort140Troll =
	std::string(kPort140Weapons) +
	"Object Port140Troll\n"
	            "  KindOf = INFANTRY SELECTABLE MONSTER\n"
	            "  TransportSlotCount = 1\n"
	            "  Geometry = CYLINDER\n"
	            "  GeometryMajorRadius = 17.6\n"
	            "  GeometryMinorRadius = 17.6\n"
	            "  GeometryHeight = 32\n"
	            // The mod's shape: the throwing weapon and the melee weapons sit in ONE set, so the choice (RW 0x6C8A4E, which walks
	            // the active set only) falls from the rock to the punch on the rock's own status. The second set is the neighbour: the
	            // WEAPONSET_TOGGLE_1 switch replaces the whole set, and the punch is still there.
	            "  WeaponSet\n"
	            "    Conditions = None\n"
	            "    Weapon = PRIMARY Port140Rock\n"
	            "    Weapon = SECONDARY Port140Punch\n"
	            "  End\n"
	            "  WeaponSet\n"
	            "    Conditions = WEAPONSET_TOGGLE_1\n"
	            "    Weapon = PRIMARY Port140Punch\n"
	            "  End\n"
	            "  Body = ActiveBody ModuleTag_Body\n"
	            "    MaxHealth = 1000\n"
	            "  End\n"
	            "  Behavior = TransportContain ModuleTag_Rock\n"
	            "    Slots = 1\n"
	            "    AllowEnemiesInside = Yes\n"
	            "    ObjectStatusOfContained = UNSELECTABLE\n"
	            "  End\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = NoArmor\n"
	"  End\n"
	"End\n"
	"Object Port140RockItem\n"
	"  KindOf = ROCK NO_FREEWILL_ENTER\n"
	"  TransportSlotCount = 1\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 4\n"
	"  GeometryMinorRadius = 4\n"
	"  GeometryHeight = 6\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = NoArmor\n"
	"  End\n"
	"End\n"
	"Object Port140OtherItem\n"
	"  KindOf = NO_FREEWILL_ENTER\n"
	"  TransportSlotCount = 1\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 4\n"
	"  GeometryMinorRadius = 4\n"
	"  GeometryHeight = 6\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = NoArmor\n"
	"  End\n"
	"End\n";

// The WEAPONSET_TOGGLE_1 switch, RW 0x691059 / 0x691106 (Object::setWeaponSetFlag, the port's ObjectWeapons::setWeaponSetFlag): the
// set update RW 0x6C99E2 re-runs FindWeaponTemplateSet (RW 0x73D89F) and the current slot is chosen again by
// chooseBestWeaponForTarget RW 0x6C8A4E. The tests call it the way the Toggle Weaponset command (WeaponSetToggle) does.
int kToggle1 = 24; // WEAPONSET_TOGGLE_1 is index 24 of TheWeaponConditionNames (GameLogic/BitFlagNames.cpp:175); CombatNames::weaponSetBit returns it

void setToggle1(Object &troll)
{
	troll.getWeapons()->setWeaponSetFlag(kToggle1, true);
	REQUIRE(troll.getWeapons()->templateSet() != nullptr);
}

std::string currentWeaponName(Object &obj)
{
	REQUIRE(obj.getWeapons() != nullptr);
	const Weapon *w = obj.getWeapons()->currentWeapon();
	REQUIRE(w != nullptr);
	REQUIRE(w->getTemplate() != nullptr);
	return w->getTemplate()->getName();
}
} // namespace

TEST_CASE("PORT-140: the host reports the container's filtered ammo (RW 0x6CD298 -> contain slot 0x114 RW 0x865CE3), and the rock weapon is ready only while a filtered item is carried")
{
	CombatWorld g(kPort140Troll.c_str());
	REQUIRE(CombatNames::weaponSetBit("WEAPONSET_TOGGLE_1") == kToggle1);
	Object *troll = g.unit("Port140Troll", 'A', 40.0f, 40.0f);
	REQUIRE(troll != nullptr);
	TransportContain *contain = dynamic_cast<TransportContain *>(troll->getContain());
	REQUIRE(contain != nullptr);

	ObjectWeapons *weapons = troll->getWeapons();
	REQUIRE(weapons->templateSet() != nullptr);
	// the first set is the mod's: the rock in PRIMARY, the punch in SECONDARY
	Weapon *rock = weapons->weaponInSlot(PRIMARY_WEAPON);
	REQUIRE(rock != nullptr);
	REQUIRE(rock->getTemplate() != nullptr);
	CHECK(std::string(rock->getTemplate()->getName()) == "Port140Rock");
	REQUIRE(weapons->weaponInSlot(SECONDARY_WEAPON) != nullptr);
	CHECK(std::string(weapons->weaponInSlot(SECONDARY_WEAPON)->getTemplate()->getName()) == "Port140Punch");

	// No rock carried: the weapon's ProjectileFilterInContainer accepts nothing that is in the container, so RW 0x6CD142 answers
	// OUT_OF_AMMO for that weapon (weaponStatus runs it through the live host, i.e. RW 0x6CD298 -> contain slot 0x114).
	REQUIRE(troll->getContain() != nullptr);
	REQUIRE(troll->getContain()->getContainCount() == 0u);
	CHECK(weapons->weaponStatus(*rock) == WEAPON_OUT_OF_AMMO);

	// A carried object the filter does not accept (neither ROCK nor INFANTRY) keeps it empty.
	Object *other = g.unit("Port140OtherItem", 'A', 41.0f, 40.0f);
	REQUIRE(other != nullptr);
	REQUIRE(troll->getContain()->addToContain(other));
	CHECK(weapons->weaponStatus(*rock) == WEAPON_OUT_OF_AMMO);

	// The carried rock is the ammo: RW 0x865CE3 counts it (the filter accepts ROCK) and RW 0x6CD298 hands the count to getStatus.
	Object *stone = g.unit("Port140RockItem", 'A', 41.0f, 41.0f);
	REQUIRE(stone != nullptr);
	REQUIRE(troll->getContain()->addToContain(stone));
	CHECK(weapons->weaponStatus(*rock) == WEAPON_READY_TO_FIRE);

	// The other weapons of the set are untouched by the container: the punch has no filter, its own clip answers.
	Weapon *punch = weapons->weaponInSlot(SECONDARY_WEAPON);
	REQUIRE(punch != nullptr);
	CHECK_FALSE(punch->getTemplate()->m_projectileFilterInContainer.flag);
	CHECK(weapons->weaponStatus(*punch) == WEAPON_READY_TO_FIRE);
}

TEST_CASE("PORT-140: the creep troll fights in melee while it carries no rock and throws only while it does (P-11)")
{
	CombatWorld g(kPort140Troll.c_str());
	REQUIRE(CombatNames::weaponSetBit("WEAPONSET_TOGGLE_1") == kToggle1);
	Object *troll = g.unit("Port140Troll", 'A', 40.0f, 40.0f);
	REQUIRE(troll != nullptr);
	Object *victim = g.unit("Port140Troll", 'B', 46.0f, 40.0f); // an enemy 6 units away, inside the melee reach of 11.5
	REQUIRE(victim != nullptr);
	REQUIRE(troll->getWeapons() != nullptr);

	// The troll carries nothing: RW 0x6C8A4E walks the slots of the set, the rock's status is OUT_OF_AMMO (RW 0x6CD142 through
	// RW 0x6CD298 -> contain slot 0x114) and the punch is the weapon that can fire. Before the fix the rock answered READY and was
	// kept: it has no ProjectileTemplateName, so it threw nothing and the troll never fell back.
	REQUIRE(troll->getContain() != nullptr);
	REQUIRE(troll->getContain()->getContainCount() == 0u);
	CHECK(troll->getWeapons()->chooseBestWeaponForTarget(victim, PREFER_TEMPLATE_DEFAULT, CMD_FROM_AI)); // RW 0x6C8A4E
	CHECK(currentWeaponName(*troll) == "Port140Punch");

	// The other half of the same mechanism: with the carried rock the throwing weapon is ready and wins the choice.
	Object *stone = g.unit("Port140RockItem", 'A', 41.0f, 41.0f);
	REQUIRE(stone != nullptr);
	REQUIRE(troll->getContain()->addToContain(stone));
	CHECK(troll->getWeapons()->chooseBestWeaponForTarget(victim, PREFER_TEMPLATE_DEFAULT, CMD_FROM_AI));
	CHECK(currentWeaponName(*troll) == "Port140Rock");
	// ... and back: the rock is spent (it leaves the container), the punch is chosen again.
	troll->getContain()->removeFromContain(stone);
	REQUIRE(troll->getContain()->getContainCount() == 0u);
	CHECK(troll->getWeapons()->chooseBestWeaponForTarget(victim, PREFER_TEMPLATE_DEFAULT, CMD_FROM_AI));
	CHECK(currentWeaponName(*troll) == "Port140Punch");

	// The punch does damage (the owner-reported "no damage" half): 10 SIEGE on NoArmor is exactly 10 off 1000.
	const float before = g.health(victim);
	REQUIRE(victim->getBodyModule() != nullptr);
	REQUIRE(before == 1000.0f);
	troll->getWeapons()->fireCurrentWeapon(victim, nullptr);
	const int ran = g.runUntil([&] { return g.health(victim) < before; }, 40);
	MESSAGE("the punch was delivered after " << ran << " frames");
	CHECK(g.health(victim) == before - 10.0f);
}

TEST_CASE("PORT-140: a weapon without ProjectileFilterInContainer is untouched by the container (the neighbour that must not move)")
{
	// RW 0x6CD298 tests the filter first (`lea ecx,[eax+0x120]; call 0x762977`): with no rule the contain is never asked and the clip
	// answers, so a melee weapon keeps firing while its owner carries nothing. This is the guard that the change did not turn every
	// weapon into a container-fed one.
	CombatWorld g(kPort140Troll.c_str());
	Object *troll = g.unit("Port140Troll", 'A', 40.0f, 40.0f);
	REQUIRE(troll != nullptr);
	Object *victim = g.unit("Port140Troll", 'B', 46.0f, 40.0f);
	REQUIRE(victim != nullptr);
	REQUIRE(troll->getWeapons() != nullptr);
	REQUIRE(troll->getContain() != nullptr);
	REQUIRE(troll->getContain()->getContainCount() == 0u);

	Weapon *punch = troll->getWeapons()->weaponInSlot(SECONDARY_WEAPON);
	REQUIRE(punch != nullptr);
	CHECK_FALSE(punch->getTemplate()->m_projectileFilterInContainer.flag);
	CHECK(troll->getWeapons()->weaponStatus(*punch) == WEAPON_READY_TO_FIRE);

	// ... and the choice lands on it with the container empty (the filtered rock weapon in PRIMARY is out of ammo)
	CHECK(troll->getWeapons()->chooseBestWeaponForTarget(victim, PREFER_TEMPLATE_DEFAULT, CMD_FROM_AI));
	CHECK(currentWeaponName(*troll) == "Port140Punch");

	// ... and it stays ready over frames with the container empty: it is not the container that feeds it
	g.runUntil([&] { return false; }, 20);
	CHECK(troll->getWeapons()->weaponStatus(*punch) == WEAPON_READY_TO_FIRE);

	// The WEAPONSET_TOGGLE_1 switch replaces the whole set (RW 0x691059 -> the set update RW 0x6C99E2 -> FindWeaponTemplateSet
	// RW 0x73D89F). That set holds the punch alone, so the answer does not depend on which set the troll wears here.
	setToggle1(*troll);
	REQUIRE(troll->getWeapons()->templateSet() != nullptr);
	CHECK(currentWeaponName(*troll) == "Port140Punch");
	Weapon *togglePunch = troll->getWeapons()->weaponInSlot(PRIMARY_WEAPON);
	REQUIRE(togglePunch != nullptr);
	CHECK(troll->getWeapons()->weaponStatus(*togglePunch) == WEAPON_READY_TO_FIRE);
}
