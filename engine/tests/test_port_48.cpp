// OpenBFME unit tests for PORT-48 (issue #152): W3DModelDraw/WadingParticleSys.
// Everything here is synthetic: no RotWK or Ennorath files are mounted.

#include "doctest.h"

#include "FxTestUtil.h"
#include "ObjectTestUtil.h"
#include "W3DDrawTestUtil.h"

#include "GameClient/LiveFX.h"
#include "GameClient/MapChunks.h"
#include "GameLogic/Map/TerrainLogic.h"
#include "GameLogic/Map/TerrainPathfindSource.h"
#include "GameEngineDevice/W3DDevice/GameClient/WorldHeightMap.h"
#include "Libraries/WWVegas/WW3D2/assetmgr.h"

#include <set>
#include <string>
#include <vector>

using namespace FXParticleSystem;

namespace
{
struct SyntheticTerrain
{
	WorldHeightMap heights;
	MapChunks chunks;
	TerrainLogic logic;
	TerrainPathfindSource path;

	SyntheticTerrain() : path(logic, heights, chunks)
	{
		heights.m_width = 12;
		heights.m_height = 12;
		heights.m_borderSize = 1;
		heights.m_dataSize = 12 * 12;
		heights.m_data.assign(12u * 12u, 0);

		StandingWaterArea shallow;
		shallow.waterHeight = 15;
		shallow.points = { { 0.0f, 0.0f }, { 50.0f, 0.0f }, { 50.0f, 50.0f }, { 0.0f, 50.0f } };
		chunks.standingWaterAreas.push_back(shallow);
		logic.init(heights, chunks);
	}
};

// A real Drawable with a synthetic draw entry; its asset source is an empty in-memory archive filesystem.
struct SyntheticDrawable
{
	objtest::World objectWorld;
	ArchiveW3DFileSource source;
	WW3DAssetManager assets;
	DrawServices services;
	ThingTemplate thing;
	std::vector<std::string> problems;
	std::unique_ptr<Drawable> drawable;

	explicit SyntheticDrawable(const W3DModelDrawModuleData *data)
		: source(objectWorld.fx.fsys)
		, assets(source)
		, services(assets, objectWorld.modules, W3DLuaDrawScriptHost::Options{})
	{
		thing.friend_setTemplateName("PORT48SyntheticDrawable");
		drawable = std::make_unique<Drawable>(services, &thing, 1, 1, std::vector<ClientEvent::ClientModule>{}, problems);
		DrawEntry entry;
		entry.className = "W3DScriptedModelDraw";
		entry.kind = W3D_DRAWKIND_MODEL;
		entry.data = data;
		drawable->entries().push_back(std::move(entry));
	}
};

std::string syntheticParticleSystem(const char *name)
{
	return fxtest::systemText(name, "    Lifetime = 50 50\n    BurstCount = 1 1\n    BurstDelay = 100 100\n", fxtest::kPointOrtho);
}

W3DModelDrawModuleData *parseWadingModule(drawtest::Harness &parser)
{
	return parser.parse("WadingParticleSys = WadingSurface\n"
						"DefaultModelConditionState\n"
						"  Model = SyntheticModel\n"
						"  ParticleSysBone = FOOT ExistingAttached\n"
						"End\n"
						"End\n");
}
} // namespace

