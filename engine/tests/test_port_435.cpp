// OpenBFME unit tests, lane PORT-435 (fork issue #435): why a Mordor catapult ordered at Helm's Deep's
// main gate throws its stones into the gatehouse instead of the gate.
//
// The weapon aim of a projectile is the victim's contact point: WeaponTemplate::adjustTargetPositionOnVictim
// RW 0x6CB85A (flag 1 - the launch aim of BezierProjectileBehavior::projectileFireAtObjectOrPosition
// RW 0x85EC51 pushes 1) -> Object::getWorldspaceBestContactPoint RW 0x690BD2 (the object's own GeometryInfo
// at +0xA8) -> GeometryInfo::getBestContactPoint RW 0xAD30E0 preference 3: the seed blends two neighbouring
// GeometryContactPoint rows, and a blend that lies outside every ACTIVE shape falls back to the first ACTIVE
// shape's centre (RW 0xAD32BA .. RW 0xAD32EB: the shape list +0x2C .. +0x30, the active byte +0x20, the
// centre (offsetX, offsetY, offsetZ + height * 0.5)).
//
// A gate's GeometryInfo is not one set of shapes: GateOpenAndCloseBehavior switches the active shapes
// (GeometryInfo::setActive RW 0xAD3520) between its own sets, GeometryForOpen / GeometryForClosed
// (RW 0xC07D08 field table, +0x2C / +0x38; the constructor's defaults {"OpenLeft", "OpenRight"} / {"Closed"}).
// Helm's Deep's main gate stands OPEN at creation (GateOpenAndCloseBehavior OpenByDefault) and stays open in
// the siege tests' world, where the gate has no skirmish AI to run AIGateUpdate (RW 0x8B4D3A .. RW 0x8B4D55:
// the module sleeps forever without one, so nothing closes it).
//
// What the tests below hold: with the OPEN geometry active, the aim point of a contact-point query at such a
// gate is never the middle of the doorway - a row pair that blends into a stood-aside panel answers that
// point, every other pair falls back to the first stood-aside panel's centre, so the stone is aimed at the
// arch's side, where the gatehouse stands. With the CLOSED geometry active the same query answers the closed
// panel's centre, the middle of the doorway. The S-362 aim these pins replace (the victim's geometry centre,
// z = position.z + height * 0.5) answered the middle of the doorway in both states, which is what the siege1
// pins measured before Local 41 (fork issue 330).
//
// The seeds below are chosen to make the arithmetic exact: `seed % (count - 1)` picks the row pair and
// `((seed >> 8) & 0xFF) / 255` is 0 for seeds 0 .. 7, so the blend is contacts[idx + 1] itself.
//
// Synthetic INI only: no retail or mod data (the shape and row values are this test's own, gate-shaped, not
// Helm's Deep's). GPL-3.0.

#include "doctest.h"

#include "CombatTestUtil.h"

#include "GameLogic/Object/Object.h"
#include "GameLogic/Object/ObjectGeometry.h"

#include <string>

using namespace combattest;

