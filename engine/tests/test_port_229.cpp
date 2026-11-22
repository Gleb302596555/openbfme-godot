// OpenBFME unit tests, lane PORT-229 (issue #552): Hordes and contains - garrison, transport, tunnels,
// horde membership and slaughter, as RotWK. Synthetic hordes and containers per item (enter, exit, split,
// member death, slaughter), expectations from RW with the address in a comment.
//
// Lane HORDE-1: HordeContainCore - the slot table (RW 0x877751), member <-> slot maps and free list
// (RW 0x873F30 / 0x87370B), rank fill-in (RW 0x873D48), slot world positions (RW 0x875847 / 0x86D720).
// No engine source changes in this lane: the lane only adds its tests. The expectations below are worked
// by hand from the formulas cited in GameLogic/Object/Contain/HordeContainCore.h.

#include "doctest.h"

#include "HordeTestUtil.h"

#include "GameLogic/Module/HordeContain.h"
#include "GameLogic/Object/Contain/HordeContainCore.h"

#include <cmath>
#include <map>

using namespace initest;

namespace
{
// A simple matcher: rank's UnitType must equal the member's template name.
bool sameName(const std::string &slotUnitType, const std::string &memberTemplateName)
{
    return slotUnitType == memberTemplateName;
}

// A horde with 3 ranks of 5 (GondorFighter), like the retail GondorFighterHorde.
const char *kGondor =
    "  RankInfo = RankNumber:1 UnitType:GondorFighter Position:X:50 Y:0 Position:X:50 Y:20 Position:X:50 Y:-20 Position:X:50 Y:40 Position:X:50 Y:-40\n"
    "  RankInfo = RankNumber:2 UnitType:GondorFighter Position:X:30 Y:0 Leader 1 0 Position:X:30 Y:20 Leader 1 1 Position:X:30 Y:-20 Leader 1 2 Position:X:30 Y:40 Leader 1 3 Position:X:30 Y:-40 Leader 1 4\n"
    "  RankInfo = RankNumber:3 UnitType:GondorFighter Position:X:10 Y:0 Leader 2 0 Position:X:10 Y:20 Leader 2 1 Position:X:10 Y:-20 Leader 2 2 Position:X:10 Y:40 Leader 2 3 Position:X:10 Y:-40 Leader 2 4\n";

HordeContainModuleData parseHorde(const std::string &body)
{
    HordeContainModuleData data;
    Fixture fx;
    fx.env.blocks.registerBlock("Body", [&data](INI *ini) { data.parseFromINI(ini); });
    const std::string err = loadError(fx.env, "core.ini", "Body\n" + body + "\nEnd\n");
    REQUIRE_MESSAGE(err.empty(), err);
    return data;
}

bool nearly(float a, double b, double eps = 1e-4f)
{
    return std::fabs((double)a - b) <= eps;
}

} // namespace

// PORT-229: the slot table is built rank by rank, position by position, centred on the horde (RW 0x877751, 0x86C4EA).
TEST_CASE("PORT-229: the 3x5 slot table, flat slot order, leader slots and the free list (RW 0x877751, 0x86C4EA)")
{
    const HordeContainModuleData d = parseHorde(kGondor);
    GameLogicRandom rng(RandomAlgorithm::RotWK_GameDat_LCG);
    rng.initGameLogicRandom(1, -1);
    HordeContainCore core(d, rng, sameName);
    core.buildSlots(true);
    REQUIRE(core.slots().size() == 15);

    const float ys[5] = { 0, 20, -20, 40, -40 };
    const float xs[3] = { 50, 30, 10 };
    // RW 0x86C4EA centres the table on the horde: the mean of the 15 slots, X 30 and Y 0.
    std::vector<float> rawX, rawY;
    for (int i = 0; i < 15; ++i)
    {
        rawX.push_back(xs[i / 5]);
        rawY.push_back(ys[i % 5]);
    }
    float sumX = 0.0f, sumY = 0.0f;
    for (float v : rawX)
        sumX = v + sumX;
    for (float v : rawY)
        sumY = v + sumY;
    const float cx = sumX * (1.0f / 15.0f), cy = sumY * (1.0f / 15.0f);

    for (int i = 0; i < 15; ++i)
    {
        const HordeContainCore::Slot &s = core.slots()[(size_t)i];
        CHECK(s.rank == i / 5 + 1);
        CHECK(nearly(s.x, xs[i / 5] - cx));
        CHECK(nearly(s.y, ys[i % 5] - cy));
        CHECK(s.x2 == s.x);
        CHECK(s.y2 == s.y);
        CHECK(s.angle == 0.0f);
        CHECK(s.leaderSlot == (i < 5 ? -1 : i - 5)); // rank 1: -1; rank 2: 0..4; rank 3: 5..9 (firstSlotOfRank(leaderRank) + leaderIndex)
    }
    CHECK(core.slots()[5].leaderSlot == 0);
    CHECK(core.slots()[10].leaderSlot == 5);

    // With no members and flag174 clear, every slot lands on the free list (RW 0x87791A-0x87799E).
    REQUIRE(core.freeList().size() == 15);
    int expected = 0;
    for (int f : core.freeList())
        CHECK(f == expected++);
}

