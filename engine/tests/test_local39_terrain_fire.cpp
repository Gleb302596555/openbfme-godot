// OpenBFME unit tests: Local 39 (P-38, burning grass looks pale). GPL-3.0.
//
// RotWK emits a GPU_TERRAINFIRE system's particles through the same burst gate as every other system: ParticleSystem::emit (RW 0x5F3CCF) has no
// type test; when the BurstDelay count is 0 it draws BurstCount and calls the storage's create slot +0x24 (RW 0x5F3DC1) with that count, and the
// terrain fire storage's slot (vtable RW 0xC33BA8 + 0x24 = RW 0x7B13A5) walks the burning cells `count` times (RW 0x7B1458 .. 0x7B17AF). The storage
// update (slot +0x28, RW 0x7B12F4, from finish RW 0x5F333D) only expires. The port walked the cells once on every update instead: Ennorath's
// GpuTerrainFire (BurstDelay 2, BurstCount 2) made 3/2 of retail's flames and GpuTerrainSmoke (BurstDelay 60, BurstCount 1) 61 times retail's smoke,
// a blanket of brown-to-black alpha puffs over the additive fire.
//
// The draw itself was already retail's and is checked here alongside: gpuparticle.fxo technique Default, SrcBlend / DestBlend preshaders map
// ShaderType 1 (ADDITIVE, the default without a Shader line) to ONE / ONE; ColorKeys come from the Color module unscaled (RW 0x5F6795 .. 0x5F67D8) and
// ColorScale / 255 (colour module RW 0x96A59E .. 0x96A5D3, read by RW 0x5F4A36 -> vslot +0x18 RW 0x7AC876).
// The INI values are those of Ennorath 1.9.8 fxparticlesystem.ini blocks GpuTerrainFire / GpuTerrainSmoke (fire.ini Fire block names them).

#include "FxTestUtil.h"
#include "GameClient/ParticleDraw.h"

using namespace FXParticleSystem;
using namespace fxtest;

namespace
{
std::string terrainSystem(const char *name, const char *systemExtra, const char *burst, const char *colour)
{
	std::string s = "FXParticleSystem ";
	s += name;
	s += "\n  System\n    Priority = ALWAYS_RENDER\n";
	s += systemExtra;
	s += "    Type = GPU_TERRAINFIRE\n    ParticleName = EXFireScroll2.tga\n    Lifetime = 150 150\n    Size = 5 20\n";
	s += burst;
	s += "  End\n";
	s += colour;
	s += "  EmissionVolume = TerrainFireEmission\n    Xoffset = 0 0\n    Yoffset = 0 0\n    Zoffset = 3 3\n    CellEmissionChance = 1.0\n  End\n"
		 "  EmissionVelocity = OrthoEmissionVelocity\n    Z = 1 1\n  End\n"
		 "  Draw = GpuDraw\n    FramesPerRow = 4\n    TotalFrames = 16\n    SpeedMultiplier = 0.7\n  End\nEnd\n";
	return s;
}
} // namespace

