// OpenBFME. PORT-188 (issue #454, P-115): Radagast in eagle form — Word of Poultry replaces enemies and enters its
// 360 s recharge when cast with CommandTrigger = Command_SpecialAbilityConvertEnemyTracker.
//
// TARGET (RotWK 2.01 game.dat):
//   * SpecialAbilityUpdate::initiateIntentToDoSpecialPower RW 0x853062 .. 0x8530A4 (BFME2 1.06 0x004500A3,
//     SpecialAbilityUpdateInitiateIntent.cpp): walks the object's behavior modules, asks each SpecialPowerUpdateInterface
//     (vslot +0x64) whether isSpecialAbility() (+0x04, RW 0x8BD372 -> true) AND isPowerCurrentlyInUse(nullptr) (+0x18,
//     RW 0x851C92), and only then calls other->finishAbility(false, true) when other != this.
//   * SpecialAbilityUpdate::isPowerCurrentlyInUse RW 0x851C92 .. 0x851D0D (BFME2 1.06 0x0044EEC4): with cmd == nullptr,
//     returns false when m_packingState == 0 (STATE_NONE), true for SPECIAL_INFANTRY_CAPTURE_BUILDING (0x1D) and
//     SPECIAL_BLACKLOTUS_CAPTURE_BUILDING (0x1A), and m_withinRange (+0x60) for all other abilities; overridden by
//     ToggleMountedSpecialAbilityUpdate / SpecialDisguiseUpdate for 0x80 / 0x8F.
//
// Synthetic INI only (no retail or mod archives required). Expected values come verbatim from Ennorath 1.9.8.2:
//   * data\ini\includes\specialpower.inc:4538 (SpecialAbilityRadagastWordOfPoultry, ReloadTime = 360000)
//   * data\ini\specialpower.ini:1254..1272 (SpecialAbilityActivateeDummy ReloadTime = 1,
//     SpecialAbilityActivateeDummy3 ReloadTime = 1000)
//   * data\ini\object\goodfaction\units\rohan\rohanradagast.ini:2299..2352 (RohanRadagastaufdemAdler:
//     ModuleTag_CreateAHeroWordOfPoultryStarter_Level2 + ModuleTag_WordOfPoultryUpdate_Level2 ReplaceObjectUpdate +
//     ModuleTag_ConvertEnemyStarter + ModuleTag_ConvertEnemyTwoStarter + ModuleTag_ConvertEnemyDummyMover
//     ActivateModuleSpecialPower + ModuleTag_ConvertEnemyShadowTrigger OCLSpecialPower OCL_DrogothShadowRad)
//   * data\ini\includes\commandbutton.inc:2442..2516 (Command_SpecialAbilityRadagastConvertEnemyAdler:
//     DisableOnModelCondition = USER_50, CommandTrigger = Command_SpecialAbilityConvertEnemyTracker)
//   * data\ini\object\evilfaction\units\wild\projectile.ini:30825..30859 (RohanRadagastNoUseAbility:
//     Lifetime = 55000, RefreshDelay = 2000, Range = 9000, BonusName = RadagastConvertFixer)
//   * data\ini\includes\attributemodifier.inc:15563..15567 (RadagastConvertFixer: Duration = 3000, USER_50)
// GPL-3.0.

#include "doctest.h"

#include "CombatTestUtil.h"

#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/SpecialPower.h"
#include "Common/Thing/ThingTemplate.h"
#include "GameClient/ControlBar.h"
#include "GameClient/ControlBarCommands.h"
#include "GameClient/FXList.h"
#include "GameClient/HudInput.h"
#include "GameClient/TacticalView.h"
#include "GameLogic/AI/WeaponCommands.h"
#include "GameLogic/AttributeModifiers.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/GameMessage.h"
#include "GameLogic/Module/HeroAbilityModules.h"
#include "GameLogic/Module/ReplaceObjectUpdate.h"
#include "GameLogic/Module/SpecialAbilityModules.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/ObjectCreationList.h"
#include "GameLogic/SpellCommands.h"
#include "GameLogic/WeaponSetToggle.h"

#include <memory>
#include <string>
#include <vector>

