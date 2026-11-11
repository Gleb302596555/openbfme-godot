// OpenBFME unit tests (lane PORT-130, issue #332): CommandButton::AffectsKindOf — which objects a command
// button applies to. Synthetic INI, no retail files. Expectations come from the RotWK instructions and from
// the Ennorath mod's values (field-audit v7 row 573, data/ini/commandbutton.ini):
//   * RW 0x942996 `test ah,8`: only Options = NEEDS_CASTLE_KINDOF (bit 11, 0x800) reads the mask;
//   * RW 0x9429E9 `add eax,0x110` / 0x942A08 `rep movsd`: the button's 28-byte KindOf mask by value;
//   * RW 0x942348 -> 0x70C548 -> 0x661359: an ANY intersection of seven dwords, over the castle's objects
//     (RW 0x7999E2 / CastleBehavior::forEachMemberId);
//   * RW 0x942A17 -> 0x942C5D `push 3; pop eax`: no match hides the button (COMMAND_HIDDEN).
// DONOR BFME2 0x53B946 (ControlBarCommand.cpp:446) is the same callback, ControlBarRappellerCount.cpp:525
// the same option gate. Full analysis: docs/ennorath/PORT130_RW_ANALYSIS.md.

#include "doctest.h"

#include "ProdTestUtil.h"

#include "GameClient/ControlBar.h"
#include "GameClient/ControlBarCommands.h"
#include "GameLogic/Module/CastleModules.h"
#include "GameLogic/Object/Object.h"

#include <string>
#include <vector>

namespace
{
// A castle, its member pieces and the objects Ennorath's masks name: STRUCTURE, the wall KindOfs,
// HERO MONSTER PORTER, and a plain INFANTRY as the negative case.
const char *const kObjects =
	"Object P130_Castle\n"
	"  KindOf = STRUCTURE IMMOBILE BASE_FOUNDATION\n"
	"  Behavior = CastleBehavior ModuleTag_castle\n"
	"  End\n"
	"End\n"
	"Object P130_StructureMember\n"
	"  KindOf = STRUCTURE\n"
	"  Behavior = CastleMemberBehavior ModuleTag_cm\n"
	"  End\n"
	"End\n"
	"Object P130_Wall\n"
	"  KindOf = WALK_ON_TOP_OF_WALL DEFENSIVE_WALL\n"
	"  Behavior = CastleMemberBehavior ModuleTag_cm\n"
	"  End\n"
	"End\n"
	"Object P130_Hero\n"
	"  KindOf = HERO MONSTER PORTER\n"
	"  Behavior = CastleMemberBehavior ModuleTag_cm\n"
	"  End\n"
	"End\n"
	"Object P130_Infantry\n"
	"  KindOf = INFANTRY\n"
	"  Behavior = CastleMemberBehavior ModuleTag_cm\n"
	"  End\n"
	"End\n"
	"Object P130_NoMember\n"
	"  KindOf = STRUCTURE\n"
	"End\n"
	"Object P130_NoBehaviour\n"
	"  KindOf = STRUCTURE IMMOBILE\n"
	"End\n";

// Command = NONE has no command-specific availability rule in RW 0x942733, so the castle gate is the
// only thing that can hide it. The masks are Ennorath's own values for the stone-maker and Balrog buttons.
const char *const kButtons =
	"CommandButton P130_Structure\n"
	"  Command = NONE\n"
	"  Options = NEEDS_CASTLE_KINDOF\n"
	"  AffectsKindOf = STRUCTURE\n"
	"End\n"
	"CommandButton P130_StoneMaker\n"
	"  Command = NONE\n"
	"  Options = NEEDS_CASTLE_KINDOF\n"
	"  AffectsKindOf = WALK_ON_TOP_OF_WALL DEFENSIVE_WALL\n"
	"End\n"
	"CommandButton P130_Balrog\n"
	"  Command = NONE\n"
	"  Options = NEEDS_CASTLE_KINDOF\n"
	"  AffectsKindOf = STRUCTURE HERO MONSTER PORTER\n"
	"End\n"
	"CommandButton P130_Plain\n"
	"  Command = NONE\n"
	"  AffectsKindOf = STRUCTURE\n"
	"End\n"
	"CommandButton P130_Empty\n"
	"  Command = NONE\n"
	"  Options = NEEDS_CASTLE_KINDOF\n"
	"End\n";

struct CastleFixture
{
	prodtest::ProdWorld f;
	CastleFixture()
	{
		f.load(kObjects);
		f.load(kButtons);
	}
	Object *make(const char *name) { return f.make(name, f.teamOf("Alice")); }
	const CommandButton *button(const char *name)
	{
		const CommandButton *b = f.commands.findCommandButton(name);
		REQUIRE(b != nullptr);
		return b;
	}
	// A fresh castle owning the named pieces; `selected` is its first member (it carries CastleMemberBehavior).
	void castleWith(const std::vector<const char *> &pieces, Object **selected)
	{
		Object *castle = make("P130_Castle");
		REQUIRE(castle != nullptr);
		CastleBehavior *behaviour = CastleBehavior::of(*castle);
		REQUIRE(behaviour != nullptr);
		*selected = nullptr;
		for (const char *piece : pieces)
		{
			Object *o = make(piece);
			REQUIRE(o != nullptr);
			behaviour->registerOwnedObject(*o); // RW 0x79AC19: sets the member's castle id and the castle's lists
			if (!*selected)
			{
				*selected = o;
			}
		}
	}
};
} // namespace

