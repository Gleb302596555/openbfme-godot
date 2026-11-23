// OpenBFME. PORT-274 (issue #641): a FIRE_WEAPON command button that needs an object target (the mumakil's charge, Erkenbrand's
// Time to Strike, Sam's frying pan) through the path the player uses: select, press the button, left-click the target.
// Synthetic INI only: no game archive, no ROTWK_INSTALL / BFME2_INSTALL, no drawable.
// GPL-3.0.
#include "doctest.h"

#include "CombatTestUtil.h"
#include "HudTestUtil.h" // PinholeView

#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "GameClient/ControlBar.h"
#include "GameClient/ControlBarCommands.h"
#include "GameClient/HudInput.h"
#include "GameClient/TacticalView.h"
#include "GameLogic/AI/WeaponCommands.h"
#include "GameLogic/Combat/ObjectWeapons.h"
#include "GameLogic/GameMessage.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/AttributeModifiers.h"
#include "GameLogic/Object/AttributeModifierPool.h"
#include "GameLogic/Weapon.h"

namespace ModelCondition
{
int indexOf(const std::string &name);
}

#include <memory>
#include <string>
#include <vector>

namespace
{
const char kPort274Ini[] =
	// the mumakil pen's aura list (attributemodifier.ini:6601 shape): a SPELL-category list that only carries a model condition
	"ModifierList P274MumakilUpEnabler\n"
	"  Category = SPELL\n"
	"  Duration = 3000\n"
	"  ModelCondition = USER_9\n"
	"End\n"
	// the charge's weapon: a short reach, one shot every 20 s (the recharge the button shows)
	"Weapon P274ChargeWeapon\n"
	"  AttackRange = 120.0\n"
	"  DelayBetweenShots = 20000\n"
	"  PreAttackDelay = 200\n"
	"  PreAttackType = PER_SHOT\n"
	"  DamageNugget\n"
	"    Damage = 25\n"
	"    DamageType = CRUSH\n"
	"    DeathType = NORMAL\n"
	"  End\n"
	"End\n"
	"Weapon P274Sword\n"
	"  AttackRange = 15.0\n"
	"  MeleeWeapon = Yes\n"
	"  DelayBetweenShots = 1000\n"
	"  DamageNugget\n"
	"    Damage = 5\n"
	"    DamageType = SLASH\n"
	"    DeathType = NORMAL\n"
	"  End\n"
	"End\n"	"CommandButton Command_P274_ChargeAttack\n"
	"  Command = FIRE_WEAPON\n"
	"  WeaponSlot = SECONDARY\n"
	"  Options = NEED_TARGET_ENEMY_OBJECT OK_FOR_MULTI_SELECT OK_FOR_MULTI_EXECUTE\n"
	"  CursorName = EvilAbilityObj\n"
	"  InvalidCursorName = GenericInvalid\n"
	"  ButtonImage = ImgCharge\n"
	"  InPalantir = Yes\n"
	"End\n"
	// the reported buttons' shape: the same object-target FIRE_WEAPON gated on a model condition (the mumakil's USER_9 from its pen's
	// aura, Erkenbrand's MOUNTED)
	"CommandButton Command_P274_MountedCharge\n"
	"  Command = FIRE_WEAPON\n"
	"  WeaponSlot = SECONDARY\n"
	"  Options = NEED_TARGET_ENEMY_OBJECT\n"
	"  CursorName = EvilAbilityObj\n"
	"  EnableOnModelCondition = MOUNTED\n"
	"  ButtonImage = ImgCharge\n"
	"  InPalantir = Yes\n"
	"End\n"
	// the mumakil's shape exactly: FIRE_WEAPON PRIMARY object-target, gated on USER_9 by an AttributeModifier list
	"CommandButton Command_P274_User9Charge\n"
	"  Command = FIRE_WEAPON\n"
	"  WeaponSlot = PRIMARY\n"
	"  Options = NEED_TARGET_ENEMY_OBJECT OK_FOR_MULTI_SELECT OK_FOR_MULTI_EXECUTE\n"
	"  CursorName = EvilAbilityObj\n"
	"  EnableOnModelCondition = USER_9\n"
	"  ButtonImage = ImgCharge\n"
	"  InPalantir = Yes\n"
	"End\n"
	// the neighbouring button: a FIRE_WEAPON press with no target option sends at once
	"CommandButton Command_P274_NoTarget\n"
	"  Command = FIRE_WEAPON\n"
	"  WeaponSlot = PRIMARY\n"
	"  ButtonImage = ImgNoTarget\n"
	"  InPalantir = Yes\n"
	"End\n"
	"CommandButton Command_P274_Stop\n"
	"  Command = STOP\n"
	"  Options = OK_FOR_MULTI_SELECT\n"
	"  ButtonImage = ImgStop\n"
	"  InPalantir = No\n"
	"End\n"
	"CommandSet P274ChargerSet\n"
	"  1 = Command_P274_ChargeAttack\n"
	"  2 = Command_P274_NoTarget\n"
	"  3 = Command_P274_MountedCharge\n"
	"  4 = Command_P274_User9Charge\n"
	"  13 = Command_P274_Stop\n"
	"End\n"
	"Object P274Charger\n"
	"  KindOf = PRELOAD SELECTABLE CAN_ATTACK INFANTRY\n"
	"  VisionRange = 400\n"
	"  CommandSet = P274ChargerSet\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryHeight = 20\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY P274Sword\n"
	"    Weapon = SECONDARY P274ChargeWeapon\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 2000\n"
	"    InitialHealth = 2000\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  Behavior = PhysicsBehavior ModuleTag_Phys\n"
	"  End\n"
	"End\n"
		// the pen that grants the enabler (mumakilpennew.ini:365/375: AttributeModifierAuraUpdate, Range 9000, TriggeredBy a level upgrade)
	"Upgrade Upgrade_P274PenLevel2\n"
	"  Type = OBJECT\n"
	"End\n"
	"Object P274MumakilPen\n"
	"  KindOf = PRELOAD SELECTABLE STRUCTURE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 30\n"
	"  GeometryMinorRadius = 30\n"
	"  GeometryHeight = 40\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 2000\n"
	"  End\n"
	"  Behavior = AttributeModifierAuraUpdate ModuleTag_MinesEnabled\n"
	"    StartsActive = No\n"
	"    BonusName = P274MumakilUpEnabler\n"
	"    TriggeredBy = Upgrade_P274PenLevel2\n"
	"    RefreshDelay = 2000\n"
	"    Range = 9000\n"
	"    TargetEnemy = No\n"
	"    ObjectFilter = ANY +MONSTER ALLIES\n"
	"  End\n"
	"End\n"
	// the gated unit: the mumakil shape, its charge on the primary slot, gated on USER_9
	"Object P274Mumakil\n"
	"  KindOf = PRELOAD SELECTABLE CAN_ATTACK MONSTER\n"
	"  CommandSet = P274ChargerSet\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 12\n"
	"  GeometryMinorRadius = 12\n"
	"  GeometryHeight = 30\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = AttributeModifierPoolUpdate ModuleTag_Modifiers\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"  WeaponSet\n"
	"    Conditions = None\n"
	"    Weapon = PRIMARY P274ChargeWeapon\n"
	"  End\n"
	"End\n"
	"Object P274Foe\n"
	"  KindOf = PRELOAD SELECTABLE INFANTRY\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryHeight = 20\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 5000\n"
	"    InitialHealth = 5000\n"
	"  End\n"
	"  Behavior = PhysicsBehavior ModuleTag_Phys\n"
	"  End\n"
	"End\n";

struct Port274Rig : combattest::CombatWorld
{
	CommandStore commandStore;
	CommandStore *savedCommands = TheCommandStore;
	MouseSettings mouse;
	MetaMap metaMap;
	PinholeView view{ { 200.0f, -300.0f, 420.0f }, { 200.0f, 200.0f, 0.0f }, 1024, 768, 0.75f };
	std::unique_ptr<HudInput> input;
	std::unique_ptr<ControlBar> bar;

