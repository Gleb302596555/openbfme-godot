// PORT-69: client terrain tracks (RotWK RW 0x483870, 0x483BD2, 0x483EEB, 0x48470D).
#include "doctest.h"
#include "W3dTestUtil.h"
#include "Libraries/WWVegas/WW3D2/assetmgr.h"
#include "Libraries/WWVegas/WW3D2/w3d_file.h"
#include "Libraries/WWVegas/WWLib/chunkio.h"
#include "GameEngineDevice/W3DDevice/GameClient/TerrainTracks.h"
#include <vector>

namespace
{
TerrainTracksHeightSampler flatTrackTerrain()
{
	TerrainTracksHeightSampler s;
	s.groundHeight = [](float, float, Coord3D *normal) {
		if (normal) *normal = {0.0f, 0.0f, 1.0f};
		return 3.0f;
	};
	return s;
}
void stepTrack(TerrainTracksSystem &sys, int id, float x, int layer = 1)
{
	const float basis[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
	const Coord3D pos{x, 0, 3};
	sys.addEdge(id, x, 0, basis, pos, layer, flatTrackTerrain());
}
}
TEST_CASE("PORT-69: track binding, edge ring, geometry and lifetime")
{
	TerrainTracksConfig config;
	config.makeTrackMarks = true;
	config.maxTerrainTracks = 1;
	config.maxTankTrackEdges = 4;
	config.maxTankTrackOpaqueEdges = 2;
	config.maxTankTrackFadeDelay = 1000;
	TerrainTracksSystem sys;
	sys.init(config);
	int id = sys.bindTrack(nullptr, 1, 10, "EXTRACKS.TGA", "", "");
	REQUIRE(id == 0);
	CHECK(sys.bindTrack(nullptr, 1, 10, "other.tga", "", "") == -1);
	CHECK(sys.stats().poolExhausted == 1);
	CHECK(sys.track(id)->width() == 14.0f);
	stepTrack(sys, id, 0); // anchor
	stepTrack(sys, id, 10);
	stepTrack(sys, id, 20);
	stepTrack(sys, id, 30);
	stepTrack(sys, id, 40);
	REQUIRE(sys.track(id)->activeEdgeCount() == 4);
	std::vector<TerrainTracksSystem::FlushBatch> batches;
	TerrainTracksShade shade;
	shade.ambient[0] = 0.2f; shade.diffuse[0] = 0.4f;
	CHECK(TerrainTracksSystem::trackDiffuseBase(shade) == 0x660000);
	sys.buildFlushBatches(shade, batches);
	REQUIRE(batches.size() == 1);
	CHECK(batches[0].textureName == "extracks.tga");
	CHECK(batches[0].vertices.size() == 8);
	CHECK(batches[0].indices.size() == 18);
	CHECK(batches[0].vertices[0].z == 5.0f);
	CHECK((batches[0].vertices[4].diffuse >> 24) == 255);
	CHECK((batches[0].vertices[6].diffuse >> 24) == 0); // newest edge transparent
	stepTrack(sys, id, 50); // bounded ring evicts oldest
	CHECK(sys.track(id)->activeEdgeCount() == 4);
	CHECK(sys.track(id)->bottomIndex() == 1);
	sys.unbindTrack(id);
	sys.update(1000);
	CHECK(sys.stats().usedTracks == 0);
	CHECK(sys.stats().freeTracks == 1);
}
TEST_CASE("PORT-69: caps, layer gate, detail changes and disabled emission")
{
	TerrainTracksConfig c;
	c.makeTrackMarks = true; c.maxTerrainTracks = 1;
	TerrainTracksSystem sys;
	sys.init(c);
	int id = sys.bindTrack(nullptr, 1, 10, "x.tga", "", "");
	stepTrack(sys, id, 0);
	stepTrack(sys, id, 10, 2); // do not emit on non-ground layers
	CHECK(sys.track(id)->activeEdgeCount() == 0);
	stepTrack(sys, id, 10);
	stepTrack(sys, id, 20);
	stepTrack(sys, id, 30);
	sys.addCapEdge(id, 45, 0, 1, flatTrackTerrain());
	CHECK(sys.track(id)->haveCap());
	CHECK_FALSE(sys.track(id)->haveAnchor());
	CHECK(sys.track(id)->edges()[3].alpha == 0.0f);
	sys.setDetail(3, 1, 2000);
	CHECK(sys.track(id)->activeEdgeCount() == 0);
	CHECK(sys.track(id)->isBound());
	stepTrack(sys, id, 50);
	sys.setMakeTrackMarks(false);
	CHECK_FALSE(sys.track(id)->haveAnchor());
}

TEST_CASE("PORT-69: stationary movement emits nothing and positive edges fade against time")
{
	TerrainTracksConfig c;
	c.makeTrackMarks = true; c.maxTerrainTracks = 1; c.maxTankTrackFadeDelay = 1000;
	TerrainTracksSystem sys;
	sys.init(c);
	const int id = sys.bindTrack(nullptr, 1, 10, "x.tga", "", "");
	stepTrack(sys, id, 0);
	for (int i = 0; i < 10; ++i) stepTrack(sys, id, 0);
	CHECK(sys.track(id)->activeEdgeCount() == 0);
	stepTrack(sys, id, 10); stepTrack(sys, id, 20); stepTrack(sys, id, 30);
	REQUIRE(sys.track(id)->edges()[2].alpha == 1.0f);
	sys.update(500);
	CHECK(sys.track(id)->edges()[2].alpha == 0.5f);
	sys.update(500);
	CHECK(sys.track(id)->activeEdgeCount() == 0);
}

TEST_CASE("PORT-69: scaled bind-pose bone offsets determine the strip centre and width")
{
	using namespace w3dtest;
	W3dPivotStruct root = {}, left = {}, right = {};
	setName(root.Name, W3D_NAME_LEN, "ROOTTRANSFORM");
	root.ParentIdx = 0xffffffffu; root.Rotation.Q[3] = 1;
	setName(left.Name, W3D_NAME_LEN, "LEFT");
	left.ParentIdx = 0; left.Rotation.Q[3] = 1; left.Translation = {2, -3, 0};
	setName(right.Name, W3D_NAME_LEN, "RIGHT");
	right.ParentIdx = 0; right.Rotation.Q[3] = 1; right.Translation = {2, 3, 0};
	W3dHierarchyStruct hh = {};
	hh.Version = W3D_MAKE_VERSION(4, 1); hh.NumPivots = 3;
	setName(hh.Name, sizeof(hh.Name), "TRACK_SKL");
	ChunkWriter hier, file;
	hier.chunk(W3D_CHUNK_HIERARCHY_HEADER, ChunkWriter::of(hh));
	hier.chunk(W3D_CHUNK_PIVOTS, ChunkWriter::ofArray(std::vector<W3dPivotStruct>{root, left, right}));
	file.wrapper(W3D_CHUNK_HIERARCHY, hier);
	ChunkLoadClass reader(file.bytes.data(), file.bytes.size());
	REQUIRE(reader.Open_Chunk());
	HTreeClass tree;
	std::string error;
	REQUIRE_MESSAGE(tree.Load_W3D(reader, &error) == HTreeClass::OK, error);
	RenderObjPrototype proto;
	proto.Tree = &tree;
	TerrainTracksConfig c; c.makeTrackMarks = true; c.maxTerrainTracks = 1;
	TerrainTracksSystem sys; sys.init(c);
	int id = sys.bindTrack(&proto, 2, 10, "track.tga", "left", "right");
	REQUIRE(id == 0);
	CHECK(sys.track(id)->width() == 16.0f);
	CHECK(sys.track(id)->localEndPoints()[0].x == 4.0f);
	CHECK(sys.track(id)->localEndPoints()[0].y == -6.0f);
	CHECK(sys.track(id)->localEndPoints()[1].y == 6.0f);
	stepTrack(sys, id, 0);
	stepTrack(sys, id, 10);
	REQUIRE(sys.track(id)->activeEdgeCount() == 1);
	CHECK(sys.track(id)->edges()[0].endPointPos[0].x == 14.0f);
	CHECK(sys.track(id)->edges()[0].endPointPos[0].y == 8.0f);
	CHECK(sys.track(id)->edges()[0].endPointPos[1].y == -8.0f);
}

// End-to-end client path: a synthetic W3D draw and its bind-pose bones, with
// poses delivered through the same snapshot sync as a live drawable.
#include "LogicTestUtil.h"
#include "W3dSynth.h"
#include "GameClient/ClientEvents.h"
#include "GameClient/DrawableManager.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DDrawModules.h"

TEST_CASE("PORT-69: moving and stationary drawables emit and fade through snapshot sync")
{
	using namespace w3dsynth;
	logictest::LogicWorld world;
	REQUIRE(world.loadError.empty());
	W3DDrawModules::registerTypedDrawModuleData(world.w.modules);
	REQUIRE(world.w.load(
		"Object TrackVehicle\n"
		"  Draw = W3DScriptedModelDraw ModuleTag_Draw\n"
		"    TrackMarks = TESTTRACK.TGA\n"
		"    TrackMarksLeftBone = LEFT\n"
		"    TrackMarksRightBone = RIGHT\n"
		"    DefaultModelConditionState\n"
		"      Model = ABTRACK_SKN\n"
		"      Skeleton = ABTRACK_SKL\n"
		"    End\n"
		"  End\n"
		"End\n").empty());

	MemoryFileSource fs;
	fs.Add("art\\w3d\\ab\\abtrack_skl.w3d", hierarchyChunk("ABTRACK_SKL", {
		pivot("ROOTTRANSFORM", 0xffffffffu, 0, 0, 0), pivot("LEFT", 0, 2, -3, 0), pivot("RIGHT", 0, 2, 3, 0) }));
	SynthMesh mesh;
	mesh.Container = "ABTRACK_SKN"; mesh.Name = "BODY";
	mesh.Vertices = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
	mesh.Triangles = {0, 1, 2};
	std::vector<std::uint8_t> model;
	append(model, meshChunk(mesh));
	append(model, hlodChunk("ABTRACK_SKN", "ABTRACK_SKL", {{"ABTRACK_SKN.BODY", 0}}));
	fs.Add("art\\w3d\\ab\\abtrack_skn.w3d", model);
	WW3DAssetManager assets(fs);
	world.logic->settings().makeTrackMarks = true;
	world.logic->settings().maxTerrainTracks = 2;
	DrawableManager manager(assets, *world.logic);
	manager.setShroudDrawn(false);
	manager.setParallelSync(false);
	manager.setTrackHeightSampler(flatTrackTerrain());
	manager.tracks().setDetail(100, 25, 1000);
	ClientEventRecorder events(*world.logic);
	world.logic->setClientHooks(&events);
	Object *moving = world.make("TrackVehicle");
	Object *still = world.make("TrackVehicle");
	manager.applyEvents(events.take());
	Drawable *d = manager.findByObject(moving->getID());
	Drawable *s = manager.findByObject(still->getID());
	REQUIRE(d); REQUIRE(s);
	auto sync = [&]() {
		auto snapshot = LogicSnapshot::build(*world.logic, nullptr, false, 0);
		manager.syncTransforms(*snapshot, 0.0, false);
	};
	sync();
	REQUIRE(d->entries().size() == 1);
	REQUIRE(s->entries().size() == 1);
	const int movingId = d->entries()[0].trackId;
	const int stillId = s->entries()[0].trackId;
	REQUIRE(movingId == 0); REQUIRE(stillId == 1);
	CHECK(manager.tracks().track(movingId)->width() == 10.0f);
	for (int x : {10, 20, 30})
	{
		Coord3D pos{(float)x, 0, 0};
		moving->setPosition(&pos);
		sync();
	}
	const TerrainTrack *track = manager.tracks().track(movingId);
	REQUIRE(track->activeEdgeCount() == 3);
	CHECK(track->edges()[0].endPointPos[0].x == 12.0f);
	CHECK(track->edges()[0].endPointPos[0].y == 5.0f);
	CHECK(track->edges()[0].endPointPos[1].y == -5.0f);
	CHECK(manager.tracks().track(stillId)->activeEdgeCount() == 0);
	std::vector<TerrainTracksSystem::FlushBatch> batches;
	manager.tracks().buildFlushBatches({}, batches);
	REQUIRE(batches.size() == 1);
	CHECK(batches[0].textureName == "testtrack.tga");
	CHECK_FALSE(batches[0].indices.empty());
	manager.advance(500);
	CHECK(track->edges()[2].alpha == 0.5f);
	manager.advance(500);
	CHECK(track->activeEdgeCount() == 0);
	world.logic->setClientHooks(nullptr);
}
