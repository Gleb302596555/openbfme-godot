// OpenBFME. GPL-3.0.
// The order lines: RotWK draws the queued group orders of the selected and hovered objects as lines while the local player's order mode is not 0
// (Alt held: ORDERMODE_WAYPOINT). This is the device-free half: which polylines in which style, width and opacity, and where the node markers stand.
// The Godot layer draws them (godot/scripts/order_lines.gd).
//
// RotWK 2.01 (game.dat md5 daed3668..., caveat S-001):
//   * the mode: InGameUI + 0x8B4, set every client frame at the end of InGameUI::update (call RW 0x6A2439 -> RW 0x69B1C3) from the local player's
//     order mode (Player + 0x770): 0 -> 0, 1 -> 1, 2 -> 2, no local player -> 0.
//   * the renderer (constructor RW 0x4E03C7, made at RW 0x46C4D4 beside the terrain): four SegmentedLineClass lines (+ 4 .. + 0x10), the node model
//     SCMoveHintSml (RW 0x4E0427), textures EXLaser.tga (+ 0x14, RW 0x4E0438) and EXLaser2.tga (+ 0x18, RW 0x4E0467); every line width 1.5
//     (RW 0xBDE8C8), mapping mode 2 = TILED (RW 0x4E0527), the shader preset at RW 0xD9B2F0 (static 0x1198B3, no writer in game.dat: src blend
//     SRC_ALPHA, dst blend ONE_MINUS_SRC_ALPHA, texturing, no depth write) with depth compare 7 = always (RW 0x4E049E); the running lines' UV offset
//     rate (0.001, 0.001) per second (RW 0x4E065C .. 0x4E0675 -> RW 0x55E0F0: * 0.001 per ms):
//       style 0 (+ 4)   EXLaser,  colour A (0.96, 0.77, 0.47, RW 0xD9A524)  the queued orders;
//       style 1 (+ 8)   EXLaser,  colour B (0.98, 0.51, 0.58, RW 0xD9A530)  from the planned marker on (queue + 8);
//       style 2 (+ 0xC) EXLaser2, colour A, the texture running            from the patrol start on (queue + 0xC);
//       style 3 (+ 0x10) EXLaser2, colour B, the texture running           from the next patrol start on (queue + 0x10).
//   * the frame, RW 0x4E013B: nothing in mode 0; the pair set (+ 0x258) is emptied (RW 0x4E0170); the running lines' UV offset is (0, 1 - (ms % 1000) *
//     0.001) (RW 0x4E0175 .. 0x4E01CF, timeGetTime); in mode 1 or 2: the moused-over object (InGameUI vslot 0x174) when the local player controls it
//     (RW 0x4E02A9 .. 0x4E02B1) and it is not IGNORED_IN_GUI, its horde in its place (RW 0x693A1A(0): the object if HORDE, else its container if HORDE),
//     at width 3 (RW 0xBDD42C) and opacity 1, then width 1.5 again; every selected object (vslot 0x124, RW 0x4E00D9: IGNORED_IN_GUI skipped) at
//     opacity 1; in mode 2 also every object of the local player (RW 0x6ABABD) at opacity 0.4 (RW 0xBE0DE4).
//   * one object, RW 0x4DFE69: its queue in TheAiOrdersManager (RW 0x756C70 by the object's ID); none or empty -> nothing. The polyline starts at the
//     object's position (+ 0x38); the line is style 0; for each order ID of the list: the planned marker -> (if a patrol start was met, the patrol
//     start order's point closes the loop) flush, style 1; the patrol start -> flush, style 2; the next patrol start -> flush, style 3; then the order
//     (RW 0x756C59) gives its point through vslot 0x20; after the first order the pair (previous order, this order) is looked up in the frame's pair
//     set: met already -> flush and start over from this point (a group's shared segment is drawn once), else it is added; the point joins the
//     polyline and the node model is drawn there. At the end a polyline of two points or more is flushed.
//   * flush, RW 0x4DFDB3: with two points or more the line takes texture tile factor 4 (RW 0xBD88C0; TILED: V = point index * 4) and is drawn; all points
//     but the last are dropped (the next line goes on from there).
//   * the order's point, vslot 0x20: MoveToGroupOrder its destination (+ 0x18, RW 0x94DA6C); MoveToFormationGroupOrder its destination (+ 0x1C,
//     RW 0x94E37D); AttackObjectGroupOrder / GarrisonObjectGroupOrder the target's position, cached in the order (+ 0x20 / + 0x1C, RW 0x94D93F /
//     RW 0x94D64B), the cache when the target is gone (the constructor zeroes it, RW 0x94D7BD).
// INFERENCE (presentation): RotWK writes the target cache into the order itself while drawing; here the client keeps it (m_targetCache, seeded from the
// order's lastTargetPos), so the logic's state and hash never depend on what was drawn. A written-only global at RW 0xD9A520 (set to 0.8 before the
// selected objects, RW 0x4E0308; no reader found) is not ported.
#pragma once

