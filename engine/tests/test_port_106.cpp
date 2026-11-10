// PORT-106: W3DScriptedModelDraw MultiPlayerOnly (issue #266). Synthetic cases only; the expectations are worked out from
// RW 0x4C72E1..0x4C730C of W3DModelDraw::doDrawModule (RW 0x4C72BE; docs/ennorath/PORT106_RW_ANALYSIS.md), not from game files.
// The gate: MultiPlayerOnly leaves the module's per-frame body unrun when isInMultiplayerGame (RW 0x441B7C, modes {1,5}) is false
// or GlobalData +0xD45 != 0; the body carries the camera fade (RW 0x4B52C8, called from RW 0x4BF586) and the animation advance
// (RW 0x4BF560, called from RW 0x4C73C7).
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
}

const char *kDefault = "DefaultModelConditionState\n  Model = ABUNIT_SKN\n  Skeleton = ABUNIT_SKL\nEnd\n";

// A draw whose Options carry the MultiPlayerOnly gate flag, exactly as Drawable::addEntry sets it in the game
// (services.inMultiplayerGame && data->multiPlayerOnlySkipsBody(...)).
struct GateRig
{
	MemoryFileSource fs;
	std::unique_ptr<WW3DAssetManager> assets;
	std::unique_ptr<WW3DDrawAssets> drawAssets;
	Harness harness;
	W3DClientRandom random;
	std::unique_ptr<W3DScriptedModelDraw> draw;
	W3DModelDrawModuleData *data = nullptr;

	GateRig(const std::string &fields, bool multiPlayerOnlySkipped) : random(RandomAlgorithm::ZH_CarryChain)
	{
		buildColumn(fs);
		assets.reset(new WW3DAssetManager(fs));
		drawAssets.reset(new WW3DDrawAssets(*assets));
		data = harness.parse(fields + kDefault + "End\n");
		REQUIRE_MESSAGE(data, harness.error);
		random.seed(1);
		W3DScriptedModelDraw::Options o;
		o.cameraFadeAvailable = true;
		o.multiPlayerOnlySkipped = multiPlayerOnlySkipped;
		draw.reset(new W3DScriptedModelDraw(*data, *drawAssets, random, nullptr, o));
	}
};
} // namespace

TEST_CASE("PORT-106 parse: MultiPlayerOnly parses as a bool with the RW false default")
{
	Harness h;
	W3DModelDrawModuleData *d = h.parse(std::string(kDefault) + "End\n");
	REQUIRE_MESSAGE(d, h.error);
	CHECK_FALSE(d->m_multiPlayerOnly); // RW 0x4C8736: mov byte [esi+0x15d], bl with bl = 0
	d = h.parse(std::string("MultiPlayerOnly = Yes\n") + kDefault + "End\n");
	REQUIRE_MESSAGE(d, h.error);
	CHECK(d->m_multiPlayerOnly); // row RW 0xBE1680, parseBool RW 0x42E558, offset 0x15D
	d = h.parse(std::string("MultiPlayerOnly = No\n") + kDefault + "End\n");
	REQUIRE_MESSAGE(d, h.error);
	CHECK_FALSE(d->m_multiPlayerOnly);
}

TEST_CASE("PORT-106 gate: the RW 0x4C72E1..0x4C730C truth table")
{
	Harness h;
	W3DModelDrawModuleData *mp = h.parse(std::string("MultiPlayerOnly = Yes\n") + kDefault + "End\n");
	REQUIRE_MESSAGE(mp, h.error);
	// not {1,5} (RW 0x4C72F8 je 0x4C78D3): the body is skipped
	CHECK(mp->multiPlayerOnlySkipsBody(false, false));
	// isInMultiplayerGame true and GlobalData +0xD45 == 0 (RW 0x4C72FA / 0x4C730C): the body runs
	CHECK_FALSE(mp->multiPlayerOnlySkipsBody(true, false));
	// +0xD45 != 0 (RW 0x4C730C jne 0x4C78D3): skipped whatever the mode says
	CHECK(mp->multiPlayerOnlySkipsBody(false, true));
	CHECK(mp->multiPlayerOnlySkipsBody(true, true));
	// adjacent: without the field neither input gates anything (RW 0x4C72EB je 0x4C7312)
	W3DModelDrawModuleData *plain = h.parse(std::string(kDefault) + "End\n");
	REQUIRE_MESSAGE(plain, h.error);
	CHECK_FALSE(plain->multiPlayerOnlySkipsBody(false, false));
	CHECK_FALSE(plain->multiPlayerOnlySkipsBody(false, true));
	CHECK_FALSE(plain->multiPlayerOnlySkipsBody(true, true));
}

TEST_CASE("PORT-106 body: a gated draw never runs the camera fade of the doDrawModule body")
{
	const char *kFade = "AlphaCameraFadeOuterRadius = 256\nAlphaCameraFadeInnerRadius = 64\nAlphaCameraAtInnerRadius = 25%\n";
	// The gate is set (skipped = true): the inner-distance fade that would drop the opacity to the at-inner
	// alpha (RW 0x4B53AB..0x4B53C7) must not run — the body was never entered (RW 0x4C72E1..0x4C730C).
	GateRig gated(std::string("MultiPlayerOnly = Yes\n") + kFade, true);
	gated.draw->updateCameraFade(Coord3D{ 0.0f, 0.0f, 1000.0f }, Coord3D{ 0.0f, 0.0f, 0.0f }, 4);
	CHECK(gated.draw->opacity() == 1.0f);
	CHECK_FALSE(gated.draw->hiddenByFade());
	CHECK(gated.draw->multiPlayerOnlySkipped());

	// adjacent: the same MultiPlayerOnly = Yes data with the gate off fades exactly as RW does — the INI field
	// alone changes nothing; only the wiring (Drawable::addEntry from the game mode) carries the gate.
	GateRig open(std::string("MultiPlayerOnly = Yes\n") + kFade, false);
	open.draw->updateCameraFade(Coord3D{ 0.0f, 0.0f, 1000.0f }, Coord3D{ 0.0f, 0.0f, 0.0f }, 4);
	CHECK(open.draw->opacity() == 0.25f); // at-inner 25% (RW 0x4B53AB..0x4B53C7, float at RW 0xBD1908)
	CHECK_FALSE(open.draw->multiPlayerOnlySkipped());
}
