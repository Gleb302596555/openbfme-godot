// OpenBFME. GPL-3.0.
//
// LOCAL-185 (fork issue 978): matrix red buttons — weapon abilities that send or fire nothing:
//   - Group A: FIRE_WEAPON with EnableOnModelCondition (RohanErkenbrand MOUNTED, HaradMumakil USER_9; RW 0x942490, RW 0x942733, RW 0x940435)
//   - Group B: GrabNugget slot 1 (RW 0x910C27) vs slot 5 (RW 0x910C8A -> 0x910D13 -> 0x910ACA) and TransportContain default RULE_NONE (RW 0x86B5A3 -> 0x763D11)
//   - Group C: AngmarNecroAcolyteBook / AngmarNecronomicon standalone horde member/banner (necronomicon.ini, RW 0x82E913, RW 0x75638B, RW 0x66D338)
//
// Applies to both RotWK 2.01 (RW) and BFME2 1.06 rule sets unless a cited rule differs.

#include "doctest.h"
#include "CombatTestUtil.h"
#include "EnnorathScenario.h"
#include "FactbRig.h"

#include "Common/GameEdition.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/SpecialPower.h"
#include "GameClient/FXList.h"
#include "GameLogic/ActionManager.h"
#include "GameLogic/AI/AIStateMachine.h"
#include "GameLogic/AttributeModifiers.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Combat/CombatState.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/HeroAbilityModules.h"
#include "GameLogic/Module/PhysicsBehavior.h"
#include "GameLogic/Module/SpecialAbilityModules.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Object/Contain/TransportContainRuntime.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/ObjectCreationList.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/WeaponStores.h"

#include <string>

namespace
{
struct Local185World : combattest::CombatWorld
{
	SpecialPowerStore powers;
	SpecialPowerStore *savedPowers = TheSpecialPowerStore;
	FXListStore fxLists;
	FXListStore *savedFx = TheFXListStore;
	AttributeModifierStore modifiers;
	AttributeModifierStore *savedModifiers = TheAttributeModifierStore;
	ObjectCreationListStore oclStore;
	ObjectCreationListStore *savedOcl = TheObjectCreationListStore;

	explicit Local185World(const char *extraIni)
		: combattest::CombatWorld("", 100)
		, oclStore(w.keys)
	{
		logic->setEdition(GameEdition::RotWK201);
		logic->combat().setAutoAcquireEnabled(false);

		TheSpecialPowerStore = &powers;
		TheFXListStore = &fxLists;
		TheAttributeModifierStore = &modifiers;
		TheObjectCreationListStore = &oclStore;

		modifiers.registerBlock(w.fx.env.blocks);
		w.fx.env.blocks.registerBlock("FXList", [](INI *ini) { ParseFXListDefinitionGlobal(ini); });
		w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
		w.fx.env.blocks.registerBlock("ObjectCreationList", [](INI *ini) { ObjectCreationListStore::parseObjectCreationListDefinitionGlobal(ini); });

		SpecialAbilityModules::registerAll(w.modules);
		SpecialPowerModules::registerAll(w.modules);
		SpecialPowerModules::installScienceHooks(*logic);
		HeroAbilityModules::registerHandlers(dispatcher);
		TransportContain::registerClasses(w.modules);
		PhysicsBehavior::registerClass(w.modules);

		const std::string err = w.load(extraIni, INI_LOAD_OVERWRITE, "local185.ini");
		REQUIRE_MESSAGE(err.empty(), err);
	}
	~Local185World()
	{
		logic->reset();
		TheObjectCreationListStore = savedOcl;
		TheAttributeModifierStore = savedModifiers;
		TheFXListStore = savedFx;
		TheSpecialPowerStore = savedPowers;
	}
};
} // namespace

