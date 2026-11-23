// OpenBFME. GPL-3.0. Synthetic BurningCell lifecycle, no game assets.
#include "doctest.h"
#include "CombatTestUtil.h"
#include "Common/StateHash.h"
#include "Common/XferLoad.h"
#include "Common/XferSave.h"
#include "GameClient/LargeGroupAudioGrid.h"
#include "GameLogic/Map/FireLogicSystem.h"
#include "GameLogic/Map/TerrainTrees.h"

namespace
{
struct BurningAudio : LargeGroupAudioSink
{
	combattest::CombatWorld world{"", 20};
	FireLogicSystem &fire = world.logic->fireLogic();
	LargeGroupAudioLink &link = world.logic->largeGroupAudio();
	LargeGroupAudioMap map;
	LargeGroupAudioSound sound;
	LargeGroupAudioGrid grid{100, 300, 300};
	std::vector<LargeGroupAudioEvent> events;
	BurningAudio()
	{
		world.frames(1);
		fire.newMap(0, 0, 100, 100);
		// One isolated, long-lived cell: no spread or decay obscures lifecycle assertions.
		fire.setDecayModifier(-4);
		fire.changeFuelInArea({50, 50, 0}, 1, 1000, 20, 4, 255, false);
		map.startThreshold = 1;
		map.stopThreshold = 1;
		sound.keys = {"BurningCell"};
		link.enable(0);
		link.setSink(this);
	}
	~BurningAudio() override { link.setSink(nullptr); }
	void onLargeGroupAudioEvent(const LargeGroupAudioEvent &e) override
	{
		events.push_back(e);
		grid.notify(map, sound, e, link.gateFrame());
	}
	void ignite()
	{
		fire.changeBurnRate(5, 5, 5, 10, false);
		for (int i = 0; i < 10; ++i)
			fire.update();
	}
	void select()
	{
		bool allocated = false;
		grid.advance(map.startThreshold, map.stopThreshold, world.logic->getFrame(), allocated);
	}
	std::vector<std::uint8_t> save()
	{
		XferSave out;
		fire.xfer(out);
		return out.data();
	}
};
} // namespace

TEST_CASE("PORT-161 burning grid cell registers anonymous audio and extinguishes without a ghost")
{
	BurningAudio b;
	b.ignite();
	REQUIRE(b.fire.cellsBurningCount() == 1);
	// RW 0x6862F3 / 0x686397 / 0x686336: temporary, node copy, temporary dtor.
	REQUIRE(b.events.size() == 3);
	CHECK(b.events[0].kind == LargeGroupAudioEvent::ADD);
	CHECK(b.events[1].kind == LargeGroupAudioEvent::ADD);
	CHECK(b.events[2].kind == LargeGroupAudioEvent::REMOVE);
	for (const auto &e : b.events)
	{
		CHECK(e.object == INVALID_ID);
		CHECK(e.weight == 1);
		REQUIRE(e.keys);
		CHECK(*e.keys == std::vector<std::string>{"BurningCell"});
		CHECK(e.x == 55);
		CHECK(e.y == 55);
		CHECK(e.storedX == e.x);
		CHECK(e.storedY == e.y);
		CHECK_FALSE(e.stealthed);
		CHECK_FALSE(e.storedStealthed);
		CHECK(e.conditions == ModelConditionMask{});
		CHECK(e.status == ObjectStatusMaskType{});
	}
	CHECK(b.grid.members == 1);
	b.select();
	CHECK(b.grid.playing.size() == 1); // without the fix: no producer, no crackle group
	b.fire.changeBurnRateAll(-100);
	CHECK_FALSE(b.fire.anyBurning());
	CHECK(b.grid.members == 0);
	b.select();
	CHECK(b.grid.playing.empty());
}

TEST_CASE("PORT-161 quiet fuel and pre-residency extinguishing do not produce burning audio")
{
	// RW 0x68715E..0x6871AB: newlyLit skips the temporary-key erase path.
	BurningAudio b;
	CHECK(b.events.empty());
	b.fire.changeBurnRate(5, 5, 5, 10, false);
	b.fire.changeBurnRateAll(-100); // newlyLit has not constructed a BurningCell yet
	for (int i = 0; i < 10; ++i)
		b.fire.update();
	CHECK(b.events.empty());
	CHECK_FALSE(b.fire.anyBurning());
	CHECK(b.grid.members == 0);
}

TEST_CASE("PORT-161 reset unregisters each resident once without a temporary lookup")
{
	BurningAudio b;
	b.ignite();
	b.events.clear();
	b.fire.reset(); // RW 0x686D3C -> resident destructors
	REQUIRE(b.events.size() == 1);
	CHECK(b.events[0].kind == LargeGroupAudioEvent::REMOVE);
	CHECK(b.grid.members == 0);
	b.fire.reset();
	CHECK(b.events.size() == 1);
}

