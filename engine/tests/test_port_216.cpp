// PORT-216: retain RotWK's stored weapon contact point, including compound/offset geometry.
// Synthetic INI only; these are not copies of retail or mod templates.
// RW 0x4504B3..0x4504BD initializes +0x50 to zero; RW 0xAD2BA7 / 0xAD3DEF refresh it through
// 0xAD25B0 on HEIGHT/CONTACT rows, not an offset (0xAD1770), shape type (0xAD4040), active flag
// (0xAD3520) or runtime height (0xAD2C60). RW 0xAD3172 reads it in mode 3, with live-height
// clamps/probe fallback. RW 0x463E45..0x463E62 copies contacts with the shapes.
// The actual firing consumer is RW 0x85EC51 and homing consumer RW 0x85F46A, both through
// RW 0x6CB85A flag 1; neither has a MONSTER/STRUCTURE size gate. No nearest-surface rule is invented.
// The shared implementation currently uses RotWK rules for all editions; BFME2 retail parity
// is not claimed from these synthetic tests. DONOR BFME2 RVA 0x2CAB3A (WeaponTemplateTargetPositions.cpp).

#include "doctest.h"
#include "CombatTestUtil.h"
#include "GameLogic/Combat/ObjectWeapons.h"
#include "GameLogic/FXEvents.h"
#include "GameLogic/Module/ProjectileModules.h"
#include "GameLogic/Object/ObjectGeometry.h"

#include <cstdint>
#include <string>
#include <vector>

namespace
{
using combattest::CombatWorld;

const char *kLateOffset =
    "  Geometry = BOX\n"
    "  GeometryName = Body\n"
    "  GeometryMajorRadius = 70\n"
    "  GeometryMinorRadius = 70\n"
    "  GeometryHeight = 40\n"
    "  GeometryOffset = X:0 Y:0 Z:12\n";

const char *kEarlyOffset =
    "  Geometry = BOX\n"
    "  GeometryName = Body\n"
    "  GeometryMajorRadius = 70\n"
    "  GeometryMinorRadius = 70\n"
    "  GeometryOffset = X:0 Y:0 Z:12\n"
    "  GeometryHeight = 40\n";

// A synthetic mumak-like body and elevated second shape. The last height row sees 56,
// so RW 0xAD25B0 stores 37.52, inside the body. Recomputing from the final height (100)
// would incorrectly move the aim to 67, inside the elevated shape. Both pass the probe:
// the discriminator is the stored point, not a collision/KindOf workaround.
const char *kCompound =
    "  Geometry = BOX\n"
    "  GeometryMajorRadius = 64\n"
    "  GeometryMinorRadius = 32\n"
    "  GeometryHeight = 48\n"
    "  GeometryOffset = X:8 Y:0 Z:0\n"
    "  AdditionalGeometry = CYLINDER\n"
    "  GeometryMajorRadius = 22\n"
    "  GeometryHeight = 56\n"
    "  GeometryOffset = X:6 Y:0 Z:44\n";

std::string target(const char *name, const std::string &geometry, const char *kind = "MONSTER SELECTABLE")
{
    return std::string("Object ") + name + "\n  KindOf = " + kind + "\n" + geometry +
        "  ArmorSet\n"
        "    Conditions = None\n"
        "    Armor = PlainArmor\n"
        "  End\n"
        "  Body = ActiveBody ModuleTag_Body\n"
        "    MaxHealth = 1000\n"
        "  End\n"
        "End\n";
}

Coord3D contact(const Object &object, bool ignoreCollision = false)
{
    Coord3D point{};
    REQUIRE(ObjectGeometry::worldspaceBestContactPoint(object, Coord3D{ 25.0f, 75.0f, 0.0f }, "", 3, 0, ignoreCollision, point));
    return point;
}

void checkLocal(const Object &object, const Coord3D &point, float x, float y, float z)
{
    const Coord3D &position = *object.getPosition();
    // tolerance: decimal expectations below are independently evaluated from RW 0.67f/0.1f;
    // the world transform and float32 stores can round their final low bits.
    CHECK(point.x == position.x + x);
    CHECK(point.y == position.y + y);
    CHECK(point.z == doctest::Approx(position.z + z)); // tolerance: RW float32 .67/.1 products and the world transform round the decimal expectations.
}

const BezierProjectileBehavior *bezierOf(const Object &arrow)
{
    for (const auto &module : arrow.modules())
    {
        if (const auto *bezier = dynamic_cast<const BezierProjectileBehavior *>(module.get()))
        {
            return bezier;
        }
    }
    return nullptr;
}

Object *findArrow(GameLogic &logic)
{
    for (Object *object = logic.getFirstObject(); object; object = object->getNextObject())
    {
        if (!object->isDestroyed() && object->getTemplate()->getName() == "Port216Arrow")
        {
            return object;
        }
    }
    return nullptr;
}

const char *kArrow =
    "ChildObject Port216Arrow Arrow\n"
    "  KindOf = PROJECTILE NO_COLLIDE\n"
    "End\n";

const char *kBow =
    "Weapon Port216Bow\n"
    "  AttackRange = 400\n"
    "  WeaponSpeed = 100\n"
    "  FireFX = FX_Port216Fire\n"
    "  ProjectileNugget\n"
    "    ProjectileTemplateName = Port216Arrow\n"
    "    WarheadTemplateName = BowWarhead\n"
    "  End\n"
    "End\n";
} // namespace

