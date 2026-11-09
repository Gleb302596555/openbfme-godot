// OpenBFME unit tests. GPL-3.0.
// PORT-51 (issue #155): Object / InstanceScaleFuzziness — a random size per placed object in BFME1, never read by RotWK 2.01.
//
// Binary verification against RotWK 2.01 game.dat (md5 daed3668006cd90f01c34e5a7da1901f):
//   * Row RW 0xDA4678 of the object table (RW 0xDA3DB8 .. 0xDA49E8): name RW 0xC25654 "InstanceScaleFuzziness", parser RW 0x42ED00
//     (parseReal: mov eax,[esp+0xc]; fstp dword [eax]), userData 0, offset +0x4F4. Neighbours: Scale RW 0xDA43C8 -> +0x4F0,
//     ShadowSizeX RW 0xDA44F8 -> +0x4F8, ShadowSizeY RW 0xDA4508 -> +0x4FC.
//   * ThingTemplate constructor (prologue RW 0x73ED19): +0x4F0 = 1.0 (RW 0x74008C), +0x4F4 = 0.0 (RW 0x740190); the copy
//     function (prologue RW 0x6D1D80) moves both at RW 0x6D21CF .. 0x6D21E1.
//   * Every memory operand with displacement 0x4F4 in .text (21 raw byte hits of F4 04 00 00, 10 instructions, 11 branch-target /
//     immediate coincidences): RW 0x740190 and RW 0x6D21DB/0x6D21E1 are the constructor and the copy above; RW 0x46E2FC / RW 0x46E323
//     are the fog light function (caller RW 0x471A27, lane VISUAL-2), where +0x4F4 is the Y component of a +0x4F0 three-float vector;
//     RW 0x699935, RW 0x892A4A, RW 0x892BC1, RW 0x892DD3 and RW 0x893203 belong to other classes (pointer / byte members).
//     No code reads the field: the verdict and the full table are in docs/ennorath/PORT51_RW_ANALYSIS.md.
//   * The instance scale itself: RW 0x679FD7 `movss xmm0,[edi+0x4F0]` -> RW 0x679FE2 `movss [esi+0x200],xmm0` in Drawable's
//     constructor (edi is the template: RW 0x67A021 takes its draw-module list at +0x2F0). Nothing randomises it.
//
// So RotWK has no random size per placed object, and the port must not invent one: Scale alone sets the instance scale, the fuzziness
// row is parsed and stored (retail data carries it) and read by nobody. These tests pin that; they fail if BFME1's randomisation is ported.

#include "doctest.h"

#include "CombatTestUtil.h"

#include "Common/Thing/ThingTemplate.h"
#include "GameLogic/Object/Object.h"

using namespace combattest;

namespace
{
struct Port51World : CombatWorld
{
	explicit Port51World(const std::string &objects)
		: CombatWorld("")
	{
		const std::string err = w.load(objects, INI_LOAD_OVERWRITE, "port51.ini");
		REQUIRE_MESSAGE(err.empty(), err);
	}
};

// an immobile prop: what a tree / bush looks like to the object model
std::string propIni(const std::string &name, const std::string &extraFields)
{
	return "Object " + name + "\n"
		"  KindOf = IMMOBILE\n"
		"  Geometry = CYLINDER\n"
		"  GeometryMajorRadius = 8\n"
		"  GeometryHeight = 20\n" +
		extraFields +
		"  ArmorSet\n"
		"    Conditions = None\n"
		"    Armor = NoArmor\n"
		"  End\n"
		"  Body = ActiveBody ModuleTag_Body\n"
		"    MaxHealth = 100\n"
		"  End\n"
		"End\n";
}

// the row's slot holds the parsed value (float), or nullptr when the row was not set
const float *fuzzinessOf(const ThingTemplate *tt)
{
	if (!tt)
	{
		return nullptr;
	}
	const FieldValue *v = tt->findField("InstanceScaleFuzziness");
	return v ? std::get_if<float>(v) : nullptr;
}
} // namespace

