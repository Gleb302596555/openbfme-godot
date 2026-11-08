// OpenBFME unit tests. GPL-3.0.
// PORT-21 (issue #77, stop S-1463): an object a contain takes out of the world becomes INAUDIBLE and leaves the large group audio; when it comes
// back it is audible again and rejoins. Synthetic INI only; no game data.
//
// The expectations are the branches of the RotWK functions, read with tools/mod/rw.py on RotWK 2.01 (game.dat, caveat S-001), not the port:
//   * RW 0x690728 (Object::setInaudible): status 52 is ORed into Object + 0x9A bit 4 (RW 0x69072B `or byte ptr [esi + 0x9a], 0x10`), the drawable's
//     audible flag is cleared (RW 0x69073C -> RW 0x674727) and every behaviour module of the list at Object + 0x24C is asked for interface 36
//     (module + 0x20, vtable RW 0xC6B400), slot 0 (RW 0x690765 `call [edx]`). No test of any status: the caller guards.
//   * RW 0x690772 (Object::setAudible): the reverse — bit 4 cleared (RW 0x690775 `and byte ptr [esi + 0x9a], 0xef`), the drawable audible again
//     (RW 0x690786 -> RW 0x67896F), interface 36 slot 1 (RW 0x6907AF `call [edx + 4]`).
//   * RW 0x692313 (the object leaves the world): test NOT_IN_WORLD (status 0x33 = 51, RW 0x692318), the world exit (RW 0x68C18F), then RW 0x690728
//     (RW 0x6923A5) and NOT_IN_WORLD set (RW 0x6923B0: setStatus(0x33, 1)). RW 0x6923B7 (the object enters the world): the world entry
//     (RW 0x68E31F), then RW 0x690772 (RW 0x692409) and NOT_IN_WORLD cleared (RW 0x692414).
//   * interface 36's slot 0 is LargeGroupAudioUpdate's leave (RW 0x8AF16C -> RW 0x8AF09D: TheLargeGroupAudio remove, RW 0x60D633), slot 1 its join
//     (RW 0x8AF174 -> RW 0x8AF005: add, RW 0x60D5FF). The member is what the group's maps count (LiveGameAudio's largeGroupMembers), so a REMOVE
//     event for its id is the drop, an ADD event its return.

#include "doctest.h"
#include "LogicTestUtil.h"

#include "GameLogic/LargeGroupAudioLink.h"
#include "GameLogic/Module/LargeGroupAudioUpdate.h"
#include "GameLogic/ObjectTemplateInfo.h"

#include <algorithm>
#include <string>
#include <vector>

namespace
{

// the drawable side of RW 0x690728 / RW 0x690772: RW 0x69073C / RW 0x690786 call the drawable, the port's ObjectClientHooks carries it
struct AudibleRecorder : ObjectClientHooks
{
	std::vector<std::string> calls;
	void objectCreated(Object &obj) override { calls.push_back("created#" + std::to_string(obj.getID())); }
	void objectDestroyed(Object &obj) override { calls.push_back("destroyed#" + std::to_string(obj.getID())); }
	void setDrawableAudible(Object &obj, bool audible) override
	{
		calls.push_back(std::string(audible ? "audible#" : "inaudible#") + std::to_string(obj.getID()));
	}
	size_t count(const std::string &what) const { return (size_t)std::count(calls.begin(), calls.end(), what); }
};

struct LgaSink : LargeGroupAudioSink
{
	std::vector<LargeGroupAudioEvent> events;
	void onLargeGroupAudioEvent(const LargeGroupAudioEvent &e) override { events.push_back(e); }
	size_t kindCount(LargeGroupAudioEvent::Kind kind) const
	{
		size_t n = 0;
		for (const LargeGroupAudioEvent &e : events)
		{
			n += e.kind == kind ? 1 : 0;
		}
		return n;
	}
};

// Object Grunt / Grunt2: a soldier in a large group audio (the module the Interface 36 slots belong to)
const char kPort21Objects[] =
	"Object P21Soldier\n"
	"  KindOf = INFANTRY SELECTABLE\n"
	"  Behavior = LargeGroupAudioUpdate ModuleTag_LGAU\n"
	"    Key = Soldier\n"
	"    UnitWeight = 3\n"
	"  End\n"
	"End\n";

const LargeGroupAudioUpdateModuleData *lgaData(const ThingTemplate &tt)
{
	for (const auto &n : tt.behaviorModules().nuggets())
	{
		if (const auto *d = dynamic_cast<const LargeGroupAudioUpdateModuleData *>(n.data.get()))
		{
			return d;
		}
	}
	return nullptr;
}

LargeGroupAudioUpdate *lgaModule(Object &o)
{
	for (const auto &m : o.modules())
	{
		if (auto *u = dynamic_cast<LargeGroupAudioUpdate *>(m.get()))
		{
			return u;
		}
	}
	return nullptr;
}

// The sink and the hooks are stack objects of the test: they go before the world does, even when an assertion aborts the case (a dangling sink
// would be called from the logic's teardown)
struct Wiring
{
	LargeGroupAudioLink &link;
	GameLogic &logic;
	ObjectClientHooks *previous;
	~Wiring()
	{
		link.setSink(nullptr);
		logic.setClientHooks(previous);
	}
};

int statusIndex(const char *name) { return ObjectTemplateInfoBuilder::objectStatusIndex(name); }

} // namespace

