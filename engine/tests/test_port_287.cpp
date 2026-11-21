// OpenBFME. GPL-3.0. Synthetic INI only; no retail or mod mounts.
// A soldier's drawable chooses its horde before the control bar supplies a button (RW 0x7108F2,
// DONOR BFME2 0x70F14C, GameClient/Rva0030F14CSelect.cpp). Neither selection nor the two buttons
// make different power templates aliases (RW 0x68C26D, SpecialPowerModule slot 0 at RW 0x8969D1).

#include "doctest.h"

#include "SyntheticHudTestUtil.h"

#include "Common/SpecialPower.h"
#include "GameClient/ControlBar.h"
#include "GameClient/HudObjects.h"
#include "GameLogic/AttributeModifiers.h"
#include "GameLogic/Combat/CombatState.h"
#include "GameLogic/Combat/WeaponDelivery.h"
#include "GameLogic/Module/LogicModules.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Object/Contain/HordeContainRuntime.h"
#include "GameLogic/SpellCommands.h"
#include "GameLogic/Weapon.h"
#include "GameLogic/WeaponStores.h"

namespace
{
constexpr char kIni[] = R"ini(
Upgrade Upgrade_P287ChargeReady
  Type = OBJECT
End
ModifierList P287ChargeBonus
  Category = SPELL
  Modifier = DAMAGE_MULT 175%
  Duration = 18000
End
SpecialPower P287OldCharge
  Enum = SPECIAL_ATTRIBUTEMOD_CANCELDISGUISE
  ReloadTime = 5000
End
SpecialPower P287Charge
  Enum = SPECIAL_ATTRIBUTEMOD_CANCELDISGUISE
  ReloadTime = 7000
End
CommandButton P287OldButton
  Command = SPECIAL_POWER
  SpecialPower = P287OldCharge
  Options = OK_FOR_MULTI_SELECT OK_FOR_MULTI_EXECUTE NEED_TARGET_ENEMY_OBJECT
  InPalantir = Yes
  AutoAbility = Yes
End
CommandButton P287ChargeButton
  Command = SPECIAL_POWER
  SpecialPower = P287Charge
  Options = OK_FOR_MULTI_SELECT OK_FOR_MULTI_EXECUTE
  InPalantir = Yes
End
CommandSet P287SoldierSet
  1 = P287OldButton
End
CommandSet P287ChargeSet
  1 = P287ChargeButton
End
Weapon P287TestHit
  AttackRange = 150
  DamageNugget
    Damage = 16
    DamageType = SLASH
  End
End
Object P287Dummy
  KindOf = SELECTABLE INFANTRY
  Body = ActiveBody BodyTag
    MaxHealth = 400
  End
End
Object P287Soldier
  KindOf = SELECTABLE INFANTRY HORDE_MEMBER
  CommandSet = P287SoldierSet
  Body = ActiveBody BodyTag
    MaxHealth = 400
  End
  Behavior = AttributeModifierPoolUpdate ModifierPoolTag
  End
  Behavior = SpecialPowerModule SoldierStarter
    SpecialPowerTemplate = P287Charge
    AvailableAtStart = Yes
    UpdateModuleStartsAttack = No
    AttributeModifier = P287ChargeBonus
    AttributeModifierAffectsSelf = Yes
  End
End
Object P287Horde
  KindOf = SELECTABLE INFANTRY HORDE
  Body = ActiveBody BodyTag
    MaxHealth = 400
  End
  Behavior = HordeContain ContainTag
    InitialPayload = P287Soldier 2
    RankInfo = RankNumber:1 UnitType:P287Soldier Position:X:20 Y:20 Position:X:20 Y:-20
  End
End
ChildObject P287ChargeHorde P287Horde
  CommandSet = P287ChargeSet
  Behavior = SpecialPowerModule ChargeStarter
    SpecialPowerTemplate = P287Charge
    StartsPaused = Yes
    AvailableAtStart = Yes
    UpdateModuleStartsAttack = No
    AttributeModifier = P287ChargeBonus
    AttributeModifierAffectsSelf = Yes
  End
  Behavior = UnpauseSpecialPowerUpgrade ChargeUnlock
    SpecialPowerTemplate = P287Charge
    TriggeredBy = Upgrade_P287ChargeReady
  End
End
)ini";

