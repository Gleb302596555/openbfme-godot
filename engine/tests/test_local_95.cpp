// LOCAL-95 (fork issue 760): W3DScriptedModelDraw and W3DHordeModelDraw uncited own functions.
// Every expectation is derived from RotWK 2.01 game.dat instructions (RW 0x... cited per test).

#include "doctest.h"

#include "Common/AsciiString.h"
#include "Common/GameEdition.h"
#include "Common/NameKeyGenerator.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DScriptedModelDraw.h"
#include "W3DDrawTestUtil.h"
#include "EnnorathScenario.h"
#include "GameClient/DrawableManager.h"
#include "W3dSynth.h"

#include <cctype>
#include <cmath>
#include <map>
#include <memory>
#include <string>

using namespace drawtest;
using namespace w3dsynth;

namespace
{
class FixedRandom : public W3DDrawRandom
{
public:
	int value(int lo, int) override { return lo; }
	float real(float lo, float) override { return lo; }
	std::vector<std::string> unverified() const override { return { "S-093: fixed test RNG" }; }
};

void buildSoldier(MemoryFileSource &fs, const std::map<std::string, std::pair<std::uint32_t, std::uint32_t>> &anims)
{
	fs.Add("art\\w3d\\l9\\l9unit_skl.w3d",
		hierarchyChunk("L9UNIT_SKL", { pivot("ROOTTRANSFORM", 0xFFFFFFFFu, 0, 0, 0), pivot("HIP", 0, 0, 0, 1) }));
	std::vector<std::uint8_t> bytes;
	SynthMesh body;
	body.Vertices = { { 0, 0, 0 }, { 1, 0, 0 }, { 1, 1, 0 }, { 0, 1, 0 } };
	body.Triangles = { 0, 1, 2, 0, 2, 3 };
	body.Container = "L9UNIT_SKN";
	body.Name = "BODY";
	append(bytes, meshChunk(body));
	append(bytes, hlodChunk("L9UNIT_SKN", "L9UNIT_SKL", { { "L9UNIT_SKN.BODY", 1 } }));
	fs.Add("art\\w3d\\l9\\l9unit_skn.w3d", bytes);

	for (const auto &kv : anims)
	{
		std::string lower = kv.first;
		for (char &c : lower)
		{
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		}
		fs.Add("art\\w3d\\l9\\" + lower + ".w3d",
			rawAnimChunk(kv.first.c_str(), "L9UNIT_SKL", kv.second.first, kv.second.second, { { ANIM_CHANNEL_Z, 1, 0, 1, { 0 } } }));
	}
}

struct Rig
{
	MemoryFileSource fs;
	std::unique_ptr<WW3DAssetManager> assets;
	std::unique_ptr<WW3DDrawAssets> drawAssets;
	Harness harness;
	FixedRandom random;
	W3DModelDrawModuleData *data = nullptr;

	Rig(const std::map<std::string, std::pair<std::uint32_t, std::uint32_t>> &anims, const std::string &iniBody)
	{
		buildSoldier(fs, anims);
		assets.reset(new WW3DAssetManager(fs));
		drawAssets.reset(new WW3DDrawAssets(*assets));
		data = harness.parse(iniBody, W3D_DRAW_SCRIPTED_MODEL);
		REQUIRE_MESSAGE(data != nullptr, harness.error);
	}
};

const std::map<std::string, std::pair<std::uint32_t, std::uint32_t>> kAnims = {
	{ "L9UNIT_IDLE", { 30u, 30u } },
	{ "L9UNIT_RUN",  { 20u, 20u } },
	{ "L9UNIT_ATK",  { 15u, 30u } },
};

const char *kBasicDrawIni = R"(
	AnimationsRequirePower = Yes
	DefaultModelConditionState
		Model = L9UNIT_SKN
		Skeleton = L9UNIT_SKL
	End
	IdleAnimationState
		StateName = STATE_Idle
		Animation = IDLA
			AnimationName = L9UNIT_IDLE
			AnimationMode = LOOP
			AnimationBlendTime = 5
		End
	End
	AnimationState = MOVING
		StateName = STATE_Moving
		Animation = RUNA
			AnimationName = L9UNIT_RUN
			AnimationMode = LOOP
			AnimationBlendTime = 4
		End
	End
	AnimationState = FIRING_OR_PREATTACK_A
		StateName = STATE_Attack
		Animation = ATKA
			AnimationName = L9UNIT_ATK
			AnimationMode = ONCE
			AnimationBlendTime = 2
		End
	End
End
)";
} // namespace

