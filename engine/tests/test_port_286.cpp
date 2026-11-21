// OpenBFME. GPL-3.0. Synthetic RotWK INI; no retail data or renderer required.
#include "doctest.h"

#include "CombatTestUtil.h"

#include "Common/ModelState.h"
#include "Common/SpecialPower.h"
#include "GameClient/ControlBar.h"
#include "GameClient/MessageStream/MessageStream.h"
#include "GameLogic/AttributeModifiers.h"
#include "GameLogic/Combat/ObjectWeapons.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Object/AttributeModifierPool.h"
#include "GameLogic/Object/Contain/HordeContainRuntime.h"
#include "GameLogic/Object/Object.h"

#include <string>

namespace
{
const char kBaseIni[] = R"ini(
SpecialPower P286ReadyPower
  Enum = SPECIAL_WOUND_ARROW
  ReloadTime = 4000
End
Object P286Unit
  KindOf = SELECTABLE INFANTRY
  Geometry = CYLINDER
  GeometryMajorRadius = 8
  GeometryMinorRadius = 8
  GeometryHeight = 15
  Body = ActiveBody BodyTag
    MaxHealth = 300
  End
  Behavior = AttributeModifierPoolUpdate PoolTag
  End
  Behavior = SpecialPowerModule ReadyTag
    SpecialPowerTemplate = P286ReadyPower
  End
End
)ini";

template<class T>
struct StoreScope
{
	T *&slot;
	T *previous;
	StoreScope(T *&global, T &store) : slot(global), previous(global) { slot = &store; }
	~StoreScope() { slot = previous; }
};

// The selection and bar use the production ControlBar::evaluate path, without drawables or test-only access to it.
class TestView : public TacticalView
{
public:
	ICoord2D size() const override { return { 1024, 768 }; }
	bool worldToScreen(const Coord3D &, ICoord2D &) const override { return false; }
	bool screenToRay(const ICoord2D &, Coord3D &, Coord3D &) const override { return false; }
	Coord3D position() const override { return {}; }
	void lookAt(const Coord3D &) override {}
};

struct GateWorld : combattest::CombatWorld
{
	CommandStore commandStore;
	AttributeModifierStore modifiers;
	SpecialPowerStore powers;
	StoreScope<CommandStore> commandsScope{ TheCommandStore, commandStore };
	StoreScope<AttributeModifierStore> modifiersScope{ TheAttributeModifierStore, modifiers };
	StoreScope<SpecialPowerStore> powersScope{ TheSpecialPowerStore, powers };
	TestView view;
	InGameUI ui;
	MessageStream stream;
	HudContext ctx{ *logic, ai.get(), view, ui, stream, &commandStore, {} };
	ControlBar bar{ ctx };

	explicit GateWorld(const char *ini) : CombatWorld("", 70)
	{
		commandStore.setThingFactory(&w.things);
		modifiers.registerBlock(w.fx.env.blocks);
		w.fx.env.blocks.registerBlock("SpecialPower", [](INI *i) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(i); });
		w.fx.env.blocks.registerBlock("CommandButton", [](INI *i) { CommandStore::parseCommandButtonDefinitionGlobal(i); });
		w.fx.env.blocks.registerBlock("CommandSet", [](INI *i) { CommandStore::parseCommandSetDefinitionGlobal(i); });
		const std::string err = w.load(std::string(kBaseIni) + ini, INI_LOAD_OVERWRITE, "port286.ini");
		REQUIRE_MESSAGE(err.empty(), err);
		players.setLocalPlayer(playerOf('A'));
		combat().setAutoAcquireEnabled(false);
	}
	~GateWorld() { logic->reset(); }

	ButtonState state(Object &o, const char *name)
	{
		ui.mirrorSelection({ o.getID() });
		bar.update();
		for (const ControlBarButton &b : bar.palantirButtons())
		{
			if (b.button && b.button->m_name == name)
			{
				return b.state;
			}
		}
		return ButtonState::Hidden;
	}
	void gate(Object &o, const char *name, const char *condition, bool set, bool enabled)
	{
		CAPTURE(std::string(name));
		CHECK(o.testModelCondition(ModelCondition::indexOf(condition)) == set);
		CHECK(state(o, name) == (enabled ? ButtonState::Enabled : ButtonState::Restricted));
	}
	void cast(Object &o, const char *name)
	{
		CAPTURE(std::string(name));
		const CommandButton *button = commandStore.findCommandButton(name);
		REQUIRE(button);
		REQUIRE(state(o, name) == ButtonState::Enabled);
		REQUIRE(SpecialPowerModules::doSpecialPower(o, powers.findSpecialPowerTemplate(button->m_specialPowerName), button->m_options, false));
	}
};