TEST_CASE("PORT-21: a member a contain takes out of the world is INAUDIBLE (RW 0x690728) and leaves the group (RW 0x8AF09D); back in the world it is audible and rejoins (RW 0x690772 / RW 0x8AF005)")
{
	logictest::LogicWorld f;
	LargeGroupAudioUpdate::registerClass(f.w.modules);
	const std::string err = f.w.load(kPort21Objects, INI_LOAD_OVERWRITE, "port21.ini");
	REQUIRE_MESSAGE(err.empty(), err);
	REQUIRE(lgaData(*f.w.get("P21Soldier")) != nullptr);
	AudibleRecorder recorder;
	ObjectClientHooks *previous = f.logic->clientHooks();
	f.logic->setClientHooks(&recorder);
	LgaSink sink;
	LargeGroupAudioLink &link = f.logic->largeGroupAudio();
	link.setSink(&sink);
	Wiring wiring{ link, *f.logic, previous };
	link.enable(f.logic->getFrame()); // RW 0x60D4D3 at the new game
	f.logic->runLogicFrame();

	Object *o = f.make("P21Soldier");
	REQUIRE(o);
	LargeGroupAudioUpdate *m = lgaModule(*o);
	REQUIRE(m);
	REQUIRE(m->registered()); // RW 0x8AF005 from onObjectCreated
	const int inaudible = statusIndex("INAUDIBLE");
	const int notInWorld = statusIndex("NOT_IN_WORLD");
	CHECK(inaudible == 52);   // Object + 0x98 bit 20, the bit RW 0x69072B sets (Object + 0x9A bit 4)
	CHECK(notInWorld == 51);  // the status RW 0x692318 reads and RW 0x6923B0 sets
	CHECK_FALSE(o->testStatus((unsigned)inaudible));
	CHECK_FALSE(o->testStatus((unsigned)notInWorld));
	CHECK(o->isInWorld());
	CHECK(recorder.count("inaudible#" + std::to_string(o->getID())) == 0);
	f.logic->runLogicFrame();
	const size_t removesBefore = sink.kindCount(LargeGroupAudioEvent::REMOVE);

	// a contain takes it out of the world (the port's seam for RW 0x692313: the contain modules call this, as OpenContain RW 0x865D3D does)
	f.logic->friend_containLeaveWorld(*o);

	// RW 0x69072B / RW 0x69073C / RW 0x6923B0 in that order
	CHECK(o->testStatus((unsigned)inaudible));   // RW 0x690728
	CHECK(o->testStatus((unsigned)notInWorld));  // RW 0x6923B0
	CHECK_FALSE(o->isInWorld());                 // RW 0x68C18F's last store, Object + 0x474
	CHECK(recorder.count("inaudible#" + std::to_string(o->getID())) == 1); // RW 0x69073C -> RW 0x674727, once
	// interface 36 slot 0: the module left TheLargeGroupAudio (RW 0x8AF09D), so the member no longer counts
	CHECK_FALSE(m->registered());
	REQUIRE(sink.kindCount(LargeGroupAudioEvent::REMOVE) == removesBefore + 1);
	const LargeGroupAudioEvent &removed = sink.events.back();
	CHECK(removed.kind == LargeGroupAudioEvent::REMOVE);
	CHECK(removed.object == o->getID());
	CHECK(removed.weight == 3); // the module data's UnitWeight

	// a second transition out of the world is the same one: RW 0x692318's guard, here `!isInWorld()`; no second REMOVE and no second audible call
	const size_t eventsAfterLeave = sink.events.size();
	f.logic->friend_containLeaveWorld(*o);
	CHECK(sink.events.size() == eventsAfterLeave);
	CHECK(recorder.count("inaudible#" + std::to_string(o->getID())) == 1);
	CHECK(m->registered() == false);

	// and the world takes it back (the port's seam for RW 0x6923B7)
	const size_t addsBefore = sink.kindCount(LargeGroupAudioEvent::ADD);
	f.logic->friend_containEnterWorld(*o);

	// RW 0x690775 / RW 0x690786 / RW 0x692414 in that order
	CHECK_FALSE(o->testStatus((unsigned)inaudible));  // RW 0x690772
	CHECK_FALSE(o->testStatus((unsigned)notInWorld)); // RW 0x692414
	CHECK(o->isInWorld());
	CHECK(recorder.count("audible#" + std::to_string(o->getID())) == 1); // RW 0x690786 -> RW 0x67896F, once
	// interface 36 slot 1: the module joined again (RW 0x8AF005 — its own INAUDIBLE gate, RW 0x8AF018, passed because the bit went first)
	CHECK(m->registered());
	REQUIRE(sink.kindCount(LargeGroupAudioEvent::ADD) == addsBefore + 1);
	CHECK(sink.events.back().kind == LargeGroupAudioEvent::ADD);
	CHECK(sink.events.back().object == o->getID());

	// a second entry into the world changes nothing
	const size_t eventsAfterEnter = sink.events.size();
	f.logic->friend_containEnterWorld(*o);
	CHECK(sink.events.size() == eventsAfterEnter);
	CHECK(recorder.count("audible#" + std::to_string(o->getID())) == 1);

	(void)wiring;
}

