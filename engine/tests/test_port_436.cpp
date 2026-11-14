// OpenBFME unit tests: PORT-436 the weapon aim at the victim's contact point (issue #436).
//
// Issue #436: after Local 41 landed (the weapon aim at the victim's contact point, issue #330), three
// game-level families went red on main: `aiexpand1 expansion in a game`, `save4b retail T4` and
// `skirmish ai vs ai: Evendim ...`. Local 41 replaced the port's S-362 geometry-centre aim
// (z = victim.z + height * 0.5f, deterministic, no logic draw) with the retail contact-point aim
// (RW 0x6CB85A flag 1: one seed draw at Weapon.cpp:1749 per shot, then the preference-3 contact
// point). Every shot of every AI game therefore moves the logic random stream and lands elsewhere,
// which is exactly what the three families' pins (frames, the module-class census, the progress
// line) measure. This is an expectation move, not a regression: the port mechanism below matches
// RotWK 2.01 call for call, and the BFME2 1.06 decomp (WeaponTemplateTargetPositions.cpp,
// adjustTargetPositionOnVictim @0x2CAB3A) carries the same body.
//
// RotWK addresses (game.dat 2.01, md5 daed3668006cd90f01c34e5a7da1901f):
//   WeaponTemplate::adjustTargetPositionOnVictim RW 0x6CB85A (preferred bone 0x6CB81A first;
//     flag 1: seed draw 0x6CB8C2..0x6CB8D3 + getWorldspaceBestContactPoint 0x690BD2 pref 3;
//     flag 0: the victim position + the geometry centre height 0xB4E370, castle clamp 0x6CB944)
//   Object::getWorldspaceBestContactPoint RW 0x690BD2
//   GeometryInfo::getBestContactPoint RW 0xAD30E0 preference 3 (0.67 of the height without
//     contact points, RW 0xAD25B0)
//   the fire FX aim RW 0x6CCAC7 (flag 1), the scatter aim on structures RW 0x6CC3EA (flag 1),
//   the Bezier launch RW 0x85EC51 and homing step RW 0x85F46A (flag 1), the LOS aim RW 0x6CB8F8
//   (flag 0, the wrapper callers 0x6CC6B6 / 0x6CC723 of RW 0x6CC37B).
//
// The tests pin the mechanism for every object that uses it (the fire FX, the Bezier launch, the
// scatter on structures) and hold the neighboring paths that must NOT move with it (the LOS flag 0
// path, the preferred-bone path, the scatter plain path for infantry). Each mechanism test fails
// without the Local 41 aim (the S-362 geometry centre, no seed draw) and passes with it.

#include "doctest.h"

#include "CombatTestUtil.h"

#include "GameLogic/Combat/CombatLineOfSight.h"
#include "GameLogic/Combat/ObjectWeapons.h"
#include "GameLogic/Combat/WeaponDelivery.h"
#include "GameLogic/FXEvents.h"
#include "GameLogic/Module/ProjectileModules.h"
#include "GameLogic/Object/ObjectGeometry.h"
#include "GameLogic/Weapon.h"

#include <string>
#include <vector>

using namespace combattest;