TEST_CASE("PORT-130 AffectsKindOf STRUCTURE: a castle owning a STRUCTURE piece accepts the button, an INFANTRY-only castle hides it")
{
	CastleFixture fx;
	Object *structure = nullptr;
	fx.castleWith({ "P130_StructureMember" }, &structure);
	REQUIRE(structure != nullptr);
	const CommandButton *b = fx.button("P130_Structure");
	// RW 0x942996 `test ah,8`: the option really is bit 11 (0x800) of the button's Options.
	CHECK(b->hasOption(COMMAND_OPTION_NEEDS_CASTLE_KINDOF));
	CHECK(b->m_affectsKindOf != KindOfMaskType{}); // the INI mask parsed at all
	// RW 0x94235B sets matched, RW 0x942A17 falls through to the command's own rule.
	CHECK(ControlBar::castleKindOfAllows(*b, *structure));

	Object *infantry = nullptr;
	fx.castleWith({ "P130_Infantry" }, &infantry);
	REQUIRE(infantry != nullptr);
	// RW 0x942A1B -> RW 0x942C5D (availability 3, HIDDEN).
	CHECK_FALSE(ControlBar::castleKindOfAllows(*b, *infantry));
}

TEST_CASE("PORT-130 adjacent behaviour: no NEEDS_CASTLE_KINDOF option, no member module, no CastleBehavior and a missing castle object")
{
	CastleFixture fx;
	Object *infantry = nullptr;
	fx.castleWith({ "P130_Infantry" }, &infantry);
	REQUIRE(infantry != nullptr);
	const CommandButton *b = fx.button("P130_Structure");
	// RW 0x942999 `je 0x942a24`: a button without the option never reads the mask, whatever the castle holds.
	const CommandButton *plain = fx.button("P130_Plain");
	CHECK_FALSE(plain->hasOption(COMMAND_OPTION_NEEDS_CASTLE_KINDOF));
	CHECK(ControlBar::castleKindOfAllows(*plain, *infantry));

	// RW 0x9429A8 `je 0x942a24`: an object with no CastleMemberBehavior keeps its availability.
	Object *noMember = fx.make("P130_NoMember");
	REQUIRE(noMember != nullptr);
	CHECK(ControlBar::castleKindOfAllows(*b, *noMember));

	// RW 0x9429D4 `je 0x942a24`: the castle object exists but has no CastleBehavior module.
	Object *bare = fx.make("P130_NoBehaviour");
	REQUIRE(bare != nullptr);
	CastleMemberBehavior *cm = dynamic_cast<CastleMemberBehavior *>(infantry->findModule("CastleMemberBehavior"));
	REQUIRE(cm != nullptr);
	cm->setCastleId(bare->getID());
	CHECK(ControlBar::castleKindOfAllows(*b, *infantry));

	// RW 0x9429BC `je 0x942c5d`: the member's castle id no longer resolves -> HIDDEN.
	cm->setCastleId(INVALID_ID);
	CHECK_FALSE(ControlBar::castleKindOfAllows(*b, *infantry));
}

TEST_CASE("PORT-130 Ennorath's multi-name masks are ANY intersections over every castle object")
{
	CastleFixture fx;
	Object *wall = nullptr;
	fx.castleWith({ "P130_Wall" }, &wall);
	REQUIRE(wall != nullptr);
	const CommandButton *stone = fx.button("P130_StoneMaker");
	const CommandButton *balrog = fx.button("P130_Balrog");
	const CommandButton *empty = fx.button("P130_Empty");
	// Command_ConstructGondorStoneMaker / _ArnorStoneMaker / _PorterConstructMenStoneMaker.
	CHECK(ControlBar::castleKindOfAllows(*stone, *wall));
	CHECK_FALSE(ControlBar::castleKindOfAllows(*balrog, *wall));
	// Command_SpecialAbilityMordorBalrogWhipAI's mask: HERO MONSTER PORTER is enough, no STRUCTURE needed.
	Object *hero = nullptr;
	fx.castleWith({ "P130_Hero" }, &hero);
	REQUIRE(hero != nullptr);
	CHECK(ControlBar::castleKindOfAllows(*balrog, *hero));
	CHECK_FALSE(ControlBar::castleKindOfAllows(*stone, *hero));
	// An unset mask (CommandButton's constructor RW 0x75D723 zeroes 0x1C bytes) matches nothing.
	CHECK_FALSE(ControlBar::castleKindOfAllows(*empty, *hero));
	// RW 0x7999E2 keeps walking after a non-matching piece: the wall is the second member here.
	Object *first = nullptr;
	fx.castleWith({ "P130_Infantry", "P130_Wall" }, &first);
	REQUIRE(first != nullptr);
	CHECK(ControlBar::castleKindOfAllows(*stone, *first));
	CHECK_FALSE(ControlBar::castleKindOfAllows(*balrog, *first));
}
