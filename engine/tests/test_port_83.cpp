// OpenBFME. GPL-3.0.
// Port 29 (#421, docs/ennorath/port29-firename-rw.md) applies the other two lists of the same RW 0x85935C loop, so every pass here also
// carries FireWindowName (WINDOW_F01) and FireName (FIRE_ONE): the four-list hide (RW 0x8593D0), on AFLAME 0->1 lists 1 and 2 shown and
// list 3 shown only at night (RW 0x859438..0x859481), then list 1 shown again (RW 0x8594D0). The expectations below were widened to that.
// PORT-83 tests: BuildingBehavior NightWindowName / GlowWindowName (issue #231): on a night map the sub objects named in NightWindowName are shown at the
// first update (permanent records), on a day map they stay hidden; GlowWindowName is shown while the building is on fire (status AFLAME, 0xA), as RW 0x85935C
// decides. Synthetic data (logictest::LogicWorld), no retail files.
//
// RW facts the expectations come from (game.dat, VA absolute):
//   ctor 0x85927B: m_field20 (module +0x20) = 0, m_field21 (module +0x21) = 1, setWakeFrame(UPDATE_SLEEP_NONE).
//   update 0x85935C: no object / effectively dead without status 0x58 (PHANTOM_STRUCTURE) / no drawable -> changed sleep;
//     AWAITING_CONSTRUCTION (bit 67) or ACTIVELY_BEING_CONSTRUCTED (bit 69, [obj+0x114] bits 3/5) -> UPDATE_SLEEP_NONE, no sub object work;
//     first update (m_field21): every list hidden, then on a night map (TheWritableGlobalData +0x134 == 4) the night window list shown;
//     steady state: no time-of-day re-check; status 0xA (AFLAME) transitions: 0->1 shows the glow list (and hides the night list), 1->0 hides
//     both and shows the night list again on a night map. The FireWindowName (data +0x14) and FireName (data +0x2C) lists RW reads in the same
//     function are lane Port 29 (issue #101)'s and are deliberately not applied in this port (see ConstructionModules.cpp).
//   apply 0x8592F0: one Drawable::showSubObject RW 0x672823 per name, the permanent flag always 1; Drawable::updateSubObjects RW 0x672713 (no port
//   counterpart: the port's setSubObjectHidden applies (or records) at the call).
#include "doctest.h"
#include "LogicTestUtil.h"
#include "Common/ModelState.h"
#include "Common/XferLoad.h"
#include "Common/XferSave.h"
#include "GameLogic/Module/ConstructionModules.h"
#include "GameLogic/Module/LogicModules.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object/Object.h"

#include <string>
#include <vector>

// prints a call list in a failed CHECK (doctest shows {?} for a vector otherwise)
namespace doctest
{
template <> struct StringMaker<std::vector<std::string>>
{
	static String convert(const std::vector<std::string> &v)
	{
		std::string out = "{";
		for (const std::string &x : v)
		{
			out += std::string(out.size() > 1 ? ", " : "") + '\"' + x + '\"';
		}
		return (out + "}").c_str();
	}
};
} // namespace doctest

using namespace logictest;
using namespace std::string_literals;

namespace
{
const char kProbe[] =
	"Object Port83Probe\n"
		"  Body = ActiveBody ModuleTag_Body\n"
		"    MaxHealth = 100\n"
		"  End\n"
		"  Behavior = BuildingBehavior ModuleTag_Behavior\n"
		"    NightWindowName = WINDOW_N01 WINDOW_N02\n"
		"    GlowWindowName = WINDOW_G01\n"
		"    FireWindowName = WINDOW_F01\n"
		"    FireName = FIRE_ONE\n"
		"  End\n"
		"  Geometry = CYLINDER\n  GeometryMajorRadius = 10\n  GeometryHeight = 10\n"
		"End\n"
		"Object Port83GlowOnly\n"
		"  Body = ActiveBody ModuleTag_Body\n"
		"    MaxHealth = 100\n"
		"  End\n"
		"  Behavior = BuildingBehavior ModuleTag_Behavior\n"
		"    GlowWindowName = WINDOW_G09\n"
		"  End\n"
		"  Geometry = CYLINDER\n  GeometryMajorRadius = 10\n  GeometryHeight = 10\n"
		"End\n"
		"Object Port83Empty\n"
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
		const std::string err = w.load(kProbe, INI_LOAD_OVERWRITE, "port83.ini");
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
} // namespace

