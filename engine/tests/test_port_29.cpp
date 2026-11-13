// OpenBFME. GPL-3.0.
// PORT-29 tests: BuildingBehavior/FireName (issue #101) - the fire a building starts. RW 0x85935C (BuildingBehavior::update) walks all four lists of the
// module data by index (data + 8 + i * 0xC): i = 0 NightWindowName (+8), i = 1 FireWindowName (+0x14), i = 2 GlowWindowName (+0x20), i = 3 FireName (+0x2C).
// The only reader of FireName in RotWK 2.01 is RW 0x85944B (`lea eax, [edi + 0x2c]`) in that loop: on the AFLAME 0->1 pass index 3 is shown when the map is
// night (TheWritableGlobalData +0x134 == 4), and hidden otherwise; the first update and the AFLAME 1->0 pass hide it with the other three lists. The same
// loop shows the FireWindowName list (index 1) and again in the shared tail (RW 0x8594D0). Synthetic data (logictest::LogicWorld), no retail files.
//
// The parse row is part of the same behaviour: every row of the BuildingBehavior table (RW 0xC56F70) uses the parser RW 0x42EED6, which erases the list
// (RW 0x42CA04: erase(begin, end)) and then appends every token of the line (RW 0x42E59E) - the port's INI::parseAsciiStringVector. The mod writes FireName
// twice (`FIRE01`, then `FIRE02`), so RotWK keeps the last line only; the port kept both until this lane.
#include "doctest.h"
#include "LogicTestUtil.h"
#include "Common/XferSave.h"
#include "GameLogic/Module/ConstructionModules.h"
#include "GameLogic/Module/LogicModules.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object/Object.h"

#include <string>
#include <vector>

using namespace logictest;
using namespace std::string_literals;

namespace
{
const char kProbe[] =
	"Object Port29Fire\n"
		"  Body = ActiveBody ModuleTag_Body\n"
		"    MaxHealth = 100\n"
		"  End\n"
		"  Behavior = BuildingBehavior ModuleTag_Behavior\n"
		"    NightWindowName = WINDOW_N01\n"
		"    FireWindowName = WINDOW_F01\n"
		"    GlowWindowName = WINDOW_G01\n"
		"    FireName = FIRE01\n"
		"    FireName = FIRE02\n"
		"  End\n"
		"  Geometry = CYLINDER\n  GeometryMajorRadius = 10\n  GeometryHeight = 10\n"
		"End\n"
		"Object Port29NoFire\n"
		"  Body = ActiveBody ModuleTag_Body\n"
		"    MaxHealth = 100\n"
		"  End\n"
		"  Behavior = BuildingBehavior ModuleTag_Behavior\n"
		"    NightWindowName = WINDOW_N01\n"
		"    GlowWindowName = WINDOW_G01\n"
		"  End\n"
		"  Geometry = CYLINDER\n  GeometryMajorRadius = 10\n  GeometryHeight = 10\n"
		"End\n"
		"Object Port29FireOnly\n"
		"  Body = ActiveBody ModuleTag_Body\n"
		"    MaxHealth = 100\n"
		"  End\n"
		"  Behavior = BuildingBehavior ModuleTag_Behavior\n"
		"    FireName = FIRE_ONE\n"
		"  End\n"
		"  Geometry = CYLINDER\n  GeometryMajorRadius = 10\n  GeometryHeight = 10\n"
		"End\n"
		"Object Port29Empty\n"
		"  Body = ActiveBody ModuleTag_Body\n"
		"    MaxHealth = 100\n"
		"  End\n"
		"  Behavior = BuildingBehavior ModuleTag_Behavior\n"
		"  End\n"
		"  Geometry = CYLINDER\n  GeometryMajorRadius = 10\n  GeometryHeight = 10\n"
		"End\n";

struct ProbeWorld : LogicWorld
{
	ProbeWorld()
	{
		LogicModules::registerAll(w.modules);
		logic->settings().bodyThresholdsLoaded = true; // ActiveBody's default damage thresholds (GameData UnitDamagedThreshold / UnitReallyDamagedThreshold)
		logic->settings().unitDamagedThreshold = 0.65f;
		logic->settings().unitReallyDamagedThreshold = 0.35f;
		const std::string err = w.load(kProbe, INI_LOAD_OVERWRITE, "port29.ini");
		REQUIRE_MESSAGE(err.empty(), err);
	}
	Object *makeProbe(const char *name, Team *team) { return make(name, team); }
};

// captures what the logic told the client about sub objects (as "name show|hide [permanent]")
class SubObjectHooks : public ObjectClientHooks
{
public:
	std::vector<std::string> calls;
	void objectCreated(Object &) override {}
	void objectDestroyed(Object &) override {}
	void showSubObject(Object &, const std::string &name, bool visible, bool permanent) override
	{
		calls.push_back(name + (visible ? " show" : " hide") + (permanent ? " permanent" : ""));
	}
	void clear() { calls.clear(); }
};

void frame(GameLogic &logic)
{
	logic.runLogicFrame();
}

BuildingBehavior *behaviorOf(Object &o)
{
	return dynamic_cast<BuildingBehavior *>(o.findModule("BuildingBehavior"));
}

const std::vector<std::string> hide(const char *name)
{
	return { std::string(name) + " hide permanent" };
}
const std::vector<std::string> show(const char *name)
{
	return { std::string(name) + " show permanent" };
}
} // namespace

