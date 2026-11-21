// OpenBFME unit tests. LOCAL-111 (fork issue 795): Lua alert classification for quirk Q5 on unregistered objects
// and alert source naming (editions: both RotWK 2.01 and BFME2 1.06).
//
// Sources:
//   - DozerActionPickActionPosState::update RW 0x88C6E6 -> DozerAIUpdate::hideInStructure RW 0x88C51B -> Object::leaveWorld RW 0x68C18F
//     -> LuaScriptEngine::unregisterObject RW 0x735A1D (at RW 0x68C1F5).
//   - AIUpdateInterface::update RW 0x6695EF -> updateScriptEvents RW 0x663E32 (at RW 0x66964D, no inWorld check)
//     -> LuaScriptEngine::updateModelConditionEvents RW 0x7378DA (at RW 0x663EB1) -> LuaScriptEngine::dispatch RW 0x735F53 (at RW 0x737925).
//   - LuaScriptEngine::pushObject RW 0x735AE9 (at RW 0x736068): quirk Q5 (RW 0x735B49..0x735B69) leaves two nils on the stack for an
//     unregistered object so lua_call RW 0x73611D calls nil and raises _ALERT RW 0x734E41 (DONOR BFME2 0x003333D1), which retail drops.
//   - Ennorath INI / Lua: data/ini/object/evilfaction/units/mordor/porter.ini (MordorPorter -> StandardPorterFunctions),
//     data/scripts/scriptevents.xml (OnProducing -> +ACTIVELY_CONSTRUCTING -> OnPorterStartBuild), data/scripts/scripts.lua:1023.

#include "doctest.h"
#include "EnnorathScenario.h"
#include "LogicTestUtil.h"

#include "GameClient/LiveScripting.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/ScriptEngine/LuaScriptEngine.h"

#include <array>
#include <memory>
#include <string>

using namespace logictest;

namespace
{

const char *kLocal111Objects =
	"Object Local111Porter\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"    AILuaEventsList = Local111PorterList\n"
	"  End\n"
	"  KindOf = INFANTRY DOZER\n"
	"End\n"
	"Object Local111BrokenUnit\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"    AILuaEventsList = Local111BrokenList\n"
	"  End\n"
	"  KindOf = INFANTRY\n"
	"End\n";

const char *kLocal111Xml =
	"<?xml version=\"1.0\"?>\n"
	"<SageLuaScriptSection>\n"
	"  <Events>\n"
	"    <InternalEvent Name=\"OnCreated\" />\n"
	"    <ModelConditionEvent Name=\"OnProducing\">\n"
	"      <Conditions>+ACTIVELY_CONSTRUCTING</Conditions>\n"
	"    </ModelConditionEvent>\n"
	"  </Events>\n"
	"  <EventList Name=\"Local111PorterList\">\n"
	"    <EventHandler EventName=\"OnProducing\" ScriptFunctionName=\"OnLocal111StartBuild\"/>\n"
	"  </EventList>\n"
	"  <EventList Name=\"Local111BrokenList\">\n"
	"    <EventHandler EventName=\"OnCreated\" ScriptFunctionName=\"OnLocal111BrokenCreated\"/>\n"
	"  </EventList>\n"
	"</SageLuaScriptSection>\n";

const char *kLocal111Lua =
	"Local111Ran = 0\n"
	"function OnLocal111StartBuild(self)\n"
	"\tLocal111Ran = Local111Ran + 1\n"
	"end\n"
	"function OnLocal111BrokenCreated(self)\n"
	"\tNoSuchLuaFunction111(self)\n"
	"end\n";

struct Local111World : LogicWorld
{
	Local111World()
		: LogicWorld(RandomAlgorithm::ZH_CarryChain, kLocal111Objects)
	{
		REQUIRE_MESSAGE(loadError.empty(), loadError);
		w.fx.mount({ { "data\\scripts\\scriptevents.xml", kLocal111Xml }, { "data\\scripts\\scripts.lua", kLocal111Lua } });
		scripting.reset(new LiveScripting(*logic, w.fx.fsys, w.keys));
		std::string err;
		REQUIRE_MESSAGE(scripting->start("", &err), err);
	}
	~Local111World()
	{
		if (scripting)
		{
			scripting->detach();
		}
	}
	std::unique_ptr<LiveScripting> scripting;
};

} // namespace

