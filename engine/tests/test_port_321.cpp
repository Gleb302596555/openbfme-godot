// OpenBFME. GPL-3.0. PORT-321 (issue #757): the release's restricted-area test of the wall line build
// (RW 0x707141, code 8, through PlaceEventTranslator::releaseLineBuild) and the line ghost
// (RW 0x6A2D38 ff: WallSpan::ghostSegments - the plan tiles the ghost draws).
//
// Synthetic INI and synthetic objects only: the restricted cells are marked through the pathfinder's
// impassable-to-players cells (the same machinery BuildPlacement::isLocationLegalToBuild reads), the
// line build is driven through PlaceEventTranslator::translate, the ghost segments through frame().

#include "doctest.h"

#include "CombatTestUtil.h"

#include "GameClient/ControlBarCommands.h"
#include "GameClient/HudInput.h"
#include "GameClient/MessageStream/PlaceEventTranslator.h"
#include "GameClient/TacticalView.h"
#include "GameLogic/AI/AIPathfind.h"
#include "GameLogic/GameMessage.h"
#include "GameLogic/SimMath.h"
#include "GameLogic/WallSpan.h"
#include <cmath>
#include <vector>

namespace
{
const char kPort321Objects[] =
	"Object TestWallHub321\n"
	"  KindOf = STRUCTURE IMMOBILE WALL_HUB DOZER\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryMinorRadius = 10\n"
	"  GeometryHeight = 20\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 1000\n"
	"  End\n"
	"  Behavior = WallHubBehavior ModuleTag_Hub\n"
	"    SegmentTemplateName = TestWallSegment321\n"
	"    HubCapTemplateName = TestWallHubCap321\n"
	"    DefaultSegmentTemplateName = TestWallSegment321\n"
	"    BuilderRadius = 20.0\n"
	"  End\n"
	"End\n"
	"Object TestWallSegment321\n"
	"  KindOf = STRUCTURE IMMOBILE WALL_BUILDABLE DOZER\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryMinorRadius = 5\n"
	"  GeometryHeight = 20\n"
	"End\n"
	"Object TestWallHubCap321\n"
	"  KindOf = STRUCTURE IMMOBILE WALL_BUILDABLE\n"
	"  Geometry = BOX\n"
	"  GeometryMajorRadius = 5\n"
	"  GeometryMinorRadius = 5\n"
	"  GeometryHeight = 20\n"
	"End\n";

// a flat tactical view (a pixel always projects: out.x/y = the pixel, true)
struct FlatView321 : TacticalView
{
	ICoord2D size() const override { return {1280, 720}; }
	bool worldToScreen(const Coord3D &p, ICoord2D &out) const override
	{
		out = {(int)p.x, (int)p.y};
		return true;
	}
	bool screenToRay(const ICoord2D &p, Coord3D &origin, Coord3D &dir) const override
	{
		origin = {(float)p.x, (float)p.y, 100.0f};
		dir = {0.0f, 0.0f, -1.0f};
		return true;
	}
	Coord3D position() const override { return {}; }
	void lookAt(const Coord3D &) override {}
};

struct PortLineBuild
{
	combattest::CombatWorld w{kPort321Objects};
	FlatView321 view;
	InGameUI ui;
	MessageStream stream;
	HudContext ctx{*w.logic, w.ai.get(), view, ui, stream, nullptr, {}};
	PlaceEventTranslator place{ctx};
	const Object *hub = nullptr;
	CommandStore commands;
	CommandStore *savedStore = TheCommandStore;
	AIWorld *savedAi = nullptr;

	explicit PortLineBuild(bool withTerrain = true)
	{
		TheCommandStore = &commands;
		w.logic->settings().buildRulesLoaded = true;
		w.logic->settings().maxLineBuildObjects = 50;
		// the combat world builds the Pf cells the release's terrain test reads (MoveWork::buildMap RW 0x5E2E9C); a world without the pathfinder's terrain is the
		// logic without its AI world (BuildPlacement reads the terrain through GameLogic::aiWorld)
		if (!withTerrain)
		{
			savedAi = w.logic->aiWorld();
			w.logic->setAIWorld(nullptr);
		}
		Object *h = w.make("TestWallHub321");
		REQUIRE(h != nullptr);
		const Coord3D hubPos{100.0f, 100.0f, 0.0f};
		h->setPosition(&hubPos);
		hub = h;
		REQUIRE(hub->isKindOfName("WALL_HUB"));
		REQUIRE(w.ai != nullptr);
		REQUIRE((w.logic->aiWorld() != nullptr) == withTerrain);
		REQUIRE(w.ai->pathfinder().terrainView() != nullptr);
		stream.attachTranslator(&place, 30); // the placement translator (HudInput's, priority 30)
	}