struct Rig : synthetichud::Rig
{
	WeaponStores weapons;
	SpecialPowerStore powers;
	CommandStore commandData;
	AttributeModifierStore modifiers;
	SpecialPowerStore *oldPowers = TheSpecialPowerStore;
	CommandStore *oldCommands = TheCommandStore;
	AttributeModifierStore *oldModifiers = TheAttributeModifierStore;
	SpellCommands spells;
	ControlBar bar{ input->context() };

	Rig()
	{
		LogicModules::registerAll(w.modules);
		weapons.install();
		TheSpecialPowerStore = &powers;
		TheCommandStore = &commandData;
		TheAttributeModifierStore = &modifiers;
		commandData.setThingFactory(&w.things);
		input->context().commands = &commandData;
		auto &blocks = w.fx.env.blocks;
		blocks.registerBlock("SpecialPower", &SpecialPowerStore::parseSpecialPowerDefinitionGlobal);
		blocks.registerBlock("CommandSet", &CommandStore::parseCommandSetDefinitionGlobal);
		blocks.registerBlock("CommandButton", &CommandStore::parseCommandButtonDefinitionGlobal);
		blocks.registerBlock("Weapon", &WeaponStore::parseWeaponTemplateDefinitionGlobal);
		modifiers.registerBlock(blocks);
		spells.registerHandlers(dispatch);
		logic->combat().setAutoAcquireEnabled(false);
		logic->settings().bodyThresholdsLoaded = true;
		logic->settings().unitDamagedThreshold = 0.5f;
		logic->settings().unitReallyDamagedThreshold = 0.1f;
	}

	~Rig()
	{
		logic->reset();
		TheAttributeModifierStore = oldModifiers;
		TheCommandStore = oldCommands;
		TheSpecialPowerStore = oldPowers;
	}

	const ControlBarButton *button(const char *name)
	{
		bar.update();
		for (const auto &b : bar.palantirButtons())
		{
			if (b.button && b.button->m_name == name)
			{
				return &b;
			}
		}
		return nullptr;
	}

	bool press(const ControlBarButton &b)
	{
		const bool sent = bar.pressButton(b.slot, b.inPalantir);
		input->update();
		return sent;
	}

	void load()
	{
		const auto error = w.load(kIni, INI_LOAD_OVERWRITE, "port287.ini");
		REQUIRE_MESSAGE(error.empty(), error);
	}

	void frames(unsigned n)
	{
		for (unsigned i = 0; i < n; ++i)
		{
			logic->runLogicFrame();
		}
	}

	const SpecialPowerTemplate *power(const char *name)
	{
		const auto *t = powers.findSpecialPowerTemplate(name);
		REQUIRE(t);
		return t;
	}

	float hit(Object &source, Object &target)
	{
		const auto *w = weapons.weapons().findWeaponTemplate("P287TestHit");
		REQUIRE(w);
		const float before = target.getBodyModule()->getHealth();
		DeliverNuggets(*logic, source.getID(), *w, WeaponBonus{}, &target, target.getPosition(), false, nullptr);
		return before - target.getBodyModule()->getHealth();
	}
};

void moveTo(Object &o, float x, float y)
{
	const Coord3D at{ x, y, 0.0f };
	o.setPosition(&at);
}

HordeContain &hordeContain(Object &o)
{
	auto *contain = dynamic_cast<HordeContain *>(o.getContain());
	REQUIRE(contain);
	REQUIRE(contain->getContainCount() == 2);
	return *contain;
}

enum class SelectionGesture
{
	Point,
	Box,
};