TEST_CASE("LOCAL-95: setPauseAnimation (RW 0x004B304E) and RotWK-only pause gate (RW 0x004BF608..0x004BF630)")
{
	Rig rig(kAnims, kBasicDrawIni);

	bool allowAnimate = true;
	bool sawRequirePowerArg = false;
	W3DScriptedModelDraw::Options opt;
	opt.buildBones = false;
	opt.edition = GameEdition::RotWK201;
	opt.shouldAnimate = [&](bool reqPower) {
		sawRequirePowerArg = reqPower;
		return allowAnimate;
	};

	W3DScriptedModelDraw draw(*rig.data, *rig.drawAssets, rig.random, nullptr, opt);
	CHECK_FALSE(draw.isAnimationPaused());
	CHECK(draw.track(0).mode == W3D_ANIM_MODE_LOOP);

	draw.advance(100.0); // 30 fps * 0.1s = 3.0 frames
	CHECK(sawRequirePowerArg);
	CHECK(draw.track(0).frame == doctest::Approx(3.0f)); // tolerance: float 0.001f ms-to-sec product
	CHECK_FALSE(draw.isAnimationPaused());

	allowAnimate = false;
	draw.advance(200.0);
	CHECK(draw.isAnimationPaused());
	CHECK(draw.track(0).mode == W3D_ANIM_MODE_MANUAL);
	CHECK(draw.track(0).frame == doctest::Approx(3.0f)); // tolerance: float 0.001f ms-to-sec product
	CHECK(draw.needsAdvance());

	allowAnimate = true;
	draw.advance(100.0);
	CHECK_FALSE(draw.isAnimationPaused());
	CHECK(draw.track(0).mode == W3D_ANIM_MODE_LOOP);
	CHECK(draw.track(0).frame == doctest::Approx(6.0f)); // tolerance: float 0.001f ms-to-sec product

	// BFME2 1.06 edition (BFME2 0x004BFDFE): no pause return in advanceAnimation.
	W3DScriptedModelDraw::Options bfme2Opt;
	bfme2Opt.buildBones = false;
	bfme2Opt.edition = GameEdition::BFME2_106;
	bfme2Opt.shouldAnimate = [](bool) { return false; };
	W3DScriptedModelDraw bfme2Draw(*rig.data, *rig.drawAssets, rig.random, nullptr, bfme2Opt);
	bfme2Draw.advance(100.0);
	CHECK_FALSE(bfme2Draw.isAnimationPaused());
	CHECK(bfme2Draw.track(0).frame == doctest::Approx(3.0f)); // tolerance: float 0.001f ms-to-sec product
}

TEST_CASE("LOCAL-95: setAnimationLoopDuration (RW 0x004B300D), setAnimationCompletionTime (RW 0x004B3049) and skipToEndOfAnimation (RW 0x004B3350)")
{
	Rig rig(kAnims, kBasicDrawIni);
	W3DScriptedModelDraw::Options opt;
	opt.buildBones = false;
	W3DScriptedModelDraw draw(*rig.data, *rig.drawAssets, rig.random, nullptr, opt);

	// L9UNIT_IDLE is 30 frames @ 30 fps => naturalMs = 1000.0 ms.
	// 10 logic frames * 200 ms = 2000.0 ms => speedSync = 0.5 (RW 0x004B300D -> 0x004B2808).
	CHECK(draw.setAnimationLoopDuration(10));
	CHECK(draw.speedMultiplier() == 0.5f);

	// setAnimationCompletionTime (RW 0x004B3049): 2 logic frames = 400 ms => speedSync = 2.5.
	CHECK(draw.setAnimationCompletionTime(2));
	CHECK(draw.speedMultiplier() == 2.5f);

	CHECK_FALSE(draw.setAnimationLoopDuration(0));
	CHECK(draw.speedMultiplier() == 2.5f);

	draw.advance(100.0);
	const float beforeSkip = draw.track(0).frame;
	draw.skipToEndOfAnimation();
	CHECK(draw.track(0).prevFrame == beforeSkip);
	CHECK(draw.track(0).frame == 29.0f);
}

