// OpenBFME unit tests for Local 162 (fork issue 929): ScriptActions upper half by address (RW 0x7BF4A3..0x9726CC).
// Synthetic INI only; every expectation comes from RotWK 2.01 game.dat disassembly (MD5 daed3668006cd90f01c34e5a7da1901f).

#include "doctest.h"
#include "MoveTestUtil.h"
#include "ScriptTestUtil.h"

#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/SpecialPower.h"
#include "Common/Team.h"
#include "GameLogic/BitFlags.h"
#include "GameLogic/Combat/ObjectWeapons.h"
#include "GameLogic/ExperienceLevels.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Module/ActiveBody.h"
#include "GameLogic/Module/FlammableUpdate.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Object/ExperienceTracker.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/Object/PartitionManager.h"
#include "GameLogic/ScriptEngine/ScriptActions.h"
#include "GameLogic/ScriptEngine/ScriptConditions.h"
#include "GameLogic/ScriptEngine/ScriptEngine.h"

#include <cmath>
#include <string>
#include <vector>

using namespace scripttest;

namespace
{

void startEngine(GameLogic &logic)
{
	ScriptEngine::SideScripts side;
	side.sideName = "";
	static ScriptList empty;
	side.lists.push_back(&empty);
	logic.scriptEngine().newGame({ side }, nullptr, nullptr, false);
}

void run(ScriptEngine &e, const ScriptActionRec &a)
{
	ScriptActions::execute(e, a);
}

std::string qual(Team *t)
{
	return t->getControllingPlayer()->getPlayerName() + "/" + t->getName();
}

const char *kLocal162Powers =
	"SpecialPower SP_L162_Test\n"
	"  Enum = SPECIAL_GENERAL_TARGETLESS_TWO\n"
	"  ReloadTime = 10000\n"
	"End\n";

const char *kLocal162Levels =
	"ExperienceLevel L162_Lvl1\n"
	"  TargetNames = L162_Unit\n"
	"  RequiredExperience = 0\n"
	"  Rank = 1\n"
	"End\n"
	"ExperienceLevel L162_Lvl2\n"
	"  TargetNames = L162_Unit\n"
	"  RequiredExperience = 50\n"
	"  Rank = 2\n"
	"End\n"
	"ExperienceLevel L162_Lvl3\n"
	"  TargetNames = L162_Unit\n"
	"  RequiredExperience = 100\n"
	"  Rank = 3\n"
	"End\n";

const char *kLocal162Objects =
	"Object L162_Unit\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  CommandPoints = 5\n"
	"  Body = ActiveBody B\n"
	"    MaxHealth = 200\n"
	"  End\n"
	"  Behavior = AIUpdateInterface AI\n"
	"  End\n"
	"  Behavior = FlammableUpdate FU\n"
	"    FlameDamageLimit = 20\n"
	"    FlameDamageExpiration = 5000\n"
	"    AflameDuration = 5000\n"
	"    AflameDamageAmount = 1\n"
	"    AflameDamageDelay = 1000\n"
	"  End\n"
	"  Behavior = SpecialPowerModule SPM\n"
	"    SpecialPowerTemplate = SP_L162_Test\n"
	"  End\n"
	"End\n"
	"Object L162_Building\n"
	"  KindOf = STRUCTURE SELECTABLE GARRISONABLE_UNTIL_DESTROYED\n"
	"  Body = ActiveBody B\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = AIUpdateInterface AI\n"
	"  End\n"
	"  Behavior = GarrisonContain GC\n"
	"    ContainMax = 4\n"
	"  End\n"
	"End\n";

struct Local162Fixture
{
	movetest::MoveWorld w;
	ExperienceLevelSystem xpSys;
	ExperienceLevelSystem *prevXp = nullptr;
	SpecialPowerStore spStore;
	SpecialPowerStore *prevSp = nullptr;
	Player *p1 = nullptr;
	Player *p2 = nullptr;

