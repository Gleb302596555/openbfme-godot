// OpenBFME. LOCAL-148 (issue #881): availability gates behind the 49+ matrix "DISABLED: needs none" buttons.
// Expectations from RotWK 2.01 ControlBar::getCommandAvailability (RW 0x942733):
//   - FIRE_WEAPON (case 0x17, RW 0x9431F9 .. 0x943247) & ObjectWeapons reloadAmmo vs loadAmmoNow (RW 0x6CEE89)
//   - SPECIAL_POWER AvailableAtStart = No / StartsPaused (RW 0x943585 .. 0x9435D2)
//   - StopSpecialPower::isReady (RW 0x8CACC6 / RW 0x943585)
//   - OBJECT_UPGRADE (case 7, RW 0x943050 .. 0x9430F9): Purchased (RW 0x9430C8 -> 0x9431A6),
//     missing ProductionUpdate (RW 0x9430B6 -> 0x9438C8), !Object::affectedByUpgrade (RW 0x9430F2 -> RW 0x694914)
//   - greyfix::effective / needsDetail on MATRIX_GREY.tsv rows (RW 0x942733)
// Synthetic INI only: no game archive, no ROTWK_INSTALL / OPENBFME_ENNORATH_INSTALL.
// GPL-3.0.
#include "doctest.h"

#include "CombatTestUtil.h"
#include "GreyFixture.h"
#include "HudTestUtil.h"

#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/SpecialPower.h"
#include "Common/Upgrade.h"
#include "GameClient/ControlBar.h"
#include "GameClient/ControlBarCommands.h"
#include "GameClient/HudInput.h"
#include "GameLogic/Module/SiegeModules.h"
#include "GameLogic/Module/SpecialPowerModules.h"

#include <memory>
#include <string>
#include <vector>