TEST_CASE("LOCAL-95: showNextAnimationState (RW 0x004BF501) and showPreviousAnimationState (RW 0x004BF4D9)")
{
	Rig rig(kAnims, kBasicDrawIni);
	W3DScriptedModelDraw::Options opt;
	opt.buildBones = false;
	W3DScriptedModelDraw draw(*rig.data, *rig.drawAssets, rig.random, nullptr, opt);

	const auto &states = rig.data->m_animationStates;
	REQUIRE(states.size() == 4);
	CHECK_FALSE(draw.sequentialAnimation());
	REQUIRE(draw.currentAnimationState() == &states[0]);

	// At index 0: showPreviousAnimationState does nothing and keeps m_sequentialAnim false.
	draw.showPreviousAnimationState();
	CHECK_FALSE(draw.sequentialAnimation());
	CHECK(draw.currentAnimationState() == &states[0]);

	// Step forward through every state in m_animationStates order (RW 0x004B4514).
	for (size_t i = 1; i < states.size(); ++i)
	{
		draw.showNextAnimationState();
		CHECK(draw.sequentialAnimation());
		CHECK(draw.currentAnimationState() == &states[i]);
	}

	// At the last state: showNextAnimationState stays on the last state.
	draw.showNextAnimationState();
	CHECK(draw.currentAnimationState() == &states.back());

	// Step backward one state (RW 0x004B44F3).
	draw.showPreviousAnimationState();
	CHECK(draw.currentAnimationState() == &states[states.size() - 2]);
}

TEST_CASE("LOCAL-95: playing animation queries (RW 0x004C0B38 / 0x004C0B88 / 0x004C0BC0) and getAnimationFrameCount (RW 0x004BF0B5)")
{
	Rig rig(kAnims, kBasicDrawIni);
	W3DScriptedModelDraw::Options opt;
	opt.buildBones = false;
	W3DScriptedModelDraw draw(*rig.data, *rig.drawAssets, rig.random, nullptr, opt);

	CHECK(AsciiStringUtil::compareNoCase(draw.getPlayingAnimationName(0), "L9UNIT_SKL.L9UNIT_IDLE") == 0);
	CHECK(draw.getPlayingAnimationName(1).empty());
	CHECK(draw.getPlayingAnimationName(-1).empty());
	CHECK(draw.getPlayingAnimationName(3).empty());

	NameKeyGenerator keys;
	keys.init();
	const NameKeyType key0 = draw.getPlayingAnimationNameKey(0, keys);
	CHECK(key0 != NAMEKEY_INVALID);
	CHECK(key0 == keys.nameToKey(draw.getPlayingAnimationName(0)));
	CHECK(draw.getPlayingAnimationNameKey(2, keys) == NAMEKEY_INVALID);

	std::string animInfo, stateInfo;
	draw.buildPlayingAnimationStringInfo(0, &animInfo, &stateInfo);
	CHECK(animInfo == draw.getPlayingAnimationName(0) + " 1/30 frames(1.0s), AnimMode:LOOP, BlendTime:5");
	CHECK(stateInfo == "(ModelState and AnimState info not available in release)");

	int frames = -1;
	CHECK(draw.getAnimationFrameCount("L9UNIT_RUN", &frames));
	CHECK(frames == 20);
	CHECK(draw.getAnimationFrameCount("L9UNIT_ATK", &frames));
	CHECK(frames == 15);
	CHECK_FALSE(draw.getAnimationFrameCount("NO_SUCH_CLIP", &frames));
	CHECK_FALSE(draw.getAnimationFrameCount("L9UNIT_RUN", nullptr));
}