namespace
{
// a gate-shaped object: a thin closed panel across a doorway named Closed, two thicker panels stood aside
// (OpenLeft / OpenRight, what the gate module makes active while it stands open), and nine contact point rows
// spread over the doorway's width - the outer ones inside the stood-aside panels, the middle ones inside no
// shape at all (so they exercise the fallback of RW 0xAD32BA).
const char *kPort435Gate =
	"Object Port435Gate\n"
	"  KindOf = STRUCTURE IMMOBILE SELECTABLE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 1.0\n"
	"  GeometryMinorRadius = 60.0\n"
	"  GeometryHeight = 60.0\n"
	"  GeometryName = Closed\n"
	"  GeometryOffset = X:-5.0 Y:0.0 Z:0.0\n"
	"  GeometryIsSmall = No\n"
	"  AdditionalGeometry = BOX\n"
	"  GeometryMajorRadius = 18.0\n"
	"  GeometryMinorRadius = 10.0\n"
	"  GeometryHeight = 56.0\n"
	"  GeometryOffset = X:10.0 Y:45.0 Z:0.0\n"
	"  GeometryName = OpenLeft\n"
	"  AdditionalGeometry = BOX\n"
	"  GeometryMajorRadius = 18.0\n"
	"  GeometryMinorRadius = 10.0\n"
	"  GeometryHeight = 56.0\n"
	"  GeometryOffset = X:10.0 Y:-45.0 Z:0.0\n"
	"  GeometryName = OpenRight\n"
	"  GeometryContactPoint = X:20 Y:-36 Z:10 Ram\n"
	"  GeometryContactPoint = X:10 Y:-45 Z:56\n"
	"  GeometryContactPoint = X:0 Y:-50 Z:10\n"
	"  GeometryContactPoint = X:0 Y:-20 Z:56\n"
	"  GeometryContactPoint = X:0 Y:0 Z:10\n"
	"  GeometryContactPoint = X:0 Y:20 Z:56\n"
	"  GeometryContactPoint = X:0 Y:50 Z:10\n"
	"  GeometryContactPoint = X:10 Y:45 Z:56\n"
	"  GeometryContactPoint = X:20 Y:36 Z:10 Ram\n"
	"  ArmorSet\n"
	"    Conditions = None\n"
	"    Armor = PlainArmor\n"
	"  End\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 4000\n"
	"  End\n"
	"  Behavior = DestroyDie ModuleTag_Destroy\n"
	"  End\n"
	"End\n";

// the gate's active shapes as GateOpenAndCloseBehavior leaves them (GeometryForOpen / GeometryForClosed,
// GeometryInfo::setActive RW 0xAD3520)
void openGeometry(Object &gate)
{
	gate.setGeometryActive("OpenLeft", true);
	gate.setGeometryActive("OpenRight", true);
	gate.setGeometryActive("Closed", false);
}

void closedGeometry(Object &gate)
{
	gate.setGeometryActive("OpenLeft", false);
	gate.setGeometryActive("OpenRight", false);
	gate.setGeometryActive("Closed", true);
}

// the weapon aim of RW 0x6CB85A flag 1: a seed (GameLogicRandomValue(0, 12345678), Weapon.cpp:1749) into
// Object::getWorldspaceBestContactPoint RW 0x690BD2 with preference 3, no label, collisions considered
Coord3D aim(Object &victim, int seed)
{
	Coord3D out{ -1.0f, -1.0f, -1.0f };
	const bool answered = ObjectGeometry::worldspaceBestContactPoint(victim, *victim.getPosition(), "", 3, seed, false, out);
	REQUIRE_MESSAGE(answered, "preference 3 always answers (RW 0xAD30E0)");
	return out;
}

void checkPoint(const Coord3D &p, float x, float y, float z, const char *what)
{
	CAPTURE(what);
	// exact values: the blend is contacts[idx + 1] itself (blend 0) and the fallback is a shape centre of the
	// synthetic rows, so no bound is needed
	CHECK(p.x == x);
	CHECK(p.y == y);
	CHECK(p.z == z);
}
} // namespace

// The case the siege run meets: the gate stands open, so the closed panel is inactive and the two stood-aside
// panels are the only geometry. RW 0xAD30E0 preference 3 then never answers a point in the middle of the
// doorway: a blend that overlaps a panel answers the blend point on that panel (RW 0xAD32A0's probe passes),
// and every blend outside both panels falls back to the first active shape's centre, the OpenLeft panel's
// (offsetX, offsetY, offsetZ + height * 0.5) = (10, 45, 0 + 56 * 0.5).
TEST_CASE("PORT-435 aim: a gate standing open aims every stone at a stood-aside panel, never at the middle of the doorway")
{
	CombatWorld w(kPort435Gate);
	Object *gate = w.unit("Port435Gate", 'B', 0.0f, 0.0f);
	REQUIRE(gate != nullptr);
	w.frames(2);
	openGeometry(*gate);
	REQUIRE(gate->geometryShapes().size() == 3);
	CHECK(gate->geometryShapes()[0].active == false); // Closed
	CHECK(gate->geometryShapes()[1].active == true);  // OpenLeft
	CHECK(gate->geometryShapes()[2].active == true);  // OpenRight

	// idx = seed % 8, blend = 0 for seeds 0 .. 7 (the tests' own rows, above)
	checkPoint(aim(*gate, 0), 10.0f, -45.0f, 56.0f, "row 0 -> row 1, inside OpenRight"); // the blend point itself
	checkPoint(aim(*gate, 1), 0.0f, -50.0f, 10.0f, "row 1 -> row 2, inside OpenRight");
	// rows 2 -> 3, 3 -> 4 and 4 -> 5 blend into no active shape: the first active shape's centre
	checkPoint(aim(*gate, 2), 10.0f, 45.0f, 28.0f, "row 2 -> row 3, the OpenLeft centre");
	checkPoint(aim(*gate, 3), 10.0f, 45.0f, 28.0f, "row 3 -> row 4, the OpenLeft centre");
	checkPoint(aim(*gate, 4), 10.0f, 45.0f, 28.0f, "row 4 -> row 5, the OpenLeft centre");
	checkPoint(aim(*gate, 5), 0.0f, 50.0f, 10.0f, "row 5 -> row 6, inside OpenLeft");
	checkPoint(aim(*gate, 6), 10.0f, 45.0f, 56.0f, "row 6 -> row 7, inside OpenLeft");
	checkPoint(aim(*gate, 7), 20.0f, 36.0f, 10.0f, "row 7 -> row 8, inside OpenLeft");

	// the rule itself: no aim of the open gate lies in the doorway's middle (|y| >= 35 is the panels' band);
	// the S-362 geometry centre answered y = 0 there, which sent the stone through the doorway's centre
	for (int seed = 0; seed < 8; ++seed)
	{
		const Coord3D p = aim(*gate, seed);
		const bool onAPanel = p.x == 10.0f && p.y == 45.0f && p.z == 28.0f; // the fallback centre
		CHECK((onAPanel || p.y >= 35.0f || p.y <= -35.0f));               // tolerance: the panels' synthetic band is the test's own data
	}
}

