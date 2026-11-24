// OpenBFME unit tests for Local 180 (fork issue 961): script engine binary-map ports (part 2 of 2).
// Synthetic INI only; every expectation comes from RotWK 2.01 game.dat disassembly (MD5 daed3668006cd90f01c34e5a7da1901f).

#include "doctest.h"
#include "MoveTestUtil.h"
#include "ScriptTestUtil.h"

#include "Common/BuildAssistant.h"
#include "Common/Dict.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/SpecialPower.h"
#include "Common/Team.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/ActiveBody.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Object/Contain/HordeContainRuntime.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/ObjectTemplateInfo.h"
#include "GameLogic/ScriptEngine/ScriptActions.h"
#include "GameLogic/ScriptEngine/ScriptConditions.h"
#include "GameLogic/ScriptEngine/ScriptEngine.h"

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

const char *kLocal180Powers =
	"SpecialPower SP_L180_Test\n"
	"  Enum = SPECIAL_GENERAL_TARGETLESS_TWO\n"
	"  ReloadTime = 10000\n"
	"End\n";

const char *kLocal180Objects =
	"Object L180_Infantry\n"
	"  Side = Alpha\n"
	"  KindOf = INFANTRY SELECTABLE SCORE\n"
	"  CommandPoints = 5\n"
	"  Body = ActiveBody B\n"
	"    MaxHealth = 200\n"
	"  End\n"
	"  Behavior = AIUpdateInterface AI\n"
	"  End\n"
	"End\n"
	"Object L180_Heavy\n"
	"  Side = Alpha\n"
	"  KindOf = INFANTRY SELECTABLE SCORE\n"
	"  CommandPoints = 12\n"
	"  Body = ActiveBody B\n"
	"    MaxHealth = 300\n"
	"  End\n"
	"  Behavior = AIUpdateInterface AI\n"
	"  End\n"
	"End\n"
	"Object L180_MenStructure\n"
	"  Side = Alpha\n"
	"  KindOf = STRUCTURE SELECTABLE SCORE GARRISONABLE_UNTIL_DESTROYED\n"
	"  Body = ActiveBody B\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"  Behavior = GarrisonContain GC\n"
	"    ContainMax = 4\n"
	"  End\n"
	"End\n"
	"Object L180_MordorStructure\n"
	"  Side = Beta\n"
	"  KindOf = STRUCTURE SELECTABLE SCORE\n"
	"  Body = ActiveBody B\n"
	"    MaxHealth = 500\n"
	"  End\n"
	"End\n"
	"Object L180_SpellBook\n"
	"  Side = Alpha\n"
	"  KindOf = SPELL_BOOK UNATTACKABLE\n"
	"  Body = ActiveBody B\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = SpecialPowerModule SPM\n"
	"    SpecialPowerTemplate = SP_L180_Test\n"
	"  End\n"
	"End\n"
	"Object L180_Horde\n"
	"  Side = Alpha\n"
	"  KindOf = HORDE INFANTRY SELECTABLE\n"
	"  Body = ActiveBody B\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = AIUpdateInterface AI\n"
	"  End\n"
	"  Behavior = HordeContain HC\n"
	"    ContainMax = 4\n"
	"    Slots = 4\n"
	"    InitialPayload = L180_Infantry 4\n"
	"  End\n"
	"End\n";

struct Local180Fixture
{
	movetest::MoveWorld w;
	SpecialPowerStore spStore;
	SpecialPowerStore *prevSp = nullptr;
	Player *p1 = nullptr;
	Player *p2 = nullptr;

	Local180Fixture() : w(30, 30)
	{
		prevSp = TheSpecialPowerStore;
		TheSpecialPowerStore = &spStore;
		w.w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
		REQUIRE(w.w.load(kLocal180Powers, INI_LOAD_OVERWRITE, "l180_powers.ini").empty());
		REQUIRE(w.w.load(kLocal180Objects, INI_LOAD_OVERWRITE, "l180_objects.ini").empty());
		w.buildMap();

		p1 = w.players.findPlayerWithName("Player_1");
		p2 = w.players.findPlayerWithName("Player_2");
		if (!p1) p1 = w.players.getNthPlayer(1);
		if (!p2) p2 = w.players.getNthPlayer(2);
		REQUIRE(p1 != nullptr);
		REQUIRE(p2 != nullptr);
		startEngine(*w.logic);
	}

