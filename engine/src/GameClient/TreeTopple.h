// OpenBFME. GPL-3.0.
// Derived from Command & Conquer Generals Zero Hour, (c) 2001-2003 Electronic Arts Inc., GPL-3.0.
//
// TreeTopple (lane SPELL-3): the client side of a toppling map tree, the part of the tree buffer that makes a felled tree fall, bounce and lie (or sink).
// The logic's TerrainTrees (GameLogic/Map/TerrainTrees.h) says which tree falls and in which direction; this animates its static instance. Client only:
// nothing here reaches the simulation.
//
// DONOR (ZH GeneralsMD W3DTreeBuffer.cpp), checked against RotWK's own tree buffer (lane TREES1: TheTerrainVisual, RW 0xDE4AC8, slots 0x70 / 0x7C / 0x58
// forward to W3DTreeBuffer RW 0x4E9E4E / 0x4EA068 / 0x4E9C81; the per-frame update RW 0x4ED508 from W3DDisplay's draw, RW 0x449CF8 -> 0x4684FB):
//   * applyTopplingForce (RW 0x4E9E4E): only an upright tree; the speed is the logic's (TerrainLogic + 0x1918) when above 0, else MinimumToppleSpeed (at
//     least 0); the direction normalised; angular velocity = speed * InitialVelocityPercent, acceleration = speed * InitialAccelPercent; ToppleFX at the
//     tree's position;
//   * updateTopplingTree (RW 0x4EA107, ZH's body), once per update: the step is the velocity, cut so the total stays within ANGULAR_LIMIT = PI/2 - PI/64 (RW
//     0xBE4CC0); the tree turns about X by -step * dir.y and about Y by step * dir.x; at the limit with a positive velocity it bounces (velocity *=
//     -BounceVelocityPercent): below 0.01 it stops (down; KillWhenFinishedToppling starts the sink), at 0.03 or more BounceFX plays at the trunk's matrix
//     times (0, 0, 27) (RW 0xBE4CC4; ZH: three tree radii); otherwise the velocity grows by the acceleration;
//   * the sink (RW 0x4ED508; started by KillWhenFinishedToppling or by the logic's lumber-gone call, slot 0x7C RW 0x4EA068, which sinks the tree where it is:
//     no fall): SinkTime (logic frames) times the client frames per logic frame updates (the buffer's + 0x5BC), each lowering the tree by SinkDistance /
//     SinkTime; at 0 the tree goes. A sinking tree that still falls keeps falling in the same update;
//   * slot 0x58 (RW 0x4E9C81, the logic's construction clear): the tree goes at once.
// PORT-73 (issue #205), the push aside of the same records (the shrubs): the shrub buffer keeps a second state per record, the bend a unit walking past gives it.
//   * the record joins the area partition (and so can be found by the walk below) when its type's MoveOutwardTime is above 0 or DoTopple is set (RW 0x4E8C66
//     cmp [data+0x10], 0 / RW 0x4E8C6C cmp byte [data+0x3d], 0; addShrub RW 0x4E8B8F; DONOR BFME2 0xE91CE same two tests);
//   * a moving unit (TheTerrainVisual slot 0x6C, RW 0x6858D0, called for every object whose cell changed before the crusher tests) reaches the shrub buffer
//     RW 0x4E70F5: the global flags GlobalData + 0x1C / + 0x1D, an IMMOBILE unit (template + 0x108 bit 2) and a zero count stop it; the radius is the unit's
//     major radius (+ 0xB8) plus 7.0 (RW 0xBE4AD4); the 50 x 50 partition cells of the unit's box (bounds + 0x1948 .. + 0x1954, 49.9 at RW 0xBE4AD0) are walked
//     and a record with a type (frame + 0x40 >= 0) whose 3D squared distance is below the radius squared and whose type's MoveOutwardTime is above 0
//     (RW 0x4E73AE) is pushed;
//   * the push (RW 0x4E6C74): the record stores the pusher (+ 0x6C) and the logic frame (+ 0x70); the same pusher within three frames (RW 0x4E6CC9 cmp 3) or a
//     record already bent (push aside + 0x5C not 0) is left alone; the direction is perpendicular to the walker's heading, chosen by the sign of
//     dir.x * (tree.y - unit.y) - dir.y * (tree.x - unit.x): cos = -dir.y, sin = dir.x when it is above 0, cos = dir.y, sin = -dir.x otherwise; the rate is
//     1 / (MoveOutwardTime in logic frames * the buffer's frames per logic frame, + 0x5BC = 6, RW 0x4E6D4E .. 0x4E6D6D);
//   * once per drawn frame (the shrub draw RW 0x4E8519): push aside += rate; at 1.0 the rate becomes -1 / (MoveInwardTime * 6) (a double division,
//     RW 0xBDFC10); at 0 the rate and the push aside are cleared;
//   * the bend (RW 0x4E7B25 / RW 0x4E7B4D, the same pair in the partial update RW 0x4E7F73 / RW 0x4E7F9B): a vertex moves x by `vertex z * push aside *
//     cos * MoveOutwardDistanceFactor` and y by the same with sin, so the model leans away from the walker and is unbent the same way;
//   * NOT PORTED (docs/STOPS.md S-5260): the two GlobalData gates (the port has no GameData block of TheWritableGlobalData), the partition itself (the port
//     scans its records linearly), the shroud / visible / pause filters of the draw loop and the vertex buffers (the port bends the instance's basis);
//     DarkeningFactor (+ 0x1C) has no reader in RotWK 2.01 (see the lane report) and is not applied.
//
// INFERENCE (S-930): the updates run per drawn frame at RotWK's 30 frames per second cap (one per 1/30 s: six per 200 ms logic frame) and the drawn angle is
// interpolated between two updates; the turn is one rotation by the accumulated angle about (-dir.y, dir.x, 0) (ZH's two small turns per update, applied
// to the identity each time, compose to it); the fog pause (TOPPLE_FOGGED, RW 0x4EA107's shroud test) is not ported.