TEST_CASE("PORT-51 InstanceScaleFuzziness: the template stores the row (RW 0xDA4678, +0x4F4) and the instance scale is exactly Scale (RW 0x679FD7 reads +0x4F0 only)")
{
	const std::string ini = propIni("P51Tree", "  Scale = 0.75\n  InstanceScaleFuzziness = 0.3\n") +
		propIni("P51TreeBig", "  Scale = 2\n  InstanceScaleFuzziness = 0.4\n") +
		propIni("P51NoScale", "  InstanceScaleFuzziness = 0.1\n") + propIni("P51ZeroFuzz", "  InstanceScaleFuzziness = 0\n");
	Port51World w(ini);

	// the row is parsed into the template (parseReal RW 0x42ED00 writes store+0x4F4) with the value from the INI, values the mod uses
	const ThingTemplate *tree = w.w.get("P51Tree");
	REQUIRE(tree != nullptr);
	REQUIRE(fuzzinessOf(tree) != nullptr);
	CHECK(*fuzzinessOf(tree) == 0.3f);
	REQUIRE(fuzzinessOf(w.w.get("P51TreeBig")) != nullptr);
	CHECK(*fuzzinessOf(w.w.get("P51TreeBig")) == 0.4f);
	REQUIRE(fuzzinessOf(w.w.get("P51NoScale")) != nullptr);
	CHECK(*fuzzinessOf(w.w.get("P51NoScale")) == 0.1f);
	REQUIRE(fuzzinessOf(w.w.get("P51ZeroFuzz")) != nullptr);
	CHECK(*fuzzinessOf(w.w.get("P51ZeroFuzz")) == 0.0f);

	// eight instances of the same template: the scale is the template's Scale bit for bit, identical in every instance.
	// A BFME1-style draw (+/- 0.3 around 0.75) would make these differ and would move the logic hash pins.
	float x = 150.0f;
	for (int i = 0; i < 8; ++i)
	{
		Object *o = w.unit("P51Tree", 'A', x, 500.0f);
		x += 30.0f;
		REQUIRE(o != nullptr);
		CHECK(o->getInstanceScale() == 0.75f);
	}

	// Scale = 2 with fuzziness 0.4: still exactly 2 (no 1.2 .. 2.8 spread)
	Object *big = w.unit("P51TreeBig", 'A', 150.0f, 560.0f);
	REQUIRE(big != nullptr);
	CHECK(big->getInstanceScale() == 2.0f);

	// no Scale row: the constructor default 1.0 (RW 0x74008C), whatever the fuzziness says
	Object *noScale = w.unit("P51NoScale", 'A', 150.0f, 620.0f);
	REQUIRE(noScale != nullptr);
	CHECK(noScale->getInstanceScale() == 1.0f);

	// fuzziness 0 with no Scale row: 1.0 as well (the issue's "0 gives scale 1")
	Object *zero = w.unit("P51ZeroFuzz", 'A', 150.0f, 680.0f);
	REQUIRE(zero != nullptr);
	CHECK(zero->getInstanceScale() == 1.0f);
}

TEST_CASE("PORT-51 InstanceScaleFuzziness: Scale still sets the instance scale and a ChildObject inherits both rows (copy RW 0x6D21DB), the child's fuzziness changes nothing")
{
	const std::string ini = propIni("P51Wall", "  Scale = 0.5\n  InstanceScaleFuzziness = 0.3\n") +
		propIni("P51Plain", "  Scale = 0.5\n") + "ChildObject P51Child P51Wall\n  InstanceScaleFuzziness = 0.4\nEnd\n" +
		"ChildObject P51ChildScale P51Wall\n  Scale = 0.25\nEnd\n";
	Port51World w(ini);

	// Scale alone still decides the size (lane RENDER-2): no fuzziness row at all
	const ThingTemplate *plain = w.w.get("P51Plain");
	REQUIRE(plain != nullptr);
	CHECK(fuzzinessOf(plain) == nullptr);
	Object *plainObject = w.unit("P51Plain", 'A', 150.0f, 500.0f);
	REQUIRE(plainObject != nullptr);
	CHECK(plainObject->getInstanceScale() == 0.5f);

	// a copied template carries the row (RW 0x6D21DB moves +0x4F4 with the rest) and keeps the parent's Scale
	const ThingTemplate *child = w.w.get("P51Child");
	REQUIRE(child != nullptr);
	REQUIRE(fuzzinessOf(child) != nullptr);
	CHECK(*fuzzinessOf(child) == 0.4f); // the child overrides the parent's 0.3
	CHECK(*fuzzinessOf(w.w.get("P51Wall")) == 0.3f); // the parent's own value is untouched
	Object *childObject = w.unit("P51Child", 'A', 150.0f, 560.0f);
	REQUIRE(childObject != nullptr);
	CHECK(childObject->getInstanceScale() == 0.5f);

	// a child that changes Scale changes the size; the inherited fuzziness still adds nothing
	Object *childScale = w.unit("P51ChildScale", 'A', 150.0f, 620.0f);
	REQUIRE(childScale != nullptr);
	CHECK(childScale->getInstanceScale() == 0.25f);
	REQUIRE(fuzzinessOf(w.w.get("P51ChildScale")) != nullptr);
	CHECK(*fuzzinessOf(w.w.get("P51ChildScale")) == 0.3f);

	// the parent object itself
	Object *wall = w.unit("P51Wall", 'A', 150.0f, 680.0f);
	REQUIRE(wall != nullptr);
	CHECK(wall->getInstanceScale() == 0.5f);
}
