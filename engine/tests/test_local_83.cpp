// OpenBFME. GPL-3.0.
// LOCAL-83 (S-1227): AutoAbilityBehavior FIRE_WEAPON readiness (RW 0x0075CC3E: Upgrade +0x24 and Object +0x478 fireWeaponDeadline)
// and dispatch through Object::doCommandButton* (RW 0x0085D9D1 -> RW 0x00696FD2 / 0x00697890 / 0x006979D9), plus StartsActive slot 32 (RW 0x0085D8F5).

#include "doctest.h"
#include "CombatTestUtil.h"

#include "Common/SpecialPower.h"
#include "Common/Upgrade.h"
#include "GameClient/ControlBarCommands.h"
#include "GameClient/FXList.h"
#include "GameLogic/AttributeModifiers.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/HeroAbilityModules.h"
#include "GameLogic/Module/SpecialAbilityModules.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/Object/ObjectDoCommandButton.h"
#include "GameLogic/Object/PartitionManager.h"

namespace
{

const char kLocal83Ini[] =
	"Upgrade Upgrade_Local83FireWeapon\n"
	"  Type = OBJECT\n"
	"End\n"
	"ModifierList Local83Buff\n"
	"  Category = LEADERSHIP\n"
	"  Duration = 5000\n"
	"  Modifier = ARMOR 150%\n"
	"End\n"
	"Weapon Local83PrimaryWeapon\n"
	"  AttackRange = 200.0\n"
	"  DelayBetweenShots = 1000\n"
	"  DamageNugget\n"
	"    Damage = 10.0\n"
	"    DamageType = SLASH\n"
	"  End\n"
	"End\n"
	"Weapon Local83SecondaryWeapon\n"
	"  AttackRange = 250.0\n"
	"  DelayBetweenShots = 1000\n"
	"  DamageNugget\n"
	"    Damage = 50.0\n"
	"    DamageType = HERO\n"
	"  End\n"
	"End\n"
	"SpecialPower SpecialAbilityLocal83Slot32\n"
	"  Enum = SPECIAL_GENERAL_TARGETLESS\n"
	"  ReloadTime = 1000\n"
	"End\n"
	"CommandButton Command_Local83FireWeaponUpgraded\n"
	"  Command = FIRE_WEAPON\n"
	"  WeaponSlot = SECONDARY\n"
	"  Upgrade = Upgrade_Local83FireWeapon\n"
	"  Options = NEED_TARGET_ENEMY_OBJECT\n"
	"  AutoAbility = Yes\n"
	"  AutoDelay = 0.2\n"
	"End\n"
	"CommandButton Command_Local83FireWeaponPos\n"
	"  Command = FIRE_WEAPON\n"
	"  WeaponSlot = SECONDARY\n"
	"  Options = NEED_TARGET_POS\n"
	"  AutoAbility = Yes\n"
	"  AutoDelay = 0.2\n"
	"End\n"
	"CommandButton Command_Local83FireWeaponObj\n"
	"  Command = FIRE_WEAPON\n"
	"  WeaponSlot = SECONDARY\n"
	"  Options = NEED_TARGET_ENEMY_OBJECT\n"
	"  AutoAbility = Yes\n"
	"  AutoDelay = 0.2\n"
	"End\n"
	"CommandButton Command_Local83Slot32Power\n"
	"  Command = SPECIAL_POWER\n"
	"  SpecialPower = SpecialAbilityLocal83Slot32\n"
	"  Options = NEED_TARGET_ENEMY_OBJECT\n"
	"  AutoAbility = Yes\n"
	"End\n"
	"CommandSet Local83HeroCommandSet\n"
	"  1 = Command_Local83FireWeaponUpgraded\n"
	"  2 = Command_Local83FireWeaponPos\n"
	"  3 = Command_Local83FireWeaponObj\n"
	"End\n"
	"CommandSet Local83Slot32CommandSet\n"
	"  InitialVisible = 8\n"
	"  32 = Command_Local83Slot32Power\n"
	"End\n"
	"Object Local83Hero\n"
	"  KindOf = SELECTABLE CAN_ATTACK HERO INFANTRY\n"
	"  VisionRange = 300.0\n"
	"  CommandSet = Local83HeroCommandSet\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY Local83PrimaryWeapon\n"
	"    Weapon = SECONDARY Local83SecondaryWeapon\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500.0\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = AutoAbilityBehavior ModuleTag_AutoAbility\n"
	"    MaxScanRange = 250.0\n"
	"    Query = 1 ALL ENEMIES\n"
	"  End\n"
	"End\n"
	"Object Local83Slot32Caster\n"
	"  KindOf = SELECTABLE STRUCTURE\n"
	"  VisionRange = 300.0\n"
	"  CommandSet = Local83Slot32CommandSet\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500.0\n"
	"  End\n"
	"  Behavior = SpecialPowerModule ModuleTag_Slot32SP\n"
	"    SpecialPowerTemplate = SpecialAbilityLocal83Slot32\n"
	"    AttributeModifier = Local83Buff\n"
	"    AttributeModifierAffectsSelf = Yes\n"
	"  End\n"
	"  Behavior = AutoAbilityBehavior ModuleTag_Slot32Auto\n"
	"    SpecialAbility = SpecialAbilityLocal83Slot32\n"
	"    MaxScanRange = 250.0\n"
	"    StartsActive = Yes\n"
	"    Query = 1 ALL ENEMIES\n"
	"  End\n"
	"End\n"
	"Object Local83Enemy\n"
	"  KindOf = SELECTABLE INFANTRY\n"
	"  VisionRange = 200.0\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500.0\n"
	"  End\n"
	"End\n";

struct Local83World : combattest::CombatWorld
{
	SpecialPowerStore powers;
	CommandStore commandStore;
	FXListStore fxLists;
	AttributeModifierStore modifiers;

