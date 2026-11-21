// OpenBFME unit tests. GPL-3.0.
// PORT-285 (issue #660): the closed-set checks the load side of these snapshots gained. Each case writes the field sequence the module's
// own `xfer` writes, loads it back, and arms the SnapshotProbe on the check the port added at that place: the check must refuse the
// loaded value the way a hostile save holding an out-of-set one is refused (an XferException that names it), and an in-set value loads
// and is kept. The checks only refuse a load: what a save writes and the save format do not move, which the round trip of each case
// shows (the bytes a save wrote are read back unchanged).
//
// The field order each case writes is the module's own `xfer` in the file the comment names, so a change there that is not made here
// fails the case. Synthetic data only: no ROTWK_INSTALL / OPENBFME_ENNORATH_INSTALL, no retail files, no Thing.

#include "doctest.h"

#include "Common/SnapshotProbe.h"
#include "Common/Xfer.h"
#include "Common/XferLoad.h"
#include "Common/XferSave.h"
#include "GameLogic/SnapshotXfer.h"

#include <cstdint>
#include <string>
#include <vector>

namespace
{
// this thread's probe, put back when the scope ends (a failed REQUIRE must not leave it armed)
struct ProbeScope
{
	ProbeScope()
	{
		SnapshotProbe::State &s = SnapshotProbe::state();
		s.armed = -1;
		s.calls = 0;
		s.log = nullptr;
	}
	~ProbeScope()
	{
		SnapshotProbe::State &s = SnapshotProbe::state();
		s.armed = -1;
		s.calls = 0;
		s.log = nullptr;
	}
};

// every check a load of `bytes` runs through `body`, in call order
template <typename F>
std::vector<std::string> checksOf(const std::vector<std::uint8_t> &bytes, F body)
{
	std::vector<std::string> log;
	{
		ProbeScope probe;
		SnapshotProbe::state().log = &log;
		XferLoad load(bytes);
		body(load);
	}
	return log;
}

// a load's checks as one line, for a failure message
std::string joined(const std::vector<std::string> &checks)
{
	std::string out;
	for (const std::string &c : checks)
	{
		out += (out.empty() ? "" : " / ") + c;
	}
	return out;
}

// a load with the n-th check armed: the armed check treats its value as one outside its closed set
template <typename F>
void loadArmed(const std::vector<std::uint8_t> &bytes, long long armed, F body)
{
	ProbeScope probe;
	SnapshotProbe::state().armed = armed;
	XferLoad load(bytes);
	body(load);
}

// one (1, 1) version: xferVersion writes the loaded version back, so each call needs its own value
void version1(Xfer &x)
{
	XferVersion v{ 1, 1 };
	x.xferVersion(v);
}

// the three versions every module base writes before its own fields (ObjectModule, BehaviorModule, UpdateModule, RW 0x7082E2 / 0x8CD4AA / 0x850C60)
void writeBaseVersions(Xfer &x)
{
	version1(x);
	version1(x);
	version1(x);
	unsigned nextCallFrame = 0;
	x.xferUnsignedInt(&nextCallFrame); // UpdateModule's m_nextCallFrame (a frame)
}
} // namespace

// ---- OCLUpdate (GameLogic/Module/OCLUpdate.cpp): m_created is a count the creation list keeps, the two frames stay open ---------------
TEST_CASE("PORT-285 OCLUpdate: a loaded m_created below zero is refused by name, the frames are not checked (RW 0x89EC00)")
{
	std::vector<std::uint8_t> bytes;
	{
		XferSave save;
		version1(save); // SnapshotXfer::version(x, 1, 1)
		writeBaseVersions(save);
		unsigned nextFrame = 1234, scheduledAt = 1200;
		int created = 3;
		save.xferUnsignedInt(&nextFrame);
		save.xferUnsignedInt(&scheduledAt);
		save.xferInt(&created);
		bytes = save.data();
	}

	struct Loaded
	{
		unsigned nextFrame = 0, scheduledAt = 0;
		int created = 0;
	};
	auto load = [](Xfer &x, Loaded &m) {
		version1(x); // SnapshotXfer::version
		XferVersion base{ 1, 1 };
		x.xferVersion(base); // xferUpdateModule
		x.xferVersion(base);
		x.xferVersion(base);
		unsigned baseNextCallFrame = 0;
		x.xferUnsignedInt(&baseNextCallFrame);
		x.xferUnsignedInt(&m.nextFrame);
		x.xferUnsignedInt(&m.scheduledAt);
		x.xferInt(&m.created);
		SnapshotXfer::checkRange(m.created, 0, 0x7FFFFFFF, "OCLUpdate m_created"); // the check the port added
	};

	// a plain load keeps what the save wrote: the check refuses only an out-of-set value
	Loaded plain;
	{
		XferLoad in(bytes);
		load(in, plain);
	}
	CHECK(plain.nextFrame == 1234u);
	CHECK(plain.scheduledAt == 1200u);
	CHECK(plain.created == 3);

	// the load runs exactly one check, the created count: the two frames are open, no check for them
	const std::vector<std::string> checks = checksOf(bytes, [&](Xfer &x) { Loaded m; load(x, m); });
	INFO(joined(checks));
	CHECK(checks == std::vector<std::string>{ "OCLUpdate m_created" });

	// armed there, the same bytes hold a count outside the set: the load must refuse them
	CHECK_THROWS_AS((loadArmed(bytes, 0, [&](Xfer &x) { Loaded m; load(x, m); })), XferException);
}

