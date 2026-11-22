// OpenBFME. PORT-232 (issue #555): HordeDispatchSpecialPower::doSpecialPowerAtObject (RW 0x8CCF76) hands the horde's own base call the
// target the member's module kept (the special power interface's slot 0x50, RW 0x7B0F25: module + 0x2C, the target id the initiate keeps at
// RW 0x897E9E .. 0x897EA3) farthest from the horde (squared distance RW 0x5E3BE4), not the order's target. The order's target starts as the candidate
// (RW computes its distance before the member loop) and is adopted only on a strictly greater distance.
//
// Synthetic INI only (no game archives / ROTWK_INSTALL / OPENBFME_ENNORATH_INSTALL required).
// GPL-3.0.

#include "doctest.h"

#include "MoveTestUtil.h"

#include "Common/Player.h"
#include "Common/SpecialPower.h"
#include "Common/Upgrade.h"
#include "GameLogic/Combat/ObjectWeapons.h"
#include "Common/Thing/ModuleFactory.h"
#include "GameLogic/Armor.h"
#include "GameLogic/WeaponStores.h"
#include "GameLogic/AttributeModifiers.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Module/HordeDispatchSpecialPower.h"
#include "GameLogic/Object/Contain/HordeContainRuntime.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/ObjectTypes.h"

#include <string>
#include <vector>
#include <iterator>
#include <list>

namespace
{
const char kPort232Ini[] =
	"ModifierList P232Buff\n"
	"  Category = SPELL\n"
	"  Modifier = ARMOR 25%\n"
	"  Duration = 5000\n"
	"End\n"
	"SpecialPower P232GoFar\n"
	"  Enum = SPECIAL_AT_VISIBLE_OBJECT\n"
	"  ReloadTime = 0\n"
	"End\n"
	"Object P232GoblinFighter\n"
	"  KindOf = PRELOAD SELECTABLE CAN_ATTACK INFANTRY\n"
	"  VisionRange = 350\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 6\n"
	"  GeometryHeight = 18\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 60\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"    InitialHealth = 100\n"
	"  End\n"
	"  Behavior = SpecialPowerModule ModuleTag_Power\n"
	"    SpecialPowerTemplate = P232GoFar\n"
	"    UpdateModuleStartsAttack = No\n"
	"    AttributeModifier = P232Buff\n"
	"    AttributeModifierRange = 200\n"
	"    AttributeModifierAffects = ANY +MACHINE\n"
	"    AttributeModifierAffectsSelf = No\n"
	"  End\n"
	"End\n"
	"Object P232GoblinHorde\n"
	"  KindOf = PRELOAD SELECTABLE CAN_ATTACK INFANTRY HORDE\n"
	"  VisionRange = 350\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 12\n"
	"  GeometryHeight = 20\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 60\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 400\n"
	"    InitialHealth = 400\n"
	"  End\n"
	"  Behavior = HordeContain ModuleTag_Contain\n"
	"    RankInfo = RankNumber:1 UnitType:P232GoblinFighter Position:X:0 Y:0 Position:X:10 Y:10\n"
	"    InitialPayload = P232GoblinFighter 2\n"
	"  End\n"
	"  Behavior = HordeDispatchSpecialPower ModuleTag_Power\n"
	"    SpecialPowerTemplate = P232GoFar\n"
	"    UpdateModuleStartsAttack = No\n"
	"    AttributeModifier = P232Buff\n"
	"    AttributeModifierRange = 200\n"
	"    AttributeModifierAffects = ANY +MACHINE\n"
	"    AttributeModifierAffectsSelf = No\n"
	"  End\n"
	"End\n"
	"Object P232Target\n"
	"  KindOf = PRELOAD SELECTABLE MACHINE\n"
	"  VisionRange = 350\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 12\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 200\n"
	"    InitialHealth = 200\n"
	"  End\n"
	"End\n"
	"Object P232Victim\n"
	"  KindOf = PRELOAD MACHINE\n"
	"  VisionRange = 100\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryHeight = 12\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 200\n"
	"    InitialHealth = 200\n"
	"  End\n"
	"End\n";

// the world with the power store and the modifier list registered on top of the move fixture
struct Port232World : movetest::MoveWorld
{
	SpecialPowerStore powers;
	SpecialPowerStore *savedPowers = TheSpecialPowerStore;
	AttributeModifierStore modifiers;
	AttributeModifierStore *savedModifiers = TheAttributeModifierStore;

	Port232World()
		: MoveWorld(100, 100) // the plain move fixture (no extra objects); the power INI is loaded after its block parsers are registered
	{
		TheSpecialPowerStore = &powers;
		TheAttributeModifierStore = &modifiers;
		w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
		modifiers.registerBlock(w.fx.env.blocks);
		const std::string err = w.load(kPort232Ini, INI_LOAD_OVERWRITE, "port232.ini");
		REQUIRE_MESSAGE(err.empty(), err);
		buildMap();
	}
	~Port232World()
	{
		logic->reset();
		TheSpecialPowerStore = savedPowers;
		TheAttributeModifierStore = savedModifiers;
	}

