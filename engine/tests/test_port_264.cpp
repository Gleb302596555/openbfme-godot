// OpenBFME unit tests. GPL-3.0.
// PORT-264 (fork issue 612): AIUpdateInterface's own uncited functions. This lane covers the AI's state queries and the guard getters, which every Ennorath
// object with an AIUpdateInterface runs. Synthetic INI only (no retail files); every expectation is cited to a RotWK 2.01 game.dat instruction.

#include "doctest.h"
#include "CombatTestUtil.h"

#include "GameLogic/AI/AIStateMachine.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object/Object.h"

using namespace combattest;

namespace
{
// a lone infantryman: nothing but the machinery these queries need
const char kPort264Objects[] =
	"Object Port264Unit\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  VisionRange = 300\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 8\n"
	"  GeometryMinorRadius = 8\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 60\n"
	"  End\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"    AutoAcquireEnemiesWhenIdle = No\n"
	"  End\n"
	"  LocomotorSet\n"
	"    Locomotor = WalkerLoco\n"
	"    Condition = SET_NORMAL\n"
	"    Speed = 55\n"
	"  End\n"
	"End\n";

struct Port264World : CombatWorld
{
	Port264World() : CombatWorld(kPort264Objects) { combat().setAutoAcquireEnabled(false); }
};
} // namespace

// RW 0x00662B50 (AI vslot 0x1C4) reads the machine (AI + 0x30) and tail-calls its vslot 0x30 (RW 0x741724), which answers the TEMPORARY state's slot 0x28
// first (RW 0x8BD372 is true for AIBusyState) and the current state's otherwise (RW 0x8DB6BD); with neither it is false. RW 0x0066281A (AI vslot 0x16C,
// isBusy) asks only the current state's slot 0x38. A temporary busy state therefore answers true to the first and false to the second.
TEST_CASE("PORT-264 AI vslot 0x1C4 counts the temporary busy state, isBusy (vslot 0x16C) only the current one (RW 0x00662B50, RW 0x741724, RW 0x0066281A)")
{
	Port264World w;
	Object *unit = w.unit("Port264Unit", 'A', 300, 300);
	REQUIRE(unit != nullptr);
	AIUpdateInterface *ai = unit->getAIUpdateInterface();
	REQUIRE(ai != nullptr);
	AIStateMachine &m = ai->stateMachine();

	CHECK(m.currentStateId() == (unsigned)AI_IDLE);
	CHECK_FALSE(m.inTemporaryState());
	CHECK_FALSE(ai->isBusyState());	// RW 0x00662B50: no temporary state, the idle state answers false (RW 0x9188EB)
	CHECK_FALSE(ai->isBusy());		// RW 0x0066281A

	// RW 0x74168E: the temporary state is the one AI commands run on top of the machine (AI command 0x53's burning death, RW 0x8C3EC3); AI_BUSY is the
	// state whose slot 0x28 answers true (RW 0x8BD372 in its vtable RW 0xC274F8)
	m.setTemporaryStateRW(AI_BUSY, 100);
	REQUIRE(m.temporaryStateId() == (unsigned)AI_BUSY);
	CHECK(ai->isBusyState());	// RW 0x00662B50 -> RW 0x741724 -> the temporary state's slot 0x28 (RW 0x8BD372, true)
	CHECK_FALSE(ai->isBusy());	// RW 0x0066281A still asks the current state, which is AI_IDLE

	m.clearTemporaryState();	// RW 0x751DA9
	CHECK_FALSE(ai->isBusyState());
	CHECK(m.currentStateId() == (unsigned)AI_IDLE);
}

// Neighbour: RW 0x662A21 (guardHere) writes the guard mode to AI + 0x4C, the location to AI + 0x58 and puts the machine into AI_GUARD. The getters the lane
// cited — AI vslot 0x7D (RW 0x00670317, [AI + 0x4C]), AI vslot 0x76 (RW 0x008F1161, lea [AI + 0x58]) and AI vslot 0x7B (RW 0x0049A4D0, [AI + 0x54]) —
// answer exactly those fields, so a guard order is visible in them.
TEST_CASE("PORT-264 the guard getters answer the fields guardHere wrote (RW 0x00670317, RW 0x008F1161, RW 0x0049A4D0)")
{
	Port264World w;
	Object *unit = w.unit("Port264Unit", 'A', 300, 300);
	REQUIRE(unit != nullptr);
	AIUpdateInterface *ai = unit->getAIUpdateInterface();
	REQUIRE(ai != nullptr);

	CHECK(ai->guardMode() == 0);
	ai->guardHere(1);											// RW 0x662A21: hold ground where the unit stands
	CHECK(ai->guardMode() == 1);								// RW 0x00670317 ([AI + 0x4C])
	// RW 0x662A21 copies the unit's own position into AI + 0x58; the unit stands at exactly (300, 300)
	CHECK(ai->guardLocation().x == 300.0f);					// RW 0x008F1161 ([AI + 0x58])
	CHECK(ai->guardLocation().y == 300.0f);
	CHECK(ai->guardTargetType() == 0);						// RW 0x0049A4D0 ([AI + 0x54]): GUARDTARGET_LOCATION, which 0x662A21 sets unless AREA
	CHECK(ai->stateMachine().currentStateId() == 16u);			// RW 0x662A21: the machine goes to AI_GUARD
}