TEST_CASE("LOCAL-39 terrain fire: GpuTerrainFire's BurstDelay 2 / BurstCount 2 gate the cell walk, the batch is additive with the INI colour keys")
{
	Sim sim(terrainSystem("GpuTerrainFire", "", "    BurstDelay = 2 2\n    BurstCount = 2 2\n",
		"  Color = DefaultColor\n    Color2 = R:98 G:69 B:51 5\n    Color3 = R:90 G:56 B:37 10\n    Color4 = R:0 G:0 B:0 25\n    ColorScale = -10 10\n  End\n"));
	sim.env.ground = 10.0f;
	sim.env.burning.insert({ 100, 200 });
	const ParticleSystemID id = sim.spawn("GpuTerrainFire");

	// RW 0x5F3CCF: update 1 bursts (delay left 0): 2 rounds over the one cell (RW 0x7B1458 .. 0x7B17AF), CellEmissionChance 1
	sim.step();
	CHECK(sim.particles(id).size() == 2u);
	// the delay counts 2 -> 1 -> 0 over updates 2 and 3: no walk (the port walked every update: 3, 4)
	sim.step();
	CHECK(sim.particles(id).size() == 2u);
	sim.step();
	CHECK(sim.particles(id).size() == 2u);
	// update 4 bursts again
	sim.step();
	CHECK(sim.particles(id).size() == 4u);
	for (const Particle *p : sim.particles(id))
	{
		CHECK(p->pos.x == 100.0f);
		CHECK(p->pos.y == 200.0f);
		CHECK(p->pos.z == 13.0f);
	}

	// the burning cell's draw: one GPU batch, ShaderType ADDITIVE (gpuparticle.fxo Default: ONE / ONE), the Color module's keys in 0..1 and frames,
	// ColorScale -10 .. 10 in 1/255 units
	ParticleDrawBuilder builder(sim.env.rng);
	DrawCamera cam;
	const std::vector<DrawBatch> batches = builder.build(*sim.mgr, cam);
	REQUIRE(batches.size() == 1u);
	const DrawBatch &b = batches[0];
	CHECK(b.kind == DrawBatch::GPU);
	CHECK(b.gpu.size() == 4u);
	CHECK(b.gpuParams.shaderType == PARTICLE_SHADER_ADDITIVE);
	CHECK(b.gpuParams.colorKeys[0][0] == 0.0f); // Color1 not written: black at frame 0
	CHECK(b.gpuParams.colorKeys[1][0] == doctest::Approx(98.0f / 255.0f)); // tolerance: INI R:98 parsed as 98 / 255 in float
	CHECK(b.gpuParams.colorKeys[1][1] == doctest::Approx(69.0f / 255.0f)); // tolerance: as above
	CHECK(b.gpuParams.colorKeys[1][2] == doctest::Approx(51.0f / 255.0f)); // tolerance: as above
	CHECK(b.gpuParams.timeKeys[1] == 5.0f);
	CHECK(b.gpuParams.timeKeys[2] == 10.0f);
	CHECK(b.gpuParams.timeKeys[3] == 25.0f);
	CHECK(b.gpuParams.colorScale[0] == doctest::Approx(-10.0f / 255.0f)); // tolerance: -10 * 0.003921568859f (RW 0xBD1920)
	CHECK(b.gpuParams.colorScale[1] == doctest::Approx(10.0f / 255.0f)); // tolerance: as above
}

TEST_CASE("LOCAL-39 terrain smoke: GpuTerrainSmoke's BurstDelay 60 / BurstCount 1 walks the cells once every 61 updates")
{
	Sim sim(terrainSystem("GpuTerrainSmoke", "    Shader = ALPHA\n", "    BurstDelay = 60 60\n    BurstCount = 1 1\n",
		"  Color = DefaultColor\n    Color1 = R:153 G:82 B:2 0\n    Color2 = R:129 G:86 B:48 20\n    Color3 = R:0 G:0 B:0 80\n    Color4 = R:0 G:0 B:0 150\n  End\n"
		"  Alpha = DefaultAlpha\n    Alpha2 = 0.25 0.25 20\n    Alpha3 = 0.25 0.25 80\n    Alpha4 = 0 0 150\n  End\n"));
	sim.env.ground = 10.0f;
	sim.env.burning.insert({ 100, 200 });
	sim.env.burning.insert({ 110, 200 });
	const ParticleSystemID id = sim.spawn("GpuTerrainSmoke");

	sim.step();
	CHECK(sim.particles(id).size() == 2u); // one round: one puff per cell
	for (int i = 0; i < 60; ++i)
	{
		sim.step(); // the delay counts 60 -> 0 (the port walked every update: 122 puffs by now)
	}
	CHECK(sim.particles(id).size() == 2u);
	sim.step(); // update 62 bursts
	CHECK(sim.particles(id).size() == 4u);

	ParticleDrawBuilder builder(sim.env.rng);
	DrawCamera cam;
	const std::vector<DrawBatch> batches = builder.build(*sim.mgr, cam);
	REQUIRE(batches.size() == 1u);
	CHECK(batches[0].gpuParams.shaderType == PARTICLE_SHADER_ALPHA);
	CHECK(batches[0].gpuParams.colorKeys[1][3] == 0.25f); // Alpha2
}
