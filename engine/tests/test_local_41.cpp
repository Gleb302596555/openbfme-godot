// OpenBFME unit tests: LOCAL-41 weapon aim at the victim's contact point (issue #330).
// RotWK addresses:
//   WeaponTemplate::adjustTargetPositionOnVictim RW 0x6CB85A (preferred bone, else seed draw + preference-3 contact point)
//   Object::getWorldspaceBestContactPoint RW 0x690BD2
//   GeometryInfo::getBestContactPoint RW 0xAD30E0 preference 3
//   PreferredTargetBone helper RW 0x6CB81A
// Each test fails without the change: the port's S-362 geometry-centre aim
// (z = victim.z + height * 0.5f) is replaced by the retail contact-point aim.

#include "doctest.h"

#include "CombatTestUtil.h"

#include "GameLogic/Combat/ObjectWeapons.h"
#include "GameLogic/Module/ProjectileModules.h"
#include "GameLogic/Object/ObjectGeometry.h"

#include <cmath>
#include <string>

using namespace combattest;

namespace
{
const char *kLocal41Shooter =
    "Object Local41Shooter\n"
    "  KindOf = INFANTRY SELECTABLE CAN_ATTACK SCORE\n"
    "  VisionRange = 250\n"
    "  Geometry = CYLINDER\n"
    "  GeometryMajorRadius = 8\n"
    "  GeometryMinorRadius = 8\n"
    "  GeometryHeight = 20\n"
    "  ArmorSet\n    Conditions = None\n    Armor = PlainArmor\n  End\n"
    "  WeaponSet\n    Conditions = None\n    Weapon = PRIMARY Arc20\n  End\n"
    "  Body = ActiveBody ModuleTag_Body\n    MaxHealth = 60\n  End\n"
    "  Behavior = AIUpdateInterface ModuleTag_AI\n  End\n"
    "  Behavior = DestroyDie ModuleTag_Destroy\n  End\n"
    "  LocomotorSet\n    Locomotor = WalkerLoco\n    Condition = SET_NORMAL\n    Speed = 55\n  End\n"
    "End\n";

// Large BOX so contact points inside it collide with the geometry (radius-2 probe).
// height = 100, shape centre at offset + (0, 0, 50).
const char *kLocal41TargetBase =
    "Object Local41Target\n"
    "  KindOf = INFANTRY SELECTABLE SCORE\n"
    "  Geometry = BOX\n"
    "  GeometryMajorRadius = 50\n"
    "  GeometryMinorRadius = 50\n"
    "  GeometryHeight = 100\n"
    "  ArmorSet\n    Conditions = None\n    Armor = PlainArmor\n  End\n"
    "  Body = ActiveBody ModuleTag_Body\n    MaxHealth = 100\n  End\n"
    "  Behavior = AIUpdateInterface ModuleTag_AI\n  End\n"
    "  Behavior = DestroyDie ModuleTag_Destroy\n  End\n"
    "  LocomotorSet\n    Locomotor = WalkerLoco\n    Condition = SET_NORMAL\n    Speed = 55\n  End\n"
    "End\n";

const char *kLocal41TargetOffset =
    "Object Local41TargetOffset\n"
    "  KindOf = INFANTRY SELECTABLE SCORE\n"
    "  Geometry = BOX\n"
    "  GeometryMajorRadius = 50\n"
    "  GeometryMinorRadius = 50\n"
    "  GeometryHeight = 100\n"
    "  AttackContactPoint = X:100 Y:0 Z:50\n"
    "  GeometryOffset = X:10 Y:0 Z:0\n"
    "  ArmorSet\n    Conditions = None\n    Armor = PlainArmor\n  End\n"
    "  Body = ActiveBody ModuleTag_Body\n    MaxHealth = 100\n  End\n"
    "  Behavior = AIUpdateInterface ModuleTag_AI\n  End\n"
    "  Behavior = DestroyDie ModuleTag_Destroy\n  End\n"
    "  LocomotorSet\n    Locomotor = WalkerLoco\n    Condition = SET_NORMAL\n    Speed = 55\n  End\n"
    "End\n";

const char *kLocal41TargetTwoPoints =
    "Object Local41TargetTwoPoints\n"
    "  KindOf = INFANTRY SELECTABLE SCORE\n"
    "  Geometry = BOX\n"
    "  GeometryMajorRadius = 50\n"
    "  GeometryMinorRadius = 50\n"
    "  GeometryHeight = 100\n"
    "  GeometryContactPoint = X:10 Y:0 Z:50\n"
    "  GeometryContactPoint = X:100 Y:0 Z:50\n"
    "  ArmorSet\n    Conditions = None\n    Armor = PlainArmor\n  End\n"
    "  Body = ActiveBody ModuleTag_Body\n    MaxHealth = 100\n  End\n"
    "  Behavior = AIUpdateInterface ModuleTag_AI\n  End\n"
    "  Behavior = DestroyDie ModuleTag_Destroy\n  End\n"
    "  LocomotorSet\n    Locomotor = WalkerLoco\n    Condition = SET_NORMAL\n    Speed = 55\n  End\n"
    "End\n";

const char *kLocal41TargetThreePoints =
    "Object Local41TargetThreePoints\n"
    "  KindOf = INFANTRY SELECTABLE SCORE\n"
    "  Geometry = BOX\n"
    "  GeometryMajorRadius = 50\n"
    "  GeometryMinorRadius = 50\n"
    "  GeometryHeight = 100\n"
    "  GeometryContactPoint = X:10 Y:0 Z:50\n"
    "  GeometryContactPoint = X:30 Y:0 Z:50\n"
    "  GeometryContactPoint = X:50 Y:0 Z:50\n"
    "  ArmorSet\n    Conditions = None\n    Armor = PlainArmor\n  End\n"
    "  Body = ActiveBody ModuleTag_Body\n    MaxHealth = 100\n  End\n"
    "  Behavior = AIUpdateInterface ModuleTag_AI\n  End\n"
    "  Behavior = DestroyDie ModuleTag_Destroy\n  End\n"
    "  LocomotorSet\n    Locomotor = WalkerLoco\n    Condition = SET_NORMAL\n    Speed = 55\n  End\n"
    "End\n";

const char *kLocal41TargetHighZ =
    "Object Local41TargetHighZ\n"
    "  KindOf = INFANTRY SELECTABLE SCORE\n"
    "  Geometry = BOX\n"
    "  GeometryMajorRadius = 50\n"
    "  GeometryMinorRadius = 50\n"
    "  GeometryHeight = 100\n"
    "  GeometryContactPoint = X:0 Y:0 Z:200\n"
    "  GeometryContactPoint = X:0 Y:0 Z:200\n"
    "  ArmorSet\n    Conditions = None\n    Armor = PlainArmor\n  End\n"
    "  Body = ActiveBody ModuleTag_Body\n    MaxHealth = 100\n  End\n"
    "  Behavior = AIUpdateInterface ModuleTag_AI\n  End\n"
    "  Behavior = DestroyDie ModuleTag_Destroy\n  End\n"
    "  LocomotorSet\n    Locomotor = WalkerLoco\n    Condition = SET_NORMAL\n    Speed = 55\n  End\n"
    "End\n";

namespace
{
// a ProjectileLaunchOffsets that answers the PreferredTargetBone query for every object
// (the victim "has" the bone); the bone's translation is (7, 8, 9) model space
class TargetBoneOffsets : public ProjectileLaunchOffsets
{
public:
	bool launchOffset(const Object &, int, int, float[12]) override { return false; }
	bool singleLogicalBone(const Object &, const std::string &bone, float out[12]) override
	{
		if (bone != "TARGETBONE")
		{
			return false;
		}
		for (int i = 0; i < 12; ++i)
		{
			out[i] = (i == 0 || i == 5 || i == 10) ? 1.0f : 0.0f;
		}
		out[3] = 7.0f;
		out[7] = 8.0f;
		out[11] = 9.0f;
		return true;
	}
};
} // namespace

Coord3D aimAt(Object *victim, const Coord3D &caller, int preference, int seed)
{
    Coord3D out;
    REQUIRE_MESSAGE(ObjectGeometry::worldspaceBestContactPoint(*victim, caller, "", preference, seed, false, out),
        "worldspaceBestContactPoint returned false");
    return out;
}

} // namespace

