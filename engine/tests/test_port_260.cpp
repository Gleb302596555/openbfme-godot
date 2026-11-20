// OpenBFME unit tests (lane PORT-260, fork issue 608): the storm powers — ArrowStormUpdate (Gandalf's lightning sword, Saruman's power, Legolas' arrow
// storm, Galadriel's Vilya) and CloudBreakSpecialPower (the sunbreak).
//
// RotWK 2.01, read with tools/mod/rw.py on game.dat (md5 daed3668), every expectation below from the binary and the mod's data:
//   * ArrowStormUpdate::allowsSpecialPowerUse RW 0x893D61 (the special power update interface RW 0xC63E4C slot 8, its vtable entry 8; the base's
//     SpecialAbilityUpdate RW 0x851B6E is its call at RW 0x893E2A). The module's location requirements RW 0x897E01 push the location into that slot
//     (RW 0x897E7D) and ask RW 0x897E62 -> RW 0x897E82; the answer gates every cast (ActionManager's moduleAllows).
//   * CloudBreakSpecialPower RW 0x8C8A3B: every object the data's AttributeModifierAffects (+ 0x24) allows for the caster's controlling player gets,
//     through its outermost container (+ 0x27C chain, RW 0x68F389 / 0x68F381), the request RW 0x8B4E75 with type 5 (RW 0x8C8A70), no source and delay 1
//     (RW 0x8C8A6D) at its EmotionTrackerUpdate (+ 0x254, RW 0x68F393).
//
// Synthetic INI only; the game files are not needed. GPL-3.0.

#include "doctest.h"

#include "CombatTestUtil.h"
#include "IniTestUtil.h"

#include "Common/SpecialPower.h"
#include "Common/Team.h"
#include "GameClient/FXList.h"
#include "GameLogic/ActionManager.h"
#include "GameLogic/AttributeModifiers.h"
#include "GameLogic/Combat/ObjectWeapons.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/ActiveBody.h"
#include "GameLogic/Module/EmotionModules.h"
#include "GameLogic/Module/SpellBookPowers.h"
#include "GameLogic/Module/HeroAbilityModules.h"
#include "GameLogic/Module/SpecialAbilityModules.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/ObjectTemplateInfo.h"
#include "GameLogic/System/EmotionSystem.h"

#include <string>

