// OpenBFME unit tests: Port 211 (fork issue 493) — the stale stop sweep of the economy group S-250 .. S-260.
//
// Two rows of the group were still open in code and are ported here (the rest of the group was done in main and its rows are closed in
// docs/STOPS.md):
//   * S-253's BOUNTY_PERCENTAGE sum. RW 0x6AC0B5 .. 0x6AC0DB reads the KILLER's attribute modifier sum of type 0x11 through RW 0x68C818 and uses it
//     instead of the player's own percent (RW 0x6AC0D3, Player + 0x334) unless it is exactly 0.0. RW 0x68C818 carries no type test, but the
//     economy's sum seam answered only COMMAND_POINT_BONUS, so every modifier of that type was dropped.
//   * S-253's model condition tests. AutoDepositUpdate asks the object's own flags — RW 0x89DBA9 bit 10 (GARRISONED) through RW 0x46E918 and
//     RW 0x89DBF0 .. 0x89DC25 the bits 5 / 59 / 60 (RUBBLE, POST_RUBBLE, POST_COLLAPSE) of the same array at Object + 0x10C — and the economy's
//     model condition provider was never installed, so a rubble keep kept paying.
//
// The expected numbers come from the RotWK rules cited above and the synthetic INI below, never from this engine's own output.

#include "doctest.h"

#include "EconTestUtil.h"

#include "Common/INI.h"
#include "Common/NumericState.h"
#include "Common/Player.h"
#include "GameLogic/AttributeModifiers.h"
#include "GameLogic/Construction.h"
#include "GameLogic/Economy.h"
#include "GameLogic/Object/Object.h"

#include <cstdint>

using namespace logictest;
using namespace econtest;

namespace
{
// Ennorath uses the same list shape for its outlaw leadership (data/ini/attributemodifier.ini: LorganPassiveOutlawLeadership,
// EomerPassiveOutlawLeadership, LurtzPassiveOutlawLeadership, SpellBookScavenger — BOUNTY_PERCENTAGE 80 .. 100% over Category WEAPON)
const char kBountyLists[] =
	"ModifierList TestOutlawBounty\n"
	"  Category = WEAPON\n"
	"  Modifier = BOUNTY_PERCENTAGE 80.0%\n"
	"  Duration = 2500\n"
	"End\n"
	"ModifierList TestCommandBonus\n"
	"  Category = WEAPON\n"
	"  Modifier = COMMAND_POINT_BONUS 30.0%\n"
	"  Duration = 2500\n"
	"End\n";

const char kBountyObjects[] =
	"Object Grunt\n"
	"  KindOf = SELECTABLE INFANTRY\n"
	"  BountyValue = 100\n"
	"End\n"
	"Object Killer\n"
	"  KindOf = SELECTABLE INFANTRY\n"
	"  Behavior = AttributeModifierPoolUpdate PoolTag\n"
	"  End\n"
	"End\n"
	"Object BareKiller\n"
	"  KindOf = SELECTABLE INFANTRY\n"
	"End\n";

const char kDepositObjects[] =
	"Object Keep\n"
	"  KindOf = SELECTABLE STRUCTURE\n"
	"  Behavior = AutoDepositUpdate DepositTag\n"
	"    DepositTiming = 6000\n"
	"    DepositAmount = 25\n"
	"  End\n"
	"End\n"
	"Object GarrisonKeep\n"
	"  KindOf = SELECTABLE STRUCTURE\n"
	"  Behavior = AutoDepositUpdate DepositTag\n"
	"    DepositTiming = 6000\n"
	"    DepositAmount = 25\n"
	"    OnlyWhenGarrisoned = Yes\n"
	"  End\n"
	"End\n";

// econtest::Fx's registry has no ModifierList block: the retail loader registers it in RetailObjectWorld and the combat lanes register it by
// hand. The base fixture is built from the object INI alone, then the store is installed and the ModifierList blocks are loaded over it, so
// the AttributeModifierPoolUpdate modules read the lists from this world's store (RW 0x6146E0's lookup), exactly as test_hero2_core.cpp and
// test_port_113.cpp do for their worlds.
struct BountyFx : econtest::Fx
{
	AttributeModifierStore modifiers;
	AttributeModifierStore *savedModifiers = TheAttributeModifierStore;
	explicit BountyFx(const char *lists, const char *objects)
		: Fx(objects)
	{
		TheAttributeModifierStore = &modifiers;
		modifiers.registerBlock(w.fx.env.blocks);
		const std::string err = w.load(lists, INI_LOAD_OVERWRITE, "modifiers.ini");
		REQUIRE_MESSAGE(err.empty(), err);
	}
	~BountyFx() { TheAttributeModifierStore = savedModifiers; }
};
// RW 0x42EE10 (scanPercentToReal): a "80.0%" token is scanReal * 0.01f, so the stored modifier is not the float literal 0.8 (the binary's own rounding)
float percent(float v)
{
	return NumericState::pc24Mul(v, 0.01f);
}
} // namespace

