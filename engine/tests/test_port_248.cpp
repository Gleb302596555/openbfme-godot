// OpenBFME. GPL-3.0. Synthetic data only. These tests cover existing parsed
// fields and adjacent implemented behavior, not the unported stops S-5308,
// S-7408 or the unresolved reader in S-7409.
#include "doctest.h"
#include "CombatTestUtil.h"
#include "GameLogic/Module/RunOffMapBehavior.h"

namespace
{
const char kPort248Ini[] =
    "Object FleeTest\n"
    "  KindOf = INFANTRY\n"
    "  Body = ActiveBody BodyTag\n"
    "    MaxHealth = 100\n"
    "  End\n"
    "  Behavior = AIUpdateInterface AITag\n"
    "  End\n"
    "  Behavior = RunOffMapBehavior FleeTag\n"
    "    RequiresSpecificTrigger = Yes\n"
    "    RunOffMapWaypointName = SyntheticExit\n"
    "    DieOnMap = No\n"
    "  End\n"
    "End\n"
    "Object ClearanceTest\n"
    "  KindOf = INFANTRY\n"
    "  Body = ActiveBody BodyTag\n"
    "    MaxHealth = 100\n"
    "  End\n"
    "  Behavior = ClearanceTestingSlowDeathBehavior DeathTag\n"
    "    ClearanceGeometry = BOX\n"
    "    ClearanceGeometryMajorRadius = 20\n"
    "    ClearanceGeometryMinorRadius = 10\n"
    "    ClearanceGeometryHeight = 10\n"
    "    ClearanceGeometryIsSmall = Yes\n"
    "  End\n"
    "End\n"
    "Object LinkedTest\n"
    "  KindOf = STRUCTURE IMMOBILE\n"
    "  Body = SymbioticStructuresBody BodyTag\n"
    "    Symbiote = SyntheticTarget\n"
    "  End\n"
    "End\n";
}

TEST_CASE("PORT-248 synthetic RunOffMap data and explicit trigger")
{
    combattest::CombatWorld w(kPort248Ini);
    Object *obj = w.unit("FleeTest", 'A', 100, 100);
    REQUIRE(obj != nullptr);
    auto *behavior = dynamic_cast<RunOffMapBehavior *>(obj->findModule("RunOffMapBehavior"));
    REQUIRE(behavior != nullptr);
    auto *data = dynamic_cast<const RunOffMapBehaviorModuleData *>(behavior->getModuleData());
    REQUIRE(data != nullptr);
    // RW 0x865321: RequiresSpecificTrigger prevents automatic triggering.
    CHECK(data->m_waypointName == "SyntheticExit");
    CHECK(data->m_requiresSpecificTrigger);
    CHECK_FALSE(data->m_dieOnMap);
    CHECK_FALSE(behavior->hasTriggered());
    behavior->triggerFlee(); // S-5308: the actual AI movement is not implemented.
    CHECK(behavior->hasTriggered());
}

TEST_CASE("PORT-248 synthetic clearance flag and neighboring geometry")
{
    combattest::CombatWorld w(kPort248Ini);
    Object *obj = w.unit("ClearanceTest", 'A', 200, 200);
    REQUIRE(obj != nullptr);
    auto *behavior = dynamic_cast<ClearanceTestingSlowDeathBehavior *>(obj->findModule("ClearanceTestingSlowDeathBehavior"));
    REQUIRE(behavior != nullptr);
    const auto *data = behavior->clearanceData();
    REQUIRE(data != nullptr);
    // RW 0xAD2B30 parses IsSmall; its downstream reader remains S-7409.
    CHECK(data->m_clearanceGeometryIsSmall);
    REQUIRE(data->m_clearanceGeometry.size() == 1);
    CHECK(data->m_clearanceGeometry[0].type == ObjectGeometry::SHAPE_BOX);
    CHECK(data->m_clearanceGeometry[0].majorRadius == 20.0f);
    CHECK(data->m_clearanceGeometry[0].minorRadius == 10.0f);
}

TEST_CASE("PORT-248 synthetic symbiote data and manual body forwarding")
{
    combattest::CombatWorld w(kPort248Ini);
    Object *target = w.unit("Dummy", 'A', 300, 300);
    Object *proxy = w.unit("LinkedTest", 'A', 350, 300);
    REQUIRE(target != nullptr);
    REQUIRE(proxy != nullptr);
    auto *body = dynamic_cast<SymbioticStructuresBody *>(proxy->getBodyModule());
    REQUIRE(body != nullptr);
    auto *data = dynamic_cast<const SymbioticStructuresBodyModuleData *>(body->getModuleData());
    REQUIRE(data != nullptr);
    CHECK(data->m_symbiote == "SyntheticTarget"); // RW table 0xC0602C
    // Explicit linking exercises existing forwarding (RW 0x8C4EE0), not the
    // missing loadPostProcess name linking recorded in S-7408.
    body->setSymbiote(target->getID());
    CHECK(body->getHealth() == target->getBodyModule()->getHealth());
    CHECK(body->getMaxHealth() == target->getBodyModule()->getMaxHealth());
}