TEST_CASE("PORT-21 review r1: in a group of two, the member that stays in the world keeps counting; the leaving one's status pair does not touch it")
{
	logictest::LogicWorld f;
	LargeGroupAudioUpdate::registerClass(f.w.modules);
	const std::string err = f.w.load(kPort21Objects, INI_LOAD_OVERWRITE, "port21.ini");
	REQUIRE_MESSAGE(err.empty(), err);
	AudibleRecorder recorder;
	ObjectClientHooks *previous = f.logic->clientHooks();
	f.logic->setClientHooks(&recorder);
	LgaSink sink;
	LargeGroupAudioLink &link = f.logic->largeGroupAudio();
	link.setSink(&sink);
	Wiring wiring{ link, *f.logic, previous };
	link.enable(f.logic->getFrame());
	f.logic->runLogicFrame();

	Object *stays = f.make("P21Soldier");
	Object *leaves = f.make("P21Soldier");
	REQUIRE(stays);
	REQUIRE(leaves);
	REQUIRE(stays->getID() != leaves->getID());
	LargeGroupAudioUpdate *staysModule = lgaModule(*stays);
	LargeGroupAudioUpdate *leavesModule = lgaModule(*leaves);
	REQUIRE(staysModule);
	REQUIRE(leavesModule);
	REQUIRE(staysModule->registered());
	REQUIRE(leavesModule->registered());
	f.logic->runLogicFrame();
	const size_t removesBefore = sink.kindCount(LargeGroupAudioEvent::REMOVE);

	f.logic->friend_containLeaveWorld(*leaves);

	// exactly the leaving member's REMOVE: the other one is still counted and was not told anything
	REQUIRE(sink.kindCount(LargeGroupAudioEvent::REMOVE) == removesBefore + 1);
	CHECK(sink.events.back().object == leaves->getID());
	CHECK(staysModule->registered());
	CHECK_FALSE(leavesModule->registered());
	CHECK(recorder.count("inaudible#" + std::to_string(stays->getID())) == 0);
	CHECK(recorder.count("inaudible#" + std::to_string(leaves->getID())) == 1);
	CHECK_FALSE(stays->testStatus((unsigned)statusIndex("INAUDIBLE")));
	CHECK_FALSE(stays->testStatus((unsigned)statusIndex("NOT_IN_WORLD")));
	CHECK(stays->isInWorld());

	// the group's count after the drop: the module that stayed is the only registered member of the two
	size_t registered = 0;
	for (Object *o : { stays, leaves })
	{
		registered += lgaModule(*o)->registered() ? 1 : 0;
	}
	CHECK(registered == 1);

	(void)wiring;
}