// RW 0x6AC0B5 .. 0x6AC0DB: the killer's BOUNTY_PERCENTAGE sum replaces the player's own percent; RW 0x68C818 answers any type, so the seam does too
TEST_CASE("PORT-211: the killer's BOUNTY_PERCENTAGE modifier pays the bounty (RW 0x6AC0B5 .. 0x6AC0DB)")
{
	BountyFx fx(kBountyLists, kBountyObjects);
	Player *alice = fx.player("Alice");
	Player *bob = fx.player("Bob");
	Object *killer = fx.makeAt("Killer", "Alice", 10.0f, 10.0f);
	Object *grunt = fx.makeAt("Grunt", "Bob", 20.0f, 10.0f);
	REQUIRE(killer->addAttributeModifier("TestOutlawBounty", -1));
	fx.frames(1); // RW 0x804D27: a category is suppressed until its "disabled until" frame, which is 0 at the start
	CHECK(fx.economy().attributeModifierSum(*killer, Economy::ATTRIBUTE_MODIFIER_BOUNTY_PERCENTAGE) == percent(80.0f));
	const std::uint32_t cash = alice->getMoney()->countMoney();
	CHECK(fx.economy().awardBounty(*alice, killer, *grunt) == 80); // ceil(100 * 0.8), RW 0x6AC0E3 .. 0x6AC109
	CHECK(alice->getMoney()->countMoney() == cash + 80);
	CHECK(alice->getScoreKeeper().moneyEarned() == 80); // ScoreKeeper + 4 / + 0x114 (the class of RW 0x79DFFA): the keeper counts the money the bounty paid
	CHECK(bob->getMoney()->countMoney() == fx.player("Bob")->getMoney()->countMoney());
	// the list expires on its own Duration (2500 frames here): the killer keeps the sum while it is active
	CHECK(fx.economy().awardBounty(*alice, killer, *grunt) == 80);
	// the removal puts the player's own percent back (RW 0x6AC0D3)
	killer->removeAttributeModifier("TestOutlawBounty");
	fx.frames(1);
	alice->setBountyPercent(0.5f);
	CHECK(fx.economy().awardBounty(*alice, killer, *grunt) == 50);
}

// RW 0x6AC0D1 .. 0x6AC0DB: a sum that is exactly 0.0 takes the player's own percent — an object with no pool at all (RW 0x68C821) and a pool
// without the list both answer 0.0
TEST_CASE("PORT-211: a killer without the modifier pays the player's own bounty percent (RW 0x6AC0D3)")
{
	BountyFx fx(kBountyLists, kBountyObjects);
	Player *alice = fx.player("Alice");
	Object *bare = fx.makeAt("BareKiller", "Alice", 10.0f, 10.0f);
	Object *killer = fx.makeAt("Killer", "Alice", 30.0f, 10.0f);
	Object *grunt = fx.makeAt("Grunt", "Bob", 20.0f, 10.0f);
	CHECK(fx.economy().attributeModifierSum(*bare, Economy::ATTRIBUTE_MODIFIER_BOUNTY_PERCENTAGE) == 0.0f);
	CHECK(fx.economy().attributeModifierSum(*killer, Economy::ATTRIBUTE_MODIFIER_BOUNTY_PERCENTAGE) == 0.0f);
	CHECK(alice->getBountyPercent() == 0.0f); // RW 0x6B0346
	CHECK(fx.economy().awardBounty(*alice, bare, *grunt) == 0);
	alice->setBountyPercent(0.5f);
	CHECK(fx.economy().awardBounty(*alice, bare, *grunt) == 50);
	CHECK(fx.economy().awardBounty(*alice, killer, *grunt) == 50);
	// with the list on the same object the modifier wins (RW 0x6AC0D1 keeps the sum)
	REQUIRE(killer->addAttributeModifier("TestOutlawBounty", -1));
	fx.frames(1);
	CHECK(fx.economy().awardBounty(*alice, killer, *grunt) == 80);
}

