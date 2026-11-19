// OpenBFME. GPL-3.0.
//
// LOCAL-89 (issue #715): the terrain decal of the model draw (docs/ennorath/W3DSMD_FUNCTIONS.md). RotWK's draw module vtable RW 0xBDFFE8 slots 19-22
// (RW 0x4B95AD, 0x4B94BC, 0x4B2A1B, 0x4B2A3B) make, size and fade a SHADOW_ALPHA_DECAL decal shadow with a texture of the table RW 0xD99F40 (EXHorde,
// EXHorde_UP, EXHordeB, EXHordeB_UP, EXJunkCrate) or a named one, sized by the template's ShadowSizeX / Y / OffsetX / Y. Drawable RW 0x672B96 /
// 0x672BBB hand the type to the first draw module, and RW 0x675C25 .. 0x675D1E fade it by the rate RW 0x67093E set, removing it at 0.
// Synthetic INI, no mod data. Without the port the decal is never made: these checks fail.

#include "doctest.h"

#include "ObjectTestUtil.h"
#include "W3dSynth.h"

#include "GameClient/Drawable.h"
#include "GameClient/ObjectShadow.h"
#include "GameClient/SelectionDecals.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DDrawModules.h"
#include "Libraries/WWVegas/WW3D2/assetmgr.h"

#include <string>
#include <vector>

namespace
{
const char *const kIni =
	"Object L89Horde\n"
	"  ShadowSizeX = 40\n"
	"  ShadowSizeY = 30\n"
	"  ShadowOffsetX = 2\n"
	"  Draw = W3DScriptedModelDraw ModuleTag_01\n"
	"    DefaultModelConditionState\n"
	"      Model = None\n"
	"    End\n"
	"  End\n"
	"End\n";

const SelectionDecal *findDecal(const std::vector<SelectionDecal> &decals, const std::string &texture)
{
	for (const SelectionDecal &d : decals)
	{
		if (d.texture == texture)
		{
			return &d;
		}
	}
	return nullptr;
}
} // namespace

TEST_CASE("LOCAL-89 terrain decal: Drawable::setTerrainDecal makes the table's decal on the first model draw at the template's shadow size "
		  "(RW 0x672B96, 0x4B94BC, 0xD99F40), a named one is type 6 (RW 0x672BBB, 0x4B95AD), type 7 removes it, and it is drawn as an alpha decal "
		  "only while shadows are on")
{
	objtest::World w;
	W3DDrawModules::registerTypedDrawModuleData(w.modules); // the typed W3DModelDrawModuleData of the draw modules
	REQUIRE(w.load(kIni).empty());
	const ThingTemplate *tt = w.get("L89Horde");
	REQUIRE(tt != nullptr);
	w3dsynth::MemoryFileSource fs;
	WW3DAssetManager assets(fs);
	W3DLuaDrawScriptHost::Options hostOpt;
	DrawServices services(assets, w.modules, hostOpt);
	std::vector<std::string> problems;
	Drawable d(services, tt, (DrawableID)1, (ObjectID)10, {}, problems);
	const Coord3D pos{ 100.0f, 200.0f, 10.0f };
	d.setPosition(&pos);
	REQUIRE(!d.entries().empty());
	REQUIRE(d.entries().front().draw);
	const W3DScriptedModelDraw &draw = *d.entries().front().draw;

	CHECK(d.terrainDecalType() == 7);
	CHECK(draw.terrainDecal() == nullptr);

	d.setTerrainDecal(1);
	CHECK(d.terrainDecalType() == 1);
	REQUIRE(draw.terrainDecal() != nullptr);
	CHECK(draw.terrainDecal()->texture == "EXHorde");
	CHECK(draw.terrainDecal()->sizeX == 40.0f);
	CHECK(draw.terrainDecal()->sizeY == 30.0f);
	CHECK(draw.terrainDecal()->offsetX == 2.0f);
	CHECK(draw.terrainDecal()->opacity == 255);

	std::vector<SelectionDecal> decals;
	ObjectShadow::buildDrawableDecalShadows(d, decals, true);
	const SelectionDecal *sd = findDecal(decals, "EXHorde.tga");
	REQUIRE(sd != nullptr);
	CHECK(sd->object == (ObjectID)10);
	CHECK(sd->style == 0x0020u);
	CHECK(sd->size == 40.0f);
	CHECK(sd->sizeY == 30.0f);
	CHECK((sd->color >> 24) == 255u);

	d.setTerrainDecal(2);
	REQUIRE(draw.terrainDecal() != nullptr);
	CHECK(draw.terrainDecal()->texture == "EXHorde_UP");
	d.setTerrainDecal(5);
	CHECK(draw.terrainDecal()->texture == "EXJunkCrate");

	d.setTerrainDecalTexture("L89Ring");
	CHECK(d.terrainDecalType() == 6);
	REQUIRE(draw.terrainDecal() != nullptr);
	CHECK(draw.terrainDecal()->texture == "L89Ring");

	d.setShadowsEnabled(false); // RW 0x4B9662 .. 0x4B9677: the decal is not drawn with shadows off
	ObjectShadow::buildDrawableDecalShadows(d, decals, true);
	CHECK(findDecal(decals, "L89Ring.tga") == nullptr);
	d.setShadowsEnabled(true);

	d.setTerrainDecal(7);
	CHECK(draw.terrainDecal() == nullptr);
	ObjectShadow::buildDrawableDecalShadows(d, decals, true);
	CHECK(findDecal(decals, "L89Ring.tga") == nullptr);
}

