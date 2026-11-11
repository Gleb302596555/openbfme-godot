// OpenBFME unit tests. GPL-3.0.
// P-86: the COMMAND_POINT_BONUS attribute modifier (type 0x18) raises its owner's command point limit when the list is added and lowers it when the
// list is removed. Synthetic data, no game files.
//
// RotWK 2.01 (game.dat):
//   RW 0x6A7C01  the bonus of one object = the template's CommandPointBonus (tt + 0x62C) + the object's COMMAND_POINT_BONUS sum (RW 0x68C818(0x18, &out, 0),
//                the sum starts at 0.0), cvttss2si; added by RW 0x6A7C3E, subtracted by RW 0x6A7C51
//   RW 0x68F1A8  Object::addAttributeModifier: RW 0x68F207 .. 0x68F243: with a controlling player and tt + 0x62C > 0 it calls Player::onObjectLost
//                (RW 0x6AA590) BEFORE the pool's add (RW 0x805A8E) and Player::onObjectGained (RW 0x6AA56D) after a successful add
//   RW 0x68F259  Object::removeAttributeModifier: RW 0x68F2AE .. 0x68F2E5: the same pair around the pool's remove (RW 0x8052FB)
// The expected numbers are derived by hand from these rules.

#include "doctest.h"

#include "EconTestUtil.h"

#include "Common/Player.h"
#include "GameLogic/AttributeModifiers.h"
#include "GameLogic/Object/Object.h"

using namespace logictest;
using namespace econtest;

namespace
{
const char kP86Objects[] =
	"Object P86Citadel
"
	"  KindOf = SELECTABLE STRUCTURE
"
	"  CommandPointBonus = 100
"
	"  Behavior = AttributeModifierPoolUpdate ModuleTag_Pool
"
	"  End
"
	"End
"
	"Object P86Farm
"
	"  KindOf = SELECTABLE STRUCTURE
"
	"  CommandPointBonus = 10
"
	"  Behavior = AttributeModifierPoolUpdate ModuleTag_Pool
"
	"  End
"
	"End
"
	"Object P86Hut
"
	"  KindOf = SELECTABLE STRUCTURE
"
	"  Behavior = AttributeModifierPoolUpdate ModuleTag_Pool
"
	"  End
"
	"End
"
	"Object P86Soldier
"
	"  KindOf = SELECTABLE INFANTRY
"
	"  CommandPoints = 6
"
	"  Behavior = AttributeModifierPoolUpdate ModuleTag_Pool
"
	"  End
"
	"End
";

const char kP86Lists[] =
	"ModifierList P86Bonus50
"
	"  Category = SPELL
"
	"  Modifier = COMMAND_POINT_BONUS 50
"
	"End
"
	"ModifierList P86Bonus20
"
	"  Category = SPELL
"
	"  Modifier = COMMAND_POINT_BONUS 20
"
	"End
"
	"ModifierList P86ArmorOnly
"
	"  Category = SPELL
"
	"  Modifier = ARMOR 50%
"
	"End
";

struct P86World : Fx
{
	AttributeModifierStore modifiers;
	AttributeModifierStore *savedModifiers = TheAttributeModifierStore;
	P86World()
		: Fx(kP86Objects)
	{
		TheAttributeModifierStore = &modifiers;
		modifiers.registerBlock(w.fx.env.blocks);
		const std::string error = w.load(kP86Lists);
		REQUIRE_MESSAGE(error.empty(), error);
	}
	~P86World()
	{
		logic->reset();
		TheAttributeModifierStore = savedModifiers;
	}
};
} // namespace

TEST_CASE("P-86 command point bonus: a COMMAND_POINT_BONUS list added to a building raises the limit, its removal lowers it (RW 0x68F207 / 0x68F2AE)")
{
	P86World fx;
	Player *alice = fx.player("Alice");
	REQUIRE(alice != nullptr);
	const int base = alice->commandPointLimit();

	Object *citadel = fx.make("P86Citadel", fx.teamOf("Alice"));
	REQUIRE(citadel != nullptr);
	CHECK(alice->commandPoints().getBonus() == 100);
	CHECK(alice->commandPointLimit() == base + 100);

	// the upgrade's list: the bonus is taken off with the old sum (100 + 0) and put back with the new one (cvttss2si(100.0 + 50.0) = 150)
	REQUIRE(citadel->addAttributeModifier("P86Bonus50", -1));
	CHECK(alice->commandPoints().getBonus() == 150);
	CHECK(alice->commandPointLimit() == base + 150);

	// a second list adds to the sum: 100 + 50 + 20
	REQUIRE(citadel->addAttributeModifier("P86Bonus20", -1));
	CHECK(alice->commandPoints().getBonus() == 170);

	// a list without the modifier is recounted too and changes nothing
	REQUIRE(citadel->addAttributeModifier("P86ArmorOnly", -1));
	CHECK(alice->commandPoints().getBonus() == 170);

	// removal: the same pair around the pool's remove
	citadel->removeAttributeModifier("P86Bonus50");
	CHECK(alice->commandPoints().getBonus() == 120);
	citadel->removeAttributeModifier("P86Bonus20");
	CHECK(alice->commandPoints().getBonus() == 100);
	CHECK(alice->commandPointLimit() == base + 100);
}

TEST_CASE("P-86 command point bonus: any building with a CommandPointBonus recounts, and its loss takes the modified bonus away (RW 0x6A7C51)")
{
	P86World fx;
	Player *alice = fx.player("Alice");
	Player *bob = fx.player("Bob");
	REQUIRE(alice != nullptr);
	REQUIRE(bob != nullptr);

	Object *farm = fx.make("P86Farm", fx.teamOf("Alice"));
	Object *citadel = fx.make("P86Citadel", fx.teamOf("Alice"));
	CHECK(alice->commandPoints().getBonus() == 110);
	REQUIRE(farm->addAttributeModifier("P86Bonus20", -1));
	REQUIRE(citadel->addAttributeModifier("P86Bonus50", -1));
	CHECK(alice->commandPoints().getBonus() == 180); // (10 + 20) + (100 + 50)
	CHECK(bob->commandPoints().getBonus() == 0);

	// destruction subtracts the bonus with the live sum: nothing of the modifier stays behind
	fx.logic->destroyObject(citadel);
	fx.logic->processDestroyList();
	CHECK(alice->commandPoints().getBonus() == 30);
	fx.logic->destroyObject(farm);
	fx.logic->processDestroyList();
	CHECK(alice->commandPoints().getBonus() == 0);
}

TEST_CASE("P-86 command point bonus: a template without a CommandPointBonus is not recounted and gets no bonus from the list (RW 0x68F217)")
{
	P86World fx;
	Player *alice = fx.player("Alice");
	REQUIRE(alice != nullptr);

	// RW 0x6AA56D: a template whose CommandPointBonus is 0 adds usage, never a bonus, whatever its modifier sum
	Object *hut = fx.make("P86Hut", fx.teamOf("Alice"));
	REQUIRE(hut->addAttributeModifier("P86Bonus50", -1));
	CHECK(alice->commandPoints().getBonus() == 0);
	CHECK(alice->commandPoints().getUsage() == 0);

	Object *soldier = fx.make("P86Soldier", fx.teamOf("Alice"));
	CHECK(alice->commandPoints().getUsage() == 6);
	REQUIRE(soldier->addAttributeModifier("P86Bonus50", -1));
	CHECK(alice->commandPoints().getBonus() == 0);
	CHECK(alice->commandPoints().getUsage() == 6);
	soldier->removeAttributeModifier("P86Bonus50");
	CHECK(alice->commandPoints().getUsage() == 6);
}