TEST_CASE("PORT-216 INI order preserves the stored point with identical final shapes and no KindOf gate")
{
    const char *kind = "MONSTER SELECTABLE";
    SUBCASE("mumak-like unit") { kind = "MONSTER SELECTABLE"; }
    SUBCASE("ordinary infantry") { kind = "INFANTRY SELECTABLE"; }
    SUBCASE("structure neighbour") { kind = "STRUCTURE IMMOBILE"; }

    CombatWorld w((target("Port216Late", kLateOffset, kind) + target("Port216Early", kEarlyOffset, kind)).c_str());
    Object *late = w.unit("Port216Late", 'B', 200.0f, 200.0f);
    Object *early = w.unit("Port216Early", 'B', 400.0f, 200.0f);
    REQUIRE(late != nullptr);
    REQUIRE(early != nullptr);
    REQUIRE(late->geometryShapes().size() == 1);
    REQUIRE(early->geometryShapes().size() == 1);
    CHECK(late->geometryShapes()[0].height == early->geometryShapes()[0].height);
    CHECK(late->geometryShapes()[0].offsetZ == early->geometryShapes()[0].offsetZ);
    // RW 0xAD2BA7 / 0xAD1770: 40 * .67 before the offset, versus (40 + 12) * .67 after it.
    checkLocal(*late, contact(*late), 0.0f, 0.0f, 26.8f);
    checkLocal(*early, contact(*early), 0.0f, 0.0f, 34.84f);
}

TEST_CASE("PORT-216 actual NO_COLLIDE arrow launch FX and homing use the compound body point; infantry stays unchanged")
{
    const char *geometry = kCompound;
    const char *kind = "MONSTER SELECTABLE";
    float expectedZ = 37.52f;
    SUBCASE("offset compound mumak-like target") {}
    SUBCASE("ordinary infantry neighbour")
    {
        geometry = "  Geometry = CYLINDER\n  GeometryMajorRadius = 4\n  GeometryHeight = 24\n";
        kind = "INFANTRY SELECTABLE";
        expectedZ = 16.08f; // RW 0xAD2BA7: no later offset, the existing 24 * .67 point is unchanged.
    }
    const std::string ini = target("Port216Victim", geometry, kind) +
        target("Port216Shooter", "  Geometry = CYLINDER\n  GeometryMajorRadius = 4\n  GeometryHeight = 20\n", "INFANTRY CAN_ATTACK") + kArrow;
    CombatWorld w(ini.c_str());
    REQUIRE(w.w.load(kBow, INI_LOAD_OVERWRITE, "port216_weapon.ini").empty());
    Object *shooter = w.unit("Port216Shooter", 'A', 240.0f, 0.0f);
    Object *victim = w.unit("Port216Victim", 'B', 240.0f, 240.0f);
    REQUIRE(shooter != nullptr);
    REQUIRE(victim != nullptr);
    w.frames(2);
    w.logic->random().enableCallLog(true);
    ObjectWeapons::createAndFireTempWeaponAt(TheWeaponStore->findWeaponTemplate("Port216Bow"), shooter, *victim);

    Object *arrow = findArrow(*w.logic);
    REQUIRE(arrow != nullptr);
    const ObjectID arrowID = arrow->getID();
    const BezierProjectileBehavior *bezier = bezierOf(*arrow);
    REQUIRE(bezier != nullptr);
    checkLocal(*victim, bezier->flightEnd(), 0.0f, 0.0f, expectedZ); // RW 0x85EC51, not a structure-only path
    const FXEvent *fireFX = nullptr;
    for (const FXEvent &event : w.logic->fxEvents().frameEvents())
    {
        if (event.kind == FXEvent::WEAPON_FIRE_FX)
        {
            fireFX = &event;
        }
    }
    REQUIRE(fireFX != nullptr);
    checkLocal(*victim, fireFX->secondaryPosition, 0.0f, 0.0f, expectedZ); // RW 0x6CCAC7
    int aimDraws = 0;
    for (const auto &call : w.logic->random().callLog())
    {
        if (call.file == "Weapon.cpp" && call.line == 1749)
        {
            ++aimDraws;
        }
    }
    CHECK(aimDraws == 4); // existing PORT-436 sequence: FX, launch, two immediate updates; no extra draw in the cache
    w.frames(1);
    arrow = w.byId(arrowID);
    REQUIRE(arrow != nullptr);
    bezier = bezierOf(*arrow);
    REQUIRE(bezier != nullptr);
    checkLocal(*victim, bezier->flightEnd(), 0.0f, 0.0f, expectedZ); // RW 0x85F46A, same stored point on homing
}

