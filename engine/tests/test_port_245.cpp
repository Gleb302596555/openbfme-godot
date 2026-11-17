// PORT-245: the five W3DScriptedModelDraw fields that RotWK parses and reads but the port stored without a consumer
// (issue #569): MinLODRequired, HighDetailOnly, UseProducerTexture, UseDefaultAnimation, BirthFadeAdditive. Synthetic cases
// only; the expectations are worked out from the RotWK 2.01 binary, not from game files.
//   MinLODRequired      RW 0x4B4262 `cmp byte [esi+0x64], 0` in the idle-like predicate RW 0x4B424F (forces the predicate true;
//                       the caller RW 0x4BE9E6 then uses the 9999 pick limit).
//   HighDetailOnly      RW 0x4C7312 `cmp byte [edi+0x15F], 0` / `cmp dword [ebx+0xBC], 0` / `jne 0x4C78D3` in doDrawModule
//                       (RW 0x4C72BE): the whole body (camera fade RW 0x4B52C8, advance RW 0x4BF560) is left unrun when the
//                       detail LOD (draw +0xBC, the 0x4B24C4 mapping stored by the constructor RW 0x4C0895) is not 0 (high);
//                       the flags-apply gate RW 0x4C0993 (`cmp byte [ecx+0x15F], bl` / `cmp dword [esi+0xBC], ebx` / `jne
//                       0x4C0B07`) leaves the apply unrun under the same condition.
//   UseProducerTexture  RW 0x4C23B1: retail takes the RandomTexture variation from drawable + 0xFC's + 0x78 instead of the
//                       drawable's own + 0x364. The port has no drawable + 0xFC object: left (S-5015).
//   UseDefaultAnimation RW 0x4C5235 `cmp byte [eax+0x138], 0` / `je`: when set, the draw-update logs retail's line (format
//                       RW 0xBD4980 "%s.%s", log RW 0x437A90).
//   BirthFadeAdditive   RW 0x4C09F6 `mov al, byte [eax+0x154]`: stored to draw +0x270 and gates the render object's birth-fade
//                       call RW 0x4C0A12 (vtable +0x154); the playback is not applied (S-096), reported as S-7399.
#include "doctest.h"

#include "W3DDrawTestUtil.h"
#include "W3dSynth.h"

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
	body.Triangles = { { 0, 1, 2, 0, 2, 3 } };
	body.Container = "ABUNIT_SKN";
	body.Name = "BODY";
	append(bytes, meshChunk(body));
	append(bytes, hlodChunk("ABUNIT_SKN", "ABUNIT_SKL", { { "ABUNIT_SKN.BODY", 0 } }));
	fs.Add("art\\w3d\\ab\\abunit_skn.w3d", bytes);
	// one looping animation clip (the advance/apply cases need a resolving clip)
	std::vector<float> zVals(11, 0.0f);
	for (int f = 0; f < 11; ++f)
	{
		zVals[(size_t)f] = (float)f * 2.0f;
	}
	std::vector<SynthChannel> channels = { SynthChannel{ ANIM_CHANNEL_Z, 0, 0, 1, zVals } };
	fs.Add("art\\w3d\\ab\\abunit_walk.w3d", rawAnimChunk("ABUNIT_WALK", "ABUNIT_SKL", 11, 30, channels));
}

const char *kDefault = "DefaultModelConditionState\n  Model = ABUNIT_SKN\n  Skeleton = ABUNIT_SKL\nEnd\n";

struct Rig
{
	MemoryFileSource fs;
	std::unique_ptr<WW3DAssetManager> assets;
	std::unique_ptr<WW3DDrawAssets> drawAssets;
	Harness harness;
	W3DClientRandom random;
	std::unique_ptr<W3DScriptedModelDraw> draw;
	W3DModelDrawModuleData *data = nullptr;

