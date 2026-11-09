// OpenBFME unit tests: LOCAL-17 tree and shrub textures: W3DTreeDraw/TextureName (issue #138, TOP20 #22).
//
// RotWK addresses:
//   W3DTreeBuffer::Add_Tree: RW 0x4EC93A (slot 21 in W3DTerrainVisual, RW 0x49184F -> RW 0x4678F3)
//     Type lookup loop: RW 0x4EC976 (ModelName at +0x8) and RW 0x4EC986 (TextureName at +0xC), stride 0x5C
//     Type creation: RW 0x4EC11E
//   W3DShrubBuffer::Add_Shrub: RW 0x4E8B8F (slot 26 in W3DTerrainVisual, RW 0x491986 -> RW 0x4679DB)
//     Type lookup loop: RW 0x4E8BDC (ModelName at +0x8) and RW 0x4E8BEE (TextureName at +0xC), stride 0x5C
//     Type creation: RW 0x4E80F5
//   Caller confirmation:
//     GameLogic::addTree (RW 0x683D89) and GameLogic::addShrub (RW 0x6808ED) call [edx + 0x40] on first draw module
//     returning W3DTreeDrawModuleData* passed as ebx (RW 0x4EC96A, RW 0x4E8BCE)
//   Texture loading:
//     RW 0x4EAE00..0x4EAF5F loads TextureName via WW3DAssetManager::Get_Texture (RW 0x532875), adds to atlas (RW 0x57082B).
//     Missing texture logs warning and renders with fallback UVs [0, 1] without crashing.
//   Tree topple physics:
//     RW 0x4E9E4E (topple velocity), RW 0x4EA068 (sink).

#include "doctest.h"

#include "EnnorathScenario.h"
#include "ObjectTestUtil.h"

#include "Common/AsciiString.h"
#include "Common/Thing/ThingTemplate.h"
#include "GameClient/MapCreationHooks.h"
#include "GameClient/MapHordeSpawn.h"
#include "GameClient/MapObjectDrawables.h"
#include "GameClient/MapObjectRuntime.h"
#include "GameClient/TreeTopple.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DDrawModules.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DTreeDraw.h"
#include "GameLogic/Map/TerrainLogic.h"
#include "Libraries/WWVegas/WW3D2/assetmgr.h"

#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace
{

struct MapWorld : objtest::World
{
	MapWorld()
	{
		W3DDrawModules::registerTypedDrawModuleData(modules);
		MapHordeSpawn::bindHordeContainData(modules);
	}
};

WorldHeightMap flatMap(int w, int h, int raw)
{
	WorldHeightMap m;
	m.m_width = w;
	m.m_height = h;
	m.m_borderSize = 0;
	m.m_dataSize = w * h;
	m.m_data.assign((size_t)w * h, static_cast<std::uint16_t>(raw));
	return m;
}

MapObject object(const char *name, float x, float y, float z = 0.0f, float angle = 0.0f, int flags = 0)
{
	MapObject o;
	o.m_objectName = name;
	o.m_location = { x, y, z };
	o.m_angle = angle;
	o.m_flags = flags;
	return o;
}

struct Scene
{
	LoadedMap map;
	MapObjectOptions options;
	std::unique_ptr<TerrainLogic> terrain;
	MapObjectDrawables out;

	explicit Scene(int raw = 256, int cells = 100)
	{
		map.heightMap = flatMap(cells, cells, raw);
		map.chunks.hasObjectsList = true;
		options.useTrees = true;
	}

	void run(MapWorld &w)
	{
		terrain = std::make_unique<TerrainLogic>();
		terrain->init(map.heightMap, map.chunks, nullptr);
		MapObjectCreation::build(map, "test", w.things, *terrain, options, out);
	}
};

struct NoFiles : W3DFileSource
{
	bool Exists(const std::string &) const override { return false; }
	bool Read(const std::string &, std::vector<std::uint8_t> &, std::string *error) override
	{
		if (error) *error = "no files";
		return false;
	}
};

void stepTopple(TreeTopple &tt, int updates)
{
	for (int i = 0; i < updates; ++i)
	{
		tt.advance(TreeTopple::kUpdateSeconds);
	}
}

} // namespace

