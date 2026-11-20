// PORT-230: DynamicPortalBehaviour::topAttackPosition follows the retail 3D scale (RW 0x863D77).
// No retail files: the portal module is instantiated from synthetic INI and given an explicit transform.

#include "doctest.h"

#include "CombatTestUtil.h"
#include "GameLogic/Module/PortalModules.h"
#include "GameLogic/Object/Object.h"

#include <string>

namespace
{
const char kTopAttackPortal[] =
	"Object Port230TopAttack\n"
	"  KindOf = STRUCTURE WALK_ON_TOP_OF_WALL\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = DynamicPortalBehaviour ModuleTag_Portal\n"
	"    TopAttackPos = X:3 Y:4 Z:5\n"
	"  End\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryMinorRadius = 10\n"
	"  GeometryHeight = 20\n"
	"End\n";

const char kFlatTopAttackPortal[] =
	"Object Port230TopAttackFlat\n"
	"  KindOf = STRUCTURE WALK_ON_TOP_OF_WALL\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Behavior = DynamicPortalBehaviour ModuleTag_Portal\n"
	"    TopAttackPos = X:3 Y:4 Z:0\n"
	"  End\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryMinorRadius = 10\n"
	"  GeometryHeight = 20\n"
	"End\n";

struct Portal230World : combattest::CombatWorld
{
	Portal230World()
	{
		DynamicPortalBehaviour::registerClass(w.modules);
		const std::string error = w.load(std::string(kTopAttackPortal) + kFlatTopAttackPortal, INI_LOAD_OVERWRITE, "port230-top-attack.ini");
		REQUIRE_MESSAGE(error.empty(), error);
	}
	Object *portal(const char *name = "Port230TopAttack")
	{
		Object *object = unit(name, 'A', 10.0f, 20.0f);
		REQUIRE(object);
		REQUIRE(DynamicPortalBehaviour::of(*object));
		return object;
	}
};
} // namespace

// RW 0x863D77 divides the transformed local X axis by max(abs(x), abs(y)), takes its 3D length (RW 0x403111),
// scales only X and Y of TopAttackPos by that length, leaves Z unchanged, then applies RW 0x70BFD1's object transform.
TEST_CASE("PORT-230 TopAttackPos: a tilted portal uses the full 3D axis length for X/Y only (RW 0x863D77)")
{
	Portal230World world;
	Object *object = world.portal();
	const Coord3D origin{ 10.0f, 20.0f, 30.0f };
	const float r = 0.7071067811865475f;
	// An orthonormal basis whose local X axis is (0.5, 0.5, sqrt(0.5)); max planar component = 0.5,
	// so the normalized axis is (1, 1, sqrt(2)) and its 3D length is 2.
	const float basis[9] = { 0.5f, -r, -0.5f, 0.5f, r, -0.5f, r, 0.0f, r };
	object->teleportTransform(&origin, basis);

	const Coord3D result = DynamicPortalBehaviour::of(*object)->topAttackPosition();
	CHECK(result.x == doctest::Approx(4.8431458f)); // tolerance: float32 rounding through RW’s sequential transform multiply/adds.
	CHECK(result.y == doctest::Approx(26.156854f)); // tolerance: float32 rounding through RW’s sequential transform multiply/adds.
	CHECK(result.z == doctest::Approx(37.778175f)); // tolerance: float32 rounding through RW’s sequential transform multiply/adds.
}

// Neighboring upright/yaw-only behavior: with TopAttackPos.Z zero, the planar result is unchanged.
TEST_CASE("PORT-230 TopAttackPos: upright yaw keeps the expected planar attack point (RW 0x863D77)")
{
	Portal230World world;
	Object *object = world.portal("Port230TopAttackFlat");
	const Coord3D origin{ 10.0f, 20.0f, 30.0f };
	const float c = 0.7071067811865475f;
	const float basis[9] = { c, -c, 0.0f, c, c, 0.0f, 0.0f, 0.0f, 1.0f };
	object->teleportTransform(&origin, basis);

	// With TopAttackPos.Z = 0, both the retail 3D norm and the old planar projection keep world Z at the origin.
	const Coord3D result = DynamicPortalBehaviour::of(*object)->topAttackPosition();
	CHECK(result.x == doctest::Approx(9.0f)); // tolerance: float32 rounding through RW’s sequential transform multiply/adds.
	CHECK(result.y == doctest::Approx(27.0f)); // tolerance: float32 rounding through RW’s sequential transform multiply/adds.
	CHECK(result.z == doctest::Approx(30.0f)); // tolerance: float32 rounding through RW’s sequential transform multiply/adds.
}