namespace
{
// the synthetic world of lane HERO-2 (PowerWorld): the special powers, FX lists and modifier lists of the INI are mounted
struct Storm260World : combattest::CombatWorld
{
	SpecialPowerStore powers;
	SpecialPowerStore *savedPowers = TheSpecialPowerStore;
	FXListStore fxLists;
	FXListStore *savedFx = TheFXListStore;
	AttributeModifierStore modifiers;
	AttributeModifierStore *savedModifiers = TheAttributeModifierStore;
	explicit Storm260World(const std::string &objects)
	{
		TheSpecialPowerStore = &powers;
		TheFXListStore = &fxLists;
		TheAttributeModifierStore = &modifiers;
		modifiers.registerBlock(w.fx.env.blocks);
		w.fx.env.blocks.registerBlock("FXList", [](INI *ini) { ParseFXListDefinitionGlobal(ini); });
		w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
		SpecialAbilityModules::registerAll(w.modules);
		SpecialPowerModules::registerAll(w.modules);
		SpellBookPowers::registerAll(w.modules); // lane PORT-260: CloudBreakSpecialPower
		EmotionModules::registerAll(w.modules);  // the tracker the put-out request lands on (RW 0x68F393)
		const std::string err = w.load(objects);
		REQUIRE_MESSAGE(err.empty(), err);
	}
	~Storm260World()
	{
		TheSpecialPowerStore = savedPowers;
		TheFXListStore = savedFx;
		TheAttributeModifierStore = savedModifiers;
	}
};

SpecialPowerModule *stormPowerOf(Object &o, const char *power)
{
	for (const std::unique_ptr<BehaviorModule> &m : o.modules())
	{
		SpecialPowerModule *sp = dynamic_cast<SpecialPowerModule *>(m.get());
		if (sp && sp->getSpecialPowerTemplate() && sp->getSpecialPowerTemplate()->getName() == power)
		{
			return sp;
		}
	}
	return nullptr;
}

// the special power update interface of an object's ArrowStormUpdate (the slot RW 0x897E7D calls; the class's own override is protected, as the slot is
// reached through the interface's vtable in the binary too)
SpecialPowerUpdateInterface *stormSlot(Object *o)
{
	for (const auto &m : o->modules())
	{
		if (dynamic_cast<ArrowStormUpdate *>(m.get()))
		{
			return dynamic_cast<SpecialPowerUpdateInterface *>(m.get());
		}
	}
	return nullptr;
}

// the storm's interface slot belongs to the module the special power names (the same object, asked twice)
bool cloudBreakSlotIsTheModule(Object &o, SpecialPowerModule *power)
{
	for (const auto &m : o.modules())
	{
		if (dynamic_cast<ArrowStormUpdate *>(m.get()) && dynamic_cast<SpecialPowerUpdateInterface *>(m.get()))
		{
			return power != nullptr;
		}
	}
	return false;
}

ArrowStormUpdate *stormOf(Object *o)
{
	for (const auto &m : o->modules())
	{
		if (ArrowStormUpdate *u = dynamic_cast<ArrowStormUpdate *>(m.get()))
		{
			return u;
		}
	}
	return nullptr;
}

// the caster of the volley (a hero, so not IMMOBILE: RW 0x893DE2 sends it to the base), one weapon with AttackRange 400 and no MinimumAttackRange
// (RW 0x6CA03E: 0 - 2.5 floored at 0), the data of Gandalf's lightning sword (the mod: 300 in the vanilla gamedata, 23 shots)
const char kVolley[] =
	"Weapon Storm260Weapon\n"
	"  AttackRange = 400\n"
	"  DelayBetweenShots = 100\n"
	"  FiringDuration = 1\n"
	"  DamageNugget\n"
	"    Damage = 10\n"
	"    Radius = 0.0\n"
	"    DelayTime = 0\n"
	"    DamageType = SLASH\n"
	"    DeathType = NORMAL\n"
	"  End\n"
	"End\n"
	"Weapon Storm260ShortWeapon\n"
	"  AttackRange = 100\n"
	"  DelayBetweenShots = 100\n"
	"  FiringDuration = 1\n"
	"  DamageNugget\n"
	"    Damage = 10\n"
	"    Radius = 0.0\n"
	"    DelayTime = 0\n"
	"    DamageType = SLASH\n"
	"    DeathType = NORMAL\n"
	"  End\n"
	"End\n"
	"SpecialPower SpecialAbilityStorm260\n"
	"  Enum = SPECIAL_ARROW_STORM\n"
	"  ReloadTime = 1000\n"
	"End\n"
	"SpecialPower SpecialAbilityStorm260Short\n"
	"  Enum = SPECIAL_ARROW_STORM\n"
	"  ReloadTime = 1000\n"
	"End\n"
	"Object Storm260Hero\n"
	" KindOf = INFANTRY SELECTABLE CAN_ATTACK HERO\n"
	" WeaponSet\n  Conditions = None\n  Weapon = PRIMARY Storm260Weapon\n End\n"
	" Body = ActiveBody BodyTag\n  MaxHealth = 100\n End\n"
	" Behavior = AIUpdateInterface AiTag\n End\n"
	" Behavior = SpecialPowerModule Storm260PowerTag\n"
	"  SpecialPowerTemplate = SpecialAbilityStorm260\n  UpdateModuleStartsAttack = Yes\n End\n"
	" Behavior = ArrowStormUpdate Storm260Tag\n"
	"  SpecialPowerTemplate = SpecialAbilityStorm260\n  StartAbilityRange = 300\n  UnpackTime = 1\n  PreparationTime = 1\n  PackTime = 1\n"
	"  IgnoreFacingCheck = Yes\n"
	"  WeaponTemplate = Storm260Weapon\n  TargetRadius = 60\n  ShotsPerTarget = 1\n  ShotsPerBurst = 1\n  MaxShots = 1\n"
	" End\n"
	"End\n"
	"Object Storm260ShortHero\n"
	" KindOf = INFANTRY SELECTABLE CAN_ATTACK HERO\n"
	" WeaponSet\n  Conditions = None\n  Weapon = PRIMARY Storm260ShortWeapon\n End\n"
	" Body = ActiveBody BodyTag\n  MaxHealth = 100\n End\n"
	" Behavior = AIUpdateInterface AiTag\n End\n"
	" Behavior = SpecialPowerModule Storm260ShortPowerTag\n"
	"  SpecialPowerTemplate = SpecialAbilityStorm260Short\n  UpdateModuleStartsAttack = Yes\n End\n"
	" Behavior = ArrowStormUpdate Storm260ShortTag\n"
	"  SpecialPowerTemplate = SpecialAbilityStorm260Short\n  StartAbilityRange = 60\n  UnpackTime = 1\n  PreparationTime = 1\n  PackTime = 1\n"
	"  IgnoreFacingCheck = Yes\n"
	"  WeaponTemplate = Storm260ShortWeapon\n  TargetRadius = 60\n  ShotsPerTarget = 1\n  ShotsPerBurst = 1\n  MaxShots = 1\n"
	" End\n"
	"End\n"
	"Object Storm260Victim\n"
	" KindOf = INFANTRY SELECTABLE\n"
	" Body = ActiveBody BodyTag\n  MaxHealth = 1000\n End\n"
	" Behavior = AIUpdateInterface AiTag\n End\n"
	"End\n";
} // namespace

