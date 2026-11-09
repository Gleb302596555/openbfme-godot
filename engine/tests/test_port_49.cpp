// PORT-49: W3DModelDraw/AlphaCameraFadeInnerRadius (issue #153). Synthetic cases only; expectations are worked out from
// RW 0x4B52C8..0x4B5494 (docs/ennorath/PORT49_RW.md), not from game files. The radii are powers of two so the between-radii
// alpha is an exact float: outer 256, inner 64, at-inner 25%, distance 160 -> ((192-96)*0.25+96)/192 = 0.625.
#include "doctest.h"

#include "ObjectTestUtil.h"
#include "W3DDrawTestUtil.h"
#include "W3dSynth.h"

#include "GameClient/Drawable.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DDrawModules.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DScriptedModelDraw.h"
#include "Libraries/WWVegas/WW3D2/assetmgr.h"

#include <memory>
#include <string>

using namespace drawtest;
using namespace w3dsynth;

namespace
{
void buildColumn(MemoryFileSource &fs)
{
	fs.Add("art\\w3d\\ab\\abunit_skl.w3d", hierarchyChunk("ABUNIT_SKL", { pivot("ROOTTRANSFORM", 0xFFFFFFFFu, 0, 0, 0) }));
	std::vector<std::uint8_t> bytes;
	SynthMesh body;
	body.Vertices = { { 0, 0, 0 }, { 1, 0, 0 }, { 1, 1, 0 }, { 0, 1, 0 } };
	body.Triangles = { 0, 1, 2, 0, 2, 3 };
	body.Container = "ABUNIT_SKN";
	body.Name = "BODY";
	append(bytes, meshChunk(body));
	append(bytes, hlodChunk("ABUNIT_SKN", "ABUNIT_SKL", { { "ABUNIT_SKN.BODY", 0 } }));
	fs.Add("art\\w3d\\ab\\abunit_skn.w3d", bytes);
}

const char *kDefault = "DefaultModelConditionState\n  Model = ABUNIT_SKN\n  Skeleton = ABUNIT_SKL\nEnd\n";

bool hasStop(const W3DScriptedModelDraw &d, const char *id)
{
	for (const W3DStopHit &h : d.stops())
	{
		if (h.Id == id)
		{
			return true;
		}
	}
	return false;
}

struct FadeRig
{
	MemoryFileSource fs;
	std::unique_ptr<WW3DAssetManager> assets;
	std::unique_ptr<WW3DDrawAssets> drawAssets;
	Harness harness;
	W3DClientRandom random;
	std::unique_ptr<W3DScriptedModelDraw> draw;
	W3DModelDrawModuleData *data = nullptr;

	explicit FadeRig(const std::string &fields, bool cameraAvailable = true) : random(RandomAlgorithm::ZH_CarryChain)
	{
		buildColumn(fs);
		assets.reset(new WW3DAssetManager(fs));
		drawAssets.reset(new WW3DDrawAssets(*assets));
		data = harness.parse(fields + kDefault + "End\n");
		REQUIRE_MESSAGE(data, harness.error);
		random.seed(1);
		W3DScriptedModelDraw::Options o;
		o.cameraFadeAvailable = cameraAvailable;
		draw.reset(new W3DScriptedModelDraw(*data, *drawAssets, random, nullptr, o));
	}
};

const char *kFade = "AlphaCameraFadeOuterRadius = 256\nAlphaCameraFadeInnerRadius = 64\nAlphaCameraAtInnerRadius = 25%\n";
} // namespace

TEST_CASE("PORT-49 parse: the camera rows keep the RW defaults and the positive-real refusal")
{
	Harness h;
	W3DModelDrawModuleData *d = h.parse(std::string(kDefault) + "End\n");
	REQUIRE_MESSAGE(d, h.error);
	CHECK(d->m_alphaCameraFadeOuterRadius == 0.0f); // RW 0x4C86F7
	CHECK(d->m_alphaCameraFadeInnerRadius == 0.0f); // RW 0x4C86FF
	CHECK(d->m_alphaCameraAtInnerRadius == 1.0f);   // RW 0x4C8707 / 0x4C8722, float 0xBD1908
	d = h.parse(std::string(kFade) + kDefault + "End\n");
	REQUIRE_MESSAGE(d, h.error);
	CHECK(d->m_alphaCameraFadeOuterRadius == 256.0f);
	CHECK(d->m_alphaCameraFadeInnerRadius == 64.0f);
	CHECK(d->m_alphaCameraAtInnerRadius == 0.25f); // RW 0x42EEFA -> 0x42EB18, percent to real
	h.parse(std::string("AlphaCameraFadeInnerRadius = 0\n") + kDefault + "End\n");
	CHECK_FALSE(h.error.empty()); // RW 0x42ED1C refuses <= 0 (message 0xBD4304)
}