// PORT-229: a member takes the first free slot whose rank's UnitType matches its template (RW 0x873F30).
TEST_CASE("PORT-229: a member takes the first free slot of a matching rank (RW 0x873F30)")
{
    const HordeContainModuleData d = parseHorde(kGondor);
    GameLogicRandom rng(RandomAlgorithm::RotWK_GameDat_LCG);
    rng.initGameLogicRandom(1, -1);
    HordeContainCore core(d, rng, sameName);
    core.buildSlots(true);

    // All 15 members match rank 1's UnitType: they fill slots 0..14 in free-list order (0,1,2,...).
    for (int k = 0; k < 15; ++k)
    {
        CHECK(core.addMember(100 + k, "GondorFighter") == k);
        CHECK(core.slotOf(100 + k) == k);
    }
    REQUIRE(core.freeList().empty());
    REQUIRE(core.members().size() == 15);

    // A non-matching template finds no free slot: -1, the member stays in the contain list without a slot.
    CHECK(core.addMember(999, "NotAMatcher") == -1);
    CHECK(core.slotOf(999) == -1);
    CHECK(core.members().size() == 16);
}

// PORT-229: removal frees the slot (non-special) and rank fill-in closes the ranks behind (RW 0x87370B, 0x873D48).
TEST_CASE("PORT-229: removal frees the slot and fill-in closes the ranks (RW 0x87370B, 0x873D48)")
{
    const HordeContainModuleData d = parseHorde(kGondor);
    GameLogicRandom rng(RandomAlgorithm::RotWK_GameDat_LCG);
    rng.initGameLogicRandom(1, -1);
    HordeContainCore core(d, rng, sameName);
    core.buildSlots(true);

    for (int k = 0; k < 15; ++k)
        core.addMember(100 + k, "GondorFighter");
    REQUIRE(core.freeList().empty());

    // Remove the member in slot 0: slot 0 goes on the free list, then fillInAll() runs inside removeMember.
    // Member 105 (slot 5, rank 2) moves to slot 0, then member 110 (slot 10, rank 3) moves to slot 5,
    // leaving slot 10 as the only free slot.
    core.removeMember(100, false);
    CHECK(core.slotOf(100) == -1);
    REQUIRE(!core.freeList().empty());
    CHECK(core.freeList().front() == 10); // slot 10 is the last free slot
    CHECK(core.slotOf(105) == 0); // member 105 was in slot 5, moved to slot 0
    CHECK(core.slotOf(110) == 5); // member 110 was in slot 10, moved to slot 5

    // No more fill-ins possible: the lowest free slot is rank 3, and no higher rank exists.
    CHECK(!core.fillInLowestFreeSlot());
    CHECK(core.freeList().front() == 10);
}

// PORT-229: a special member (banner carrier id or MACHINE template flag) does not free its slot on removal (RW 0x86C4BE).
TEST_CASE("PORT-229: a special member keeps its slot on removal (RW 0x86C4BE)")
{
    const HordeContainModuleData d = parseHorde(kGondor);
    GameLogicRandom rng(RandomAlgorithm::RotWK_GameDat_LCG);
    rng.initGameLogicRandom(1, -1);
    HordeContainCore core(d, rng, sameName);
    core.buildSlots(true);

    // Put a member in slot 0 and mark it as the banner carrier (H+0x26C).
    core.addMember(100, "GondorFighter");
    CHECK(core.slotOf(100) == 0);
    core.setBannerCarrierId(100);

    // Removing the banner carrier does not free its slot: slot 0 stays off the free list
    // while slots 1..14 remain free (only one member was added).
    core.removeMember(100, true); // templateIsSpecial irrelevant: id == bannerCarrierId is special
    CHECK(core.slotOf(100) == -1);
    bool slot0Freed = false;
    for (int f : core.freeList())
        if (f == 0)
            slot0Freed = true;
    CHECK(!slot0Freed); // slot 0 is not freed
}