	~PortLineBuild()
	{
		if (savedAi)
		{
			w.logic->setAIWorld(savedAi);
		}
		TheCommandStore = savedStore;
	}

	ClientMessage msg(int type, ICoord2D p)
	{
		ClientMessage m(type);
		m.appendPixel(p);
		return m;
	}

	void anchorAt(const ICoord2D &p)
	{
		stream.append(CMSG_RAW_MOUSE_POSITION) = msg(CMSG_RAW_MOUSE_POSITION, p);
		stream.propagate();
		stream.append(CMSG_RAW_MOUSE_LEFT_BUTTON_DOWN) = msg(CMSG_RAW_MOUSE_LEFT_BUTTON_DOWN, p);
		REQUIRE(stream.propagate().empty()); // the press is eaten (RW 0x83E570)
		REQUIRE(ui.isPlacementAnchored());
	}

	std::vector<ClientMessage> releaseAt(const ICoord2D &p)
	{
		stream.append(CMSG_RAW_MOUSE_POSITION) = msg(CMSG_RAW_MOUSE_POSITION, p);
		stream.propagate();
		stream.append(CMSG_MOUSE_LEFT_CLICK) = ClientMessage(CMSG_MOUSE_LEFT_CLICK);
		return stream.propagate();
	}

	// RW 0x6A2D26 ends the update of the frame that starts the line build (RW 0x6A2D33): the ghost is planned by the next frame, so a frame is two calls
	void frameWithPointer(const ICoord2D &p)
	{
		place.setLivePointer(p);
		place.frame();
		place.frame();
	}

	void endBuild()
	{
		stream.append(CMSG_MOUSE_RIGHT_CLICK) = ClientMessage(CMSG_MOUSE_RIGHT_CLICK);
		stream.propagate();
	}
};

// the area around the point: its pathfinder cells are the restricted area (RW 0x707141's areas)
void markRestricted(combattest::CombatWorld &w, float x, float y)
{
	for (int x2 = -20; x2 <= 20; x2 += 4)
	{
		for (int y2 = -20; y2 <= 20; y2 += 4)
		{
			const Coord3D c{x + (float)x2, y + (float)y2, 0.0f};
			PathfindCell *cell = w.ai->pathfinder().getCell(LAYER_GROUND, &c);
			if (cell)
			{
				cell->setImpassableToPlayers(true);
			}
		}
	}
}
} // namespace

// The refusal after RW 0x83E874 is RW's own refusal path (RW 0x83EA3A ff), read in the disassembly: the label of code 8 (RW 0x69CCEC: GUI:CantBuildObjectsInTheWay), ONE
// voice post (RW 0x83EAA0: message 0x7EB, the builder alone, the click as the position), ONE BadBuildingPlacementSound (RW 0x83EAC5), the anchor dropped (RW 0x83EB02),
// the mode kept (RW 0x83EB17), no MSG_WALL_HUB_CONSTRUCT_SPAN. The restricted cells are the INFERENCE substitute of RW 0x707141 (S-7661)
TEST_CASE("PORT-321: the release of the line build on a restricted point is refused: one label, one voice, one sound, the anchor dropped, no span")
{
	PortLineBuild w;
	struct Post { int msgType; ObjectID object; Coord3D position; };
	std::vector<Post> posts;
	w.ctx.playObjectVoice = [&posts](int msgType, ObjectID object, const Coord3D &position) { posts.push_back({ msgType, object, position }); };
	markRestricted(w.w, 150.0f, 100.0f);
	w.ui.placeBuildAvailable("TestWallHub321", w.hub->getID());
	w.anchorAt({100, 100});
	w.releaseAt({100, 100}); // the release drops the anchor; the line build starts on the next frame
	w.frameWithPointer({200, 100});
	REQUIRE(w.ui.isLineBuildStarted());
	REQUIRE(!w.place.ghostSegments().empty());
	const size_t messagesBefore = w.ui.messages().size();
	const size_t soundsBefore = w.ui.miscSounds().size();
	w.anchorAt({150, 100});
	w.releaseAt({150, 100});
	CHECK(w.place.stats().refused == 1);
	CHECK(w.place.stats().wallSpans == 0);
	CHECK(w.place.stats().voicesUnheard == 0);
	CHECK(w.ui.isPlacing());
	CHECK(!w.ui.isPlacementAnchored());
	CHECK(w.ui.placeLegalCode() == (int)LBC_OBJECTS_IN_THE_WAY);
	REQUIRE(w.ui.messages().size() == messagesBefore + 1);
	CHECK(w.ui.messages().back() == "GUI:CantBuildObjectsInTheWay");
	REQUIRE(w.ui.miscSounds().size() == soundsBefore + 1);
	CHECK(w.ui.miscSounds().back() == "BadBuildingPlacementSound");
	REQUIRE(posts.size() == 1);
	CHECK(posts[0].msgType == 0x7EB);
	CHECK(posts[0].object == w.hub->getID());
	CHECK(posts[0].position.x == 150.0f);
	CHECK(posts[0].position.y == 100.0f);
	w.endBuild();
}