TEST_CASE("PORT-216 each contact-row spelling refreshes the point even when one row cannot supply a segment")
{
    const char *row = "GeometryContactPoint";
    SUBCASE("geometry row") { row = "GeometryContactPoint"; }
    SUBCASE("attack row") { row = "AttackContactPoint"; }
    SUBCASE("swoop row") { row = "SwoopContactPoint"; }
    const std::string geometry = std::string(kLateOffset) + "  " + row + " = X:4 Y:0 Z:18\n";
    CombatWorld w(target("Port216OneRow", geometry).c_str());
    Object *object = w.unit("Port216OneRow", 'B', 200.0f, 200.0f);
    REQUIRE(object != nullptr);
    // RW 0xAD3DEF refreshes after the offset. RW 0xAD31AF retains that point with fewer than two rows.
    checkLocal(*object, contact(*object), 0.0f, 0.0f, 34.84f);
}

TEST_CASE("PORT-216 inherited geometry preserves event history and a later height row refreshes it")
{
    const std::string ini = target("Port216Parent", kLateOffset) +
        "ChildObject Port216Inherited Port216Parent\n  GeometryOffset = X:0 Y:0 Z:24\nEnd\n"
        "ChildObject Port216Refreshed Port216Inherited\n  GeometryHeight = 40\nEnd\n";
    CombatWorld w(ini.c_str());
    Object *parent = w.unit("Port216Parent", 'B', 150.0f, 200.0f);
    Object *inherited = w.unit("Port216Inherited", 'B', 350.0f, 200.0f);
    Object *refreshed = w.unit("Port216Refreshed", 'B', 550.0f, 200.0f);
    REQUIRE(parent != nullptr);
    REQUIRE(inherited != nullptr);
    REQUIRE(refreshed != nullptr);
    // RW 0x463E5A copies +0x50; offset alone leaves it, another height row invokes RW 0xAD25B0.
    checkLocal(*parent, contact(*parent), 0.0f, 0.0f, 26.8f);
    checkLocal(*inherited, contact(*inherited), 0.0f, 0.0f, 26.8f);
    checkLocal(*refreshed, contact(*refreshed), 0.0f, 0.0f, 42.88f);
}

TEST_CASE("PORT-216 runtime height and active changes keep the stored point but still apply live clamps")
{
    CombatWorld w(target("Port216Mutable", kLateOffset).c_str());
    Object *object = w.unit("Port216Mutable", 'B', 200.0f, 200.0f);
    REQUIRE(object != nullptr);
    object->setGeometryMaxHeight(80.0f); // RW 0xAD2C96 only refreshes bounds, not the point.
    checkLocal(*object, contact(*object), 0.0f, 0.0f, 26.8f);
    object->setGeometryActive("Body", false); // RW 0xAD35A5: no point refresh.
    checkLocal(*object, contact(*object), 0.0f, 0.0f, 0.0f); // RW 0xAD3283 clamps to current height zero.
    object->setGeometryActive("Body", true);
    checkLocal(*object, contact(*object), 0.0f, 0.0f, 26.8f);
    object->setGeometryMaxHeight(8.0f);
    checkLocal(*object, contact(*object), 0.0f, 0.0f, 20.0f); // RW 0xAD3283: live height 8 + offset 12.
}

TEST_CASE("PORT-216 shape type and radius rows do not refresh the constructor or previous stored point")
{
    const std::string ini = target("Port216NoHeight", "  Geometry = SPHERE\n  GeometryMajorRadius = 30\n") +
        target("Port216ChangedType", std::string(kLateOffset) + "  Geometry = SPHERE\n  GeometryMajorRadius = 80\n");
    CombatWorld w(ini.c_str());
    Object *initial = w.unit("Port216NoHeight", 'B', 150.0f, 200.0f);
    Object *changed = w.unit("Port216ChangedType", 'B', 400.0f, 200.0f);
    REQUIRE(initial != nullptr);
    REQUIRE(changed != nullptr);
    // RW 0x4504B3 zero, then RW 0xAD330F floor at 30 * .1. Type parsing RW 0xAD4040 only refreshes bounds.
    checkLocal(*initial, contact(*initial), 0.0f, 0.0f, 3.0f);
    checkLocal(*changed, contact(*changed), 0.0f, 0.0f, 26.8f);
}

