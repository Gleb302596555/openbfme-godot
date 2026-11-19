// OpenBFME unit tests. GPL-3.0.
// PORT-263 (fork issue 611): Emotions — EmotionTrackerUpdate (terror, fear, cheering, idle emotions) and BoredUpdate.
//
// Synthetic INI only (no retail / mod files). Every expectation is read from RotWK 2.01 game.dat instructions cited at the case:
//   * EmotionTrackerUpdate's own table RW 0xC6D26C (13 slots) in order: ~EmotionTrackerUpdate RW 0x8B56CF (RW 0x8B5427 re-points the four tables
//     RW 0xC6D26C / 0xC6D1B0 / 0xC6D1A0 / 0xC6D19C, destroys the + 0xA4 set RW 0x6F1FED, frees the + 0x90 vector RW 0x430170, then the UpdateModule's
//     RW 0x64A492), RW 0x8B50BE (the load fixup: + 0xC4 index to + 0x9C pointer over the + 0x90 vector's ends), RW 0x8B5492 (the class name RW 0xC0B038),
//     RW 0x8B5FD0 (xfer), RW 0x8B5498 (the typed data factory RW 0x5487EC by the same name), then the interface thunks RW 0x63F3BF / RW 0x8851E4 / RW 0x851E97.
//     RW 0x8B56EB is the + 0x20 interface's first slot (table RW 0xC6D19C, one entry) and has no caller in the class's own code: S-7507.
//   * the fear scan's order (already as in RW in main, commit 363acb9c — this lane cites and pins it, it changes nothing): RW 0x8B5B35 measures the edge
//     distance (RW 0x66352C) into [ebp - 0x2C] and RW 0x8B5B41 compares the fear radius squared
//     with it; a node past it goes to the point branch (RW 0x8B5BF7) with that same value, and only a node inside it reaches the AfraidOf /
//     AlwaysAfraidOf filters (RW 0x7640C1, RW 0x8B5B4E .. 0x8B5B6A) and canSee (RW 0x68FA3D).
//   * BoredUpdate's data (constructor RW 0x899E8C / RW 0x89A145: ScanDelayTime RW 0xD9F608, ScanDistance RW 0xBD9E90, the default BoredFilter
//     RW 0x763D11) and its update RW 0x899F9D with the countdown RW 0x8B66B4 (a neighbour case). Its six previously uncited functions are the shared
//     module plumbing (RW 0x464A09 operator delete, RW 0x4B612F the data registry's entry fixup, RW 0x899E41 the class name, RW 0x899E47 the data
//     factory, RW 0x899F81 / RW 0x89A129 the two deleting destructors): no method of the class adds anything, the header says where each is ported.

#include "doctest.h"

#include "MoveTestUtil.h"

