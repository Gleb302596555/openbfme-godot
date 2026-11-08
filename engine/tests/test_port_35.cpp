// PORT-35 (issue #120, HordeContain/ComboHorde): two hordes combining into one, as RW 0x876481's combine branch (RW 0x8765B4 .. 0x8769F7) does it, with the
// entry lookup RW 0x87043B and the join test RW 0x871E89 .. 0x871F12 (interface slot 0x70). Synthetic data only: the expectations are the INI above (the
// ComboHorde entries) and the RW branch's shape (a new horde of the entry's Result, both old hordes gone, every member of both in the new one).
#include "doctest.h"
#include "CombatTestUtil.h"
#include "Common/Thing/ThingTemplate.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Object/Contain/HordeContainRuntime.h"
#include "GameLogic/Object/Object.h"

namespace {

// The caster's list has two entries (the archer horde and the lone swordsman horde), so the lookup must pick the entry by the target's template name
// (RW 0x87043B, the compare RW 0x4065AA) and take that entry's Result. TestComboBadResultHorde names a Result template the game does not have.
const char kComboObjects[] =
	"Object TestComboSwordHorde\n"
	" KindOf = HORDE MELEE_HORDE SELECTABLE\n"
	" Geometry = BOX\n GeometryMajorRadius = 30\n GeometryMinorRadius = 45\n GeometryHeight = 20\n"
	" Body = ImmortalBody BodyTag\n MaxHealth = 1\n End\n"
	" Behavior = HordeAIUpdate AITag\n End\n"
	" Behavior = HordeContain ContainTag\n"
	"  Slots = 2\n"
	"  RankInfo = RankNumber:1 UnitType:Swordsman Position:X:20 Y:0\n"
	"  RankInfo = RankNumber:2 UnitType:Swordsman Position:X:-20 Y:0\n"
	"  InitialPayload = Swordsman 2\n"
	"  ComboHorde = Target:TestComboArcherHorde Result:TestComboMixedHorde InitiateVoice:NoSound\n"
	"  ComboHorde = Target:TestComboLoneHorde Result:TestComboMixedHorde InitiateVoice:NoSound\n"
	" End\n"
	" LocomotorSet\n Locomotor = HordeLoco\n Condition = SET_NORMAL\n Speed = 50\n End\n"
	"End\n"
	"Object TestComboArcherHorde\n"
	" KindOf = HORDE SELECTABLE\n"
	" Geometry = BOX\n GeometryMajorRadius = 30\n GeometryMinorRadius = 45\n GeometryHeight = 20\n"
	" Body = ImmortalBody BodyTag\n MaxHealth = 1\n End\n"
	" Behavior = HordeAIUpdate AITag\n End\n"
	" Behavior = HordeContain ContainTag\n"
	"  Slots = 2\n"
	"  RankInfo = RankNumber:1 UnitType:Archer Position:X:20 Y:0\n"
	"  RankInfo = RankNumber:2 UnitType:Archer Position:X:-20 Y:0\n"
	"  InitialPayload = Archer 2\n"
	" End\n"
	" LocomotorSet\n Locomotor = HordeLoco\n Condition = SET_NORMAL\n Speed = 50\n End\n"
	"End\n"
	// the Result: it holds both unit types and starts empty (its slots are what the four members take)
	"Object TestComboMixedHorde\n"
	" KindOf = HORDE COMBO_HORDE MELEE_HORDE SELECTABLE\n"
	" Geometry = BOX\n GeometryMajorRadius = 30\n GeometryMinorRadius = 45\n GeometryHeight = 20\n"
	" Body = ImmortalBody BodyTag\n MaxHealth = 1\n End\n"
	" Behavior = HordeAIUpdate AITag\n End\n"
	" Behavior = HordeContain ContainTag\n"
	"  Slots = 4\n"
	"  RankInfo = RankNumber:1 UnitType:Swordsman Position:X:20 Y:0 Position:X:20 Y:20\n"
	"  RankInfo = RankNumber:2 UnitType:Archer Position:X:-20 Y:0 Position:X:-20 Y:20\n"
	" End\n"
	" LocomotorSet\n Locomotor = HordeLoco\n Condition = SET_NORMAL\n Speed = 50\n End\n"
	"End\n"
	"Object TestComboLoneHorde\n"
	" KindOf = HORDE SELECTABLE\n"
	" Geometry = BOX\n GeometryMajorRadius = 30\n GeometryMinorRadius = 45\n GeometryHeight = 20\n"
	" Body = ImmortalBody BodyTag\n MaxHealth = 1\n End\n"
	" Behavior = HordeAIUpdate AITag\n End\n"
	" Behavior = HordeContain ContainTag\n"
	"  Slots = 1\n"
	"  RankInfo = RankNumber:1 UnitType:Swordsman Position:X:20 Y:0\n"
	"  InitialPayload = Swordsman 1\n"
	" End\n"
	" LocomotorSet\n Locomotor = HordeLoco\n Condition = SET_NORMAL\n Speed = 50\n End\n"
	"End\n"
	"Object TestComboBadResultHorde\n"
	" KindOf = HORDE SELECTABLE\n"
	" Geometry = BOX\n GeometryMajorRadius = 30\n GeometryMinorRadius = 45\n GeometryHeight = 20\n"
	" Body = ImmortalBody BodyTag\n MaxHealth = 1\n End\n"
	" Behavior = HordeAIUpdate AITag\n End\n"
	" Behavior = HordeContain ContainTag\n"
	"  Slots = 2\n"
	"  RankInfo = RankNumber:1 UnitType:Swordsman Position:X:20 Y:0\n"
	"  InitialPayload = Swordsman 1\n"
	"  ComboHorde = Target:TestComboArcherHorde Result:TestComboNoSuchHorde InitiateVoice:NoSound\n"
	" End\n"
	" LocomotorSet\n Locomotor = HordeLoco\n Condition = SET_NORMAL\n Speed = 50\n End\n"
	"End\n";

struct ComboWorld : combattest::CombatWorld
{
	ComboWorld()
	{
		const std::string error = w.load(kComboObjects, INI_LOAD_OVERWRITE, "port35.ini");
		REQUIRE_MESSAGE(error.empty(), error);
	}
};

int countNamed(const std::vector<Object *> &members, const std::string &name)
{
	int n = 0;
	for (Object *m : members)
	{
		n += m->getTemplate()->getName() == name ? 1 : 0;
	}
	return n;
}

} // namespace

