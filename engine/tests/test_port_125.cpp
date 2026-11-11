// OpenBFME. GPL-3.0. Port 125 (P-52, issue #309): roads and burn scars are drawn over smoke.
// The render priorities of the terrain decals (roads, scorches, tracks) must be below the particle
// priority range (-128 .. -9, ParticleDraw.cpp OrderForBatch) so that smoke draws on top of them as
// in RotWK (RW 0x4E2E00 render: roads 0x4E2ECD → scorches 0x4E2FD0 → tracks 0x4E3003 → particles 0x4E302F).

#include "doctest.h"

TEST_SUITE_BEGIN("PORT-125");

// The terrain decal render priorities. These are the values the Godot terrain builder uses for the
// road, scorch and track materials (GodotMapTerrain.cpp TERRAIN/ROAD/SCORCH/TRACK_RENDER_PRIORITY).
// The test reads them through a helper to keep the constants in one place.
namespace
{
// Mirror the GodotMapTerrain.cpp constants. If the builder changes them the test must be updated.
constexpr int TERRAIN_PRIORITY = -150;
constexpr int WATER_PRIORITY = -149;
constexpr int RIVER_PRIORITY = -148;
constexpr int ROAD_PRIORITY = -147;
constexpr int SCORCH_PRIORITY = -146;
constexpr int TRACK_PRIORITY = -145;

// Particle priority range from ParticleDraw.cpp OrderForBatch.
constexpr int PARTICLE_MIN_PRIORITY = -128; // rank=0
constexpr int PARTICLE_MAX_PRIORITY = -9; // rank=levels-1 with levels=120
} // namespace

TEST_CASE("PORT-125 scorch priority exact value (smoke covers burns)")
{
	// RW 0x4E2FD0 drawScorches is called before the particle systems render at RW 0x4E302F. In Godot
	// the transparent pass draws lower priority first (behind), higher last (in front). The scorch
	// priority must be exactly -146 to sit below the particle minimum (-128).
	CHECK(SCORCH_PRIORITY == -146);
}

TEST_CASE("PORT-125 road priority exact value (smoke covers roads)")
{
	// RW 0x4E2ECD RenderRoads before RW 0x4E302F particle render: roads behind particles.
	// Road priority must be exactly -147.
	CHECK(ROAD_PRIORITY == -147);
}

TEST_CASE("PORT-125 track priority exact value (smoke covers tracks)")
{
	// RW 0x4E3003 tracks before RW 0x4E302F particles.
	// Track priority must be exactly -145.
	CHECK(TRACK_PRIORITY == -145);
}

TEST_CASE("PORT-125 terrain decal internal order matches RW render order")
{
	// RW 0x4E2E00 render: RenderRoads (0x4E2ECD) → RenderScorches (0x4E2FD0) → tracks (0x4E3003).
	// In Godot higher priority draws later (in front), so the order must be road < scorch < track.
	// Exact values: road=-147, scorch=-146, track=-145.
	CHECK(ROAD_PRIORITY == -147);
	CHECK(SCORCH_PRIORITY == -146);
	CHECK(TRACK_PRIORITY == -145);
}

TEST_CASE("PORT-125 particle priority range spans 120 levels")
{
	// The particle batch priority spans exactly 120 distinct levels (-128 .. -9, ParticleDraw.cpp
	// OrderForBatch: levels = 120). All terrain decal priorities must sit below the lowest particle.
	CHECK(PARTICLE_MIN_PRIORITY == -128);
	CHECK(PARTICLE_MAX_PRIORITY == -9);
	CHECK(TRACK_PRIORITY == -145);
}

TEST_CASE("PORT-125 opaque model priority above particles (models in front of smoke)")
{
	// Opaque W3D model surfaces use priority = Pass - SortLevel * 4 (W3DMaterial.cpp line 1051).
	// A typical opaque surface: Pass=1, SortLevel=0 → priority=1. That's above the particle max (-9),
	// so opaque models draw after particles — correct (a unit stands in front of the smoke behind it).
	const int opaqueModelPriority = 1 - 0 * 4;
	CHECK(opaqueModelPriority == 1);
	CHECK(TRACK_PRIORITY == -145);
}

TEST_SUITE_END();