#include "Common/RandomValue.h"
#include "Common/SpecialPower.h"
#include "Common/StateHash.h"
#include "GameLogic/Module/BoredUpdate.h"
#include "GameLogic/Module/EmotionModules.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/Object/PartitionManager.h"
#include "GameLogic/ObjectFilter.h"
#include "GameLogic/ObjectFilterMatch.h"
#include "GameLogic/System/EmotionSystem.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace
{
// EmotionNugget blocks: TAUNT (the point branch) and FEAR (what a SCARY object the tracker is afraid of requests)
const char kNuggets[] =
	"SpecialPower Port263BoredPower\n"
	"  Enum = SPECIAL_SCREECH\n"
	"  ReloadTime = 30000\n"
	"End\n"
	"EmotionNugget Port263Taunt\n"
	"  Type = TAUNT\n"
	"  Duration = 1000\n" // 5 frames
	"End\n"
	"EmotionNugget Port263Fear\n"
	"  Type = FEAR\n"
	"  Duration = 1000\n"
	"End\n";

const char kObjects[] =
	"Object Port263Scared\n"
	"  KindOf = SELECTABLE INFANTRY\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 5\n"
	"  GeometryHeight = 10\n"
	"  ShroudClearingRange = 300\n"
	"  Behavior = EmotionTrackerUpdate ModuleTag_Emotion\n"
	"    TauntAndPointDistance = 100\n"
	"    TauntAndPointUpdateDelay = 1000\n" // 5 frames: the taunt / point scan
	"    AfraidOf = NONE +Port263Scary\n"
	"    FearScanDistance = 200\n"
	"    AddEmotion = Port263Taunt\n"
	"    AddEmotion = Port263Fear\n"
	"  End\n"
	"End\n"
	"Object Port263Scary\n"
	"  KindOf = SELECTABLE MONSTER SCARY\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryHeight = 20\n"
	"  ShroudClearingRange = 300\n"
	"End\n"
	// the BoredUpdate neighbour: a troll with a special power that has no module to run it
	"Object Port263Troll\n"
	"  KindOf = SELECTABLE MONSTER\n"
	"  Body = ActiveBody ModuleTag_Body\n"
	"    MaxHealth = 100\n"
	"  End\n"
	"  Geometry = CYLINDER\n"
	"  GeometryMajorRadius = 10\n"
	"  GeometryHeight = 20\n"
	"  Behavior = BoredUpdate ModuleTag_Bored\n"
	"    ScanDelayTime = 1\n" // one frame: the countdown lets every frame through
	"    ScanDistance = 100\n"
	"    BoredFilter = NONE +Port263Troll\n"
	"    SpecialPowerTemplate = Port263BoredPower\n"
	"  End\n"
	"End\n";

// TheEmotionSystem of the fixture (the block `EmotionNugget`, RW 0x8E0D59, needs a store to look the names up in: RW 0x835C70)
struct EmotionsFixture : movetest::MoveWorld
{
	EmotionSystem emotions;
	EmotionSystem *savedEmotions = TheEmotionSystem;
	// BoredUpdate's SpecialPowerTemplate (RW 0x73B22F) is looked up in TheSpecialPowerStore, which refuses the field without a store
	SpecialPowerStore powers;
	SpecialPowerStore *savedPowers = TheSpecialPowerStore;

	// objects: false keeps the world empty when the case reads a data class's own constructor and no object
	explicit EmotionsFixture(bool objects = true)
	{
		TheEmotionSystem = &emotions;
		TheSpecialPowerStore = &powers;
		emotions.registerBlock(w.fx.env.blocks);
		w.fx.env.blocks.registerBlock("SpecialPower", [](INI *ini) { SpecialPowerStore::parseSpecialPowerDefinitionGlobal(ini); });
		loadEmotions(objects);
		buildMap();
		logic->partition().setRegion(0.0f, 0.0f, 2000.0f, 2000.0f);
	}
	~EmotionsFixture()
	{
		TheEmotionSystem = savedEmotions;
		TheSpecialPowerStore = savedPowers;
	}

	void loadEmotions(bool objects)
	{
		const std::string text = objects ? std::string(kNuggets) + kObjects : std::string(kNuggets);
		const std::string err = w.load(text, INI_LOAD_OVERWRITE, "port263.ini");
		REQUIRE_MESSAGE(err.empty(), err);
	}
	Object *at(const char *name, const char *owner, float x, float y)
	{
		Object *o = make(name, teamOf(owner));
		REQUIRE(o);
		const Coord3D p{ x, y, 0.0f };
		o->setPosition(&p);
		return o;
	}
};
} // namespace

TEST_CASE("PORT-263 EmotionTrackerUpdate: the object's nuggets are the ones its data names, in the data's order (RW 0x8B62BE -> RW 0x835C70), and the data keeps its own fields")
{
	EmotionsFixture f;
	Object *scared = f.at("Port263Scared", "Alice", 500.0f, 500.0f);
	REQUIRE(scared);
	EmotionTrackerUpdate *t = EmotionTrackerUpdate::of(*scared);
	REQUIRE(t);
	// RW 0x8B6391 .. RW 0x8B644F: every AddEmotion name is looked up in TheEmotionSystem (RW 0x835C70) and the nugget is made in the + 0x90 vector, in order
	REQUIRE(t->nuggets().size() == 2);
	CHECK(t->nuggets()[0]->type() == EMOTION_TAUNT);
	CHECK(t->nuggets()[1]->type() == EMOTION_FEAR);
	CHECK_FALSE(t->current()); // RW 0x8B631E: + 0x9C is null until a nugget starts
	const EmotionTrackerUpdateModuleData *d = static_cast<const EmotionTrackerUpdateModuleData *>(t->getModuleData());
	REQUIRE(d);
	CHECK(d->m_tauntAndPointDistance == 100.0f); // TaskTauntAndPointDistance (+ 8), parseReal
	CHECK(d->m_tauntAndPointUpdateDelay == 5u);  // + 0xC, parseDurationUnsignedInt: 1000 ms are 5 frames
	CHECK(d->m_fearScanDistance == 200.0f);      // + 0x24, parseReal
	CHECK(d->m_immuneToFearLevel == 5);          // + 0x30, the data's own default
	CHECK(d->m_heroScanDistance == 0.0f);        // + 0x20: not in this INI
}