namespace
{
const char kPort188Ini[] =
	"Locomotor SynthFellBeastLocomotor\n"
	"  Surfaces = AIR\n"
	"  MinSpeed = 0%\n"
	"  SlowTurnRadius = 32.0\n"
	"  FastTurnRadius = 48.0\n"
	"  TurnTime = 500\n"
	"  TurnTimeDamaged = 500\n"
	"  Acceleration = 400\n"
	"  Lift = 100%\n"
	"  LiftDamaged = 100%\n"
	"  Braking = 1000\n"
	"  MinTurnSpeed = 30%\n"
	"  PreferredHeight = 121\n"
	"  PreferredAttackHeight = 5\n"
	"  AllowAirborneMotiveForce = Yes\n"
	"  ZAxisBehavior = SURFACE_RELATIVE_HEIGHT\n"
	"  Appearance = GIANT_BIRD\n"
	"  CloseEnoughDist = 2\n"
	"End\n"
	"ModifierList Rider1Tracker\n"
	"  Category = SPELL\n"
	"  Duration = 2000\n"
	"  ModelCondition = RIDER1\n"
	"End\n"
	"ModifierList SynthRadagastConvertFixer\n"
	"  Category = SPELL\n"
	"  Duration = 3000\n"
	"  ModelCondition = USER_50\n"
	"End\n"
	"SpecialPower SpecialAbilityRadagastWordOfPoultry\n"
	"  Enum = SPECIAL_WORD_OF_POWER\n"
	"  ReloadTime = 360000\n"
	"  RadiusCursorRadius = 140.0\n"
	"End\n"
	"SpecialPower SpecialAbilityActivateeDummy\n"
	"  Enum = SPECIAL_GENERAL_TARGETLESS\n"
	"  ReloadTime = 1\n"
	"End\n"
	"SpecialPower SpecialAbilityActivateeDummy3\n"
	"  Enum = SPECIAL_GENERAL_TARGETLESS_THREE\n"
	"  ReloadTime = 1000\n"
	"End\n"
	"SpecialPower SpecialAbilityCaptureTest\n"
	"  Enum = SPECIAL_INFANTRY_CAPTURE_BUILDING\n"
	"  ReloadTime = 10000\n"
	"End\n"
	"SpecialPower SpecialAbilitySecondPowerTest\n"
	"  Enum = SPECIAL_GENERAL_TARGETLESS\n"
	"  ReloadTime = 10000\n"
	"End\n"
	"ObjectCreationList OCL_SynthDrogothShadowRad\n"
	"  CreateObject\n"
	"    ObjectNames = SynthRadagastNoUseAbility\n"
	"  End\n"
	"End\n"
	"CommandButton Command_SpecialAbilityConvertEnemyTracker\n"
	"  Command = SPECIAL_POWER\n"
	"  SpecialPower = SpecialAbilityActivateeDummy\n"
	"End\n"
	"CommandButton Command_SpecialAbilityRadagastConvertEnemyAdler\n"
	"  Command = SPECIAL_POWER\n"
	"  SpecialPower = SpecialAbilityRadagastWordOfPoultry\n"
	"  Options = NEED_TARGET_POS CONTEXTMODE_COMMAND\n"
	"  DisableOnModelCondition = USER_50\n"
	"  InPalantir = Yes\n"
	"  CommandTrigger = Command_SpecialAbilityConvertEnemyTracker\n"
	"End\n"
	"CommandSet SynthRadagastAdlerCommandSet\n"
	"  1 = Command_SpecialAbilityRadagastConvertEnemyAdler\n"
	"End\n"
	"Object SynthRadagastNoUseAbility\n"
	"  KindOf = IMMOBILE UNATTACKABLE INERT NO_COLLIDE\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 99999\n"
	"    InitialHealth = 99999\n"
	"  End\n"
	"  Behavior = LifetimeUpdate ModuleTag_Lifetime\n"
	"    MinLifetime = 55000\n"
	"    MaxLifetime = 55000\n"
	"  End\n"
	"  Behavior = AttributeModifierAuraUpdate ModuleTag_Aura\n"
	"    StartsActive = Yes\n"
	"    BonusName = SynthRadagastConvertFixer\n"
	"    RefreshDelay = 2000\n"
	"    Range = 9000\n"
	"    ObjectFilter = NONE +SynthRadagastAdler SAME_PLAYER\n"
	"  End\n"
	"End\n"
	"Object Chicken_Replacement\n"
	"  KindOf = INFANTRY SELECTABLE NO_COLLIDE\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 50\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 30\n"
	"  End\n"
	"End\n"
	"Object SynthEnemyInfantry\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 40\n"
	"  End\n"
	"End\n"
	"Object SynthCapturableBuilding\n"
	"  KindOf = STRUCTURE IMMOBILE CAPTURABLE SELECTABLE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryMinorRadius = 20\n"
	"  GeometryHeight = 30\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 1000\n"
	"  End\n"
	"End\n"
	"Object SynthRadagastAdler\n"
	"  KindOf = PRELOAD SELECTABLE CAN_CAST_REFLECTIONS SCORE HERO GIANT_BIRD MONSTER SCARY ARMY_SUMMARY\n"
	"  CommandSet = SynthRadagastAdlerCommandSet\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 16\n"
	"  GeometryMinorRadius = 16\n"
	"  GeometryHeight = 16\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 2500\n"
	"  End\n"
	"  Behavior = AttributeModifierPoolUpdate ModuleTag_ModPool\n"
	"  End\n"
	"  Behavior = GiantBirdAIUpdate ModuleTag_GiantBirdAI\n"
	"    AutoAcquireEnemiesWhenIdle = Yes ATTACK_BUILDINGS\n"
	"    FollowThroughDistance = 160\n"
	"    FollowThroughCheckStep = 50\n"
	"    FollowThroughGradient = 1.0\n"
	"    GrabTossTimeTrigger = 3.0\n"
	"    GrabTossHeightTrigger = 80.0\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = SynthFellBeastLocomotor\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 88\n"
	"  End\n"
	"  Behavior = SpecialPowerModule ModuleTag_CreateAHeroWordOfPoultryStarter_Level2\n"
	"    SpecialPowerTemplate = SpecialAbilityRadagastWordOfPoultry\n"
	"    StartsPaused = No\n"
	"    UpdateModuleStartsAttack = Yes\n"
	"    SetModelCondition = ModelConditionState:RIDER1\n"
	"    SetModelConditionTime = 3.0\n"
	"  End\n"
	"  Behavior = ReplaceObjectUpdate ModuleTag_WordOfPoultryUpdate_Level2\n"
	"    SpecialPowerTemplate = SpecialAbilityRadagastWordOfPoultry\n"
	"    TriggerAttributeModifier = Rider1Tracker\n"
	"    AttributeModifierDuration = 2000\n"
	"    UnpackingVariation = 1\n"
	"    UnpackTime = 800\n"
	"    PreparationTime = 1\n"
	"    PackTime = 100\n"
	"    AwardXPForTriggering = 0\n"
	"    StartAbilityRange = 300.0\n"
	"    MustFinishAbility = Yes\n"
	"    ReplaceObject\n"
	"      TargetObjectFilter = NONE +INFANTRY -HERO ENEMIES NEUTRAL -IsengardRamCrew -MordorRamCrew -DwarvenBattleWagonAxeThrower -DwarvenBattleWagonMenOfDale -DwarvenBattleWagonPhalanx -MordorHaradrimArcherMumakil\n"
	"      ReplacementObjectName = Chicken_Replacement\n"
	"    End\n"
	"    ReplaceRadius = 140.0\n"
	"    Scatter = Yes\n"
	"  End\n"
	"  Behavior = SpecialPowerModule ModuleTag_ConvertEnemyStarter\n"
	"    SpecialPowerTemplate = SpecialAbilityActivateeDummy\n"
	"    StartsPaused = No\n"
	"    UpdateModuleStartsAttack = Yes\n"
	"  End\n"
	"  Behavior = SpecialPowerModule ModuleTag_ConvertEnemyTwoStarter\n"
	"    SpecialPowerTemplate = SpecialAbilityActivateeDummy3\n"
	"    StartsPaused = No\n"
	"    UpdateModuleStartsAttack = Yes\n"
	"  End\n"
	"  Behavior = ActivateModuleSpecialPower ModuleTag_ConvertEnemyDummyMover\n"
	"    SpecialPowerTemplate = SpecialAbilityActivateeDummy\n"
	"    StartAbilityRange = 300.0\n"
	"    TriggerSpecialPower = ModuleTag_ConvertEnemyShadowTrigger OBJECTPOS\n"
	"  End\n"
	"  Behavior = OCLSpecialPower ModuleTag_ConvertEnemyShadowTrigger\n"
	"    SetModelCondition = ModelConditionState:RIDER1\n"
	"    SetModelConditionTime = 2.0\n"
	"    IdleWhenStartingPower = Yes\n"
	"    AvailableAtStart = Yes\n"
	"    StartsPaused = No\n"
	"    SpecialPowerTemplate = SpecialAbilityActivateeDummy3\n"
	"    OCL = OCL_SynthDrogothShadowRad\n"
	"    CreateLocation = CREATE_AT_LOCATION\n"
	"  End\n"
	"End\n"
	"Object SynthDualAbilityHero\n"
	"  KindOf = INFANTRY SELECTABLE HERO\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 50\n"
	"  End\n"
	"  Behavior = SpecialPowerModule ModuleTag_FirstStarter\n"
	"    SpecialPowerTemplate = SpecialAbilityRadagastWordOfPoultry\n"
	"    UpdateModuleStartsAttack = Yes\n"
	"  End\n"
	"  Behavior = ReplaceObjectUpdate ModuleTag_FirstAbility\n"
	"    SpecialPowerTemplate = SpecialAbilityRadagastWordOfPoultry\n"
	"    ReplaceRadius = 50.0\n"
	"    StartAbilityRange = 100.0\n"
	"    UnpackTime = 1000\n"
	"    PreparationTime = 200\n"
	"    PackTime = 400\n"
	"    IgnoreFacingCheck = Yes\n"
	"    ReplaceObject\n"
	"      TargetObjectFilter = NONE +INFANTRY ENEMIES\n"
	"      ReplacementObjectName = Chicken_Replacement\n"
	"    End\n"
	"  End\n"
	"  Behavior = SpecialPowerModule ModuleTag_CaptureStarter\n"
	"    SpecialPowerTemplate = SpecialAbilityCaptureTest\n"
	"    UpdateModuleStartsAttack = Yes\n"
	"  End\n"
	"  Behavior = SpecialAbilityUpdate ModuleTag_CaptureAbility\n"
	"    SpecialPowerTemplate = SpecialAbilityCaptureTest\n"
	"    StartAbilityRange = 20.0\n"
	"    UnpackTime = 1000\n"
	"    PreparationTime = 1000\n"
	"    PackTime = 400\n"
	"    IgnoreFacingCheck = Yes\n"
	"  End\n"
	"  Behavior = SpecialPowerModule ModuleTag_SecondStarter\n"
	"    SpecialPowerTemplate = SpecialAbilitySecondPowerTest\n"
	"    UpdateModuleStartsAttack = Yes\n"
	"  End\n"
	"  Behavior = SpecialAbilityUpdate ModuleTag_SecondAbility\n"
	"    SpecialPowerTemplate = SpecialAbilitySecondPowerTest\n"
	"    StartAbilityRange = 100.0\n"
	"    UnpackTime = 400\n"
	"    PreparationTime = 200\n"
	"    PackTime = 400\n"
	"    IgnoreFacingCheck = Yes\n"
	"  End\n"
	"End\n";

struct Port188Rig : combattest::CombatWorld
{
	SpecialPowerStore powers;
	SpecialPowerStore *savedPowers = TheSpecialPowerStore;
	CommandStore commandStore;
	CommandStore *savedCommands = TheCommandStore;
	FXListStore fxLists;
	FXListStore *savedFx = TheFXListStore;
	AttributeModifierStore modifiers;
	AttributeModifierStore *savedModifiers = TheAttributeModifierStore;
	ObjectCreationListStore oclStore;
	ObjectCreationListStore *savedOcl = TheObjectCreationListStore;
	SpellCommands spellCommands;
	MouseSettings mouse;
	MetaMap metaMap;
	PinholeView view{ { 500.0f, -100.0f, 450.0f }, { 500.0f, 300.0f, 0.0f }, 1024, 768, 0.7f };
	std::unique_ptr<HudInput> input;
	std::unique_ptr<ControlBar> bar;
	int timeMs = 1000;

