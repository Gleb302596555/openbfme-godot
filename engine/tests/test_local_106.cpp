// LOCAL-106 (fork issue 784): W3DScriptedModelDraw and W3DHordeModelDraw second pass + Drawable::getShouldAnimate wiring.
// Every expectation is derived from RotWK 2.01 game.dat instructions (RW 0x... cited per test) and Ennorath 1.9.8.3 INI data.

#include "doctest.h"

#include "Common/AsciiString.h"
#include "Common/GameEdition.h"
#include "Common/ModelState.h"
#include "Common/NameKeyGenerator.h"
#include "EnnorathScenario.h"
#include "GameClient/Drawable.h"
#include "GameClient/DrawableManager.h"
#include "GameClient/LogicSnapshot.h"
#include "GameClient/RadiusCursor.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DScriptedModelDraw.h"
#include "W3DDrawTestUtil.h"
#include "W3dSynth.h"

#include <cctype>
#include <cstring>
#include <map>
#include <memory>
#include <string>

using namespace drawtest;
using namespace w3dsynth;

namespace
{
class FixedRandom106 : public W3DDrawRandom
{
public:
	int value(int lo, int) override { return lo; }
	float real(float lo, float) override { return lo; }
	std::vector<std::string> unverified() const override { return { "S-093: fixed test RNG" }; }
};

void buildSyntheticModel(MemoryFileSource &fs, const std::map<std::string, std::pair<std::uint32_t, std::uint32_t>> &anims)
{
	fs.Add("art\\w3d\\l106\\l106bldg_skl.w3d",
		hierarchyChunk("L106BLDG_SKL", { pivot("ROOTTRANSFORM", 0xFFFFFFFFu, 0, 0, 0), pivot("BODY", 0, 0, 0, 1) }));
	std::vector<std::uint8_t> bytes;
	SynthMesh body;
	body.Vertices = { { 0, 0, 0 }, { 1, 0, 0 }, { 1, 1, 0 }, { 0, 1, 0 } };
	body.Triangles = { 0, 1, 2, 0, 2, 3 };
	body.Container = "L106BLDG_SKN";
	body.Name = "BODY";
	append(bytes, meshChunk(body));
	append(bytes, hlodChunk("L106BLDG_SKN", "L106BLDG_SKL", { { "L106BLDG_SKN.BODY", 1 } }));
	fs.Add("art\\w3d\\l106\\l106bldg_skn.w3d", bytes);

	for (const auto &kv : anims)
	{
		std::string lower = kv.first;
		for (char &c : lower)
		{
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		}
		fs.Add("art\\w3d\\l106\\" + lower + ".w3d",
			rawAnimChunk(kv.first.c_str(), "L106BLDG_SKL", kv.second.first, kv.second.second, { { ANIM_CHANNEL_Z, 1, 0, 1, { 0 } } }));
	}
}

struct DrawRig106
{
	MemoryFileSource fs;
	std::unique_ptr<WW3DAssetManager> assets;
	std::unique_ptr<WW3DDrawAssets> drawAssets;
	Harness harness;
	FixedRandom106 random;
	W3DModelDrawModuleData *data = nullptr;

	DrawRig106(const std::map<std::string, std::pair<std::uint32_t, std::uint32_t>> &anims, const std::string &iniBody)
	{
		buildSyntheticModel(fs, anims);
		assets.reset(new WW3DAssetManager(fs));
		drawAssets.reset(new WW3DDrawAssets(*assets));
		data = harness.parse(iniBody, W3D_DRAW_SCRIPTED_MODEL);
		REQUIRE_MESSAGE(data != nullptr, harness.error);
	}
};

int findModelConditionIndex(const char *name)
{
	for (int i = 0; TheModelConditionNames[i] != nullptr; ++i)
	{
		if (std::strcmp(TheModelConditionNames[i], name) == 0)
		{
			return i;
		}
	}
	return -1;
}
} // namespace