void checkHordeCharge(SelectionGesture gesture)
{
	Rig r;
	r.load();
	Object *horde = r.place("P287ChargeHorde", 100, 100);
	Object *victim = r.place("P287Dummy", 180, 100, "Bob");
	HordeContain &contain = hordeContain(*horde);
	Object *released = contain.getContainedItemsList()->front();
	Object *fighting = contain.getContainedItemsList()->back();
	// The release (RW 0x86EBF4) takes the soldier out of the contain list but leaves its ProducerID.
	contain.releaseMemberForGarrison(released);
	moveTo(*released, 250, 100);
	REQUIRE(released->getContainedBy() == nullptr);
	REQUIRE(released->getProducerID() == horde->getID());
	CHECK(r.hit(*fighting, *victim) == 16.0f);
	CHECK(r.hit(*released, *victim) == 16.0f);

	if (gesture == SelectionGesture::Point)
	{
		r.click(250, 100);
	}
	else
	{
		r.drag(230, 80, 270, 120);
	}
	CHECK(HudObjects::pickObject(r.input->context(), { 250, 100 }) == released); // ordinary order pick still sees the raw soldier
	r.selected({ horde->getID() });
	REQUIRE(r.button("P287OldButton") == nullptr);
	const ControlBarButton *button = r.button("P287ChargeButton");
	REQUIRE(button);
	CHECK(button->state != ButtonState::Enabled);
	horde->giveUpgrade("Upgrade_P287ChargeReady"); // real UnpauseSpecialPowerUpgrade
	r.frames(1);
	button = r.button("P287ChargeButton");
	REQUIRE(button);
	CHECK(button->state == ButtonState::Enabled);
	REQUIRE(r.press(*button));
	const GameMessage &cast = r.onlyCommand(MSG_DO_SPECIAL_POWER);
	REQUIRE(cast.getArgumentCount() == 4);
	CHECK(cast.getArgument(0)->integer == (int)r.power("P287Charge")->getID());
	CHECK(cast.getArgument(3)->objectID == INVALID_ID); // the horde is taken from the player's selection
	r.drain();

	// RW 0x897987 adds only the current member(s) to a horde's self-affecting modifier.
	// The ordinary damage path reads DAMAGE_MULT from that source (RW 0x90E4D2–0x90E4F3).
	CHECK(r.hit(*fighting, *victim) == 28.0f);
	CHECK(r.hit(*released, *victim) == 16.0f);
}

} // namespace

TEST_CASE("PORT-287 charge: clicking a released soldier casts the horde's damage buff")
{
	checkHordeCharge(SelectionGesture::Point);
}

TEST_CASE("PORT-287 charge: boxing a released soldier casts the horde's damage buff")
{
	checkHordeCharge(SelectionGesture::Box);
}

TEST_CASE("PORT-287 charge: without a horde producer the old targeted button does not alias a power with the same Enum")
{
	Rig r;
	r.load();
	Object *soldier = r.place("P287Soldier", 140, 100);
	Object *victim = r.place("P287Dummy", 200, 100, "Bob");
	SUBCASE("no producer") { CHECK(soldier->getProducerID() == INVALID_ID); }
	SUBCASE("a non-horde producer") { soldier->setProducer(r.place("TestUnarmed", 80, 80)); }
	r.drag(120, 80, 160, 120);
	r.selected({ soldier->getID() });
	r.click(140, 100);
	r.selected({ soldier->getID() });
	const auto *actual = r.power("P287Charge");
	const auto *old = r.power("P287OldCharge");
	REQUIRE(actual->getID() != old->getID());
	REQUIRE(actual->getSpecialPowerType() == old->getSpecialPowerType());
	REQUIRE(SpecialPowerModules::findModule(*soldier, actual));
	CHECK(SpecialPowerModules::findModule(*soldier, old) == nullptr);
	const ControlBarButton *button = r.button("P287OldButton");
	REQUIRE(button);
	CHECK(button->state == ButtonState::Enabled);
	REQUIRE(r.press(*button));
	CHECK(r.input->ui().getGUICommand() == button->button);
	r.click(200, 100);
	CHECK(std::none_of(r.commands.messages().begin(), r.commands.messages().end(), [](const GameMessage &m) {
		return m.getType() == MSG_DO_SPECIAL_POWER_AT_OBJECT;
	}));
	r.drain();
	r.frames(5);
	CHECK(r.hit(*soldier, *victim) == 16.0f);
}

TEST_CASE("PORT-287 selection: an unselectable soldier or horde producer is not selected")
{
	Rig r;
	r.load();
	Object *horde = r.place("P287ChargeHorde", 100, 100);
	HordeContain &contain = hordeContain(*horde);
	Object *soldier = contain.getContainedItemsList()->front();
	contain.releaseMemberForGarrison(soldier);
	moveTo(*soldier, 240, 100);
	const auto status = (unsigned)ObjectTemplateInfoBuilder::objectStatusIndex("UNSELECTABLE");
	SUBCASE("horde producer") { horde->setStatus(status, true); }
	SUBCASE("raw soldier") { soldier->setStatus(status, true); }
	r.click(240, 100);
	r.selected({});
	r.drag(220, 80, 260, 120);
	r.selected({});
}