	SpecialPowerStore *savedPowers = TheSpecialPowerStore;
	CommandStore *savedCommands = TheCommandStore;
	FXListStore *savedFx = TheFXListStore;
	AttributeModifierStore *savedModifiers = TheAttributeModifierStore;

	Local83World()
		: combattest::CombatWorld("", 40)
	{
		TheSpecialPowerStore = &powers;
		TheCommandStore = &commandStore;
		TheFXListStore = &fxLists;
		TheAttributeModifierStore = &modifiers;
		commandStore.setThingFactory(&w.things);

		modifiers.registerBlock(w.fx.env.blocks);
		w.fx.env.blocks.registerBlock("FXList", [](INI *ini) { ParseFXListDefinitionGlobal(ini); });
		w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
		w.fx.env.blocks.registerBlock("CommandButton", [](INI *ini) { CommandStore::parseCommandButtonDefinitionGlobal(ini); });
		w.fx.env.blocks.registerBlock("CommandSet", [](INI *ini) { CommandStore::parseCommandSetDefinitionGlobal(ini); });

		SpecialAbilityModules::registerAll(w.modules);
		SpecialPowerModules::registerAll(w.modules);
		SpecialPowerModules::installScienceHooks(*logic);
		HeroAbilityModules::registerHandlers(dispatcher);

		const std::string err = w.load(kLocal83Ini, INI_LOAD_OVERWRITE, "local83.ini");
		REQUIRE_MESSAGE(err.empty(), err);

		logic->partition().setRegion(0.0f, 0.0f, 4000.0f, 4000.0f);
		logic->combat().setAutoAcquireEnabled(false);
	}

	~Local83World()
	{
		logic->reset();
		TheSpecialPowerStore = savedPowers;
		TheCommandStore = savedCommands;
		TheFXListStore = savedFx;
		TheAttributeModifierStore = savedModifiers;
	}
};

} // namespace

// RW 0x0075CC3E (0x0075CC4A .. 0x0075CC76) + RW 0x0085D9D1 (-> RW 0x00697890):
// a FIRE_WEAPON auto button requires Upgrade (+0x24, RW 0x00691421) when set and frame >= Object::m_fireWeaponDeadline (+0x478),
// and dispatches through Object::doCommandButtonAtObject (setting the 10s deadline and locking the weapon slot).
TEST_CASE("LOCAL-83: FIRE_WEAPON auto ability checks Upgrade (+0x24) and fireWeaponDeadline (+0x478) and fires through Object::doCommandButtonAtObject")
{
	Local83World f;
	Object *hero = f.unit("Local83Hero", 'A', 300.0f, 300.0f);
	Object *enemy = f.unit("Local83Enemy", 'B', 380.0f, 300.0f);
	REQUIRE(hero);
	REQUIRE(enemy);

	AutoAbilityBehavior *aa = AutoAbilityBehavior::first(*hero);
	REQUIRE(aa);
	const CommandButton *btn = TheCommandStore->findCommandButton("Command_Local83FireWeaponUpgraded");
	REQUIRE(btn);
	aa->setAutoButton(btn);
	REQUIRE(aa->autoButton() == "Command_Local83FireWeaponUpgraded");

	const unsigned long long fwBefore = ObjectDoCommandButton::stats().fireWeapons;
	const unsigned long long unportedBefore = AutoAbilityBehavior::stats().unportedCommands;

	// Without Upgrade_Local83FireWeapon, RW 0x0075CC57 -> RW 0x00691421 returns false.
	f.frames(12);
	CHECK(aa->castRecord().casts == 0);
	CHECK(ObjectDoCommandButton::stats().fireWeapons == fwBefore);
	CHECK(hero->getFireWeaponDeadline() == 0);
	CHECK(AutoAbilityBehavior::stats().unportedCommands == unportedBefore);

	// Grant the upgrade: the next scan fires via Object::doCommandButtonAtObject (RW 0x00697890).
	const UpgradeTemplate *up = TheUpgradeCenter->findUpgrade("Upgrade_Local83FireWeapon");
	REQUIRE(up);
	hero->giveUpgrade(up);

	for (int i = 0; i < 15 && aa->castRecord().casts == 0; ++i)
	{
		f.frames(1);
	}
	REQUIRE(aa->castRecord().casts == 1);
	CHECK(aa->castRecord().power == "Command_Local83FireWeaponUpgraded");
	CHECK(aa->castRecord().target == enemy->getID());
	CHECK(ObjectDoCommandButton::stats().fireWeapons == fwBefore + 1);
	CHECK(AutoAbilityBehavior::stats().unportedCommands == unportedBefore);
	const UnsignedInt deadline = hero->getFireWeaponDeadline();
	REQUIRE(deadline == aa->castRecord().frame + 10u * (UnsignedInt)LOGICFRAMES_PER_SECOND);

	// Before deadline (even well past AutoDelay = 0.2s = 1 frame), RW 0x0075CC6F blocks a second cast.
	while (f.logic->getFrame() + 1 < deadline)
	{
		f.frames(1);
	}
	CHECK(aa->castRecord().casts == 1);
	CHECK(ObjectDoCommandButton::stats().fireWeapons == fwBefore + 1);

	// At/after deadline, RW 0x0075CC6F allows the second cast.
	for (int i = 0; i < 15 && aa->castRecord().casts < 2; ++i)
	{
		f.frames(1);
	}
	CHECK(aa->castRecord().casts == 2);
	CHECK(ObjectDoCommandButton::stats().fireWeapons == fwBefore + 2);
}