// Expectations from RotWK 2.01:
//   - TransportContainModuleData ctor (RW 0x86B5A3 -> RW 0x763D11): omitted PassengerFilter defaults to RULE_NONE -> isValidContainerFor returns false (RW 0x86A002 -> RW 0x86603B).
//   - WeaponTemplate::fireWeaponTemplate (RW 0x6CCEE6) calls GrabNugget slot 1 (RW 0x910C27), which checks isValidContainerFor(victim, 1, 0) BEFORE calling slot 5 (RW 0x910C8A).
//   - Therefore omitted PassengerFilter skips GrabNugget::doEffect completely (no contain, no shockwave), whereas valid PassengerFilter + ContainTargetOnEffect = Yes contains the victim.
TEST_CASE("LOCAL-185 GrabNugget slot 1 gate with omitted vs active TransportContain PassengerFilter (RW 0x6CCEE6, RW 0x910C27, RW 0x910C8A, RW 0x86B5A3)")
{
	const char *ini = R"(
SpecialPower SpecialAbilityGrabTest
  Enum = SPECIAL_GENERAL_TARGETLESS
  ReloadTime = 1000
End

Weapon TestGrabWeapon
  AttackRange = 50.0
  DelayBetweenShots = 100
  FiringDuration = 100
  MeleeWeapon = Yes
  GrabNugget
    SpecialObjectFilter = NONE +INFANTRY
    ContainTargetOnEffect = Yes
    RemoveTargetFromOtherContain = Yes
    ImpactTargetOnEffect = Yes
    ShockWaveAmount = 50.0
    ShockWaveRadius = 16.0
    ShockWaveTaperOff = 0.75
    ShockWaveZMult = 1.1
  End
End

Object GrabCasterNoFilter
  KindOf = SELECTABLE CAN_ATTACK
  Body = ActiveBody ModuleTag_Body
    MaxHealth = 500
  End
  Behavior = AIUpdateInterface ModuleTag_AI
    AutoAcquireEnemiesWhenIdle = No
  End
  Behavior = TransportContain ModuleTag_Contain
    Slots = 1
    AllowEnemiesInside = Yes
  End
  Behavior = SpecialPowerModule ModuleTag_SP
    SpecialPowerTemplate = SpecialAbilityGrabTest
    UpdateModuleStartsAttack = Yes
  End
  Behavior = WeaponFireSpecialAbilityUpdate ModuleTag_WF
    SpecialPowerTemplate = SpecialAbilityGrabTest
    SpecialWeapon = TestGrabWeapon
    StartAbilityRange = 50.0
    IgnoreFacingCheck = Yes
  End
End

Object GrabCasterWithFilter
  KindOf = SELECTABLE CAN_ATTACK
  Body = ActiveBody ModuleTag_Body
    MaxHealth = 500
  End
  Behavior = AIUpdateInterface ModuleTag_AI
    AutoAcquireEnemiesWhenIdle = No
  End
  Behavior = TransportContain ModuleTag_Contain
    Slots = 1
    PassengerFilter = ANY +INFANTRY
    AllowEnemiesInside = Yes
  End
  Behavior = SpecialPowerModule ModuleTag_SP
    SpecialPowerTemplate = SpecialAbilityGrabTest
    UpdateModuleStartsAttack = Yes
  End
  Behavior = WeaponFireSpecialAbilityUpdate ModuleTag_WF
    SpecialPowerTemplate = SpecialAbilityGrabTest
    SpecialWeapon = TestGrabWeapon
    StartAbilityRange = 50.0
    IgnoreFacingCheck = Yes
  End
End

Object GrabVictim
  KindOf = SELECTABLE INFANTRY
  TransportSlotCount = 1
  Body = ActiveBody ModuleTag_Body
    MaxHealth = 200
  End
  Behavior = PhysicsBehavior ModuleTag_Phys
    GravityMult = 1.0
  End
End
)";
	Local185World w(ini);
	const SpecialPowerTemplate *sp = w.powers.findSpecialPowerTemplate("SpecialAbilityGrabTest");
	REQUIRE(sp != nullptr);
	REQUIRE(TheWeaponStore->findWeaponTemplate("TestGrabWeapon") != nullptr);

	// 1) Omitted PassengerFilter -> RULE_NONE (RW 0x86B5A3): slot 1 (RW 0x910C27) rejects -> neither contain nor shockwave
	Object *casterNoFilter = w.spawn("GrabCasterNoFilter", 100.0f, 100.0f, 0.0f, w.teamA());
	Object *victim1 = w.spawn("GrabVictim", 110.0f, 100.0f, 0.0f, w.teamB());
	REQUIRE(casterNoFilter != nullptr);
	REQUIRE(victim1 != nullptr);
	REQUIRE(casterNoFilter->getContain() != nullptr);
	CHECK_FALSE(casterNoFilter->getContain()->isValidContainerFor(*victim1, true, false));

	const unsigned long long flingsBefore = w.logic->combat().counters().shockwaveFlings + w.logic->combat().counters().shockwaveStandUps;
	REQUIRE(SpecialPowerModules::doSpecialPowerAtObject(*casterNoFilter, sp, victim1, 0, false));
	w.frames(15);
	const unsigned long long flingsAfter = w.logic->combat().counters().shockwaveFlings + w.logic->combat().counters().shockwaveStandUps;
	CHECK(flingsAfter == flingsBefore);
	CHECK(victim1->getContainedBy() == nullptr);

	// 2) Active PassengerFilter = ANY +INFANTRY -> slot 1 accepts -> slot 5 (RW 0x910C8A) contains victim
	Object *casterWithFilter = w.spawn("GrabCasterWithFilter", 200.0f, 100.0f, 0.0f, w.teamA());
	Object *victim2 = w.spawn("GrabVictim", 210.0f, 100.0f, 0.0f, w.teamB());
	REQUIRE(casterWithFilter != nullptr);
	REQUIRE(victim2 != nullptr);
	REQUIRE(casterWithFilter->getContain() != nullptr);
	CHECK(casterWithFilter->getContain()->isValidContainerFor(*victim2, true, false));

	REQUIRE(SpecialPowerModules::doSpecialPowerAtObject(*casterWithFilter, sp, victim2, 0, false));
	w.frames(15);
	CHECK(victim2->getContainedBy() == casterWithFilter);
}