TEST_CASE("LOCAL-106: Drawable::getShouldAnimate (RW 0x00675655) bit checks and power/freeze/EMP gating")
{
	// Expectations from RotWK 2.01 RW 0x00675655..0x0067569E:
	// - No object (INVALID_ID): always true (0x00675655..0x0067565E)
	// - animationsRequirePower && (Object+0x457 & 0x02): false (0x00675660..0x0067566E)
	// - m_disabled == 0: true (0x00675670..0x0067567F)
	// - DISABLED_USER_FROZEN (bit 11, 0x800): false regardless of animationsRequirePower (0x00675681..0x00675686)
	// - DISABLED_EMP (bit 2, 0x04) | DISABLED_UNMANNED (bit 5, 0x20): false regardless of animationsRequirePower (0x00675688..0x0067568A)
	// - animationsRequirePower && DISABLED_UNDERPOWERED (bit 6, 0x40): false (0x0067568C..0x00675695)
	// - Other disabled bits (e.g. DISABLED_HELD bit 3, 0x08): true (0x0067569B)
	ennorathscenario::World world;
	if (!world.load("LOCAL-106: Drawable::getShouldAnimate bit checks"))
	{
		return;
	}
	hudtest::Rig game(world.shared);
	std::string error;
	Object *farm = game.game->createObject("GondorFarm", game.index(), Coord3D{ 1200.0f, 1200.0f, 0.0f }, 0.0f, &error);
	REQUIRE_MESSAGE(farm != nullptr, error);

	game.logic().runLogicFrame();
	game.game->refreshClient(1000.0 / 30.0, 0.0);
	Drawable *d = game.game->drawables().findByObject(farm->getID());
	REQUIRE(d != nullptr);

	// Undisabled -> true for both animationsRequirePower=true and false.
	d->setDisabledState(DISABLEDMASK_NONE, 0);
	CHECK(d->getShouldAnimate(true));
	CHECK(d->getShouldAnimate(false));

	// Object +0x457 bit 1 (SCRIPT_UNPOWERED): gates only when animationsRequirePower=true.
	d->setDisabledState(DISABLEDMASK_NONE, 0x02u);
	CHECK_FALSE(d->getShouldAnimate(true));
	CHECK(d->getShouldAnimate(false));

	// DISABLED_UNDERPOWERED (bit 6, 0x40): gates only when animationsRequirePower=true.
	d->setDisabledState(1u << DISABLED_UNDERPOWERED, 0);
	CHECK_FALSE(d->getShouldAnimate(true));
	CHECK(d->getShouldAnimate(false));

	// DISABLED_USER_FROZEN (bit 11, 0x800): gates regardless of animationsRequirePower.
	d->setDisabledState(1u << DISABLED_USER_FROZEN, 0);
	CHECK_FALSE(d->getShouldAnimate(true));
	CHECK_FALSE(d->getShouldAnimate(false));

	// DISABLED_EMP (bit 2, 0x04) and DISABLED_UNMANNED (bit 5, 0x20): gate regardless of animationsRequirePower.
	d->setDisabledState(1u << DISABLED_EMP, 0);
	CHECK_FALSE(d->getShouldAnimate(true));
	CHECK_FALSE(d->getShouldAnimate(false));

	d->setDisabledState(1u << DISABLED_UNMANNED, 0);
	CHECK_FALSE(d->getShouldAnimate(true));
	CHECK_FALSE(d->getShouldAnimate(false));

	// DISABLED_HELD (bit 3, 0x08): NOT in 0x800/0x24/0x40 -> stays animated.
	d->setDisabledState(1u << DISABLED_HELD, 0);
	CHECK(d->getShouldAnimate(true));
	CHECK(d->getShouldAnimate(false));
}