	Rig(const std::string &fields, int lodNumber = 0) : random(RandomAlgorithm::ZH_CarryChain)
	{
		buildColumn(fs);
		assets.reset(new WW3DAssetManager(fs));
		drawAssets.reset(new WW3DDrawAssets(*assets));
		data = harness.parse(fields + kDefault + "End\n");
		REQUIRE_MESSAGE(data, harness.error);
		random.seed(1);
		W3DScriptedModelDraw::Options o;
		o.cameraFadeAvailable = true;
		o.animationLodNumber = lodNumber; // 0 HIGH, 1 MEDIUM, 2 LOW (RW 0x4B24C4)
		draw.reset(new W3DScriptedModelDraw(*data, *drawAssets, random, nullptr, o));
	}
};

// A draw whose weighted-pick limit is 1, so the idle-like predicate's 9999 limit (RW 0x4BE9E6) is observable: with the
// limit 1 a two-animation state can only pick animation 0 (RW 0x4B4272 `size < 2` returns 0), with 9999 it picks by weight.
struct LimitDraw : W3DScriptedModelDraw
{
	using W3DScriptedModelDraw::W3DScriptedModelDraw;
	int maxRandomAnimations() const override { return 1; }
};

const char *kTwoAnimState =
	"AnimationState MOVING\n" // a condition: the state is not idle-like without MinLODRequired
	"  StateName = STATE_Move\n"
	"  Animation = A\n"
	"    AnimationName = ABUNIT_WALK\n"
	"    AnimationPriority = 0\n"
	"  End\n"
	"  Animation = B\n"
	"    AnimationName = ABUNIT_WALK\n"
	"    AnimationPriority = 1\n"
	"  End\n"
	"End\n";
} // namespace

TEST_CASE("PORT-245 parse: the five fields parse into the module data")
{
	Harness h;
	W3DModelDrawModuleData *d = h.parse(std::string(kDefault) + "End\n");
	REQUIRE_MESSAGE(d, h.error);
	// the RW constructor defaults (RW 0x4C86DC..0x4C87D7: mov byte [esi+0x154/0x15F/0x138/0xB8], bl with bl = 0)
	CHECK(d->m_minLODRequired == 0);
	CHECK_FALSE(d->m_highDetailOnly);
	CHECK_FALSE(d->m_useProducerTexture);
	CHECK_FALSE(d->m_useDefaultAnimation);
	CHECK_FALSE(d->m_birthFadeAdditive);

	d = h.parse(std::string("MinLODRequired = High\nHighDetailOnly = Yes\nUseProducerTexture = Yes\nUseDefaultAnimation = Yes\n"
		"BirthFadeAdditive = Yes\n") + kDefault + "End\n");
	REQUIRE_MESSAGE(d, h.error);
	CHECK(d->m_minLODRequired == 2); // parseIndexList, kStaticGameLODNames: "High" is index 2 (RW 0xBE1380)
	CHECK(d->m_highDetailOnly);      // row RW 0xBE16A0
	CHECK(d->m_useProducerTexture);  // row RW 0xBE1440
	CHECK(d->m_useDefaultAnimation);  // row RW 0xBE15D0
	CHECK(d->m_birthFadeAdditive);    // row RW 0xBE1660
}

TEST_CASE("PORT-245 HighDetailOnly: the doDrawModule gate (RW 0x4C7312) leaves the body unrun at a non-high LOD")
{
	const char *kFade = "AlphaCameraFadeOuterRadius = 256\nAlphaCameraFadeInnerRadius = 64\nAlphaCameraAtInnerRadius = 25%\n";
	// HighDetailOnly set, LOD 1 (not high): the body is skipped (RW 0x4C731B cmp [ebx+0xBC], 0 / jne 0x4C78D3) — the fade
	// that would drop the opacity to the at-inner alpha must not run.
	Rig gated(std::string("HighDetailOnly = Yes\n") + kFade, 1);
	gated.draw->updateCameraFade(Coord3D{ 0.0f, 0.0f, 1000.0f }, Coord3D{ 0.0f, 0.0f, 0.0f }, 4);
	CHECK(gated.draw->opacity() == 1.0f);
	CHECK_FALSE(gated.draw->hiddenByFade());

	// the same data at LOD 0 (high): the gate lets the body run (RW 0x4C7319 je 0x4C7328)
	Rig high(std::string("HighDetailOnly = Yes\n") + kFade, 0);
	high.draw->updateCameraFade(Coord3D{ 0.0f, 0.0f, 1000.0f }, Coord3D{ 0.0f, 0.0f, 0.0f }, 4);
	CHECK(high.draw->opacity() == 0.25f); // at-inner 25% (RW 0x4B53AB..0x4B53C7)

	// adjacent: without the field the LOD does not gate anything (RW 0x4C7312 is only reached for a set field)
	Rig plain(kFade, 1);
	plain.draw->updateCameraFade(Coord3D{ 0.0f, 0.0f, 1000.0f }, Coord3D{ 0.0f, 0.0f, 0.0f }, 4);
	CHECK(plain.draw->opacity() == 0.25f);
}