namespace
{
const char kLocal148Ini[] =
	"Upgrade Upgrade_L148_Test\n"
	"  Type = OBJECT\n"
	"  BuildTime = 1.0\n"
	"  BuildCost = 100\n"
	"End\n"
	"SpecialPower SpecialAbilityL148Delayed\n"
	"  Enum = SPECIAL_TOGGLE_MOUNTED\n"
	"  ReloadTime = 1000\n"
	"End\n"
	"SpecialPower SpecialAbilityL148Paused\n"
	"  Enum = SPECIAL_TOGGLE_MOUNTED\n"
	"  ReloadTime = 1000\n"
	"End\n"
	"SpecialPower SpecialAbilityL148Stop\n"
	"  Enum = SPECIAL_GENERAL_TARGETLESS\n"
	"  ReloadTime = 0\n"
	"End\n"
	"SpecialPower SpecialAbilityL148Deploy\n"
	"  Enum = SPECIAL_GENERAL_TARGETLESS\n"
	"  ReloadTime = 0\n"
	"End\n"
	"Weapon L148BallistaWeapon\n"
	"  AttackRange = 300.0\n"
	"  PreAttackDelay = 100\n"
	"  FiringDuration = 100\n"
	"  DelayBetweenShots = 100\n"
	"  ClipSize = 1\n"
	"  AutoReloadsClip = Yes\n"
	"  ClipReloadTime = 1000\n"
	"  DamageNugget\n"
	"    Damage = 10\n"
	"    DamageType = SIEGE\n"
	"    DeathType = NORMAL\n"
	"  End\n"
	"End\n"
	"Weapon L148InstantWeapon\n"
	"  AttackRange = 300.0\n"
	"  PreAttackDelay = 100\n"
	"  FiringDuration = 100\n"
	"  DelayBetweenShots = 100\n"
	"  ClipSize = 1\n"
	"  AutoReloadsClip = Yes\n"
	"  ClipReloadTime = 1000\n"
	"  InstantLoadClipOnActivate = Yes\n"
	"  DamageNugget\n"
	"    Damage = 10\n"
	"    DamageType = SIEGE\n"
	"    DeathType = NORMAL\n"
	"  End\n"
	"End\n"
	"CommandButton Command_L148_Bombard\n"
	"  Command = FIRE_WEAPON\n"
	"  WeaponSlot = PRIMARY\n"
	"  Options = NEED_TARGET_POS\n"
	"  ButtonImage = ImgBombard\n"
	"  InPalantir = Yes\n"
	"End\n"
	"CommandButton Command_L148_DelayedPower\n"
	"  Command = SPECIAL_POWER\n"
	"  SpecialPower = SpecialAbilityL148Delayed\n"
	"  ButtonImage = ImgPower\n"
	"  InPalantir = Yes\n"
	"End\n"
	"CommandButton Command_L148_PausedPower\n"
	"  Command = SPECIAL_POWER\n"
	"  SpecialPower = SpecialAbilityL148Paused\n"
	"  ButtonImage = ImgPower\n"
	"  InPalantir = Yes\n"
	"End\n"
	"CommandButton Command_L148_StopRetract\n"
	"  Command = SPECIAL_POWER\n"
	"  SpecialPower = SpecialAbilityL148Stop\n"
	"  ButtonImage = ImgStop\n"
	"  InPalantir = Yes\n"
	"End\n"
	"CommandButton Command_L148_BuyUpgrade\n"
	"  Command = OBJECT_UPGRADE\n"
	"  Upgrade = Upgrade_L148_Test\n"
	"  Options = CANCELABLE\n"
	"  ButtonImage = ImgUpgrade\n"
	"  InPalantir = Yes\n"
	"End\n"
	"CommandSet L148BallistaSet\n"
	"  1 = Command_L148_Bombard\n"
	"End\n"
	"CommandSet L148PowerSet\n"
	"  1 = Command_L148_DelayedPower\n"
	"  2 = Command_L148_PausedPower\n"
	"End\n"
	"CommandSet L148StopSet\n"
	"  1 = Command_L148_StopRetract\n"
	"End\n"
	"CommandSet L148UpgradeSet\n"
	"  1 = Command_L148_BuyUpgrade\n"
	"End\n"
	"Object L148Ballista\n"
	"  KindOf = SELECTABLE MACHINE SIEGEENGINE CAN_ATTACK\n"
	"  CommandSet = L148BallistaSet\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY L148BallistaWeapon\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"End\n"
	"Object L148InstantBallista\n"
	"  KindOf = SELECTABLE MACHINE SIEGEENGINE CAN_ATTACK\n"
	"  CommandSet = L148BallistaSet\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY L148InstantWeapon\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"End\n"
	"Object L148HeroPowers\n"
	"  KindOf = SELECTABLE HERO INFANTRY\n"
	"  CommandSet = L148PowerSet\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = SpecialPowerModule ModuleTag_Delayed\n"
	"    SpecialPowerTemplate = SpecialAbilityL148Delayed\n"
	"    StartsPaused = No\n"
	"    AvailableAtStart = No\n"
	"  End\n"
	"  Behavior = SpecialPowerModule ModuleTag_Paused\n"
	"    SpecialPowerTemplate = SpecialAbilityL148Paused\n"
	"    StartsPaused = Yes\n"
	"    AvailableAtStart = No\n"
	"  End\n"
	"End\n"
	"Object L148SiegeTower\n"
	"  KindOf = SELECTABLE MACHINE SIEGEENGINE\n"
	"  CommandSet = L148StopSet\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = StopSpecialPower ModuleTag_Stop\n"
	"    SpecialPowerTemplate = SpecialAbilityL148Stop\n"
	"    StopPowerTemplate = SpecialAbilityL148Deploy\n"
	"  End\n"
	"End\n"
	"Object L148UnitNoPU\n"
	"  KindOf = SELECTABLE INFANTRY\n"
	"  CommandSet = L148UpgradeSet\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 200\n"
	"  End\n"
	"  Behavior = SubObjectsUpgrade ModuleTag_Show\n"
	"    TriggeredBy = Upgrade_L148_Test\n"
	"    ShowSubObjects = BLADE\n"
	"  End\n"
	"End\n"
	"Object L148UnitUnaffected\n"
	"  KindOf = SELECTABLE INFANTRY\n"
	"  CommandSet = L148UpgradeSet\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 200\n"
	"  End\n"
	"  Behavior = ProductionUpdate ModuleTag_PU\n"
	"  End\n"
	"End\n"
	"Object L148UnitPreUpgraded\n"
	"  KindOf = SELECTABLE INFANTRY\n"
	"  CommandSet = L148UpgradeSet\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 200\n"
	"  End\n"
	"  Behavior = ProductionUpdate ModuleTag_PU\n"
	"  End\n"
	"  Behavior = SubObjectsUpgrade ModuleTag_Show\n"
	"    TriggeredBy = Upgrade_L148_Test\n"
	"    ShowSubObjects = BLADE\n"
	"  End\n"
	"  Behavior = GrantUpgradeCreate ModuleTag_Grant\n"
	"    UpgradeToGrant = Upgrade_L148_Test\n"
	"  End\n"
	"End\n"
	"Object L148UnitUpgradable\n"
	"  KindOf = SELECTABLE INFANTRY\n"
	"  CommandSet = L148UpgradeSet\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 200\n"
	"  End\n"
	"  Behavior = ProductionUpdate ModuleTag_PU\n"
	"  End\n"
	"  Behavior = SubObjectsUpgrade ModuleTag_Show\n"
	"    TriggeredBy = Upgrade_L148_Test\n"
	"    ShowSubObjects = BLADE\n"
	"  End\n"
	"End\n";

struct Local148Rig : combattest::CombatWorld
{
	CommandStore commandStore;
	CommandStore *savedCommands = TheCommandStore;
	SpecialPowerStore specialPowers;
	SpecialPowerStore *savedPowers = TheSpecialPowerStore;
	MouseSettings mouse;
	MetaMap metaMap;
	PinholeView view{ { 200.0f, -300.0f, 420.0f }, { 200.0f, 200.0f, 0.0f }, 1024, 768, 0.75f };
	std::unique_ptr<HudInput> input;
	std::unique_ptr<ControlBar> bar;

