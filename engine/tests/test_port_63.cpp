// OpenBFME unit tests. GPL-3.0. PORT-63 / issue #173: W3DSailModelDraw's shared matrix slot.
// Synthetic object INI and W3D chunks only; no retail game or mod data. Expectations come from
// RW 0x4B6B26 (one matrix hook), RW 0x4D00CD..0x4D033B (wind, clamp, first/next calls,
// Matrix3D::Rotate_Z) and RW 0x9F3A3C (base no-op), NOT from the implementation under test.
// These call the post-attachment callback directly; RW 0x4B6A77/0x4B6AA2 only reaches
// that callback when the separate cross-module bone lookup succeeds (remaining S-112).

#include "doctest.h"

#include "ObjectTestUtil.h"
#include "W3dSynth.h"

#include "GameClient/Drawable.h"
#include "GameClient/MapObjectDrawables.h"
#include "GameClient/MapObjectRuntime.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DModelDrawMatrix.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DSailModelDraw.h"

#include <cmath>
#include <memory>
#include <string>
#include <vector>

using namespace w3dsynth;

namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr float kIdentity[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
// A model facing +Y instead of +X (columns X=(0,1,0), Y=(-1,0,0)).
constexpr float kNorth[9] = { 0, -1, 0, 1, 0, 0, 0, 0, 1 };

struct World : objtest::World
{
	World() { W3DDrawModules::registerTypedDrawModuleData(modules); }
};

struct Rig
{
	World world;
	MemoryFileSource files;
	WW3DAssetManager assets;
	DrawServices services;
	std::vector<std::string> problems;

	Rig() : assets(files), services(assets, world.modules, W3DLuaDrawScriptHost::Options{})
	{
		files.Add("art\\w3d\\po\\port63_skl.w3d", hierarchyChunk("PORT63_SKL", { pivot("ROOTTRANSFORM", 0xFFFFFFFFu, 0, 0, 0) }));
		SynthMesh mesh;
		mesh.Name = "BODY";
		mesh.Container = "PORT63_SKN";
		mesh.Vertices = { { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 } };
		mesh.Triangles = { 0, 1, 2 };
		std::vector<std::uint8_t> model;
		append(model, meshChunk(mesh));
		append(model, hlodChunk("PORT63_SKN", "PORT63_SKL", { { "PORT63_SKN.BODY", 0 } }));
		files.Add("art\\w3d\\po\\port63_skn.w3d", model);
		const std::string ini =
			"Object SailNarrow\n"
			"  Draw = W3DSailModelDraw ModuleTag_Sail\n"
			"    MaxRotationDegrees = 30\n    AboutDamping = 0.1\n"
			"    DefaultModelConditionState\n      Model = PORT63_SKN\n    End\n"
			"  End\nEnd\n"
			"Object SailWide\n"
			"  Draw = W3DSailModelDraw ModuleTag_Sail\n"
			"    MaxRotationDegrees = 90\n    AboutDamping = 0.1\n"
			"    DefaultModelConditionState\n      Model = PORT63_SKN\n    End\n"
			"  End\nEnd\n"
			"Object Plain\n"
			"  Draw = W3DScriptedModelDraw ModuleTag_Plain\n"
			"    DefaultModelConditionState\n      Model = PORT63_SKN\n    End\n"
			"  End\nEnd\n"
			"Object Truck\n"
			"  Draw = W3DTruckDraw ModuleTag_Truck\n"
			"    DefaultModelConditionState\n      Model = PORT63_SKN\n    End\n"
			"  End\nEnd\n";
		REQUIRE(world.load(ini).empty());
	}

	std::unique_ptr<Drawable> make(const char *name, DrawableID id)
	{
		auto drawable = std::make_unique<Drawable>(services, world.get(name), id, 1, std::vector<ClientEvent::ClientModule>{}, problems);
		const std::string error = problems.empty() ? "" : problems.front();
		REQUIRE_MESSAGE(problems.empty(), error);
		REQUIRE(drawable->entries().size() == 1);
		REQUIRE(drawable->entries()[0].draw != nullptr);
		REQUIRE(drawable->entries()[0].draw->errors().empty());
		return drawable;
	}
};

void checkBasisTurn(const Matrix3D &matrix, double theta, float x = 0.0f, float y = 0.0f, float z = 0.0f)
{
	// Identity * Rotate_Z(theta) is a clockwise local Z turn in the matrix rows:
	// [cos(theta), -sin(theta); sin(theta), cos(theta)] (RW 0x4D028D..0x4D02C5).
	CHECK(matrix.Row[0][0] == doctest::Approx(std::cos(theta)).epsilon(1e-5)); // tolerance: expected trigonometric values are real angles; float radians and libm sine/cosine round on different precisions.
	CHECK(matrix.Row[0][1] == doctest::Approx(-std::sin(theta)).epsilon(1e-5)); // tolerance: expected trigonometric values are real angles; float radians and libm sine/cosine round on different precisions.
	CHECK(matrix.Row[1][0] == doctest::Approx(std::sin(theta)).epsilon(1e-5)); // tolerance: expected trigonometric values are real angles; float radians and libm sine/cosine round on different precisions.
	CHECK(matrix.Row[1][1] == doctest::Approx(std::cos(theta)).epsilon(1e-5)); // tolerance: expected trigonometric values are real angles; float radians and libm sine/cosine round on different precisions.
	CHECK(matrix.Row[2][2] == 1.0f); // Z axis is exactly unchanged by a Z turn
	CHECK(matrix.Row[0][3] == x);
	CHECK(matrix.Row[1][3] == y);
	CHECK(matrix.Row[2][3] == z);
}
} // namespace

