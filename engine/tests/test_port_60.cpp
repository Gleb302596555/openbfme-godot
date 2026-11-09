// PORT-60 (issue #170): EvaAnnounceClientCreate / CreateFakeRadarEvent — RotWK 2.01 verdict, pinned.
// The flag is parsed (field row RW 0xC75958, table RW 0xC758E8, parser RW 0x42E558, data +0x1B) and then
// never read anywhere in RotWK 2.01: the module's clientUpdate RW 0x8CDF07 reads data +0x18/+0x19/+0x1A and the
// events +0x8/+0xC/+0x10 only, the create slot RW 0x8CDED6 reads data +0x14 only, the xfer RW 0x8CDDD7 serializes
// neither the ModuleData nor +0x1B, and a full linear .text sweep for +0x1B memory operands finds only the ctor's
// default-write RW 0x8CDE44 and other classes' code. BFME2 1.06 is the same (parse registration only).
// No radar event exists for the flag in the original; the mod's "jump to the announced object" ping comes from
// RadarMarkerClientUpdate MarkerType=PingOneRing/PingBeacon (RW 0x8CE116, port lane MODGAP-2 / S-2451).
// These tests pin the verdict: parsing works, the announce decision and the xfer bytes do not depend on the flag,
// and the neighbouring members are unaffected. No engine behaviour changed. See docs/ennorath/port60-evaannounce-rw.md.
// GPL-3.0.

#include "doctest.h"

#include "Common/XferLoad.h"
#include "Common/XferSave.h"
#include "GameClient/ClientBehaviorModules.h"
#include "GameClient/EvaAnnounceClientCreate.h"
#include "LogicTestUtil.h"

#include <cstdint>
#include <utility>
#include <vector>

namespace
{
std::vector<std::pair<int, int>> announceSequence(bool fakeRadar, bool alreadySighted, int relationship, bool owner)
{
	EvaAnnounceClientCreateModuleData data;
	data.enemy = 10;
	data.ally = 20;
	data.owner = 30;
	data.delay = 4;
	data.firstSight = true;
	data.fakeRadar = fakeRadar;
	EvaAnnounceClientCreate module(nullptr, &data);
	module.onBuildCompleteAt(5); // m_due = max(7, 5 + max(2, 4)) = 9
	std::vector<std::pair<int, int>> out;
	for (std::uint32_t frame = 6; frame <= 20; ++frame)
	{
		bool sight = false;
		const int event = module.update(frame, false, true, owner, relationship, sight, alreadySighted);
		out.emplace_back(event, sight ? 1 : 0);
	}
	return out;
}

bool sameSequence(const std::vector<std::pair<int, int>> &a, const std::vector<std::pair<int, int>> &b)
{
	if (a.size() != b.size())
		return false;
	for (size_t i = 0; i < a.size(); ++i)
		if (a[i] != b[i])
			return false;
	return true;
}
} // namespace

TEST_CASE("PORT-60 EvaAnnounceClientCreate CreateFakeRadarEvent is parsed from INI (RW 0xC75958, parser RW 0x42E558, data +0x1B); absent stays false")
{
	logictest::LogicWorld f;
	ClientBehaviorModules::registerAll(f.w.modules);
	const char *kIniYes = "Object PingRingYes\n  ClientUpdate = EvaAnnounceClientCreate ModuleTag_Eva\n    CreateFakeRadarEvent = Yes\n  End\nEnd\n";
	const char *kIniNo = "Object PingRingNo\n  ClientUpdate = EvaAnnounceClientCreate ModuleTag_Eva\n    CreateFakeRadarEvent = No\n  End\nEnd\n";
	const char *kIniAbsent = "Object PingRingAbsent\n  ClientUpdate = EvaAnnounceClientCreate ModuleTag_Eva\n  End\nEnd\n";
	REQUIRE(f.w.load(kIniYes, INI_LOAD_OVERWRITE, "yes.ini").empty());
	REQUIRE(f.w.load(kIniNo, INI_LOAD_OVERWRITE, "no.ini").empty());
	REQUIRE(f.w.load(kIniAbsent, INI_LOAD_OVERWRITE, "absent.ini").empty());
	const auto findEva = [&](const char *name) -> const EvaAnnounceClientCreateModuleData * {
		const ThingTemplate *tt = f.w.get(name);
		if (!tt)
			return nullptr;
		for (const auto &n : tt->clientUpdateModules().nuggets())
			if (const auto *d = dynamic_cast<const EvaAnnounceClientCreateModuleData *>(n.data.get()))
				return d;
		return nullptr;
	};
	const EvaAnnounceClientCreateModuleData *yes = findEva("PingRingYes");
	const EvaAnnounceClientCreateModuleData *no = findEva("PingRingNo");
	const EvaAnnounceClientCreateModuleData *absent = findEva("PingRingAbsent");
	REQUIRE(yes != nullptr);
	REQUIRE(no != nullptr);
	REQUIRE(absent != nullptr);
	CHECK(yes->fakeRadar);
	CHECK_FALSE(no->fakeRadar);
	CHECK_FALSE(absent->fakeRadar); // RW: the ModuleData ctor zeroes +0x1B (RW 0x8CDE44)
}

