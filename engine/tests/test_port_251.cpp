// OpenBFME unit tests. PORT-251 (issue #599, arena-task 227):
// Missing RotWK 2.01 methods of SpecialPowerModule (and audit of UnpauseSpecialPowerUpgrade):
//   1. SpecialPowerModule::updateRequirementsPause (slot 0x58, RW 0x896A32) driven per-frame from
//      Player::update via SpecialPowerModules::updateSpellBookRequirements (RW 0x6ADD22): edge-toggles
//      m_pausedByRequirements (+0x30) and pauseCountdown (slot 0x24, RW 0x896756) when
//      RequirementsFilterMPSkirmish flips in skirmish/MP.
//   2. SpecialPowerModule::initiate records m_targetObjectID (+0x2C, RW 0x897EA3, slot 0x50 RW 0x7B0F25)
//      when target != nullptr, and sets m_updateDisabled (+0x28, RW 0x897FD9..0x897FEB) when the driving
//      SpecialPowerUpdate is active (slot 0x08) and options carries kOptionUngated (0x40000); cleared by
//      interface slots 0x44 / 0x4C (RW 0x896751): SpecialAbilityUpdate::finishAbility (RW 0x85314C, S-859)
//      and StopSpecialPower::doSpecialPower (RW 0x8CAC5E).
//   3. SpecialPowerModuleInterface::isPaused() (slot 0x0C, RW 0x6519A6 / 0x89825A) implemented on
//      SpecialPowerModule, WeaponModeSpecialPowerUpdate, and SpecialPowerUpdateBase, and used by
//      ControlBar SPECIAL_POWER evaluation (RW 0x943520) instead of dynamic_cast<const SpecialPowerModule *>.
//   4. SpecialPowerModule::crc() and xfer() v2 (RW 0x89679D, S-529 item 2) hash and serialize
//      m_targetObjectID (+0x2C) and m_pausedByRequirements (+0x30) and reject version 1 / 3 blocks.
//   5. UnpauseSpecialPowerUpgrade (RW 0x8B9676 / 0x8B9701) pause count stacks cleanly with
//      RequirementsFilterMPSkirmish and drives ControlBar availability on WeaponModeSpecialPowerUpdate.
// Synthetic INI, no retail files. Expectations come from RotWK game.dat instructions cited at each case.

#include "doctest.h"
#include "CombatTestUtil.h"

#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/SpecialPower.h"
#include "Common/StateHash.h"
#include "Common/Upgrade.h"
#include "Common/Xfer.h"
#include "Common/XferLoad.h"
#include "Common/XferSave.h"
#include "GameClient/ControlBar.h"
#include "GameClient/ControlBarCommands.h"
#include "GameClient/HudContext.h"
#include "GameClient/InGameUI.h"
#include "GameClient/MessageStream/MessageStream.h"
#include "GameClient/TacticalView.h"
#include "GameClient/FXList.h"
#include "GameLogic/AttributeModifiers.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/HeroAbilityModules.h"
#include "GameLogic/Module/HeroModules.h"
#include "GameLogic/Module/SiegeModules.h"
#include "GameLogic/Module/SpecialAbilityModules.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Object/Object.h"

#include <cstdint>
#include <string>
#include <vector>

namespace
{
struct Port251World : combattest::CombatWorld
{
	SpecialPowerStore powers;
	SpecialPowerStore *savedPowers = TheSpecialPowerStore;
	FXListStore fxLists;
	FXListStore *savedFx = TheFXListStore;
	AttributeModifierStore modifiers;
	AttributeModifierStore *savedModifiers = TheAttributeModifierStore;
	CommandStore commands;
	CommandStore *savedCommands = TheCommandStore;

