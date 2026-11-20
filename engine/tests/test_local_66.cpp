// OpenBFME unit and Ennorath scenario tests for LOCAL-66 (fork issue 511):
// ParticleSystemFXNugget OnlyIfOnLand / OnlyIfOnWater (RW 0x005E1270) and
// W3DModelDraw ParticleSysBone OnlyIfOnWater / OnlyIfOnLand (RW 0x004C6514) via LiveFX::isWater (RW 0x67DAE6).

#include "doctest.h"
#include "EnnorathScenario.h"
#include "FxTestUtil.h"
#include "HudTestUtil.h"
#include "W3DDrawTestUtil.h"

#include "GameClient/Drawable.h"
#include "GameClient/DrawableManager.h"
#include "GameClient/FXList.h"
#include "GameClient/FXPlayback.h"
#include "GameClient/LiveFX.h"
#include "GameClient/ParticleSys.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DModelDraw.h"
#include "GameLogic/FXEvents.h"
#include "GameLogic/Map/TerrainPathfindSource.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Object/Object.h"

#include <memory>
#include <string>
#include <vector>

namespace
{
class MockFXObject : public FXObject
{
public:
	MockFXObject(Coord3D pos, int layerValue) : m_pos(pos), m_layer(layerValue) {}
	std::uint32_t objectId() const override { return 1; }
	Coord3D position() const override { return m_pos; }
	Matrix3D transform() const override { return Matrix3D(); }
	ModelConditionFlags modelConditions() const override { return ModelConditionFlags(); }
	bool passesObjectFilter(const ObjectFilter &) const override { return true; }
	bool hasDrawable() const override { return true; }
	float boundingCircleRadius() const override { return 10.0f; }
	int controllingPlayerIndex() const override { return 0; }
	bool isControlledByLocalPlayer() const override { return true; }
	int relationshipToLocalPlayer() const override { return 2; }
	bool hasKindOf(const char *) const override { return false; }
	bool isHorde() const override { return false; }
	int layer() const override { return m_layer; }

private:
	Coord3D m_pos;
	int m_layer = 1;
};

class MockWaterWorld : public FXPlaybackWorld
{
public:
	bool waterAnswer = false;

	std::uint32_t logicFrame() const override { return 100; }
	float groundHeight(float, float) override { return 0.0f; }
	bool isWater(float, float) override { return waterAnswer; }
	int shroudStatusAt(const Coord3D &) override { return 0; }
	bool objectShroudedForLocalPlayer(const FXObject &) override { return false; }
	FXParticleSystem::ParticleAttachInfo attachedDrawable(std::uint32_t, const std::string &) override { return {}; }
	FXParticleSystem::ParticleAttachInfo attachedObject(std::uint32_t, const std::string &, int) override { return {}; }
	bool boneWorldMatrix(const FXObject &, const std::string &, Matrix3D &) override { return false; }
	std::vector<FXBoneTransform> boneWorldTransforms(const FXObject &, const std::string &, int, int) override { return {}; }
	bool playSound(const std::string &, const Coord3D *, int) override { return true; }
	bool turnOnBuff(const FXObject &, int, const std::string &, int) override { return false; }
	bool turnOffBuff(const FXObject &, int) override { return false; }
	bool tintDrawable(const FXObject &, const RGBColor &, unsigned, unsigned, unsigned, float, float) override { return false; }
	bool viewShake(const Coord3D &, int, float) override { return false; }
	bool createDecal(const FXDecalDesc &) override { return false; }
	bool addScorch(const Coord3D &, float, int) override { return false; }
	int mapWeather() const override { return 0; }
};

int countLiveSystems(const FXParticleSystem::ParticleSystemManager &mgr, const std::string &templateName)
{
	const auto *tmpl = mgr.templates().findTemplate(templateName);
	if (!tmpl)
	{
		return 0;
	}
	int count = 0;
	mgr.forEachSystem([&](const FXParticleSystem::ParticleSystem &s) {
		if (!s.isDestroyed() && &s.getTemplate() == tmpl)
		{
			++count;
		}
	});
	return count;
}

bool findWaterAndLand(const TerrainPathfindSource &terrain, const GameLogic &logic, Coord3D &waterPos, Coord3D &landPos)
{
	bool foundWater = false;
	bool foundLand = false;
	for (float y = 200.0f; y <= 3800.0f && (!foundWater || !foundLand); y += 40.0f)
	{
		for (float x = 200.0f; x <= 3800.0f && (!foundWater || !foundLand); x += 40.0f)
		{
			float wz = 0.0f, tz = 0.0f;
			if (terrain.isUnderwater(x, y, &wz, &tz))
			{
				if (!foundWater && (wz - tz) > 1.5f)
				{
					waterPos = Coord3D{ x, y, tz };
					foundWater = true;
				}
			}
			else if (!foundLand)
			{
				landPos = Coord3D{ x, y, logic.getGroundHeight(x, y) };
				foundLand = true;
			}
		}
	}
	return foundWater && foundLand;
}
} // namespace