TEST_CASE("PORT-60 the announce decision does not depend on CreateFakeRadarEvent (RW 0x8CDF07 reads +0x18/+0x19/+0x1A and events +0x8/+0xC/+0x10, never +0x1B)")
{
	// owner branch (owner index == local), enemy / neutral / ally branches, first-sight already consumed or not
	for (int relationship : {0, 1, 2})
		for (bool owner : {false, true})
			for (bool alreadySighted : {false, true})
				CHECK(sameSequence(announceSequence(false, alreadySighted, relationship, owner), announceSequence(true, alreadySighted, relationship, owner)));
	// and the sequence itself fires as RW does: once, at frame 9 (due = 9), owner event 30, firstSight flagged
	const std::vector<std::pair<int, int>> expected{{-1, 0}, {-1, 0}, {-1, 0}, {30, 1}, {-1, 0}, {-1, 0}, {-1, 0}, {-1, 0}, {-1, 0}, {-1, 0}, {-1, 0}, {-1, 0}, {-1, 0}, {-1, 0}, {-1, 0}};
	CHECK(sameSequence(announceSequence(true, false, 2, true), expected));
	CHECK(sameSequence(announceSequence(false, false, 2, true), expected));
}

TEST_CASE("PORT-60 xfer does not store CreateFakeRadarEvent (RW 0x8CDDD7: version, ObjectModule base, m_started, m_due) — the bytes are flag-independent")
{
	EvaAnnounceClientCreateModuleData ownerData;
	ownerData.owner = 30;
	ownerData.delay = 8;
	EvaAnnounceClientCreateModuleData withoutFlag = ownerData;
	EvaAnnounceClientCreateModuleData withFlag = ownerData;
	withFlag.fakeRadar = true;
	EvaAnnounceClientCreate a(nullptr, &withoutFlag), b(nullptr, &withFlag);
	a.onBuildCompleteAt(10); // due = max(7, 10 + max(2, 8)) = 18
	b.onBuildCompleteAt(10);
	XferSave aSave, bSave;
	a.xfer(aSave);
	b.xfer(bSave);
	CHECK(aSave.data() == bSave.data());
	CHECK(aSave.data() == std::vector<std::uint8_t>{2, 1, 1, 1, 18, 0, 0, 0});
	const std::vector<std::uint8_t> raw = aSave.data();
	EvaAnnounceClientCreate loaded(nullptr, &withFlag);
	XferLoad load(raw.data(), raw.size());
	loaded.xfer(load);
	CHECK(loaded.started());
	CHECK(loaded.due() == 18);
	bool sight = false;
	CHECK(loaded.update(17, false, true, true, 2, sight) == -1);  // before due: nothing fires
	CHECK(loaded.update(18, false, true, true, 2, sight) == 30);  // owner branch (RW 0x8CDF76)
}

TEST_CASE("PORT-60 neighbour members are unaffected by the flag (onlyVisible RW 0x8CDF2F/0x8CDF32, firstSight RW 0x8CDF56/0x8CDF59, usePosition RW 0x8CDFA1)")
{
	// onlyVisible + hidden: the announcement is cancelled, not deferred, and identically for both flag values
	for (bool fake : {false, true})
	{
		EvaAnnounceClientCreateModuleData data;
		data.owner = 30;
		data.onlyVisible = true;
		data.usePosition = true;
		data.fakeRadar = fake;
		EvaAnnounceClientCreate module(nullptr, &data);
		module.onBuildCompleteAt(0); // due = max(7, 0 + max(2, 0)) = 7
		bool sight = false;
		CHECK(module.update(7, true, true, true, 2, sight) == -1); // frame gate passed, hidden: cancelled
		CHECK_FALSE(sight);
		CHECK(module.usePosition());   // the +0x1A member still drives the position flag
		CHECK(data.fakeRadar == fake); // the parsed flag lives on, unread (RW verdict)
	}
	// firstSight: an already-sighted object never announces, for either flag value
	for (bool fake : {false, true})
	{
		EvaAnnounceClientCreateModuleData data;
		data.enemy = 10;
		data.firstSight = true;
		data.fakeRadar = fake;
		EvaAnnounceClientCreate module(nullptr, &data);
		module.onBuildCompleteAt(0);
		bool sight = false;
		CHECK(module.update(7, false, true, false, 0, sight, /*alreadySighted=*/true) == -1);
		CHECK_FALSE(sight);
	}
}