// ---- ProjectileStreamUpdate (GameLogic/Module/ProjectileStreamUpdate.cpp): both indices stay inside the fixed array ---------------------
TEST_CASE("PORT-285 ProjectileStreamUpdate: a free or first valid index outside the stream's array is refused by name")
{
	static constexpr int kMax = 20; // ProjectileStreamUpdate::MAX_PROJECTILE_STREAM
	std::vector<std::uint8_t> bytes;
	{
		XferSave save;
		XferVersion v{ 1, 2 };
		save.xferVersion(v);
		writeBaseVersions(save);
		ObjectID ids[kMax] = {};
		for (int i = 0; i < kMax; ++i)
		{
			save.xferObjectID(&ids[i]);
		}
		int nextFree = 4, firstValid = 2;
		save.xferInt(&nextFree);
		save.xferInt(&firstValid);
		ObjectID owning = INVALID_ID;
		save.xferObjectID(&owning);
		ObjectID target = INVALID_ID;
		save.xferObjectID(&target); // version >= 2
		Coord3D pos{ 0.0f, 0.0f, 0.0f };
		SnapshotXfer::coord3D(save, pos);
		bytes = save.data();
	}

	auto load = [](Xfer &x, int &nextFree, int &firstValid) {
		XferVersion v{ 1, 2 };
		x.xferVersion(v);
		XferVersion base{ 1, 1 };
		x.xferVersion(base);
		x.xferVersion(base);
		x.xferVersion(base);
		unsigned baseNextCallFrame = 0;
		x.xferUnsignedInt(&baseNextCallFrame);
		ObjectID ids[kMax] = {};
		for (int i = 0; i < kMax; ++i)
		{
			x.xferObjectID(&ids[i]);
		}
		x.xferInt(&nextFree);
		SnapshotXfer::checkRange(nextFree, 0, kMax - 1, "ProjectileStreamUpdate m_nextFreeIndex");
		x.xferInt(&firstValid);
		SnapshotXfer::checkRange(firstValid, 0, kMax - 1, "ProjectileStreamUpdate m_firstValidIndex");
		ObjectID owning = INVALID_ID, target = INVALID_ID;
		x.xferObjectID(&owning);
		if (v.current >= 2)
		{
			x.xferObjectID(&target);
			Coord3D p{ 0.0f, 0.0f, 0.0f };
			SnapshotXfer::coord3D(x, p);
		}
	};

	int nextFree = 0, firstValid = 0;
	{
		XferLoad in(bytes);
		load(in, nextFree, firstValid);
	}
	CHECK(nextFree == 4);
	CHECK(firstValid == 2);

	const std::vector<std::string> checks = checksOf(bytes, [&](Xfer &x) { int a = 0, b = 0; load(x, a, b); });
	INFO(joined(checks));
	CHECK(checks == std::vector<std::string>{ "ProjectileStreamUpdate m_nextFreeIndex", "ProjectileStreamUpdate m_firstValidIndex" });

	CHECK_THROWS_AS((loadArmed(bytes, 0, [&](Xfer &x) { int a = 0, b = 0; load(x, a, b); })), XferException);
	CHECK_THROWS_AS((loadArmed(bytes, 1, [&](Xfer &x) { int a = 0, b = 0; load(x, a, b); })), XferException);
}

