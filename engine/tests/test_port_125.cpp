// OpenBFME. GPL-3.0. Port 125 (P-52, issue #309) and P-101: the terrain decals (terrain, water, rivers, roads, scorches, tracks) draw below every particle,
// as RotWK's render (RW 0x4E2E00: roads 0x4E2ECD -> scorches 0x4E2FD0 -> tracks 0x4E3003 -> particles 0x4E302F). In Godot's transparent pass a lower
// render_priority draws first. Godot accepts only -128 .. 127 (Material::set_render_priority, scene/resources/material.cpp:71: anything else is refused and
// the priority stays 0; checked with Godot 4.7.2). Port 125 set -150 .. -145: Godot kept 0, so roads and scorches drew over smoke and explosions (P-101).
// The test reads the engine's own constants (ParticleDraw.h), which GodotMapTerrain.cpp and OrderForBatch use.

#include "doctest.h"

#include "GameClient/ParticleDraw.h"

using namespace FXParticleSystem;

TEST_SUITE_BEGIN("PORT-125");

TEST_CASE("PORT-125 / P-101: the terrain decal priorities are the six lowest Godot accepts, in RotWK's order, and the particles start right above them")
{
	CHECK(kRenderPriorityMin == -128); // Godot's RENDER_PRIORITY_MIN (scene/resources/material.h)
	CHECK(kTerrainRenderPriority == -128); // the symptom: Port 125's -150 is refused by Godot and stays 0
	CHECK(kWaterRenderPriority == -127);
	CHECK(kRiverRenderPriority == -126);
	CHECK(kRoadRenderPriority == -125);   // RW 0x4E2ECD
	CHECK(kScorchRenderPriority == -124); // RW 0x4E2FD0
	CHECK(kTrackRenderPriority == -123);  // RW 0x4E3003
	CHECK(kParticleRenderPriorityMin == -122); // RW 0x4E302F: the particles after the tracks
	CHECK(kParticleRenderPriorityMax == -9);
}

TEST_CASE("PORT-125 / P-101: the first particle batch takes -122 and the last of 114 takes -9; more batches share the top bucket (OrderForBatch)")
{
	CHECK(OrderForBatch(0, 1).priority == -122);
	CHECK(OrderForBatch(0, 114).priority == -122);
	CHECK(OrderForBatch(113, 114).priority == -9);
	CHECK(OrderForBatch(0, 1000).priority == -122);
	CHECK(OrderForBatch(999, 1000).priority == -9);
}

TEST_SUITE_END();