TEST_CASE("LOCAL-66 ParticleSystemFXNugget OnlyIfOnLand and OnlyIfOnWater respect isWater and Object::layer (RW 0x005E1270)")
{
	Win32BIGFileSystem emptyFs;
	FXPlayback playback(emptyFs, RandomAlgorithm::RotWK_GameDat_LCG);
	MockWaterWorld world;
	playback.setWorld(&world);

	const std::string psIni =
		fxtest::systemText("TestLandDust", "    Lifetime = 50 50\n    BurstCount = 1 1\n    BurstDelay = 100 100\n", fxtest::kPointOrtho) +
		fxtest::systemText("TestWaterSplash", "    Lifetime = 50 50\n    BurstCount = 1 1\n    BurstDelay = 100 100\n", fxtest::kPointOrtho);
	REQUIRE(playback.loadIniText("local66_ps.ini", psIni).empty());

	const char *fxIni =
		"FXList FX_Local66FootStep\n"
		"  ParticleSystem\n"
		"    Name = TestLandDust\n"
		"    OnlyIfOnLand = Yes\n"
		"  End\n"
		"  ParticleSystem\n"
		"    Name = TestWaterSplash\n"
		"    OnlyIfOnWater = Yes\n"
		"  End\n"
		"End\n";
	REQUIRE(playback.loadIniText("local66_fx.ini", fxIni).empty());

	const FXList *fx = playback.fxLists().findFXList("FX_Local66FootStep");
	REQUIRE(fx != nullptr);

	// 1. Dry land: dust spawns, splash does not.
	world.waterAnswer = false;
	MockFXObject landObj(Coord3D{ 10.0f, 10.0f, 0.0f }, 1);
	fx->doFXObj(playback.services(), playback.fxLists(), &landObj, nullptr);
	CHECK(countLiveSystems(playback.particles(), "TestLandDust") == 1);
	CHECK(countLiveSystems(playback.particles(), "TestWaterSplash") == 0);
	playback.particles().reset();

	// 2. Water on ground layer (layer == 1): splash spawns, dust does not.
	world.waterAnswer = true;
	MockFXObject waterObj(Coord3D{ 20.0f, 20.0f, 0.0f }, 1);
	fx->doFXObj(playback.services(), playback.fxLists(), &waterObj, nullptr);
	CHECK(countLiveSystems(playback.particles(), "TestLandDust") == 0);
	CHECK(countLiveSystems(playback.particles(), "TestWaterSplash") == 1);
	playback.particles().reset();

	// 3. Over water on a non-ground layer (e.g. bridge, layer != 1, RW 0x5E19E1 / 0x5E1A6F): treated as land.
	world.waterAnswer = true;
	MockFXObject bridgeObj(Coord3D{ 20.0f, 20.0f, 10.0f }, 2);
	fx->doFXObj(playback.services(), playback.fxLists(), &bridgeObj, nullptr);
	CHECK(countLiveSystems(playback.particles(), "TestLandDust") == 1);
	CHECK(countLiveSystems(playback.particles(), "TestWaterSplash") == 0);
}