#include "Common/INIDataTypes.h"
#include "GameLogic/ObjectTypes.h"

#include <cstdint>
#include <map>
#include <set>
#include <utility>
#include <vector>

class GameLogic;
class Object;

struct OrderLineStrip
{
	int style = 0;         ///< 0 .. 3, the renderer's line + 4 .. + 0x10
	float width = 1.5f;    ///< RW 0xBDE8C8, 3 for the moused-over object (RW 0xBDD42C)
	float opacity = 1.0f;  ///< 1, or 0.4 for the player's other objects in mode 2 (RW 0xBE0DE4)
	float tileFactor = 4.0f; ///< RW 0xBD88C0: V = point index * tileFactor
	std::vector<Coord3D> points;
};

struct OrderLineMarker
{
	Coord3D pos{};
	float opacity = 1.0f;
};

struct OrderLinesFrame
{
	int mode = 0;
	float runningU = 0.0f; ///< styles 2 and 3: the UV offset set each frame, (0, 1 - (ms % 1000) * 0.001) (RW 0x4E0175), then the renderer's advance by
	float runningV = 0.0f; ///< the rate times the milliseconds since its last draw, each kept to its fraction (seglinerenderer Render)
	std::vector<OrderLineStrip> strips;
	std::vector<OrderLineMarker> markers;
};

class OrderLines
{
public:
	static constexpr float kWidth = 1.5f;         // RW 0xBDE8C8
	static constexpr float kHoverWidth = 3.0f;    // RW 0xBDD42C
	static constexpr float kOthersOpacity = 0.4f; // RW 0xBE0DE4
	static constexpr float kTileFactor = 4.0f;    // RW 0xBD88C0
	static constexpr float kUVRatePerMs = 0.001f * 0.001f; // RW 0xBD88A0 (0.001 per second) * RW 0x55E105 (0.001)

	// RW 0x69B1C3: the local player's order mode (Player + 0x770) as the line mode; no local player -> 0
	static int modeFor(const GameLogic &logic);

	// RW 0x4E013B: this frame's lines. `selected` in the selection's order, `mousedOver` the object under the cursor (INVALID_ID for none),
	// `milliseconds` the system clock (timeGetTime in RotWK), `sinceLastDrawMs` the time since the last frame (WW3D sync time delta)
	void build(GameLogic &logic, const std::vector<ObjectID> &selected, ObjectID mousedOver, std::uint32_t milliseconds, OrderLinesFrame &out,
		std::uint32_t sinceLastDrawMs = 0);

	// a new game: the target cache follows the orders of the game
	void reset() { m_targetCache.clear(); }

private:
	struct Pass
	{
		OrderLinesFrame *out = nullptr;
		std::set<std::pair<std::uint32_t, std::uint32_t>> pairs; // + 0x258
		float width = kWidth;
		float opacity = 1.0f;
	};
	void drawObject(GameLogic &logic, const Object &obj, Pass &pass);             // RW 0x4DFE69
	bool orderPoint(GameLogic &logic, std::uint32_t orderId, Coord3D &out);       // vslot 0x20
	static void flush(Pass &pass, int style, std::vector<Coord3D> &points);         // RW 0x4DFDB3

	std::map<std::uint32_t, Coord3D> m_targetCache; // order ID -> the target's last position seen (RotWK: the order's + 0x20 / + 0x1C)
	const GameLogic *m_logic = nullptr;             // the game the cache belongs to
};