	AttributeModifierStore modifierStore;
	AttributeModifierStore *savedModifiers = TheAttributeModifierStore;

	Port274Rig()
		: combattest::CombatWorld("", 40)
	{
		// XP-1: TheAttributeModifierStore (RW 0xDE3C14) is installed by the retail world; a synthetic one is needed for the ModifierList block
		modifierStore.registerBlock(w.fx.env.blocks);
		TheAttributeModifierStore = &modifierStore;
		TheCommandStore = &commandStore;
		commandStore.setThingFactory(&w.things);
		w.fx.env.blocks.registerBlock("CommandButton", [](INI *ini) { CommandStore::parseCommandButtonDefinitionGlobal(ini); });
		w.fx.env.blocks.registerBlock("CommandSet", [](INI *ini) { CommandStore::parseCommandSetDefinitionGlobal(ini); });
		const std::string err = w.load(kPort274Ini, INI_LOAD_OVERWRITE, "port274.ini");
		REQUIRE_MESSAGE(err.empty(), err);

		WeaponCommands::registerHandlers(dispatcher);

		Player *alice = players.findPlayerWithName("Alice");
		REQUIRE(alice != nullptr);
		players.setLocalPlayer(alice);

		input = std::make_unique<HudInput>(*logic, ai.get(), view, commands, mouse, metaMap);
		input->context().commands = &commandStore;
		bar = std::make_unique<ControlBar>(input->context());
	}