TEST_CASE("PORT-83: the model condition table facts the module reads (RW 0x8593A2; RW 0xD9FAD8 indices)")
{
	CHECK(ModelCondition::indexOf("AWAITING_CONSTRUCTION") == 67);   // (m_bits[2] >> 3) & 1
	CHECK(ModelCondition::indexOf("ACTIVELY_BEING_CONSTRUCTED") == 69); // (m_bits[2] >> 5) & 1
}

TEST_CASE("PORT-83 night map: the night windows are shown at the first update; on a day map they stay hidden; the records are permanent (RW 0x85935C, 0x8592F0)")
{
	SUBCASE("night map")
	{
		ProbeWorld f;
		f.logic->settings().night = true;
		Object *o = f.makeProbe("Port83Probe", f.teamOf("Alice"));
		SubObjectHooks hooks;
		o->friend_bindToClient(&hooks);
		frame(*f.logic);
		CHECK(hooks.calls == std::vector<std::string>{ "WINDOW_N01 hide permanent", "WINDOW_N02 hide permanent", "WINDOW_F01 hide permanent",
			"WINDOW_G01 hide permanent", "FIRE_ONE hide permanent", "WINDOW_N01 show permanent", "WINDOW_N02 show permanent" });
	}
	SUBCASE("day map")
	{
		ProbeWorld f;
		Object *o = f.makeProbe("Port83Probe", f.teamOf("Alice"));
		SubObjectHooks hooks;
		o->friend_bindToClient(&hooks);
		frame(*f.logic);
		CHECK(hooks.calls == std::vector<std::string>{ "WINDOW_N01 hide permanent", "WINDOW_N02 hide permanent", "WINDOW_F01 hide permanent", "WINDOW_G01 hide permanent", "FIRE_ONE hide permanent" });
	}
	SUBCASE("glow-only list: one hide, no show; the fire lists are not read by this port")
	{
		ProbeWorld f;
		f.logic->settings().night = true;
		Object *o = f.makeProbe("Port83GlowOnly", f.teamOf("Alice"));
		SubObjectHooks hooks;
		o->friend_bindToClient(&hooks);
		frame(*f.logic);
		CHECK(hooks.calls == std::vector<std::string>{ "WINDOW_G09 hide permanent" });
	}
	SUBCASE("no field: nothing changes (the module has no lists to apply)")
	{
		ProbeWorld f;
		f.logic->settings().night = true;
		Object *o = f.makeProbe("Port83Empty", f.teamOf("Alice"));
		SubObjectHooks hooks;
		o->friend_bindToClient(&hooks);
		frame(*f.logic);
		CHECK(hooks.calls.empty());
	}
}

TEST_CASE("PORT-83 fire: the glow windows are shown while AFLAME and hidden when the fire goes out, and the night windows hide for the fire (RW 0x85935C)")
{
	SUBCASE("night map")
	{
		ProbeWorld f;
		f.logic->settings().night = true;
		Object *o = f.makeProbe("Port83Probe", f.teamOf("Alice"));
		SubObjectHooks hooks;
		o->friend_bindToClient(&hooks);
		frame(*f.logic); // the first update: baseline
		hooks.clear();
		o->setStatus(10, true); // AFLAME (TheObjectStatusNames[10])
		frame(*f.logic);
		CHECK(hooks.calls == std::vector<std::string>{ "WINDOW_N01 hide permanent", "WINDOW_N02 hide permanent", "WINDOW_F01 show permanent", "WINDOW_G01 show permanent", "FIRE_ONE show permanent", "WINDOW_F01 show permanent" });
		hooks.clear();
		frame(*f.logic); // still aflame: nothing changes
		CHECK(hooks.calls.empty());
		hooks.clear();
		o->setStatus(10, false); // the fire goes out
		frame(*f.logic);
		CHECK(hooks.calls == std::vector<std::string>{ "WINDOW_N01 hide permanent", "WINDOW_N02 hide permanent", "WINDOW_F01 hide permanent",
			"WINDOW_G01 hide permanent", "FIRE_ONE hide permanent", "WINDOW_N01 show permanent", "WINDOW_N02 show permanent" });
	}
	SUBCASE("day map")
	{
		ProbeWorld f;
		Object *o = f.makeProbe("Port83Probe", f.teamOf("Alice"));
		SubObjectHooks hooks;
		o->friend_bindToClient(&hooks);
		frame(*f.logic);
		hooks.clear();
		o->setStatus(10, true);
		frame(*f.logic);
		CHECK(hooks.calls == std::vector<std::string>{ "WINDOW_N01 hide permanent", "WINDOW_N02 hide permanent", "WINDOW_F01 show permanent", "WINDOW_G01 show permanent", "FIRE_ONE hide permanent", "WINDOW_F01 show permanent" });
		hooks.clear();
		o->setStatus(10, false);
		frame(*f.logic); // day: no night window show
		CHECK(hooks.calls == std::vector<std::string>{ "WINDOW_N01 hide permanent", "WINDOW_N02 hide permanent", "WINDOW_F01 hide permanent", "WINDOW_G01 hide permanent", "FIRE_ONE hide permanent" });
	}
}