TEST_CASE("PORT-35 combine: two hordes with a ComboHorde entry become the Result horde and keep every member")
{
	ComboWorld w;
	Object *sword = w.unit("TestComboSwordHorde", 'A', 300, 300);
	Object *archer = w.unit("TestComboArcherHorde", 'A', 340, 300);
	REQUIRE(sword);
	REQUIRE(archer);
	w.frames(2);
	const std::vector<Object *> swordMembers = w.membersOf(sword);
	const std::vector<Object *> archerMembers = w.membersOf(archer);
	REQUIRE(swordMembers.size() == 2);
	REQUIRE(archerMembers.size() == 2);
	HordeContain *caster = w.hordeOf(sword);
	REQUIRE(caster);
	// RW 0x8765CC: the entry naming the archer horde's template, its Result (RW 0x6D1305) is TestComboMixedHorde
	Object *made = caster->combineWith(archer);
	REQUIRE(made);
	CHECK(made->getTemplate()->getName() == "TestComboMixedHorde");
	// RW 0x62BBAB at RW 0x8769A1 / RW 0x8769B9: both old hordes are destroyed, their members are not
	CHECK(sword->isDestroyed());
	CHECK(archer->isDestroyed());
	for (Object *m : swordMembers)
	{
		CAPTURE(m->getTemplate()->getName());
		CHECK_FALSE(m->isDestroyed());
		REQUIRE(m->getContainedBy());
		CHECK(m->getContainedBy() == made);
	}
	for (Object *m : archerMembers)
	{
		CAPTURE(m->getTemplate()->getName());
		CHECK_FALSE(m->isDestroyed());
		REQUIRE(m->getContainedBy());
		CHECK(m->getContainedBy() == made);
	}
	// RW 0x873F30 (slot 0x2C) for each member: the new horde holds both hordes' members
	const std::vector<Object *> all = w.membersOf(made);
	CHECK(all.size() == 4);
	CHECK(countNamed(all, "Swordsman") == 2);
	CHECK(countNamed(all, "Archer") == 2);
	CHECK(made->getContain()->getContainCount() == 4);
	// RW 0x68C17D: the new horde takes the caster's team
	CHECK(made->getTeam() == w.teamA());
	// RW 0x876A08's end: the member pass walks the members onto the Result horde's slots
	w.frames(2);
	CHECK(w.membersOf(made).size() == 4);
}

TEST_CASE("PORT-35 combine: a pair without an entry does not combine and nothing churns")
{
	ComboWorld w;
	Object *archer = w.unit("TestComboArcherHorde", 'A', 300, 300);
	Object *lone = w.unit("TestComboLoneHorde", 'A', 340, 300);
	REQUIRE(archer);
	REQUIRE(lone);
	w.frames(2);
	const std::vector<Object *> loneMembers = w.membersOf(lone);
	REQUIRE(loneMembers.size() == 1);
	HordeContain *caster = w.hordeOf(archer);
	REQUIRE(caster);
	// the archer horde's list is empty: RW 0x87043B finds no entry for the lone horde's template
	CHECK_FALSE(caster->canCombineWith(*lone));
	CHECK(caster->combineWith(lone) == nullptr);
	CHECK_FALSE(archer->isDestroyed());
	CHECK_FALSE(lone->isDestroyed());
	CHECK_FALSE(loneMembers.front()->isDestroyed());
	CHECK(loneMembers.front()->getContainedBy() == lone);
	CHECK(w.membersOf(lone).size() == 1);
	CHECK(w.membersOf(archer).size() == 2);
}