// The neighbouring behaviour: with the closed panel the only active geometry, every contact point row lies
// outside that thin panel (the rows sit on the doorway's face, x >= 0, the closed panel spans x -6 .. -4), so
// every seed answers the first active shape's centre - the closed panel's (offsetX, offsetY, offsetZ +
// height * 0.5) = (-5, 0, 30), the middle of the doorway. This is the aim a closed gate has, and the aim the
// S-362 geometry centre gave in both states.
TEST_CASE("PORT-435 aim: a gate standing closed aims at the closed panel's centre, the middle of the doorway")
{
	CombatWorld w(kPort435Gate);
	Object *gate = w.unit("Port435Gate", 'B', 0.0f, 0.0f);
	REQUIRE(gate != nullptr);
	w.frames(2);
	closedGeometry(*gate);
	REQUIRE(gate->geometryShapes().size() == 3);
	CHECK(gate->geometryShapes()[0].active == true);
	CHECK(gate->geometryShapes()[1].active == false);
	CHECK(gate->geometryShapes()[2].active == false);

	for (int seed = 0; seed < 8; ++seed)
	{
		checkPoint(aim(*gate, seed), -5.0f, 0.0f, 30.0f, "the closed panel's centre");
	}
	// the clamps (RW 0xAD1920 over the ACTIVE shapes) read the closed panel's 60, not the stood-aside 56
	CHECK(ObjectGeometry::maxHeightAbovePosition(gate->geometryShapes()) == 60.0f);
}

// The labelled path is untouched: preference 0 (RW 0xAD30E0 case 0, the AI's findNearestLabeledContactPointOnTarget
// and the radial menu's "Menu" point) still answers the nearest point of the requested label whatever state the
// gate is in - only preference 3, the weapon aim, follows the active geometry here.
TEST_CASE("PORT-435 aim: preference 0 keeps answering the nearest labelled contact point of a gate standing open")
{
	CombatWorld w(kPort435Gate);
	Object *gate = w.unit("Port435Gate", 'B', 0.0f, 0.0f);
	REQUIRE(gate != nullptr);
	w.frames(2);
	openGeometry(*gate);

	// the two "Ram" rows: (20, -36, 10) and (20, 36, 10); the caller stands to the -Y side, so the -36 one is
	// nearer, and it overlaps the active OpenRight panel (the probe of RW 0xAD3415 .. RW 0xAD3431)
	Coord3D out{ -1.0f, -1.0f, -1.0f };
	const Coord3D caller{ 100.0f, -100.0f, 0.0f };
	REQUIRE(ObjectGeometry::worldspaceBestContactPoint(*gate, caller, "Ram", 0, 0, false, out));
	checkPoint(out, 20.0f, -36.0f, 10.0f, "the nearer Ram row");
}
