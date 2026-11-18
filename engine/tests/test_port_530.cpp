// OpenBFME unit tests, lane PORT-530 (Local 72, fork issue 530): the button matrix meets each grey button's conditions
// and presses again.
//
// The sweep (engine/tests/test_hud_factb_units.cpp unitsSweep) presses every button; a grey one is either a
// requirement the sweep does not meet or a port bug. Port 220 (fork issue 529) writes docs/ennorath/MATRIX_GREY.tsv
// (faction, object, button, condition, value, satisfied-by, note); the sweep parses it with engine/tests/GreyFixture.h
// and gives each fixable condition (upgrade, science, object, model, level) before pressing again. A button still grey
// with every condition met is GREY-BUG (the port's availability rule differs from RotWK's
// ControlBar::getCommandAvailability, RW 0x942733); a condition the sweep cannot give (option, other, none) keeps
// DISABLED: needs ... These tests cover the parsing, the classification, the verdicts and the fixture plan on a
// synthetic table: no game files, no mod data.

#include "doctest.h"

#include "GreyFixture.h"

#include <string>

namespace
{
using greyfix::Action;
using greyfix::Kind;
using greyfix::Row;
using greyfix::Table;

const char *kTable =
	"# a comment line\n"
	"faction\tobject\tbutton\tcondition\tvalue\tsatisfied-by\tnote\n"
	"FactionTest\tTestUnit\tCommand_UpgradeBtn\tupgrade\tUpgrade_TestPlayer\tTestArmoury\t\n"
	"FactionTest\tTestUnit\tCommand_ScienceBtn\tscience\tScience_Test\t-\t\n"
	"FactionTest\tTestUnit\tCommand_ObjectBtn\tobject\tTestArmoury\t-\t\n"
	"FactionTest\tTestUnit\tCommand_ModelBtn\tmodel\tWEAPONSET_TOGGLE_1\tCommand_ToggleWeapon\t\n"
	"FactionTest\tTestUnit\tCommand_LevelBtn\tlevel\t5\t-\t\n"
	"FactionTest\tTestUnit\tCommand_OptionBtn\toption\tNEED_UPGRADE\t-\t\n"
	"FactionTest\tTestUnit\tCommand_NoneBtn\tnone\t-\t-\tno gating field in the data\n"
	"FactionTest\tTestUnit\tCommand_MultiBtn\tupgrade\tUpgrade_TestPlayer\tTestArmoury\t\n"
	"FactionTest\tTestUnit\tCommand_MultiBtn\tlevel\t3\t-\t\n"
	"FactionTest\tTestUnit\tCommand_MultiBtn\tmodel\tMOUNTED\tCommand_ToggleMounted\t\n";

Row rowOf(const Table &t, const std::string &button)
{
	const Row *r = greyfix::findRow(t, "TestUnit", button);
	REQUIRE(r != nullptr);
	return *r;
}
} // namespace

TEST_CASE("PORT-530: the grey table parses one row per button, conditions in table order")
{
	const Table t = greyfix::parseGreyTable(kTable);
	CHECK(t.size() == 8);
	const Row multi = rowOf(t, "Command_MultiBtn");
	REQUIRE(multi.conditions.size() == 3);
	CHECK(multi.conditions[0].condition == "upgrade");
	CHECK(multi.conditions[0].value == "Upgrade_TestPlayer");
	CHECK(multi.conditions[0].satisfiedBy == "TestArmoury");
	CHECK(multi.conditions[1].condition == "level");
	CHECK(multi.conditions[1].value == "3");
	CHECK(multi.conditions[2].condition == "model");
	CHECK(multi.conditions[2].satisfiedBy == "Command_ToggleMounted");
	CHECK(multi.faction == "FactionTest");
	CHECK(multi.object == "TestUnit");
	CHECK(multi.button == "Command_MultiBtn");
	// a button with no row in the table: no fixture, the sweep keeps its old verdict
	CHECK(greyfix::findRow(t, "TestUnit", "Command_UnknownBtn") == nullptr);
	CHECK(greyfix::findRow(t, "OtherUnit", "Command_UpgradeBtn") == nullptr);
}

