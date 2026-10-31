// P-14 diagnostic on the owner's mounted Ennorath data; no game data is copied.
// Ring delivery: RW 0x883B4A / 0x8840FC (S-6010); replacement: RW 0x8B1690.
#include "doctest.h"
#include "StructureArena.h"
#include "Common/ModMount.h"
#include "Common/Player.h"
#include "Common/RetailArchivePolicy.h"
#include "Common/Thing/ThingTemplate.h"
#include "GameLogic/Combat/CombatNames.h"
#include "GameLogic/Object/PartitionManager.h"
#include <cstdlib>
#include <filesystem>

TEST_CASE("modgap2 Ennorath P-14: ring delivery then the white upgrade transform Gandalf")
{
    const char *rotwk = std::getenv("ROTWK_INSTALL");
    const char *bfme2 = std::getenv("BFME2_INSTALL");
    const char *mod = std::getenv("OPENBFME_ENNORATH_INSTALL");
    if (!rotwk || !*rotwk || !bfme2 || !*bfme2 || !mod || !*mod) {
        retailtest::printSkip("P-14 ring chain (set ROTWK_INSTALL, BFME2_INSTALL, OPENBFME_ENNORATH_INSTALL)");
        return;
    }
    std::string error;
    std::vector<RetailInstall> installs(2);
    installs[0].label = "rotwk"; installs[0].root = rotwk;
    installs[1].label = "bfme2"; installs[1].root = bfme2;
    REQUIRE_MESSAGE(RetailArchivePolicy::loadBuiltin("rotwk-201", installs[0].policy, &error), error);
    REQUIRE_MESSAGE(RetailArchivePolicy::loadBuiltin("bfme2-106", installs[1].policy, &error), error);
    Win32BIGFileSystem pure;
    RetailMountOptions options;
    options.md5CachePath = (std::filesystem::temp_directory_path() / "openbfme-modgap2-ring-md5.tsv").u8string();
    const RetailMountReport retail = MountRetailArchives(pure, installs, options);
    REQUIRE_MESSAGE(retail.ok, "retail mount failed");
    retailtest::Mount merged;
    merged.fs = std::make_unique<Win32BIGFileSystem>();
    ModMountOptions modOptions;
    modOptions.rotwkInstallRoot = rotwk;
    modOptions.md5CachePath = options.md5CachePath;
    const ModMountReport mounted = MountInstallOverlay(*merged.fs, retail, mod, modOptions);
    REQUIRE_MESSAGE(mounted.ok, "Ennorath overlay mount failed");
    hudtest::SharedWorld shared;
    shared.mount = &merged;
    shared.world = std::make_unique<RetailObjectWorld>(*merged.fs);
    REQUIRE_MESSAGE(shared.world->load(&error), error);
    REQUIRE(shared.world->report().errors.empty());
    structtest::Arena arena(shared, "FactionMen", "FactionMordor");
    // Match the retail ring-delivery fixture's partition region before creation.
    arena.logic.partition().setRegion(0.0f, 0.0f, 1200.0f, 1200.0f);
    auto spawn = [&](const char *name, float x, float y) {
        const ThingTemplate *t = shared.world->things().findTemplate(name);
        REQUIRE_MESSAGE(t, name);
        Object *obj = arena.logic.newObject(t, arena.player(0)->getDefaultTeam(), ObjectStatusMaskType{});
        REQUIRE_MESSAGE(obj, name);
        const Coord3D position{x, y, 0};
        obj->setPosition(&position);
        return obj;
    };
    Object *gandalf = spawn("GondorGandalf_mod", 500, 400);
    Object *carrier = spawn("GondorFighter", 380, 400);
    Object *ring = spawn("TheDroppedRing", 380, 400);
    const ObjectID original = gandalf->getID();
    for (int i = 0; i < 3; ++i) arena.logic.runLogicFrame();
    REQUIRE(carrier->testStatus((unsigned)CombatNames::status("HOLDING_THE_RING")));
    REQUIRE(gandalf->getContain());
    REQUIRE(gandalf->getContain()->addToContain(carrier));
    CHECK((arena.player(0)->hasUpgradeComplete("Upgrade_RingHero") || gandalf->hasUpgrade("Upgrade_RingHero")));
    CHECK(ring->isDestroyed());
    for (int i = 0; i < 60; ++i) arena.logic.runLogicFrame();
    PartitionFilterFn changed([](Object &o) { return o.getTemplate()->getName() == "GondorGandalfVerfhrte"; });
    const auto replacements = arena.logic.partition().iterateAllObjects({&changed});
    REQUIRE(replacements.size() == 1);
    CHECK(arena.logic.findObjectByID(original) == nullptr);
    const ObjectID corrupted = replacements[0].object->getID();
    arena.player(0)->addCompletedUpgrade("Upgrade_GandalfWhite");
    for (int i = 0; i < 60; ++i) arena.logic.runLogicFrame();
    PartitionFilterFn redeemed([](Object &o) { return o.getTemplate()->getName() == "GondorGandalfGeprfte"; });
    const auto redeemedHeroes = arena.logic.partition().iterateAllObjects({&redeemed});
    CHECK(redeemedHeroes.size() == 1);
    CHECK(arena.logic.findObjectByID(corrupted) == nullptr);
}