// Expectations from Ennorath data (wildbuhrdur.ini:957, weapon.ini:26633) and RotWK 2.01 (RW 0x86B5A3, RW 0x910C27, RW 0x6CCEE6):
//   WildDuhrbur's TransportContain has ;PassengerFilter commented out -> RULE_NONE -> isValidContainerFor returns false ->
//   GrabNugget slot 1 (RW 0x910C27) skips doEffect; only SpecialModelConditionNugget (STUNNED_FLAILING) applies.
TEST_CASE("LOCAL-185 ennorath: WildDuhrbur Command_SpecialAbilityBuhrdurFressen matches RotWK GrabNugget slot 1 gate (wildbuhrdur.ini:957, RW 0x910C27)")
{
	ennorathscenario::World w;
	if (!w.load("LOCAL-185 WildDuhrbur"))
	{
		return;
	}
	factb::HudGame g(w.shared, "FactionWild");
	auto ctx = g.hud->enterContext();
	Player *enemy = g.logic().players().findPlayerWithName("Player_2");
	REQUIRE(enemy != nullptr);

	const Coord3D c = g.rig.freeSpot(1800, 1400, 300.0f);
	Object *buhrdur = g.rig.make("WildDuhrbur", c.x, c.y);
	Object *foeHorde = g.rig.make("GondorFighterHorde", c.x + 80.0f, c.y, enemy);
	REQUIRE(buhrdur != nullptr);
	REQUIRE(foeHorde != nullptr);
	g.step(5);

	Object *victim = foeHorde->getContain() && !foeHorde->getContain()->getContainedItemsList()->empty()
		? foeHorde->getContain()->getContainedItemsList()->front()
		: foeHorde;
	REQUIRE(victim != nullptr);
	REQUIRE(buhrdur->getContain() != nullptr);
	CHECK_FALSE(buhrdur->getContain()->isValidContainerFor(*victim, true, false));

	const SpecialPowerTemplate *sp = TheSpecialPowerStore->findSpecialPowerTemplate("SpecialAbilityAngmarShadeEatMeGrab");
	REQUIRE(sp != nullptr);
	SpecialPowerModuleInterface *spm = SpecialPowerModules::findModule(*buhrdur, sp);
	REQUIRE(spm != nullptr);
	const unsigned readyBefore = spm->getReadyFrame();
	REQUIRE(SpecialPowerModules::doSpecialPowerAtObject(*buhrdur, sp, victim, 0, false));
	bool fired = false;
	for (int f = 0; f < 120 && !fired; ++f)
	{
		g.step(1);
		fired = spm->getReadyFrame() != readyBefore;
	}
	CHECK(fired);
	CHECK(victim->getContainedBy() != buhrdur);
}

