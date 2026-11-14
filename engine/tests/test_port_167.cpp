// OpenBFME. GPL-3.0.
//
// PORT-167 (issue #396, problem P-72): under the Fury ability Lurtz is no longer tinted red.
//
// Mechanism (RotWK 2.01): the hero special power (SPECIAL_HERO_MODE, SpecialAbilityModules.cpp
// HeroModeSpecialAbilityUpdate) sets a model condition bit for the ability's duration
// (RW 0x68B581 setSpecialModelConditionState). The draw module selects the matching AnimationState
// (W3DScriptedModelDraw::setModelConditionFlags RW 0x4BF2D8 -> select RW 0x4BF15E) whose
// EnteringStateFX (RW 0x4BF222) names a TintDrawable FXList; the FX sets the drawable's custom tint
// envelope (Drawable::setCustomTintEnvelope RW 0x671A73, TINT_STATUS_CUSTOM RW 0x675E9A) and the
// render reads it (Drawable::evaluateRenderTint RW 0x46F4E7). Local 3 (#90) ported the envelope and
// the tint; Local 21 (#161) reworked the model life cycle (setModelState(state, force), the render
// object reuse) and the device's model key (material / render generations, per-mesh colour).
//
// These tests are synthetic (no game files): one hero with an ability that tints the drawable red,
// like Lurtz's data. The drawable's tint colour must be red while the ability lasts, also after a
// model state change in between (the render object is rebuilt, RW 0x4B2C18), and return to none when
// it ends.
//
// SCOPE: these tests exercise the CORE (openbfme_core) end of the chain — Drawable's tint envelope
// (updateTint / evaluateRenderTint) and the draw module's model state machine (setModelConditionFlags
// -> select / EnteringStateFX) — and they PASS on main: the core tint chain and the render-object
// rebuild (the generation bump RW 0x4B2C18) are both intact. The regression (issue #396) is one
// layer down, in the Godot device (GodotGameWorld's per-frame model-key / material regeneration,
// LOCAL-21's material / render generations and the per-instance w3d_tint re-application), which is
// compiled only with -DOPENBFME_BUILD_GODOT=ON and cannot be built or run in a headless unit test
// (it needs the godot-cpp runtime). See the issue comment for the exact device locations.

#include "doctest.h"

#include "ObjectTestUtil.h"
#include "W3dSynth.h"

#include "Common/INIDataTypes.h"
#include "Common/ModelState.h"
#include "GameClient/Drawable.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DDrawModules.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DDrawServices.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DLuaDrawScriptHost.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DScriptedModelDraw.h"
#include "Libraries/WWVegas/WW3D2/assetmgr.h"

#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace w3dsynth;

namespace
{
// Lurtz's bits: the ability sets USER_3 (kMcUser3, RW 0x68B581); USER_4 stands in for the second
// model condition that forces the render object to be rebuilt while the ability still holds.
constexpr int kUser3 = 186; // 0xBA
constexpr int kUser4 = 187; // 0xBB

// The client effect player the draw module calls (DrawableFXHost, lane FX-2). It plays the hero FX
// by setting the drawable's custom tint envelope, exactly as TintDrawableFXNugget::doFXObj does
// through FXServices::tintDrawable (RW 0x5E0127); it records every EnteringStateFX it is asked to
// play so the tests can name the step that stops firing.
class TintFXHost : public DrawableFXHost
{
public:
	Drawable *d = nullptr;
	std::vector<std::string> fired;
	bool enteringStateFX(DrawableID, const std::string &fx) override
	{
		fired.push_back(fx);
		if (d && fx == "FX_P167")
		{
			// the mod's red: R:180 G:0 B:0 scaled to 0..1, the envelope holds it for the ability
			const RGBColor red{ 180.0f / 255.0f, 0.0f, 0.0f };
			d->setCustomTintEnvelope(red, 1u, 1u, 12u, 0.0f, 0.0f);
		}
		return true;
	}
};

// One W3D model (a single mesh, a two-pivot hierarchy) from a base name: "P167" is the hierarchy
// P167_SKL, the mesh container P167_SKN, the files art/w3d/p1/p167_{skl,skn}.w3d (assetmgr.h:
// "GUMAArms_SKN" -> art/w3d/gu/gumaarms_skn.w3d).
void addModel(MemoryFileSource &fs, const std::string &base)
{
	std::string lower = base;
	for (char &c : lower)
	{
		c = (char)std::tolower((unsigned char)c);
	}
	const std::string skl = base + "_SKL";
	const std::string skn = base + "_SKN";
	const std::string dir = lower.substr(0, 2);
	fs.Add("art\\w3d\\" + dir + "\\\\" + lower + "_skl.w3d",
		hierarchyChunk(skl.c_str(), { pivot("ROOTTRANSFORM", 0xFFFFFFFFu, 0, 0, 0), pivot("HIP", 0, 0, 0, 0) }));
	std::vector<std::uint8_t> bytes;
	SynthMesh body;
	body.Vertices = { { 0, 0, 0 }, { 1, 0, 0 }, { 1, 1, 0 } };
	body.Triangles = { 0, 1, 2 };
	body.Container = skn;
	body.Name = "BODY";
	append(bytes, meshChunk(body));
	append(bytes, hlodChunk(skn.c_str(), skl.c_str(), { { skn + ".BODY", 1 } }));
	fs.Add("art\\w3d\\" + dir + "\\\\" + lower + "_skn.w3d", bytes);
}

// The hero: the default model P167, the idle animation, and the USER_3 state whose EnteringStateFX
// is the red tint. The USER_4 model condition swaps to P167B so a model state change (a render
// object rebuild) can happen while the ability still holds.
const char *kHero =
	"Object P167Hero\n"
	"  Draw = W3DScriptedModelDraw ModuleTag_Draw\n"
	"    DefaultModelConditionState\n"
	"      Model = P167_SKN\n"
	"    End\n"
	"    ModelConditionState = USER_4\n"
	"      Model = P167B_SKN\n"
	"    End\n"
	"    IdleAnimationState\n"
	"    End\n"
	"    AnimationState = USER_3\n"
	"      EnteringStateFX = FX_P167\n"
	"    End\n"
	"  End\n"
	"End\n";

struct Site
{
	objtest::World world;
	MemoryFileSource fs;
	std::unique_ptr<WW3DAssetManager> assets;
	DrawServices *services = nullptr;
	TintFXHost host;
	Drawable *d = nullptr;
	std::vector<std::string> problems;