TEST_CASE("PORT-48 shallow water: WADING creates and follows the configured WadingParticleSys; dry land stops it")
{
	drawtest::Harness parser;
	W3DModelDrawModuleData *data = parseWadingModule(parser);
	REQUIRE(data != nullptr);
	CHECK(data->m_wadingParticleSys == "WadingSurface");
	REQUIRE(data->m_conditionStates.size() == 1u);
	REQUIRE(data->m_conditionStates[0].particleSysBones.size() == 1u);
	CHECK(data->m_conditionStates[0].particleSysBones[0].systemName == "ExistingAttached");
	CHECK(ModelCondition::indexOf("WADING") == WadingParticleSysUpdater::kWadingConditionBit);

	SyntheticDrawable synthetic(data);
	SyntheticTerrain terrain;
	fxtest::Sim sim(syntheticParticleSystem("WadingSurface"));
	WadingParticleSysUpdater updater(*sim.mgr);
	std::set<std::string> unresolved;

	ModelConditionFlags wading;
	wading.set(WadingParticleSysUpdater::kWadingConditionBit);
	synthetic.drawable->setModelConditionFlags(wading);
	Coord3D wetPosition{ 25.0f, 25.0f, 2.0f };
	synthetic.drawable->setPosition(&wetPosition);
	updater.update(*synthetic.drawable, terrain.path, unresolved);

	const ParticleSystemID id = updater.systemId(synthetic.drawable->getID(), 0);
	REQUIRE(id != INVALID_PARTICLE_SYSTEM_ID);
	ParticleSystem *system = sim.mgr->findParticleSystemByID(id);
	REQUIRE(system != nullptr);
	sim.step(); // the manager applies the particle system's local transform before emission
	CHECK(system->transform().Row[0][3] == 25.0f);
	CHECK(system->transform().Row[1][3] == 25.0f);
	CHECK(system->transform().Row[2][3] == 15.0f); // TerrainPathfindSource's standing-water surface
	CHECK(updater.liveCount() == 1u);
	CHECK(unresolved.empty());

	// Still wading: update the same system's position instead of creating a duplicate.
	Coord3D movedPosition{ 30.0f, 25.0f, 1.0f };
	synthetic.drawable->setPosition(&movedPosition);
	updater.update(*synthetic.drawable, terrain.path, unresolved);
	CHECK(updater.systemId(synthetic.drawable->getID(), 0) == id);
	CHECK(updater.liveCount() == 1u);
	sim.step();
	CHECK(system->transform().Row[0][3] == 30.0f);
	CHECK(system->transform().Row[2][3] == 15.0f);

	// Even if a synthetic drawable retains WADING, dry terrain must not start a surface effect.
	Coord3D dryPosition{ 75.0f, 25.0f, 4.0f };
	synthetic.drawable->setPosition(&dryPosition);
	updater.update(*synthetic.drawable, terrain.path, unresolved);
	CHECK(updater.systemId(synthetic.drawable->getID(), 0) == INVALID_PARTICLE_SYSTEM_ID);
	CHECK(updater.liveCount() == 0u);
	const ParticleSystem *stopped = sim.mgr->findParticleSystemByID(id);
	if (stopped)
	{
		CHECK(stopped->isDestroyed());
	}
	else
	{
		CHECK(stopped == nullptr);
	}

	// Conversely, water without the WADING condition is still not an active WadingParticleSys state.
	ModelConditionFlags dry;
	synthetic.drawable->setModelConditionFlags(dry);
	synthetic.drawable->setPosition(&wetPosition);
	updater.update(*synthetic.drawable, terrain.path, unresolved);
	CHECK(updater.systemId(synthetic.drawable->getID(), 0) == INVALID_PARTICLE_SYSTEM_ID);
	CHECK(updater.liveCount() == 0u);
}

TEST_CASE("PORT-48 adjacent: WadingParticleSys cleanup leaves ParticleSysBone data and its separate system alone")
{
	drawtest::Harness parser;
	W3DModelDrawModuleData *data = parseWadingModule(parser);
	REQUIRE(data != nullptr);
	REQUIRE(data->m_conditionStates.size() == 1u);
	REQUIRE(data->m_conditionStates[0].particleSysBones.size() == 1u);
	CHECK(data->m_conditionStates[0].particleSysBones[0].systemName == "ExistingAttached");

	SyntheticDrawable synthetic(data);
	SyntheticTerrain terrain;
	fxtest::Sim sim(syntheticParticleSystem("WadingSurface") + syntheticParticleSystem("ExistingAttached"));
	const ParticleSystemID attachedId = sim.spawn("ExistingAttached"); // the existing ParticleSysBone path's independent system
	REQUIRE(attachedId != INVALID_PARTICLE_SYSTEM_ID);
	WadingParticleSysUpdater updater(*sim.mgr);
	std::set<std::string> unresolved;

	ModelConditionFlags wading;
	wading.set(WadingParticleSysUpdater::kWadingConditionBit);
	synthetic.drawable->setModelConditionFlags(wading);
	Coord3D wetPosition{ 25.0f, 25.0f, 0.0f };
	synthetic.drawable->setPosition(&wetPosition);
	updater.update(*synthetic.drawable, terrain.path, unresolved);
	const ParticleSystemID wadingId = updater.systemId(synthetic.drawable->getID(), 0);
	REQUIRE(wadingId != INVALID_PARTICLE_SYSTEM_ID);
	CHECK(wadingId != attachedId);

	ModelConditionFlags dry;
	synthetic.drawable->setModelConditionFlags(dry);
	updater.update(*synthetic.drawable, terrain.path, unresolved);
	CHECK(updater.systemId(synthetic.drawable->getID(), 0) == INVALID_PARTICLE_SYSTEM_ID);
	const ParticleSystem *attached = sim.mgr->findParticleSystemByID(attachedId);
	REQUIRE(attached != nullptr);
	CHECK_FALSE(attached->isDestroyed());
	CHECK(data->m_conditionStates[0].particleSysBones[0].systemName == "ExistingAttached");
	CHECK(unresolved.empty());
}
