// OpenBFME MODGAP-2: BoredUpdate invokes a targetless power (RW 0x899F9D,
// BFME2 0x89680F). Synthetic data; no game or mod files are included.
#include "doctest.h"
#include "CombatTestUtil.h"
#include "Common/SpecialPower.h"
#include "GameLogic/Module/BoredUpdate.h"
#include "GameLogic/Module/SpecialAbilityModules.h"
#include "GameLogic/Module/SpecialPowerModules.h"
#include "GameLogic/Object/PartitionManager.h"

namespace {
const char kObjects[] =
    "SpecialPower TestBoredPower\n Enum = SPECIAL_SCREECH\n ReloadTime = 30000\nEnd\n"
    "Object TestBoredUnit\n"
    " KindOf = INFANTRY SELECTABLE HERO\n"
    " Geometry = CYLINDER\n GeometryMajorRadius = 5\n GeometryHeight = 10\n"
    " Body = ActiveBody BodyTag\n MaxHealth = 100\n End\n"
    " Behavior = AIUpdateInterface AITag\n End\n"
    " Behavior = SpecialPowerModule PowerTag\n SpecialPowerTemplate = TestBoredPower\n UpdateModuleStartsAttack = Yes\n End\n"
    " Behavior = SpecialAbilityUpdate AbilityTag\n SpecialPowerTemplate = TestBoredPower\n UnpackTime = 200\n PreparationTime = 200\n PackTime = 200\n IgnoreFacingCheck = Yes\n End\n"
    " Behavior = BoredUpdate BoredTag\n ScanDelayTime = 200\n SpecialPowerTemplate = TestBoredPower\n End\n"
    "End\n";

struct BoredWorld : combattest::CombatWorld {
    SpecialPowerStore powers;
    SpecialPowerStore *previous = TheSpecialPowerStore;
    explicit BoredWorld(bool filtered = false) {
        logic->partition().setRegion(0.0f, 0.0f, 1000.0f, 1000.0f); // the test terrain's extent, RW 0x62FCCD
        TheSpecialPowerStore = &powers;
        w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
        std::string objects = kObjects;
        if (filtered) {
            const auto pos = objects.find(" ScanDelayTime = 200\n SpecialPowerTemplate");
            REQUIRE(pos != std::string::npos);
            objects.insert(pos, " BoredFilter = NONE +INFANTRY\n");
        }
        const std::string err = w.load(objects);
        REQUIRE_MESSAGE(err.empty(), err);
    }
    ~BoredWorld() { TheSpecialPowerStore = previous; }
};
}

TEST_CASE("modgap2 bored: a filtered scan reports the unsupported target validation")
{
    BoredWorld w(true);
    Object *obj = w.unit("TestBoredUnit", 'A', 300, 300);
    REQUIRE(obj);
    w.frames(5);
    bool found = false;
    for (const auto &line : w.logic->report().stops) found = found || line == BoredUpdate::stopLine();
    // RW 0x89A0A4: a filter with no other allied object does not reach
    // ActionManager, so it must not report an attempted target validation.
    CHECK_FALSE(found);
    REQUIRE(w.unit("TestBoredUnit", 'A', 310, 300));
    w.frames(5);
    found = false;
    for (const auto &line : w.logic->report().stops) found = found || line == BoredUpdate::stopLine();
    CHECK(found);
}

TEST_CASE("modgap2 bored: targetless power is triggered by the periodic scan")
{
    BoredWorld w;
    Object *obj = w.unit("TestBoredUnit", 'A', 300, 300);
    REQUIRE(obj);
    auto *ability = dynamic_cast<SpecialAbilityUpdate *>(obj->findModule("SpecialAbilityUpdate"));
    REQUIRE(ability);
    w.frames(20);
    CHECK(ability->abilitiesTriggered() == 1);
}

TEST_CASE("modgap2 bored: unpaired BFME2 targetless validation reports a stop")
{
    BoredWorld w;
    w.logic->setEdition(GameEdition::BFME2_106);
    REQUIRE(w.unit("TestBoredUnit", 'A', 300, 300));
    w.frames(5);
    bool found = false;
    for (const auto &line : w.logic->report().stops) found = found || line == BoredUpdate::stopLine();
    CHECK(found);
}