TEST_CASE("PORT-83 steady state: the time of day is not re-checked outside a fire transition; the night windows catch up when the fire goes out (RW 0x85935C)")
{
	// the map's time of day is fixed in RotWK; a mid-game flip cannot happen in game - the flip here documents exactly what RW does when it does
	ProbeWorld f;
	Object *o = f.makeProbe("Port83Probe", f.teamOf("Alice"));
	SubObjectHooks hooks;
	o->friend_bindToClient(&hooks);
	frame(*f.logic); // day: baseline hides
	hooks.clear();
	f.logic->settings().night = true; // the flip
	frame(*f.logic);
	CHECK(hooks.calls.empty()); // RW: the steady state does not re-check the time of day
	hooks.clear();
	o->setStatus(10, true); // the fire starts
	frame(*f.logic);
	CHECK(hooks.calls == std::vector<std::string>{ "WINDOW_N01 hide permanent", "WINDOW_N02 hide permanent", "WINDOW_F01 show permanent", "WINDOW_G01 show permanent", "FIRE_ONE show permanent", "WINDOW_F01 show permanent" });
	hooks.clear();
	o->setStatus(10, false); // the fire goes out at night: the night windows are shown now
	frame(*f.logic);
	CHECK(hooks.calls == std::vector<std::string>{ "WINDOW_N01 hide permanent", "WINDOW_N02 hide permanent", "WINDOW_F01 hide permanent",
		"WINDOW_G01 hide permanent", "FIRE_ONE hide permanent", "WINDOW_N01 show permanent", "WINDOW_N02 show permanent" });
}

TEST_CASE("PORT-83 construction: no sub object work while AWAITING_CONSTRUCTION / ACTIVELY_BEING_CONSTRUCTED (RW 0x8593A2)")
{
	ProbeWorld f;
	f.logic->settings().night = true;
	Object *o = f.makeProbe("Port83Probe", f.teamOf("Alice"));
	SubObjectHooks hooks;
	o->friend_bindToClient(&hooks);
	o->setModelConditionState(67, true); // AWAITING_CONSTRUCTION
	frame(*f.logic);
	CHECK(hooks.calls.empty());
	o->setModelConditionState(67, false);
	o->setModelConditionState(69, true); // ACTIVELY_BEING_CONSTRUCTED
	frame(*f.logic);
	CHECK(hooks.calls.empty());
	o->setModelConditionState(69, false);
	frame(*f.logic); // the construction state ends: the normal rules apply from now on
	CHECK(hooks.calls == std::vector<std::string>{ "WINDOW_N01 hide permanent", "WINDOW_N02 hide permanent", "WINDOW_F01 hide permanent",
		"WINDOW_G01 hide permanent", "FIRE_ONE hide permanent", "WINDOW_N01 show permanent", "WINDOW_N02 show permanent" });
}