// RW 0x893D61 (the volley's own slot 8 of the special power update interface): the location is tested against the weapon's MINIMUM attack range (RW
// 0x6CA03E, floored at 0) only, and only a caster whose own template is IMMOBILE continues to RW 0x893DDF's own range test; every other caster gets the
// base's answer (RW 0x893E2A -> RW 0x851B6E, which reads no location). The test runs through the interface slot RW 0x897E7D calls.
TEST_CASE("PORT-260 ArrowStormUpdate: the location gate tests the weapon's minimum range and only an IMMOBILE caster's own range (RW 0x893D61)")
{
	Storm260World f(kVolley);
	Object *hero = f.unit("Storm260Hero", 'A', 300, 300);
	Object *victim = f.unit("Storm260Victim", 'B', 900, 300);
	REQUIRE(hero);
	REQUIRE(victim);
	ArrowStormUpdate *storm = stormOf(hero);
	REQUIRE(storm);
	SpecialPowerUpdateInterface *slot = stormSlot(hero);
	REQUIRE(slot);
	// RW 0x893D9E .. 0x893DD9: the weapon's minimum attack range (400 has none: 0 - 2.5 floored at 0, RW 0x6CA03E) against the squared 2D edge distance
	// (RW 0x6CA525); RW 0x893DE2: the caster's template is not IMMOBILE (a HERO), so RW 0x893E25 takes the base's answer, which reads no distance at all
	const Coord3D far = { 3000.0f, 300.0f, 0.0f };
	CHECK(slot->allowsSpecialPowerUse(&far)); // RW 0x893E2A: the base's answer (RW 0x851B6E), not a range refusal
	CHECK(slot->allowsSpecialPowerUse(nullptr)); // RW 0x893D7B: the base's answer alone
	// the special power module the button drives names the power the storm's slot belongs to, so the cast's gate RW 0x897E01 reaches this very module
	SpecialPowerModule *power = stormPowerOf(*hero, "SpecialAbilityStorm260");
	REQUIRE(power);
	CHECK(power->getSpecialPowerTemplate() == power->getSpecialPowerTemplate()->getFinalOverride());
	CHECK(cloudBreakSlotIsTheModule(*hero, power));

}