	const SpecialPowerTemplate *power()
	{
		return TheSpecialPowerStore->findSpecialPowerTemplate("P232GoFar");
	}
	SpecialPowerModuleInterface *powerModule(Object &o)
	{
		return SpecialPowerModules::findModule(o, power());
	}
};
} // namespace

// RW 0x8CCF76: the horde's base call (after the members') takes the target the member's module kept (interface slot 0x50) farthest from the horde;
// the order's target starts as the candidate
TEST_CASE("PORT-232 the horde's base cast takes the member's kept target farthest from the horde (RW 0x8CCF76, slot 0x50 RW 0x7B0F25)")
{
	Port232World w;
	Team *alice = w.teamOf("Alice");
	Team *bob = w.teamOf("Bob");
	Object *horde = w.spawn("P232GoblinHorde", 100.0f, 0.0f, 0.0f, alice);
	REQUIRE(horde);
	const auto *members = horde->getContain() ? horde->getContain()->getContainedItemsList() : nullptr;
	REQUIRE(members);
	REQUIRE(members->size() == 2);
	const auto memberIt = members->begin();
	Object *memberFar = *memberIt;          // keeps the far target (a gated member whose cast kept an earlier, farther one)
	Object *memberNear = *std::next(memberIt);
	// the near, enemy order target (0,0); the far enemy target (600,0); the allied victim only in range of the far target
	Object *order = w.spawn("P232Target", 0.0f, 0.0f, 0.0f, bob);
	Object *far = w.spawn("P232Target", 600.0f, 0.0f, 0.0f, bob);
	Object *victim = w.spawn("P232Victim", 600.0f, 100.0f, 0.0f, alice);
	REQUIRE(order);
	REQUIRE(far);
	REQUIRE(victim);

	// the member casts at the far target and keeps it; then it is paused (its own gate refuses the order's target cast, so the kept id stays the far one)
	SpecialPowerModuleInterface *farModule = w.powerModule(*memberFar);
	REQUIRE(farModule);
	farModule->doSpecialPowerAtObject(far, 0);
	CHECK(farModule->getTargetId() == far->getID());
	farModule->pauseCountdown(true);

	// the horde is ordered at the near target
	SpecialPowerModuleInterface *hordeModule = w.powerModule(*horde);
	REQUIRE(hordeModule);
	const unsigned long long appliedBefore = static_cast<SpecialPowerModule *>(hordeModule)->applied();
	hordeModule->doSpecialPowerAtObject(order, 0);

	// the kept target (the far one, 500 away) beats the order's target (100 away): the horde's base trigger scans around the far target,
	// and the victim (only in its range) is the one victim
	CHECK(hordeModule->getTargetId() == far->getID());
	CHECK(static_cast<SpecialPowerModule *>(hordeModule)->applied() == appliedBefore + 1);
	CHECK(static_cast<SpecialPowerModule *>(hordeModule)->victimsAffected() == 1);
	// the paused member kept the far target (its cast was refused); the other member kept the order's target
	CHECK(farModule->getTargetId() == far->getID());
	CHECK(w.powerModule(*memberNear)->getTargetId() == order->getID());
}

TEST_CASE("PORT-232 with no member keeping a different target the horde's base cast takes the order's target")
{
	Port232World w;
	Team *alice = w.teamOf("Alice");
	Team *bob = w.teamOf("Bob");
	Object *horde = w.spawn("P232GoblinHorde", 100.0f, 0.0f, 0.0f, alice);
	REQUIRE(horde);
	Object *order = w.spawn("P232Target", 0.0f, 0.0f, 0.0f, bob);
	Object *far = w.spawn("P232Target", 600.0f, 0.0f, 0.0f, bob);
	REQUIRE(order);
	REQUIRE(far);
	// nobody casts at the far target first: the far target is not a member's kept target, the order's target wins the start distance
	SpecialPowerModuleInterface *hordeModule = w.powerModule(*horde);
	REQUIRE(hordeModule);
	const unsigned long long appliedBefore = static_cast<SpecialPowerModule *>(hordeModule)->applied();
	hordeModule->doSpecialPowerAtObject(order, 0);
	CHECK(hordeModule->getTargetId() == order->getID());
	CHECK(static_cast<SpecialPowerModule *>(hordeModule)->victimsAffected() == 0); // the enemy target itself does not match AffectGood's relationship mask
	CHECK(static_cast<SpecialPowerModule *>(hordeModule)->applied() == appliedBefore);
}