	explicit Port188Rig(GameEdition edition)
		: combattest::CombatWorld("", 100)
		, oclStore(w.keys)
	{
		logic->setEdition(edition);
		logic->combat().setAutoAcquireEnabled(false);

		TheSpecialPowerStore = &powers;
		TheCommandStore = &commandStore;
		TheFXListStore = &fxLists;
		TheAttributeModifierStore = &modifiers;
		TheObjectCreationListStore = &oclStore;
		commandStore.setThingFactory(&w.things);

		modifiers.registerBlock(w.fx.env.blocks);
		w.fx.env.blocks.registerBlock("FXList", [](INI *ini) { ParseFXListDefinitionGlobal(ini); });
		w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
		w.fx.env.blocks.registerBlock("ObjectCreationList", [](INI *ini) { ObjectCreationListStore::parseObjectCreationListDefinitionGlobal(ini); });
		w.fx.env.blocks.registerBlock("CommandButton", [](INI *ini) { CommandStore::parseCommandButtonDefinitionGlobal(ini); });
		w.fx.env.blocks.registerBlock("CommandSet", [](INI *ini) { CommandStore::parseCommandSetDefinitionGlobal(ini); });

		SpecialAbilityModules::registerAll(w.modules);
		SpecialPowerModules::registerAll(w.modules);
		SpecialPowerModules::installScienceHooks(*logic);

		WeaponSetToggle::registerHandlers(dispatcher);
		HordeCommands::registerHandlers(dispatcher);
		WeaponCommands::registerHandlers(dispatcher);
		HeroAbilityModules::registerHandlers(dispatcher);
		spellCommands.registerHandlers(dispatcher);

		const std::string err = w.load(kPort188Ini, INI_LOAD_OVERWRITE, "port188.ini");
		REQUIRE_MESSAGE(err.empty(), err);

		Player *alice = players.findPlayerWithName("Alice");
		REQUIRE(alice != nullptr);
		players.setLocalPlayer(alice);

		input = std::make_unique<HudInput>(*logic, ai.get(), view, commands, mouse, metaMap);
		input->context().commands = &commandStore;
		bar = std::make_unique<ControlBar>(input->context());
	}