TEST_CASE("LOCAL-66 ParticleSysBone OnlyIfOnWater and OnlyIfOnLand filter bone-attached systems by LiveFX::isWater (RW 0x004C6514)")
{
	hudtest::SharedWorld &s = hudtest::shared();
	hudtest::Rig rig(s, "map mp fords of isen ii");
	FXPlayback playback(*s.mount->fs, RandomAlgorithm::RotWK_GameDat_LCG);
	REQUIRE(playback.loadRetailData().empty());

	const std::string psIni =
		fxtest::systemText("Local66BoneLand", "    Lifetime = 50 50\n    BurstCount = 1 1\n    BurstDelay = 100 100\n", fxtest::kPointOrtho) +
		fxtest::systemText("Local66BoneWater", "    Lifetime = 50 50\n    BurstCount = 1 1\n    BurstDelay = 100 100\n", fxtest::kPointOrtho);
	REQUIRE(playback.loadIniText("local66_bone_ps.ini", psIni).empty());

	drawtest::Harness parser;
	W3DModelDrawModuleData *data = parser.parse(
		"DefaultModelConditionState\n"
		"  Model = SyntheticModel\n"
		"  ParticleSysBone = NONE Local66BoneLand OnlyIfOnLand: Yes\n"
		"  ParticleSysBone = NONE Local66BoneWater OnlyIfOnWater: Yes\n"
		"End\n"
		"End\n");
	REQUIRE(data != nullptr);

	LiveFX fx(*rig.game, playback);
	const TerrainPathfindSource *terrain = rig.game->pathTerrain();
	REQUIRE(terrain != nullptr);
	Coord3D waterPos{}, landPos{};
	REQUIRE(findWaterAndLand(*terrain, rig.logic(), waterPos, landPos));

	std::string err;
	Object *unit = rig.game->createObject("GondorFighter", rig.index(), waterPos, 0.0f, &err);
	REQUIRE_MESSAGE(unit != nullptr, err);
	rig.frame(1);
	Drawable *d = rig.game->drawables().findByObject(unit->getID());
	REQUIRE(d != nullptr);
	struct DummyAssets : W3DDrawAssets {
		const RenderObjPrototype *model(const std::string &, std::string *) override { return nullptr; }
		const HAnimClass *animation(const W3DAnimationLookup &, std::string *, std::string *) override { return nullptr; }
	} drawAssets;
	struct DummyRng : W3DDrawRandom { int value(int lo, int) override { return lo; } float real(float lo, float) override { return lo; } } drawRng;
	W3DScriptedModelDraw::Options drawOpts;
	drawOpts.buildBones = false;

	d->entries().clear();
	DrawEntry entry;
	entry.className = "W3DScriptedModelDraw";
	entry.kind = W3D_DRAWKIND_MODEL;
	entry.data = data;
	entry.draw = std::make_unique<W3DScriptedModelDraw>(*data, drawAssets, drawRng, nullptr, drawOpts);
	d->entries().push_back(std::move(entry));

	// In water: OnlyIfOnWater attaches, OnlyIfOnLand does not.
	d->setPosition(&waterPos);
	fx.updateAttachedSystems();
	CHECK(countLiveSystems(playback.particles(), "Local66BoneLand") == 0);
	CHECK(countLiveSystems(playback.particles(), "Local66BoneWater") == 1);

	// Move to dry land and re-attach: OnlyIfOnLand attaches, OnlyIfOnWater does not.
	d->entries().clear();
	fx.updateAttachedSystems();
	DrawEntry entry2;
	entry2.className = "W3DScriptedModelDraw";
	entry2.kind = W3D_DRAWKIND_MODEL;
	entry2.data = data;
	entry2.draw = std::make_unique<W3DScriptedModelDraw>(*data, drawAssets, drawRng, nullptr, drawOpts);
	d->entries().push_back(std::move(entry2));
	d->setPosition(&landPos);
	fx.updateAttachedSystems();
	CHECK(countLiveSystems(playback.particles(), "Local66BoneLand") == 1);
	CHECK(countLiveSystems(playback.particles(), "Local66BoneWater") == 0);
}