#pragma once

#include "Common/INIDataTypes.h"

#include <cstdint>
#include <string>
#include <vector>

class W3DTreeDrawModuleData;

class TreeTopple
{
public:
	enum State
	{
		UPRIGHT,
		FALLING,
		DOWN,
		GONE
	};
	struct Tree
	{
		std::uint32_t clientId = 0;
		const W3DTreeDrawModuleData *data = nullptr;
		Coord3D position{};
		State state = UPRIGHT;
		Coord3D direction{};          ///< normalised, z = 0
		float angularVelocity = 0.0f;
		float angularAcceleration = 0.0f;
		float angle = 0.0f;           ///< ZH m_angularAccumulation
		float previousAngle = 0.0f;   ///< the angle before the last update (drawn with the interpolation)
		float sink = 0.0f;            ///< how far it sank
		float previousSink = 0.0f;
		unsigned sinkFramesLeft = 0;  ///< the sink's updates left
		bool sinking = false;
		bool removed = false;         ///< the logic took its lumber
		// PORT-73 (RW 0x4E8B8F addShrub / RW 0x4E6C74 / RW 0x4E8519): the push aside of a shrub
		bool pushable = false;        ///< RW 0x4E8C66 / 0x4E8C6C: MoveOutwardTime above 0 or DoTopple (the record joins the area partition)
		float pushAside = 0.0f;       ///< + 0x5C: 0 upright, 1 bent as far as the walker pushes it
		float pushAsideDelta = 0.0f;  ///< + 0x60: the step per drawn frame (positive outward, negative back)
		float pushAsideSin = 1.0f;    ///< + 0x64
		float pushAsideCos = 1.0f;    ///< + 0x68
		std::uint32_t pushAsideSource = 0; ///< + 0x6C: the unit that pushed it
		std::uint32_t pushAsideFrame = 0;  ///< + 0x70: the logic frame of the last push
	};
	// an FX the animation asks for (ToppleFX, BounceFX)
	struct FX
	{
		std::string name;
		Coord3D position{};
	};