// Expectations from Ennorath data (necronomicon.ini, _gamedata.inc:1294, locomotor.ini:154) and RotWK 2.01 (RW 0x82E913, RW 0x75638B):
//   - AngmarNecroAcolyteBook has ShroudClearingRange = SHROUD_CLEAR_STANDARD (25) -> ActionManager::canDoSpecialPowerAtLocation (FINAL_CLEAR, RW 0x82E913)
//     returns false at 120 without reveal.
//   - AngmarNecronomicon has WebbedLocomotor (TurnTime = 999999999, Speed = 0) -> AIFacePositionState (RW 0x75638B) cannot complete turning to a side target,
//     and fires immediately when target is in front.
TEST_CASE("LOCAL-185 ennorath: AngmarNecroAcolyteBook and AngmarNecronomicon standalone RW shroud and WebbedLocomotor facing (necronomicon.ini, RW 0x82E913, RW 0x75638B)")
{
	ennorathscenario::World w;
	if (!w.load("LOCAL-185 AngmarNecro"))
	{
		return;
	}
	factb::HudGame g(w.shared, "FactionAngmar");
	auto ctx = g.hud->enterContext();
	Player *enemy = g.logic().players().findPlayerWithName("Player_2");
	REQUIRE(enemy != nullptr);
	const SpecialPowerTemplate *plague = TheSpecialPowerStore->findSpecialPowerTemplate("SpecialAbilityGulzarMorgulSeucheAcolyte");
	REQUIRE(plague != nullptr);

	{
		const Coord3D c = g.rig.freeSpot(1800, 1400, 300.0f);
		Object *book = g.rig.make("AngmarNecroAcolyteBook", c.x, c.y);
		REQUIRE(book != nullptr);
		book->giveUpgrade("Upgrade_MiniHordeLvl1");
		g.step(5);
		const Coord3D farLoc{ c.x + 120.0f, c.y, g.logic().getGroundHeight(c.x + 120.0f, c.y) };
		CHECK_FALSE(ActionManager::canDoSpecialPowerAtLocation(*book, farLoc, plague, 0));
		g.logic().destroyObject(book);
	}

	{
		const Coord3D c = g.rig.freeSpot(1800, 1400, 300.0f);
		Object *necro = g.rig.make("AngmarNecronomicon", c.x, c.y);
		REQUIRE(necro != nullptr);
		necro->giveUpgrade("Upgrade_MiniHordeLvl1");
		g.step(5);
		SpecialPowerModuleInterface *spm = SpecialPowerModules::findModule(*necro, plague);
		REQUIRE(spm != nullptr);
		const unsigned ready0 = spm->getReadyFrame();
		const Coord3D sideLoc{ c.x, c.y + 120.0f, g.logic().getGroundHeight(c.x, c.y + 120.0f) };
		REQUIRE(ActionManager::canDoSpecialPowerAtLocation(*necro, sideLoc, plague, 0));
		REQUIRE(SpecialPowerModules::doSpecialPowerAtLocation(*necro, plague, sideLoc, 0, false));
		g.step(20);
		CHECK(necro->getAIUpdateInterface()->stateMachine().currentStateId() == 37u); // AI_FACE_POSITION (0x25)
		CHECK(spm->getReadyFrame() == ready0);

		const Coord3D frontLoc{ c.x + 120.0f, c.y, g.logic().getGroundHeight(c.x + 120.0f, c.y) };
		REQUIRE(SpecialPowerModules::doSpecialPowerAtLocation(*necro, plague, frontLoc, 0, false));
		bool fired = false;
		for (int f = 0; f < 30 && !fired; ++f)
		{
			g.step(1);
			fired = spm->getReadyFrame() != ready0;
		}
		CHECK(fired);
		g.logic().destroyObject(necro);
	}
}