namespace
{
// S-1672 (PORT-232): the weapon set flags' write path is Object::setWeaponSetFlag / Object::setWeaponSetFlags (RW 0x691059 / 0x691106 / 0x68DECA / 0x6911B7):
// the OBJECT's word at RW + 0x38C exists on every object (with or without a weapon set); the mapped model condition the table RW 0xC16958 names follows.
const char kWeaponModeIni[] =
	"Armor PlainArmor\n"
	"  Armor = DEFAULT 100%\n"
	"End\n"
	"Weapon P232Sword\n"
	"  AttackRange = 11.5\n"
	"  MeleeWeapon = Yes\n"
	"  DelayBetweenShots = 1000\n"
	"  DamageNugget\n"
	"    Damage = 10\n"
	"    DamageType = SLASH\n"
	"    DeathType = NORMAL\n"
	"  End\n"
	"End\n"
	"Upgrade Upgrade_P232Toggle\n"
	"  Type = OBJECT\n"
	"End\n"
	"Upgrade Upgrade_P232Close\n"
	"  Type = OBJECT\n"
	"End\n"
	"Object P232Weaponless\n"
	"  KindOf = PRELOAD SELECTABLE INFANTRY\n"
	"  VisionRange = 350\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 6\n"
	"  GeometryHeight = 18\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 60\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = WeaponSetUpgrade ModuleTag_Toggle\n"
	"    TriggeredBy = Upgrade_P232Toggle\n"
	"    WeaponCondition = WEAPONSET_TOGGLE_1\n"
	"  End\n"
	"  Behavior = WeaponSetUpgrade ModuleTag_Close\n"
	"    TriggeredBy = Upgrade_P232Close\n"
	"    WeaponCondition = CLOSE_RANGE\n"
	"  End\n"
	"End\n"
	"Object P232WithWeapons\n"
	"  KindOf = PRELOAD SELECTABLE INFANTRY CAN_ATTACK\n"
	"  VisionRange = 350\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 6\n"
	"  GeometryHeight = 18\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 60\n"
	"  End\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY P232Sword\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = WeaponSetUpgrade ModuleTag_Toggle\n"
	"    TriggeredBy = Upgrade_P232Toggle\n"
	"    WeaponCondition = WEAPONSET_TOGGLE_1\n"
	"  End\n"
	"  Behavior = WeaponSetUpgrade ModuleTag_Close\n"
	"    TriggeredBy = Upgrade_P232Close\n"
	"    WeaponCondition = CLOSE_RANGE\n"
	"  End\n"
	"End\n";

// the weapon / armour blocks the combat fixture registers (CombatTestUtil.h); the move world does not
void registerWeaponArmorBlocks(movetest::MoveWorld &w)
{
	w.w.fx.env.blocks.registerBlock("Weapon", [](INI *ini) { WeaponStore::parseWeaponTemplateDefinitionGlobal(ini); });
	w.w.fx.env.blocks.registerBlock("Armor", [](INI *ini) { ArmorStore::parseArmorDefinitionGlobal(ini); });
}

constexpr unsigned kFlagToggle1 = 24;  // TheWeaponConditionNames[24] WEAPONSET_TOGGLE_1 (RW 0xDA1328)
constexpr unsigned kFlagCloseRange = 7; // TheWeaponConditionNames[7] CLOSE_RANGE
constexpr int kMcToggle1 = 0x12D;       // the model condition the table RW 0xC16958 maps 24 to (WEAPONSET_TOGGLE_1)
constexpr int kMcEngaged = 159;         // the model condition the table RW 0xC16958 maps 7 to (ENGAGED)

const UpgradeTemplate *p232Up(const char *name)
{
	const UpgradeTemplate *u = TheUpgradeCenter->findUpgrade(name);
	REQUIRE_MESSAGE(u, name);
	return u;
}
} // namespace