	Local148Rig()
		: combattest::CombatWorld("", 40)
	{
		TheCommandStore = &commandStore;
		TheSpecialPowerStore = &specialPowers;
		commandStore.setThingFactory(&w.things);
		w.fx.env.blocks.registerBlock("CommandButton", [](INI *ini) { CommandStore::parseCommandButtonDefinitionGlobal(ini); });
		w.fx.env.blocks.registerBlock("CommandSet", [](INI *ini) { CommandStore::parseCommandSetDefinitionGlobal(ini); });
		w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
		const std::string err = w.load(kLocal148Ini, INI_LOAD_OVERWRITE, "local148.ini");
		REQUIRE_MESSAGE(err.empty(), err);

		Player *alice = players.findPlayerWithName("Alice");
		REQUIRE(alice != nullptr);
		alice->getMoney()->deposit(5000);
		players.setLocalPlayer(alice);

		input = std::make_unique<HudInput>(*logic, ai.get(), view, commands, mouse, metaMap);
		input->context().commands = &commandStore;
		bar = std::make_unique<ControlBar>(input->context());
	}

	~Local148Rig()
	{
		bar.reset();
		input.reset();
		logic->reset();
		TheCommandStore = savedCommands;
		TheSpecialPowerStore = savedPowers;
	}

	void frames(int n)
	{
		for (int i = 0; i < n; ++i)
		{
			(void)input->update();
			logic->runLogicFrame();
		}
		bar->update();
	}

	ButtonState eval(const char *buttonName, Object &obj)
	{
		input->ui().setGUICommand(nullptr);
		std::vector<ObjectID> ids{ obj.getID() };
		input->ui().mirrorSelection(ids);
		bar->update();
		for (const auto *list : { &bar->palantirButtons(), &bar->sideButtons(), &bar->offBarButtons() })
		{
			for (const ControlBarButton &cb : *list)
			{
				if (cb.button && cb.button->m_name == buttonName)
				{
					return cb.state;
				}
			}
		}
		return ButtonState::Hidden;
	}
};
} // namespace