namespace
{
const char *kPort436Shooter =
    "Object Port436Shooter\n"
    "  KindOf = INFANTRY SELECTABLE CAN_ATTACK SCORE\n"
    "  VisionRange = 250\n"
    "  Geometry = CYLINDER\n"
    "  GeometryMajorRadius = 8\n"
    "  GeometryMinorRadius = 8\n"
    "  GeometryHeight = 20\n"
    "  ArmorSet\n"
    "    Conditions = None\n"
    "    Armor = PlainArmor\n"
    "  End\n"
    "  WeaponSet\n"
    "    Conditions = None\n"
    "    Weapon = PRIMARY Arc20\n"
    "  End\n"
    "  Body = ActiveBody ModuleTag_Body\n"
    "    MaxHealth = 60\n"
    "  End\n"
    "  Behavior = AIUpdateInterface ModuleTag_AI\n"
    "  End\n"
    "  Behavior = DestroyDie ModuleTag_Destroy\n"
    "  End\n"
    "  LocomotorSet\n"
    "    Locomotor = WalkerLoco\n"
    "    Condition = SET_NORMAL\n"
    "    Speed = 55\n"
    "  End\n"
    "End\n";

// A BOX of height 100 without contact points: the preference-3 contact point is (0, 0, 67)
// (RW 0xAD25B0, height * 0.67f), the S-362 geometry centre would be (0, 0, 50).
const char *kPort436Target =
    "Object Port436Target\n"
    "  KindOf = INFANTRY SELECTABLE SCORE\n"
    "  Geometry = BOX\n"
    "  GeometryMajorRadius = 50\n"
    "  GeometryMinorRadius = 50\n"
    "  GeometryHeight = 100\n"
    "  ArmorSet\n"
    "    Conditions = None\n"
    "    Armor = PlainArmor\n"
    "  End\n"
    "  Body = ActiveBody ModuleTag_Body\n"
    "    MaxHealth = 100\n"
    "  End\n"
    "  Behavior = AIUpdateInterface ModuleTag_AI\n"
    "  End\n"
    "  Behavior = DestroyDie ModuleTag_Destroy\n"
    "  End\n"
    "  LocomotorSet\n"
    "    Locomotor = WalkerLoco\n"
    "    Condition = SET_NORMAL\n"
    "    Speed = 55\n"
    "  End\n"
    "End\n";

// A ranged weapon with a FireFX, so the fire FX block emits its WEAPON_FIRE_FX event (the standard
// Arc20 carries no FireFX and the block returns before the event, although the aim draw still runs).
const char kPort436FireWeapon[] =
    "Weapon Port436Bow\n"
    "  AttackRange = 200\n"
    "  WeaponSpeed = 100\n"
    "  FireFX = FX_Port436Fire\n"
    "  ProjectileNugget\n"
    "    ProjectileTemplateName = Arrow\n"
    "    WarheadTemplateName = BowWarhead\n"
    "  End\n"
    "End\n";

// A structure wall (like FlankWall): STRUCTURE, BOX 5 x 30 x 60, no AI. The preference-3 contact
// point is (0, 0, 40.2) (60 * 0.67f); the bounding circle sqrt(5^2 + 30^2) + 5 = 35.4 is above the
// ScatterRadius 20, so a missed shot draws no radius.
const char *kPort436Wall =
    "Object Port436Wall\n"
    "  KindOf = STRUCTURE IMMOBILE\n"
    "  Geometry = BOX\n"
    "  GeometryMajorRadius = 5\n"
    "  GeometryMinorRadius = 30\n"
    "  GeometryHeight = 60\n"
    "  ArmorSet\n"
    "    Conditions = None\n"
    "    Armor = NoArmor\n"
    "  End\n"
    "  Body = ActiveBody ModuleTag_Body\n"
    "    MaxHealth = 1000\n"
    "  End\n"
    "End\n";

// a ProjectileLaunchOffsets that answers the PreferredTargetBone query for every object (the bone
// "TARGETBONE" of the BoneBow); the bone's translation is (7, 8, 9) model space
class Port436BoneOffsets : public ProjectileLaunchOffsets
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

int drawsAt(const std::vector<GameLogicRandom::Call> &log, int line)
{
    int n = 0;
    for (const auto &c : log)
    {
        if (c.line == line)
        {
            ++n;
        }
    }
    return n;
}

std::vector<int> weaponDraws(const std::vector<GameLogicRandom::Call> &log)
{
    std::vector<int> lines;
    for (const auto &c : log)
    {
        if (c.file == "Weapon.cpp")
        {
            lines.push_back(c.line);
        }
    }
    return lines;
}

const BezierProjectileBehavior *bezierOf(Object *arrow)
{
    for (const std::unique_ptr<BehaviorModule> &m : arrow->modules())
    {
        if (const BezierProjectileBehavior *b = dynamic_cast<const BezierProjectileBehavior *>(m.get()))
        {
            return b;
        }
    }
    return nullptr;
}

Object *findArrow(GameLogic *logic)
{
    for (Object *o = logic->getFirstObject(); o; o = o->getNextObject())
    {
        if (o->getTemplate()->getName() == "Arrow" && !o->isDestroyed())
        {
            return o;
        }
    }
    return nullptr;
}

const FXEvent *findFireFX(const FXEventLog &log)
{
    for (const FXEvent &e : log.frameEvents())
    {
        if (e.kind == FXEvent::WEAPON_FIRE_FX)
        {
            return &e;
        }
    }
    return nullptr;
}
} // namespace