	Site()
	{
		W3DDrawModules::registerTypedDrawModuleData(world.modules);
		addModel(fs, "P167");
		addModel(fs, "P167B");
		assets.reset(new WW3DAssetManager(fs));
		services = new DrawServices(*assets, world.modules, W3DLuaDrawScriptHost::Options());
		services->fx = &host;
		const std::string err = world.load(kHero, INI_LOAD_OVERWRITE, "p167.ini");
		if (!err.empty())
		{
			throw std::runtime_error(err);
		}
		const ThingTemplate *tt = world.get("P167Hero");
		if (!tt)
		{
			throw std::runtime_error("P167Hero not found");
		}
		d = new Drawable(*services, tt, (DrawableID)1, (ObjectID)10, {}, problems);
		const Coord3D pos{ 100.0f, 200.0f, 0.0f };
		d->setPosition(&pos);
		host.d = d;
	}
	~Site()
	{
		delete d; // the drawable borrows the services; the services borrow the assets and the world
		delete services;
	}
	// one client frame: step the tint envelopes (Drawable::updateTint)
	void frame()
	{
		d->updateTint();
	}
	// the render tint the device would apply (Drawable::evaluateRenderTint); false when there is none
	bool renderTint(RGBColor &out) const
	{
		return d->evaluateRenderTint(0u, out);
	}
	void flags(bool user3, bool user4)
	{
		ModelConditionFlags f;
		if (user3)
		{
			f.set(kUser3);
		}
		if (user4)
		{
			f.set(kUser4);
		}
		d->setModelConditionFlags(f);
	}
};
} // namespace

TEST_CASE("PORT-167 lurtz fury: the ability tints the drawable red, the tint survives a model state change in between, and returns to none when it ends (RW 0x4BF222, 0x671A73, 0x46F4E7)")
{
	Site s;
	REQUIRE(s.d != nullptr);
	RGBColor c{};
	// resting: no tint
	s.frame();
	CHECK_FALSE(s.renderTint(c));
	// the ability: USER_3 selects the hero state and its EnteringStateFX fires the red tint
	s.flags(true, false);
	REQUIRE(s.host.fired.size() == 1);
	CHECK(s.host.fired[0] == "FX_P167");
	const std::uint32_t genBefore = s.d->entries()[0].draw->renderObjectGeneration();
	// the envelope starts (updateTint plays it once CUSTOM was seen new) and ramps to the peak over
	// the attack frames
	for (int i = 0; i < 3; ++i)
	{
		s.frame();
	}
	CHECK(s.renderTint(c));
	CHECK(c.red > 0.0f); // tolerance: red is the peak (180/255) scaled by the envelope pulse and attack ramp, so it is frame-dependent; >0 proves the tint is present
	CHECK(c.green == 0.0f);
	CHECK(c.blue == 0.0f);
	// a model state change in between: USER_4 rebuilds the render object (P167 -> P167B) while the
	// ability still holds; the animation state is unchanged, so the tint keeps the colour it had
	s.flags(true, true);
	const std::uint32_t genAfter = s.d->entries()[0].draw->renderObjectGeneration();
	CHECK(genAfter > genBefore); // tolerance: the render-object generation is a monotonic counter (RW 0x4B2C18), not a fixed value; strictly greater proves the rebuild happened
	for (int i = 0; i < 3; ++i)
	{
		s.frame();
	}
	CHECK(s.renderTint(c));
	CHECK(c.red > 0.0f); // tolerance: same frame-dependent pulse as above; the point is the tint SURVIVES the render-object rebuild
	CHECK(c.green == 0.0f);
	CHECK(c.blue == 0.0f);
	// the ability ends: the model condition clears, the envelope runs out, the tint returns to none
	s.flags(false, false);
	for (int i = 0; i < 20; ++i)
	{
		s.frame();
	}
	CHECK_FALSE(s.renderTint(c));
}

TEST_CASE("PORT-167 lurtz fury: a second casting re-fires the state FX and restarts the tint (RW 0x4BF222)")
{
	Site s;
	REQUIRE(s.d != nullptr);
	RGBColor c{};
	s.flags(true, false);
	for (int i = 0; i < 3; ++i)
	{
		s.frame();
	}
	CHECK(s.renderTint(c));
	// drop the condition and let the first tint run out
	s.flags(false, false);
	for (int i = 0; i < 20; ++i)
	{
		s.frame();
	}
	CHECK_FALSE(s.renderTint(c));
	// cast again: the state is re-selected and the FX fires a second time
	s.flags(true, false);
	REQUIRE(s.host.fired.size() == 2);
	CHECK(s.host.fired[1] == "FX_P167");
	for (int i = 0; i < 3; ++i)
	{
		s.frame();
	}
	CHECK(s.renderTint(c));
	CHECK(c.red > 0.0f); // tolerance: same frame-dependent pulse; the re-fired tint is present
	CHECK(c.green == 0.0f);
	CHECK(c.blue == 0.0f);
}