TEST_CASE("PORT-35 canCombineWith: the entry, the object itself and the two excluded statuses (RW 0x871E89 .. 0x871F12)")
{
	ComboWorld w;
	Object *sword = w.unit("TestComboSwordHorde", 'A', 300, 300);
	Object *archer = w.unit("TestComboArcherHorde", 'A', 340, 300);
	Object *lone = w.unit("TestComboLoneHorde", 'A', 380, 300);
	REQUIRE(sword);
	REQUIRE(archer);
	REQUIRE(lone);
	w.frames(2);
	HordeContain *caster = w.hordeOf(sword);
	REQUIRE(caster);
	CHECK(caster->canCombineWith(*archer));
	CHECK_FALSE(caster->canCombineWith(*sword)); // the horde itself
	CHECK_FALSE(caster->canCombineWith(*lone));  // no entry for that template
	// RW 0x871ECE .. 0x871F02: RW 0x44DDEC with 2 (UNDER_CONSTRUCTION) and 3 (UNSELECTABLE), on both objects
	const unsigned underConstruction = (unsigned)CombatNames::status("UNDER_CONSTRUCTION");
	const unsigned unselectable = (unsigned)CombatNames::status("UNSELECTABLE");
	archer->setStatus(underConstruction, true);
	CHECK_FALSE(caster->canCombineWith(*archer));
	archer->setStatus(underConstruction, false);
	CHECK(caster->canCombineWith(*archer));
	sword->setStatus(unselectable, true);
	CHECK_FALSE(caster->canCombineWith(*archer));
	sword->setStatus(unselectable, false);
	CHECK(caster->canCombineWith(*archer));
	// the prologue's 0x3b (DEPLOYED) on this horde's object and 0x3e (TEMPORARILY_DEFECTED) on either
	const unsigned deployed = (unsigned)CombatNames::status("DEPLOYED");
	const unsigned defected = (unsigned)CombatNames::status("TEMPORARILY_DEFECTED");
	sword->setStatus(deployed, true);
	CHECK_FALSE(caster->canCombineWith(*archer));
	sword->setStatus(deployed, false);
	CHECK(caster->canCombineWith(*archer));
	sword->setStatus(defected, true);
	CHECK_FALSE(caster->canCombineWith(*archer));
	sword->setStatus(defected, false);
	archer->setStatus(defected, true);
	CHECK_FALSE(caster->canCombineWith(*archer));
	archer->setStatus(defected, false);
	CHECK(caster->canCombineWith(*archer));
}

TEST_CASE("PORT-35 holdings: a member names its horde, an entry's Result must exist, several entries pick by target")
{
	ComboWorld w;
	Object *sword = w.unit("TestComboSwordHorde", 'A', 300, 300);
	Object *archer = w.unit("TestComboArcherHorde", 'A', 340, 300);
	Object *lone = w.unit("TestComboLoneHorde", 'A', 380, 300);
	Object *bad = w.unit("TestComboBadResultHorde", 'A', 420, 300);
	REQUIRE(sword);
	REQUIRE(archer);
	REQUIRE(lone);
	REQUIRE(bad);
	w.frames(2);
	HordeContain *caster = w.hordeOf(sword);
	REQUIRE(caster);
	// RW 0x8764AA: a target that is a member of a horde combines that horde (the second entry of the caster's list names the lone horde). The join test
	// RW 0x871EC1 tests the object it is handed, so it answers for the horde, not for the member (S-6027)
	const std::vector<Object *> loneMembers = w.membersOf(lone);
	REQUIRE(loneMembers.size() == 1);
	CHECK(caster->canCombineWith(*lone));
	Object *made = caster->combineWith(loneMembers.front());
	REQUIRE(made);
	CHECK(made->getTemplate()->getName() == "TestComboMixedHorde");
	CHECK(lone->isDestroyed());
	CHECK(sword->isDestroyed());
	CHECK_FALSE(loneMembers.front()->isDestroyed());
	CHECK(loneMembers.front()->getContainedBy() == made);
	CHECK(w.membersOf(made).size() == 3);
	// RW 0x876601: an entry whose Result template the game does not have makes no horde and destroys nothing
	HordeContain *badHorde = w.hordeOf(bad);
	REQUIRE(badHorde);
	const std::vector<Object *> badMembers = w.membersOf(bad);
	const std::vector<Object *> archerMembers = w.membersOf(archer);
	REQUIRE(badMembers.size() == 1);
	REQUIRE(archerMembers.size() == 2);
	// the entry's Target (TestComboArcherHorde) is found, its Result (TestComboNoSuchHorde) is not (RW 0x6D1305 returns null at RW 0x876601)
	CHECK(badHorde->combineWith(archer) == nullptr);
	CHECK_FALSE(bad->isDestroyed());
	CHECK_FALSE(archer->isDestroyed());
	CHECK_FALSE(badMembers.front()->isDestroyed());
	CHECK(badMembers.front()->getContainedBy() == bad);
	CHECK(w.membersOf(bad).size() == 1);
	// the archer horde is untouched by that attempt too
	CHECK(w.membersOf(archer).size() == 2);
	for (Object *m : archerMembers)
	{
		CHECK(m->getContainedBy() == archer);
	}
}