	explicit Port251World(const char *iniText)
	{
		TheSpecialPowerStore = &powers;
		TheFXListStore = &fxLists;
		TheAttributeModifierStore = &modifiers;
		commands.setThingFactory(&w.things);
		TheCommandStore = &commands;

		modifiers.registerBlock(w.fx.env.blocks);
		w.fx.env.blocks.registerBlock("FXList", [](INI *ini) { ParseFXListDefinitionGlobal(ini); });
		w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
		w.fx.env.blocks.registerBlock("CommandButton", [](INI *ini) { CommandStore::parseCommandButtonDefinitionGlobal(ini); });
		w.fx.env.blocks.registerBlock("CommandSet", [](INI *ini) { CommandStore::parseCommandSetDefinitionGlobal(ini); });

		SpecialAbilityModules::registerAll(w.modules);
		SpecialPowerModules::registerAll(w.modules);
		HeroModules::registerAll(w.modules);
		HeroAbilityModules::registerAll(w.modules);
		SiegeModules::registerAll(w.modules);

		const std::string err = w.load(iniText);
		REQUIRE_MESSAGE(err.empty(), err);
	}

	~Port251World()
	{
		TheCommandStore = savedCommands;
		TheSpecialPowerStore = savedPowers;
		TheFXListStore = savedFx;
		TheAttributeModifierStore = savedModifiers;
	}

