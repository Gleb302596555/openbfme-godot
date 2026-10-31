// Both editions: RW 0x887191 / 0x861868; BFME2 0x883F19 / onDie 0x85E6B8.
#include "doctest.h"
#include "CombatTestUtil.h"
#include "GameClient/MapChunks.h"
#include "GameEngineDevice/W3DDevice/GameClient/WorldHeightMap.h"
#include "GameLogic/Map/TerrainLogic.h"
#include "GameLogic/Module/ClearanceTestingSlowDeathBehavior.h"

namespace {
const char kClearanceObjects[] =
    "Object TestClearanceCreature\n KindOf = MONSTER SELECTABLE\n"
    " Geometry = CYLINDER\n GeometryMajorRadius = 5\n GeometryHeight = 10\n"
    " Body = ActiveBody BodyTag\n MaxHealth = 100\n End\n"
    " Behavior = ClearanceTestingSlowDeathBehavior DeathTag\n"
    "  DeathTypes = ALL\n DestructionDelay = 10000\n"
    "  ClearanceGeometry = BOX\n ClearanceGeometryMajorRadius = 20\n"
    "  ClearanceGeometryMinorRadius = 20\n ClearanceGeometryHeight = 20\n"
    "  ClearanceGeometryOffset = X:40 Y:0 Z:0\n"
    " End\nEnd\n"
    "Object TestClearanceObstacle\n KindOf = STRUCTURE SELECTABLE\n"
    " Geometry = CYLINDER\n GeometryMajorRadius = 10\n GeometryHeight = 20\n"
    " Body = ActiveBody BodyTag\n MaxHealth = 100\n End\nEnd\n"
    "Object TestClearanceAnchor\n KindOf = MONSTER SELECTABLE\n"
    " Body = ActiveBody BodyTag\n MaxHealth = 100\n End\n"
    " Behavior = ClearanceTestingSlowDeathBehavior DeathTag\n"
    "  DeathTypes = ALL\n DestructionDelay = 10000\n ClearanceGeometry = BOX\n"
    "  ClearanceGeometryRotationAnchorOffset = X:5 Y:0\n"
    " End\nEnd\n"
    "Object TestClearanceSlope\n KindOf = MONSTER SELECTABLE\n"
    " Body = ActiveBody BodyTag\n MaxHealth = 100\n End\n"
    " Behavior = ClearanceTestingSlowDeathBehavior DeathTag\n"
    "  DeathTypes = ALL\n DestructionDelay = 10000\n ClearanceGeometry = BOX\n"
    "  ClearanceGeometryMajorRadius = 20\n ClearanceGeometryMinorRadius = 20\n ClearanceGeometryHeight = 20\n"
    "  ClearanceMaxHeight = 20\n ClearanceMaxHeightFraction = 0.5\n"
    "  ClearanceMinHeight = -20\n ClearanceMinHeightFraction = 0.5\n"
    " End\nEnd\n";
}

TEST_CASE("modgap2 clearance death: rotation anchor does not reject a clear area") {
    for (GameEdition edition : { GameEdition::RotWK201, GameEdition::BFME2_106 }) {
        combattest::CombatWorld w(kClearanceObjects);
        w.logic->setEdition(edition);
        CAPTURE(gameEditionName(edition));
        Object *creature = w.unit("TestClearanceAnchor", 'A', 300, 300);
        REQUIRE(creature);
        auto *death = dynamic_cast<ClearanceTestingSlowDeathBehavior *>(creature->findModule("ClearanceTestingSlowDeathBehavior"));
        REQUIRE(death);
        CHECK(death->clearanceSatisfied());
    }
}

TEST_CASE("modgap2 clearance death: an obstacle rejects the selected death") {
    for (GameEdition edition : { GameEdition::RotWK201, GameEdition::BFME2_106 }) {
        combattest::CombatWorld w(kClearanceObjects);
        w.logic->setEdition(edition);
        CAPTURE(gameEditionName(edition));
        Object *creature = w.unit("TestClearanceCreature", 'A', 300, 300);
        REQUIRE(creature);
        auto *death = dynamic_cast<ClearanceTestingSlowDeathBehavior *>(creature->findModule("ClearanceTestingSlowDeathBehavior"));
        REQUIRE(death);
        CHECK(death->clearanceSatisfied());
        Object *obstacle = w.unit("TestClearanceObstacle", 'A', 340, 300);
        REQUIRE(obstacle);
        CHECK_FALSE(death->clearanceSatisfied());
        creature->kill(DAMAGE_UNRESISTABLE, 0);
        CHECK_FALSE(death->isSlowDeathActivated());
    }
}

TEST_CASE("modgap2 clearance death: an unobstructed selected death starts") {
    for (GameEdition edition : { GameEdition::RotWK201, GameEdition::BFME2_106 }) {
        combattest::CombatWorld w(kClearanceObjects);
        w.logic->setEdition(edition);
        CAPTURE(gameEditionName(edition));
        Object *creature = w.unit("TestClearanceCreature", 'A', 300, 300);
        REQUIRE(creature);
        auto *death = dynamic_cast<ClearanceTestingSlowDeathBehavior *>(creature->findModule("ClearanceTestingSlowDeathBehavior"));
        REQUIRE(death);
        REQUIRE(death->clearanceSatisfied());
        creature->kill(DAMAGE_UNRESISTABLE, 0);
        CHECK(death->isSlowDeathActivated());
    }
}

TEST_CASE("modgap2 clearance death: terrain at the height limit is allowed") {
    for (GameEdition edition : { GameEdition::RotWK201, GameEdition::BFME2_106 }) {
        combattest::CombatWorld w(kClearanceObjects);
        w.logic->setEdition(edition);
        CAPTURE(gameEditionName(edition));
        WorldHeightMap map;
        map.m_width = 101; map.m_height = 101; map.m_borderSize = 0;
        map.m_dataSize = 101 * 101;
        map.m_data.assign((size_t)map.m_dataSize, 0);
        map.m_hasBlendTileData = true;
        map.m_tileNdxes.assign((size_t)map.m_dataSize, 0);
        map.m_blendTileNdxes.assign((size_t)map.m_dataSize, 0);
        map.m_extraBlendTileNdxes.assign((size_t)map.m_dataSize, 0);
        map.m_cliffInfoNdxes.assign((size_t)map.m_dataSize, 0);
        map.m_planeStride = (101 + 7) / 8;
        map.m_blendedTiles.assign(1, TBlendTileInfo());
        map.m_cliffInfo.assign(1, TCliffInfo());
        map.m_boundaries = { { 101, 101 } };
        MapChunks chunks;
        TerrainLogic terrain;
        std::vector<std::string> problems;
        terrain.init(map, chunks, &problems);
        REQUIRE(problems.empty());
        w.logic->setTerrain(&terrain);
        Object *creature = w.unit("TestClearanceSlope", 'A', 300, 300);
        REQUIRE(creature);
        auto *death = dynamic_cast<ClearanceTestingSlowDeathBehavior *>(creature->findModule("ClearanceTestingSlowDeathBehavior"));
        REQUIRE(death);
        const auto atHeight = [&](float z) {
            const Coord3D at{300, 300, z};
            creature->setPosition(&at);
            return death->clearanceSatisfied();
        };
        CHECK(atHeight(-20.0f)); // equal to the upper limit is not high
        CHECK_FALSE(atHeight(-21.0f));
        CHECK(atHeight(20.0f)); // equal to the lower limit is not low
        CHECK_FALSE(atHeight(21.0f));
        w.logic->setTerrain(nullptr);
    }
}