TEST_CASE("PORT-29 parse: the FireName row is RW 0x42EED6 (erase, then append the line's tokens) - the last line wins")
{
	ProbeWorld f;
	Object *o = f.makeProbe("Port29Fire", f.teamOf("Alice"));
	BuildingBehavior *module = behaviorOf(*o);
	REQUIRE(module != nullptr);
	// the mod writes FireName twice (ElvenBarracksBFME1: FIRE01, then FIRE02); RW 0x42EED6 erases the list before it appends, so FIRE02 is the whole list
	CHECK(module->data()->m_fireName == std::vector<std::string>{ "FIRE02" });
	// every token of one line stays in the list (RW 0x42E59E appends token by token)
	CHECK(module->data()->m_nightWindowName == std::vector<std::string>{ "WINDOW_N01" });
	CHECK(module->data()->m_fireWindowName == std::vector<std::string>{ "WINDOW_F01" });
	CHECK(module->data()->m_glowWindowName == std::vector<std::string>{ "WINDOW_G01" });
}

TEST_CASE("PORT-29 fire at night: the fire sub objects are shown when the building catches fire (RW 0x85944B, 0x859470)")
{
	ProbeWorld f;
	f.logic->settings().night = true;
	Object *o = f.makeProbe("Port29Fire", f.teamOf("Alice"));
	SubObjectHooks hooks;
	o->friend_bindToClient(&hooks);
	frame(*f.logic); // the first update: the baseline (every list hidden, the night windows shown)
	CHECK(hooks.calls == std::vector<std::string>{ "WINDOW_N01 hide permanent", "WINDOW_F01 hide permanent", "WINDOW_G01 hide permanent",
		"FIRE02 hide permanent", "WINDOW_N01 show permanent" });
	hooks.clear();
	o->setStatus(10, true); // AFLAME (TheObjectStatusNames[10])
	frame(*f.logic);
	// RW 0x859438..0x859481: i = 0 hides the night list, i = 1 (fire window) and i = 2 (glow) are shown, i = 3 (FireName) is shown because the map is night;
	// then RW 0x859483 + the shared tail RW 0x8594D0 shows the fire window list once more
	CHECK(hooks.calls == std::vector<std::string>{ "WINDOW_N01 hide permanent", "WINDOW_F01 show permanent", "WINDOW_G01 show permanent",
		"FIRE02 show permanent", "WINDOW_F01 show permanent" });
	hooks.clear();
	frame(*f.logic); // still aflame: RW 0x859499 -> 0x8594E5, nothing is applied
	CHECK(hooks.calls.empty());
	hooks.clear();
	o->setStatus(10, false); // the fire goes out
	frame(*f.logic);
	// RW 0x8594AD..0x8594C0 hides every list, the shared tail shows the night list again at night
	CHECK(hooks.calls == std::vector<std::string>{ "WINDOW_N01 hide permanent", "WINDOW_F01 hide permanent", "WINDOW_G01 hide permanent",
		"FIRE02 hide permanent", "WINDOW_N01 show permanent" });
}

