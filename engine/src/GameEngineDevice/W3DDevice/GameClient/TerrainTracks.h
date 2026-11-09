// OpenBFME. GPL-3.0.
// Derived from Command & Conquer Generals Zero Hour, (c) 2001-2003 Electronic Arts Inc., GPL-3.0.
//
// TerrainTracks: vehicle / siege / wagon ground track marks (ZH / RotWK W3DTerrainTracks.cpp:
// TerrainTracksRenderObjClass and TerrainTracksRenderObjClassSystem, port of issue #183 / PORT-69).
// Client / draw only: never read by the simulation, never hashed, never saved in xfer.
//
// TARGET FACTS (RotWK game.dat, read with tools/mod/rw.py; caveat S-001):
//   * GlobalData (RW 0xDE4364) settings:
//       - MakeTrackMarks   (+0x13C, row RW 0xBFFAA0, parseBool, ctor RW 0x642C51: false)
//       - MaxTerrainTracks (+0x10C, row RW 0xBFFA70, parseInt,  ctor RW 0x642C07: 0)
//       - MaxTankTrackEdges (+0x110 = 100), MaxTankTrackOpaqueEdges (+0x114 = 25), MaxTankTrackFadeDelay (+0x118 = 300000 ms),
//         all populated by GameLODManager (StaticLODState).
//   * TerrainTracksRenderObjClassSystem (RW 0xDC7A54, size 0x28):
//       - init (RW 0x484662, called from W3DTerrainVisual::load RW 0x491543): allocates MaxTerrainTracks pooled track modules.
//       - Reset (RW 0x483963, called from W3DTerrainVisual::reset RW 0x4916B2): releases every active module back to free list.
//       - setDetail (RW 0x4839B6, virtual slot 23 of RW 0xBDDFEC, called at RW 0x4919EA): clears tracks and updates MaxTankTrack* limits.
//       - update (RW 0x483870, called from W3DGameClient::update RW 0x44B90C when not frozen): clears haveAnchor when !MakeTrackMarks,
//         linearly fades edge alpha over MaxTankTrackFadeDelay against the WW3D::Sync millisecond clock [RW 0xDD1E0C], expires zero-alpha
//         edges from bottomIndex, and releases unbound empty modules back to free list.
//       - bindTrack (RW 0x484566): acquires a free module, resolves left/right bone offsets (RW 0x484479) on the render prototype's
//         base pose scaled by instance scale (width = dist(left, right) + 4.0f, RW 0xBD88C0; fallback 14.0f, RW 0xBDD368 when either
//         bone index is 0 / missing), and initializes the track (RW 0x483B67).
//       - unbindTrack (RW 0x483663, called from ~W3DScriptedModelDraw RW 0x4C7126): detaches owner and marks bound = false so remaining
//         edges keep fading out until activeEdgeCount reaches 0.
//       - flush (RW 0x48470D, called from FlatHeightMapRenderObjClass::Render RW 0x4E3108 after RenderScorches RW 0x4E2FED): emits
//         (0.5 * terrainDiffuse[0] + terrainAmbient[0]) * 255 without terrainLightScale or overbright (verified against S-033), fades
//         the newest edge alpha to 0 and tail edges older than MaxTankTrackOpaqueEdges over (MaxTankTrackEdges - MaxTankTrackOpaqueEdges),
//         and draws triangle strips with _PresetAlphaShader (RW 0xD9B2F0).
//   * Draw callers:
//       - W3DScriptedModelDraw::setModelState (RW 0x4C4E41 .. 0x4C4ED5) & W3DHordeModelDraw::setModelState (RW 0x47A7B3 .. 0x47A846):
//         binds once per draw module when model exists, MakeTrackMarks != 0, m_track == nullptr, and TrackMarks is non-empty (length = 10.0f).
//       - W3DScriptedModelDraw::setHidden (RW 0x4B672A .. 0x4B6752): calls addCapEdgeToTrack on hide.
//       - W3DScriptedModelDraw::doDrawModule (RW 0x4B79BB .. 0x4B7A1B): calls addCapEdgeToTrack when m_fullyObscured (+0x49); else sets
//         airborne = true (+0x131C) when obj->isAboveTerrain() (RW 0x70C55D), then calls addEdgeToTrack.
//
// DONOR: ZH W3DTerrainTracks.cpp, BFME2 decomp W3DTerrainTracksBind.cpp / W3DTerrainTracksFlush.cpp.
// INFERENCE:
//   * TerrainTracksSystem::update snapshots activeEdgeCount before walking the ring so multiple simultaneously expired edges (e.g. a
//     large dt jump) all clear in one update rather than stranding half the ring per tick as retail's mutating loop condition does.
//   * Local bone endpoints are reset to (0, 0, 0) in clear() so a recycled module whose new unit lacks bones never reuses stale offsets.