TEST_CASE("LOCAL-41 contact point: one shape without contact points aims at height * 0.67 (RW 0xAD30E0 pref 3, count < 2)")
{
    CombatWorld w((std::string(kLocal41Shooter) + kLocal41TargetBase).c_str());
    Object *victim = w.unit("Local41Target", 'B', 0.0f, 0.0f);
    REQUIRE(victim != nullptr);
    w.frames(2);

    const std::vector<ObjectGeometry::ContactPoint> pts =
        ObjectGeometry::contactPointsOf(*victim->getTemplate());
    REQUIRE(pts.empty());

    // RW 0xAD25B0: the default / innermost point = (0, 0, height * 0.67f) = (0, 0, 67)
    Coord3D out = aimAt(victim, Coord3D{ 0.0f, 0.0f, 0.0f }, 3, 0);
    CHECK(out.x == 0.0f);
    CHECK(out.y == 0.0f);
    // tolerance: height * 0.67f = 67.0f, exact float from RW 0xBDD984
    CHECK(out.z == doctest::Approx(67.0f)); // 100 * 0.67; S-362's centre would give 50
}

TEST_CASE("LOCAL-41 contact point: two contact points blend by the seed (RW 0xAD30E0 pref 3, count >= 2)")
{
    // hand-crafted shapes and points to bypass INI parsing
    std::vector<ObjectGeometry::Shape> shapes;
    ObjectGeometry::Shape box;
    box.type = ObjectGeometry::SHAPE_BOX;
    box.height = 100.0f;
    box.majorRadius = 50.0f;
    box.minorRadius = 50.0f;
    box.offsetX = 10.0f;
    box.offsetY = 0.0f;
    box.offsetZ = 0.0f;
    box.active = true;
    shapes.push_back(box);

    std::vector<ObjectGeometry::ContactPoint> pts;
    pts.push_back({ 10.0f, 0.0f, 50.0f, "" });   // inside the BOX
    pts.push_back({ 100.0f, 0.0f, 50.0f, "" });  // outside the BOX (x in [-40, 60])

    // seed 65280 (0xFF00): (seed >> 8) & 0xFF = 255, blend = 1.0 -> exactly contact[0] = (10, 0, 50)
    Coord3D a;
    REQUIRE(ObjectGeometry::getBestContactPoint(shapes, pts, Coord3D{ 0.0f, 0.0f, 0.0f }, "", 3, 65280, false, a));
    // tolerance: exact contact point coordinates from hand-crafted test data
    CHECK(a.x == doctest::Approx(10.0f));
    CHECK(a.y == 0.0f);
    // tolerance: exact contact point z from hand-crafted test data
    CHECK(a.z == doctest::Approx(50.0f));

    // seed 1: blend = 0 -> exactly contact[1] = (100, 0, 50), outside the BOX -> fallback to shape centre
    // first active shape centre = (10, 0, 100 * 0.5 + 0) = (10, 0, 50)
    Coord3D b;
    REQUIRE(ObjectGeometry::getBestContactPoint(shapes, pts, Coord3D{ 0.0f, 0.0f, 0.0f }, "", 3, 1, false, b));
    // tolerance: exact fallback shape centre coordinates
    CHECK(b.x == doctest::Approx(10.0f));   // fallback to shape centre x = 10
    CHECK(b.y == 0.0f);
    // tolerance: exact fallback shape centre z
    CHECK(b.z == doctest::Approx(50.0f));   // fallback to shape centre z = 50

    // seed 0: blend = 0 -> same fallback
    Coord3D c;
    REQUIRE(ObjectGeometry::getBestContactPoint(shapes, pts, Coord3D{ 0.0f, 0.0f, 0.0f }, "", 3, 0, false, c));
    // tolerance: exact fallback shape centre x
    CHECK(c.x == doctest::Approx(10.0f));
    // tolerance: exact fallback shape centre z
    CHECK(c.z == doctest::Approx(50.0f));
}