TEST_CASE("PORT-530: upgrade, science, object, model and level are fixable; option, other and none are not faked")
{
	const Table t = greyfix::parseGreyTable(kTable);
	CHECK(greyfix::kindOf("upgrade") == Kind::Upgrade);
	CHECK(greyfix::kindOf("science") == Kind::Science);
	CHECK(greyfix::kindOf("object") == Kind::Object);
	CHECK(greyfix::kindOf("model") == Kind::Model);
	CHECK(greyfix::kindOf("level") == Kind::Level);
	CHECK(greyfix::kindOf("option") == Kind::Unfixable);
	CHECK(greyfix::kindOf("other") == Kind::Unfixable);
	CHECK(greyfix::kindOf("none") == Kind::Unfixable);
	CHECK(greyfix::kindOf("bogus") == Kind::Unfixable);
	CHECK(greyfix::fixable(rowOf(t, "Command_UpgradeBtn")));
	CHECK(greyfix::fixable(rowOf(t, "Command_ScienceBtn")));
	CHECK(greyfix::fixable(rowOf(t, "Command_ObjectBtn")));
	CHECK(greyfix::fixable(rowOf(t, "Command_ModelBtn")));
	CHECK(greyfix::fixable(rowOf(t, "Command_LevelBtn")));
	CHECK(greyfix::fixable(rowOf(t, "Command_MultiBtn")));
	CHECK_FALSE(greyfix::fixable(rowOf(t, "Command_OptionBtn")));
	CHECK_FALSE(greyfix::fixable(rowOf(t, "Command_NoneBtn")));
	// a row that mixes a fixable and an unfixable condition is not faked either
	Row mixed;
	mixed.conditions.push_back(greyfix::Condition{ "upgrade", "Upgrade_TestPlayer", "-", "" });
	mixed.conditions.push_back(greyfix::Condition{ "option", "NEED_UPGRADE", "-", "" });
	CHECK_FALSE(greyfix::fixable(mixed));
	// no conditions: nothing the sweep can meet
	Row empty;
	CHECK_FALSE(greyfix::fixable(empty));
}

TEST_CASE("PORT-530: the verdicts name the first unmet condition, or GREY-BUG with every condition met")
{
	const Table t = greyfix::parseGreyTable(kTable);
	// a condition the sweep cannot give: the button keeps its grey verdict with the reason
	CHECK(greyfix::needsDetail(rowOf(t, "Command_OptionBtn")) == "DISABLED: needs option NEED_UPGRADE");
	CHECK(greyfix::needsDetail(rowOf(t, "Command_NoneBtn")) == "DISABLED: needs none -");
	// a button still grey with every condition met: the port's rule differs from RotWK's (RW 0x942733)
	CHECK(greyfix::bugDetail(rowOf(t, "Command_UpgradeBtn")) == "GREY-BUG: still restricted with upgrade Upgrade_TestPlayer met");
	CHECK(greyfix::bugDetail(rowOf(t, "Command_MultiBtn")) == "GREY-BUG: still restricted with upgrade Upgrade_TestPlayer, level 3, model MOUNTED met");
}

TEST_CASE("PORT-530: the fixture plan gives the object, the upgrades, the sciences, the level, and the model toggle last")
{
	const Table t = greyfix::parseGreyTable(kTable);
	const std::vector<Action> plan = greyfix::fixturePlan(rowOf(t, "Command_MultiBtn"));
	REQUIRE(plan.size() == 3);
	CHECK(plan[0].kind == Kind::Upgrade);
	CHECK(plan[0].value == "Upgrade_TestPlayer");
	CHECK(plan[1].kind == Kind::Level);
	CHECK(plan[1].value == "3");
	CHECK(plan[2].kind == Kind::Model);
	CHECK(plan[2].value == "MOUNTED");
	CHECK(plan[2].satisfiedBy == "Command_ToggleMounted");
	// an unfixable row plans nothing: the sweep does not fake option / other / none
	CHECK(greyfix::fixturePlan(rowOf(t, "Command_OptionBtn")).empty());
	CHECK(greyfix::fixturePlan(rowOf(t, "Command_NoneBtn")).empty());
	// a single-condition row plans exactly its one fixture
	const std::vector<Action> one = greyfix::fixturePlan(rowOf(t, "Command_ScienceBtn"));
	REQUIRE(one.size() == 1);
	CHECK(one[0].kind == Kind::Science);
	CHECK(one[0].value == "Science_Test");
}

// 2026-11-17 (matrix GREY-BUG review): the satisfied-by column carries a note after the button name, and "-" / "Object:ModuleTag" name no
// pressable button; a condition the fixture did not give reads as the grey verdict with its reason, never as GREY-BUG.
TEST_CASE("PORT-530: the model setter is the satisfied-by name without its note; '-' and a module tag name no button")
{
	CHECK(greyfix::setterButton("Command_GorkilToggleWeapon (weapon-set toggle)") == "Command_GorkilToggleWeapon");
	CHECK(greyfix::setterButton("Command_ToggleMounted") == "Command_ToggleMounted");
	CHECK(greyfix::setterButton("-").empty());
	CHECK(greyfix::setterButton("").empty());
	CHECK(greyfix::setterButton("RohanErkenbrand:ModuleTag_HorseToggle (mount replacement)").empty());
	greyfix::Condition c;
	c.condition = "model";
	c.value = "USER_1";
	CHECK(greyfix::unmetDetail(c, "the sweep could not set it: no known setter") ==
	      "DISABLED: needs model USER_1 (the sweep could not set it: no known setter)");
}