TEST_CASE("PORT-245 HighDetailOnly: the advance of the body is left unrun at a non-high LOD")
{
	// a draw with a playing track: at LOD 1 (not high) the advance returns at the gate (RW 0x4C7312), the track frame stays;
	// at LOD 0 (high) it steps. The track is set up through a state with one looping animation.
	const char *kState =
		"IdleAnimationState\n"
		"  StateName = STATE_Idle\n"
		"  Animation = A\n"
		"    AnimationName = ABUNIT_WALK\n"
		"    AnimationMode = LOOP\n"
		"  End\n"
		"End\n";
	Rig gated(std::string("HighDetailOnly = Yes\n") + kState, 1);
	Rig open(std::string("HighDetailOnly = Yes\n") + kState, 0);
	// the state is selected (the flags-apply gate lets it through at LOD 0; at LOD 1 it is skipped, so only the open rig
	// has a playing track — the gate under test is the body gate, exercised on the open rig's advance below)
	open.draw->setModelConditionFlags(ModelConditionFlags());
	gated.draw->setModelConditionFlags(ModelConditionFlags());
	CHECK(open.draw->trackCount() == 1);
	CHECK(gated.draw->trackCount() == 0); // the flags-apply gate (RW 0x4C0993) left the apply unrun
	const float before = open.draw->track(0).frame;
	open.draw->advance(100.0);
	// tolerance: the advance step is render-time-dependent; only its direction (the body ran, RW 0x4C7328) is fixed
	CHECK(open.draw->track(0).frame > before);
}

TEST_CASE("PORT-245 HighDetailOnly: the flags-apply gate (RW 0x4C0993) leaves the apply unrun at a non-high LOD")
{
	const char *kState =
		"AnimationState MOVING\n"
		"  StateName = STATE_Move\n"
		"  Animation = A\n"
		"    AnimationName = ABUNIT_WALK\n"
		"    AnimationMode = LOOP\n"
		"  End\n"
		"End\n";
	Rig gated(std::string("HighDetailOnly = Yes\n") + kState, 1);
	Rig open(std::string("HighDetailOnly = Yes\n") + kState, 0);
	gated.draw->setModelConditionFlags(flagsOf({ "MOVING" }));
	open.draw->setModelConditionFlags(flagsOf({ "MOVING" }));
	// the apply was left unrun (RW 0x4C099B..0x4C0A01 jne 0x4C0B07): no state was entered
	CHECK(gated.draw->log().empty());
	// at LOD 0 the apply runs: the state is entered (RW 0x4C09A7..0x4C09CD)
	bool entered = false;
	for (const std::string &line : open.draw->log())
	{
		if (line.rfind("enter ", 0) == 0)
		{
			entered = true;
		}
	}
	CHECK(entered);
}