TEST_CASE("LOCAL-106: W3DScriptedModelDraw own methods — hide override, EmbedPortal, sub-object overrides, RadiusDecal")
{
	// Expectations from RotWK 2.01:
	// - RW 0x007B2EFE / RW 0x004B3752 / DONOR W3DModelDraw_setHidden.cpp: +0x4D blocks setHidden(false)
	// - RW 0x0047836C -> RW 0x00464A02: allowsUnhideWhileInHorde (+0x15E)
	// - RW 0x004B7A22 -> RW 0x0088FCBB: getEmbedPortal bounds check and copy (+0x11C)
	// - RW 0x004B86BA / RW 0x004B30DC / RW 0x004BC171 / RW 0x004BB86C: sub-object override +0xA0/+0xA4/+0xA8/+0xB4
	// - RW 0x004B2A5D..0x004B2C0D -> RW 0x0073266B..0x00732CBB: RadiusDecal template/create/position/offset/opacity/color/clear
	const std::string ini = R"(
		EmbedPortal = PORTAL_A ramp
		EmbedPortal = PORTAL_B ladder
		DefaultModelConditionState
			Model = L106BLDG_SKN
			Skeleton = L106BLDG_SKL
		End
		IdleAnimationState
			Animation = IDLE
				AnimationName = L106BLDG_IDLE
				AnimationMode = LOOP
			End
		End
	End
	)";
	DrawRig106 rig({ { "L106BLDG_IDLE", { 10u, 30u } } }, ini);
	W3DScriptedModelDraw draw(*rig.data, *rig.drawAssets, rig.random);

	// RW 0x007B2EFE + RW 0x004B3752 / DONOR W3DModelDraw_setHidden.cpp: hide override blocks unhide.
	draw.setHidden(true);
	CHECK(draw.isHidden());
	draw.setHideOverride(true);
	CHECK(draw.isHideOverride());
	draw.setHidden(false);
	CHECK(draw.isHidden()); // blocked by +0x4D
	draw.setHideOverride(false);
	draw.setHidden(false);
	CHECK_FALSE(draw.isHidden());

	// RW 0x0047836C -> RW 0x00464A02: default false.
	CHECK_FALSE(draw.allowsUnhideWhileInHorde());

	// RW 0x004B7A22 -> RW 0x0088FCBB: EmbedPortal lookup (+0x11C).
	W3DModelDrawModuleData::W3DEmbedPortalInfo portal;
	CHECK_FALSE(draw.getEmbedPortal(-1, portal));
	CHECK(draw.getEmbedPortal(0, portal));
	CHECK(portal.name == "portal_a");
	CHECK(portal.kind == "ramp");
	CHECK(draw.getEmbedPortal(1, portal));
	CHECK(portal.name == "portal_b");
	CHECK(portal.kind == "ladder");
	CHECK_FALSE(draw.getEmbedPortal(2, portal));

	// RW 0x004B86BA / 0x004B30DC / 0x004BC171 / 0x004BB86C: sub-object override state.
	draw.setSubObjectOverride(7, "SHIELD_01");
	CHECK(draw.subObjectOverrideValue() == 7);
	CHECK_FALSE(draw.isSubObjectOverrideDirty());
	CHECK(draw.subObjectOverrideName() == "SHIELD_01");
	draw.markSubObjectOverrideDirty();
	CHECK(draw.isSubObjectOverrideDirty());
	draw.insertTrackedSubObjectKey(42);
	draw.insertTrackedSubObjectKey(99);
	CHECK(draw.trackedSubObjectKeys().count(42) == 1);
	CHECK(draw.trackedSubObjectKeys().count(99) == 1);
	draw.eraseTrackedSubObjectKey(42);
	CHECK(draw.trackedSubObjectKeys().count(42) == 0);
	CHECK(draw.trackedSubObjectKeys().count(99) == 1);

	// RW 0x004B2A5D..0x004B2C0D -> RW 0x0073266B..0x00732CBB: RadiusDecal.
	RadiusDecalTemplate tmpl;
	tmpl.texture = "SCCSelection";
	tmpl.opacityMin = 0.25f;
	tmpl.opacityMax = 0.85f;
	draw.setSelectionDecalTemplate(&tmpl);
	CHECK(draw.selectionDecalTemplate().texture == "SCCSelection");

	// Without player -> createSelectionDecal returns false (RW 0x00732CDC).
	CHECK_FALSE(draw.createSelectionDecal(Coord3D{ 10.0f, 20.0f, 0.0f }, 15.0f, false));
	CHECK_FALSE(draw.selectionRadiusDecal().active);

	// With player -> creates active selection decal.
	CHECK(draw.createSelectionDecal(Coord3D{ 10.0f, 20.0f, 0.0f }, 15.0f, true));
	CHECK(draw.selectionRadiusDecal().active);
	CHECK(draw.selectionRadiusDecal().radius == 15.0f);
	CHECK(draw.selectionRadiusDecal().opacity == 0.85f);

	Coord3D newPos{ 30.0f, 40.0f, 5.0f };
	Coord3D newOff{ 1.0f, 2.0f, 3.0f };
	draw.setSelectionDecalPosition(&newPos);
	draw.setSelectionDecalOffset(&newOff);
	draw.setSelectionDecalOpacity(0.5f);
	draw.setSelectionDecalColor(0xFF00FF00u);
	CHECK(draw.selectionRadiusDecal().position.x == 30.0f);
	CHECK(draw.selectionRadiusDecal().offset.y == 2.0f);
	CHECK(draw.selectionRadiusDecal().opacity == 0.5f);
	CHECK(draw.selectionRadiusDecal().color == 0xFF00FF00u);

	draw.clearSelectionDecal();
	CHECK_FALSE(draw.selectionRadiusDecal().active);

	// Terrain radius decal clear/radius no-op when inactive (RW 0x004B2A5D / 0x004B2B36).
	draw.setTerrainDecalRadius(25.0f);
	CHECK_FALSE(draw.terrainRadiusDecal().active);
	draw.clearTerrainDecal();
	CHECK_FALSE(draw.terrainRadiusDecal().active);
}