TEST_CASE("PORT-161 saved registration frame survives replacement and keeps simulation CRC")
{
	BurningAudio b;
	b.ignite();
	const auto saved = b.save();
	REQUIRE(saved.front() == 3); // native v3 frame extension, RW field +0x10
	StateHasher before;
	b.fire.crc(before);
	b.world.frames(1);
	XferLoad load(saved);
	b.fire.xfer(load);
	CHECK(load.atEnd());
	CHECK(b.save() == saved); // includes original registration frame, not the current frame
	StateHasher after;
	b.fire.crc(after);
	CHECK(before.value() == after.value());
	CHECK(b.grid.members == 1);
	b.fire.changeBurnRateAll(-100);
	CHECK(b.grid.members == 0);
}

TEST_CASE("PORT-161 old snapshot registers only on tenth slice and does not update twice")
{
	BurningAudio b;
	b.ignite();
	auto legacy = b.save();
	REQUIRE(b.fire.cellsBurningCount() == 1);
	legacy[0] = 2;
	legacy.resize(legacy.size() - 4); // v2 had coordinates, no per-cell frame
	XferLoad load(legacy);
	b.fire.xfer(load);
	REQUIRE(load.atEnd());
	CHECK(b.grid.members == 0);
	b.events.clear();
	for (int i = 0; i < 9; ++i)
		b.fire.update();
	CHECK(b.events.empty());
	b.fire.update(); // RW 0x688CCF..0x688D02 -> 0x685E0A
	REQUIRE(b.events.size() == 1);
	CHECK(b.events.front().kind == LargeGroupAudioEvent::UPDATE);
	CHECK(static_cast<std::int32_t>(b.events.front().storedFrame) == -1);
	CHECK(b.grid.members == 1);
	for (int i = 0; i < 10; ++i)
		b.fire.update();
	CHECK(b.events.size() == 1); // RW 0x685E0A: a non--1 frame never retries
}

TEST_CASE("PORT-161 gate frame drops constructor notifications without periodic re-add")
{
	BurningAudio b;
	b.link.enable(b.world.logic->getFrame());
	b.ignite();
	CHECK(b.fire.anyBurning());
	CHECK(b.events.empty());
	CHECK(b.grid.members == 0);
	b.world.frames(1);
	for (int i = 0; i < 10; ++i)
		b.fire.update();
	CHECK(b.events.empty()); // RW 0x685E55 stamps even when global add was gated
	b.fire.changeBurnRateAll(-100);
	CHECK(b.grid.members == 0); // stored frame <= gate, only balanced temporary pair
}

TEST_CASE("PORT-161 tree fuel registers cell audio without creating a tree object")
{
	BurningAudio b;
	// Synthetic tree input through the existing optimized-tree fire registration.
	// RW 0x683F80 -> 0x686A82: template fuel enters the cell, not FlammableUpdate.
	REQUIRE(b.world.w.load("Object Port161Tree\n Flammability\n Fuel = 777\n MaxBurnRate = 10\n Decay = 4\n Resistance = 255\n End\nEnd\n").empty());
	const auto *tree = b.world.w.get("Port161Tree");
	REQUIRE(tree);
	b.fire.registerTree(123, {50, 50, 0}, tree);
	CHECK(b.fire.getCellFuel(5, 5) == 777);
	CHECK(b.events.empty()); // merely planting a tree must not crackle
	b.ignite();
	CHECK(b.fire.getCellFuel(5, 5) == 767);
	CHECK(b.grid.members == 1);
	REQUIRE_FALSE(b.events.empty());
	CHECK(b.events.front().object == INVALID_ID);
	b.fire.changeBurnRateAll(-100);
	CHECK(b.grid.members == 0);
	b.fire.unregisterTree(123, {50, 50, 0});
}

TEST_CASE("PORT-161 extinguishing a cell preserves another cell and an object contribution")
{
	BurningAudio b;
	// Real object members and anonymous cell members share the position/weight
	// consumer (RW 0x971326 / 0x971392), not an INVALID_ID-indexed registry.
	LargeGroupAudioEvent object;
	const std::vector<std::string> keys{"BurningCell"};
	object.keys = &keys;
	object.object = 17;
	object.frame = b.world.logic->getFrame();
	object.x = object.storedX = 85;
	object.y = object.storedY = 85;
	b.link.notify(object);
	b.ignite();
	b.fire.changeBurnRate(7, 7, 5, 10, false);
	for (int i = 0; i < 10; ++i)
		b.fire.update();
	REQUIRE(b.fire.cellsBurningCount() == 2);
	CHECK(b.grid.members == 3);
	b.fire.changeBurnRate(5, 5, 5, -100, false);
	CHECK(b.fire.cellsBurningCount() == 1);
	CHECK(b.grid.members == 2);
	b.fire.changeBurnRateAll(-100);
	CHECK(b.grid.members == 1);
	object.kind = LargeGroupAudioEvent::REMOVE;
	object.storedFrame = object.frame;
	b.link.notify(object);
	CHECK(b.grid.members == 0);
}
