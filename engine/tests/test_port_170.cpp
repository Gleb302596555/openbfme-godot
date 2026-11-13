// PORT-170 (issue #399, P-106): the placement seal's colour and the buildout range the code 9 comes from.
// Synthetic INI and synthetic objects only: no retail data, no models.
//
// The owner's symptom: with a still cursor the seal flickers between red and white. The colour path (RW 0x6A3932 .. 0x6A3B37) takes the legal-build code of
// the site (BuildPlacement::isLocationLegalToBuild, RW 0x79681A .. 0x796D34): 0 = no tint, 9 = the blue of RW 0x6A3A0B, anything else = the illegal red.
// RotWK has no taint branch there at all (neither the decomp's BuildAssistant nor the tint loop reads TheTaintManager), so these tests pin the two facts the
// report can be checked against: the code of a still cursor never alternates frame to frame, and the code 9 the tint asks for is produced by the hub's
// MaxBuildoutDistance (RW 0x796C66 .. 0x796D24) - the branch the port was missing.
//
// GPL-3.0.

#include "doctest.h"

#include "GameClient/HudInput.h"
#include "GameClient/TacticalView.h"
#include "GameLogic/BuildPlacement.h"
#include "GameLogic/Map/TerrainLogic.h"
#include "GameLogic/Object/PartitionManager.h"
#include "GameLogic/System/TaintManager.h"
#include "GameEngineDevice/W3DDevice/GameClient/WorldHeightMap.h"
#include "MoveTestUtil.h"

#include <string>
#include <vector>

using namespace movetest;

namespace
{
// An orthographic view makes a pixel an exact world x/y (as in SyntheticHudTestUtil.h).
class PixelView : public TacticalView
{
public:
	ICoord2D size() const override { return { 2048, 2048 }; }
	bool worldToScreen(const Coord3D &p, ICoord2D &s) const override { s = { (int)p.x, (int)p.y }; return true; }
	bool screenToRay(const ICoord2D &p, Coord3D &o, Coord3D &d) const override
	{
		o = { (float)p.x, (float)p.y, 100.0f };
		d = { 0.0f, 0.0f, -1.0f };
		return true;
	}
	Coord3D position() const override { return m_position; }
	void lookAt(const Coord3D &p) override { m_position = p; }

private:
	Coord3D m_position{ 1024, 1024, 0 };
};

// Port170Build is an ordinary structure (no hub: never code 9). Port170Hub is a WALL_HUB build with a buildout range of 200: the code 9's owner. The dozer
// carries an AIUpdateInterface because isLocationLegalToBuild asks the builder's one (RW 0x796A46 .. 0x796A98) and refuses without it.
const char kPort170Objects[] =
	"Object Port170Build\n"
	"  KindOf = STRUCTURE IMMOBILE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryMinorRadius = 10\n"
	"  GeometryHeight = 20\n"
	"End\n"
	"Object Port170Hub\n"
	"  KindOf = STRUCTURE IMMOBILE WALL_HUB\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryMinorRadius = 10\n"
	"  GeometryHeight = 20\n"
	"  Behavior = WallHubBehavior ModuleTag_Hub\n"
	"    MaxBuildoutDistance = 200\n"
	"  End\n"
	"End\n"
	"Object Port170Centre\n"
	"  KindOf = STRUCTURE IMMOBILE COMMANDCENTER\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 20\n"
	"  GeometryMinorRadius = 20\n"
	"  GeometryHeight = 20\n"
	"End\n"
	"Object Port170Dozer\n"
	"  KindOf = DOZER\n"
	"  Behavior = AIUpdateInterface ModuleTag_AI\n"
	"  End\n"
	"End\n";

struct Port170World : MoveWorld
{
	WorldHeightMap heightMap;
	MapChunks chunks;
	TerrainLogic boundaryTerrain;
	PixelView view;
	MetaMap map;
	std::unique_ptr<HudInput> input;