TEST_CASE("LOCAL-95: getPortraitImage (RW 0x004B6789) and getButtonImage (RW 0x004B9460)")
{
	const char *ini = R"(
		DefaultModelConditionState
			Model = L9UNIT_SKN
			Skeleton = L9UNIT_SKL
		End
		ModelConditionState = USER_1
			Model = L9UNIT_SKN
			Skeleton = L9UNIT_SKL
			ButtonImageName = UPButton_NoPortrait
		End
		ModelConditionState = USER_2
			Model = L9UNIT_SKN
			Skeleton = L9UNIT_SKL
			PortraitImageName = UPPortrait_Alt
			ButtonImageName = UPButton_Alt
		End
		IdleAnimationState
			StateName = STATE_Idle
			Animation = IDLA
				AnimationName = L9UNIT_IDLE
				AnimationMode = LOOP
			End
		End
	End
	)";

	Rig rig(kAnims, ini);
	W3DScriptedModelDraw::Options opt;
	opt.buildBones = false;
	W3DScriptedModelDraw draw(*rig.data, *rig.drawAssets, rig.random, nullptr, opt);

	CHECK(draw.portraitImageName().empty());
	CHECK(draw.buttonImageName().empty());

	// USER_1 has ButtonImageName but empty PortraitImageName: RW 0x004B948A..0x004B94AA clears button image.
	draw.setModelConditionFlags(flagsOf({ "USER_1" }));
	CHECK(draw.portraitImageName().empty());
	CHECK(draw.buttonImageName().empty());

	// USER_2 has both PortraitImageName and ButtonImageName: both refresh.
	draw.setModelConditionFlags(flagsOf({ "USER_2" }));
	CHECK(draw.portraitImageName() == "UPPortrait_Alt");
	CHECK(draw.buttonImageName() == "UPButton_Alt");
}

TEST_CASE("LOCAL-95: shadows/hidden (RW 0x004B26F9 / 0x004B26E4 / 0x004B26E0), mesh overloads (RW 0x004B5809 / 0x004B5814 / 0x004B582B) and decals (RW 0x004B94BC / 0x004B95AD / 0x004B2A1B / 0x004B2A3B)")
{
	const char *ini = R"(
		DefaultModelConditionState
			Model = L9UNIT_SKN
			Skeleton = L9UNIT_SKL
			Shadow = SHADOW_DECAL
		End
		IdleAnimationState
			StateName = STATE_Idle
			Animation = IDLA
				AnimationName = L9UNIT_IDLE
				AnimationMode = LOOP
			End
		End
	End
	)";

	Rig rig(kAnims, ini);
	W3DScriptedModelDraw::Options opt;
	opt.buildBones = false;
	W3DScriptedModelDraw draw(*rig.data, *rig.drawAssets, rig.random, nullptr, opt);

	// Shadows enabled by default when model state has Shadow (RW 0x004B26F9).
	CHECK(draw.hasShadow());
	CHECK(draw.shadowsEnabled());
	CHECK(draw.shadowRenderEnabled());

	// Shroud obscures shadow render even while shadowsEnabled() stays true (RW 0x004B270B).
	draw.setFullyObscuredByShroud(true);
	CHECK(draw.shadowsEnabled());
	CHECK_FALSE(draw.shadowRenderEnabled());
	draw.setShadowsEnabled(true);
	CHECK_FALSE(draw.shadowRenderEnabled());
	draw.setFullyObscuredByShroud(false);
	CHECK(draw.shadowRenderEnabled());

	draw.setShadowsEnabled(false);
	CHECK_FALSE(draw.shadowsEnabled());
	CHECK_FALSE(draw.shadowRenderEnabled());

	// releaseShadows (RW 0x004B26E4) drops the active shadow.
	draw.releaseShadows();
	CHECK_FALSE(draw.hasShadow());
	CHECK_FALSE(draw.shadowRenderEnabled());

	// isHidden (RW 0x004B26E0).
	CHECK_FALSE(draw.isHidden());
	draw.setHidden(true);
	CHECK(draw.isHidden());

	// Wall/ramp mesh overloads (RW 0x004B5809 / 0x004B5814 / 0x004B582B).
	CHECK_FALSE(draw.hasWallBoundsMeshOverload());
	draw.setWallBoundsMeshOverload("WALL_P1");
	CHECK(draw.hasWallBoundsMeshOverload());
	CHECK(draw.wallBoundsMeshOverload() == "WALL_P1");
	draw.setRampMeshOverload("RAMP_L", 0);
	draw.setRampMeshOverload("RAMP_R", 1);
	draw.setRampMeshOverload("IGNORED", 2);
	CHECK(draw.rampMeshOverload(0) == "RAMP_L");
	CHECK(draw.rampMeshOverload(1) == "RAMP_R");
	CHECK(draw.rampMeshOverload(2).empty());

	// Terrain decals (RW 0x004B94BC / 0x004B95AD / 0x004B2A1B / 0x004B2A3B).
	draw.setTerrainDecal(1, 10.0f, 12.0f, 0.0f, 0.0f);
	REQUIRE(draw.terrainDecal() != nullptr);
	CHECK(draw.terrainDecal()->texture == "EXHorde");
	draw.setTerrainDecalSize(20.0f, 24.0f);
	CHECK(draw.terrainDecal()->sizeX == 20.0f);
	CHECK(draw.terrainDecal()->sizeY == 24.0f);
	draw.setTerrainDecalOpacity(0.5f);
	CHECK(draw.terrainDecal()->opacity == 127);
	draw.setTerrainDecalTexture("CustomDecal", 5.0f, 5.0f, 0.0f, 0.0f);
	REQUIRE(draw.terrainDecal() != nullptr);
	CHECK(draw.terrainDecal()->texture == "CustomDecal");
	draw.setTerrainDecal(7, 1.0f, 1.0f, 0.0f, 0.0f);
	CHECK(draw.terrainDecal() == nullptr);
}

