// PORT-135: synthetic AttachUpdate data and parent tracking (RW 0x89502A, 0x89515C, 0x8953BB). No game files.
#include "doctest.h"
#include "CombatTestUtil.h"
#include "GameLogic/Module/AttachUpdate.h"

using namespace combattest;

namespace
{
const char *const kObjects =
    "Object Port135Hero\n"
    "  KindOf = HERO INFANTRY\n"
    "  Body = ActiveBody ModuleTag_Body\n"
    "    MaxHealth = 100\n"
    "  End\n"
    "End\n"
    "Object Port135Other\n"
    "  KindOf = INFANTRY\n"
    "  Body = ActiveBody ModuleTag_Body\n"
    "    MaxHealth = 100\n"
    "  End\n"
    "End\n"
    "Object Port135Marker\n"
    "  KindOf = INERT\n"
    "  Body = ActiveBody ModuleTag_Body\n"
    "    MaxHealth = 1\n"
    "  End\n"
    "  Behavior = AttachUpdate ModuleTag_Attach\n"
    "    ObjectFilter = NONE +HERO\n"
    "  End\n"
    "End\n"
    "Object Port135Explicit\n"
    "  KindOf = INERT\n"
    "  Behavior = AttachUpdate ModuleTag_Attach\n"
    "    ObjectFilter = NONE +HERO\n"
    "    ScanRange = 2\n"
    "    AlwaysTeleport = No\n"
    "  End\n"
    "End\n";
}

TEST_CASE("PORT-135 defaults: range and teleport match the RW constructor")
{
    // RW 0x89502A loads 10.0 from 0xBD83D8; 0x895043 stores true.
    AttachUpdateModuleData data;
    CHECK(data.m_scanRange == 10.0f);
    CHECK(data.m_alwaysTeleport);
    CHECK_FALSE(data.m_anchorToTopOfGeometry);
}

TEST_CASE("PORT-135 marker: filtered parent is followed and parent death kills the marker")
{
    CombatWorld w(kObjects);
    w.combat().setAutoAcquireEnabled(false);
    Object *other = w.unit("Port135Other", 'B', 201, 200);
    Object *hero = w.unit("Port135Hero", 'B', 206, 200);
    Object *marker = w.unit("Port135Marker", 'A', 200, 200);
    REQUIRE(other != nullptr);
    REQUIRE(hero != nullptr);
    REQUIRE(marker != nullptr);
    AttachUpdate *attach = dynamic_cast<AttachUpdate *>(marker->findModule("AttachUpdate"));
    REQUIRE(attach != nullptr);
    CHECK(attach->update() == UPDATE_SLEEP_NONE);
    REQUIRE(attach->parentID() == hero->getID()); // RW 0x8951D1: nearest matching object within default range
    CHECK(marker->getPosition()->x == hero->getPosition()->x);
    CHECK(marker->wasTeleportGatheredAt(w.logic->getFrame())); // RW 0x895412–0x895420
    for (int i = 0; i < 310; ++i)
    {
        Coord3D pos{206.0f + i, 200.0f, 0.0f};
        hero->setPosition(&pos);
        w.frames(1); // the module wakes every frame (RW 0x895425)
        CHECK(marker->getPosition()->x == pos.x);
    }
    // RW 0x8953BB–0x89545D contains no timer/modifier test: a live parent keeps the marker.
    CHECK_FALSE(marker->isEffectivelyDead());
    hero->kill(DAMAGE_UNRESISTABLE, DEATH_NORMAL);
    REQUIRE(hero->isEffectivelyDead());
    CHECK(attach->update() == UPDATE_SLEEP_FOREVER); // RW 0x8953EA / 0x895453
    CHECK(marker->isEffectivelyDead());
}

TEST_CASE("PORT-135 explicit scan range overrides default")
{
    CombatWorld w(kObjects);
    w.combat().setAutoAcquireEnabled(false);
    Object *hero = w.unit("Port135Hero", 'B', 206, 200);
    Object *marker = w.unit("Port135Explicit", 'A', 200, 200);
    REQUIRE(hero != nullptr);
    REQUIRE(marker != nullptr);
    AttachUpdate *attach = dynamic_cast<AttachUpdate *>(marker->findModule("AttachUpdate"));
    REQUIRE(attach != nullptr);
    CHECK(attach->update() == UPDATE_SLEEP_NONE);
    CHECK(attach->parentID() == 0);
    Coord3D close{201.0f, 200.0f, 0.0f};
    hero->setPosition(&close);
    CHECK(attach->update() == UPDATE_SLEEP_NONE);
    CHECK(attach->parentID() == hero->getID());
    CHECK_FALSE(marker->wasTeleportGatheredAt(w.logic->getFrame())); // explicit No: RW 0x89542E
}