	~Local180Fixture()
	{
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

// RW 0x7EB7CD case 162 -> RW 0x7E5F5A: UNIT_HAS_PASSENGER tests contain->getContainCount() > 0.
TEST_CASE("LOCAL-180 UNIT_HAS_PASSENGER (RW 0x7E5F5A)")
{
	Local180Fixture f;
	Object *bldg = f.spawn("L180_MenStructure", f.p1, "Bldg1", 50.0f, 50.0f);
	Object *unit = f.spawn("L180_Infantry", f.p1, "Unit1", 60.0f, 50.0f);

	ScriptCondition c = cond("UNIT_HAS_PASSENGER", { sp(0, "Bldg1") });
	CHECK_FALSE(ScriptConditions::evaluate(f.w.logic->scriptEngine(), c));

	REQUIRE(bldg->getContain() != nullptr);
	bldg->getContain()->addToContain(unit);
	CHECK(ScriptConditions::evaluate(f.w.logic->scriptEngine(), c));

	// Adjacent behaviour: unit without contain returns false; missing unit returns false.
	CHECK_FALSE(ScriptConditions::evaluate(f.w.logic->scriptEngine(), cond("UNIT_HAS_PASSENGER", { sp(0, "Unit1") })));
	CHECK_FALSE(ScriptConditions::evaluate(f.w.logic->scriptEngine(), cond("UNIT_HAS_PASSENGER", { sp(0, "NoSuchUnit") })));
}

// RW 0x7CAFA5 cases 238/562/239/563 -> RW 0x7C0587 / 0x7C061F / 0x7C06BA / 0x7C0752: team production priority mutators.
TEST_CASE("LOCAL-180 TEAM_INCREASE/DECREASE_PRIORITY[_BY_VALUE] (RW 0x7C0587/0x7C061F/0x7C06BA/0x7C0752)")
{
	Local180Fixture f;
	Dict d;
	d.setInt("teamProductionPriority", 50);
	d.setInt("teamProductionPrioritySuccessIncrease", 10);
	d.setInt("teamProductionPriorityFailureDecrease", 7);
	TeamPrototype *proto = f.w.teams.initTeam("PrioTeam", f.p1, true, &d);
	Team *team = f.w.teams.createTeam(proto, true);
	REQUIRE(team != nullptr);
	const std::string tname = qual(team);

	run(f.w.logic->scriptEngine(), act("TEAM_INCREASE_PRIORITY", { sp(3, tname) }));
	CHECK(proto->teamProductionPriority() == 60);

	run(f.w.logic->scriptEngine(), act("TEAM_INCREASE_PRIORITY_BY_VALUE", { sp(3, tname), ip(0, 15) }));
	CHECK(proto->teamProductionPriority() == 75);

	run(f.w.logic->scriptEngine(), act("TEAM_DECREASE_PRIORITY", { sp(3, tname) }));
	CHECK(proto->teamProductionPriority() == 68);

	run(f.w.logic->scriptEngine(), act("TEAM_DECREASE_PRIORITY_BY_VALUE", { sp(3, tname), ip(0, 8) }));
	CHECK(proto->teamProductionPriority() == 60);
}

// RW 0x7CAFA5 case 410 -> RW 0x7C387F: sum template CommandPoints (+0x628) over team members into counter.
TEST_CASE("LOCAL-180 SET_COMMAND_POINTS_TO_BUILD_TEAM_TO_COUNTER (RW 0x7C387F)")
{
	Local180Fixture f;
	TeamPrototype *proto = f.w.teams.initTeam("CpTeam", f.p1, true, nullptr);
	Team *team = f.w.teams.createTeam(proto, true);
	Object *u1 = f.spawn("L180_Infantry", f.p1, nullptr, 10.0f, 10.0f);
	Object *u2 = f.spawn("L180_Infantry", f.p1, nullptr, 15.0f, 10.0f);
	Object *u3 = f.spawn("L180_Heavy", f.p1, nullptr, 20.0f, 10.0f);
	u1->setTeam(team);
	u2->setTeam(team);
	u3->setTeam(team);

	run(f.w.logic->scriptEngine(), act("SET_COMMAND_POINTS_TO_BUILD_TEAM_TO_COUNTER", { sp(3, qual(team)), sp(4, "cpCnt") }));
	CHECK(f.w.logic->scriptEngine().counter("cpCnt").value == 22);

	// Missing team leaves counter untouched.
	run(f.w.logic->scriptEngine(), act("SET_COMMAND_POINTS_TO_BUILD_TEAM_TO_COUNTER", { sp(3, f.p1->getPlayerName() + "/NoTeam"), sp(4, "cpCnt") }));
	CHECK(f.w.logic->scriptEngine().counter("cpCnt").value == 22);
}

// RW 0x7CAFA5 case 421 -> RW 0x7C395E -> RW 0x79E47A: count destroyed templates matching KindOf bit into counter.
TEST_CASE("LOCAL-180 SET_PLAYER_KILLS_OF_KINDOF_TO_COUNTER (RW 0x7C395E)")
{
	Local180Fixture f;
	f.p1->getScoreKeeper().setCounting(true);

	Object *vStr = f.spawn("L180_MordorStructure", f.p2, nullptr, 30.0f, 10.0f);
	f.p1->getScoreKeeper().addSlaughterTransferDestroyed("L180_Infantry", *f.p2, 1);
	f.p1->getScoreKeeper().addSlaughterTransferDestroyed("L180_Heavy", *f.p2, 1);
	f.p1->getScoreKeeper().addObjectDestroyed(*f.w.logic, *vStr);

	const int kInf = ObjectTemplateInfoBuilder::kindOfIndex("INFANTRY");
	const int kStr = ObjectTemplateInfoBuilder::kindOfIndex("STRUCTURE");
	const int kCav = ObjectTemplateInfoBuilder::kindOfIndex("CAVALRY");
	REQUIRE(kInf >= 0); // tolerance: kindOfIndex returns -1 only if token missing
	REQUIRE(kStr >= 0); // tolerance: kindOfIndex returns -1 only if token missing
	REQUIRE(kCav >= 0); // tolerance: kindOfIndex returns -1 only if token missing

	const std::string p1Name = f.p1->getPlayerName();
	run(f.w.logic->scriptEngine(), act("SET_PLAYER_KILLS_OF_KINDOF_TO_COUNTER", { sp(2, p1Name), ip(0, kInf), sp(4, "kInf") }));
	run(f.w.logic->scriptEngine(), act("SET_PLAYER_KILLS_OF_KINDOF_TO_COUNTER", { sp(2, p1Name), ip(0, kStr), sp(4, "kStr") }));
	run(f.w.logic->scriptEngine(), act("SET_PLAYER_KILLS_OF_KINDOF_TO_COUNTER", { sp(2, p1Name), ip(0, kCav), sp(4, "kCav") }));
	CHECK(f.w.logic->scriptEngine().counter("kInf").value == 2);
	CHECK(f.w.logic->scriptEngine().counter("kStr").value == 1);
	CHECK(f.w.logic->scriptEngine().counter("kCav").value == 0);
}

// RW 0x7CAFA5 case 578 -> RW 0x7C49BC: set readyFrame = now + seconds * LOGICFRAMES_PER_SECOND on player's spellbook module.
TEST_CASE("LOCAL-180 PLAYER_SET_SPECIAL_POWER_COUNTDOWN (RW 0x7C49BC)")
{
	Local180Fixture f;
	Object *sb = f.spawn("L180_SpellBook", f.p1, nullptr, 0.0f, 0.0f);
	f.p1->science().setSpellBookId(sb->getID());

	const SpecialPowerTemplate *spTmpl = f.spStore.findSpecialPowerTemplate("SP_L180_Test");
	REQUIRE(spTmpl != nullptr);
	SpecialPowerModuleInterface *mod = SpecialPowerModules::findModule(*sb, spTmpl);
	REQUIRE(mod != nullptr);

	const unsigned before = f.w.logic->getFrame();
	run(f.w.logic->scriptEngine(), act("PLAYER_SET_SPECIAL_POWER_COUNTDOWN", { sp(2, f.p1->getPlayerName()), sp(0, "SP_L180_Test"), ip(0, 4) }));
	CHECK(mod->getReadyFrame() == before + 4u * (unsigned)LOGICFRAMES_PER_SECOND);
}

// RW 0x7CAFA5 case 388 -> RW 0x7C6B06: clear disabled templates (RW 0x6AC4CD), and on disallow disable same-side STRUCTURE templates (RW 0x6ADA1B).
TEST_CASE("LOCAL-180 ALLOW_DISALLOW_ALL_BUILDINGS (RW 0x7C6B06)")
{
	Local180Fixture f;
	Player *pMen = f.p1;
	REQUIRE(pMen->getSide() == "Alpha");
	const ThingTemplate *menStr = f.w.logic->things().findTemplate("L180_MenStructure");
	const ThingTemplate *menInf = f.w.logic->things().findTemplate("L180_Infantry");
	const ThingTemplate *morStr = f.w.logic->things().findTemplate("L180_MordorStructure");
	REQUIRE(menStr != nullptr);
	REQUIRE(menInf != nullptr);
	REQUIRE(morStr != nullptr);

	run(f.w.logic->scriptEngine(), act("ALLOW_DISALLOW_ALL_BUILDINGS", { sp(2, pMen->getPlayerName()), ip(8, 0) }));
	CHECK(pMen->isTemplateDisabled(menStr->getTemplateID()));
	CHECK_FALSE(pMen->isTemplateDisabled(menInf->getTemplateID()));
	CHECK_FALSE(pMen->isTemplateDisabled(morStr->getTemplateID()));

	run(f.w.logic->scriptEngine(), act("ALLOW_DISALLOW_ALL_BUILDINGS", { sp(2, pMen->getPlayerName()), ip(8, 1) }));
	CHECK_FALSE(pMen->isTemplateDisabled(menStr->getTemplateID()));
}

// RW 0x7CAFA5 case 436 -> RW 0x7C6FA1: kill trunc(percent * 0.01f * total) horde members via kill(DAMAGE_UNRESISTABLE, DEATH_NORMAL).
TEST_CASE("LOCAL-180 KILL_HORDE_MEMBERS (RW 0x7C6FA1)")
{
	Local180Fixture f;
	Object *horde = f.spawn("L180_Horde", f.p1, "Horde1", 50.0f, 50.0f);
	ContainModuleInterface *contain = horde->getContain();
	REQUIRE(contain != nullptr);
	REQUIRE(contain->getHordeContainInterface() != nullptr);
	REQUIRE(contain->getContainedItemsList() != nullptr);
	REQUIRE(contain->getContainedItemsList()->size() == 4);

	std::vector<Object *> snap(contain->getContainedItemsList()->begin(), contain->getContainedItemsList()->end());

	// 0% kills nobody
	run(f.w.logic->scriptEngine(), act("KILL_HORDE_MEMBERS", { sp(0, "Horde1"), rp(0.0f) }));
	int dead = 0;
	for (Object *m : snap) if (m->isEffectivelyDead()) ++dead;
	CHECK(dead == 0);

	// 50% of 4 = 2 members killed
	run(f.w.logic->scriptEngine(), act("KILL_HORDE_MEMBERS", { sp(0, "Horde1"), rp(50.0f) }));
	dead = 0;
	for (Object *m : snap) if (m->isEffectivelyDead()) ++dead;
	CHECK(dead == 2);
}