	Local162Fixture() : w(30, 30)
	{
		prevXp = TheExperienceLevelSystem;
		TheExperienceLevelSystem = &xpSys;
		prevSp = TheSpecialPowerStore;
		TheSpecialPowerStore = &spStore;

		xpSys.registerBlocks(w.w.fx.env.blocks);
		w.w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
		REQUIRE(w.w.load(kLocal162Powers, INI_LOAD_OVERWRITE, "l162_powers.ini").empty());
		REQUIRE(w.w.load(kLocal162Levels, INI_LOAD_OVERWRITE, "l162_levels.ini").empty());
		REQUIRE(w.w.load(kLocal162Objects, INI_LOAD_OVERWRITE, "l162_objects.ini").empty());
		w.buildMap();

		p1 = w.players.findPlayerWithName("Player_1");
		p2 = w.players.findPlayerWithName("Player_2");
		if (!p1) p1 = w.players.getNthPlayer(0);
		if (!p2) p2 = w.players.getNthPlayer(1);
		REQUIRE(p1 != nullptr);
		REQUIRE(p2 != nullptr);
		startEngine(*w.logic);
	}

	~Local162Fixture()
	{
		TheExperienceLevelSystem = prevXp;
		TheSpecialPowerStore = prevSp;
	}

	Object *spawn(const char *tmpl, Player *p, const char *name, float x = 100.0f, float y = 100.0f)
	{
		Object *o = w.spawn(tmpl, x, y, 0.0f, p->getDefaultTeam());
		REQUIRE(o != nullptr);
		if (name)
		{
			o->setName(name);
		}
		return o;
	}
};

} // namespace

// RW 0x7C0556 (doTeamDelete): TEAM_DELETE (ignoreDead=0) destroys all; TEAM_DELETE_LIVING (ignoreDead=1, RW 0x7A4046) skips dead members.
TEST_CASE("LOCAL-162 ScriptActions: TEAM_DELETE_LIVING and DELETE_ALL_UNMANNED")
{
	Local162Fixture f;
	Team *tm = f.p1->getDefaultTeam();
	Object *alive = f.spawn("L162_Unit", f.p1, "u_alive");
	Object *dead = f.spawn("L162_Unit", f.p1, "u_dead", 120.0f, 100.0f);
	const ObjectID aliveId = alive->getID();
	const ObjectID deadId = dead->getID();
	dead->kill(DAMAGE_UNRESISTABLE, DEATH_NORMAL);
	REQUIRE(dead->isEffectivelyDead());

	run(f.w.logic->scriptEngine(), act("TEAM_DELETE_LIVING", { sp(3, qual(tm)) }));
	f.w.logic->runLogicFrame();
	CHECK(f.w.logic->findObjectByID(aliveId) == nullptr);
	CHECK(f.w.logic->findObjectByID(deadId) != nullptr);

	// RW 0x7C2A40 (DELETE_ALL_UNMANNED): destroys objects with DISABLED_UNMANNED (type 5, Object +0x1C8 bit 0x20)
	Object *manned = f.spawn("L162_Unit", f.p1, "u_manned", 140.0f, 100.0f);
	Object *unmanned = f.spawn("L162_Unit", f.p1, "u_unmanned", 160.0f, 100.0f);
	const ObjectID mannedId = manned->getID();
	const ObjectID unmannedId = unmanned->getID();
	unmanned->setDisabled(5u, 1000u);
	REQUIRE(unmanned->isDisabledByType(5u));

	run(f.w.logic->scriptEngine(), act("DELETE_ALL_UNMANNED", {}));
	f.w.logic->runLogicFrame();
	CHECK(f.w.logic->findObjectByID(unmannedId) == nullptr);
	CHECK(f.w.logic->findObjectByID(mannedId) != nullptr);
}

// RW 0x7C69B5: SET_PLAYER_OWNERSHIP_OF_TYPE_COUNTER (ignoreDead=1) vs SET_PLAYER_OWNERSHIP_OF_TYPE_COUNTER_INCLUDE_DEAD (ignoreDead=0)
TEST_CASE("LOCAL-162 ScriptActions: SET_PLAYER_OWNERSHIP_OF_TYPE_COUNTER_INCLUDE_DEAD")
{
	Local162Fixture f;
	f.spawn("L162_Unit", f.p1, "u1");
	Object *u2 = f.spawn("L162_Unit", f.p1, "u2", 120.0f, 100.0f);
	u2->kill(DAMAGE_UNRESISTABLE, DEATH_NORMAL);

	auto &se = f.w.logic->scriptEngine();
	const std::string p1Name = f.p1->getPlayerName();
	run(se, act("SET_PLAYER_OWNERSHIP_OF_TYPE_COUNTER", { sp(15, "L162_Unit"), sp(11, p1Name), sp(4, "c_alive") }));
	run(se, act("SET_PLAYER_OWNERSHIP_OF_TYPE_COUNTER_INCLUDE_DEAD", { sp(15, "L162_Unit"), sp(11, p1Name), sp(4, "c_all") }));
	CHECK(se.counter("c_alive").value == 1);
	CHECK(se.counter("c_all").value == 2);
}