TEST_CASE("PORT-63 sail: the factory selects its matrix callback, fixed wind and MaxRotationDegrees on the first call")
{
	Rig rig;
	auto narrow = rig.make("SailNarrow", 1);
	auto wide = rig.make("SailWide", 2);
	DrawEntry &n = narrow->entries()[0];
	DrawEntry &w = wide->entries()[0];
	REQUIRE(dynamic_cast<W3DSailModelDraw *>(n.draw.get()) != nullptr);
	REQUIRE(dynamic_cast<W3DSailModelDraw *>(w.draw.get()) != nullptr);
	CHECK(n.matrixChangesEachFrame); // a stationary drawable must still push its next matrix to the instancer
	CHECK(w.matrixChangesEachFrame);
	CHECK(n.draw->frame().modelName == "PORT63_SKN"); // unchanged scripted-model resolution
	const Coord3D at{ 12, -8, 3 };
	checkBasisTurn(W3DModelDrawMatrix(*n.draw, kIdentity, at, 1.0f), kPi / 6.0, 12, -8, 3); // +45 wind, capped at +30
	checkBasisTurn(W3DModelDrawMatrix(*w.draw, kIdentity, at, 1.0f), kPi / 4.0, 12, -8, 3); // +45, not +0.1: +0x2EC initially clear
}

TEST_CASE("PORT-63 sail: a stationary ship advances AboutDamping once per matrix and preserves each instance's own last angle")
{
	Rig rig;
	auto ship = rig.make("SailWide", 3);
	auto other = rig.make("SailWide", 4);
	W3DScriptedModelDraw &draw = *ship->entries()[0].draw;
	const Coord3D at{ 4, 9, 2 };
	// Facing +Y: dot(fixed wind, X)=+sqrt(1/2); dot(wind,Y)=-sqrt(1/2),
	// so the first angle is -pi/4. The second/third input is always the SAME
	// stationary identity transform: target +pi/4 but steps only +0.1 per draw.
	const Matrix3D first = W3DModelDrawMatrix(draw, kNorth, at, 1.0f);
	CHECK(first.Row[0][0] == doctest::Approx(-std::sin(-kPi / 4.0)).epsilon(1e-5)); // world X faces +45 after +Y facing and -45 local turn // tolerance: expected trigonometric values are real angles; float radians and libm sine/cosine round on different precisions.
	CHECK(first.Row[1][0] == doctest::Approx(std::cos(-kPi / 4.0)).epsilon(1e-5)); // tolerance: expected trigonometric values are real angles; float radians and libm sine/cosine round on different precisions.
	checkBasisTurn(W3DModelDrawMatrix(draw, kIdentity, at, 1.0f), -kPi / 4.0 + 0.1, 4, 9, 2);
	checkBasisTurn(W3DModelDrawMatrix(draw, kIdentity, at, 1.0f), -kPi / 4.0 + 0.2, 4, 9, 2);
	// RW +0x2E8/+0x2EC belong to EACH runtime, not a shared wind/static angle.
	checkBasisTurn(W3DModelDrawMatrix(*other->entries()[0].draw, kIdentity, at, 1.0f), kPi / 4.0, 4, 9, 2);
}