	Port170World() : MoveWorld(200, 200, kPort170Objects)
	{
		heightMap.m_width = 200;
		heightMap.m_height = 200;
		heightMap.m_data.assign(200u * 200u, 0);
		heightMap.m_boundaries = { ICoord2D{ 200, 200 }, ICoord2D{ 200, 200 } }; // the whole map playable: the sites below stay inside MinDistFromEdgeOfMapForBuild
		boundaryTerrain.init(heightMap, chunks);
		logic->setTerrain(&boundaryTerrain);
		logic->partition().setRegion(0.0f, 0.0f, 2000.0f, 2000.0f);
		logic->settings().buildRulesLoaded = true;
		logic->settings().allowedHeightVariationForBuilding = 100.0f;
		logic->settings().supplyBuildBorder = 20.0f;
		logic->settings().minDistFromEdgeOfMapForBuild = 30.0f;
		buildMap();
		// taint (defiled ground) in a disc around (500, 500); 0x40 is below NEUTRAL 0x80
		TaintManager::Region region;
		region.loX = 0.0f;
		region.loY = 0.0f;
		region.hiX = 2000.0f;
		region.hiY = 2000.0f;
		logic->taint().setRegion(region, 10.0f);
		logic->taint().paint(Coord3D{ 500.0f, 500.0f, 0.0f }, 200.0f, 0x40, true, -1);
		players.setLocalPlayer(players.findPlayerWithName("Alice"));
		input = std::make_unique<HudInput>(*logic, ai.get(), view, commands, MouseSettings{ 15, 5, 150 }, map);
	}

	~Port170World()
	{
		logic->reset();
		ai.reset();
		logic->setTerrain(nullptr);
	}

	// The device's cadence (GodotInGameHud::draw): the client frame advances in a 30 Hz catch-up loop, so a render frame carries 0, 1 or 2 of them, and the
	// placement update runs after that with judge = the worker's idle answer. steps[i] is the client frames of the i-th render frame, idle[i] its logic gate.
	void renderFrame(int steps, bool idle)
	{
		for (int i = 0; i < steps; ++i)
		{
			input->ui().advanceClientFrame();
		}
		input->placementUpdate(idle);
	}
};

std::string codesToString(const std::vector<int> &codes)
{
	std::string s;
	for (int c : codes)
	{
		s += std::to_string(c);
		s += ' ';
	}
	return s;
}

bool allEqual(const std::vector<int> &codes)
{
	for (size_t i = 1; i < codes.size(); ++i)
	{
		if (codes[i] != codes[0])
		{
			return false;
		}
	}
	return true;
}

// The judged codes of `n` render frames with the pointer parked at the pixel `at` (the device's cadence: alternating 1 and 2 client frames, the worker idle
// on every second frame, which is what a running game looks like). Judging needs an idle frame whose client frame is odd (RW 0x6A3939), so the loop warms up
// until the first one and the frames it counts are the judged ones and the ones between them.
std::vector<int> judgeFrames(Port170World &w, const char *tmpl, Object *builder, ICoord2D at, int n, bool logicSteps)
{
	w.input->ui().placeBuildAvailable(tmpl, builder->getID());
	w.input->placeTranslator().setLivePointer(at);
	// the first judged frame: an odd client frame with the worker idle
	for (int i = 0; i < 8 && (!w.input->ui().placeHasGhost() || (w.input->ui().clientFrame() & 1) == 0); ++i)
	{
		w.renderFrame(1, true);
	}
	REQUIRE((w.input->ui().clientFrame() & 1) == 1);
	std::vector<int> codes;
	for (int i = 0; i < n; ++i)
	{
		if (logicSteps)
		{
			w.logic->runLogicFrame();
		}
		w.renderFrame(2, i % 2 == 0); // two client frames: the parity returns to odd, where the judgement runs; the busy frames in between keep the last code
		codes.push_back(w.input->ui().placeLegalCode());
	}
	return codes;
}
} // namespace