#pragma once

#include "Common/INIDataTypes.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

struct RenderObjPrototype;
struct TimeOfDayLights;

struct TerrainTracksConfig
{
	bool makeTrackMarks = false;      ///< GlobalData + 0x13C (MakeTrackMarks)
	int maxTerrainTracks = 0;         ///< GlobalData + 0x10C (MaxTerrainTracks)
	int maxTankTrackEdges = 100;      ///< GlobalData + 0x110 (MaxTankTrackEdges)
	int maxTankTrackOpaqueEdges = 25; ///< GlobalData + 0x114 (MaxTankTrackOpaqueEdges)
	int maxTankTrackFadeDelay = 300000; ///< GlobalData + 0x118 (MaxTankTrackFadeDelay, ms)
};

// Terrain light 0 read by flush() (RW 0x48473F .. 0x4847B2): no terrainLightScale and no overbright.
struct TerrainTracksShade
{
	float ambient[3] = { 0.0f, 0.0f, 0.0f }; ///< +0x8D8
	float diffuse[3] = { 0.0f, 0.0f, 0.0f }; ///< +0x8FC
};

struct TerrainTracksHeightSampler
{
	// TheTerrainLogic vtable +0x18 (getGroundHeight): ground height at (x, y) and optional smoothed unit normal.
	std::function<float(float x, float y, Coord3D *normal)> groundHeight;
	// TheTerrainLogic vtable +0x1C (getLayerHeight): height of non-ground layer at (x, y) and optional normal; falls back to groundHeight if unset.
	std::function<float(float x, float y, int layer, Coord3D *normal)> layerHeight;
};

class TerrainTrack
{
public:
	static constexpr float DEFAULT_TRACK_SPACING = 14.0f;        ///< RW 0xBDD368 (MAP_XY_FACTOR * 1.4f)
	static constexpr float DEFAULT_TRACK_WIDTH = 4.0f;           ///< RW 0xBD88C0
	static constexpr float DEFAULT_TRACK_SEGMENT_LENGTH = 10.0f; ///< RW 0xBD83D8
	static constexpr float BRIDGE_OFFSET_FACTOR = 0.25f;         ///< RW 0xBD1904
	static constexpr float HEIGHT_ABOVE_TERRAIN = 2.0f;          ///< RW 0xBD889C (0.2f * MAP_XY_FACTOR)
	static constexpr int LAYER_GROUND = 1;                       ///< PathfindLayerEnum::LAYER_GROUND

	struct Edge
	{
		Coord3D endPointPos[2];
		float endPointUV[2][2] = { { 0.0f, 0.0f }, { 0.0f, 0.0f } };
		std::int32_t timeAdded = 0;
		float alpha = 0.0f;
	};

	TerrainTrack() = default;

	// RW 0x48363E
	void clear();
	// RW 0x483B67
	void init(float width, float length, const std::string &textureName, int maxEdges);
	void resizeEdges(int maxEdges);

	// RW 0x483BD2
	bool addCapEdgeToTrack(float x, float y, int layer, const TerrainTracksHeightSampler &sampler, std::int32_t syncTimeMs, int maxEdges);
	// RW 0x483EEB
	bool addEdgeToTrack(float x, float y, const float basis[9], const Coord3D &pos, int layer, const TerrainTracksHeightSampler &sampler,
		std::int32_t syncTimeMs, int maxEdges);

	const std::string &textureName() const { return m_textureName; }
	const Coord3D *localEndPoints() const { return m_endPoints; }
	void setLocalEndPoints(const Coord3D &left, const Coord3D &right)
	{
		m_endPoints[0] = left;
		m_endPoints[1] = right;
	}
	int activeEdgeCount() const { return m_activeEdgeCount; }
	int totalEdgesAdded() const { return m_totalEdgesAdded; }
	int bottomIndex() const { return m_bottomIndex; }
	int topIndex() const { return m_topIndex; }
	bool haveAnchor() const { return m_haveAnchor; }
	void setHaveAnchor(bool v) { m_haveAnchor = v; }
	bool isBound() const { return m_bound; }
	void setBound(bool v) { m_bound = v; }
	bool isOwnerAlive() const { return m_ownerAlive; }
	void setOwnerAlive(bool v) { m_ownerAlive = v; }
	float width() const { return m_width; }
	float length() const { return m_length; }
	bool airborne() const { return m_airborne; }
	void setAirborne(bool v) { m_airborne = v; }
	bool haveCap() const { return m_haveCap; }
	const Coord3D &lastAnchor() const { return m_lastAnchor; }
	const std::vector<Edge> &edges() const { return m_edges; }
	std::vector<Edge> &edgesMutable() { return m_edges; }
	void setBottomAndCount(int bottom, int count)
	{
		m_bottomIndex = bottom;
		m_activeEdgeCount = count;
	}

private:
	Coord3D m_endPoints[2];                        ///< +0x08 / +0x14: local bone positions
	std::string m_textureName;                     ///< +0x2C
	int m_activeEdgeCount = 0;                     ///< +0x30
	int m_totalEdgesAdded = 0;                     ///< +0x34
	bool m_ownerAlive = false;                     ///< +0x38 != nullptr
	std::vector<Edge> m_edges;                     ///< +0x3C (ring capacity = max(1, maxTankTrackEdges))
	Coord3D m_lastAnchor{ 0.0f, 1.0f, 2.25f };     ///< +0x12FC (RW 0x483AA1)
	int m_bottomIndex = 0;                         ///< +0x1308
	int m_topIndex = 0;                            ///< +0x130C
	bool m_haveAnchor = false;                     ///< +0x1310
	bool m_bound = false;                          ///< +0x1311
	float m_width = 0.0f;                          ///< +0x1314
	float m_length = 0.0f;                         ///< +0x1318
	bool m_airborne = false;                       ///< +0x131C
	bool m_haveCap = true;                         ///< +0x131D
};