TEST_CASE("PORT-63 neighbor: the base slot leaves a scaled non-sail model and its pose untouched")
{
	Rig rig;
	auto plain = rig.make("Plain", 5);
	auto truck = rig.make("Truck", 6);
	for (Drawable *d : { plain.get(), truck.get() })
	{
		DrawEntry &e = d->entries()[0];
		CHECK(dynamic_cast<W3DSailModelDraw *>(e.draw.get()) == nullptr);
		CHECK_FALSE(e.matrixChangesEachFrame);
		const Coord3D at{ -1, 4, 11 };
		const Matrix3D matrix = W3DModelDrawMatrix(*e.draw, kNorth, at, 2.0f);
		for (int r = 0; r < 3; ++r)
		{
			for (int c = 0; c < 3; ++c)
			{
				CHECK(matrix.Row[r][c] == 2.0f * kNorth[r * 3 + c]); // RW 0x9F3A3C ret 4
			}
		}
		CHECK(matrix.Row[0][3] == -1);
		CHECK(matrix.Row[1][3] == 4);
		CHECK(matrix.Row[2][3] == 11);
		CHECK(e.draw->frame().modelName == "PORT63_SKN");
		CHECK(e.draw->frame().model != nullptr);
	}
}

TEST_CASE("PORT-63 map objects: their factory selects the sail runtime, without converting a neighboring draw")
{
	Rig rig;
	MapObjectDrawables objects;
	for (const char *name : { "SailWide", "Plain" })
	{
		const ThingTemplate *tt = rig.world.get(name);
		REQUIRE(tt != nullptr);
		MapTemplateInfo &info = objects.templates[tt];
		info.tmpl = tt;
		info.name = name;
		const ThingTemplate::Nugget &n = tt->drawModules().nuggets()[0];
		MapObjectDrawable d;
		d.info = &info;
		d.fate = MAPOBJ_CLIENT_PROP; // use the map's direct drawable path, without a simulation Object
		MapDrawModule m;
		m.className = n.name;
		m.tag = n.tag;
		m.kind = W3D_DRAWKIND_MODEL;
		m.data = n.data.get();
		m.model = "PORT63_SKN";
		d.draws.push_back(m);
		objects.drawables.push_back(d);
	}
	MapObjectRuntime runtime(rig.assets);
	runtime.build(objects);
	REQUIRE(runtime.models().size() == 2);
	REQUIRE(runtime.models()[0].draw != nullptr);
	REQUIRE(runtime.models()[1].draw != nullptr);
	CHECK(dynamic_cast<W3DSailModelDraw *>(runtime.models()[0].draw.get()) != nullptr);
	CHECK(dynamic_cast<W3DSailModelDraw *>(runtime.models()[1].draw.get()) == nullptr);
	checkBasisTurn(W3DModelDrawMatrix(*runtime.models()[0].draw, kIdentity, { 0, 0, 0 }, 1.0f), kPi / 4.0);
	checkBasisTurn(W3DModelDrawMatrix(*runtime.models()[1].draw, kIdentity, { 0, 0, 0 }, 1.0f), 0);
}