	Object *spawnFor(const char *tmpl, const char *playerName, float x, float y)
	{
		return spawn(tmpl, x, y, 0.0f, players.findPlayerWithName(playerName)->getDefaultTeam());
	}
};

constexpr unsigned kOptionUngated = 0x40000u; // RW 0x897FE5 (SpecialPowerModules.cpp)

SpecialPowerModule *findPower(Object *obj, const char *powerName)
{
	const SpecialPowerTemplate *t = TheSpecialPowerStore->findSpecialPowerTemplate(powerName);
	REQUIRE(t != nullptr);
	SpecialPowerModule *sp = dynamic_cast<SpecialPowerModule *>(SpecialPowerModules::findModule(*obj, t));
	REQUIRE(sp != nullptr);
	return sp;
}

std::uint32_t hashModule(const SpecialPowerModule &sp)
{
	StateHasher h;
	sp.crc(h);
	return h.value();
}

const char kPort251Ini[] = R"(
Upgrade Upgrade_P251Unlock
  Type = OBJECT
End

SpecialPower SpecialPowerP251Req
  Enum = SPECIAL_SPELL_BOOK_WAR_CHANT
  ReloadTime = 10000
End

SpecialPower SpecialPowerP251Stacked
  Enum = SPECIAL_SPELL_BOOK_WAR_CHANT
  ReloadTime = 10000
End

SpecialPower SpecialPowerP251Ability
  Enum = SPECIAL_GENERAL_TARGETLESS
  ReloadTime = 1000
End

SpecialPower SpecialPowerP251Stop
  Enum = SPECIAL_GENERAL_TARGETLESS
  ReloadTime = 0
End

SpecialPower SpecialPowerP251WeaponMode
  Enum = SPECIAL_HERO_MODE
  ReloadTime = 1000
End

SpecialPower SpecialPowerP251Deploy
  Enum = SPECIAL_GENERAL_TARGETLESS
  ReloadTime = 1000
End

CommandButton Command_P251Req
  Command = SPECIAL_POWER
  SpecialPower = SpecialPowerP251Req
  InPalantir = Yes
End

CommandButton Command_P251Stacked
  Command = SPECIAL_POWER
  SpecialPower = SpecialPowerP251Stacked
  InPalantir = Yes
End

CommandButton Command_P251WeaponMode
  Command = SPECIAL_POWER
  SpecialPower = SpecialPowerP251WeaponMode
  InPalantir = Yes
End

CommandButton Command_P251Deploy
  Command = SPECIAL_POWER
  SpecialPower = SpecialPowerP251Deploy
  InPalantir = Yes
End

CommandSet P251SpellBookSet
  1 = Command_P251Req
  2 = Command_P251Stacked
End

CommandSet P251HeroSet
  1 = Command_P251WeaponMode
  2 = Command_P251Deploy
End

Object P251RequiredBuilding
  KindOf = STRUCTURE SELECTABLE
  Geometry = BOX
  GeometryMajorRadius = 20
  GeometryMinorRadius = 20
  GeometryHeight = 20
  Body = ActiveBody ModuleTag_Body
    MaxHealth = 500
  End
End

Object P251SpellBook
  KindOf = SPELL_BOOK UNATTACKABLE
  CommandSet = P251SpellBookSet
  Geometry = CYLINDER
  GeometryMajorRadius = 1
  GeometryMinorRadius = 1
  GeometryHeight = 1
  Body = ActiveBody ModuleTag_Body
    MaxHealth = 100
  End
  Behavior = SpecialPowerModule ModuleTag_Req
    SpecialPowerTemplate = SpecialPowerP251Req
    RequirementsFilterMPSkirmish = NONE +P251RequiredBuilding
  End
  Behavior = SpecialPowerModule ModuleTag_Stacked
    SpecialPowerTemplate = SpecialPowerP251Stacked
    StartsPaused = Yes
    RequirementsFilterMPSkirmish = NONE +P251RequiredBuilding
  End
  Behavior = UnpauseSpecialPowerUpgrade ModuleTag_UnpauseStacked
    SpecialPowerTemplate = SpecialPowerP251Stacked
    TriggeredBy = Upgrade_P251Unlock
  End
End

Object P251Caster
  KindOf = INFANTRY SELECTABLE CAN_ATTACK HERO
  Geometry = CYLINDER
  GeometryMajorRadius = 8
  GeometryMinorRadius = 8
  GeometryHeight = 20
  Body = ActiveBody ModuleTag_Body
    MaxHealth = 200
  End
  Behavior = AIUpdateInterface ModuleTag_AI
  End
  LocomotorSet
    Locomotor = WalkerLoco
    Condition = SET_NORMAL
    Speed = 55
  End
  Behavior = SpecialPowerModule ModuleTag_AbilityPower
    SpecialPowerTemplate = SpecialPowerP251Ability
    UpdateModuleStartsAttack = Yes
  End
  Behavior = SpecialAbilityUpdate ModuleTag_AbilityUpdate
    SpecialPowerTemplate = SpecialPowerP251Ability
    StartAbilityRange = 200
    UnpackTime = 200
    PreparationTime = 200
    PackTime = 200
    IgnoreFacingCheck = Yes
  End
End

Object P251Stopper
  KindOf = INFANTRY SELECTABLE CAN_ATTACK HERO
  CommandSet = P251HeroSet
  Geometry = CYLINDER
  GeometryMajorRadius = 8
  GeometryMinorRadius = 8
  GeometryHeight = 20
  Body = ActiveBody ModuleTag_Body
    MaxHealth = 200
  End
  Behavior = AIUpdateInterface ModuleTag_AI
  End
  LocomotorSet
    Locomotor = WalkerLoco
    Condition = SET_NORMAL
    Speed = 55
  End
  Behavior = SpecialPowerModule ModuleTag_AbilityPower
    SpecialPowerTemplate = SpecialPowerP251Ability
    UpdateModuleStartsAttack = Yes
  End
  Behavior = SpecialAbilityUpdate ModuleTag_AbilityUpdate
    SpecialPowerTemplate = SpecialPowerP251Ability
    StartAbilityRange = 200
    UnpackTime = 1000
    PreparationTime = 1000
    PackTime = 1000
    IgnoreFacingCheck = Yes
  End
  Behavior = StopSpecialPower ModuleTag_Stop
    SpecialPowerTemplate = SpecialPowerP251Stop
    StopPowerTemplate = SpecialPowerP251Ability
    StartsPaused = Yes
  End
End

Object P251Target
  KindOf = INFANTRY SELECTABLE
  Geometry = CYLINDER
  GeometryMajorRadius = 8
  GeometryMinorRadius = 8
  GeometryHeight = 20
  Body = ActiveBody ModuleTag_Body
    MaxHealth = 200
  End
End

Object P251WeaponModeHero
  KindOf = INFANTRY SELECTABLE CAN_ATTACK HERO
  CommandSet = P251HeroSet
  Geometry = CYLINDER
  GeometryMajorRadius = 8
  GeometryMinorRadius = 8
  GeometryHeight = 20
  Body = ActiveBody ModuleTag_Body
    MaxHealth = 200
  End
  Behavior = AIUpdateInterface ModuleTag_AI
  End
  Behavior = WeaponModeSpecialPowerUpdate ModuleTag_WeaponMode
    SpecialPowerTemplate = SpecialPowerP251WeaponMode
    StartsPaused = Yes
    Duration = 2000
  End
  Behavior = UnpauseSpecialPowerUpgrade ModuleTag_UnpauseWM
    SpecialPowerTemplate = SpecialPowerP251WeaponMode
    TriggeredBy = Upgrade_P251Unlock
  End
  Behavior = SiegeDeployHordeSpecialPower ModuleTag_Deploy
    SpecialPowerTemplate = SpecialPowerP251Deploy
    StartsPaused = Yes
  End
End
)";
} // namespace