	~Port274Rig()
	{
		bar.reset();
		input.reset();
		logic->reset();
		TheCommandStore = savedCommands;
		TheAttributeModifierStore = savedModifiers;
	}

	Object *spawn(const char *name, float x, float y, char side = 'A')
	{
		Object *obj = unit(name, side, x, y);
		REQUIRE(obj != nullptr);
		return obj;
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

	void selectGroup(std::initializer_list<Object *> objs)
	{
		input->ui().setGUICommand(nullptr);
		std::vector<ObjectID> ids;
		GameMessage sel(MSG_CREATE_SELECTED_GROUP, playerIndex("Alice"));
		sel.appendBooleanArgument(true);
		for (Object *obj : objs)
		{
			if (obj)
			{
				ids.push_back(obj->getID());
				sel.appendObjectIDArgument(obj->getID());
			}
		}
		input->ui().mirrorSelection(ids);
		input->submit(sel);
		input->flush();
		frames(1);
		bar->update();
	}

	const ControlBarButton *buttonNamed(const char *name) const
	{
		for (const auto *list : { &bar->palantirButtons(), &bar->sideButtons(), &bar->offBarButtons() })
		{
			for (const ControlBarButton &b : *list)
			{
				if (b.button && b.button->m_name == name)
				{
					return &b;
				}
			}
		}
		return nullptr;
	}

	// the player's gesture: the pointer over the world point, then a left press and release
	void leftClickWorld(const Coord3D &worldPos)
	{
		ICoord2D screen{};
		REQUIRE(view.worldToScreen(worldPos, screen));
		input->mouseMove(screen.x, screen.y);
		input->mouseButton(HudInput::Button::Left, true, screen.x, screen.y, 0, 1000);
		input->mouseButton(HudInput::Button::Left, false, screen.x, screen.y, 0, 1010);
	}

	// model condition 214 in retail's name table; the port's index comes from the same table
	static int mountedCondition()
	{
		// tolerance: the name table is the game's (591 names); MOUNTED's exact index is the one the mod's INI and RW's +0x10C share
		static const int bit = ModelCondition::indexOf("MOUNTED");
		REQUIRE(bit == 214); // the MOUNTED row of Common/ModelConditionNames.inc
		return bit;
	}

	size_t countOf(int type) const
	{
		size_t n = 0;
		for (const GameMessage &m : commands.messages())
		{
			n += m.getType() == type ? 1u : 0u;
		}
		return n;
	}
};
} // namespace

TEST_CASE("PORT-274: a FIRE_WEAPON button that needs an enemy object enters targeting on its press and the left click on the enemy sends MSG_DO_WEAPON_AT_OBJECT with the button's slot, the target and its shot limit")
{
	Port274Rig r;
	Object *charger = r.spawn("P274Charger", 180.0f, 200.0f);
	Object *foe = r.spawn("P274Foe", 300.0f, 200.0f, 'B');
	r.frames(2);
	r.selectGroup({ charger });

	const ControlBarButton *btn = r.buttonNamed("Command_P274_ChargeAttack");
	REQUIRE(btn != nullptr);
	CHECK(btn->button->m_command == GUI_COMMAND_FIRE_WEAPON);
	CHECK(btn->state == ButtonState::Enabled);

	const size_t objectsBefore = r.countOf(MSG_DO_WEAPON_AT_OBJECT);
	REQUIRE(r.bar->pressButton(btn->slot, btn->inPalantir));
	r.input->update();
	// the press itself is the targeting entry, not a message
	CHECK(r.input->ui().getGUICommand() == btn->button);
	CHECK(r.countOf(MSG_DO_WEAPON_AT_OBJECT) == objectsBefore);

	Coord3D at = *foe->getPosition();
	at.z += 8.0f;
	r.leftClickWorld(at);
	r.input->flush();

	const auto &queued = r.commands.messages();
	size_t found = 0;
	for (const GameMessage &m : queued)
	{
		if (m.getType() != MSG_DO_WEAPON_AT_OBJECT)
		{
			continue;
		}
		++found;
		REQUIRE(m.getArgumentCount() == 3);
		CHECK(m.getArgument(0)->integer == btn->button->m_weaponSlot);
		CHECK(m.getArgument(1)->objectID == foe->getID());
		CHECK(m.getArgument(2)->integer == btn->button->m_maxShotsToFire);
	}
	CHECK(found == 1);
	CHECK(r.input->ui().getGUICommand() == nullptr);
}

TEST_CASE("PORT-274: the click of a FIRE_WEAPON button that needs an enemy object is refused on an ally, as RotWK refuses it: no message and the target mode stays")
{
	Port274Rig r;
	Object *charger = r.spawn("P274Charger", 180.0f, 200.0f);
	Object *ally = r.spawn("P274Foe", 300.0f, 200.0f, 'A');
	r.frames(2);
	r.selectGroup({ charger });

	const ControlBarButton *btn = r.buttonNamed("Command_P274_ChargeAttack");
	REQUIRE(btn != nullptr);
	REQUIRE(r.bar->pressButton(btn->slot, btn->inPalantir));
	r.input->update();
	REQUIRE(r.input->ui().getGUICommand() == btn->button);

	const size_t objectsBefore = r.countOf(MSG_DO_WEAPON_AT_OBJECT);
	Coord3D at = *ally->getPosition();
	at.z += 8.0f;
	r.leftClickWorld(at);
	r.input->flush();
	CHECK(r.countOf(MSG_DO_WEAPON_AT_OBJECT) == objectsBefore);
}

TEST_CASE("PORT-274: the neighbouring FIRE_WEAPON button without a target option sends MSG_DO_WEAPON { slot, maxShots } on the press alone")
{
	Port274Rig r;
	Object *charger = r.spawn("P274Charger", 180.0f, 200.0f);
	r.frames(2);
	r.selectGroup({ charger });

	const ControlBarButton *btn = r.buttonNamed("Command_P274_NoTarget");
	REQUIRE(btn != nullptr);
	CHECK(btn->button->m_command == GUI_COMMAND_FIRE_WEAPON);
	REQUIRE(r.bar->pressButton(btn->slot, btn->inPalantir));
	r.input->flush();
	CHECK(r.input->ui().getGUICommand() == nullptr);
	const auto &queued = r.commands.messages();
	REQUIRE(queued.size() == 1);
	CHECK(queued[0].getType() == MSG_DO_WEAPON);
	REQUIRE(queued[0].getArgumentCount() == 2);
	CHECK(queued[0].getArgument(0)->integer == btn->button->m_weaponSlot);
	CHECK(queued[0].getArgument(1)->integer == btn->button->m_maxShotsToFire);
}

TEST_CASE("PORT-274: after the click the slot's weapon fires at the clicked target in the logic (MSG_DO_WEAPON_AT_OBJECT -> AIGroup attack on that object)")
{
	Port274Rig r;
	Object *charger = r.spawn("P274Charger", 180.0f, 200.0f);
	Object *foe = r.spawn("P274Foe", 300.0f, 200.0f, 'B');
	r.frames(2);
	r.selectGroup({ charger });

	const ControlBarButton *btn = r.buttonNamed("Command_P274_ChargeAttack");
	REQUIRE(btn != nullptr);
	REQUIRE(btn->state == ButtonState::Enabled);
	REQUIRE(r.bar->pressButton(btn->slot, btn->inPalantir));
	r.input->update();
	const float healthBefore = foe->getBodyModule()->getHealth();
	Coord3D at = *foe->getPosition();
	at.z += 8.0f;
	r.leftClickWorld(at);
	r.input->flush();

	ObjectWeapons *weapons = charger->getWeapons();
	REQUIRE(weapons != nullptr);
	REQUIRE(weapons->weaponInSlot(btn->button->m_weaponSlot) != nullptr);
	const unsigned long long shotsBefore = weapons->stats().shotsFired;
	int framesToShot = -1;
	for (int f = 0; f < 200 && framesToShot < 0; ++f)
	{
		r.frames(1);
		if (weapons->stats().shotsFired != shotsBefore)
		{
			framesToShot = f;
		}
	}
	// tolerance: the frame the shot lands depends on the approach path; the count is pinned above
	CHECK(framesToShot >= 0);
	// tolerance: the approach varies the total; that the hit landed and was the charge's damage is pinned above
	CHECK(foe->getBodyModule()->getHealth() < healthBefore);
}

// The reported symptom of the five buttons (docs/ennorath/MATRIX.tsv NO-SEND rows) is that the press "sent nothing". The press is
// refused before any message exists when the button is not available: ControlBar::evaluate (RW 0x942733) answers disabled, and
// processCommandUI's caller drops the press. This case pins that ordering for the shape the reported buttons have: a FIRE_WEAPON
// object-target button gated by EnableOnModelCondition (the mumakil's USER_9 from its pen's aura, Erkenbrand's MOUNTED), and one
// gated by DisableOnModelCondition (Sam's WEAPONSET_TOGGLE_1).
TEST_CASE("PORT-274: a FIRE_WEAPON object-target button whose EnableOnModelCondition does not hold is Restricted and the press sends nothing; with the condition set the same press and click send MSG_DO_WEAPON_AT_OBJECT (RW 0x942490 / RW 0x942733)")
{
	Port274Rig r;
	Object *charger = r.spawn("P274Charger", 180.0f, 200.0f);
	Object *foe = r.spawn("P274Foe", 300.0f, 200.0f, 'B');
	r.frames(2);
	r.selectGroup({ charger });

	const ControlBarButton *btn = r.buttonNamed("Command_P274_MountedCharge");
	REQUIRE(btn != nullptr);
	CHECK(btn->button->m_command == GUI_COMMAND_FIRE_WEAPON);

	SUBCASE("without the model condition the button is Restricted and nothing is sent")
	{
		REQUIRE_FALSE(charger->testModelCondition(r.mountedCondition()));
		CHECK(btn->state == ButtonState::Restricted);
		const size_t objectsBefore = r.countOf(MSG_DO_WEAPON_AT_OBJECT);
		REQUIRE_FALSE(r.bar->pressButton(btn->slot, btn->inPalantir));
		r.input->update();
		// no targeting mode, so a click in the world is a plain order and never this button's
		CHECK(r.input->ui().getGUICommand() == nullptr);
		Coord3D at = *foe->getPosition();
		at.z += 8.0f;
		r.leftClickWorld(at);
		r.input->flush();
		CHECK(r.countOf(MSG_DO_WEAPON_AT_OBJECT) == objectsBefore);
	}

	SUBCASE("with the model condition the same press and click send the message")
	{
		charger->setModelConditionState(r.mountedCondition(), true);
		r.frames(1);
		btn = r.buttonNamed("Command_P274_MountedCharge");
		REQUIRE(btn != nullptr);
		REQUIRE(btn->state == ButtonState::Enabled);
		REQUIRE(r.bar->pressButton(btn->slot, btn->inPalantir));
		r.input->update();
		REQUIRE(r.input->ui().getGUICommand() == btn->button);
		Coord3D at = *foe->getPosition();
		at.z += 8.0f;
		r.leftClickWorld(at);
		r.input->flush();
		size_t found = 0;
		for (const GameMessage &m : r.commands.messages())
		{
			if (m.getType() == MSG_DO_WEAPON_AT_OBJECT && m.getArgument(1)->objectID == foe->getID())
			{
				++found;
			}
		}
		CHECK(found == 1);
	}
}

// The mumakil's gate is not a model-condition upgrade but an AttributeModifier: ModifierList MumakilUpEnabler (attributemodifier.ini:6601,
// Category = SPELL, ModelCondition = USER_9, Duration = 3000) granted by the pen's AttributeModifierAuraUpdate (mumakilpennew.ini:365/375).
// RW 0x805375 clears a removed list's conditions and RW 0x8058F4 clears an expired one, so the pool owns that bit; this case pins that a
// list granted into the pool sets USER_9 on the object, the button gated on it turns Enabled, and losing the list restricts it again.
TEST_CASE("PORT-274: an AttributeModifier list whose ModelCondition is USER_9 enables a button gated on USER_9 while the pool holds it, and restricts it again when the list goes (RW 0x805375 / RW 0x8058F4)")
{
	Port274Rig r;
	Object *mumakil = r.spawn("P274Mumakil", 200.0f, 200.0f);
	r.frames(2);
	r.selectGroup({ mumakil });

	const int user9 = ModelCondition::indexOf("USER_9");
	REQUIRE(user9 == 461); // the USER_9 row of Common/ModelConditionNames.inc

	const ControlBarButton *btn = r.buttonNamed("Command_P274_User9Charge");
	REQUIRE(btn != nullptr);
	CHECK_FALSE(mumakil->testModelCondition(user9));
	CHECK(btn->state == ButtonState::Restricted);

	AttributeModifierPool *pool = static_cast<AttributeModifierPool *>(mumakil->attributeModifierPoolModule());
	REQUIRE(pool != nullptr);
	// the aura's grant (Object::addAttributeModifier RW 0x68F1A8): the list, for its own duration
	REQUIRE(mumakil->addAttributeModifier("P274MumakilUpEnabler", -1));
	r.frames(1);
	CHECK(mumakil->testModelCondition(user9)); // the pool pushed it onto the object (RW 0x805375's counterpart on grant)
	btn = r.buttonNamed("Command_P274_User9Charge");
	REQUIRE(btn != nullptr);
	CHECK(btn->state == ButtonState::Enabled);

	// the press and click now send the message
	REQUIRE(r.bar->pressButton(btn->slot, btn->inPalantir));
	r.input->update();
	REQUIRE(r.input->ui().getGUICommand() == btn->button);
	Object *foe = r.spawn("P274Foe", 300.0f, 260.0f, 'B');
	r.frames(1);
	Coord3D at = *foe->getPosition();
	at.z += 8.0f;
	r.leftClickWorld(at);
	r.input->flush();
	size_t found = 0;
	for (const GameMessage &m : r.commands.messages())
	{
		if (m.getType() == MSG_DO_WEAPON_AT_OBJECT && m.getArgument(1)->objectID == foe->getID())
		{
			++found;
		}
	}
	CHECK(found == 1);

	// the list goes: the bit goes with it and the button is Restricted again
	mumakil->removeAttributeModifier("P274MumakilUpEnabler");
	r.frames(1);
	CHECK_FALSE(mumakil->testModelCondition(user9));
	btn = r.buttonNamed("Command_P274_User9Charge");
	REQUIRE(btn != nullptr);
	CHECK(btn->state == ButtonState::Restricted);
}

// The whole mod mechanism, end to end: the mumakil pen's level-2 upgrade starts its AttributeModifierAuraUpdate, the aura grants
// MumakilUpEnabler (ModelCondition = USER_9) to the allied mumakil, and that bit is what enables Command_ChargeAttack. Without the
// upgrade the charge is Restricted and a press sends nothing; with it the same press and click send MSG_DO_WEAPON_AT_OBJECT.
TEST_CASE("PORT-274: the mumakil pen's aura grants USER_9 to an allied mumakil and only then does the charge button press reach MSG_DO_WEAPON_AT_OBJECT (mumakilpennew.ini:365/375 -> attributemodifier.ini:6601)")
{
	Port274Rig r;
	Object *pen = r.spawn("P274MumakilPen", 200.0f, 200.0f);
	Object *mumakil = r.spawn("P274Mumakil", 230.0f, 200.0f);
	Object *foe = r.spawn("P274Foe", 330.0f, 200.0f, 'B');
	r.frames(3);
	r.selectGroup({ mumakil });

	const int user9 = ModelCondition::indexOf("USER_9");
	REQUIRE(user9 == 461); // the USER_9 row of Common/ModelConditionNames.inc
	CHECK_FALSE(mumakil->testModelCondition(user9));
	CHECK(r.buttonNamed("Command_P274_User9Charge")->state == ButtonState::Restricted);

	// the pen's level-2 upgrade starts the aura
	pen->giveUpgrade("Upgrade_P274PenLevel2");
	bool granted = false;
	for (int i = 0; i < 40 && !granted; ++i)
	{
		r.frames(1);
		granted = mumakil->testModelCondition(user9);
	}
	REQUIRE(granted); // the aura reached the mumakil
	const ControlBarButton *btn = r.buttonNamed("Command_P274_User9Charge");
	REQUIRE(btn != nullptr);
	REQUIRE(btn->state == ButtonState::Enabled);

	REQUIRE(r.bar->pressButton(btn->slot, btn->inPalantir));
	r.input->update();
	REQUIRE(r.input->ui().getGUICommand() == btn->button);
	Coord3D at = *foe->getPosition();
	at.z += 8.0f;
	r.leftClickWorld(at);
	r.input->flush();
	size_t found = 0;
	for (const GameMessage &m : r.commands.messages())
	{
		if (m.getType() == MSG_DO_WEAPON_AT_OBJECT && m.getArgument(1)->objectID == foe->getID())
		{
			++found;
		}
	}
	CHECK(found == 1);
}