TEST_CASE("PORT-170: a still cursor keeps one legality code, on taint and off it")
{
	Port170World w;
	Object *dozer = w.spawn("Port170Dozer", 100.0f, 100.0f);
	REQUIRE(dozer != nullptr);

	// on taint: world (500, 500) is inside the painted disc, off it: (1500, 1500) is outside
	REQUIRE(w.logic->taint().stateAt(Coord3D{ 500.0f, 500.0f, 0.0f }, nullptr) == TaintManager::STATE_TAINT);
	REQUIRE(w.logic->taint().stateAt(Coord3D{ 1500.0f, 1500.0f, 0.0f }, nullptr) == TaintManager::STATE_NONE);

	const std::vector<int> onTaint = judgeFrames(w, "Port170Build", dozer, { 500, 500 }, 20, false);
	INFO("codes on taint: " << codesToString(onTaint));
	INFO("site: " << w.input->ui().placeLocation().x << "," << w.input->ui().placeLocation().y);
	CHECK(allEqual(onTaint));
	CHECK(onTaint.front() == (int)LBC_OK); // the legal site: no tint, the same answer on every frame of the flicker's window

	const std::vector<int> offTaint = judgeFrames(w, "Port170Build", dozer, { 1500, 1500 }, 20, false);
	INFO("codes off taint: " << codesToString(offTaint));
	CHECK(allEqual(offTaint));
	// RotWK asks no taint test for a placement legality (BuildAssistant.cpp:2009 .. 2191 has none, the tint loop's RW 0x6A3932 .. 0x6A3B37 reads only the
	// code): the same legal site is the same colour inside the defiled ground and outside it. The red / white the owner sees is not a taint rule to port.
	CHECK(onTaint.front() == offTaint.front());
}

TEST_CASE("PORT-170: the code stays the same while the logic steps under the still cursor")
{
	Port170World w;
	Object *dozer = w.spawn("Port170Dozer", 100.0f, 100.0f);
	REQUIRE(dozer != nullptr);
	const std::vector<int> codes = judgeFrames(w, "Port170Build", dozer, { 500, 500 }, 20, true);
	INFO("codes with the logic stepping: " << codesToString(codes));
	CHECK(allEqual(codes));
	CHECK(codes.front() == (int)LBC_OK);
}

TEST_CASE("PORT-170: a WALL_HUB build outside its MaxBuildoutDistance is the code 9, inside it is legal")
{
	Port170World w;
	Object *dozer = w.spawn("Port170Dozer", 100.0f, 100.0f);
	Object *centre = w.spawn("Port170Centre", 500.0f, 500.0f);
	REQUIRE(dozer != nullptr);
	REQUIRE(centre != nullptr);

	// RW 0x796C66 .. 0x796D24: the hub's own WallHubBehavior range (200 above), measured to the owner's command centres (the collector RW 0x6B00F3 ->
	// 0x6B00CE, its objects' positions); (560, 500) is 60 away, (1500, 1500) is a thousand
	std::vector<int> nearCodes = judgeFrames(w, "Port170Hub", dozer, { 560, 500 }, 8, false);
	INFO("codes within the buildout range: " << codesToString(nearCodes));
	CHECK(allEqual(nearCodes));
	CHECK(nearCodes.front() == (int)LBC_OK);

	std::vector<int> farCodes = judgeFrames(w, "Port170Hub", dozer, { 1500, 1500 }, 8, false);
	INFO("codes beyond the buildout range: " << codesToString(farCodes));
	CHECK(allEqual(farCodes));
	CHECK(farCodes.front() == (int)LBC_BEYOND_BUILDOUT);

	// the code 9 refusal is not the illegal red: PlacementIcons' mapping (RW 0x6A3A0B .. 0x6A3A26) keeps its own blue tint and the plain bib, and the frame &
	// 4 pulse alternates the blue's brightness only (never the code)
	for (int i = 0; i < 8; ++i)
	{
		w.renderFrame(1, true);
		CHECK(w.input->ui().placeLegalCode() == (int)LBC_BEYOND_BUILDOUT);
	}

	// an ordinary structure with the same site is unaffected: the range belongs to the WALL_HUB kind and its module row, not to the position
	const std::vector<int> plain = judgeFrames(w, "Port170Build", dozer, { 1500, 1500 }, 8, false);
	INFO("codes of a plain building beyond the range: " << codesToString(plain));
	CHECK(plain.front() == (int)LBC_OK);
}