// Expectations: RW 0x88C51B -> RW 0x68C18F -> RW 0x735A1D unregisters the object from Lua; RW 0x663E32 -> RW 0x7378DA -> RW 0x735F53
// dispatches OnProducing -> RW 0x735AE9 (quirk Q5) -> lua_call RW 0x73611D raises _ALERT RW 0x734E41, which retail drops.
TEST_CASE("LOCAL-111: unregistered object dispatch (quirk Q5) is a retail-dropped dataDefect, not a LiveScripting error")
{
	Local111World f;
	Object *porter = f.make("Local111Porter");
	REQUIRE(porter != nullptr);

	// Advance past frame 2 (RW 0x663E41 gate: frame < 2 only snapshots flags).
	f.logic->runLogicFrame();
	f.logic->runLogicFrame();
	f.logic->runLogicFrame();

	std::array<std::uint32_t, 19> snapshot = porter->getModelConditionBits();

	// Leave world like DozerAIUpdate::hideInStructure (RW 0x88C51B -> RW 0x68C18F -> RW 0x735A1D).
	f.logic->friend_containLeaveWorld(*porter);
	const int bit = AIUpdateInterface::modelConditionBit("ACTIVELY_CONSTRUCTING");
	REQUIRE(bit >= 0); // tolerance: modelConditionBit returns -1 when unknown, non-negative bit index otherwise
	porter->setModelConditionState(bit, true);

	f.logic->scriptModelConditionEvents(*porter, snapshot);

	const LuaRuntime *rt = f.scripting->engine().logic();
	REQUIRE(rt != nullptr);
	REQUIRE(rt->alerts().size() == 1);
	CHECK(rt->alerts()[0].find("attempt to call a nil value") != std::string::npos);
	REQUIRE(rt->alertRetailDropped().size() == 1);
	CHECK(rt->alertRetailDropped()[0] == true);

	const LiveScripting::Stats s = f.scripting->stats();
	CHECK(s.errors.empty());
	REQUIRE(s.dataDefects.size() == 1);
	const std::string &msg = s.dataDefects.begin()->first;
	CHECK(msg.find("OnLocal111StartBuild") != std::string::npos);
	CHECK(msg.find("NOT registered: quirk Q5") != std::string::npos);
	CHECK(msg.find("Local111Porter") != std::string::npos);
}

// Expectations: RW 0x735F53 on a registered object enters Lua; calling an undefined global raises _ERRORMESSAGE (liolib.c errorfb)
// with stack traceback and line number, which must stay in LiveScripting::Stats::errors (not retail-dropped).
TEST_CASE("LOCAL-111: registered object handler calling a nil Lua function stays an error and names template, handler and call site")
{
	Local111World f;
	Object *unit = f.make("Local111BrokenUnit");
	REQUIRE(unit != nullptr);

	const LuaRuntime *rt = f.scripting->engine().logic();
	REQUIRE(rt != nullptr);
	REQUIRE(rt->alerts().size() == 1);
	REQUIRE(rt->alertRetailDropped().size() == 1);
	CHECK(rt->alertRetailDropped()[0] == false);

	const LiveScripting::Stats s = f.scripting->stats();
	CHECK(s.dataDefects.empty());
	REQUIRE(s.errors.size() == 1);
	CHECK(s.errors[0].find("Local111BrokenUnit") != std::string::npos);
	CHECK(s.errors[0].find("OnLocal111BrokenCreated") != std::string::npos);
	CHECK(s.errors[0].find("NoSuchLuaFunction111") != std::string::npos);
	CHECK(s.errors[0].find("nil value") != std::string::npos);
	CHECK(s.errors[0].find("stack traceback:") != std::string::npos);
	CHECK(s.errors[0].find("at line 6") != std::string::npos);
}

// Expectations: Ennorath MordorPorter (data/ini/object/evilfaction/units/mordor/porter.ini -> StandardPorterFunctions,
// data/scripts/scriptevents.xml -> OnProducing -> OnPorterStartBuild): when hidden out of world during construction
// (RW 0x88C51B -> RW 0x68C18F -> RW 0x735A1D), AIUpdateInterface::update (RW 0x6695EF -> RW 0x663E32 -> RW 0x7378DA)
// dispatches OnPorterStartBuild on the unregistered porter, raising quirk Q5 (RW 0x735AE9) which retail drops (RW 0x734E41).
TEST_CASE("LOCAL-111 ENNORATH scenario: MordorPorter building out of world triggers OnPorterStartBuild as dataDefect without LiveScripting errors")
{
	ennorathscenario::World world;
	if (!world.load("LOCAL-111 ENNORATH scenario"))
	{
		return;
	}
	structtest::Arena arena(world.shared, "FactionMen", "FactionMordor");

	LiveScripting scripting(arena.logic, *world.shared.mount->fs, world.shared.world->nameKeys());
	std::string err;
	REQUIRE_MESSAGE(scripting.start("", &err), err);
	struct Detach
	{
		LiveScripting &scripting;
		~Detach() { scripting.detach(); }
	} detach{ scripting };

	Object *porter = world.place(arena, "MordorPorter", 1, 300.0f, 300.0f);
	REQUIRE(porter != nullptr);

	for (int i = 0; i < 3; ++i)
	{
		arena.logic.runLogicFrame();
	}

	arena.logic.friend_containLeaveWorld(*porter);
	const int bit = AIUpdateInterface::modelConditionBit("ACTIVELY_CONSTRUCTING");
	REQUIRE(bit >= 0); // tolerance: modelConditionBit returns -1 when unknown, non-negative bit index otherwise
	porter->setModelConditionState(bit, true);

	arena.logic.runLogicFrame();

	const LiveScripting::Stats s = scripting.stats();
	CHECK(s.errors.empty());
	bool sawPorterQ5 = false;
	for (const auto &kv : s.dataDefects)
	{
		if (kv.first.find("OnPorterStartBuild") != std::string::npos &&
			kv.first.find("MordorPorter") != std::string::npos &&
			kv.first.find("NOT registered: quirk Q5") != std::string::npos)
		{
			sawPorterQ5 = true;
		}
	}
	CHECK(sawPorterQ5);
}