// Expectations from Ennorath data (commandbutton.inc:42962, commandbutton.ini:2921) and RotWK 2.01 (RW 0x942490, RW 0x942733, RW 0x940435):
//   - RohanErkenbrand Command_ErkenbrandChargeAttack (FIRE_WEAPON, EnableOnModelCondition = MOUNTED) is Restricted unmounted and Enabled after ToggleMounted.
//   - HaradMumakil Command_ChargeAttack (FIRE_WEAPON, EnableOnModelCondition = USER_9) is Restricted without USER_9.
TEST_CASE("LOCAL-185 ennorath: FIRE_WEAPON EnableOnModelCondition gates for RohanErkenbrand and HaradMumakil (commandbutton.inc:42962, commandbutton.ini:2921, RW 0x942490)")
{
	ennorathscenario::World w;
	if (!w.load("LOCAL-185 FIRE_WEAPON"))
	{
		return;
	}
	{
		factb::HudGame g(w.shared, "FactionRohan");
		auto ctx = g.hud->enterContext();
		const Coord3D c = g.rig.freeSpot(1800, 1400, 300.0f);
		Object *erk = g.rig.make("RohanErkenbrand", c.x, c.y);
		REQUIRE(erk != nullptr);
		g.step(5);
		ControlBarButton b{};
		REQUIRE(g.select(erk));
		REQUIRE(g.reveal(erk, [](const CommandButton &cb) { return cb.m_name == "Command_ErkenbrandChargeAttack"; }, b));
		CHECK(b.state == ButtonState::Restricted);
		ControlBarButton mount{};
		REQUIRE(g.reveal(erk, [](const CommandButton &cb) { return cb.m_specialPowerName == "SpecialAbilityToggleMounted"; }, mount));
		REQUIRE(g.press(mount));
		g.step(40);
		REQUIRE(g.select(erk));
		REQUIRE(g.reveal(erk, [](const CommandButton &cb) { return cb.m_name == "Command_ErkenbrandChargeAttack"; }, b));
		CHECK(b.state == ButtonState::Enabled);
		REQUIRE(g.press(b));
		CHECK(g.hud->input().ui().getGUICommand() == b.button);
	}
	{
		factb::HudGame g(w.shared, "FactionEvilmen");
		auto ctx = g.hud->enterContext();
		const Coord3D c = g.rig.freeSpot(1800, 1400, 300.0f);
		Object *mumak = g.rig.make("HaradMumakil", c.x, c.y);
		REQUIRE(mumak != nullptr);
		mumak->giveUpgrade("Upgrade_MumakilLevel2");
		g.step(5);
		ControlBarButton b{};
		REQUIRE(g.select(mumak));
		REQUIRE(g.reveal(mumak, [](const CommandButton &cb) { return cb.m_name == "Command_ChargeAttack"; }, b));
		CHECK(b.state == ButtonState::Restricted);
	}
}