// PORT-229: slot world position - the offset rotated by the horde's angle plus the horde's position (RW 0x875847).
TEST_CASE("PORT-229: slot world position is the rotated offset plus the horde position (RW 0x875847)")
{
    const HordeContainModuleData d = parseHorde(kGondor);
    GameLogicRandom rng(RandomAlgorithm::RotWK_GameDat_LCG);
    rng.initGameLogicRandom(1, -1);
    HordeContainCore core(d, rng, sameName);
    core.buildSlots(true);

    // Member 100 in slot 0: position (50-30, 0-0) = (20, 0) after centring.
    core.addMember(100, "GondorFighter");
    CHECK(core.slotOf(100) == 0);

    HordeContainCore::Placement placement;
    placement.position = Coord3D{ 100.0f, 200.0f, 0.0f };
    placement.angle = 0.0f;
    placement.computeTrig();

    float outAngle = 0.0f;
    Coord3D pos = core.getSlotWorldPos(100, placement, &outAngle);
    // Slot 0: x=20, y=0, angle 0, no rotation -> (120, 200, 0).
    CHECK(nearly(pos.x, 120.0f));
    CHECK(nearly(pos.y, 200.0f));
    CHECK(nearly(pos.z, 0.0f));
    CHECK(nearly(outAngle, 0.0f));

    // Rotate 90 degrees: x' = 20*cos - 0*sin = 0, y' = 20*sin + 0*cos = 20 -> (100, 220, 0).
    placement.angle = 1.57079632679f; // pi/2
    placement.computeTrig();
    pos = core.getSlotWorldPos(100, placement, &outAngle);
    CHECK(nearly(pos.x, 100.0f));
    CHECK(nearly(pos.y, 220.0f));

    // Test the banner-carrier-alone case: offset (0,0).
    core.setBannerCarrierId(100);
    pos = core.getSlotWorldPos(100, placement, &outAngle);
    CHECK(nearly(pos.x, 100.0f));
    CHECK(nearly(pos.y, 200.0f));
}

// PORT-229: chooseRandomFreeSlotUnitType picks a random free slot's rank UnitType (RW 0x86E0CB).
TEST_CASE("PORT-229: a random free slot's rank UnitType (RW 0x86E0CB)")
{
    const HordeContainModuleData d = parseHorde(kGondor);
    GameLogicRandom rng(RandomAlgorithm::RotWK_GameDat_LCG);
    rng.initGameLogicRandom(1, -1);
    HordeContainCore core(d, rng, sameName);
    core.buildSlots(true);

    // All slots are free: r = getValue(0, 14) picks the r-th free-list entry.
    core.addMember(100, "GondorFighter"); // takes slot 0 (first free)
    // Slot 0 is no longer free. The remaining free slots are 1..14.
    std::string unitType = core.chooseRandomFreeSlotUnitType();
    CHECK(unitType == "GondorFighter");
}

// PORT-229: rejoinMember - a member that kept its slot rejoins the contain list as-is (RW 0x872D0A, lane GARRISON-3).
TEST_CASE("PORT-229: rejoinMember - a member that kept its slot rejoins as-is (RW 0x872D0A)")
{
    const HordeContainModuleData d = parseHorde(kGondor);
    GameLogicRandom rng(RandomAlgorithm::RotWK_GameDat_LCG);
    rng.initGameLogicRandom(1, -1);
    HordeContainCore core(d, rng, sameName);
    core.buildSlots(true);

    core.addMember(100, "GondorFighter");
    CHECK(core.slotOf(100) == 0);
    CHECK(core.slotIndexOrZero(100) == 0);

    // releaseMember removes from the contain list but keeps the slot (lane GARRISON-3).
    core.releaseMember(100);
    CHECK(core.slotOf(100) == 0); // slot kept
    CHECK(core.slotIndexOrZero(100) == 0);

    // rejoinMember: the member holds a slot, so it rejoins the contain list as-is, true.
    CHECK(core.rejoinMember(100, "GondorFighter"));

    // A member without a slot: false, and it is added to the contain list without a slot.
    CHECK(!core.rejoinMember(999, "GondorFighter"));
}

// PORT-229: hasFreeSlotFor reports whether a free slot's rank matches a template (lane MODS-3).
TEST_CASE("PORT-229: hasFreeSlotFor reports a matching free slot (lane MODS-3)")
{
    const HordeContainModuleData d = parseHorde(kGondor);
    GameLogicRandom rng(RandomAlgorithm::RotWK_GameDat_LCG);
    rng.initGameLogicRandom(1, -1);
    HordeContainCore core(d, rng, sameName);
    core.buildSlots(true);

    CHECK(core.hasFreeSlotFor("GondorFighter"));
    core.addMember(100, "GondorFighter"); // takes slot 0
    CHECK(core.hasFreeSlotFor("GondorFighter")); // slots 1..14 still free
    // Fill all slots.
    for (int k = 1; k < 15; ++k)
        core.addMember(100 + k, "GondorFighter");
    CHECK(!core.hasFreeSlotFor("GondorFighter"));
    CHECK(!core.hasFreeSlotFor("NotAMatcher"));
}
