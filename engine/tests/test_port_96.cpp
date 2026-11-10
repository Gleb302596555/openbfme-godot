// OpenBFME unit tests. GPL-3.0.
// PORT-96 (issue #244): SlavedUpdate's client fade on the master's guard point (RW 0x8A6024 .. 0x8A6097).
// A slaved object inside FadeOutRange of its guard point fades its drawable out; outside it fades it back in.
// m_faded remembers which transition is active so each happens once per entry/exit. m_fadeTime (logic frames,
// parseDurationUnsignedInt) is passed straight through as the client-frame count since logic and client share
// the 5 fps rate. Synthetic INI only; no game data.

#include "doctest.h"
#include "MoveTestUtil.h"

#include "GameClient/ClientEvents.h"
#include "GameLogic/Module/SlavedUpdate.h"
#include "GameLogic/Object/Object.h"

#include <string>
#include <vector>

using namespace movetest;

namespace
{

struct FadeRecord
{
    ObjectID id;
    UnsignedInt frames;
    bool fadeOut; // true = fadeOut, false = fadeIn
};

struct FadeRecorder : ObjectClientHooks
{
    std::vector<FadeRecord> fades;

    void objectCreated(Object &) override {}
    void objectDestroyed(Object &) override {}

    void fadeIn(Object &obj, UnsignedInt frames) override { fades.push_back({ obj.getID(), frames, false }); }
    void fadeOut(Object &obj, UnsignedInt frames) override { fades.push_back({ obj.getID(), frames, true }); }

    size_t fadeOutCount(ObjectID id) const
    {
        size_t n = 0;
        for (const auto &f : fades)
            if (f.id == id && f.fadeOut) ++n;
        return n;
    }
    size_t fadeInCount(ObjectID id) const
    {
        size_t n = 0;
        for (const auto &f : fades)
            if (f.id == id && !f.fadeOut) ++n;
        return n;
    }
    void clear() { fades.clear(); }
};

SlavedUpdate *slavedOf(Object &o)
{
    auto *m = dynamic_cast<SlavedUpdate *>(o.findModule("SlavedUpdate"));
    REQUIRE_MESSAGE(m != nullptr, "the synthetic slave has a SlavedUpdate");
    return m;
}

// the wiring: the recorder replaces the logic's client hooks for the life of the test
struct Wiring
{
    GameLogic &logic;
    ObjectClientHooks *previous;
    ~Wiring() { logic.setClientHooks(previous); }
};

// Builds the synthetic INI for a master + slave pair. MoveWorld registers the Locomotor block and loads
// kLocomotors / kObjects, so this text only needs the P96* templates.
std::string p96Ini(int fadeOutRange, int fadeTime)
{
    return
        "Locomotor P96Loco\n"
        "  Surfaces = GROUND\n"
        "  TurnTime = 500\n"
        "  TurnTimeDamaged = 500\n"
        "  Acceleration = 510\n"
        "  Braking = 510\n"
        "  MinSpeed = 0\n"
        "  ZAxisBehavior = NO_Z_MOTIVE_FORCE\n"
        "  Appearance = TWO_LEGS\n"
        "  StickToGround = Yes\n"
        "  CloseEnoughDist = 1\n"
        "End\n"
        "Object P96Master\n"
        "  KindOf = INFANTRY SELECTABLE\n"
        "  Geometry = CYLINDER\n"
        "  GeometryMajorRadius = 8\n"
        "  GeometryMinorRadius = 8\n"
        "  GeometryHeight = 20\n"
        "  Behavior = AIUpdateInterface ModuleTag_AI\n"
        "  End\n"
        "  Body = ActiveBody ModuleTag_Body\n"
        "    MaxHealth = 100\n"
        "  End\n"
        "  LocomotorSet\n"
        "    Locomotor = P96Loco\n"
        "    Condition = SET_NORMAL\n"
        "    Speed = 55\n"
        "  End\n"
        "  ArmorSet\n"
        "    Conditions = None\n"
        "    Armor = NoArmor\n"
        "  End\n"
        "End\n"
        "Object P96Slave\n"
        "  KindOf = INFANTRY SELECTABLE\n"
        "  Geometry = CYLINDER\n"
        "  GeometryMajorRadius = 8\n"
        "  GeometryMinorRadius = 8\n"
        "  GeometryHeight = 20\n"
        "  Behavior = AIUpdateInterface ModuleTag_AI\n"
        "  End\n"
        "  Behavior = SlavedUpdate ModuleTag_Slaved\n"
        "    FadeOutRange = " + std::to_string(fadeOutRange) + "\n"
        "    FadeTime = " + std::to_string(fadeTime) + "\n"
        "    DieOnMastersDeath = No\n"
        "  End\n"
        "  Body = ActiveBody ModuleTag_Body\n"
        "    MaxHealth = 100\n"
        "  End\n"
        "  LocomotorSet\n"
        "    Locomotor = P96Loco\n"
        "    Condition = SET_NORMAL\n"
        "    Speed = 55\n"
        "  End\n"
        "  ArmorSet\n"
        "    Conditions = None\n"
        "    Armor = NoArmor\n"
        "  End\n"
        "End\n";
}

} // namespace

