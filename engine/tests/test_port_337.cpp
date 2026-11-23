// OpenBFME. GPL-3.0. Synthetic INI and map only; no game files.
// Expectations: RW 0x6860E0, RW 0x6866CE, RW 0x42ECB2, RW 0x687748,
// RW 0x462520 -> RW 0x466AC3 -> RW 0x4ADC78, RW 0x6886E0, RW 0x685EB0.
#include "doctest.h"
#include "IniTestUtil.h"
#include "LogicTestUtil.h"
#include "GameLogic/Map/FireLogicSystem.h"
#include "GameLogic/Map/TerrainLogic.h"
#include "GameLogic/Object/RetailObjectWorld.h"

namespace
{
const char kFireIni[] =
    "FireLogicSystem\n"
    " MaxCellsBurning = 10\n"
    " FuelRegenerationRate = 3\n"
    " TerrainCellType 1\n"
    "  Name = First\n"
    "  Color = R:1 G:2 B:3\n"
    "  Fuel = 70\n"
    "  MaxBurnRate = 20\n"
    "  Decay = 2\n"
    "  Resistance = 9\n"
    " End\n"
    " TerrainCellType 2\n"
    "  Name = Second\n"
    "  Color = R:3 G:2 B:1\n"
    "  Fuel = 90\n"
    "  MaxBurnRate = 11\n"
    "  Decay = 7\n"
    "  Resistance = 5\n"
    " End\n"
    "End\n";

struct Grid
{
    WorldHeightMap map;
    MapChunks chunks;
    TerrainLogic terrain;
    logictest::LogicWorld world;

    Grid()
    {
        map.m_width = 20;
        map.m_height = 20;
        map.m_borderSize = 2;
        map.m_flammability.assign(400, 0);
        terrain.init(map, chunks);
        world.logic->setTerrain(&terrain);
        INIEnvironment env;
        env.blocks.registerBlock("FireLogicSystem", [this](INI *ini) { world.logic->fireLogic().parseINI(ini); });
        REQUIRE_MESSAGE(initest::loadError(env, "synthetic.ini", kFireIni).empty(), "fire INI parse");
    }
    void material(int x, int y, int type)
    {
        map.m_flammability[(y + 2) * 20 + x + 2] = static_cast<std::uint8_t>(type);
    }
    FireLogicSystem &fire() { return world.logic->fireLogic(); }
    void newMap() { fire().newMap(0, 0, 90, 20); }
};
}

TEST_CASE("PORT-337 terrain materials seed cells and control decay")
{
    Grid g;
    g.material(0, 0, 1);
    g.material(0, 2, 2);
    g.newMap();
    CHECK(g.fire().getCellFuel(0, 0) == 70);
    CHECK(g.fire().getCellFuel(0, 1) == 0);
    CHECK(g.fire().getCellFuel(0, 2) == 90);
    g.fire().changeBurnRate(0, 0, 0, 20, false);
    g.fire().changeBurnRate(0, 0, 2, 20, false);
    g.fire().update();
    CHECK(g.fire().getCellBurnRate(0, 0) == 18);
    CHECK(g.fire().getCellBurnRate(0, 2) == 13);
    CHECK(g.fire().getCellFuel(0, 0) == 52);
    CHECK(g.fire().getCellFuel(0, 2) == 77);
}

TEST_CASE("PORT-337 resistance and maximum burn rate govern spread")
{
    Grid g;
    g.material(0, 0, 1);
    g.material(0, 1, 2);
    g.newMap();
    g.fire().changeBurnRate(0, 0, 0, 20, false);
    g.fire().update();
    CHECK(g.fire().getCellBurnRate(0, 1) == 0); // RW 0x688A7E: no spread rows in slice zero
    g.fire().update();
    CHECK(g.fire().getCellBurnRate(0, 1) == 11); // min(18 - resistance 5, max 11)
}

TEST_CASE("PORT-337 burning set cap and zero terrain survive")
{
    Grid g;
    g.material(0, 0, 1);
    g.material(0, 2, 2);
    g.newMap();
    g.fire().setMaxCellsBurning(1);
    g.fire().changeBurnRate(0, 0, 0, 20, false);
    g.fire().changeBurnRate(0, 0, 2, 20, false);
    g.fire().update(); // RW 0x6886E0 processes descending y: y=2 is admitted first
    CHECK(g.fire().cellsBurningCount() == 1);
    CHECK(g.fire().getCellBurnRate(0, 2) == 13);
    CHECK(g.fire().getCellBurnRate(0, 0) == 0);
    CHECK(g.fire().getCellFuel(0, 1) == 0);
}

TEST_CASE("PORT-337 indexed table rejects values outside retail limits")
{
    FireLogicConfig config;
    INIEnvironment env;
    env.blocks.registerBlock("FireLogicSystem", [&config](INI *ini) { config.parseINI(ini); });
    CHECK_FALSE(initest::loadError(env, "bad-index.ini", "FireLogicSystem\n TerrainCellType 4\n End\nEnd\n").empty());
    CHECK_FALSE(initest::loadError(env, "bad-fuel.ini", "FireLogicSystem\n TerrainCellType 1\n Fuel = 65536\n End\nEnd\n").empty());
}

TEST_CASE("PORT-337 world loader and map override use the same fire configuration")
{
    initest::Fixture fx({
        { "Data\\INI\\Default\\SubsystemLegendExpansion1.ini", "LoadSubsystem TheFireLogicSystem\n Loader = INI\n InitFile = Data\\INI\\FireLogicSystem.ini\nEnd\n" },
        { "Data\\INI\\FireLogicSystem.ini", kFireIni },
        { "maps\\fire\\map.ini", "FireLogicSystem\n MaxCellsBurning = 2\n TerrainCellType 1\n Fuel = 43\n End\nEnd\n" },
    });
    RetailObjectWorld world(fx.fsys);
    std::string error;
    REQUIRE_MESSAGE(world.load(&error), error);
    CHECK(world.fireLogicConfig().maxCellsBurning == 10);
    CHECK(world.fireLogicConfig().materials[1].fuel == 70);
    auto map = world.applyMapIni("maps\\fire");
    REQUIRE(map.mapIniFound);
    CHECK(map.errors.empty());
    CHECK(world.fireLogicConfig().maxCellsBurning == 2);
    CHECK(world.fireLogicConfig().materials[1].fuel == 43);
    CHECK(world.fireLogicConfig().materials[2].fuel == 90);
}