class TerrainTracksSystem
{
public:
	struct Vertex
	{
		float x = 0.0f, y = 0.0f, z = 0.0f;
		std::uint32_t diffuse = 0; ///< 0xAARRGGBB
		float u = 0.0f, v = 0.0f;
	};

	struct FlushBatch
	{
		std::string textureName;
		std::vector<Vertex> vertices;
		std::vector<std::uint16_t> indices;
	};

	struct Stats
	{
		int poolSize = 0;
		int usedTracks = 0;
		int freeTracks = 0;
		int boundTracks = 0;
		int activeEdges = 0;
		std::uint64_t totalEdgesAdded = 0;
		std::uint64_t poolExhausted = 0;
	};

	TerrainTracksSystem() = default;

	// RW 0x484662
	void init(const TerrainTracksConfig &config);
	// RW 0x483963
	void reset();
	// RW 0x4839B6
	void setDetail(int maxEdges, int maxOpaqueEdges, int fadeDelayMs);
	void setMakeTrackMarks(bool on);
	bool makeTrackMarks() const { return m_config.makeTrackMarks; }
	const TerrainTracksConfig &config() const { return m_config; }

	// RW 0x484479
	static float computeTrackSpacing(const RenderObjPrototype *proto, float scale, const std::string &leftBone, const std::string &rightBone,
		Coord3D outLocalEndpoints[2]);

	// RW 0x484566: returns track slot id (>= 0) or -1 if pool empty / invalid
	int bindTrack(const RenderObjPrototype *proto, float scale, float length, const std::string &textureName, const std::string &leftBone,
		const std::string &rightBone);
	// RW 0x483663
	void unbindTrack(int trackId);

	bool addCapEdge(int trackId, float x, float y, int layer, const TerrainTracksHeightSampler &sampler);
	bool addEdge(int trackId, float x, float y, const float basis[9], const Coord3D &pos, int layer, const TerrainTracksHeightSampler &sampler);
	void setAirborne(int trackId, bool airborne);

	// RW 0x483870: advances the sync ms clock by `elapsedMs`, fades and expires edges, recycles empty unbound tracks
	void update(double elapsedMs);

	// RW 0x48473F .. 0x4847B2 (returns 0x00RRGGBB; no terrainLightScale or overbright)
	static std::uint32_t trackDiffuseBase(const TerrainTracksShade &shade);
	static TerrainTracksShade shadeFrom(const TimeOfDayLights *lights);

	// RW 0x48470D .. 0x484A77: builds draw batches grouped by textureName for tracks with activeEdgeCount >= 2
	void buildFlushBatches(const TerrainTracksShade &shade, std::vector<FlushBatch> &outBatches) const;

	const TerrainTrack *track(int trackId) const;
	const std::vector<int> &usedTrackIds() const { return m_usedIds; }
	const std::vector<int> &freeTrackIds() const { return m_freeIds; }
	std::int32_t syncTimeMs() const { return m_syncTimeMs; }
	std::uint64_t revision() const { return m_revision; }
	Stats stats() const;

private:
	void releaseTrack(int trackId);

	TerrainTracksConfig m_config;
	std::vector<TerrainTrack> m_pool;
	std::vector<int> m_usedIds;
	std::vector<int> m_freeIds;
	std::int32_t m_syncTimeMs = 0;
	double m_syncAccumMs = 0.0;
	std::uint64_t m_revision = 0;
	std::uint64_t m_totalEdgesAdded = 0;
	std::uint64_t m_poolExhausted = 0;
};