// RW 0x6CCAC7 (the fire FX block's aim query, flag 1): the WEAPON_FIRE_FX event carries the
// contact point as its secondary position, and the seed is drawn at Weapon.cpp:1749. Without the
// Local 41 aim the event would carry the S-362 geometry centre (z = 50) and no seed would be drawn.
TEST_CASE("PORT-436 fire FX aim: a shot at a victim aims at the contact point and draws the seed (RW 0x6CCAC7 -> 0x6CB85A flag 1)")
{
    CombatWorld w((std::string(kPort436Shooter) + kPort436Target).c_str());
    REQUIRE(w.w.load(kPort436FireWeapon, INI_LOAD_OVERWRITE, "weapon_port436.ini").empty());
    Object *shooter = w.unit("Port436Shooter", 'A', 0.0f, 0.0f);
    Object *victim = w.unit("Port436Target", 'B', 100.0f, 0.0f);
    REQUIRE(shooter != nullptr);
    REQUIRE(victim != nullptr);
    w.frames(2);
    w.logic->random().enableCallLog(true);
    ObjectWeapons::createAndFireTempWeaponAt(TheWeaponStore->findWeaponTemplate("Port436Bow"), shooter, *victim);

    const FXEvent *fx = findFireFX(w.logic->fxEvents());
    REQUIRE(fx != nullptr);
    const Coord3D &vp = *victim->getPosition();
    // tolerance: the preference-3 contact point of a BOX without contact points = (0, 0, 67),
    // exact float from RW 0xAD25B0 (height * 0.67f); S-362's centre would give 50
    CHECK(fx->secondaryPosition.x == doctest::Approx(vp.x)); // tolerance: the contact point x in world space
    CHECK(fx->secondaryPosition.y == doctest::Approx(vp.y)); // tolerance: the contact point y in world space
    CHECK(fx->secondaryPosition.z == doctest::Approx(vp.z + 67.0f)); // tolerance: height * 0.67 = 67, exact float from RW 0xAD25B0

    // the seed draws of the contact-point mechanism: the fire FX aim (1749), the Bezier launch aim
    // (1749) and the two update steps the launch takes at once (RW 0x85EF34 .. 0x85EF5E, each
    // re-aiming through the homing step RW 0x85F42B, 1749); without the Local 41 aim none is drawn
    CHECK(drawsAt(w.logic->random().callLog(), 1749) == 4);
}

// RW 0x85EC51 (the Bezier projectile's launch): the projectile's flight end is the contact point.
// Without the Local 41 aim it would be the S-362 geometry centre (z = 50).
TEST_CASE("PORT-436 Bezier launch aim: a projectile fired at a victim launches at the contact point (RW 0x85EC51 -> 0x6CB85A flag 1)")
{
    CombatWorld w((std::string(kPort436Shooter) + kPort436Target).c_str());
    Object *shooter = w.unit("Port436Shooter", 'A', 0.0f, 0.0f);
    Object *victim = w.unit("Port436Target", 'B', 100.0f, 0.0f);
    REQUIRE(shooter != nullptr);
    REQUIRE(victim != nullptr);
    w.frames(2);
    ObjectWeapons::createAndFireTempWeaponAt(TheWeaponStore->findWeaponTemplate("Arc20"), shooter, *victim);

    Object *arrow = findArrow(w.logic.get());
    REQUIRE(arrow != nullptr);
    const BezierProjectileBehavior *b = bezierOf(arrow);
    REQUIRE(b != nullptr);
    const Coord3D &vp = *victim->getPosition();
    // tolerance: the contact point (0, 0, height * 0.67) = (0, 0, 67) in world space
    CHECK(b->flightEnd().x == doctest::Approx(vp.x)); // tolerance: the contact point x in world space
    CHECK(b->flightEnd().y == doctest::Approx(vp.y)); // tolerance: the contact point y in world space
    CHECK(b->flightEnd().z == doctest::Approx(vp.z + 67.0f)); // tolerance: height * 0.67 = 67; S-362's centre would give 50
}