TEST_CASE("LOCAL-41 contact point: three contact points pick a neighbouring pair by the seed (RW 0xAD30E0 pref 3, count = 3)")
{
    CombatWorld w((std::string(kLocal41Shooter) + kLocal41TargetThreePoints).c_str());
    Object *victim = w.unit("Local41TargetThreePoints", 'B', 0.0f, 0.0f);
    REQUIRE(victim != nullptr);
    w.frames(2);

    const std::vector<ObjectGeometry::ContactPoint> pts =
        ObjectGeometry::contactPointsOf(*victim->getTemplate());
    REQUIRE(pts.size() == 3);

    // seed 0: idx = 0 % 2 = 0, blend = 0 -> contact[1] = (30, 0, 50)
    Coord3D a = aimAt(victim, Coord3D{ 0.0f, 0.0f, 0.0f }, 3, 0);
    // tolerance: exact contact point coordinate from INI text
    CHECK(a.x == doctest::Approx(30.0f));
    // tolerance: exact contact point z from INI text
    CHECK(a.z == doctest::Approx(50.0f));

    // seed 1: idx = 1 % 2 = 1, blend = 0 -> contact[2] = (50, 0, 50)
    Coord3D b = aimAt(victim, Coord3D{ 0.0f, 0.0f, 0.0f }, 3, 1);
    // tolerance: exact contact point coordinate from INI text
    CHECK(b.x == doctest::Approx(50.0f));
    // tolerance: exact contact point z from INI text
    CHECK(b.z == doctest::Approx(50.0f));

    // both are contact-point blends, never S-362's centre (x = 0)
    // tolerance: contact-point blend result must be strictly positive x
    CHECK(a.x > 0.0f);
    // tolerance: contact-point blend result must be strictly positive x
    CHECK(b.x > 0.0f);
}