	~Port188Rig()
	{
		bar.reset();
		input.reset();
		logic->reset();
		TheObjectCreationListStore = savedOcl;
		TheAttributeModifierStore = savedModifiers;
		TheFXListStore = savedFx;
		TheCommandStore = savedCommands;
		TheSpecialPowerStore = savedPowers;
	}

	void select(Object *obj)
	{
		input->ui().deselectAll(true);
		GameMessage sel(MSG_CREATE_SELECTED_GROUP, playerIndex("Alice"));
		sel.appendBooleanArgument(true);
		input->ui().selectObject(obj->getID());
		sel.appendObjectIDArgument(obj->getID());
		input->submit(sel);
		input->flush();
		frames(1);
		bar->update();
	}

	void clickGround(const Coord3D &worldTarget)
	{
		view.lookAt(worldTarget);
		ICoord2D px{ 0, 0 };
		REQUIRE(view.worldToScreen(worldTarget, px));
		input->mouseMove(px.x, px.y, 0);
		input->mouseButton(HudInput::Button::Left, true, px.x, px.y, 0, timeMs += 40);
		input->mouseButton(HudInput::Button::Left, false, px.x, px.y, 0, timeMs += 40);
		input->update();
	}

	void stepAndRefresh(int n = 1)
	{
		input->flush();
		frames(n);
		bar->update();
	}