TEST_CASE("PORT-49 fade: a static model draw fades by XY camera distance (RW 0x4B52C8)")
{
	FadeRig r(kFade);
	const Coord3D camera{ 0.0f, 0.0f, 1000.0f };
	// outside / at the outer radius: opaque (RW 0x4B5366 takes the outer tail when dist2 >= outer^2)
	r.draw->updateCameraFade(camera, Coord3D{ 300.0f, 0.0f, 0.0f }, 4);
	CHECK(r.draw->opacity() == 1.0f);
	CHECK_FALSE(r.draw->hiddenByFade());
	r.draw->updateCameraFade(camera, Coord3D{ 256.0f, 0.0f, 0.0f }, 4);
	CHECK(r.draw->opacity() == 1.0f);
	// at / inside the inner radius: the at-inner alpha (RW 0x4B53AB..0x4B53C7)
	r.draw->updateCameraFade(camera, Coord3D{ 64.0f, 0.0f, 0.0f }, 4);
	CHECK(r.draw->opacity() == 0.25f);
	r.draw->updateCameraFade(camera, Coord3D{ 0.0f, 0.0f, 0.0f }, 4);
	CHECK(r.draw->opacity() == 0.25f);
	// between the radii (RW 0x4B53C9..0x4B5401); Z is not read (RW 0x4B532A..0x4B5353)
	r.draw->updateCameraFade(camera, Coord3D{ 160.0f, 0.0f, 500.0f }, 4);
	CHECK(r.draw->opacity() == 0.625f);
	CHECK_FALSE(r.draw->hiddenByFade());
	r.draw->updateCameraFade(Coord3D{ 0.0f, 0.0f, 9999.0f }, Coord3D{ 0.0f, 160.0f, -500.0f }, 4);
	CHECK(r.draw->opacity() == 0.625f);
}

TEST_CASE("PORT-49 fade: at-inner >= 1 leaves the draw alone (RW 0x4B52E8)")
{
	FadeRig r("AlphaCameraFadeOuterRadius = 256\nAlphaCameraFadeInnerRadius = 64\nAlphaCameraAtInnerRadius = 100%\n");
	r.draw->updateCameraFade(Coord3D{ 0.0f, 0.0f, 0.0f }, Coord3D{ 0.0f, 0.0f, 0.0f }, 4);
	CHECK(r.draw->opacity() == 1.0f);
	CHECK_FALSE(r.draw->hiddenByFade());
	FadeRig over("AlphaCameraFadeOuterRadius = 256\nAlphaCameraFadeInnerRadius = 64\nAlphaCameraAtInnerRadius = 150%\n");
	CHECK(over.data->m_alphaCameraAtInnerRadius == 1.5f); // the parser does not clamp (RW 0x42EEFA)
	over.draw->updateCameraFade(Coord3D{ 0.0f, 0.0f, 0.0f }, Coord3D{ 0.0f, 0.0f, 0.0f }, 4);
	CHECK(over.draw->opacity() == 1.0f);
}

