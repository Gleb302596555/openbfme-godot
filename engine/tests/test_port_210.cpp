// OpenBFME. GPL-3.0. Synthetic data only.
// RW 0xB6D060 -> 0x68EEC3 dispatches collide interfaces for contacts;
// HordeContain's vtable 0xC5D250 slot 0 is RW 0x86C61E.
#include "doctest.h"
#include "CombatTestUtil.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object/Contain/HordeContainRuntime.h"
#include "GameLogic/Object/Object.h"

using namespace combattest;
namespace
{
const char kObjects[] = R"ini(
Object Port210Member
  KindOf = INFANTRY SELECTABLE CAN_CLIMB_WALLS
  Geometry = CYLINDER
  GeometryMajorRadius = 5
  GeometryMinorRadius = 5
  GeometryHeight = 20
  Body = ActiveBody BodyTag
    MaxHealth = 100
  End
  Behavior = AIUpdateInterface AITag
  End
  Behavior = PhysicsBehavior PhysicsTag
  End
  LocomotorSet
    Locomotor = Port210MemberLoco
    Condition = SET_NORMAL
    Speed = 50
  End
End
Object Port210Horde
  KindOf = HORDE MELEE_HORDE SELECTABLE
  Geometry = BOX
  GeometryMajorRadius = 20
  GeometryMinorRadius = 20
  GeometryHeight = 20
  Body = ImmortalBody BodyTag
    MaxHealth = 1
  End
  Behavior = HordeAIUpdate AITag
  End
  Behavior = HordeContain ContainTag
    RankInfo = RankNumber:1 UnitType:Port210Member Position:X:0 Y:0
    Slots = 1
    InitialPayload = Port210Member 1
  End
  LocomotorSet
    Locomotor = Port210Loco
    Condition = SET_NORMAL
    Speed = 50
  End
End
Object Port210Wall
  KindOf = STRUCTURE IMMOBILE SCALEABLE_WALL
  Geometry = BOX
  GeometryMajorRadius = 20
  GeometryMinorRadius = 10
  GeometryHeight = 50
  Body = ImmortalBody BodyTag
    MaxHealth = 100
  End
End
)ini";
const char kLoco[] = R"ini(
Locomotor Port210MemberLoco
  Surfaces = GROUND
  TurnTime = 500
  TurnTimeDamaged = 500
  Acceleration = 510
  Braking = 510
  ZAxisBehavior = SCALING_WALLS
  Appearance = TWO_LEGS
  ScalesWalls = Yes
End
Locomotor Port210Loco
  Surfaces = GROUND
  TurnTime = 2000
  TurnTimeDamaged = 2000
  Acceleration = 500
  Braking = 500
  ZAxisBehavior = NO_Z_MOTIVE_FORCE
  Appearance = HORDE
  ScalesWalls = Yes
End
)ini";
struct Fixture : CombatWorld
{
    Fixture() : CombatWorld(kObjects)
    {
        const std::string err = w.load(kLoco, INI_LOAD_OVERWRITE, "port210-loco.ini");
        REQUIRE_MESSAGE(err.empty(), err);
    }
};
}

TEST_CASE("PORT-210 static wall contact starts horde scaling")
{
    Fixture f;
    Object *horde = f.unit("Port210Horde", 'A', 300, 300);
    Object *wall = f.unit("Port210Wall", 'B', 310, 300);
    auto *contain = dynamic_cast<HordeContain *>(horde->getContain());
    REQUIRE(contain != nullptr);
    REQUIRE(horde->getAIUpdateInterface()->curLocomotor() != nullptr);
    REQUIRE(f.membersOf(horde).size() == 1);
    CHECK(contain->wallToScale() == 0);
    f.frames(1); // RW 0x62E93B: partition and collision phase
    // RW 0x86C633..0x86C6DF: valid wall, ScalesWalls, |dot| >= 0.5.
    CHECK(contain->wallToScale() == wall->getID());
    CHECK(contain->isScalingWall());
    const int bit = AIUpdateInterface::modelConditionBit("SCALING_WALL");
    CHECK(horde->testModelCondition(bit));
    CHECK(f.membersOf(horde)[0]->testModelCondition(bit)); // RW 0x694233
}

TEST_CASE("PORT-210 static wall contact rejects sideways facing and disabled scaling")
{
    Fixture f;
    Object *horde = f.unit("Port210Horde", 'A', 300, 300);
    Object *wall = f.unit("Port210Wall", 'B', 310, 300);
    auto *contain = dynamic_cast<HordeContain *>(horde->getContain());
    REQUIRE(contain != nullptr);
    wall->setOrientation(1.57079632679f);
    f.frames(1);
    CHECK(contain->wallToScale() == 0); // RW 0x86C6C1..0x86C6D2
    wall->setOrientation(0.0f);
    f.store.findLocomotorTemplate("Port210Loco")->m_scalesWalls = false;
    f.frames(1);
    CHECK(contain->wallToScale() == 0); // RW 0x86C676..0x86C680
}

// The horde is placed across the wall after a real collision, to isolate the
// member hub from pathfinding (RW 0x871897 -> 0x86E214 -> 0x8747AE).
// A separate end-to-end moving-horde test is still required.
TEST_CASE("PORT-210 collision lets a climbing member rise on a wall")
{
    Fixture f;
    Object *horde = f.unit("Port210Horde", 'A', 300, 300);
    Object *wall = f.unit("Port210Wall", 'B', 310, 300);
    f.ai->addObjectToPathfindMap(*wall);
    auto *contain = dynamic_cast<HordeContain *>(horde->getContain());
    REQUIRE(contain != nullptr);
    const auto members = f.membersOf(horde);
    REQUIRE(members.size() == 1);
    f.frames(1);
    REQUIRE(contain->wallToScale() == wall->getID());
    Coord3D across{ 340.0f, 300.0f, 0.0f };
    Coord3D atWall{ 310.0f, 300.0f, 0.0f };
    horde->setPosition(&across);
    members[0]->setPosition(&atWall);
    bool climbed = false;
    for (int i = 0; i < 30; ++i)
    {
        f.frames(1);
        climbed = climbed || members[0]->getPosition()->z > 0.0f;
    }
    CHECK(climbed); // RW 0x86E214 raises the member; RW 0x8747AE places it.
}