TEST_CASE("PORT-96 SlavedUpdate: entering FadeOutRange fades the slave out, leaving fades it back in (RW 0x8A6024 .. 0x8A6097)")
{
    std::string ini = p96Ini(100, 60);
    MoveWorld w(100, 100, ini.c_str());

    Team *alice = w.teamOf("Alice");
    Object *master = w.spawn("P96Master", 0, 0);
    Object *slave = w.spawn("P96Slave", 0, 0);
    REQUIRE(master);
    REQUIRE(slave);
    REQUIRE(master->getID() != slave->getID());

    // let the AI modules pick a locomotor
    w.frames(1);

    SlavedUpdate *slaved = slavedOf(*slave);
    slaved->onEnslave(master);

    // record client fade calls
    FadeRecorder recorder;
    ObjectClientHooks *previous = w.logic->clientHooks();
    w.logic->setClientHooks(&recorder);
    Wiring wiring{ *w.logic, previous };

    // both at the origin: the slave is in range of the guard point (= the master at the origin),
    // not faded yet -> fadeOut
    recorder.clear();
    slaved->update();
    CHECK(recorder.fadeOutCount(slave->getID()) == 1);
    CHECK(recorder.fadeInCount(slave->getID()) == 0);

    // a second update while still in range: already faded -> no new call
    recorder.clear();
    slaved->update();
    CHECK(recorder.fadeOutCount(slave->getID()) == 0);
    CHECK(recorder.fadeInCount(slave->getID()) == 0);

    // move the master away: the guard point follows, the slave is now out of range -> fadeIn
    Coord3D masterFar{ 1000.0f, 0.0f, 0.0f };
    master->teleportTo(&masterFar, true);
    recorder.clear();
    slaved->update();
    CHECK(recorder.fadeInCount(slave->getID()) == 1);
    CHECK(recorder.fadeOutCount(slave->getID()) == 0);

    // a second update while still out of range: already unfaded -> no new call
    recorder.clear();
    slaved->update();
    CHECK(recorder.fadeInCount(slave->getID()) == 0);
    CHECK(recorder.fadeOutCount(slave->getID()) == 0);

    // move the master back: the slave is in range again -> fadeOut again
    Coord3D masterHome{ 0.0f, 0.0f, 0.0f };
    master->teleportTo(&masterHome, true);
    recorder.clear();
    slaved->update();
    CHECK(recorder.fadeOutCount(slave->getID()) == 1);
    CHECK(recorder.fadeInCount(slave->getID()) == 0);
}

TEST_CASE("PORT-96 SlavedUpdate: FadeOutRange = 0 means no fade ever (RW 0x8A6024)")
{
    std::string ini = p96Ini(0, 60);
    MoveWorld w(100, 100, ini.c_str());

    Team *alice = w.teamOf("Alice");
    Object *master = w.spawn("P96Master", 0, 0);
    Object *slave = w.spawn("P96Slave", 0, 0);
    REQUIRE(master);
    REQUIRE(slave);

    w.frames(1);

    SlavedUpdate *slaved = slavedOf(*slave);
    slaved->onEnslave(master);

    FadeRecorder recorder;
    ObjectClientHooks *previous = w.logic->clientHooks();
    w.logic->setClientHooks(&recorder);
    Wiring wiring{ *w.logic, previous };

    // FadeOutRange = 0: the whole block is skipped, no fade calls at all
    slaved->update();
    CHECK(recorder.fades.empty());
}

TEST_CASE("PORT-96 SlavedUpdate: the frames passed to fadeOut equal m_fadeTime (logic frames = client frames at 5 fps)")
{
    std::string ini = p96Ini(100, 123);
    MoveWorld w(100, 100, ini.c_str());

    Team *alice = w.teamOf("Alice");
    Object *master = w.spawn("P96Master", 0, 0);
    Object *slave = w.spawn("P96Slave", 0, 0);
    REQUIRE(master);
    REQUIRE(slave);

    w.frames(1);

    SlavedUpdate *slaved = slavedOf(*slave);
    slaved->onEnslave(master);

    FadeRecorder recorder;
    ObjectClientHooks *previous = w.logic->clientHooks();
    w.logic->setClientHooks(&recorder);
    Wiring wiring{ *w.logic, previous };

    // the frames passed to fadeOut equal m_fadeTime, parsed as logic frames from the INI
    slaved->update();
    REQUIRE(recorder.fadeOutCount(slave->getID()) == 1);
    bool found = false;
    for (const auto &fr : recorder.fades)
    {
        if (fr.id == slave->getID() && fr.fadeOut)
        {
            CHECK(fr.frames == 123);
            found = true;
        }
    }
    CHECK(found);
}