TEST_CASE("PORT-216 collision fallback remains active when a late offset leaves the stored point outside")
{
    CombatWorld w(target("Port216Outside", std::string(kLateOffset) + "  GeometryOffset = X:0 Y:0 Z:100\n").c_str());
    Object *object = w.unit("Port216Outside", 'B', 200.0f, 200.0f);
    REQUIRE(object != nullptr);
    // RW 0xAD329E..0xAD32EB: stored 26.8 is outside, so use the first active shape centre 100 + 40/2.
    checkLocal(*object, contact(*object), 0.0f, 0.0f, 120.0f);
    checkLocal(*object, contact(*object, true), 0.0f, 0.0f, 26.8f); // the existing ignoreCollision branch
}

TEST_CASE("PORT-216 geometry copies carry both rows and stored point through a byte-identical snapshot")
{
    const std::string rows = std::string(kLateOffset) +
        "  GeometryContactPoint = X:-12 Y:8 Z:30 Ram\n"
        "  GeometryContactPoint = X:14 Y:6 Z:38 Menu\n";
    CombatWorld w((target("Port216Plain", kLateOffset) + target("Port216Rows", rows)).c_str());
    Object *plain = w.unit("Port216Plain", 'B', 100.0f, 200.0f);
    Object *withRows = w.unit("Port216Rows", 'B', 300.0f, 200.0f);
    Object *copyPlain = w.unit("Port216Rows", 'B', 500.0f, 200.0f);
    Object *copyRows = w.unit("Port216Plain", 'B', 700.0f, 200.0f);
    REQUIRE(plain != nullptr);
    REQUIRE(withRows != nullptr);
    REQUIRE(copyPlain != nullptr);
    REQUIRE(copyRows != nullptr);
    plain->setGeometryMaxHeight(80.0f);
    copyPlain->setGeometryInfoFrom(*plain);
    copyRows->setGeometryInfoFrom(*withRows);
    // RW 0x463E45..0x463E62: rows AND point belong to the copied GeometryInfo, not the recipient's original template.
    const auto checkCopiedContacts = [](const Object &noRows, const Object &rowsObject) {
        checkLocal(noRows, contact(noRows), 0.0f, 0.0f, 26.8f);
        checkLocal(rowsObject, contact(rowsObject), 14.0f, 6.0f, 38.0f); // RW 0xAD31BD: seed 0 chooses the second row.
        Coord3D point{};
        const Coord3D caller{ 25.0f, 75.0f, 0.0f };
        // RW 0xAD337C..0xAD3517 (mode 0): labelled contacts must follow the same geometry copy.
        REQUIRE_FALSE(ObjectGeometry::worldspaceBestContactPoint(noRows, caller, "Ram", 0, 0, false, point));
        REQUIRE(ObjectGeometry::worldspaceBestContactPoint(rowsObject, caller, "Ram", 0, 0, false, point));
        checkLocal(rowsObject, point, -12.0f, 8.0f, 30.0f);
        // RW 0xAD3340 (mode 1): this row is above the initializer's point, so it is unambiguous in both paths.
        // This does NOT change the independent no-row/preferred-initializer behavior retained in S-7387.
        REQUIRE(ObjectGeometry::worldspaceBestContactPoint(rowsObject, caller, "", 1, 0, false, point));
        checkLocal(rowsObject, point, 14.0f, 6.0f, 38.0f);
    };
    checkCopiedContacts(*copyPlain, *copyRows);
    const ObjectID plainID = copyPlain->getID();
    const ObjectID rowsID = copyRows->getID();
    const std::uint32_t hash = w.logic->computeStateHash();
    std::vector<std::uint8_t> saved;
    std::string error;
    REQUIRE_MESSAGE(w.logic->saveSnapshot(saved, &error), error);
    copyPlain->setGeometryInfoFrom(*withRows);
    copyRows->setGeometryMaxHeight(5.0f);
    REQUIRE_MESSAGE(w.logic->loadSnapshot(saved, &error), error);
    CHECK(w.logic->computeStateHash() == hash);
    copyPlain = w.byId(plainID);
    copyRows = w.byId(rowsID);
    REQUIRE(copyPlain != nullptr);
    REQUIRE(copyRows != nullptr);
    CHECK(copyPlain->hasGeometryHeights());
    checkCopiedContacts(*copyPlain, *copyRows);
    std::vector<std::uint8_t> resaved;
    REQUIRE_MESSAGE(w.logic->saveSnapshot(resaved, &error), error);
    CHECK(resaved == saved); // cache is derived; the existing geometry source/heights xfer is sufficient.
}