// RW 0x7BF941 / RW 0x7BF964 (REPULSOR bit 8), RW 0x7C17FD / RW 0x7C1823 (USER_1..USER_4 bits 0xB8..0xBB),
// RW 0x7C101D (SCRIPT_UNSTEALTHED 0x08), RW 0x7C1071 (SCRIPT_STRICT_CONTROL 0x20), RW 0x7C1874 (TEAM_SET_FLAME_STATUS)
TEST_CASE("LOCAL-162 ScriptActions: repulsor, modelcondition, stealth, strict control, flame status")
{
	Local162Fixture f;
	Team *tm = f.p1->getDefaultTeam();
	Object *u1 = f.spawn("L162_Unit", f.p1, "u1");
	Object *u2 = f.spawn("L162_Unit", f.p1, "u2", 120.0f, 100.0f);
	auto &se = f.w.logic->scriptEngine();
	const std::string p1Name = f.p1->getPlayerName();

	run(se, act("NAMED_SET_REPULSOR", { sp(14, "u1"), ip(8, 1) }));
	CHECK(u1->testStatus(8u));
	CHECK_FALSE(u2->testStatus(8u));

	run(se, act("TEAM_SET_REPULSOR", { sp(3, qual(tm)), ip(8, 1) }));
	CHECK(u2->testStatus(8u));
	run(se, act("TEAM_SET_REPULSOR", { sp(3, qual(tm)), ip(8, 0) }));
	CHECK_FALSE(u1->testStatus(8u));
	CHECK_FALSE(u2->testStatus(8u));

	run(se, act("UNIT_SET_MODELCONDITION", { sp(14, "u1"), ip(0, 2), ip(8, 1) }));
	CHECK(u1->testModelCondition(0xB9));
	CHECK_FALSE(u2->testModelCondition(0xB9));

	run(se, act("TEAM_SET_MODELCONDITION", { sp(3, qual(tm)), ip(0, 4), ip(8, 1) }));
	CHECK(u1->testModelCondition(0xBB));
	CHECK(u2->testModelCondition(0xBB));

	run(se, act("SET_COUNTER_TO_NUMBER_OBJECTS_PLAYER_OWNES_WITH_MODELCONDITION", { sp(11, p1Name), sp(0, "USER_2"), sp(4, "c_mc2") }));
	run(se, act("SET_COUNTER_TO_NUMBER_OBJECTS_PLAYER_OWNES_WITH_MODELCONDITION", { sp(11, p1Name), sp(0, "USER_4"), sp(4, "c_mc4") }));
	CHECK(se.counter("c_mc2").value == 1);
	CHECK(se.counter("c_mc4").value == 2);

	run(se, act("TEAM_SET_STEALTH_ENABLED", { sp(3, qual(tm)), ip(8, 0) }));
	CHECK((u1->scriptStatus() & 0x08u) != 0u);
	run(se, act("TEAM_SET_STEALTH_ENABLED", { sp(3, qual(tm)), ip(8, 1) }));
	CHECK((u1->scriptStatus() & 0x08u) == 0u);

	run(se, act("TEAM_SET_STRICT_CONTROL_ENABLED", { sp(3, qual(tm)), ip(8, 1) }));
	CHECK((u1->scriptStatus() & 0x20u) != 0u);
	run(se, act("TEAM_SET_STRICT_CONTROL_ENABLED", { sp(3, qual(tm)), ip(8, 0) }));
	CHECK((u1->scriptStatus() & 0x20u) == 0u);

	run(se, act("TEAM_SET_FLAME_STATUS", { sp(3, qual(tm)), ip(8, 1) }));
	CHECK(u1->testStatus(10u));
	run(se, act("TEAM_SET_FLAME_STATUS", { sp(3, qual(tm)), ip(8, 0) }));
	CHECK_FALSE(u1->testStatus(10u));
}