	const ControlBarButton *buttonNamed(const std::string &name) const
	{
		for (const ControlBarButton &b : bar->palantirButtons())
		{
			if (b.button && b.button->m_name == name)
			{
				return &b;
			}
		}
		return nullptr;
	}

	std::vector<Object *> findAllNamed(const std::string &templateName) const
	{
		std::vector<Object *> out;
		for (Object *o = logic->getFirstObject(); o != nullptr; o = o->getNextObject())
		{
			if (!o->isEffectivelyDead() && o->getTemplate() && o->getTemplate()->getName() == templateName)
			{
				out.push_back(o);
			}
		}
		return out;
	}

	template <typename T>
	T *moduleByTag(Object *obj, const char *tag)
	{
		return dynamic_cast<T *>(obj->findModuleByTag(w.keys.nameToKey(tag)));
	}
};

SpecialPowerModuleInterface *findPowerModule(Object &obj, const std::string &powerName)
{
	const SpecialPowerTemplate *tmpl = TheSpecialPowerStore ? TheSpecialPowerStore->findSpecialPowerTemplate(powerName) : nullptr;
	REQUIRE(tmpl != nullptr);
	SpecialPowerModuleInterface *sp = SpecialPowerModules::findModule(obj, tmpl);
	REQUIRE(sp != nullptr);
	return sp;
}
} // namespace