// RW 0x6CC3EA (the scatter aim on a STRUCTURE victim, flag 1): a missed shot at a wall aims at the
// contact point (the second 1749 draw) and the projectile keeps that point's z. Without the Local
// 41 aim there is no scatter aim draw and the z would be the S-362 centre (30).
TEST_CASE("PORT-436 scatter aim: a missed shot at a structure victim aims at the contact point (RW 0x6CC3EA -> 0x6CB85A flag 1)")
{
    CombatWorld w((std::string(kPort436Shooter) + kPort436Wall).c_str());
    Object *shooter = w.unit("Port436Shooter", 'A', 0.0f, 0.0f);
    Object *wall = w.unit("Port436Wall", 'B', 100.0f, 0.0f);
    REQUIRE(shooter != nullptr);
    REQUIRE(wall != nullptr);
    w.frames(2);
    w.logic->random().enableCallLog(true);
    ObjectWeapons::createAndFireTempWeaponAt(TheWeaponStore->findWeaponTemplate("ScatterBow"), shooter, *wall);

    // the fire FX aim (1749), the hit roll (1489, HitPercentage 0%), the structure's scatter aim
    // (1749), the angle around the source direction (1639); no radius draw (ScatterRadius 20 is
    // below the wall's bounding circle sqrt(5^2 + 30^2) + 5 = 35.4)
    CHECK(weaponDraws(w.logic->random().callLog()) == std::vector<int>{ 1749, 1489, 1749, 1639 });

    Object *arrow = findArrow(w.logic.get());
    REQUIRE(arrow != nullptr);
    const BezierProjectileBehavior *b = bezierOf(arrow);
    REQUIRE(b != nullptr);
    // tolerance: height * 0.67f = 60 * 0.67f = 40.2f, exact float from RW 0xBDD984; the wall has
    // no AI, so the scatter keeps the aim point's z; S-362's centre would give 30
    CHECK(b->flightEnd().z == doctest::Approx(40.2f)); // tolerance: height * 0.67 = 60 * 0.67 = 40.2, exact float from RW 0xBDD984
}

// RW 0x6CB8F8 (the LOS aim, flag 0, the wrapper callers 0x6CC6B6 / 0x6CC723 of RW 0x6CC37B): the
// line-of-sight point is the victim position plus the geometry's centre height, and nothing is
// drawn. This is the neighboring path the contact-point aim must not move.
TEST_CASE("PORT-436 LOS aim: the line-of-sight point is the victim position plus the geometry height, no draw (RW 0x6CB8F8, flag 0)")
{
    CombatWorld w((std::string(kPort436Shooter) + kPort436Target).c_str());
    Object *shooter = w.unit("Port436Shooter", 'A', 0.0f, 0.0f);
    Object *victim = w.unit("Port436Target", 'B', 100.0f, 0.0f);
    REQUIRE(shooter != nullptr);
    REQUIRE(victim != nullptr);
    w.frames(2);
    w.logic->random().enableCallLog(true);
    Weapon weapon(TheWeaponStore->findWeaponTemplate("Arc20"), PRIMARY_WEAPON, w.logic->getFrame());

    const Coord3D point = CombatLineOfSight::victimLineOfSightPoint(weapon, *shooter, *victim);
    const Coord3D &vp = *victim->getPosition();
    // tolerance: the victim position with z + the geometry centre height (height * 0.5 = 50,
    // RW 0xB4E370); the contact point (67) is the flag 1 path, not this one
    CHECK(point.x == doctest::Approx(vp.x)); // tolerance: the victim position x
    CHECK(point.y == doctest::Approx(vp.y)); // tolerance: the victim position y
    CHECK(point.z == doctest::Approx(vp.z + 50.0f)); // tolerance: height * 0.5 = 50, the geometry centre height RW 0xB4E370

    // the flag 0 path draws nothing from the logic random
    CHECK(drawsAt(w.logic->random().callLog(), 1749) == 0);
}