const char kDelayedIni[] = R"ini(
Upgrade Upgrade_P286Granted
  Type = OBJECT
End
ModifierList P286Delayed
  Category = BUFF
  Duration = 1800
  Upgrade = Upgrade_P286Granted Delay:650
End
ModifierList P286Immediate
  Category = BUFF
  Duration = 1800
  Upgrade = Upgrade_P286Granted
End
SpecialPower P286CastBuff
  Enum = SPECIAL_ATHELAS
  ReloadTime = 5000
End
CommandButton P286Cast
  Command = SPECIAL_POWER
  SpecialPower = P286CastBuff
  InPalantir = Yes
End
CommandButton P286AfterGrant
  Command = SPECIAL_POWER
  SpecialPower = P286ReadyPower
  EnableOnModelCondition = USER_2
  InPalantir = Yes
End
CommandSet P286DelayedSet
  1 = P286Cast
  2 = P286AfterGrant
End
ChildObject P286Caster P286Unit
  CommandSet = P286DelayedSet
  Behavior = SpecialPowerModule CastTag
    SpecialPowerTemplate = P286CastBuff
    AttributeModifier = P286Delayed
    AttributeModifierRange = 1
    AttributeModifierAffectsSelf = Yes
    AttributeModifierAffects = NONE +P286Caster
  End
  Behavior = ModelConditionUpgrade GrantTag
    TriggeredBy = Upgrade_P286Granted
    AddConditionFlags = USER_2
  End
End
ChildObject P286ImmediateCaster P286Caster
  ReplaceModule CastTag
    Behavior = SpecialPowerModule InstantCastTag
      SpecialPowerTemplate = P286CastBuff
      AttributeModifier = P286Immediate
      AttributeModifierAffectsSelf = Yes
    End
  End
End
ModifierList P286Lesson
  Category = SPELL
  Duration = 3000
  Upgrade = Upgrade_P286Granted Delay:100
End
Weapon P286LessonWarhead
  AttackRange = 200
  RadiusDamageAffects = ALLIES
  AttributeModifierNugget
    AttributeModifier = P286Lesson
    Radius = 1
    SpecialObjectFilter = NONE +P286Learner ALLIES
  End
End
CommandSet P286LearnerSet
  1 = P286AfterGrant
End
ChildObject P286Learner P286Unit
  CommandSet = P286LearnerSet
  Behavior = ModelConditionUpgrade GrantTag
    TriggeredBy = Upgrade_P286Granted
    AddConditionFlags = USER_2
  End
End
)ini";

const char kLockIni[] = R"ini(
Weapon P286Blade
  AttackRange = 12
  MeleeWeapon = Yes
  DamageNugget
    Damage = 10
    DamageType = SLASH
    DeathType = NORMAL
  End
End
SpecialPower P286Secondary
  Enum = SPECIAL_KNIFE_FIGHTER
  ReloadTime = 6000
End
SpecialPower P286Primary
  Enum = SPECIAL_HERO_MODE
  ReloadTime = 6000