TEST_CASE("LOCAL-106 ENNORATH scenario: building animation pause on disable, horde draw, and unit model condition")
{
	// Expectations:
	// - RotWK 2.01 RW 0x004BF615 -> RW 0x00675655 -> RW 0x004B304E: GondorFarm (Ennorath INI, AnimationsRequirePower default Yes)
	//   pauses its W3DScriptedModelDraw animation when DISABLED_UNDERPOWERED or DISABLED_USER_FROZEN is set on the Object
	//   and resumes automatically once the disable is cleared.
	// - GondorFighter uses W3DHordeModelDraw (RW 0x004789BC / 0x004789E6 / 0x00478A15).
	// - GondorAragorn changes animation state when MOVING model condition is set.
	ennorathscenario::World world;
	if (!world.load("LOCAL-106 ENNORATH scenario"))
	{
		return;
	}
	hudtest::Rig game(world.shared);
	std::string error;

	Object *farm = game.game->createObject("GondorFarm", game.index(), Coord3D{ 1200.0f, 1200.0f, 0.0f }, 0.0f, &error);
	REQUIRE_MESSAGE(farm != nullptr, error);
	Object *fighter = game.game->createObject("GondorFighter", game.index(), Coord3D{ 1300.0f, 1200.0f, 0.0f }, 0.0f, &error);
	REQUIRE_MESSAGE(fighter != nullptr, error);
	Object *hero = game.game->createObject("GondorAragorn", game.index(), Coord3D{ 1400.0f, 1200.0f, 0.0f }, 0.0f, &error);
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

	W3DScriptedModelDraw *farmDraw = findPrimaryDraw(farm);
	W3DScriptedModelDraw *fighterDraw = findPrimaryDraw(fighter);
	W3DScriptedModelDraw *heroDraw = findPrimaryDraw(hero);
	REQUIRE(farmDraw != nullptr);
	REQUIRE(fighterDraw != nullptr);
	REQUIRE(heroDraw != nullptr);

	// 1. Building animation pause wired through Object -> LogicSnapshot -> Drawable::getShouldAnimate -> W3DScriptedModelDraw.
	CHECK_FALSE(farmDraw->isAnimationPaused());

	farm->setDisabled(DISABLED_UNDERPOWERED, UPDATE_SLEEP_FOREVER);
	game.logic().runLogicFrame();
	game.game->refreshClient(1000.0 / 30.0, 0.0); // syncs LogicSnapshot disabledMask onto Drawable
	game.game->refreshClient(1000.0 / 30.0, 0.0); // advances W3DScriptedModelDraw -> pauses via getShouldAnimate
	CHECK(farmDraw->isAnimationPaused());
	const float pausedFrame = farmDraw->track(0).frame;
	game.game->refreshClient(100.0, 0.0);
	CHECK(farmDraw->isAnimationPaused());
	CHECK(farmDraw->track(0).frame == pausedFrame);

	farm->clearDisabled(DISABLED_UNDERPOWERED);
	game.logic().runLogicFrame();
	game.game->refreshClient(1000.0 / 30.0, 0.0);
	game.game->refreshClient(1000.0 / 30.0, 0.0);
	CHECK_FALSE(farmDraw->isAnimationPaused());

	// Also verify DISABLED_USER_FROZEN pauses the building animation.
	farm->setDisabled(DISABLED_USER_FROZEN, UPDATE_SLEEP_FOREVER);
	game.logic().runLogicFrame();
	game.game->refreshClient(1000.0 / 30.0, 0.0);
	game.game->refreshClient(1000.0 / 30.0, 0.0);
	CHECK(farmDraw->isAnimationPaused());
	farm->clearDisabled(DISABLED_USER_FROZEN);
	game.logic().runLogicFrame();
	game.game->refreshClient(1000.0 / 30.0, 0.0);
	game.game->refreshClient(1000.0 / 30.0, 0.0);
	CHECK_FALSE(farmDraw->isAnimationPaused());

	// 2. Horde member draw is W3DHordeModelDraw with active track 0.
	CHECK(dynamic_cast<W3DHordeModelDraw *>(fighterDraw) != nullptr);
	CHECK_FALSE(fighterDraw->getPlayingAnimationName(0).empty());

	// 3. Unit animation changes when DYING model condition is set on GondorAragorn.
	const std::string idleAnim = heroDraw->getPlayingAnimationName(0);
	CHECK_FALSE(idleAnim.empty());
	const int kDying = findModelConditionIndex("DYING");
	REQUIRE(kDying != -1);
	hero->setModelConditionState(kDying, true);
	game.logic().runLogicFrame();
	game.game->refreshClient(500.0, 0.0); // complete blend into DYING animation
	game.game->refreshClient(500.0, 0.0);
	const std::string dyingAnim = heroDraw->getPlayingAnimationName(0);
	CHECK_FALSE(dyingAnim.empty());
	CHECK(dyingAnim != idleAnim);
}
