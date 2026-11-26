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

	PortLineBuild()
	{
		TheCommandStore = &commands;
		w.logic->settings().buildRulesLoaded = true;
		w.logic->settings().maxLineBuildObjects = 50;
		w.ai->newMap(w.terrain); // the Pf cells the release's terrain test reads (MoveWork::buildMap RW 0x5E2E9C)
		Object *h = w.make("TestWallHub321");
		REQUIRE(h != nullptr);
		const Coord3D hubPos{100.0f, 100.0f, 0.0f};
		h->setPosition(&hubPos);
		hub = h;
		REQUIRE(hub->isKindOfName("WALL_HUB"));
		REQUIRE(w.ai != nullptr);
		REQUIRE(w.ai->pathfinder().terrainView() != nullptr);
		REQUIRE(w.logic->aiWorld() != nullptr);
		stream.attachTranslator(&place, 30); // the placement translator (HudInput's, priority 30)
	}

	~PortLineBuild()
	{
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

	void frameWithPointer(const ICoord2D &p)
	{
		place.setLivePointer(p);
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

TEST_CASE("PORT-321: the release of the line build over a restricted area is refused with RW's code 8")
{
	PortLineBuild w;
	const ICoord2D start{100, 100};
	markRestricted(w.w, 150.0f, 100.0f); // the line crosses the restricted cells around (150, 100)
	w.ui.placeBuildAvailable("TestWallHub321", w.hub->getID());
	w.anchorAt(start);
	w.releaseAt(start); // the release drops the anchor; the line build starts on the next frame
	w.frameWithPointer({200, 100});
	REQUIRE(w.ui.isLineBuildStarted());
	REQUIRE(!w.place.ghostSegments().empty());
	w.anchorAt({150, 100});
	w.releaseAt({150, 100}); // restricted: the release of RW 0x83E874 refuses
	CHECK(w.place.stats().refused == 1);
	CHECK(w.place.stats().wallSpans == 0); // the span is not built
	CHECK(w.ui.isPlacing());               // RW 0x83EB17: the mode stays for the failure
	w.endBuild();
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