TEST_CASE("LOCAL-17 tree types: distinct (ModelName, TextureName) create separate types in MapObjectRuntime (RW 0x4E8C1B, RW 0x4EC9B5)")
{
	MapWorld w;
	const std::string ini =
		"Object TreeTypeA\n"
		"  Draw = W3DTreeDraw ModuleTag_01\n"
		"    ModelName = PTTree01\n"
		"    TextureName = TreeA.tga\n"
		"  End\n"
		"  KindOf = SHRUBBERY TREE\n"
		"End\n"
		"Object TreeTypeB\n"
		"  Draw = W3DTreeDraw ModuleTag_01\n"
		"    ModelName = PTTree01\n"
		"    TextureName = TreeB.tga\n"
		"  End\n"
		"  KindOf = SHRUBBERY TREE\n"
		"End\n"
		"Object TreeTypeA_Dup\n"
		"  Draw = W3DTreeDraw ModuleTag_01\n"
		"    ModelName = PTTree01\n"
		"    TextureName = TreeA.tga\n"
		"  End\n"
		"  KindOf = SHRUBBERY TREE\n"
		"End\n"
		"Object TreeNoTex\n"
		"  Draw = W3DTreeDraw ModuleTag_01\n"
		"    ModelName = PTTree01\n"
		"  End\n"
		"  KindOf = SHRUBBERY TREE\n"
		"End\n"
		"Object ShrubTypeC\n"
		"  Draw = W3DTreeDraw ModuleTag_01\n"
		"    ModelName = PTShrub01\n"
		"    TextureName = ShrubC.tga\n"
		"  End\n"
		"  KindOf = SHRUBBERY SHRUB OPTIMIZED_PROP\n"
		"End\n";

	REQUIRE(w.load(ini).empty());

	Scene s;
	s.map.chunks.objects = {
		object("TreeTypeA", 10.0f, 10.0f),
		object("TreeTypeB", 20.0f, 10.0f),
		object("TreeTypeA_Dup", 30.0f, 10.0f),
		object("TreeNoTex", 40.0f, 10.0f),
		object("ShrubTypeC", 50.0f, 10.0f)
	};
	s.run(w);

	NoFiles files;
	WW3DAssetManager assets(files);
	MapObjectRuntime runtime(assets);
	runtime.build(s.out, false);

	const MapRuntimeReport &r = runtime.report();
	CHECK(r.treeDraws == 5);

	// In RotWK retail:
	// - TreeTypeA (pttree01, treea.tga) and TreeTypeA_Dup (pttree01, treea.tga) share one tree type entry
	// - TreeTypeB (pttree01, treeb.tga) forms a second distinct tree type entry despite sharing ModelName
	// - TreeNoTex (pttree01, "") forms a third type with empty texture name
	// - ShrubTypeC (ptshrub01, shrubc.tga) forms a fourth type
	CHECK(r.distinctTreeTypes.size() == 4u);
	CHECK(r.distinctTreeTypes.at({ "pttree01", "treea.tga" }) == 2u);
	CHECK(r.distinctTreeTypes.at({ "pttree01", "treeb.tga" }) == 1u);
	CHECK(r.distinctTreeTypes.at({ "pttree01", "" }) == 1u);
	CHECK(r.distinctTreeTypes.at({ "ptshrub01", "shrubc.tga" }) == 1u);

	// Placed models preserve textureName across instances for Godot device instantiation
	const std::vector<MapPlacedModel> &placed = runtime.models();
	REQUIRE(placed.size() == 5u);
	CHECK(placed[0].modelName == "PTTree01");
	CHECK(placed[0].textureName == "TreeA.tga");
	CHECK(placed[1].modelName == "PTTree01");
	CHECK(placed[1].textureName == "TreeB.tga");
	CHECK(placed[2].modelName == "PTTree01");
	CHECK(placed[2].textureName == "TreeA.tga");
	CHECK(placed[3].modelName == "PTTree01");
	CHECK(placed[3].textureName.empty());
	CHECK(placed[4].modelName == "PTShrub01");
	CHECK(placed[4].textureName == "ShrubC.tga");
}

TEST_CASE("LOCAL-17 missing texture: retail WW3DAssetManager does not throw or crash on missing texture (RW 0x532875)")
{
	NoFiles files;
	WW3DAssetManager assets(files);

	// RW 0x532875 (WW3DAssetManager::Get_Texture): missing texture logs a warning and returns null/fallback, no crash
	std::string err;
	const W3DFileContents *contents = assets.Load_File("art/compiledtextures/xx/nonexistent.dds", &err);
	CHECK(contents == nullptr);
	CHECK_FALSE(err.empty());

	const RenderObjPrototype *robj = assets.Create_Render_Obj("nonexistent_tree_model", &err);
	CHECK(robj == nullptr);
	CHECK_FALSE(err.empty());
}

