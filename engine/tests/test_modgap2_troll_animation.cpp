// Ennorath P-16 diagnostic: live logic and drawable, with the player's own
// mounted mod. No game data is copied into this test. RotWK rules (S-001).
#include "doctest.h"
#include "StructureArena.h"
#include "Common/ModMount.h"
#include "Common/RetailArchivePolicy.h"
#include "GameClient/ClientEvents.h"
#include "GameClient/DrawableManager.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Module/AIUpdate.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>

TEST_CASE("modgap2 Ennorath P-16: trace the cave troll's moving animation")
{
    const char *rotwk = std::getenv("ROTWK_INSTALL");
    const char *bfme2 = std::getenv("BFME2_INSTALL");
    const char *mod = std::getenv("OPENBFME_ENNORATH_INSTALL");
    if (!rotwk || !*rotwk || !bfme2 || !*bfme2 || !mod || !*mod) {
        retailtest::printSkip("P-16 troll animation (set ROTWK_INSTALL, BFME2_INSTALL, OPENBFME_ENNORATH_INSTALL)");
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
    retailOptions.md5CachePath = (std::filesystem::temp_directory_path() / "openbfme-modgap2-troll-md5.tsv").u8string();
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
    structtest::Arena arena(shared, "FactionWild", "FactionMordor");
    ArchiveW3DFileSource source(*merged.fs);
    WW3DAssetManager assets(source);
    DrawableManager drawables(assets, arena.logic);
    ClientEventRecorder events(arena.logic);
    arena.logic.setClientHooks(&events);
    struct UnbindHooks {
        GameLogic &logic;
        ~UnbindHooks() { logic.setClientHooks(nullptr); }
    } unbind{arena.logic};
    const ThingTemplate *tmpl = shared.world->things().findTemplate("GoblinCaveTroll");
    REQUIRE(tmpl);
    Object *troll = arena.logic.newObject(tmpl, arena.player(0)->getDefaultTeam(), ObjectStatusMaskType{});
    REQUIRE(troll);
    const Coord3D start{ 300.0f, 300.0f, 0.0f }, dest{ 900.0f, 450.0f, 0.0f };
    troll->setPosition(&start);
    auto step = [&]() {
        arena.logic.runLogicFrame();
        drawables.applyEvents(events.take());
        drawables.advance(200.0);
    };
    step(); step();
    REQUIRE(troll->getAIUpdateInterface());
    troll->getAIUpdateInterface()->aiMoveToPosition(dest, CMD_FROM_PLAYER);
    bool moved = false, movingAnimation = false, advanced = false;
    Coord3D last = *troll->getPosition();
    std::string lastClip;
    float lastFrame = -1.0f;
    for (int i = 0; i < 80; ++i) {
        if (i == 20) troll->getAIUpdateInterface()->aiMoveToPosition(Coord3D{ 500.0f, 850.0f, 0.0f }, CMD_FROM_PLAYER);
        if (i == 45) troll->getAIUpdateInterface()->aiMoveToPosition(Coord3D{ 900.0f, 450.0f, 0.0f }, CMD_FROM_PLAYER);
        step();
        const Drawable *drawable = drawables.findByObject(troll->getID());
        REQUIRE(drawable);
        const W3DScriptedModelDraw *draw = nullptr;
        for (const DrawEntry &entry : drawable->entries()) {
            if (entry.draw && entry.draw->currentModelState() && entry.draw->currentModel()) { draw = entry.draw.get(); break; }
        }
        REQUIRE(draw);
        const W3DDrawFrame frame = draw->frame();
        const W3DDrawTrack *track = frame.trackCount ? &frame.tracks[frame.trackCount - 1] : nullptr;
        const Coord3D now = *troll->getPosition();
        const bool moving = troll->testModelCondition(CombatNames::modelCondition("MOVING"));
        const bool left = troll->testModelCondition(CombatNames::modelCondition("TURN_LEFT"));
        const bool right = troll->testModelCondition(CombatNames::modelCondition("TURN_RIGHT"));
        std::printf("P16 frame=%d pos=%.3f,%.3f moving=%d left=%d right=%d state=%s clip=%s animation_frame=%.3f\n",
            i, now.x, now.y, moving, left, right, frame.animationStateName.c_str(), track ? track->clipName.c_str() : "<none>", track ? track->frame : -1.0f);
        moved = moved || now.x != last.x || now.y != last.y;
        if (moving && track && track->anim) {
            movingAnimation = true;
            if (track->clipName == lastClip && track->frame != lastFrame) advanced = true;
            lastClip = track->clipName;
            lastFrame = track->frame;
        } else {
            lastClip.clear(); lastFrame = -1.0f;
        }
        last = now;
    }
    CHECK(moved);
    CHECK(movingAnimation);
    CHECK(advanced);
}

// P-30: inspect the actual member drawables of the two affected hordes, not
// the (invisible) horde object. This is a diagnostic until the reported visual
// failure is reproduced; moving a track alone does not prove the pose is right.
TEST_CASE("modgap2 Ennorath P-30: Gondor horde members animate while moving")
{
    const char *rotwk = std::getenv("ROTWK_INSTALL");
    const char *bfme2 = std::getenv("BFME2_INSTALL");
    const char *mod = std::getenv("OPENBFME_ENNORATH_INSTALL");
    if (!rotwk || !*rotwk || !bfme2 || !*bfme2 || !mod || !*mod) {
        retailtest::printSkip("P-30 Gondor animation (set ROTWK_INSTALL, BFME2_INSTALL, OPENBFME_ENNORATH_INSTALL)");
        return;
    }
    std::string err;
    std::vector<RetailInstall> installs(2);
    installs[0].label = "rotwk"; installs[0].root = rotwk;
    installs[1].label = "bfme2"; installs[1].root = bfme2;
    REQUIRE_MESSAGE(RetailArchivePolicy::loadBuiltin("rotwk-201", installs[0].policy, &err), err);
    REQUIRE_MESSAGE(RetailArchivePolicy::loadBuiltin("bfme2-106", installs[1].policy, &err), err);
    Win32BIGFileSystem pure;
    RetailMountOptions options;
    options.md5CachePath = (std::filesystem::temp_directory_path() / "openbfme-modgap2-troll-md5.tsv").u8string();
    const RetailMountReport retail = MountRetailArchives(pure, installs, options);
    REQUIRE(retail.ok);
    retailtest::Mount merged;
    merged.fs = std::make_unique<Win32BIGFileSystem>();
    ModMountOptions modOptions;
    modOptions.rotwkInstallRoot = rotwk; modOptions.md5CachePath = options.md5CachePath;
    REQUIRE(MountInstallOverlay(*merged.fs, retail, mod, modOptions).ok);
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
    struct UnbindHooks { GameLogic &logic; ~UnbindHooks() { logic.setClientHooks(nullptr); } } unbind{arena.logic};
    auto step = [&]() { arena.logic.runLogicFrame(); drawables.applyEvents(events.take()); drawables.advance(200.0); };
    for (const char *name : { "GondorFighterHorde", "GondorArcherHorde" }) {
        const ThingTemplate *tmpl = shared.world->things().findTemplate(name);
        REQUIRE_MESSAGE(tmpl, name);
        Object *horde = arena.logic.newObject(tmpl, arena.player(0)->getDefaultTeam(), ObjectStatusMaskType{});
        REQUIRE(horde);
        Coord3D start{ 300.0f, name[6] == 'F' ? 300.0f : 700.0f, 0.0f };
        horde->setPosition(&start);
        testutil::completeHorde(horde);
        step(); step();
        REQUIRE(horde->getContain());
        const auto *items = horde->getContain()->getContainedItemsList();
        REQUIRE(items);
        REQUIRE(!items->empty());
        Object *member = items->front();
        REQUIRE(member);
        REQUIRE(horde->getAIUpdateInterface());
        horde->getAIUpdateInterface()->aiMoveToPosition(Coord3D{ 900.0f, start.y, 0.0f }, CMD_FROM_PLAYER);
        bool moved = false, animated = false;
        Coord3D last = *member->getPosition();
        std::string clip;
        float previousFrame = -1.0f;
        for (int i = 0; i < 70; ++i) {
            step();
            const Drawable *drawable = drawables.findByObject(member->getID());
            REQUIRE(drawable);
            for (const DrawEntry &entry : drawable->entries()) {
                if (!entry.draw || !entry.draw->currentModel()) continue;
                const W3DDrawFrame f = entry.draw->frame();
                if (!f.trackCount) continue;
                const W3DDrawTrack &t = f.tracks[f.trackCount - 1];
                const Coord3D pos = *member->getPosition();
                const bool moving = member->testModelCondition(CombatNames::modelCondition("MOVING"));
                moved = moved || pos.x != last.x || pos.y != last.y;
                if (moving && t.anim && t.clipName == clip && t.frame != previousFrame) animated = true;
                if (i % 5 == 0) std::printf("P30 %s frame=%d pos=%.2f,%.2f moving=%d clip=%s animation_frame=%.2f\n", name, i, pos.x, pos.y, moving, t.clipName.c_str(), t.frame);
                clip = t.clipName; previousFrame = t.frame; last = pos;
                break;
            }
        }
        CHECK_MESSAGE(moved, name);
        CHECK_MESSAGE(animated, name);
    }
}