// RW 0x7C3724, RW 0x7C2955, RW 0x7C4324, RW 0x7C506A, RW 0x7C5006, RW 0x7C3AB2, RW 0x7C3825, RW 0x7C0FC5
TEST_CASE("LOCAL-162 ScriptActions: experience, health, command points, music reset")
{
	Local162Fixture f;
	Team *tm = f.p1->getDefaultTeam();
	Object *u1 = f.spawn("L162_Unit", f.p1, "u1");
	auto &se = f.w.logic->scriptEngine();
	const std::string p1Name = f.p1->getPlayerName();

	run(se, act("TEAM_SET_EXPERIENCE_POINTS", { sp(3, qual(tm)), ip(0, 60) }));
	run(se, act("SET_UNIT_EXPERIENCE_TO_COUNTER", { sp(14, "u1"), sp(4, "c_xp") }));
	CHECK(se.counter("c_xp").value == 60);

	const int rankBefore = u1->getExperienceTracker()->getRank();
	run(se, act("TEAM_GAIN_LEVEL", { sp(3, qual(tm)), ip(8, 0) }));
	CHECK(u1->getExperienceTracker()->getRank() == rankBefore + 1);

	run(se, act("UNIT_GIVE_EXPERIENCE_LEVEL", { sp(14, "u1"), sp(10, "L162_Lvl2") }));
	CHECK(u1->getExperienceTracker()->getRank() == 2);
	run(se, act("TEAM_GIVE_EXPERIENCE_LEVEL", { sp(3, qual(tm)), sp(10, "L162_Lvl3") }));
	CHECK(u1->getExperienceTracker()->getRank() == 3);

	run(se, act("TEAM_SET_HEALTH", { sp(3, qual(tm)), rp(50.0f) }));
	CHECK(u1->getBodyModule()->getHealth() == 100.0f);
	run(se, act("TEAM_SET_HEALTH", { sp(3, qual(tm)), rp(150.0f) }));
	CHECK(u1->getBodyModule()->getHealth() == 200.0f);

	run(se, act("SET_PLAYER_COMMAND_POINTS_TOTAL_TO_COUNTER", { sp(11, p1Name), sp(4, "c_cp") }));
	CHECK(se.counter("c_cp").value == f.p1->commandPointLimit());

	run(se, act("MUSIC_RESET_MUSIC_SCRIPTING_SYSTEM", { ip(8, 1) }));
	CHECK(se.flag("/___MusicScript_Init"));
}

// RW 0x7C4DE6, RW 0x7C4E39, RW 0x7C4EDF, RW 0x7C146D: player/team relationship override set & remove
TEST_CASE("LOCAL-162 ScriptActions: relationship overrides set and remove")
{
	Local162Fixture f;
	Team *t1 = f.p1->getDefaultTeam();
	Team *t2 = f.p2->getDefaultTeam();
	auto &se = f.w.logic->scriptEngine();
	const std::string p1Name = f.p1->getPlayerName();
	const std::string p2Name = f.p2->getPlayerName();

	f.p1->setPlayerRelationship(f.p2, ENEMIES);
	CHECK(f.p1->getRelationship(t2) == ENEMIES);

	run(se, act("PLAYER_SET_OVERRIDE_RELATION_TO_TEAM", { sp(11, p1Name), sp(3, qual(t2)), ip(19, (int)ALLIES) }));
	CHECK(f.p1->getRelationship(t2) == ALLIES);

	run(se, act("PLAYER_REMOVE_OVERRIDE_RELATION_TO_TEAM", { sp(11, p1Name), sp(3, qual(t2)) }));
	CHECK(f.p1->getRelationship(t2) == ENEMIES);

	t1->setTeamRelationship(t2, ALLIES);
	CHECK(t1->getRelationship(t2) == ALLIES);
	run(se, act("TEAM_REMOVE_OVERRIDE_RELATION_TO_TEAM", { sp(3, qual(t1)), sp(3, qual(t2)) }));
	CHECK(t1->getRelationship(t2) == ENEMIES);

	t1->setPlayerRelationship(f.p2, NEUTRAL);
	CHECK(t1->getRelationship(t2) == NEUTRAL);
	run(se, act("TEAM_REMOVE_OVERRIDE_RELATION_TO_PLAYER", { sp(3, qual(t1)), sp(11, p2Name) }));
	CHECK(t1->getRelationship(t2) == ENEMIES);
}