TEST_CASE("PORT-251: spellbook RequirementsFilterMPSkirmish edge-pauses and unpauses per Player::update (RW 0x6ADD22 -> 0x896A32)")
{
	Port251World f(kPort251Ini);
	Player *alice = f.players.findPlayerWithName("Alice");
	Player *bob = f.players.findPlayerWithName("Bob");
	REQUIRE(alice != nullptr);
	REQUIRE(bob != nullptr);

	// Skirmish/MP mode for Alice; non-skirmish mode for Bob (neighbor check).
	alice->science().bind(nullptr, SpellGameMode{ true, false }, nullptr);
	bob->science().bind(nullptr, SpellGameMode{ false, false }, nullptr);

	Object *aliceBook = f.spawnFor("P251SpellBook", "Alice", 50.0f, 50.0f);
	Object *bobBook = f.spawnFor("P251SpellBook", "Bob", 250.0f, 50.0f);
	SpecialPowerModule *aliceSp = findPower(aliceBook, "SpecialPowerP251Req");
	SpecialPowerModule *bobSp = findPower(bobBook, "SpecialPowerP251Req");

	// Before first Player::update: neither is paused by requirements yet.
	CHECK(aliceSp->isPaused() == false);
	CHECK(aliceSp->pauseCount() == 0);
	CHECK(aliceSp->isPausedByRequirements() == false);

	// Frame 1: Alice lacks P251RequiredBuilding -> RW 0x6ADD22 calls slot 0x58 (RW 0x896A32),
	// setting m_pausedByRequirements (+0x30) and pausing countdown once.
	f.frames(1);
	CHECK(aliceSp->requirementsMet() == false);
	CHECK(aliceSp->isPausedByRequirements() == true);
	CHECK(aliceSp->isPaused() == true);
	CHECK(aliceSp->pauseCount() == 1);

	// Neighbor: Bob is not in skirmish/MP mode, so his spellbook power is untouched.
	CHECK(bobSp->requirementsMet() == true);
	CHECK(bobSp->isPausedByRequirements() == false);
	CHECK(bobSp->isPaused() == false);
	CHECK(bobSp->pauseCount() == 0);

	// Additional ticks while requirement remains unmet must stay edge-triggered (pauseCount stays 1, not incremented each frame).
	f.frames(4);
	CHECK(aliceSp->isPausedByRequirements() == true);
	CHECK(aliceSp->pauseCount() == 1);
	CHECK(aliceSp->isPaused() == true);

	// Spawn the required building for Alice -> next Player::update clears +0x30 and unpauses once.
	Object *bldg = f.spawnFor("P251RequiredBuilding", "Alice", 100.0f, 100.0f);
	f.frames(1);
	CHECK(aliceSp->requirementsMet() == true);
	CHECK(aliceSp->isPausedByRequirements() == false);
	CHECK(aliceSp->isPaused() == false);
	CHECK(aliceSp->pauseCount() == 0);

	// Another frame with requirement still met must not underflow pauseCount.
	f.frames(1);
	CHECK(aliceSp->isPausedByRequirements() == false);
	CHECK(aliceSp->pauseCount() == 0);

	// Destroying the required building re-pauses on the next frame.
	f.logic->destroyObject(bldg);
	f.frames(1);
	CHECK(aliceSp->requirementsMet() == false);
	CHECK(aliceSp->isPausedByRequirements() == true);
	CHECK(aliceSp->isPaused() == true);
	CHECK(aliceSp->pauseCount() == 1);
}