End
CommandButton P286LockSecondary
  Command = SPECIAL_POWER
  SpecialPower = P286Secondary
  InPalantir = Yes
End
CommandButton P286LockPrimary
  Command = SPECIAL_POWER
  SpecialPower = P286Primary
  InPalantir = Yes
End
CommandButton P286WhenLocked
  Command = SPECIAL_POWER
  SpecialPower = P286ReadyPower
  EnableOnModelCondition = WEAPONLOCK_SECONDARY
  InPalantir = Yes
End
CommandSet P286LockSet
  1 = P286LockSecondary
  2 = P286LockPrimary
  3 = P286WhenLocked
End
ChildObject P286WeaponUser P286Unit
  KindOf = SELECTABLE INFANTRY CAN_ATTACK
  CommandSet = P286LockSet
  WeaponSet
    Conditions = None
    Weapon = PRIMARY P286Blade
    Weapon = SECONDARY P286Blade
  End
  WeaponSet
    Conditions = WEAPONSET_TOGGLE_1
    Weapon = PRIMARY P286Blade
  End
  Behavior = WeaponModeSpecialPowerUpdate SecondaryModeTag
    SpecialPowerTemplate = P286Secondary
    LockWeaponSlot = SECONDARY
    Duration = 1200
  End
  Behavior = WeaponModeSpecialPowerUpdate PrimaryModeTag
    SpecialPowerTemplate = P286Primary
    LockWeaponSlot = PRIMARY
    Duration = 1200
  End
End
)ini";

const char kAreaIni[] = R"ini(
Upgrade Upgrade_P286Leader
  Type = OBJECT
End
Upgrade Upgrade_P286Stable
  Type = OBJECT
End
Upgrade Upgrade_P286Beast
  Type = OBJECT
End
Upgrade Upgrade_P286Tower
  Type = OBJECT
End
Upgrade Upgrade_P286Reinforce
  Type = OBJECT
End
ModifierList P286FireEnabled
  Category = BUFF
  Duration = 0
  ModelCondition = HERO
End
ModifierList P286WeaponMark
  Category = WEAPON
  Duration = 1400
  ModelCondition = USER_1
End
ModifierList P286PassiveMark
  Category = BUFF
  Duration = 2600
  ModelCondition = USER_3
End
ModifierList P286AuraMark
  Category = BUFF
  Duration = 2600
  ModelCondition = USER_9
End
ModifierList P286TowerMark
  Category = BUFF
  Duration = 2400
  ModelCondition = USER_8
End
Weapon P286Pulse
  AttackRange = 80
  RadiusDamageAffects = ALLIES
  AttributeModifierNugget
    AttributeModifier = P286WeaponMark
    Radius = 130
    SpecialObjectFilter = NONE +URUK ALLIES
  End
End
CommandButton P286NearLeader
  Command = SPECIAL_POWER
  SpecialPower = P286ReadyPower
  EnableOnModelCondition = USER_1
  InPalantir = Yes
End
CommandButton P286NearStable
  Command = SPECIAL_POWER
  SpecialPower = P286ReadyPower
  EnableOnModelCondition = USER_9
  InPalantir = Yes
End
CommandButton P286NearPassive
  Command = SPECIAL_POWER
  SpecialPower = P286ReadyPower
  EnableOnModelCondition = USER_3
  InPalantir = Yes
End
CommandButton P286BuyReinforce
  Command = OBJECT_UPGRADE
  Upgrade = Upgrade_P286Reinforce
  DisableOnModelCondition = USER_8
  InPalantir = Yes
End
CommandButton P286BuyBeast
  Command = OBJECT_UPGRADE
  Upgrade = Upgrade_P286Beast
  EnableOnModelCondition = USER_9
  InPalantir = Yes
End
CommandSet P286HordeSet
  1 = P286NearLeader
  2 = P286NearStable
End
CommandSet P286PassiveSet
  1 = P286NearPassive
End
CommandSet P286TowerSet
  1 = P286BuyReinforce