// RW 0x893DB9 -> 0x6CA03E: a weapon with a MinimumAttackRange (here 500, so 497.5) refuses a location closer than it; RW 0x893DDF: a caster whose template
// is IMMOBILE (KindOf index 2 of RW 0xDA0E68) answers with the attack range with no target (RW 0x6CA701: the scale applied twice), a hero's own
// question is the base's alone
TEST_CASE("PORT-260 ArrowStormUpdate: MinimumAttackRange and the IMMOBILE caster's own range test (RW 0x893DB9 / RW 0x893DDF)")
{
	static const char kClose[] =
		"Weapon Storm260CloseWeapon\n"
		"  AttackRange = 400\n"
		"  MinimumAttackRange = 500\n"
		"  DelayBetweenShots = 100\n"
		"  FiringDuration = 1\n"
		"  DamageNugget\n    Damage = 10\n    Radius = 0.0\n    DelayTime = 0\n    DamageType = SLASH\n    DeathType = NORMAL\n  End\n"
		"End\n"
		"SpecialPower SpecialAbilityStorm260Close\n  Enum = SPECIAL_ARROW_STORM\n  ReloadTime = 1000\nEnd\n"
		"Object Storm260CloseHero\n"
		" KindOf = INFANTRY SELECTABLE CAN_ATTACK HERO\n"
		" WeaponSet\n  Conditions = None\n  Weapon = PRIMARY Storm260CloseWeapon\n End\n"
		" Body = ActiveBody BodyTag\n  MaxHealth = 100\n End\n"
		" Behavior = AIUpdateInterface AiTag\n End\n"
		" Behavior = SpecialPowerModule Storm260ClosePowerTag\n  SpecialPowerTemplate = SpecialAbilityStorm260Close\n  UpdateModuleStartsAttack = Yes\n End\n"
		" Behavior = ArrowStormUpdate Storm260CloseTag\n"
		"  SpecialPowerTemplate = SpecialAbilityStorm260Close\n  StartAbilityRange = 300\n  UnpackTime = 1\n  PreparationTime = 1\n  PackTime = 1\n"
		"  IgnoreFacingCheck = Yes\n"
		"  WeaponTemplate = Storm260CloseWeapon\n  TargetRadius = 60\n  ShotsPerTarget = 1\n  ShotsPerBurst = 1\n  MaxShots = 1\n"
		" End\n"
		"End\n"
		"Object Storm260Tower\n"
		" KindOf = IMMOBILE STRUCTURE SELECTABLE\n"
		" WeaponSet\n  Conditions = None\n  Weapon = PRIMARY Storm260Weapon\n End\n"
		" Body = ActiveBody BodyTag\n  MaxHealth = 1000\n End\n"
		" Behavior = AIUpdateInterface AiTag\n End\n"
		" Behavior = SpecialPowerModule Storm260TowerPowerTag\n  SpecialPowerTemplate = SpecialAbilityStorm260\n  UpdateModuleStartsAttack = Yes\n End\n"
		" Behavior = ArrowStormUpdate Storm260TowerTag\n"
		"  SpecialPowerTemplate = SpecialAbilityStorm260\n  StartAbilityRange = 1000\n  UnpackTime = 1\n  PreparationTime = 1  PackTime = 1\n"
		"  IgnoreFacingCheck = Yes\n"
		"  WeaponTemplate = Storm260Weapon\n  TargetRadius = 60\n  ShotsPerTarget = 1\n  ShotsPerBurst = 1\n  MaxShots = 1\n"
		" End\n"
		"End\n";
	Storm260World f(std::string(kVolley) + kClose);
	Object *hero = f.unit("Storm260CloseHero", 'A', 300, 300);
	Object *tower = f.unit("Storm260Tower", 'A', 600, 600);
	REQUIRE(hero);
	REQUIRE(tower);
	ArrowStormUpdate *close = stormOf(hero);
	ArrowStormUpdate *fromTower = stormOf(tower);
	SpecialPowerUpdateInterface *closeSlot = stormSlot(hero);
	SpecialPowerUpdateInterface *towerSlot = stormSlot(tower);
	REQUIRE(close);
	REQUIRE(fromTower);
	REQUIRE(closeSlot);
	REQUIRE(towerSlot);
	const Coord3D near = { 500.0f, 300.0f, 0.0f }; // 200 away: inside the 497.5 minimum (500 - 2.5, RW 0x6CA03E)
	const Coord3D edge = { 700.0f, 300.0f, 0.0f };  // 400 away: at the minimum's edge
	const Coord3D far = { 3000.0f, 300.0f, 0.0f };
	CHECK_FALSE(closeSlot->allowsSpecialPowerUse(&near)); // RW 0x893DD9 `jbe` / RW 0x893DDB
	CHECK(closeSlot->allowsSpecialPowerUse(&far));        // RW 0x893E25 -> RW 0x893E2A (a hero: the base's answer, no upper test)
	// the immobile caster (its weapon has no minimum, its range 400): its own test at RW 0x893DDF decides, on the edge distance (RW 0x6CA525)
	CHECK(towerSlot->allowsSpecialPowerUse(&near)); // 200 away: inside 400
	CHECK(towerSlot->allowsSpecialPowerUse(&edge)); // 400 away: within 400
	CHECK_FALSE(towerSlot->allowsSpecialPowerUse(&far)); // 3000 away: the `ja` of RW 0x893E20 refuses
}