TEST_CASE("PORT-83 effectively dead: the module sleeps forever, unless PHANTOM_STRUCTURE (status 0x58) is set (RW 0x44DDEC(0x58))")
{
	SUBCASE("effectively dead")
	{
		ProbeWorld f;
		f.logic->settings().night = true;
		Object *o = f.makeProbe("Port83Probe", f.teamOf("Alice"));
		SubObjectHooks hooks;
		o->friend_bindToClient(&hooks);
		o->friend_setEffectivelyDead(true);
		frame(*f.logic);
		CHECK(hooks.calls.empty());
		// the module sleeps forever now, even when the phantom structure status is set afterwards (RW never wakes it)
		o->setStatus(88, true); // PHANTOM_STRUCTURE
		frame(*f.logic);
		CHECK(hooks.calls.empty());
	}
	SUBCASE("effectively dead and PHANTOM_STRUCTURE")
	{
		ProbeWorld f;
		f.logic->settings().night = true;
		Object *o = f.makeProbe("Port83Probe", f.teamOf("Alice"));
		SubObjectHooks hooks;
		o->friend_bindToClient(&hooks);
		o->friend_setEffectivelyDead(true);
		o->setStatus(88, true); // PHANTOM_STRUCTURE
		frame(*f.logic);
		CHECK(hooks.calls == std::vector<std::string>{ "WINDOW_N01 hide permanent", "WINDOW_N02 hide permanent", "WINDOW_F01 hide permanent",
			"WINDOW_G01 hide permanent", "FIRE_ONE hide permanent", "WINDOW_N01 show permanent", "WINDOW_N02 show permanent" });
	}
}

TEST_CASE("PORT-83 save/load: the two RW latches (module +0x20/+0x21) round trip; a version 1 module loads with the defaults (RW 0x859324)")
{
	// world A: consume the first update with the building on fire, then save the module
	ProbeWorld a;
	a.logic->settings().night = true;
	Object *buildingA = a.makeProbe("Port83Probe", a.teamOf("Alice"));
	buildingA->setStatus(10, true); // AFLAME before the first frame
	SubObjectHooks hooksA;
	buildingA->friend_bindToClient(&hooksA);
	frame(*a.logic); // the first update: baseline + the AFLAME 0->1 pass
	BuildingBehavior *moduleA = behaviorOf(*buildingA);
	REQUIRE(moduleA != nullptr);
	XferSave save;
	moduleA->xfer(save);
	REQUIRE(!save.data().empty());
	CHECK(save.data()[0] == 2); // the module's version byte (2: the two latches)

	// world B: load the saved state into a module that has not run yet
	ProbeWorld b;
	b.logic->settings().night = true;
	Object *buildingB = b.makeProbe("Port83Probe", b.teamOf("Alice"));
	BuildingBehavior *moduleB = behaviorOf(*buildingB);
	REQUIRE(moduleB != nullptr);
	XferLoad load(save.data());
	moduleB->xfer(load);
	SubObjectHooks hooksB;
	buildingB->friend_bindToClient(&hooksB);
	// the saved wake frame is world A's frame 2; world B's frames are 1 and 2, so the module is due on the second one
	frame(*b.logic); // frame 1: not due (the saved wake frame is 2)
	CHECK(hooksB.calls.empty());
	frame(*b.logic); // frame 2: the saved state runs; the object is not aflame here, so the exit pass runs with the latch still true
	CHECK(hooksB.calls == std::vector<std::string>{ "WINDOW_N01 hide permanent", "WINDOW_N02 hide permanent", "WINDOW_F01 hide permanent",
		"WINDOW_G01 hide permanent", "FIRE_ONE hide permanent", "WINDOW_N01 show permanent", "WINDOW_N02 show permanent" });

	// world C: the same, but with a version 1 buffer (the version byte flipped): the latches default (not aflame, first update pending)
	ProbeWorld c;
	c.logic->settings().night = true;
	Object *buildingC = c.makeProbe("Port83Probe", c.teamOf("Alice"));
	BuildingBehavior *moduleC = behaviorOf(*buildingC);
	REQUIRE(moduleC != nullptr);
	std::vector<std::uint8_t> v1 = save.data();
	v1[0] = 1; // pretend a pre-PORT-83 snapshot: no latch bytes
	XferLoad loadV1(v1);
	moduleC->xfer(loadV1); // must not throw; the module loads with the version-1 defaults
	SubObjectHooks hooksC;
	buildingC->friend_bindToClient(&hooksC);
	frame(*c.logic); // frame 1: not due (the saved wake frame is 2)
	frame(*c.logic); // frame 2: the first update runs with the version-1 defaults (firstUpdate default true)
	CHECK(hooksC.calls == std::vector<std::string>{ "WINDOW_N01 hide permanent", "WINDOW_N02 hide permanent", "WINDOW_F01 hide permanent",
		"WINDOW_G01 hide permanent", "FIRE_ONE hide permanent", "WINDOW_N01 show permanent", "WINDOW_N02 show permanent" });
}