End
CommandSet P286BeastSet
  1 = P286BuyBeast
End
ChildObject P286Leader P286Unit
  Behavior = AttributeModifierUpgrade ActivateTag
    TriggeredBy = Upgrade_P286Leader
    AttributeModifier = P286FireEnabled
  End
  Behavior = FireWeaponUpdate FireTag
    HeroModeTrigger = Yes
    FireWeaponNugget
      WeaponName = P286Pulse
      FireDelay = 0
      OneShot = Yes
    End
  End
End
ChildObject P286Member P286Unit
  KindOf = SELECTABLE INFANTRY HORDE_MEMBER
  Behavior = AIUpdateInterface AITag
  End
  LocomotorSet
    Locomotor = WalkerLoco
    Condition = SET_NORMAL
    Speed = 32
  End
End
ChildObject P286Horde P286Unit
  KindOf = HORDE MELEE_HORDE SELECTABLE LARGE_RECTANGLE_PATHFIND URUK
  CommandSet = P286HordeSet
  Geometry = BOX
  GeometryMajorRadius = 20
  GeometryMinorRadius = 20
  GeometryHeight = 15
  Behavior = HordeAIUpdate MoveTag
  End
  Behavior = HordeContain FormationTag
    Slots = 1
    InitialPayload = P286Member 1
    RankInfo = RankNumber:1 UnitType:P286Member Position:X:0 Y:0
    AlternateFormation = P286Wedge
    ThisFormationIsTheMainFormation = Yes
  End
  LocomotorSet
    Locomotor = HordeLoco
    Condition = SET_NORMAL
    Speed = 32
  End
End
ChildObject P286Wedge P286Horde
  ReplaceModule FormationTag
    Behavior = HordeContain WedgeTag
      Slots = 1
      InitialPayload = P286Member 1
      RankInfo = RankNumber:1 UnitType:P286Member Position:X:18 Y:0
      AlternateFormation = P286Horde
      ThisFormationIsTheMainFormation = No
    End
  End
End
ChildObject P286Stable P286Unit
  KindOf = STRUCTURE SELECTABLE IMMOBILE
  Behavior = AttributeModifierAuraUpdate AuraTag
    StartsActive = No
    TriggeredBy = Upgrade_P286Stable
    BonusName = P286AuraMark
    RefreshDelay = 600
    Range = 130
    ObjectFilter = NONE +P286Horde +P286Beast +P286Member ALLIES
  End
End
ChildObject P286Beast P286Unit
  CommandSet = P286BeastSet
  Behavior = ProductionUpdate QueueTag
  End
  Behavior = ModelConditionUpgrade PromoteTag
    TriggeredBy = Upgrade_P286Beast
    AddConditionFlags = USER_10
  End
End
ChildObject P286Receiver P286Unit
  CommandSet = P286PassiveSet
End
ChildObject P286Shrine P286Unit
  KindOf = STRUCTURE SELECTABLE IMMOBILE
  Behavior = PassiveAreaEffectBehavior PassiveTag
    EffectRadius = 95
    PingDelay = 600
    AllowFilter = NONE +P286Receiver
    ModifierName = P286PassiveMark
  End
End
ChildObject P286Tower P286Unit
  KindOf = STRUCTURE SELECTABLE IMMOBILE
  CommandSet = P286TowerSet
  Behavior = ProductionUpdate QueueTag
  End
  Behavior = AttributeModifierUpgrade BlockWhileNewTag
    TriggeredBy = Upgrade_P286Tower
    AttributeModifier = P286TowerMark
  End
  Behavior = ModelConditionUpgrade ReinforceTag
    TriggeredBy = Upgrade_P286Reinforce
    AddConditionFlags = USER_10
  End
End
)ini";
} // namespace