TEST_CASE("LOCAL-66 ENNORATH scenario: wading RohanEntFir and MordorMumakil on Fords of Isen raise splashes instead of footstep dust (RW 0x005E1270, RW 0x67DAE6)")
{
	ennorathscenario::World world;
	if (!world.load("LOCAL-66 ENNORATH scenario"))
	{
		return;
	}
	hudtest::Rig rig(world.shared, "map mp fords of isen ii");
	FXPlayback playback(*world.shared.mount->fs, RandomAlgorithm::RotWK_GameDat_LCG);
	std::vector<std::string> fxErrors = playback.loadRetailData(world.shared.world->report().macrosBeforeFX);
	REQUIRE_MESSAGE(fxErrors.empty(), "FXPlayback::loadRetailData failed");
	LiveFX fx(*rig.game, playback);

	const TerrainPathfindSource *terrain = rig.game->pathTerrain();
	REQUIRE(terrain != nullptr);
	Coord3D waterPos{}, landPos{};
	REQUIRE(findWaterAndLand(*terrain, rig.logic(), waterPos, landPos));
	CHECK(fx.isWater(waterPos.x, waterPos.y));
	CHECK_FALSE(fx.isWater(landPos.x, landPos.y));

	const int player1 = rig.index();
	std::string err;

	// 1. RohanEntFir (RohanEntBase, data/ini/object/goodfaction/units/ents/entsinfantry.ini):
	// FX_EntLeftFootStep -> FX_EntFootStep (entFootDust OnlyIfOnLand=Yes, MumakilFootSplash OnlyIfOnWater=Yes)
	Object *ent = rig.game->createObject("RohanEntFir", player1, waterPos, 0.0f, &err);
	REQUIRE_MESSAGE(ent != nullptr, err);
	REQUIRE(ent->getAIUpdateInterface() != nullptr);
	ent->getAIUpdateInterface()->aiMoveToPosition(Coord3D{ waterPos.x + 100.0f, waterPos.y, waterPos.z }, CMD_FROM_PLAYER);
	rig.frame(3);
	fx.flushPending();
	fx.updateAttachedSystems();
	CHECK(ent->testModelCondition(ModelCondition::indexOf("WADING")));

	const FXList *entLeftFoot = playback.fxLists().findFXList("FX_EntLeftFootStep");
	const FXList *entFoot = playback.fxLists().findFXList("FX_EntFootStep");
	REQUIRE(entLeftFoot != nullptr);
	REQUIRE(entFoot != nullptr);

	playback.particles().reset();
	entFoot->doFXPos(playback.services(), playback.fxLists(), &waterPos, nullptr, 0.0f, nullptr);
	CHECK(countLiveSystems(playback.particles(), "entFootDust") == 0);
	CHECK(countLiveSystems(playback.particles(), "MumakilFootSplash") == 1);

	playback.particles().reset();
	entFoot->doFXPos(playback.services(), playback.fxLists(), &landPos, nullptr, 0.0f, nullptr);
	CHECK(countLiveSystems(playback.particles(), "entFootDust") == 1);
	CHECK(countLiveSystems(playback.particles(), "MumakilFootSplash") == 0);

	// 2. MordorMumakil (data/ini/object/evilfaction/units/evilmen/mumakil.ini):
	// FX_MUMLeftFrontFootStep -> FX_MUMFootStep (MumakilFootDust OnlyIfOnLand=Yes, MumakilFootSplash OnlyIfOnWater=Yes)
	const FXList *mumLeftFoot = playback.fxLists().findFXList("FX_MUMLeftFrontFootStep");
	const FXList *mumFoot = playback.fxLists().findFXList("FX_MUMFootStep");
	REQUIRE(mumLeftFoot != nullptr);
	REQUIRE(mumFoot != nullptr);

	playback.particles().reset();
	mumFoot->doFXPos(playback.services(), playback.fxLists(), &waterPos, nullptr, 0.0f, nullptr);
	CHECK(countLiveSystems(playback.particles(), "MumakilFootDust") == 0);
	CHECK(countLiveSystems(playback.particles(), "MumakilFootSplash") == 1);

	playback.particles().reset();
	mumFoot->doFXPos(playback.services(), playback.fxLists(), &landPos, nullptr, 0.0f, nullptr);
	CHECK(countLiveSystems(playback.particles(), "MumakilFootDust") == 1);
	CHECK(countLiveSystems(playback.particles(), "MumakilFootSplash") == 0);
}