// Neighboring behavior: FIRE_WEAPON without Upgrade, NEED_TARGET_POS (RW 0x006979D9, no disabled exception at RW 0x006979E5)
// vs NEED_TARGET_ENEMY_OBJECT (RW 0x00697890, allows DISABLED_HELD via RW 0x00855830).
TEST_CASE("LOCAL-83: FIRE_WEAPON auto ability without Upgrade respects NEED_TARGET_POS disabled block and NEED_TARGET_ENEMY_OBJECT DISABLED_HELD exception")
{
	Local83World f;
	Object *hero = f.unit("Local83Hero", 'A', 300.0f, 300.0f);
	Object *enemy = f.unit("Local83Enemy", 'B', 380.0f, 300.0f);
	REQUIRE(hero);
	REQUIRE(enemy);

	AutoAbilityBehavior *aa = AutoAbilityBehavior::first(*hero);
	REQUIRE(aa);

	// DISABLED_HELD (bit 3): NEED_TARGET_POS (RW 0x006979D9) refuses disabled objects even with fromAutoAbility=true.
	hero->setDisabled(DISABLED_HELD, 1000u);
	const CommandButton *posBtn = TheCommandStore->findCommandButton("Command_Local83FireWeaponPos");
	REQUIRE(posBtn);
	aa->setAutoButton(posBtn);
	const unsigned long long fwBefore = ObjectDoCommandButton::stats().fireWeapons;
	f.frames(10);
	CHECK(aa->castRecord().casts == 0);
	CHECK(ObjectDoCommandButton::stats().fireWeapons == fwBefore);

	// NEED_TARGET_ENEMY_OBJECT (RW 0x00697890): allows DISABLED_HELD because AutoAbilityBehavior::getDisabledTypesToProcess() returns DISABLED_HELD.
	const CommandButton *objBtn = TheCommandStore->findCommandButton("Command_Local83FireWeaponObj");
	REQUIRE(objBtn);
	aa->setAutoButton(objBtn);
	for (int i = 0; i < 15 && aa->castRecord().casts == 0; ++i)
	{
		f.frames(1);
	}
	CHECK(aa->castRecord().casts == 1);
	CHECK(ObjectDoCommandButton::stats().fireWeapons == fwBefore + 1);
}

// RW 0x0085D8A6 .. 0x0085D8F9 (loop bound 0x21 = 33 slots): StartsActive finds a button in hidden slot 32 (INI 32 -> index 31).
TEST_CASE("LOCAL-83: AutoAbilityBehavior StartsActive picks up a SPECIAL_POWER button in hidden slot 32")
{
	Local83World f;
	Object *caster = f.unit("Local83Slot32Caster", 'A', 300.0f, 300.0f);
	Object *enemy = f.unit("Local83Enemy", 'B', 380.0f, 300.0f);
	REQUIRE(caster);
	REQUIRE(enemy);

	AutoAbilityBehavior *aa = AutoAbilityBehavior::forPower(*caster, "SpecialAbilityLocal83Slot32");
	REQUIRE(aa);
	CHECK(aa->autoButton() == "Command_Local83Slot32Power");

	for (int i = 0; i < 15 && aa->castRecord().casts == 0; ++i)
	{
		f.frames(1);
	}
	CHECK(aa->castRecord().casts == 1);
	CHECK(aa->castRecord().power == "SpecialAbilityLocal83Slot32");
	CHECK(aa->castRecord().target == enemy->getID());
}