// RW 0x68DECA / 0x6911B7 write the OBJECT's weapon set word, not only its ObjectWeapons (which exists only for a template that can have a weapon, RW 0x73C191)
TEST_CASE("PORT-232 a WeaponSetUpgrade sets its mask on an object without a weapon set (RW 0x68DECA / 0x6911B7)")
{
	movetest::MoveWorld w(100, 100);
	WeaponStores stores;
	stores.install();
	registerWeaponArmorBlocks(w);
	const std::string p232Err = w.w.load(kWeaponModeIni, INI_LOAD_OVERWRITE, "port232_weapon_set.ini");
	REQUIRE_MESSAGE(p232Err.empty(), p232Err);
	Object *o = w.make("P232Weaponless", w.teamOf("Alice"));
	REQUIRE(o);
	REQUIRE(o->getWeapons() == nullptr); // no WeaponSet block with a real weapon
	CHECK((o->getWeaponSetFlags()[kFlagToggle1 / 32] & (1u << (kFlagToggle1 & 31))) == 0);
	CHECK_FALSE(o->testModelCondition(kMcToggle1));
	o->giveUpgrade(p232Up("Upgrade_P232Toggle"));
	CHECK((o->getWeaponSetFlags()[kFlagToggle1 / 32] & (1u << (kFlagToggle1 & 31))) == (1u << (kFlagToggle1 & 31))); // the object's word, RW 0x68DECA -> 0x68CC53 over Object + 0x38C
	CHECK_FALSE(o->testModelCondition(kMcToggle1)); // RW 0x68DECA sets no model condition; only the single-flag path (RW 0x691059) does
	o->setWeaponSetFlag((int)kFlagToggle1, true); // RW 0x691059: the flag, the set update, then the mapped model condition (RW 0xC16958 maps bit 24 to 301)
	CHECK(o->testModelCondition(kMcToggle1));
	o->giveUpgrade(p232Up("Upgrade_P232Close"));
	CHECK((o->getWeaponSetFlags()[kFlagCloseRange / 32] & (1u << (kFlagCloseRange & 31))) == (1u << (kFlagCloseRange & 31)));
	CHECK_FALSE(o->testModelCondition(kMcEngaged));
	o->setWeaponSetFlag((int)kFlagCloseRange, true); // RW 0xC16958 maps bit 7 to 159
	CHECK(o->testModelCondition(kMcEngaged));
	o->removeUpgrade(p232Up("Upgrade_P232Toggle"));
	CHECK((o->getWeaponSetFlags()[kFlagToggle1 / 32] & (1u << (kFlagToggle1 & 31))) == 0); // RW 0x6911B7
	CHECK_FALSE(o->testModelCondition(kMcToggle1)); // RW 0x6911B7's second loop clears the mapped condition when the object has it
	o->removeUpgrade(p232Up("Upgrade_P232Close"));
	CHECK((o->getWeaponSetFlags()[kFlagCloseRange / 32] & (1u << (kFlagCloseRange & 31))) == 0);
	CHECK_FALSE(o->testModelCondition(kMcEngaged));
}

// the same mask through the ObjectWeapons of a template that can have a weapon: the set adds, the clear mirrors the named model conditions (RW 0x6911B7's second loop)
TEST_CASE("PORT-232 a WeaponSetUpgrade on an object with a weapon set mirrors the mapped model conditions on remove (RW 0x68DECA / 0x6911B7)")
{
	movetest::MoveWorld w(100, 100);
	WeaponStores stores;
	stores.install();
	registerWeaponArmorBlocks(w);
	const std::string p232Err = w.w.load(kWeaponModeIni, INI_LOAD_OVERWRITE, "port232_weapon_set.ini");
	REQUIRE_MESSAGE(p232Err.empty(), p232Err);
	Object *o = w.make("P232WithWeapons", w.teamOf("Alice"));
	REQUIRE(o);
	REQUIRE(o->getWeapons() != nullptr);
	o->giveUpgrade(p232Up("Upgrade_P232Toggle"));
	CHECK((o->getWeapons()->weaponSetFlags()[kFlagToggle1 / 32] & (1u << (kFlagToggle1 & 31))) == (1u << (kFlagToggle1 & 31)));
	CHECK_FALSE(o->testModelCondition(kMcToggle1)); // the mask set (RW 0x68DECA) touches no model condition
	o->setWeaponSetFlag((int)kFlagToggle1, true); // RW 0x691059: the mapped condition follows the single-flag path
	CHECK(o->testModelCondition(kMcToggle1));
	o->giveUpgrade(p232Up("Upgrade_P232Close"));
	CHECK((o->getWeapons()->weaponSetFlags()[kFlagCloseRange / 32] & (1u << (kFlagCloseRange & 31))) == (1u << (kFlagCloseRange & 31)));
	CHECK_FALSE(o->testModelCondition(kMcEngaged));
	o->setWeaponSetFlag((int)kFlagCloseRange, true);
	CHECK(o->testModelCondition(kMcEngaged));
	o->removeUpgrade(p232Up("Upgrade_P232Toggle"));
	CHECK((o->getWeapons()->weaponSetFlags()[kFlagToggle1 / 32] & (1u << (kFlagToggle1 & 31))) == 0);
	CHECK_FALSE(o->testModelCondition(kMcToggle1)); // RW 0x6911B7's second loop: the mask's bits whose conditions the owner has, cleared one by one
	o->removeUpgrade(p232Up("Upgrade_P232Close"));
	CHECK((o->getWeapons()->weaponSetFlags()[kFlagCloseRange / 32] & (1u << (kFlagCloseRange & 31))) == 0);
	CHECK_FALSE(o->testModelCondition(kMcEngaged));
}