	static constexpr float kAngularLimit = 1.5217089f; ///< ZH ANGULAR_LIMIT = PI/2 - PI/64
	static constexpr float kUpdateSeconds = 0.0333333351f; ///< 1 / 30: one update per drawn frame at 30 fps (INFERENCE, S-930)
	static constexpr float kSinkFrameSeconds = 0.2f;      ///< a SinkTime frame is a logic frame (parseDurationUnsignedInt)
	static constexpr float kPushAsideMargin = 7.0f;      ///< RW 0xBE4AD4: the unit's radius is grown by it (RW 0x4E713C)
	static constexpr float kPushAsideFrameScale = 6.0f;  ///< the buffer's + 0x5BC: 30 drawn frames per 5 logic frames (RW 0x4E6D6D / RW 0x4E8563)
	static constexpr std::uint32_t kWalkRepeatFrames = 3; ///< RW 0x4E6CC9: the same walker pushes at most every three logic frames

	// a tree the map placed; its index is the handle. `shrub`: the map object is a SHRUB, so RotWK keeps it in the shrub buffer (RW 0x62E456 .. 0x62E470 ->
	// RW 0x6808ED -> addShrub RW 0x4E8B8F), the only buffer a walking unit bends (RW 0x4E70F5); a TREE goes to the tree buffer (RW 0x62E433 .. 0x62E44F ->
	// RW 0x683D89), which a walker never pushes aside
	size_t add(std::uint32_t clientId, const Coord3D &position, const W3DTreeDrawModuleData *data, bool shrub = false);
	// PORT-73, RW 0x4E70F5 (TheTerrainVisual slot 0x6C for a moving unit): every pushable record within (radius + 7.0) is pushed by RW 0x4E6C74;
	// returns how many it bent. `frame` is the logic frame (RW 0xDE412C + 0x40, the same clock as the three-frame rule)
	int unitMoved(std::uint32_t pusherId, const Coord3D &position, const Coord3D &direction, float majorRadius, std::uint32_t frame);
	// PORT-73: the bend of the record (0 .. 1), for the harness and the tests
	float pushAsideAt(size_t index) const { return index < m_trees.size() ? m_trees[index].pushAside : 0.0f; }
	// the logic's topple event (applyTopplingForce); false when the id is unknown or the tree is not upright
	bool topple(std::uint32_t clientId, const Coord3D &direction, float speed);
	// the logic's lumber-gone event (slot 0x7C): the tree sinks
	void remove(std::uint32_t clientId);
	// the logic's construction clear (slot 0x58): the tree goes at once
	void removeNow(std::uint32_t clientId);
	// advances by `seconds` (whole updates of kUpdateSeconds; the rest is kept for the interpolation)
	void advance(double seconds);
	// the indices of the trees that moved since the last call (their transform must be redrawn)
	std::vector<size_t> takeChanged();
	// the tree's rotation (row major 3x3) and its position offset at the current interpolation point
	void pose(size_t index, float rotation[9], Coord3D &offset) const;
	// the drawn placement: the pose applied to the tree's own placement (row major 3x3 basis, columns X, Y, Z) and position, then the push aside of PORT-73
	// sheared along the model's own up axis (`scale` is the instance's scale; RW 0x4E7B25 / RW 0x4E7B4D shear the untransformed vertex z)
	void placement(size_t index, const float baseBasis[9], const Coord3D &basePosition, float scale, float outBasis[9], Coord3D &outPosition) const;
	// a harness filter (GameWorld.get_logic_trees): the point lies within `radius` of (x, y) in 2D
	static bool within2D(const Coord3D &p, double x, double y, double radius);
	std::vector<FX> takeFX();
	const std::vector<Tree> &trees() const { return m_trees; }
	size_t animating() const;
	// S-921: a morph ended and the tree is of another type now (its topple and sink read that type's W3DTreeDraw, RW 0x4EAB0A)
	void setData(size_t index, const W3DTreeDrawModuleData *data)
	{
		if (index < m_trees.size() && data)
		{
			m_trees[index].data = data;
		}
	}

private:
	void update(Tree &t); // RW 0x4ED508's step: the sink, then updateTopplingTree
	bool pushOne(size_t index, std::uint32_t pusherId, const Coord3D &position, const Coord3D &direction, std::uint32_t frame); // RW 0x4E6C74
	void advancePushAside(Tree &t); // RW 0x4E8519: one drawn frame's step of the bend
	std::vector<Tree> m_trees;
	std::vector<size_t> m_changed;
	std::vector<FX> m_fx;
	double m_accumulator = 0.0;
};