TEST_CASE("PORT-188: live HUD click on Eagle Radagast Word of Poultry in range replaces enemies and starts 360s recharge (RW 0x853076 .. 0x85309A)")
{
	const int kMcUser50 = CombatNames::modelCondition("USER_50");
	REQUIRE(kMcUser50 == CombatNames::modelCondition("USER_50"));

	for (GameEdition edition : { GameEdition::RotWK201, GameEdition::BFME2_106 })
	{
		CAPTURE(gameEditionName(edition));
		Port188Rig rig(edition);

		Object *eagle = rig.unit("SynthRadagastAdler", 'A', 300.0f, 300.0f);
		Object *enemy1 = rig.unit("SynthEnemyInfantry", 'B', 420.0f, 300.0f);
		Object *enemy2 = rig.unit("SynthEnemyInfantry", 'B', 440.0f, 315.0f);
		Object *outsideEnemy = rig.unit("SynthEnemyInfantry", 'B', 600.0f, 300.0f);
		Object *ally = rig.unit("SynthEnemyInfantry", 'A', 420.0f, 315.0f);
		REQUIRE(eagle != nullptr);
		REQUIRE(enemy1 != nullptr);
		REQUIRE(enemy2 != nullptr);
		REQUIRE(outsideEnemy != nullptr);
		REQUIRE(ally != nullptr);

		const ObjectID enemy1Id = enemy1->getID();
		const ObjectID enemy2Id = enemy2->getID();
		const ObjectID outsideEnemyId = outsideEnemy->getID();
		const ObjectID allyId = ally->getID();

		auto *poultryUpdate = rig.moduleByTag<SpecialAbilityUpdate>(eagle, "ModuleTag_WordOfPoultryUpdate_Level2");
		auto *dummyMover = rig.moduleByTag<SpecialAbilityUpdate>(eagle, "ModuleTag_ConvertEnemyDummyMover");
		SpecialPowerModuleInterface *poultryPower = findPowerModule(*eagle, "SpecialAbilityRadagastWordOfPoultry");
		REQUIRE(poultryUpdate != nullptr);
		REQUIRE(dummyMover != nullptr);
		CHECK(poultryPower->isReady());

		rig.select(eagle);
		const ControlBarButton *btn = rig.buttonNamed("Command_SpecialAbilityRadagastConvertEnemyAdler");
		REQUIRE(btn != nullptr);
		CHECK(btn->state == ButtonState::Enabled);

		// Click the Palantir button, then left-click bare ground at (430, 307, 0) between enemy1 and enemy2 (distance ~130 <= StartAbilityRange 300).
		// CommandTranslator emits MSG_DO_SPECIAL_POWER_AT_LOCATION for SpecialAbilityRadagastWordOfPoultry AND
		// MSG_DO_SPECIAL_POWER_AT_LOCATION for CommandTrigger = Command_SpecialAbilityConvertEnemyTracker in the same frame.
		REQUIRE(rig.bar->pressButton(btn->slot, true));
		rig.clickGround(Coord3D{ 430.0f, 307.0f, 0.0f });

		// Drain the command list and run one frame: neither module was in use during command dispatch (m_withinRange == false
		// inside initiateIntentToDoSpecialPower), so ModuleTag_ConvertEnemyDummyMover MUST NOT abort WordOfPoultryUpdate_Level2.
		rig.frames(1);
		CHECK(poultryUpdate->isActive());

		// Advance until WordOfPoultry triggers (facing + UnpackTime = 800 ms = 4 frames + 1 frame preparation) and starts its 360 s (1800-frame) recharge.
		unsigned triggerFrame = 0;
		for (int i = 0; i < 30; ++i)
		{
			rig.frames(1);
			if (poultryUpdate->abilitiesTriggered() == 1u)
			{
				triggerFrame = rig.logic->getFrame();
				break;
			}
		}
		CHECK(poultryUpdate->abilitiesTriggered() == 1u);
		CHECK_FALSE(poultryPower->isReady());
		CHECK_FALSE(poultryPower->isReadyForDisplay());
		CHECK(poultryPower->getReadyFrame() == triggerFrame + 1800u);

		// Both enemies inside ReplaceRadius = 140 are replaced by neutral Chicken_Replacement; distant enemy and ally remain untouched.
		CHECK(rig.byId(enemy1Id) == nullptr);
		CHECK(rig.byId(enemy2Id) == nullptr);
		CHECK(rig.byId(outsideEnemyId) == outsideEnemy);
		CHECK(rig.byId(allyId) == ally);
		const std::vector<Object *> chickens = rig.findAllNamed("Chicken_Replacement");
		REQUIRE(chickens.size() == 2u);
		CHECK(chickens[0]->getControllingPlayer() == rig.players.getNeutralPlayer());
		CHECK(chickens[1]->getControllingPlayer() == rig.players.getNeutralPlayer());

		// Give the companion OCL_SynthDrogothShadowRad -> SynthRadagastNoUseAbility aura a pulse: while USER_50 is active (~58 s),
		// DisableOnModelCondition = USER_50 marks the button Restricted while the 360 s power timer continues recharging underneath.
		rig.stepAndRefresh(15);
		CHECK(eagle->testModelCondition(kMcUser50));
		btn = rig.buttonNamed("Command_SpecialAbilityRadagastConvertEnemyAdler");
		REQUIRE(btn != nullptr);
		CHECK(btn->state == ButtonState::Restricted);
		CHECK_FALSE(poultryPower->isReady());

		// Once SynthRadagastNoUseAbility (Lifetime 55 s = 275 frames) + SynthRadagastConvertFixer (Duration 3 s = 15 frames) expire,
		// USER_50 clears and the button shows NotReady with the active 360 s recharge clock (cb.timer == getPercentReadyForDisplay()).
		rig.stepAndRefresh(300);
		CHECK_FALSE(eagle->testModelCondition(kMcUser50));
		CHECK_FALSE(poultryPower->isReady());
		CHECK_FALSE(poultryPower->isReadyForDisplay());
		btn = rig.buttonNamed("Command_SpecialAbilityRadagastConvertEnemyAdler");
		REQUIRE(btn != nullptr);
		CHECK(btn->state == ButtonState::NotReady);
		CHECK(btn->timer == poultryPower->getPercentReadyForDisplay());

		// After the remaining frames of ReloadTime = 360000 ms (1800 frames total), the ability is Ready / Enabled again.
		rig.stepAndRefresh(1500);
		CHECK(poultryPower->isReady());
		CHECK(poultryPower->isReadyForDisplay());
		btn = rig.buttonNamed("Command_SpecialAbilityRadagastConvertEnemyAdler");
		REQUIRE(btn != nullptr);
		CHECK(btn->state == ButtonState::Enabled);
	}
}