// RW 0x6CB81A (the first step of the aim query): a weapon with a PreferredTargetBone that the
// victim's drawable answers aims at the bone translation and draws nothing. This is the neighboring
// path the contact-point aim must not move.
TEST_CASE("PORT-436 bone aim: a weapon with PreferredTargetBone aims at the bone, no draw (RW 0x6CB81A)")
{
    CombatWorld w((std::string(kPort436Shooter) + kPort436Target).c_str());
    Object *shooter = w.unit("Port436Shooter", 'A', 0.0f, 0.0f);
    Object *victim = w.unit("Port436Target", 'B', 100.0f, 0.0f);
    REQUIRE(shooter != nullptr);
    REQUIRE(victim != nullptr);
    w.frames(2);
    static Port436BoneOffsets bones;
    w.combat().setLaunchOffsets(&bones);
    w.logic->random().enableCallLog(true);
    ObjectWeapons::createAndFireTempWeaponAt(TheWeaponStore->findWeaponTemplate("BoneBow"), shooter, *victim);

    Object *arrow = findArrow(w.logic.get());
    REQUIRE(arrow != nullptr);
    const BezierProjectileBehavior *b = bezierOf(arrow);
    REQUIRE(b != nullptr);
    const Coord3D &vp = *victim->getPosition();
    // tolerance: exact bone translation (7, 8, 9) added to the victim position
    CHECK(b->flightEnd().x == doctest::Approx(vp.x + 7.0f)); // tolerance: exact bone translation x
    CHECK(b->flightEnd().y == doctest::Approx(vp.y + 8.0f)); // tolerance: exact bone translation y
    CHECK(b->flightEnd().z == doctest::Approx(vp.z + 9.0f)); // tolerance: exact bone translation z

    // the bone answer comes with no logic draw (Weapon.cpp:1749)
    CHECK(drawsAt(w.logic->random().callLog(), 1749) == 0);
}

// RW 0x6CC399 (the scatter target): a missed shot at an INFANTRY victim scatters from the plain
// victim position, with no scatter aim draw. This is the neighboring path the structure scatter aim
// must not move: only STRUCTURE victims take the contact-point aim.
TEST_CASE("PORT-436 scatter plain: a missed shot at an infantry victim scatters from the plain position, no aim draw (RW 0x6CC399)")
{
    CombatWorld w(kPort436Shooter);
    Object *shooter = w.unit("Port436Shooter", 'A', 0.0f, 0.0f);
    Object *victim = w.unit("Dummy", 'B', 100.0f, 0.0f);
    REQUIRE(shooter != nullptr);
    REQUIRE(victim != nullptr);
    w.frames(2);
    w.logic->random().enableCallLog(true);
    ObjectWeapons::createAndFireTempWeaponAt(TheWeaponStore->findWeaponTemplate("ScatterBow"), shooter, *victim);

    // the fire FX aim (1749), the hit roll (1489), the radius (1625: the Dummy's bounding circle 8
    // + 5 = 13 is below the ScatterRadius 20), the angle (1639); NO scatter aim (1749): the victim
    // is infantry, not a structure
    CHECK(weaponDraws(w.logic->random().callLog()) == std::vector<int>{ 1749, 1489, 1625, 1639 });
}