// RW 0x9431F9 .. 0x943247 (FIRE_WEAPON case 0x17) & RW 0x6CEE89 (reloadAmmo vs loadAmmoNow)
TEST_CASE("LOCAL-148 FIRE_WEAPON clip reload on spawn starts NotReady and becomes Enabled once reloaded (RW 0x9431F9, RW 0x6CEE89)")
{
	Local148Rig r;
	Object *ballista = r.unit("L148Ballista", 'A', 180.0f, 200.0f);
	Object *instant = r.unit("L148InstantBallista", 'A', 220.0f, 200.0f);
	REQUIRE(ballista != nullptr);
	REQUIRE(instant != nullptr);

	r.frames(2);
	CHECK(r.eval("Command_L148_Bombard", *ballista) == ButtonState::NotReady);
	CHECK(r.eval("Command_L148_Bombard", *instant) == ButtonState::Enabled);

	// ClipReloadTime = 1000 ms (5 logic frames at 5 Hz or 30 frames at 30 Hz — step 40 frames to cover clip reload)
	r.frames(40);
	CHECK(r.eval("Command_L148_Bombard", *ballista) == ButtonState::Enabled);
}

// RW 0x943585 .. 0x9435D2 (SPECIAL_POWER AvailableAtStart = No vs StartsPaused = Yes)
TEST_CASE("LOCAL-148 SPECIAL_POWER AvailableAtStart = No starts NotReady and becomes Enabled after ReloadTime, while StartsPaused = Yes stays Restricted (RW 0x943585)")
{
	Local148Rig r;
	Object *hero = r.unit("L148HeroPowers", 'A', 180.0f, 200.0f);
	REQUIRE(hero != nullptr);

	r.frames(2);
	CHECK(r.eval("Command_L148_DelayedPower", *hero) == ButtonState::NotReady);
	CHECK(r.eval("Command_L148_PausedPower", *hero) == ButtonState::Restricted);

	r.frames(40);
	CHECK(r.eval("Command_L148_DelayedPower", *hero) == ButtonState::Enabled);
	CHECK(r.eval("Command_L148_PausedPower", *hero) == ButtonState::Restricted);
}

// RW 0x8CACC6 (StopSpecialPower::isReady) & RW 0x943585 (SPECIAL_POWER NotReady)
TEST_CASE("LOCAL-148 StopSpecialPower stays NotReady while its StopPowerTemplate is inactive (RW 0x8CACC6 / RW 0x943585)")
{
	Local148Rig r;
	Object *tower = r.unit("L148SiegeTower", 'A', 180.0f, 200.0f);
	REQUIRE(tower != nullptr);

	r.frames(2);
	CHECK(r.eval("Command_L148_StopRetract", *tower) == ButtonState::NotReady);
	const SpecialPowerTemplate *t = TheSpecialPowerStore->findSpecialPowerTemplate("SpecialAbilityL148Stop");
	REQUIRE(t != nullptr);
	SpecialPowerModuleInterface *sp = SpecialPowerModules::findModule(*tower, t);
	REQUIRE(dynamic_cast<const StopSpecialPower *>(sp) != nullptr);

	// No timer advances StopSpecialPower: still NotReady after stepping
	r.frames(60);
	CHECK(r.eval("Command_L148_StopRetract", *tower) == ButtonState::NotReady);
}