TEST_CASE("PORT-286: self cast grants the model-condition upgrade only on the pool's delayed wake (RW 0x8050D3)")
{
	GateWorld f(kDelayedIni);
	Object *caster = f.unit("P286Caster", 'A', 300, 300);
	Object *idle = f.unit("P286Caster", 'A', 340, 300);
	Object *instant = f.unit("P286ImmediateCaster", 'A', 400, 300);
	f.frames(2);
	f.gate(*caster, "P286AfterGrant", "USER_2", false, false);
	const UpgradeTemplate *grant = f.upgradeCenter.findUpgrade("Upgrade_P286Granted");
	REQUIRE(grant);
	REQUIRE(f.modifiers.find("P286Delayed")->m_upgrade);
	CHECK(f.modifiers.find("P286Delayed")->m_upgrade->delayFrames == 4);
	f.cast(*caster, "P286Cast");
	CHECK_FALSE(caster->hasUpgrade(grant));
	f.gate(*caster, "P286AfterGrant", "USER_2", false, false);

	// RW 0x805A8E schedules this otherwise empty pool by the list duration, not by the smaller grant timestamp.
	f.frames(8);
	f.gate(*caster, "P286AfterGrant", "USER_2", false, false);
	f.frames(1);
	CHECK(caster->hasUpgrade(grant));
	f.gate(*caster, "P286AfterGrant", "USER_2", true, true);
	f.gate(*idle, "P286AfterGrant", "USER_2", false, false);
	f.cast(*instant, "P286Cast");
	CHECK(instant->hasUpgrade(grant));
	f.gate(*instant, "P286AfterGrant", "USER_2", true, true);
}

TEST_CASE("PORT-286: an allied warhead grants the upgrade at the idle pool's first wake, not on impact (RW 0x805A8E)")
{
	GateWorld f(kDelayedIni);
	Object *source = f.unit("P286Unit", 'A', 300, 300);
	Object *target = f.unit("P286Learner", 'A', 380, 300);
	Object *bystander = f.unit("P286Learner", 'A', 440, 300);
	f.frames(2);
	const auto *pool = dynamic_cast<const AttributeModifierPool *>(target->attributeModifierPoolModule());
	REQUIRE(pool);
	REQUIRE(pool->entryCount() == 0);
	const auto *grant = f.upgradeCenter.findUpgrade("Upgrade_P286Granted");
	const auto *lesson = f.modifiers.find("P286Lesson");
	const auto *warhead = f.stores.weapons().findWeaponTemplate("P286LessonWarhead");
	REQUIRE(grant);
	REQUIRE(lesson);
	REQUIRE(lesson->m_upgrade);
	CHECK(lesson->m_upgrade->delayFrames == 1);
	REQUIRE(warhead);

	// Start at impact, exercising the same AttributeModifierNugget path as the scholar's warhead (RW 0x90EAF9).
	ObjectWeapons::createAndFireTempWeaponAt(warhead, source, *target);
	REQUIRE(pool->hasList("P286Lesson"));
	CHECK_FALSE(target->hasUpgrade(grant));
	f.frames(10); // the old LOCAL-75 assertion was too early even if the shot had hit immediately
	CHECK_FALSE(target->hasUpgrade(grant));
	f.frames(4);
	CHECK_FALSE(target->hasUpgrade(grant));
	f.gate(*target, "P286AfterGrant", "USER_2", false, false);
	f.frames(1); // Delay:100 is due at +1; the previously idle pool first processes it at +15 (Duration = 3000).
	CHECK(target->hasUpgrade(grant));
	CHECK_FALSE(pool->hasList("P286Lesson"));
	f.gate(*target, "P286AfterGrant", "USER_2", true, true);
	CHECK_FALSE(bystander->hasUpgrade(grant));
	f.gate(*bystander, "P286AfterGrant", "USER_2", false, false);
}

