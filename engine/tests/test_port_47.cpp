// PORT-47 (#151): RW 0x8B9269/72, 0x67274A/81, 0x4B872C/3C and 0x54BE43.
// Synthetic INI and meshes only; no installation required. Do not derive expectations from the port.
#include "doctest.h"
#include "EconTestUtil.h"
#include "W3dSynth.h"
#include "GameClient/ClientEvents.h"
#include "GameClient/DrawableManager.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DScriptedModelDraw.h"

namespace {
std::string objects(bool texture, bool recolor, const std::string &exclude, const std::string &kind = "INFANTRY")
{
    return "Upgrade Upgrade_Port47\n Type = OBJECT\nEnd\nObject Port47Unit\n KindOf = " + kind +
        "\n Draw = W3DScriptedModelDraw DrawTag\n DefaultModelConditionState\n Model = P47\n End\n End\n"
        " Body = ActiveBody BodyTag\n MaxHealth = 100\n End\n Behavior = SubObjectsUpgrade UpgradeTag\n"
        " TriggeredBy = Upgrade_Port47\n HideSubObjectsOnRemove = Yes\n" +
        (texture ? " UpgradeTexture = Original.tga 0 Armor.tga\n" : "") +
        (recolor ? " RecolorHouse = Yes\n" : " RecolorHouse = No\n") +
        (exclude.empty() ? "" : " ExcludeSubobjects = " + exclude + "\n") + " End\nEnd\n";
}
void model(w3dsynth::MemoryFileSource &fs)
{
    using namespace w3dsynth;
    std::vector<std::uint8_t> bytes;
    std::vector<SynthSub> subs;
    for (const char *name : { "Blade", "Armor", "Extra.Blade" }) {
        SynthMesh mesh;
        mesh.Container = "P47";
        mesh.Name = name;
        mesh.Vertices = { {0,0,0}, {1,0,0}, {0,1,0} };
        mesh.Triangles = {0,1,2};
        append(bytes, meshChunk(mesh));
        subs.push_back({std::string("P47.") + name, 0});
    }
    append(bytes, hlodChunk("P47", "UnusedSkeleton", subs));
    fs.Add("art\\w3d\\p4\\p47.w3d", bytes);
}
struct World {
    econtest::Fx f;
    w3dsynth::MemoryFileSource fs;
    WW3DAssetManager assets;
    DrawableManager manager;
    ClientEventRecorder recorder;
    explicit World(bool texture = true, bool recolor = true, const std::string &exclude = "bLaDe Armor", const std::string &kind = "INFANTRY")
        : f(objects(texture, recolor, exclude, kind).c_str()), assets(fs), manager(assets, *f.logic), recorder(*f.logic)
    {
        model(fs);
        f.logic->setClientHooks(&recorder);
        f.player("Alice")->setPlayerColor(0xFF102030);
        f.player("Alice")->setPlayerNightColor(0xFF8090A0);
    }
    ~World() { f.logic->setClientHooks(nullptr); }
    Object *make() { return f.make("Port47Unit", f.teamOf("Alice")); }
    const UpgradeTemplate *upgrade() { return TheUpgradeCenter->findUpgrade("Upgrade_Port47"); }
};
}

TEST_CASE("PORT-47 upgrade recolours after texture replacement using day colour and first exclusion only")
{
    World w;
    w.f.logic->settings().night = true;
    Object *obj = w.make();
    w.manager.applyEvents(w.recorder.take());
    Drawable *drawable = w.manager.findByObject(obj->getID());
    REQUIRE(drawable);
    REQUIRE(drawable->entries().size() == 1);
    auto *draw = drawable->entries()[0].draw.get();
    REQUIRE(draw);
    const std::uint32_t custom[] = {0xFF111111, 0xFF222222, 0xFF333333};
    drawable->setCustomColors(2, custom);
    obj->giveUpgrade(w.upgrade());
    auto events = w.recorder.take();
    int texture = -1, recolor = -1, count = 0;
    for (size_t i = 0; i < events.size(); ++i) {
        if (events[i].kind == ClientEvent::UPGRADE_TEXTURES) texture = (int)i;
        if (events[i].kind == ClientEvent::HOUSE_RECOLOR) {
            recolor = (int)i;
            ++count;
            CHECK(events[i].name == "bLaDe");
            CHECK(events[i].houseColor == 0xFF102030);
        }
    }
    CHECK(count == 1);
    CHECK(texture >= 0); // tolerance: event presence; unrelated visibility events may precede the texture operation
    CHECK(recolor == texture + 1);
    w.manager.applyEvents(events);
    const auto &colors = draw->meshHouseColors();
    REQUIRE(colors.count("P47.Blade") == 1); // case-insensitive first-dot suffix, retains base palette
    CHECK(colors.at("P47.Blade").kind == 2);
    CHECK(colors.at("P47.Blade").colors[0] == custom[0]);
    REQUIRE(colors.count("P47.Armor") == 1); // second exclusion is NOT read
    CHECK(colors.at("P47.Armor").kind == 1);
    CHECK(colors.at("P47.Armor").colors[0] == 0xFF102030);
    CHECK(colors.count("P47.Extra.Blade") == 1); // first dot, not last dot
    obj->removeUpgrade(w.upgrade());
    w.manager.applyEvents(w.recorder.take());
    CHECK(draw->meshHouseColors().empty()); // existing Local21 lifecycle, no permanent exclusion filter
}

TEST_CASE("PORT-47 no recolour without texture or RecolorHouse and campaign house-colour gates")
{
    for (int scenario = 0; scenario < 6; ++scenario) {
        CAPTURE(scenario);
        World w(scenario != 0, scenario != 1, "Blade", scenario == 5 ? "BASE_SITE" : "INFANTRY");
        if (scenario == 2) w.f.logic->settings().enableHouseColor = false;
        if (scenario >= 3) w.f.economy().context().livingWorld = true;
        if (scenario == 4) w.f.economy().context().gameKind = 2;
        Object *obj = w.make();
        w.recorder.take();
        obj->giveUpgrade(w.upgrade());
        int count = 0;
        for (const auto &e : w.recorder.take()) count += e.kind == ClientEvent::HOUSE_RECOLOR;
        const int expected[] = {0, 0, 0, 0, 1, 1};
        CHECK(count == expected[scenario]);
    }
}

TEST_CASE("PORT-47 empty exclusion reaches every mesh without changing ordinary palette")
{
    World w(true, true, "");
    Object *obj = w.make();
    w.manager.applyEvents(w.recorder.take());
    obj->giveUpgrade(w.upgrade());
    w.manager.applyEvents(w.recorder.take());
    auto *d = w.manager.findByObject(obj->getID());
    REQUIRE(d);
    REQUIRE(d->entries().size() == 1);
    CHECK(d->entries()[0].draw->meshHouseColors().size() == 3);
}
