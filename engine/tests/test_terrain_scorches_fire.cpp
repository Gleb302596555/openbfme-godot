// OpenBFME unit tests: the fire manager's scorch marks (LiveFX::fireScorchUpdate, RotWK W3DFire update RW 0x49829C; TOP20 "Блок Fire, подпалины").
// Every expected value is read off the RotWK body (tools/mod/rw.py RotWK\game.dat dis 0x49829C 140), not off the port.

#include "doctest.h"

#include "GameClient/FireSetting.h"
#include "GameClient/LiveFX.h"
#include "GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DDrawServices.h"
#include "GameEngineDevice/W3DDevice/GameClient/TerrainScorches.h"

#include <cstdint>
#include <set>
#include <utility>
#include <vector>

namespace
{
// the client stream with scripted answers: real(lo, hi) returns the next scripted value and records (lo, hi)
class ScriptedRandom : public W3DDrawRandom
{
public:
	std::vector<float> answers;
	std::vector<std::pair<float, float>> calls;
	size_t next = 0;
	int value(int lo, int) override { return lo; }
	float real(float lo, float hi) override
	{
		calls.emplace_back(lo, hi);
		return next < answers.size() ? answers[next++] : lo;
	}
};

} // namespace

TEST_CASE("terrain scorches fire: the fire manager's scorches of the burning cells (RW 0x49829C)")
{
	FireSetting fire;
	fire.enableScorches = true;
	fire.scorchFrequency.setRange(5.0f, 5.0f, GameClientRandomVariable::CONSTANT); // low == high: no draw (RW 0x6D343B)
	fire.scorchSize.setRange(8.0f, 12.0f, GameClientRandomVariable::UNIFORM);
	fire.scorchIntensity = 0.5f;
	const std::set<std::pair<int, int>> cells = { { 15, 25 }, { 35, 45 } };
	auto ground = [](float x, float y) { return x + y; };
	TerrainScorchBuffer buf;
	std::uint32_t next = 0;
	ScriptedRandom rng;
	// cell (15, 25): chance 0.3 < 0.5 adds; then the y offset (2), the x offset (-3), the size (9); cell (35, 45): chance 0.7 does not add
	rng.answers = { 0.3f, 2.0f, -3.0f, 9.0f, 0.7f };
	CHECK(LiveFX::fireScorchUpdate(fire, 2, 1, next, cells, rng, 0, ground, buf) == 1);
	CHECK(next == 6); // RW 0x49830D: the frame plus (int) ScorchFrequency
	// RW 0x498352 (0, 1), RW 0x4983BC / 0x4983E5 (-10, 10) twice, RW 0x49840C ScorchSize, then the next cell's (0, 1)
	const std::vector<std::pair<float, float>> order = { { 0.0f, 1.0f }, { -10.0f, 10.0f }, { -10.0f, 10.0f }, { 8.0f, 12.0f }, { 0.0f, 1.0f } };
	CHECK(rng.calls == order);
	REQUIRE(buf.numScorches() == 1);
	CHECK(buf.scorch(0).location.x == 12.0f); // 15 + the second offset
	CHECK(buf.scorch(0).location.y == 27.0f); // 25 + the first offset
	CHECK(buf.scorch(0).location.z == 41.0f); // ground(15, 25) + 1
	CHECK(buf.scorch(0).radius == 9.0f);
	CHECK(buf.scorch(0).type == 7); // RW 0x4983FD: SCORCH_7 (GlobalData + 0x138 is not 1)
	// RW 0x4982E3: not before the client frame passes the stored one (6): frames 2 .. 6 do nothing and draw nothing
	rng.calls.clear();
	for (std::uint32_t f = 2; f <= 6; ++f)
	{
		CHECK(LiveFX::fireScorchUpdate(fire, 2, f, next, cells, rng, 0, ground, buf) == 0);
	}
	CHECK(rng.calls.empty());
	// frame 7, the map weather 1 (snowy): SCORCH_8 (RW 0x498405)
	rng.answers = { 0.1f, 0.0f, 0.0f, 10.0f, 0.9f };
	rng.next = 0;
	CHECK(LiveFX::fireScorchUpdate(fire, 2, 7, next, cells, rng, 1, ground, buf) == 1);
	CHECK(buf.scorch(1).type == 8);
	CHECK(next == 12);
	// RW 0x4982B6 / 0x4982CF: without EnableScorches, or at an effects LOD of 1, nothing is drawn
	rng.calls.clear();
	CHECK(LiveFX::fireScorchUpdate(fire, 1, 100, next, cells, rng, 0, ground, buf) == 0);
	fire.enableScorches = false;
	CHECK(LiveFX::fireScorchUpdate(fire, 2, 100, next, cells, rng, 0, ground, buf) == 0);
	CHECK(rng.calls.empty());
	CHECK(next == 12);
}