// RW 0x943050 .. 0x9430F9 (OBJECT_UPGRADE case 7) & RW 0x694914 (Object::affectedByUpgrade)
TEST_CASE("LOCAL-148 OBJECT_UPGRADE gates: Purchased, missing ProductionUpdate, and unaffected object (RW 0x943050, RW 0x694914)")
{
	Local148Rig r;
	Object *noPu = r.unit("L148UnitNoPU", 'A', 160.0f, 200.0f);
	Object *unaff = r.unit("L148UnitUnaffected", 'A', 180.0f, 200.0f);
	Object *preUp = r.unit("L148UnitPreUpgraded", 'A', 200.0f, 200.0f);
	Object *ok = r.unit("L148UnitUpgradable", 'A', 220.0f, 200.0f);
	REQUIRE(noPu != nullptr);
	REQUIRE(unaff != nullptr);
	REQUIRE(preUp != nullptr);
	REQUIRE(ok != nullptr);

	r.frames(2);
	// Missing ProductionUpdate -> Restricted (RW 0x9430B6 -> 0x9438C8)
	CHECK(noPu->getProductionUpdate() == nullptr);
	CHECK(r.eval("Command_L148_BuyUpgrade", *noPu) == ButtonState::Restricted);
	// ProductionUpdate present, no triggered UpgradeMux -> Restricted (RW 0x9430F2 -> RW 0x694914)
	CHECK(unaff->getProductionUpdate() != nullptr);
	CHECK(!unaff->affectedByUpgrade(TheUpgradeCenter->findUpgrade("Upgrade_L148_Test")));
	CHECK(r.eval("Command_L148_BuyUpgrade", *unaff) == ButtonState::Restricted);
	// Already owned upgrade (e.g. GrantUpgradeCreate on ElvenRivendellLancerHordeMod_up) -> Purchased (RW 0x9430C8 -> 0x9431A6)
	preUp->giveUpgrade("Upgrade_L148_Test");
	CHECK(r.eval("Command_L148_BuyUpgrade", *preUp) == ButtonState::Purchased);
	// Neighbor: ProductionUpdate + triggered SubObjectsUpgrade -> Enabled
	CHECK(r.eval("Command_L148_BuyUpgrade", *ok) == ButtonState::Enabled);
}

// RW 0x942733: MATRIX_GREY.tsv rows listing only BuildCost / ON_GROUND_ONLY / DisableOnModelCondition yield "DISABLED: needs none..."
TEST_CASE("LOCAL-148 greyfix::needsDetail returns needs-none for rows with only non-gating conditions, so runtime gates override it (RW 0x942733)")
{
	const std::string tsv =
		"faction\tobject\tbutton\tcondition\tvalue\tsatisfied-by\tnote\n"
		"FactionElves\tLothlorienGrenzwchter\tCommand_Lembas\tother\t150\t-\tupgrade.BuildCost (Upgrade_Lembas)\n"
		"FactionMordor\tMordorWitchKingSpell\tCommand_Mount\toption\tON_GROUND_ONLY\t-\tCommandButton.Options\n"
		"FactionMordor\tMordorWitchKingSpell\tCommand_Mount\tmodel\tRIDER5\t-\tCommandButton.DisableOnModelCondition\n"
		"FactionElves\tLothlorienGrenzwchter\tCommand_Blades\tupgrade\tUpgrade_TechBlades\t-\tCommandButton.NeededUpgrade\n";
	const greyfix::Table table = greyfix::parseGreyTable(tsv);
	const greyfix::Row *lembas = greyfix::findRow(table, "LothlorienGrenzwchter", "Command_Lembas");
	const greyfix::Row *mount = greyfix::findRow(table, "MordorWitchKingSpell", "Command_Mount");
	const greyfix::Row *blades = greyfix::findRow(table, "LothlorienGrenzwchter", "Command_Blades");
	REQUIRE(lembas != nullptr);
	REQUIRE(mount != nullptr);
	REQUIRE(blades != nullptr);

	CHECK(!greyfix::fixable(*lembas));
	CHECK(greyfix::needsDetail(*lembas).rfind("DISABLED: needs none", 0) == 0);
	CHECK(!greyfix::fixable(*mount));
	CHECK(greyfix::needsDetail(*mount).rfind("DISABLED: needs none", 0) == 0);
	CHECK(greyfix::fixable(*blades));
}