TEST_CASE("PORT-263 EmotionTrackerUpdate (neighbour, no change): the edge distance (RW 0x8B5B35 -> RW 0x66352C) is compared with the fear radius (RW 0x8B5B41) before the AfraidOf filter (RW 0x7640C1 runs at RW 0x8B5B4E), one value serving the point branch too (RW 0x8B5BF7)")
{
	EmotionsFixture f;
	Object *scared = f.at("Port263Scared", "Alice", 500.0f, 500.0f);
	// the SCARY cylinder is radius 10, the tracker's object radius 5: the edge distance is the centre distance less 15 (RW 0x66352C)
	Object *near = f.at("Port263Scary", "Bob", 500.0f, 560.0f); // 60 centres -> 45 edges, inside both distances
	REQUIRE(scared);
	REQUIRE(near);
	EmotionTrackerUpdate *t = EmotionTrackerUpdate::of(*scared);
	REQUIRE(t);
	f.frames(1);
	// the FEAR nugget is applied to the object it is afraid of, with that object as the source (RW 0x8B5B7B canSee, then the nugget's canApply RW 0x8E12F5)
	REQUIRE(t->current());
	CHECK(t->current()->type() == EMOTION_FEAR);
	CHECK(t->current()->sourceID() == near->getID());
	// the point branch (RW 0x8B5BF7) measures nothing again: RW 0x8B5BFC compares [ebp - 0x24] (the taunt / point distance found so far, RW 0x8B5Axx)
	// with the same [ebp - 0x2C] the fear branch used. The target is in AfraidOf, so the pending request of that branch is POINT (RW 0x8B5C37,
	// the point filter + 0x1C is not even asked once AfraidOf passes); this fixture has no POINT nugget, so nothing runs and the object keeps FEAR
	CHECK(t->current()->type() == EMOTION_FEAR);
}

TEST_CASE("PORT-263 EmotionTrackerUpdate: a SCARY object past FearScanDistance requests nothing (RW 0x8B5B35 measures it first) and the request ends when the source leaves")
{
	EmotionsFixture f;
	Object *scared = f.at("Port263Scared", "Alice", 500.0f, 500.0f);
	Object *far = f.at("Port263Scary", "Bob", 500.0f, 760.0f); // 260 centres -> 245 edges: past FearScanDistance (200)
	REQUIRE(scared);
	REQUIRE(far);
	EmotionTrackerUpdate *t = EmotionTrackerUpdate::of(*scared);
	REQUIRE(t);
	f.frames(2);
	CHECK_FALSE(t->current());
	CHECK_FALSE(t->requested(EMOTION_FEAR));
	// the same object comes into the reach: the fear request starts on the next scan
	const Coord3D close{ 500.0f, 600.0f, 0.0f };
	far->setPosition(&close);
	f.frames(2);
	REQUIRE(t->current());
	CHECK(t->current()->type() == EMOTION_FEAR);
	CHECK(t->current()->sourceID() == far->getID());
}

TEST_CASE("PORT-263 EmotionTrackerUpdate: the destructor releases the tracker with the object (RW 0x8B56CF -> RW 0x8B5427: the four tables, the + 0xA4 set RW 0x6F1FED, the + 0x90 vector RW 0x430170, then RW 0x64A492)")
{
	EmotionsFixture f;
	Object *scared = f.at("Port263Scared", "Alice", 500.0f, 500.0f);
	Object *near = f.at("Port263Scary", "Bob", 500.0f, 560.0f);
	REQUIRE(scared);
	REQUIRE(near);
	EmotionTrackerUpdate *t = EmotionTrackerUpdate::of(*scared);
	REQUIRE(t);
	f.frames(1);
	REQUIRE(t->current()); // a running nugget the destructor's members must let go of
	const std::uint32_t h0 = f.logic->computeStateHash();
	// RW 0x698F80 / RW 0x69A89D: the object dies, its modules are released (EmotionModules.h: ~EmotionTrackerUpdate, RW 0x8B56CF). The world runs on: the
	// tracker's nuggets, their templates and the requests are gone with the object, and nothing of them is left in the state hash
	scared->kill(DAMAGE_UNRESISTABLE, 0);
	f.frames(2);
	CHECK_FALSE(f.logic->findObjectByID(near->getID()) == nullptr); // the source is untouched
	CHECK(f.logic->computeStateHash() != h0);
}