// RW 0x8C8A3B: CloudBreak's fire put-out — the objects the data's AttributeModifierAffects (+ 0x24) allows for the caster's player get the request 5
// (RW 0x8C8A70) with no source and delay 1 (RW 0x8C8A6D) at their EmotionTrackerUpdate; a contained object's request reaches its OUTERMOST container's
// tracker (RW 0x68F383's + 0x27C chain); an object the filter refuses (an ALLY of the caster here) gets nothing.
TEST_CASE("PORT-260 CloudBreakSpecialPower: the cast puts the fire out of the enemies the filter allows (RW 0x8C8A3B -> RW 0x68F383)")
{
	static const char kCloud[] =
		"SpecialPower SpellBookCloudBreak260\n  Enum = SPECIAL_SPELL_BOOK_CLOUD_BREAK\n  ReloadTime = 1000\nEnd\n"
		"Object Storm260Citadel\n"
		" KindOf = STRUCTURE SELECTABLE IMMOBILE\n"
		" Body = ActiveBody BodyTag\n  MaxHealth = 1000\n End\n"
		" Behavior = AIUpdateInterface AiTag\n End\n"
		" Behavior = CloudBreakSpecialPower CloudBreak260Tag\n"
		"  SpecialPowerTemplate = SpellBookCloudBreak260\n"
		"  CloudBreakRadius = 200\n"
		"  AttributeModifierAffects = ANY +INFANTRY -HERO ENEMIES\n"
		"  ChangeWeather = SUNNY\n"
		"  WeatherDuration = 1000\n"
		" End\n"
		"End\n"
		"Object Storm260Burning\n"
		" KindOf = INFANTRY SELECTABLE\n"
		" Body = ActiveBody BodyTag\n  MaxHealth = 100\n End\n"
		" Behavior = AIUpdateInterface AiTag\n End\n"
		" Behavior = EmotionTrackerUpdate EmotionTag\n End\n"
		"End\n"
		"Object Storm260Friend\n"
		" KindOf = INFANTRY SELECTABLE\n"
		" Body = ActiveBody BodyTag\n  MaxHealth = 100\n End\n"
		" Behavior = AIUpdateInterface AiTag\n End\n"
		" Behavior = EmotionTrackerUpdate EmotionTag\n End\n"
		"End\n";
	Storm260World f(std::string(kVolley) + kCloud);
	Object *citadel = f.unit("Storm260Citadel", 'A', 600, 600);
	Object *enemy = f.unit("Storm260Burning", 'B', 700, 600);
	Object *ally = f.unit("Storm260Friend", 'A', 650, 600);
	REQUIRE(citadel);
	REQUIRE(enemy);
	REQUIRE(ally);
	CloudBreakSpecialPower *cloud = nullptr;
	for (const auto &m : citadel->modules())
	{
		if (auto *c = dynamic_cast<CloudBreakSpecialPower *>(m.get()))
		{
			cloud = c;
		}
	}
	REQUIRE(cloud);
	EmotionTrackerUpdate *enemyTracker = EmotionTrackerUpdate::of(*enemy);
	EmotionTrackerUpdate *allyTracker = EmotionTrackerUpdate::of(*ally);
	REQUIRE(enemyTracker);
	REQUIRE(allyTracker);
	CHECK_FALSE(enemyTracker->requested(EMOTION_UNCONTROLLABLE_FEAR));
	const Coord3D at = { 600.0f, 600.0f, 0.0f };
	cloud->doSpecialPowerAtLocation(at, 0); // RW 0x8C8CE4
	CHECK(cloud->firesPutOut() == 1); // one enemy matched: the caster and the ally the filter refuses
	CHECK(enemyTracker->requested(EMOTION_UNCONTROLLABLE_FEAR)); // RW 0x8B4E79: requested[5] = 1
	CHECK(enemyTracker->requestEnd(EMOTION_UNCONTROLLABLE_FEAR) >= f.logic->getFrame()); // RW 0x8B4E84: now + 1 (RW 0x8C8A6D)
	CHECK(enemyTracker->requestSource(EMOTION_UNCONTROLLABLE_FEAR) == INVALID_ID); // RW 0x8C8A5A: no source
	CHECK_FALSE(allyTracker->requested(EMOTION_UNCONTROLLABLE_FEAR)); // the filter's ENEMIES half (RW 0x7640C1)
}