TEST_CASE("LOCAL-89 terrain decal fade: the module gets the opacity before each step of rate x frames, a fade-in stops at 1 and a fade-out removes the "
		  "decal at 0 (RW 0x67093E, 0x670972, 0x675C25 .. 0x675D1E, 0x4B2A3B)")
{
	objtest::World w;
	W3DDrawModules::registerTypedDrawModuleData(w.modules); // the typed W3DModelDrawModuleData of the draw modules
	REQUIRE(w.load(kIni).empty());
	w3dsynth::MemoryFileSource fs;
	WW3DAssetManager assets(fs);
	W3DLuaDrawScriptHost::Options hostOpt;
	DrawServices services(assets, w.modules, hostOpt);
	std::vector<std::string> problems;
	Drawable d(services, w.get("L89Horde"), (DrawableID)1, (ObjectID)10, {}, problems);
	REQUIRE(!d.entries().empty());
	REQUIRE(d.entries().front().draw);
	const W3DScriptedModelDraw &draw = *d.entries().front().draw;

	d.setTerrainDecal(1);
	d.setTerrainDecalOpacity(0.0f);
	d.setTerrainDecalFadeTarget(1.0f, 0.25f, 100);
	d.advanceTerrainDecal(101);
	CHECK(draw.terrainDecal()->opacity == 0);   // the opacity before the step
	CHECK(d.terrainDecalOpacity() == 0.25f);
	d.advanceTerrainDecal(103);                  // two frames
	CHECK(draw.terrainDecal()->opacity == 63);  // int(0.25 * 255)
	CHECK(d.terrainDecalOpacity() == 0.75f);
	d.advanceTerrainDecal(103);                  // the same frame counts as one: the module gets 0.75, the opacity reaches 1
	CHECK(d.terrainDecalOpacity() == 1.0f);      // held at 1, the rate 0, the module at full
	CHECK(draw.terrainDecal()->opacity == 255);
	d.advanceTerrainDecal(110);                  // the rate is 0 now: nothing moves
	CHECK(draw.terrainDecal()->opacity == 255);

	d.setTerrainDecalFadeTarget(0.0f, -0.5f, 200);
	d.advanceTerrainDecal(201);
	CHECK(draw.terrainDecal()->opacity == 255);
	CHECK(d.terrainDecalOpacity() == 0.5f);
	d.advanceTerrainDecal(202);                  // reaches 0: the decal goes (type 7)
	CHECK(d.terrainDecalType() == 7);
	CHECK(draw.terrainDecal() == nullptr);
	CHECK(d.terrainDecalOpacity() == 0.0f);
}