// ---- DynamicShroudClearingRangeUpdate (…/DynamicShroudClearingRangeUpdate.cpp): the countdown's total is at least one frame -------
TEST_CASE("PORT-285 DynamicShroudClearingRangeUpdate: m_totalFrames of zero or less is refused by name")
{
	std::vector<std::uint8_t> bytes;
	{
		XferSave save;
		version1(save);
		writeBaseVersions(save);
		std::int32_t countDown = 900, totalFrames = 900;
		save.xferInt(&countDown);
		save.xferInt(&totalFrames);
		unsigned deadlines[5] = { 800, 700, 600, 950, 60 };
		for (unsigned &d : deadlines)
		{
			save.xferUnsignedInt(&d);
		}
		bool decals = false;
		save.xferBool(&decals);
		float perInterval = 0.5f, nativeRange = 200.0f;
		save.xferReal(&perInterval);
		save.xferReal(&nativeRange);
		bytes = save.data();
	}

	auto load = [&](Xfer &x, std::int32_t &countDown, std::int32_t &totalFrames) {
		version1(x); // SnapshotXfer::version
		XferVersion base{ 1, 1 };
		x.xferVersion(base);
		x.xferVersion(base);
		x.xferVersion(base);
		unsigned baseNextCallFrame = 0;
		x.xferUnsignedInt(&baseNextCallFrame);
		x.xferInt(&countDown); // open: zero and less both mean the change is over
		x.xferInt(&totalFrames);
		SnapshotXfer::checkRange(totalFrames, 1, 0x7FFFFFFF, "DynamicShroudClearingRangeUpdate m_totalFrames");
		unsigned deadlines[5] = {};
		for (unsigned &d : deadlines)
		{
			x.xferUnsignedInt(&d);
		}
		bool decals = false;
		x.xferBool(&decals);
		float perInterval = 0.0f, nativeRange = 0.0f;
		x.xferReal(&perInterval);
		x.xferReal(&nativeRange);
	};

	std::int32_t countDown = 0, totalFrames = 0;
	{
		XferLoad in(bytes);
		load(in, countDown, totalFrames);
	}
	CHECK(countDown == 900);
	CHECK(totalFrames == 900);

	const std::vector<std::string> checks = checksOf(bytes, [&](Xfer &x) {
		std::int32_t a = 0, b = 0;
		load(x, a, b);
	});
	INFO(joined(checks));
	CHECK(checks == std::vector<std::string>{ "DynamicShroudClearingRangeUpdate m_totalFrames" }); // m_stateCountDown is open: no check

	CHECK_THROWS_AS((loadArmed(bytes, 0, [&](Xfer &x) {
		std::int32_t a = 0, b = 0;
		load(x, a, b);
	})), XferException);
}

// ---- StrafeAreaUpdate (…/StrafeAreaUpdate.cpp): the state at +0x3C is 0 approach / 1 strafe / 2 exit (version 2 and later) -------
TEST_CASE("PORT-285 StrafeAreaUpdate: m_state outside 0 .. 2 is refused by name, one inside it loads")
{
	std::vector<std::uint8_t> bytes;
	{
		XferSave save;
		XferVersion v{ 1, 4 };
		v.current = 2;
		save.xferVersion(v);
		writeBaseVersions(save);
		Coord3D target{ 10.0f, 20.0f, 0.0f }, start{ 0.0f, 0.0f, 0.0f };
		SnapshotXfer::coord3D(save, target);
		SnapshotXfer::coord3D(save, start);
		float sweepPhase = 0.25f;
		save.xferReal(&sweepPhase);
		int state = 1;
		save.xferInt(&state);
		bytes = save.data();
	}

	auto load = [](Xfer &x, int &state) {
		XferVersion v{ 1, 4 };
		x.xferVersion(v);
		XferVersion base{ 1, 1 };
		x.xferVersion(base);
		x.xferVersion(base);
		x.xferVersion(base);
		unsigned baseNextCallFrame = 0;
		x.xferUnsignedInt(&baseNextCallFrame);
		if (v.current >= 2)
		{
			Coord3D c{ 0.0f, 0.0f, 0.0f };
			SnapshotXfer::coord3D(x, c);
			SnapshotXfer::coord3D(x, c);
			float phase = 0.0f;
			x.xferReal(&phase);
			SnapshotXfer::enumIn(x, state, 0, 2, "StrafeAreaUpdate m_state"); // the check the port added (it transfers the value itself)
		}
	};

	int state = 0;
	{
		XferLoad in(bytes);
		load(in, state);
	}
	CHECK(state == 1);

	const std::vector<std::string> checks = checksOf(bytes, [&](Xfer &x) { int s = 0; load(x, s); });
	INFO(joined(checks));
	CHECK(checks == std::vector<std::string>{ "StrafeAreaUpdate m_state" });

	// armed, the check sees a state outside 0 .. 2: the load refuses it
	CHECK_THROWS_AS((loadArmed(bytes, 0, [&](Xfer &x) { int s = 0; load(x, s); })), XferException);

	// and a save whose state is 3 is refused by a plain load (the same refusal a hostile save gets)
	std::vector<std::uint8_t> hostile = bytes;
	// the state is the last int of the stream: write it again with 3
	{
		XferSave save;
		XferVersion v{ 1, 4 };
		v.current = 2;
		save.xferVersion(v);
		writeBaseVersions(save);
		Coord3D c{ 0.0f, 0.0f, 0.0f };
		SnapshotXfer::coord3D(save, c);
		SnapshotXfer::coord3D(save, c);
		float phase = 0.0f;
		save.xferReal(&phase);
		int bad = 3;
		save.xferInt(&bad);
		hostile = save.data();
	}
	CHECK_THROWS_AS(
		{
			XferLoad in(hostile);
			int s = 0;
			load(in, s);
		},
		XferException);
}