TEST_CASE("PORT-245 MinLODRequired: the idle-like predicate term (RW 0x4B4262) raises the pick limit to 9999")
{
	// the rig's maxRandomAnimations() is 1: without MinLODRequired the conditioned state is not idle-like, the limit is 1
	// and a two-animation state can only pick animation 0 (RW 0x4B4272 size < 2 returns 0).
	{
		MemoryFileSource fs;
		buildColumn(fs);
		std::unique_ptr<WW3DAssetManager> assets(new WW3DAssetManager(fs));
		std::unique_ptr<WW3DDrawAssets> drawAssets(new WW3DDrawAssets(*assets));
		Harness h;
		W3DModelDrawModuleData *d = h.parse(std::string(kTwoAnimState) + kDefault + "End\n");
		REQUIRE_MESSAGE(d, h.error);
		W3DClientRandom random(RandomAlgorithm::ZH_CarryChain);
		random.seed(1);
		W3DScriptedModelDraw::Options o;
		LimitDraw draw(*d, *drawAssets, random, nullptr, o);
		draw.setModelConditionFlags(flagsOf({ "MOVING" }));
		CHECK(draw.currentAnimationIndex() == 0); // limit 1: only the first animation is pickable
	}
	// with MinLODRequired set the predicate is forced true (RW 0x4B4262 jne): the limit is 9999 (RW 0x4BE9E6) and the
	// weighted pick (priority 0 then 1, RW 0x4B4272) lands on animation 1.
	{
		MemoryFileSource fs;
		buildColumn(fs);
		std::unique_ptr<WW3DAssetManager> assets(new WW3DAssetManager(fs));
		std::unique_ptr<WW3DDrawAssets> drawAssets(new WW3DDrawAssets(*assets));
		Harness h;
		W3DModelDrawModuleData *d = h.parse(std::string("MinLODRequired = High\n") + kTwoAnimState + kDefault + "End\n");
		REQUIRE_MESSAGE(d, h.error);
		W3DClientRandom random(RandomAlgorithm::ZH_CarryChain);
		random.seed(1);
		W3DScriptedModelDraw::Options o;
		LimitDraw draw(*d, *drawAssets, random, nullptr, o);
		draw.setModelConditionFlags(flagsOf({ "MOVING" }));
		CHECK(draw.currentAnimationIndex() == 1); // limit 9999: the weighted pick reaches the second animation
	}
}

TEST_CASE("PORT-245 UseDefaultAnimation: the draw-update log line is reported once (RW 0x4C5235)")
{
	Rig rig(std::string("UseDefaultAnimation = Yes\n"));
	rig.draw->setModelConditionFlags(ModelConditionFlags());
	bool reported = false;
	for (const std::string &e : rig.draw->errors())
	{
		if (e.find("UseDefaultAnimation") != std::string::npos)
		{
			reported = true;
		}
	}
	CHECK(reported);
	// reported once: a second apply does not repeat the line
	const std::size_t before = rig.draw->errors().size();
	rig.draw->setModelConditionFlags(ModelConditionFlags());
	CHECK(rig.draw->errors().size() == before);
}

TEST_CASE("PORT-245 BirthFadeAdditive: the gated birth-fade call is reported, not made (RW 0x4C09F6)")
{
	Rig rig(std::string("BirthFadeAdditive = Yes\n"));
	rig.draw->setModelConditionFlags(ModelConditionFlags());
	bool stopped = false;
	for (const W3DStopHit &s : rig.draw->stops())
	{
		if (s.Id == "S-7399")
		{
			stopped = true;
		}
	}
	CHECK(stopped);
}

TEST_CASE("PORT-245 UseProducerTexture: parsed, not read (left: S-5015)")
{
	// The field parses and is stored, but the port has no drawable + 0xFC object to take the variation from (RW 0x4C23B1
	// reads [drawable+0xFC]+0x78); the RandomTexture pick keeps using the drawable's own variation (+ 0x364). Stop S-5015.
	Harness h;
	W3DModelDrawModuleData *d = h.parse(std::string("UseProducerTexture = Yes\n") + kDefault + "End\n");
	REQUIRE_MESSAGE(d, h.error);
	CHECK(d->m_useProducerTexture);
	Rig rig(std::string("UseProducerTexture = Yes\n"));
	CHECK(rig.draw->errors().empty()); // the port does not act on the field; no invented behaviour
}