TEST_CASE("LOCAL-41 contact point: the clamp keeps z within [height * 0.1f, height] (RW 0xAD30E0 pref 3)")
{
    CombatWorld w((std::string(kLocal41Shooter) + kLocal41TargetHighZ).c_str());
    Object *victim = w.unit("Local41TargetHighZ", 'B', 0.0f, 0.0f);
    REQUIRE(victim != nullptr);
    w.frames(2);

    // both contact points at z = 200, blend = (0, 0, 200); clamp z <= height -> 100;
    // 100 >= height * 0.1f = 10, so out.z = 100
    Coord3D out = aimAt(victim, Coord3D{ 0.0f, 0.0f, 0.0f }, 3, 0);
    CHECK(out.x == 0.0f);
    CHECK(out.y == 0.0f);
    CHECK(out.z == doctest::Approx(100.0f)); // tolerance: clamp to height = 100.0f; S-362's centre would give z = 50
}

TEST_CASE("LOCAL-41 contact point: RUBBLE body puts the point at the geometry's height (RW 0x690BD2)")
{
    CombatWorld w((std::string(kLocal41Shooter) + kLocal41TargetBase).c_str());
    Object *victim = w.unit("Local41Target", 'B', 0.0f, 0.0f);
    REQUIRE(victim != nullptr);
    w.frames(2);

    auto *body = dynamic_cast<ActiveBody *>(victim->getBodyModule());
    REQUIRE(body != nullptr);
    body->friend_setHealthRaw(0.0f); // BODY_RUBBLE
    REQUIRE(victim->getBodyModule()->getDamageState() == BODY_RUBBLE);

    Coord3D out = aimAt(victim, Coord3D{ 0.0f, 0.0f, 0.0f }, 3, 0);
    CHECK(out.x == 0.0f);
    CHECK(out.y == 0.0f);
    CHECK(out.z == doctest::Approx(100.0f)); // tolerance: maxHeightAbovePosition = 100; S-362's centre would give 50
}

TEST_CASE("LOCAL-41 aim: PreferredTargetBone answers with no logic draw (RW 0x6CB85A -> 0x6CB81A)")
{
    CombatWorld w(std::string(kLocal41Shooter).c_str());
    Object *shooter = w.unit("Local41Shooter", 'A', 0.0f, 0.0f);
    Object *victim = w.unit("Dummy", 'B', 100.0f, 0.0f);
    REQUIRE(shooter != nullptr);
    REQUIRE(victim != nullptr);
    w.frames(2);

    TargetBoneOffsets bones;
    w.combat().setLaunchOffsets(&bones);
    w.logic->random().enableCallLog(true);
    ObjectWeapons::createAndFireTempWeaponAt(TheWeaponStore->findWeaponTemplate("BoneBow"), shooter, *victim);
    Object *arrow = nullptr;
    for (Object *o = w.logic->getFirstObject(); o; o = o->getNextObject())
    {
        if (o->getTemplate()->getName() == "Arrow" && !o->isDestroyed())
        {
            arrow = o;
            break;
        }
    }
    REQUIRE(arrow != nullptr);

    const BezierProjectileBehavior *b = nullptr;
    for (const std::unique_ptr<BehaviorModule> &m : arrow->modules())
    {
        if (BezierProjectileBehavior *pb = dynamic_cast<BezierProjectileBehavior *>(m.get()))
        {
            b = pb;
            break;
        }
    }
    REQUIRE(b != nullptr);

    const Coord3D &vp = *victim->getPosition();
    // tolerance: exact bone translation (7, 8, 9) added to victim position
    CHECK(b->flightEnd().x == doctest::Approx(vp.x + 7.0f));
    // tolerance: exact bone translation (7, 8, 9) added to victim position
    CHECK(b->flightEnd().y == doctest::Approx(vp.y + 8.0f));
    // tolerance: exact bone translation (7, 8, 9) added to victim position
    CHECK(b->flightEnd().z == doctest::Approx(vp.z + 9.0f));

    // The bone path draws nothing from the logic random (Weapon.cpp:1749)
    int draws1749 = 0;
    for (const auto &c : w.logic->random().callLog())
    {
        if (c.line == 1749)
            ++draws1749;
    }
    CHECK(draws1749 == 0);
}