TEST_CASE("PORT-263 EmotionTrackerUpdate: the snapshot keeps the requests, the running nugget and the countdown (RW 0x8B5FD0), and a load restores them (RW 0x8B50BE's + 0xC4 to + 0x9C fixup)")
{
	EmotionsFixture f;
	Object *scared = f.at("Port263Scared", "Alice", 500.0f, 500.0f);
	Object *near = f.at("Port263Scary", "Bob", 500.0f, 560.0f);
	REQUIRE(scared);
	REQUIRE(near);
	EmotionTrackerUpdate *t = EmotionTrackerUpdate::of(*scared);
	REQUIRE(t);
	f.frames(1);
	REQUIRE(t->current());
	const int currentIndex = t->current() == t->nuggets()[0].get() ? 0 : 1;
	const ObjectID scaredID = scared->getID();

	std::string err;
	std::vector<std::uint8_t> blob;
	REQUIRE_MESSAGE(f.logic->saveSnapshot(blob, &err), err);
	const std::uint32_t savedHash = f.logic->computeStateHash();
	f.frames(1);
	REQUIRE_MESSAGE(f.logic->loadSnapshot(blob, &err), err);
	// RW 0x8B50BE: the loaded + 0xC4 index names the same nugget, so + 0x9C points into the vector again
	Object *after = f.logic->findObjectByID(scaredID);
	REQUIRE(after);
	EmotionTrackerUpdate *t2 = EmotionTrackerUpdate::of(*after);
	REQUIRE(t2);
	REQUIRE(t2->nuggets().size() == 2);
	REQUIRE(t2->current());
	CHECK((t2->current() == t2->nuggets()[0].get() ? 0 : 1) == currentIndex);
	CHECK(f.logic->computeStateHash() == savedHash);
	std::vector<std::uint8_t> roundTrip;
	REQUIRE_MESSAGE(f.logic->saveSnapshot(roundTrip, &err), err);
	CHECK(roundTrip == blob);
}

TEST_CASE("PORT-263 BoredUpdate: the data's constructor sets ScanDelayTime 5, ScanDistance 300, an invalid BoredFilter and no template (RW 0x899E8C -> RW 0xD9F608, RW 0xBD9E90, RW 0x763D11)")
{
	EmotionsFixture f(false); // no objects: the data class's own constructor is what the case reads
	BoredUpdateModuleData data;
	CHECK(data.m_scanDelayTime == 5u);                      // RW 0x899ED3: [0xD9F608] = LOGICFRAMES_PER_SECOND
	CHECK(data.m_scanDistance == 300.0f);                  // RW 0x899ED6: [0xBD9E90], exactly 300.0f
	CHECK(data.m_canScanWhileAttackingOrMoving == false);   // RW 0x899EFD: + 0x14 = 0
	CHECK(data.m_specialPowerTemplate == nullptr);          // RW 0x899EF5: + 0x18 = 0
	// RW 0x899EF0 -> RW 0x763D11: NONE with empty masks, which RW 0x762977 calls not valid
	CHECK_FALSE(ObjectFilterMatch::isValid(&data.m_boredFilter));
}

TEST_CASE("PORT-263 BoredUpdate (neighbour): the constructor draws GameLogicRandomValue(0, ScanDelayTime) (RW 0x899F1F -> RW 0x6D328E) and every countdown that reaches 0 reloads it and runs the scan (RW 0x899F9D -> RW 0x8B66B4)")
{
	EmotionsFixture f;
	f.logic->random().enableCallLog(true);
	f.logic->random().clearCallLog();
	Object *troll = f.at("Port263Troll", "Alice", 300.0f, 300.0f);
	REQUIRE(troll);
	BoredUpdate *b = dynamic_cast<BoredUpdate *>(troll->findModule("BoredUpdate"));
	REQUIRE(b);
	REQUIRE(b->data()->m_specialPowerTemplate); // the SpecialPower block of this INI (RW 0x73B22F)
	// RW 0x899F33 -> RW 0x6D328E, line 0x46: one draw, bounds 0 and the data's ScanDelayTime (1 in this INI), and + 0x20 starts at 0
	int draw = -1;
	for (const GameLogicRandom::Call &c : f.logic->random().callLog())
	{
		if (!c.real && c.file == "BoredUpdate.cpp" && c.line == 0x46)
		{
			CHECK(c.lo == 0);
			CHECK(c.hi == 1);
			draw = c.result;
		}
	}
	REQUIRE(draw != -1); // the draw of RW 0x6D328E was logged (its result is 0 or 1)
	CHECK(b->countdown() == (unsigned)draw);
	// RW 0x8B66B4 is a countdown, not a period: a countdown of 0 reloads ScanDelayTime and lets the scan through, anything else only decrements
	if (draw == 0)
	{
		f.frames(1);
		CHECK(b->scans() == 1);
	}
	else
	{
		f.frames(1);
		CHECK(b->scans() == 0);
		CHECK(b->countdown() == 0u);
		f.frames(1);
		CHECK(b->scans() == 1);
	}
	CHECK(b->countdown() == 1u); // reloaded with ScanDelayTime (RW 0x8B66B4's 0 -> reload branch)
	// from there the pattern is fixed whatever the draw was: the reload frame scans, the next one only decrements, so one scan per two frames
	const unsigned long long scans = b->scans();
	f.frames(10);
	CHECK(b->scans() == scans + 5);
	CHECK(b->countdown() == 1u);
	// the troll has no SpecialPowerModule for its power, so the scan finds no ally to use it on (RW 0x82D925 refuses)
	CHECK(b->uses() == 0);
	CHECK(BoredUpdate::stopLines().size() == 1);
}