// RW 0x6A7C21 (P-86): the same seam still answers COMMAND_POINT_BONUS (0x18) from the object's pool, and no other type than the one asked
TEST_CASE("PORT-211: the sum seam still answers COMMAND_POINT_BONUS (RW 0x6A7C21)")
{
	BountyFx fx(kBountyLists, kBountyObjects);
	Object *killer = fx.makeAt("Killer", "Alice", 10.0f, 10.0f);
	REQUIRE(killer->addAttributeModifier("TestCommandBonus", -1));
	fx.frames(1);
	CHECK(fx.economy().attributeModifierSum(*killer, Economy::ATTRIBUTE_MODIFIER_COMMAND_POINT_BONUS) == percent(30.0f));
	CHECK(fx.economy().attributeModifierSum(*killer, Economy::ATTRIBUTE_MODIFIER_BOUNTY_PERCENTAGE) == 0.0f);
	CHECK(fx.economy().attributeModifierSum(*killer, Economy::ATTRIBUTE_MODIFIER_PRODUCTION) == 0.0f);
}

// RW 0x89DBA9 .. 0x89DBB2 (bit 10 = GARRISONED) and RW 0x89DBF0 .. 0x89DC25 (bits 5 / 59 / 60 = RUBBLE, POST_RUBBLE, POST_COLLAPSE) read the object's
// own model condition flags (RW 0x46E918: and eax, [esi + edx*4 + 0x10C]) and stop the deposit
TEST_CASE("PORT-211: a keep in RUBBLE / POST_RUBBLE / POST_COLLAPSE stops paying and GARRISONED gates it (RW 0x89DBA9 / 0x89DBF0)")
{
	Fx fx(kDepositObjects);
	Player *alice = fx.player("Alice");
	Object *keep = fx.makeAt("Keep", "Alice", 100.0f, 100.0f);
	Object *garrison = fx.makeAt("GarrisonKeep", "Alice", 120.0f, 100.0f);
	const std::uint32_t start = alice->getMoney()->countMoney();
	fx.frames(30); // DepositTiming 6000 ms = 30 frames
	CHECK(alice->getMoney()->countMoney() == start + 25); // the garrison keep pays nothing yet
	for (const char *c : { "RUBBLE", "POST_RUBBLE", "POST_COLLAPSE" })
	{
		REQUIRE(keep->testModelCondition(Construction::modelConditionIndex(c)) == false);
		keep->setModelConditionState(Construction::modelConditionIndex(c), true);
		const std::uint32_t before = alice->getMoney()->countMoney();
		fx.frames(30);
		CHECK_MESSAGE(alice->getMoney()->countMoney() == before, c); // the plain keep stopped, the garrison keep never paid
		keep->setModelConditionState(Construction::modelConditionIndex(c), false);
	}
	// GARRISONED: the gate opens and closes with the flag
	garrison->setModelConditionState(Construction::modelConditionIndex("GARRISONED"), true);
	const std::uint32_t open = alice->getMoney()->countMoney();
	fx.frames(30);
	CHECK(alice->getMoney()->countMoney() == open + 50); // both keeps pay
	garrison->setModelConditionState(Construction::modelConditionIndex("GARRISONED"), false);
	const std::uint32_t shut = alice->getMoney()->countMoney();
	fx.frames(30);
	CHECK(alice->getMoney()->countMoney() == shut + 25); // only the plain keep
}