TEST_CASE("PORT-321: without a voice callback the refusal counts the unheard voice instead of dropping it")
{
	PortLineBuild w;
	markRestricted(w.w, 150.0f, 100.0f);
	w.ui.placeBuildAvailable("TestWallHub321", w.hub->getID());
	w.anchorAt({100, 100});
	w.releaseAt({100, 100});
	w.frameWithPointer({200, 100});
	REQUIRE(w.ui.isLineBuildStarted());
	w.anchorAt({150, 100});
	w.releaseAt({150, 100});
	CHECK(w.place.stats().refused == 1);
	CHECK(w.place.stats().voicesUnheard == 1);
	w.endBuild();
}

// A world without the pathfinder's terrain cannot test the restriction: that is an error that reaches the report (no silent legal answer); the click is refused
TEST_CASE("PORT-321: the release without the pathfinder's terrain reports the error and refuses")
{
	PortLineBuild w(false);
	w.ui.placeBuildAvailable("TestWallHub321", w.hub->getID());
	w.anchorAt({100, 100});
	w.releaseAt({100, 100});
	w.frameWithPointer({200, 100});
	REQUIRE(w.ui.isLineBuildStarted());
	w.anchorAt({150, 100});
	w.releaseAt({150, 100});
	CHECK(w.place.stats().refused == 1);
	CHECK(w.place.stats().wallSpans == 0);
	bool reported = false;
	for (const std::string &e : w.w.logic->report().errors)
	{
		reported = reported || e.find("TERRAIN_RESTRICTIONS asked without a pathfinder terrain") != std::string::npos;
	}
	CHECK(reported);
	w.endBuild();
}

// RW 0x6A2D12 .. 0x6A2D26 starts the line build only for BuildAssistant slot 0x60 (RW 0x793E33): the pending template AND the source object both KindOf WALL_HUB.
// A dozer (KindOf DOZER, no WALL_HUB) with a wall hub template does not start it; the hub with a non-hub template does not either
TEST_CASE("PORT-321: the line build starts for a hub with a hub template only, never for a dozer (RW 0x793E33)")
{
	PortLineBuild w;
	Object *dozer = w.w.make("TestDozer321");
	REQUIRE(dozer != nullptr);
	w.ui.placeBuildAvailable("TestWallHub321", dozer->getID());
	w.place.frame();
	CHECK(!w.ui.isLineBuildStarted());
	CHECK(w.place.ghostSegments().empty());
	w.ui.placeBuildAvailable("TestWallSegment321", w.hub->getID());
	w.place.frame();
	CHECK(!w.ui.isLineBuildStarted());
	w.ui.placeBuildAvailable("TestWallHub321", w.hub->getID());
	w.place.frame();
	CHECK(w.ui.isLineBuildStarted()); // RW 0x6A2D26: vslot 0x100(1), the update ends there for the frame
	CHECK(w.place.ghostSegments().empty());
}

TEST_CASE("PORT-321: the release of the line build over a legal area sends MSG_WALL_HUB_CONSTRUCT_SPAN")
{
	PortLineBuild w;
	w.ui.placeBuildAvailable("TestWallHub321", w.hub->getID());
	w.anchorAt({100, 100});
	w.releaseAt({100, 100});
	w.frameWithPointer({200, 100});
	REQUIRE(w.ui.isLineBuildStarted());
	REQUIRE(!w.place.ghostSegments().empty());
	w.anchorAt({200, 100});
	w.releaseAt({200, 100});
	CHECK(w.place.stats().wallSpans == 1); // RW 0x83E93A: MSG_WALL_HUB_CONSTRUCT_SPAN
	CHECK(!w.ui.isPlacing());               // RW 0x83E6A4: the mode ends
}

TEST_CASE("PORT-321: the line ghost holds the segments the plan holds, from the tile centre toward the drag end")
{
	PortLineBuild w;
	w.ui.placeBuildAvailable("TestWallHub321", w.hub->getID());
	w.anchorAt({100, 100});
	w.releaseAt({100, 100});
	w.frameWithPointer({200, 100});
	REQUIRE(w.ui.isLineBuildStarted());
	REQUIRE(!w.place.ghostSegments().empty());
	for (const WallSpan::GhostSegment &s : w.place.ghostSegments())
	{
		CHECK(s.b.x > s.a.x); // the direction is toward the drag end (+x)
		CHECK(SimMath::length2d(SimMath::subf32(s.b.x, s.a.x), SimMath::subf32(s.b.y, s.a.y)) > 0.0f);
	}
	w.endBuild();
	w.place.frame();
	CHECK(w.place.ghostSegments().empty());
}