TEST_CASE("PORT-286: secondary weapon mode opens the button, a replacement lock removes it (RW 0x6C97F9)")
{
	GateWorld f(kLockIni);
	Object *unit = f.unit("P286WeaponUser", 'A', 300, 300);
	Object *idle = f.unit("P286WeaponUser", 'A', 365, 300);
	ObjectWeapons *weapons = unit->getWeapons();
	REQUIRE(weapons);
	REQUIRE(weapons->weaponInSlot(PRIMARY_WEAPON));
	REQUIRE(weapons->weaponInSlot(SECONDARY_WEAPON));
	f.frames(2);
	CHECK(f.state(*unit, "P286LockSecondary") == ButtonState::NotReady);
	f.frames(28); // WeaponModeSpecialPowerUpdate starts its full 6000 ms recharge at creation (RW 0x991694).
	f.gate(*unit, "P286WhenLocked", "WEAPONLOCK_SECONDARY", false, false);
	f.cast(*unit, "P286LockSecondary");
	f.gate(*unit, "P286WhenLocked", "WEAPONLOCK_SECONDARY", true, true);
	f.gate(*idle, "P286WhenLocked", "WEAPONLOCK_SECONDARY", false, false);

	const int primary = ModelCondition::indexOf("WEAPONLOCK_PRIMARY");
	CHECK(weapons->setWeaponLock(PRIMARY_WEAPON, LOCKED_TEMPORARILY));
	CHECK_FALSE(unit->testModelCondition(primary)); // accepted, but cannot override permanent
	CHECK_FALSE(weapons->setWeaponLock(TERTIARY_WEAPON, LOCKED_PERMANENTLY)); // empty slot
	f.gate(*unit, "P286WhenLocked", "WEAPONLOCK_SECONDARY", true, true);
	f.cast(*unit, "P286LockPrimary");
	CHECK(unit->testModelCondition(primary));
	f.gate(*unit, "P286WhenLocked", "WEAPONLOCK_SECONDARY", false, false);

	f.frames(8);
	CHECK_FALSE(unit->testModelCondition(primary));
	f.gate(*unit, "P286WhenLocked", "WEAPONLOCK_SECONDARY", false, false);
	f.cast(*idle, "P286LockSecondary");
	f.gate(*idle, "P286WhenLocked", "WEAPONLOCK_SECONDARY", true, true);
	f.frames(8);
	f.gate(*idle, "P286WhenLocked", "WEAPONLOCK_SECONDARY", false, false);
}

TEST_CASE("PORT-286: changing to a non-shared weapon set releases the secondary lock (RW 0x6C99E2)")
{
	GateWorld f(kLockIni);
	Object *unit = f.unit("P286WeaponUser", 'A', 300, 300);
	f.frames(30); // let the mode's initial recharge finish
	f.cast(*unit, "P286LockSecondary");
	f.gate(*unit, "P286WhenLocked", "WEAPONLOCK_SECONDARY", true, true);
	unit->setWeaponSetFlag(24, true); // WEAPONSET_TOGGLE_1: switches the set, not the model lock bit
	CHECK_FALSE(unit->getWeapons()->weaponInSlot(SECONDARY_WEAPON));
	f.gate(*unit, "P286WhenLocked", "WEAPONLOCK_SECONDARY", false, false);
}

TEST_CASE("PORT-286: a leader's upgrade starts the modifier weapon, which opens the horde's button (RW 0x88F554)")
{
	GateWorld f(kAreaIni);
	Object *leader = f.unit("P286Leader", 'A', 400, 400);
	Object *near = f.unit("P286Horde", 'A', 470, 400);
	Object *enemy = f.unit("P286Horde", 'B', 330, 400);
	Object *far = f.unit("P286Horde", 'A', 720, 400);
	f.frames(3);
	f.gate(*near, "P286NearLeader", "USER_1", false, false);
	leader->giveUpgrade("Upgrade_P286Leader");
	CHECK(leader->testModelCondition(ModelCondition::indexOf("HERO")));
	f.frames(2);
	f.gate(*near, "P286NearLeader", "USER_1", true, true);
	CHECK_FALSE(enemy->testModelCondition(ModelCondition::indexOf("USER_1")));
	f.gate(*far, "P286NearLeader", "USER_1", false, false);
	f.frames(9);
	f.gate(*near, "P286NearLeader", "USER_1", false, false);
}