TEST_CASE("LOCAL-17 neighbouring topple: tree topple, sink and removal physics remain intact (RW 0x4E9E4E, RW 0x4EA068)")
{
	W3DTreeDrawModuleData d;
	d.m_initialVelocityPercent = 0.2f;
	d.m_initialAccelPercent = 0.01f;
	d.m_bounceVelocityPercent = 0.3f;
	d.m_minimumToppleSpeed = 0.5f;
	d.m_sinkFrames = 10; // 10 logic frames * 6 client frames = 60 updates
	d.m_sinkDistance = 20.0f;
	d.m_bounceFX = "FX_TreeBounce";
	d.m_toppleFX = "FX_TreeTopple";

	TreeTopple tt;
	tt.add(1, Coord3D{ 0.0f, 0.0f, 0.0f }, &d);
	tt.add(2, Coord3D{ 50.0f, 0.0f, 0.0f }, &d);

	REQUIRE(tt.topple(1, Coord3D{ 1.0f, 0.0f, 0.0f }, 0.0f));
	stepTopple(tt, 2);

	const float v0 = 0.5f * 0.2f, acc = 0.5f * 0.01f;
	const float a = v0 + (v0 + acc);
	CHECK(tt.trees()[0].angle == a);

	// Lumber removed while falling -> keeps falling and starts sinking (RW 0x4EA068)
	tt.remove(1);
	stepTopple(tt, 1);
	CHECK(tt.trees()[0].sinking);
	CHECK(tt.trees()[0].sink == 2.0f); // 20 / 10

	// Immediate removal on construction drops tree at once (RW 0x684BCA)
	tt.removeNow(2);
	CHECK(tt.trees()[1].state == TreeTopple::GONE);
	CHECK_FALSE(tt.topple(2, Coord3D{ 1.0f, 0.0f, 0.0f }, 0.0f));
}

TEST_CASE("LOCAL-17 Ennorath scenario tree textures (TOP20 #22)")
{
	ennorathscenario::World world;
	if (!world.load("LOCAL-17 Ennorath scenario tree textures"))
	{
		return;
	}

	// 1. Grass01: model PTGrass02 overridden with PShrubbery01.tga
	const ThingTemplate *grass01 = world.shared.world->things().findTemplate("Grass01");
	REQUIRE(grass01 != nullptr);
	const W3DTreeDrawModuleData *tdGrass01 = nullptr;
	for (const ThingTemplate::Nugget &n : grass01->drawModules().nuggets())
	{
		tdGrass01 = dynamic_cast<const W3DTreeDrawModuleData *>(n.data.get());
		if (tdGrass01) break;
	}
	REQUIRE(tdGrass01 != nullptr);
	CHECK(tdGrass01->m_modelName == "PTGrass02");
	CHECK(tdGrass01->m_textureName == "PShrubbery01.tga");

	// 2. OptGrass03: model PTGrass06 overridden with PTGrass05.tga
	const ThingTemplate *grass03 = world.shared.world->things().findTemplate("OptGrass03");
	REQUIRE(grass03 != nullptr);
	const W3DTreeDrawModuleData *tdGrass03 = nullptr;
	for (const ThingTemplate::Nugget &n : grass03->drawModules().nuggets())
	{
		tdGrass03 = dynamic_cast<const W3DTreeDrawModuleData *>(n.data.get());
		if (tdGrass03) break;
	}
	REQUIRE(tdGrass03 != nullptr);
	CHECK(tdGrass03->m_modelName == "PTGrass06");
	CHECK(tdGrass03->m_textureName == "PTGrass05.tga");

	// 3. TreeOldForest01: model PTOldForest01 overridden with PTOldForest01.tga
	const ThingTemplate *oldForest = world.shared.world->things().findTemplate("TreeOldForest01");
	REQUIRE(oldForest != nullptr);
	const W3DTreeDrawModuleData *tdOldForest = nullptr;
	for (const ThingTemplate::Nugget &n : oldForest->drawModules().nuggets())
	{
		tdOldForest = dynamic_cast<const W3DTreeDrawModuleData *>(n.data.get());
		if (tdOldForest) break;
	}
	REQUIRE(tdOldForest != nullptr);
	CHECK(tdOldForest->m_modelName == "PTOldForest01");
	CHECK(tdOldForest->m_textureName == "PTOldForest01.tga");
}
