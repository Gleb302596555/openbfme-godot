// Owner 2026-11-10: Ennorath's builders walk backwards (the model does not turn to where it goes). Live logic and drawable with the player's own
// mounted mod; no game data is copied into this test. RotWK rules (S-001).
#include "doctest.h"
#include "StructureArena.h"
#include "Common/ModMount.h"
#include "Common/RetailArchivePolicy.h"
#include "GameClient/ClientEvents.h"
#include "GameClient/DrawableManager.h"
#include "GameClient/LogicSnapshot.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Module/AIUpdate.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>

#include <cmath>

TEST_CASE("modgap2 Ennorath builder: a worker ordered behind itself turns to its heading and does not back up")
{
    const char *rotwk = std::getenv("ROTWK_INSTALL");
    const char *bfme2 = std::getenv("BFME2_INSTALL");
    const char *mod = std::getenv("OPENBFME_ENNORATH_INSTALL");
    if (!rotwk || !*rotwk || !bfme2 || !*bfme2 || !mod || !*mod) {
        retailtest::printSkip("builder heading (set ROTWK_INSTALL, BFME2_INSTALL, OPENBFME_ENNORATH_INSTALL)");
        return;
    }
    std::string err;
    std::vector<RetailInstall> installs(2);
    installs[0].label = "rotwk";
    installs[0].root = rotwk;
    installs[1].label = "bfme2";
    installs[1].root = bfme2;
    REQUIRE_MESSAGE(RetailArchivePolicy::loadBuiltin("rotwk-201", installs[0].policy, &err), err);
    REQUIRE_MESSAGE(RetailArchivePolicy::loadBuiltin("bfme2-106", installs[1].policy, &err), err);
    Win32BIGFileSystem pure;
    RetailMountOptions retailOptions;
    retailOptions.md5CachePath = (std::filesystem::temp_directory_path() / "openbfme-modgap2-builder-md5.tsv").u8string();
    const RetailMountReport retail = MountRetailArchives(pure, installs, retailOptions);
    REQUIRE_MESSAGE(retail.ok, "retail mount failed");
    retailtest::Mount merged;
    merged.fs = std::make_unique<Win32BIGFileSystem>();
    ModMountOptions modOptions;
    modOptions.rotwkInstallRoot = rotwk;
    modOptions.md5CachePath = retailOptions.md5CachePath;
    const ModMountReport mounted = MountInstallOverlay(*merged.fs, retail, mod, modOptions);
    REQUIRE_MESSAGE(mounted.ok, "Ennorath overlay mount failed");
    hudtest::SharedWorld shared;
    shared.mount = &merged;
    shared.world = std::make_unique<RetailObjectWorld>(*merged.fs);
    REQUIRE_MESSAGE(shared.world->load(&err), err);
    REQUIRE(shared.world->report().errors.empty());
    structtest::Arena arena(shared, "FactionMen", "FactionMordor");
    ArchiveW3DFileSource source(*merged.fs);
    WW3DAssetManager assets(source);
    DrawableManager drawables(assets, arena.logic);
    ClientEventRecorder events(arena.logic);
    arena.logic.setClientHooks(&events);
    struct UnbindHooks {
        GameLogic &logic;
        ~UnbindHooks() { logic.setClientHooks(nullptr); }
    } unbind{arena.logic};
    std::shared_ptr<const LogicSnapshot> snap;
    // GondorWorker: TWO_LEGS, no presentation physics; ImladrisPorter: a locomotor with presentation physics (RW 0x67BDC0), whose drawn basis is the
    // physics applied to the drawable's own (owner 2026-11-10: it stayed at the identity while the pose turned about Z)
    for (const char *name : { "GondorWorker", "ImladrisPorter" }) {
    CAPTURE(name);
    const ThingTemplate *tmpl = shared.world->things().findTemplate(name);
    REQUIRE(tmpl);
    Object *worker = arena.logic.newObject(tmpl, arena.player(0)->getDefaultTeam(), ObjectStatusMaskType{});
    REQUIRE(worker);
    const Coord3D start{ 400.0f, 400.0f, 0.0f };
    worker->setPosition(&start);
    worker->setOrientation(0.0f); // facing +x
    auto step = [&]() {
        arena.logic.runLogicFrame();
        drawables.applyEvents(events.take());
        snap = LogicSnapshot::build(arena.logic, snap.get(), false, 0);
        drawables.advance(200.0);
        drawables.syncTransforms(*snap, 0.5, true); // the game's path: the interpolated pose half way into the frame
    };
    step(); step();
    REQUIRE(worker->getAIUpdateInterface());
    worker->getAIUpdateInterface()->aiMoveToPosition(Coord3D{ 100.0f, 400.0f, 0.0f }, CMD_FROM_PLAYER); // straight behind it
    const int backingUp = CombatNames::modelCondition("BACKING_UP");
    int backingFrames = 0, misaligned = 0, shownMisaligned = 0, movingFrames = 0;
    Coord3D last = *worker->getPosition();
    for (int i = 0; i < 60; ++i) {
        step();
        const Coord3D now = *worker->getPosition();
        const float dx = now.x - last.x, dy = now.y - last.y;
        const float facing = worker->getOrientation();
        // the picture: the drawable's presentation pose, the transform the renderer gives the model (GameWorld::refresh_view)
        const Drawable *drawable = drawables.findByObject(worker->getID());
        REQUIRE(drawable);
        float shownBasis[9];
        Coord3D shownAt;
        drawable->getPresentationPose(shownBasis, shownAt);
        const float shownFacing = std::atan2(shownBasis[3], shownBasis[0]);
        if (worker->testModelCondition(backingUp)) ++backingFrames;
        if (dx * dx + dy * dy > 0.25f) {
            ++movingFrames;
            const float heading = std::atan2(dy, dx);
            float diff = std::fabs(heading - facing);
            while (diff > 3.14159265f) diff = std::fabs(diff - 6.2831853f);
            if (diff > 1.0f) ++misaligned;
            float shownDiff = std::fabs(heading - shownFacing);
            while (shownDiff > 3.14159265f) shownDiff = std::fabs(shownDiff - 6.2831853f);
            if (shownDiff > 1.0f) ++shownMisaligned;
            std::printf("BUILDER frame=%d pos=%.2f,%.2f shownAt=%.2f,%.2f facing=%.3f shown=%.3f heading=%.3f backing=%d\n", i, now.x, now.y, shownAt.x, shownAt.y, facing, shownFacing, heading, (int)worker->testModelCondition(backingUp));
        }
        last = now;
    }
    CHECK(movingFrames > 10); // tolerance: the walk length depends on the mod's speed and acceleration; only "it walked" is asserted
    CHECK(backingFrames == 0);
    CHECK(misaligned <= 3); // tolerance: the turn in place at the start (TurnTime 500 ms) lags the first frames of the walk
    CHECK(shownMisaligned <= 3); // tolerance: the same start-of-walk turn, interpolated; the bug drew every walking frame wrong (owner 2026-11-10)
    }
}