TEST_CASE("PORT-251: initiate records m_targetObjectID (+0x2C, RW 0x897EA3 / 0x7B0F25) and m_updateDisabled (+0x28, RW 0x897FD9..0x897FEB), cleared by finishAbility (RW 0x85314C -> 0x896751)")
{
	Port251World f(kPort251Ini);
	Object *casterA = f.spawnFor("P251Caster", "Alice", 100.0f, 100.0f);
	Object *casterB = f.spawnFor("P251Caster", "Alice", 100.0f, 140.0f);
	Object *target1 = f.spawnFor("P251Target", "Bob", 140.0f, 100.0f);
	Object *target2 = f.spawnFor("P251Target", "Bob", 140.0f, 140.0f);

	SpecialPowerModule *spA = findPower(casterA, "SpecialPowerP251Ability");
	SpecialPowerModule *spB = findPower(casterB, "SpecialPowerP251Ability");
	const SpecialPowerTemplate *tmpl = TheSpecialPowerStore->findSpecialPowerTemplate("SpecialPowerP251Ability");
	SpecialPowerUpdateInterface *updA = SpecialAbilityModules::findUpdate(*casterA, tmpl);
	SpecialPowerUpdateInterface *updB = SpecialAbilityModules::findUpdate(*casterB, tmpl);
	REQUIRE(updA != nullptr);
	REQUIRE(updB != nullptr);

	// Wait until cooldown finishes (ReloadTime = 1000ms = 5 frames).
	f.frames(6);
	CHECK(spA->isReady() == true);
	CHECK(spA->getTargetObjectID() == INVALID_ID);
	CHECK(spA->isUpdateDisabled() == false);

	// Initiate without kOptionUngated (options = 0): +0x2C records target1->getID(), +0x28 remains false.
	spA->doSpecialPowerAtObject(target1, 0u);
	CHECK(updA->isActive() == true);
	CHECK(spA->getTargetObjectID() == target1->getID());
	CHECK(spA->isUpdateDisabled() == false);

	// Initiate with kOptionUngated (0x40000) while driving update becomes active: +0x2C records target2->getID(), +0x28 becomes true (RW 0x897FD9..0x897FEB).
	spB->doSpecialPowerAtObject(target2, kOptionUngated);
	CHECK(updB->isActive() == true);
	CHECK(spB->getTargetObjectID() == target2->getID());
	CHECK(spB->isUpdateDisabled() == true);

	// While m_updateDisabled is true, refreshTimer() (RW 0x896F99) must not reset readyFrame to now().
	spB->setReadyFrame(f.logic->getFrame() + 40u);
	const unsigned expectedReady = spB->getReadyFrame();
	spB->refreshTimer();
	CHECK(spB->getReadyFrame() == expectedReady);

	// Step frames until SpecialAbilityUpdate finishes -> finishAbility calls slot 0x4C (RW 0x85314C -> RW 0x896751), clearing +0x28.
	for (int i = 0; i < 20 && updB->isActive(); ++i)
	{
		f.frames(1);
	}
	CHECK(updB->isActive() == false);
	CHECK(spB->isUpdateDisabled() == false);

	// Location-only initiate (target == nullptr) leaves the stored +0x2C target ID untouched (RW 0x897E94 test eax,eax; jz).
	f.frames(6);
	const Coord3D loc{ 160.0f, 140.0f, 0.0f };
	spB->doSpecialPowerAtLocation(loc, 0u);
	CHECK(spB->getTargetObjectID() == target2->getID());
}