TEST_CASE("PORT-188: live HUD click on Eagle Radagast Word of Poultry out of range flies into StartAbilityRange, replaces enemy, and is deterministic")
{
	std::uint32_t hashes[2] = {};
	for (int run = 0; run < 2; ++run)
	{
		Port188Rig rig(GameEdition::RotWK201);
		Object *eagle = rig.unit("SynthRadagastAdler", 'A', 200.0f, 300.0f);
		Object *farEnemy = rig.unit("SynthEnemyInfantry", 'B', 750.0f, 300.0f);
		REQUIRE(eagle != nullptr);
		REQUIRE(farEnemy != nullptr);
		const ObjectID farEnemyId = farEnemy->getID();

		auto *poultryUpdate = rig.moduleByTag<SpecialAbilityUpdate>(eagle, "ModuleTag_WordOfPoultryUpdate_Level2");
		SpecialPowerModuleInterface *poultryPower = findPowerModule(*eagle, "SpecialAbilityRadagastWordOfPoultry");
		REQUIRE(poultryUpdate != nullptr);

		rig.select(eagle);
		const ControlBarButton *btn = rig.buttonNamed("Command_SpecialAbilityRadagastConvertEnemyAdler");
		REQUIRE(btn != nullptr);
		REQUIRE(rig.bar->pressButton(btn->slot, true));
		// Distance 550 exceeds StartAbilityRange = 300: GiantBirdAIUpdate must fly Eagle Radagast into range first.
		rig.clickGround(Coord3D{ 750.0f, 300.0f, 0.0f });

		rig.frames(1);
		CHECK(poultryUpdate->isActive());
		CHECK_FALSE(poultryUpdate->isPowerCurrentlyInUse());

		unsigned triggerFrame = 0;
		for (int i = 0; i < 120; ++i)
		{
			rig.frames(1);
			if (poultryUpdate->abilitiesTriggered() == 1u)
			{
				triggerFrame = rig.logic->getFrame();
				break;
			}
		}
		CHECK(poultryUpdate->abilitiesTriggered() == 1u);
		CHECK_FALSE(poultryPower->isReady());
		CHECK(poultryPower->getReadyFrame() == triggerFrame + 1800u);
		CHECK(rig.byId(farEnemyId) == nullptr);
		const std::vector<Object *> chickens = rig.findAllNamed("Chicken_Replacement");
		REQUIRE(chickens.size() == 1u);
		CHECK(chickens[0]->getControllingPlayer() == rig.players.getNeutralPlayer());

		hashes[run] = rig.hash();
	}
	CHECK(hashes[0] == hashes[1]);
}