// RW 0x7C490B (STOP/START), RW 0x7C495B (SET), RW 0x7C4A34 (ADD), RW 0x7C1A7D (TEAM_SPIN_FOR_SECONDS),
// RW 0x7C9C6D (NAMED_STOP), RW 0x7C9105 / RW 0x7C9161 / RW 0x7BFA8D / RW 0x7C9B4D / RW 0x7C98C7 (enter/exit)
TEST_CASE("LOCAL-162 ScriptActions: special power countdown, spin timer, stop, enter/exit")
{
	Local162Fixture f;
	Team *tm = f.p1->getDefaultTeam();
	Object *u1 = f.spawn("L162_Unit", f.p1, "u1", 100.0f, 100.0f);
	Object *b1 = f.spawn("L162_Building", f.p1, "b1", 130.0f, 100.0f);
	auto &se = f.w.logic->scriptEngine();

	const SpecialPowerTemplate *spTmpl = TheSpecialPowerStore->findSpecialPowerTemplate("SP_L162_Test");
	REQUIRE(spTmpl != nullptr);
	SpecialPowerModuleInterface *spm = SpecialPowerModules::findModule(*u1, spTmpl);
	REQUIRE(spm != nullptr);

	run(se, act("NAMED_SET_SPECIAL_POWER_COUNTDOWN", { sp(14, "u1"), sp(31, "SP_L162_Test"), ip(0, 10) }));
	CHECK(spm->getReadyFrame() == f.w.logic->getFrame() + 10u * (unsigned)LOGICFRAMES_PER_SECOND);

	const unsigned beforeAdd = spm->getReadyFrame();
	run(se, act("NAMED_ADD_SPECIAL_POWER_COUNTDOWN", { sp(14, "u1"), sp(31, "SP_L162_Test"), ip(0, 4) }));
	CHECK(spm->getReadyFrame() == beforeAdd + 4u * (unsigned)LOGICFRAMES_PER_SECOND);

	run(se, act("NAMED_STOP_SPECIAL_POWER_COUNTDOWN", { sp(14, "u1"), sp(31, "SP_L162_Test") }));
	run(se, act("NAMED_START_SPECIAL_POWER_COUNTDOWN", { sp(14, "u1"), sp(31, "SP_L162_Test") }));
	CHECK(spm->getReadyFrame() == beforeAdd + 4u * (unsigned)LOGICFRAMES_PER_SECOND);

	u1->getAIUpdateInterface()->aiMoveToPosition({ 300.0f, 100.0f, 0.0f }, CMD_FROM_SCRIPT);
	run(se, act("NAMED_STOP", { sp(14, "u1") }));
	CHECK(u1->getAIUpdateInterface()->isIdle());

	run(se, act("NAMED_ENTER_NAMED", { sp(14, "u1"), sp(14, "b1") }));
	CHECK(u1->getAIUpdateInterface()->lastCommandSource() == CMD_FROM_SCRIPT);
	run(se, act("NAMED_EXIT_ALL", { sp(14, "b1") }));
	run(se, act("TEAM_EXIT_ALL", { sp(3, qual(tm)) }));
	run(se, act("NAMED_EXIT_BUILDING", { sp(14, "u1") }));
	run(se, act("TEAM_EXIT_ALL_BUILDINGS", { sp(3, qual(tm)) }));

	ScriptEngine::SequentialScript seq;
	seq.team = tm->getID();
	seq.scriptName = "S_Spin";
	se.appendSequentialScript(seq);
	run(se, act("TEAM_SPIN_FOR_SECONDS", { sp(3, qual(tm)), rp(2.0f) }));
	REQUIRE_FALSE(se.sequentialScripts().empty());
	REQUIRE_FALSE(se.sequentialScripts()[0].empty());
	CHECK(se.sequentialScripts()[0][0].framesToWait == 2 * LOGICFRAMES_PER_SECOND);
}