TEST_CASE("PORT-251: StopSpecialPower clears m_updateDisabled via interface slot 0x44 (RW 0x8CAC5E -> 0x896751)")
{
	Port251World f(kPort251Ini);
	Object *stopper = f.spawnFor("P251Stopper", "Alice", 100.0f, 100.0f);
	Object *target = f.spawnFor("P251Target", "Bob", 140.0f, 100.0f);

	SpecialPowerModule *abilitySp = findPower(stopper, "SpecialPowerP251Ability");
	const SpecialPowerTemplate *stopTmpl = TheSpecialPowerStore->findSpecialPowerTemplate("SpecialPowerP251Stop");
	REQUIRE(stopTmpl != nullptr);
	SpecialPowerModuleInterface *stopSp = SpecialPowerModules::findModule(*stopper, stopTmpl);
	REQUIRE(stopSp != nullptr);

	// StopSpecialPower starts paused (StartsPaused = Yes), exercising SpecialPowerUpdateBase::isPaused() (RW 0x89825A).
	CHECK(stopSp->isPaused() == true);
	stopSp->pauseCountdown(false);
	CHECK(stopSp->isPaused() == false);

	// Initiate the long-duration ability with kOptionUngated so m_updateDisabled (+0x28) is set.
	f.frames(6);
	abilitySp->doSpecialPowerAtObject(target, kOptionUngated);
	CHECK(abilitySp->isUpdateDisabled() == true);

	// Invoking StopSpecialPower::doSpecialPower (RW 0x8CAC43 -> 0x8CAC5E slot 0x44) clears +0x28 on the target power module.
	stopSp->doSpecialPower(0u);
	CHECK(abilitySp->isUpdateDisabled() == false);
}

TEST_CASE("PORT-251: ControlBar SPECIAL_POWER checks interface isPaused() (RW 0x6519A6 / 0x89825A) for WeaponModeSpecialPowerUpdate and SpecialPowerUpdateBase")
{
	Port251World f(kPort251Ini);
	Player *alice = f.players.findPlayerWithName("Alice");
	REQUIRE(alice != nullptr);
	f.players.setLocalPlayer(alice);

	PinholeView view;
	InGameUI ui;
	MessageStream stream;
	ui.setLogic(f.logic.get());
	ui.setMaxSelectCount(50);
	HudContext context{ *f.logic, nullptr, view, ui, stream, &f.commands, {}, {} };
	ControlBar bar{ context };

	Object *hero = f.spawnFor("P251WeaponModeHero", "Alice", 100.0f, 100.0f);
	const SpecialPowerTemplate *wmTmpl = TheSpecialPowerStore->findSpecialPowerTemplate("SpecialPowerP251WeaponMode");
	const SpecialPowerTemplate *deployTmpl = TheSpecialPowerStore->findSpecialPowerTemplate("SpecialPowerP251Deploy");
	REQUIRE(wmTmpl != nullptr);
	REQUIRE(deployTmpl != nullptr);

	SpecialPowerModuleInterface *wmSp = SpecialPowerModules::findModule(*hero, wmTmpl);
	SpecialPowerModuleInterface *deploySp = SpecialPowerModules::findModule(*hero, deployTmpl);
	REQUIRE(wmSp != nullptr);
	REQUIRE(deploySp != nullptr);
	REQUIRE(dynamic_cast<WeaponModeSpecialPowerUpdate *>(wmSp) != nullptr);
	REQUIRE(dynamic_cast<SpecialPowerUpdateBase *>(deploySp) != nullptr);

	// Advance past ReloadTime (1000ms = 5 frames) so readiness is solely gated by pause state.
	f.frames(6);

	auto findButton = [&bar](const char *powerName) -> const ControlBarButton * {
		for (const ControlBarButton &b : bar.palantirButtons())
		{
			if (b.button && b.button->m_specialPowerName == powerName)
			{
				return &b;
			}
		}
		return nullptr;
	};

	ui.deselectAll(false);
	ui.selectObject(hero->getID());
	bar.update();

	// Both WeaponModeSpecialPowerUpdate and SiegeDeployHordeSpecialPower (SpecialPowerUpdateBase) have StartsPaused = Yes
	// with 0.0f paused progress: ControlBar must report Restricted via SpecialPowerModuleInterface::isPaused() (not NotReady!).
	CHECK(wmSp->isPaused() == true);
	CHECK(deploySp->isPaused() == true);
	CHECK(wmSp->getPercentReadyForDisplay() == 0.0f);
	CHECK(deploySp->getPercentReadyForDisplay() == 0.0f);
	const ControlBarButton *wmBtn = findButton("SpecialPowerP251WeaponMode");
	const ControlBarButton *deployBtn = findButton("SpecialPowerP251Deploy");
	REQUIRE(wmBtn != nullptr);
	REQUIRE(deployBtn != nullptr);
	CHECK(wmBtn->state == ButtonState::Restricted);
	CHECK(deployBtn->state == ButtonState::Restricted);

	// Granting Upgrade_P251Unlock triggers UnpauseSpecialPowerUpgrade::upgradeImplementation (RW 0x8B9676),
	// unpausing WeaponModeSpecialPowerUpdate and flipping its ControlBar button to Enabled.
	const UpgradeTemplate *unlock = TheUpgradeCenter->findUpgrade("Upgrade_P251Unlock");
	REQUIRE(unlock != nullptr);
	hero->giveUpgrade(unlock);
	CHECK(wmSp->isPaused() == false);
	bar.update();
	wmBtn = findButton("SpecialPowerP251WeaponMode");
	deployBtn = findButton("SpecialPowerP251Deploy");
	REQUIRE(wmBtn != nullptr);
	REQUIRE(deployBtn != nullptr);
	CHECK(wmBtn->state == ButtonState::Enabled);
	CHECK(deployBtn->state == ButtonState::Restricted);

	// Unpausing SiegeDeployHordeSpecialPower (SpecialPowerUpdateBase) and advancing past its 5-frame reload flips it to Enabled.
	deploySp->pauseCountdown(false);
	CHECK(deploySp->isPaused() == false);
	f.frames(6);
	bar.update();
	deployBtn = findButton("SpecialPowerP251Deploy");
	REQUIRE(deployBtn != nullptr);
	CHECK(deployBtn->state == ButtonState::Enabled);

	// Resetting upgrades (RW 0x68E083 -> RW 0x8D2688 -> UnpauseSpecialPowerUpgrade::processUpgradeRemoval RW 0x8B9701)
	// re-pauses WeaponModeSpecialPowerUpdate while its paused progress is 1.0f (> 0.0f): per RW 0x943585..0x9435D2,
	// positive paused progress reports NotReady (recharge clock) rather than Restricted.
	hero->resetUpgrades();
	CHECK(wmSp->isPaused() == true);
	CHECK(wmSp->getPercentReadyForDisplay() == 1.0f);
	bar.update();
	wmBtn = findButton("SpecialPowerP251WeaponMode");
	REQUIRE(wmBtn != nullptr);
	CHECK(wmBtn->state == ButtonState::NotReady);
}