TEST_CASE("PORT-188: SpecialAbilityUpdate::initiateIntentToDoSpecialPower aborts sibling iff isPowerCurrentlyInUse() is true (RW 0x853076 .. 0x85309A, RW 0x851C92)")
{
	for (GameEdition edition : { GameEdition::RotWK201, GameEdition::BFME2_106 })
	{
		CAPTURE(gameEditionName(edition));
		Port188Rig rig(edition);

		Object *hero = rig.unit("SynthDualAbilityHero", 'A', 200.0f, 300.0f);
		Object *enemy = rig.unit("SynthEnemyInfantry", 'B', 500.0f, 300.0f);
		Object *building = rig.unit("SynthCapturableBuilding", 'B', 500.0f, 400.0f);
		REQUIRE(hero != nullptr);
		REQUIRE(enemy != nullptr);
		REQUIRE(building != nullptr);

		auto *first = rig.moduleByTag<SpecialAbilityUpdate>(hero, "ModuleTag_FirstAbility");
		auto *capture = rig.moduleByTag<SpecialAbilityUpdate>(hero, "ModuleTag_CaptureAbility");
		auto *second = rig.moduleByTag<SpecialAbilityUpdate>(hero, "ModuleTag_SecondAbility");
		REQUIRE(first != nullptr);
		REQUIRE(capture != nullptr);
		REQUIRE(second != nullptr);

		const Coord3D farTarget{ 500.0f, 300.0f, 0.0f };
		const Coord3D nearTarget{ 240.0f, 300.0f, 0.0f };

		// 1) Approach phase (m_packingState == PACKED, m_withinRange == false -> isPowerCurrentlyInUse() == false):
		//    initiating a second SpecialAbilityUpdate must NOT abort the approaching first ability.
		first->initiateIntentToDoSpecialPower(first->abilityData()->m_specialPowerTemplate, nullptr, &farTarget, 0, 0);
		rig.frames(2);
		CHECK(first->isActive());
		CHECK_FALSE(first->isPowerCurrentlyInUse());
		second->initiateIntentToDoSpecialPower(second->abilityData()->m_specialPowerTemplate, nullptr, &farTarget, 0, 0);
		CHECK(first->isActive());
		CHECK(second->isActive());
		first->finishAbility(false, true);
		second->finishAbility(false, true);

		// 2) In-range unpacking phase (m_withinRange == true, m_packingState == UNPACKING -> isPowerCurrentlyInUse() == true):
		//    initiating a second SpecialAbilityUpdate MUST abort the in-use first ability via finishAbility(false, true).
		first->initiateIntentToDoSpecialPower(first->abilityData()->m_specialPowerTemplate, nullptr, &nearTarget, 0, 0);
		rig.frames(1);
		CHECK(first->isActive());
		CHECK(first->isPowerCurrentlyInUse());
		second->initiateIntentToDoSpecialPower(second->abilityData()->m_specialPowerTemplate, nullptr, &nearTarget, 0, 0);
		CHECK_FALSE(first->isActive());
		CHECK(second->isActive());
		second->finishAbility(false, true);

		// 3) SPECIAL_INFANTRY_CAPTURE_BUILDING (0x1D, RW 0x851CAB): isPowerCurrentlyInUse(nullptr) returns true as soon as
		//    m_packingState != STATE_NONE even while approaching out of range, so a second ability aborts it immediately.
		capture->initiateIntentToDoSpecialPower(capture->abilityData()->m_specialPowerTemplate, building, nullptr, 0, 0);
		CHECK(capture->isActive());
		CHECK(capture->isPowerCurrentlyInUse());
		second->initiateIntentToDoSpecialPower(second->abilityData()->m_specialPowerTemplate, nullptr, &nearTarget, 0, 0);
		CHECK_FALSE(capture->isActive());
		CHECK(second->isActive());
	}
}