TEST_CASE("PORT-29 fire by day: the fire sub objects stay hidden when the building catches fire (RW 0x85945A: index 3 falls through to the hide branch)")
{
	ProbeWorld f;
	Object *o = f.makeProbe("Port29Fire", f.teamOf("Alice"));
	SubObjectHooks hooks;
	o->friend_bindToClient(&hooks);
	frame(*f.logic); // the first update on a day map: every list hidden, no night show
	CHECK(hooks.calls == std::vector<std::string>{ "WINDOW_N01 hide permanent", "WINDOW_F01 hide permanent", "WINDOW_G01 hide permanent",
		"FIRE02 hide permanent" });
	hooks.clear();
	o->setStatus(10, true);
	frame(*f.logic);
	// same loop, but `night && i == 3` is false, so the fire list is hidden while the fire window and the glow lists are shown
	CHECK(hooks.calls == std::vector<std::string>{ "WINDOW_N01 hide permanent", "WINDOW_F01 show permanent", "WINDOW_G01 show permanent",
		"FIRE02 hide permanent", "WINDOW_F01 show permanent" });
	hooks.clear();
	o->setStatus(10, false);
	frame(*f.logic); // day: no night show in the shared tail
	CHECK(hooks.calls == std::vector<std::string>{ "WINDOW_N01 hide permanent", "WINDOW_F01 hide permanent", "WINDOW_G01 hide permanent",
		"FIRE02 hide permanent" });
}

TEST_CASE("PORT-29 fire list alone: a building with only FireName shows the fire when it burns at night (RW 0x85944B)")
{
	SUBCASE("night map")
	{
		ProbeWorld f;
		f.logic->settings().night = true;
		Object *o = f.makeProbe("Port29FireOnly", f.teamOf("Alice"));
		SubObjectHooks hooks;
		o->friend_bindToClient(&hooks);
		frame(*f.logic); // the first update hides the list (the other three lists are empty, the night show has nothing to show)
		CHECK(hooks.calls == hide("FIRE_ONE"));
		hooks.clear();
		o->setStatus(10, true);
		frame(*f.logic); // the fire list is shown (the fire window and the glow lists are empty, so the shared tail adds nothing)
		CHECK(hooks.calls == show("FIRE_ONE"));
		hooks.clear();
		o->setStatus(10, false);
		frame(*f.logic);
		CHECK(hooks.calls == hide("FIRE_ONE"));
	}
	SUBCASE("day map")
	{
		ProbeWorld f;
		Object *o = f.makeProbe("Port29FireOnly", f.teamOf("Alice"));
		SubObjectHooks hooks;
		o->friend_bindToClient(&hooks);
		frame(*f.logic);
		CHECK(hooks.calls == hide("FIRE_ONE"));
		hooks.clear();
		o->setStatus(10, true);
		frame(*f.logic); // by day the fire list is hidden even while the building burns
		CHECK(hooks.calls == hide("FIRE_ONE"));
	}
}

TEST_CASE("PORT-29 without FireName: nothing about the fire sub objects, and the fire window list of the same loop")
{
	SUBCASE("no FireName list: the fire changes the night and glow lists only (lane PORT-83)")
	{
		ProbeWorld f;
		f.logic->settings().night = true;
		Object *o = f.makeProbe("Port29NoFire", f.teamOf("Alice"));
		SubObjectHooks hooks;
		o->friend_bindToClient(&hooks);
		frame(*f.logic);
		CHECK(hooks.calls == std::vector<std::string>{ "WINDOW_N01 hide permanent", "WINDOW_G01 hide permanent", "WINDOW_N01 show permanent" });
		hooks.clear();
		o->setStatus(10, true);
		frame(*f.logic); // i = 0 hide, i = 1 and i = 3 are empty, i = 2 (glow) is shown; the shared tail has nothing to show
		CHECK(hooks.calls == std::vector<std::string>{ "WINDOW_N01 hide permanent", "WINDOW_G01 show permanent" });
	}
	SUBCASE("no field at all: nothing changes")
	{
		ProbeWorld f;
		f.logic->settings().night = true;
		Object *o = f.makeProbe("Port29Empty", f.teamOf("Alice"));
		SubObjectHooks hooks;
		o->friend_bindToClient(&hooks);
		frame(*f.logic);
		CHECK(hooks.calls.empty());
		o->setStatus(10, true);
		frame(*f.logic);
		CHECK(hooks.calls.empty());
		o->setStatus(10, false);
		frame(*f.logic);
		CHECK(hooks.calls.empty());
	}
}