TEST_CASE("PORT-251: SpecialPowerModule::crc and xfer v2 round-trip m_targetObjectID (+0x2C) and m_pausedByRequirements (+0x30) and reject v1/v3 (RW 0x89679D)")
{
	Port251World f(kPort251Ini);
	Player *alice = f.players.findPlayerWithName("Alice");
	REQUIRE(alice != nullptr);
	alice->science().bind(nullptr, SpellGameMode{ true, false }, nullptr);

	Object *book1 = f.spawnFor("P251SpellBook", "Alice", 50.0f, 50.0f);
	Object *book2 = f.spawnFor("P251SpellBook", "Alice", 80.0f, 50.0f);
	Object *target = f.spawnFor("P251Target", "Bob", 140.0f, 50.0f);

	SpecialPowerModule *src = findPower(book1, "SpecialPowerP251Req");
	SpecialPowerModule *dst = findPower(book2, "SpecialPowerP251Req");

	const std::uint32_t hashClean = hashModule(*src);

	// Mutating m_pausedByRequirements (+0x30) via Player::update changes crc().
	f.frames(1);
	CHECK(src->isPausedByRequirements() == true);
	CHECK(src->pauseCount() == 1);
	const std::uint32_t hashReqPaused = hashModule(*src);
	CHECK(hashReqPaused != hashClean);

	// Use ungated target initiate so +0x2C is populated even while paused, and verify crc() changes again.
	src->doSpecialPowerAtObject(target, kOptionUngated);
	CHECK(src->getTargetObjectID() == target->getID());
	const std::uint32_t hashBoth = hashModule(*src);
	CHECK(hashBoth != hashReqPaused);

	// Save v2 snapshot of src.
	XferSave save;
	src->xfer(save);
	const std::vector<std::uint8_t> bytes = save.data();
	REQUIRE(!bytes.empty());
	CHECK(bytes.front() == 2u);

	// Load into fresh dst (whose +0x2C is INVALID_ID and whose +0x30 is false because book1 is Alice's cached spellbook).
	CHECK(dst->getTargetObjectID() == INVALID_ID);
	CHECK(dst->isPausedByRequirements() == false);
	CHECK(dst->pauseCount() == 0);
	dst->setReadyFrame(0u);
	XferLoad load(bytes);
	dst->xfer(load);
	CHECK(load.atEnd() == true);
	CHECK(dst->getTargetObjectID() == target->getID());
	CHECK(dst->isPausedByRequirements() == true);
	CHECK(dst->isPaused() == true);
	CHECK(dst->pauseCount() == 1);
	CHECK(hashModule(*dst) == hashBoth);

	// Version 1 (pre-PORT-251) and version 3 (future) blocks are refused with XferException.
	std::vector<std::uint8_t> v1 = bytes;
	v1.front() = 1u;
	XferLoad loadV1(v1);
	CHECK_THROWS_AS(dst->xfer(loadV1), XferException);

	std::vector<std::uint8_t> v3 = bytes;
	v3.front() = 3u;
	XferLoad loadV3(v3);
	CHECK_THROWS_AS(dst->xfer(loadV3), XferException);
}