TEST_CASE("LOCAL-95 ENNORATH scenario W3DScriptedModelDraw and W3DHordeModelDraw: real mod unit and horde member through drawable harness")
{
	ennorathscenario::World world;
	if (!world.load("LOCAL-95 ENNORATH scenario W3DScriptedModelDraw"))
	{
		return;
	}
	hudtest::Rig game(world.shared);
	std::string error;
	Object *fighter = game.game->createObject("GondorFighter", game.index(), Coord3D{ 1200.0f, 1200.0f, 0.0f }, 0.0f, &error);
	REQUIRE_MESSAGE(fighter != nullptr, error);
	Object *hero = game.game->createObject("GondorAragorn", game.index(), Coord3D{ 1300.0f, 1200.0f, 0.0f }, 0.0f, &error);
	REQUIRE_MESSAGE(hero != nullptr, error);

	game.logic().runLogicFrame();
	game.game->refreshClient(1000.0 / 30.0, 0.0);

	auto findPrimaryDraw = [&](Object *obj) -> W3DScriptedModelDraw * {
		Drawable *d = game.game->drawables().findByObject(obj->getID());
		if (!d) return nullptr;
		for (auto &e : d->entries())
		{
			if (e.draw) return e.draw.get();
		}
		return nullptr;
	};

	W3DScriptedModelDraw *fighterDraw = findPrimaryDraw(fighter);
	W3DScriptedModelDraw *heroDraw = findPrimaryDraw(hero);
	REQUIRE(fighterDraw != nullptr);
	REQUIRE(heroDraw != nullptr);
	CHECK(dynamic_cast<W3DHordeModelDraw *>(fighterDraw) != nullptr);

	// Track 0 queries & pause/skip work on real Ennorath GondorFighter (W3DHordeModelDraw) and GondorAragorn (W3DScriptedModelDraw).
	CHECK_FALSE(fighterDraw->getPlayingAnimationName(0).empty());
	CHECK_FALSE(heroDraw->getPlayingAnimationName(0).empty());

	std::string animStr, stateStr;
	heroDraw->buildPlayingAnimationStringInfo(0, &animStr, &stateStr);
	CHECK(animStr.find("frames(") != std::string::npos);
	CHECK(stateStr == "(ModelState and AnimState info not available in release)");

	// Pause freezes track 0 frame on real Ennorath hero drawable (RW 0x004B304E / RW 0x004BF62A).
	const float frameBefore = heroDraw->track(0).frame;
	heroDraw->setPauseAnimation(true);
	heroDraw->advance(100.0);
	CHECK(heroDraw->isAnimationPaused());
	CHECK(heroDraw->track(0).frame == frameBefore);
	heroDraw->setPauseAnimation(false);
	CHECK_FALSE(heroDraw->isAnimationPaused());
}