TEST_CASE("PORT-29 neighbour: the night and glow rules of the same loop are unchanged (lane PORT-83, RW 0x8593FD / 0x859467 / 0x8594D0)")
{
	// the four-list loop must not change what PORT-83 already held: the night windows are shown on a night map at the first update and again when the fire
	// goes out, the glow windows are shown while the building burns, and the steady state does not re-check the time of day
	ProbeWorld f;
	Object *o = f.makeProbe("Port29Fire", f.teamOf("Alice"));
	SubObjectHooks hooks;
	o->friend_bindToClient(&hooks);
	frame(*f.logic); // day: the baseline (no night show)
	hooks.clear();
	f.logic->settings().night = true; // the flip
	frame(*f.logic);
	CHECK(hooks.calls.empty()); // RW: the steady state does not re-check the time of day
	hooks.clear();
	o->setStatus(10, true);
	frame(*f.logic);
	CHECK(hooks.calls == std::vector<std::string>{ "WINDOW_N01 hide permanent", "WINDOW_F01 show permanent", "WINDOW_G01 show permanent",
		"FIRE02 show permanent", "WINDOW_F01 show permanent" });
	hooks.clear();
	o->setStatus(10, false); // the fire goes out at night: the night windows come back
	frame(*f.logic);
	CHECK(hooks.calls == std::vector<std::string>{ "WINDOW_N01 hide permanent", "WINDOW_F01 hide permanent", "WINDOW_G01 hide permanent",
		"FIRE02 hide permanent", "WINDOW_N01 show permanent" });
}

TEST_CASE("PORT-29 construction: the fire lists are not applied while AWAITING_CONSTRUCTION / ACTIVELY_BEING_CONSTRUCTED (RW 0x8593A2)")
{
	ProbeWorld f;
	f.logic->settings().night = true;
	Object *o = f.makeProbe("Port29Fire", f.teamOf("Alice"));
	SubObjectHooks hooks;
	o->friend_bindToClient(&hooks);
	o->setStatus(10, true); // aflame before the first frame, but the construction model conditions gate the whole update
	o->setModelConditionState(67, true); // AWAITING_CONSTRUCTION
	frame(*f.logic);
	CHECK(hooks.calls.empty());
	o->setModelConditionState(67, false);
	o->setModelConditionState(69, true); // ACTIVELY_BEING_CONSTRUCTED
	frame(*f.logic);
	CHECK(hooks.calls.empty());
	o->setModelConditionState(69, false);
	frame(*f.logic); // the construction state ends: the first update and the AFLAME 0->1 pass run in this frame
	CHECK(hooks.calls == std::vector<std::string>{ "WINDOW_N01 hide permanent", "WINDOW_F01 hide permanent", "WINDOW_G01 hide permanent",
		"FIRE02 hide permanent", "WINDOW_N01 show permanent", "WINDOW_N01 hide permanent", "WINDOW_F01 show permanent", "WINDOW_G01 show permanent",
		"FIRE02 show permanent", "WINDOW_F01 show permanent" });
}

TEST_CASE("PORT-29 state: the fire lists add no simulation state - the module still saves its two latches as version 2 (RW 0x859324)")
{
	ProbeWorld f;
	f.logic->settings().night = true;
	Object *o = f.makeProbe("Port29Fire", f.teamOf("Alice"));
	o->setStatus(10, true);
	SubObjectHooks hooks;
	o->friend_bindToClient(&hooks);
	frame(*f.logic);
	BuildingBehavior *module = behaviorOf(*o);
	REQUIRE(module != nullptr);
	XferSave save;
	module->xfer(save);
	REQUIRE(!save.data().empty());
	CHECK(save.data()[0] == 2); // the module's version byte (2: the two latches, unchanged by this lane)
}