TEST_CASE("PORT-49 fade: zero alpha hides; EffectsLOD VeryLow/Low hides inside the outer radius (RW 0x4B5405 / 0x4B544F)")
{
	FadeRig r("AlphaCameraFadeOuterRadius = 256\nAlphaCameraFadeInnerRadius = 64\nAlphaCameraAtInnerRadius = 0%\n");
	const Coord3D camera{ 0.0f, 0.0f, 0.0f };
	r.draw->updateCameraFade(camera, Coord3D{ 0.0f, 0.0f, 0.0f }, 4);
	CHECK(r.draw->opacity() == 0.0f);
	CHECK(r.draw->hiddenByFade()); // RW 0x4B544F: visible byte 0, render hidden toggle 1
	r.draw->updateCameraFade(camera, Coord3D{ 160.0f, 0.0f, 0.0f }, 1);
	CHECK(r.draw->opacity() == 1.0f); // the override jumps to the outer tail's opacity (RW 0x4B5433 -> 0x4B538F)
	CHECK(r.draw->hiddenByFade());
	r.draw->updateCameraFade(camera, Coord3D{ 160.0f, 0.0f, 0.0f }, 2);
	CHECK(r.draw->opacity() == 0.5f);
	CHECK_FALSE(r.draw->hiddenByFade());
	r.draw->updateCameraFade(camera, Coord3D{ 160.0f, 0.0f, 0.0f }, -1); // no manager: the global-null branch (RW 0x4B540C)
	CHECK(r.draw->opacity() == 0.5f);
	r.draw->updateCameraFade(camera, Coord3D{ 300.0f, 0.0f, 0.0f }, 0); // outside: the override is not reached
	CHECK(r.draw->opacity() == 1.0f);
	CHECK_FALSE(r.draw->hiddenByFade());
}

TEST_CASE("PORT-49 stops: S-096 keeps the unported fades, not a wired camera fade")
{
	FadeRig wired(kFade, true);
	CHECK_FALSE(hasStop(*wired.draw, "S-096"));
	FadeRig unwired(kFade, false);
	CHECK(hasStop(*unwired.draw, "S-096")); // the core has no camera source (the device layer installs one)
	FadeRig sort(std::string("StaticSortLevelWhileFading = 0\n") + kFade, true);
	CHECK(hasStop(*sort.draw, "S-096")); // the static-sort write stays unported
	FadeRig birth(std::string("BirthFadeTime = 1000\n") + kFade, true);
	CHECK(hasStop(*birth.draw, "S-096")); // the birth fade stays unported
}

TEST_CASE("PORT-49 path: Drawable advances a static model draw's camera fade without an animation")
{
	objtest::World w;
	W3DDrawModules::registerTypedDrawModuleData(w.modules);
	const std::string ini =
		"Object P49Column\n"
		"  Draw = W3DScriptedModelDraw ModuleTag_01\n"
		"    DefaultModelConditionState\n"
		"      Model = ABUNIT_SKN\n"
		"      Skeleton = ABUNIT_SKL\n"
		"    End\n"
		"    AlphaCameraFadeOuterRadius = 256\n"
		"    AlphaCameraFadeInnerRadius = 64\n"
		"    AlphaCameraAtInnerRadius = 25%\n"
		"  End\n"
		"End\n";
	REQUIRE(w.load(ini).empty());
	const ThingTemplate *tt = w.get("P49Column");
	REQUIRE(tt != nullptr);
	MemoryFileSource fs;
	buildColumn(fs);
	WW3DAssetManager assets(fs);
	W3DLuaDrawScriptHost::Options hostOpt;
	DrawServices services(assets, w.modules, hostOpt);
	Coord3D camera{ 0.0f, 0.0f, 1000.0f };
	services.cameraPosition = [&camera](Coord3D &out) { out = camera; return true; };
	services.effectsLOD = []() { return 4; };
	std::vector<std::string> problems;
	Drawable d(services, tt, (DrawableID)1, (ObjectID)10, {}, problems);
	REQUIRE(problems.empty());
	REQUIRE(d.entries().size() == 1);
	const Coord3D pos{ 160.0f, 0.0f, 0.0f };
	d.setPosition(&pos);
	d.advanceAnimation(0.0);
	const DrawEntry &e = d.entries()[0];
	REQUIRE(e.draw != nullptr);
	CHECK_FALSE(e.animated); // a DefaultModelConditionState prop: the old animated-only path never ran its fade
	CHECK(e.draw->opacity() == 0.625f);
	CHECK_FALSE(e.draw->hiddenByFade());
	camera = Coord3D{ 1000.0f, 0.0f, 1000.0f };
	d.advanceAnimation(0.0);
	CHECK(e.draw->opacity() == 1.0f);
	// the client job pool's entry point runs the same consumer (RW 0x4BF586 is before the track step on both port paths)
	camera = Coord3D{ 0.0f, 0.0f, 1000.0f };
	d.advanceAnimationLocal(0.0);
	CHECK(e.draw->opacity() == 0.625f);
	CHECK_FALSE(e.draw->hiddenByFade());
}