TEST_CASE("PORT-286: an upgraded aura reaches the beast and the same horde before and after changing formation (RW 0x89F42D)")
{
	GateWorld f(kAreaIni);
	Object *stable = f.unit("P286Stable", 'A', 400, 400);
	Object *horde = f.unit("P286Horde", 'A', 465, 400);
	Object *beast = f.unit("P286Beast", 'A', 400, 475);
	Object *far = f.unit("P286Beast", 'A', 720, 400);
	Object *standalone = f.unit("P286Wedge", 'A', 335, 400);
	f.frames(3);
	f.gate(*horde, "P286NearStable", "USER_9", false, false);
	f.gate(*beast, "P286BuyBeast", "USER_9", false, false);
	stable->giveUpgrade("Upgrade_P286Stable");
	f.frames(3);
	f.gate(*horde, "P286NearStable", "USER_9", true, true);
	f.gate(*beast, "P286BuyBeast", "USER_9", true, true);
	f.gate(*far, "P286BuyBeast", "USER_9", false, false);
	f.gate(*standalone, "P286NearStable", "USER_9", false, false);
	REQUIRE(standalone->getContain());
	const auto *members = standalone->getContain()->getContainedItemsList();
	REQUIRE(members);
	REQUIRE(members->size() == 1);
	CHECK(members->front()->testModelCondition(ModelCondition::indexOf("USER_9"))); // a marked rider does not mark its horde

	const ThingTemplate *base = horde->getTemplate();
	auto *formation = dynamic_cast<HordeContain *>(horde->getContain());
	REQUIRE(formation);
	REQUIRE(formation->canToggleFormation());
	REQUIRE(formation->toggleFormation());
	CHECK(horde->getTemplate() == base);
	f.frames(16); // longer than one modifier duration: the aura must replenish it after the change
	f.gate(*horde, "P286NearStable", "USER_9", true, true);
	f.gate(*standalone, "P286NearStable", "USER_9", false, false);
}

TEST_CASE("PORT-286: a passive building opens the nearby hero's button, and the mark expires out of range (RW 0x887DF7)")
{
	GateWorld f(kAreaIni);
	Object *near = f.unit("P286Receiver", 'A', 460, 400);
	Object *far = f.unit("P286Receiver", 'A', 765, 400);
	f.gate(*near, "P286NearPassive", "USER_3", false, false);
	f.unit("P286Shrine", 'A', 400, 400);
	f.frames(9);
	f.gate(*near, "P286NearPassive", "USER_3", true, true);
	f.gate(*far, "P286NearPassive", "USER_3", false, false);
	const Coord3D away{ 800, 480, 0 };
	near->setPosition(&away);
	f.frames(20);
	f.gate(*near, "P286NearPassive", "USER_3", false, false);
}

TEST_CASE("PORT-286: the tower's timed upgrade disables purchase only while the flag is set (RW 0x8BA8C3 and RW 0x942490)")
{
	GateWorld f(kAreaIni);
	Object *tower = f.unit("P286Tower", 'A', 400, 400);
	Object *neighbour = f.unit("P286Tower", 'A', 480, 400);
	f.frames(2);
	f.gate(*tower, "P286BuyReinforce", "USER_8", false, true);
	tower->giveUpgrade("Upgrade_P286Tower");
	f.gate(*tower, "P286BuyReinforce", "USER_8", true, false);
	f.gate(*neighbour, "P286BuyReinforce", "USER_8", false, true);
	f.frames(15);
	f.gate(*tower, "P286BuyReinforce", "USER_8", false, true);
}