TEST_CASE("PORT-251 neighbor: UnpauseSpecialPowerUpgrade stacks cleanly with RequirementsFilterMPSkirmish pause count (RW 0x8B9676 / 0x8B9701 + 0x896A32)")
{
	Port251World f(kPort251Ini);
	Player *alice = f.players.findPlayerWithName("Alice");
	REQUIRE(alice != nullptr);
	alice->science().bind(nullptr, SpellGameMode{ true, false }, nullptr);

	Object *book = f.spawnFor("P251SpellBook", "Alice", 50.0f, 50.0f);
	SpecialPowerModule *stacked = findPower(book, "SpecialPowerP251Stacked");
	const UpgradeTemplate *unlock = TheUpgradeCenter->findUpgrade("Upgrade_P251Unlock");
	REQUIRE(unlock != nullptr);

	// StartsPaused = Yes sets pauseCount to 1 before any Player::update.
	CHECK(stacked->pauseCount() == 1);
	CHECK(stacked->isPaused() == true);
	CHECK(stacked->isPausedByRequirements() == false);

	// Frame 1 without P251RequiredBuilding adds the requirements pause -> pauseCount == 2.
	f.frames(1);
	CHECK(stacked->isPausedByRequirements() == true);
	CHECK(stacked->pauseCount() == 2);
	CHECK(stacked->isPaused() == true);

	// Granting Upgrade_P251Unlock drops pauseCount 2 -> 1; power stays paused because requirement is still unmet.
	book->giveUpgrade(unlock);
	CHECK(stacked->pauseCount() == 1);
	CHECK(stacked->isPaused() == true);
	CHECK(stacked->isPausedByRequirements() == true);

	// Spawning P251RequiredBuilding drops pauseCount 1 -> 0 on the next update; power is now unpaused.
	f.spawnFor("P251RequiredBuilding", "Alice", 120.0f, 120.0f);
	f.frames(1);
	CHECK(stacked->isPausedByRequirements() == false);
	CHECK(stacked->pauseCount() == 0);
	CHECK(stacked->isPaused() == false);

	// Resetting upgrades (RW 0x68E083 -> RW 0x8D2688 -> processUpgradeRemoval RW 0x8B9701) increments pauseCount 0 -> 1 without touching m_pausedByRequirements.
	book->resetUpgrades();
	CHECK(stacked->pauseCount() == 1);
	CHECK(stacked->isPaused() == true);
	CHECK(stacked->isPausedByRequirements() == false);
}
