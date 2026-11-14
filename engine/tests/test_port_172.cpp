// OpenBFME unit tests. PORT-172 (issue #425): the skirmish AI position sampler must not hold an object
// the game no longer has. Synthetic INI, no retail files. Expectations come from the RotWK instructions
// named in each test (RW 0x8E480C, RW 0x8E46A5, RW 0x8E4475, RW 0x854712 / 0x85474D), never this
// engine's own output.

#include "doctest.h"

#include "LogicTestUtil.h"

#include "Common/INI.h"
#include "GameLogic/Object/Object.h"
#include "GameLogic/SkirmishAI/AIStatCollector.h"
#include "GameLogic/SkirmishAI/SkirmishAIData.h"
#include "GameLogic/SkirmishAI/SkirmishAIManager.h"

#include <string>
#include <vector>

using namespace logictest;

namespace
{
const char kAIData[] = "SkirmishAIData TheSkirmishAIData\n DisableWallBuilding = No\nEnd\n"
					   "ArmyDefinition AlphaArmy\n Side = Alpha\n ArmyMemberDefinition M\n  Unit = Soldier\n  PercentageOfArmyPhase1 = 100\n End\nEnd\n"
					   "AIDozerAssignment AlphaDozer\n Side = Alpha\n Unit = AlphaPorter\nEnd\n";

const char kStructures[] = "Object Port425Structure\n"
						   " KindOf = STRUCTURE IMMOBILE\n"
						   " Geometry = CYLINDER\n"
						   " GeometryMajorRadius = 10\n"
						   " GeometryHeight = 20\n"
						   " Body = ActiveBody ModuleTag_Body\n"
						   "  MaxHealth = 1000\n"
						   " End\n"
						   "End\n"
						   "Object Port425Economy\n"
						   " KindOf = STRUCTURE IMMOBILE ECONOMY_STRUCTURE\n"
						   " Geometry = CYLINDER\n"
						   " GeometryMajorRadius = 10\n"
						   " GeometryHeight = 20\n"
						   " Body = ActiveBody ModuleTag_Body\n"
						   "  MaxHealth = 1000\n"
						   " End\n"
						   "End\n";

struct Fixture : LogicWorld
{
	SkirmishAIStore store{ w.keys };
	INIEnvironment env;

	Fixture()
	{
		store.registerBlocks(env.blocks);
		INI ini(env);
		const std::string text = kAIData;
		ini.loadMemory("skirmishaidata.ini", std::vector<std::uint8_t>(text.begin(), text.end()), INI_LOAD_OVERWRITE);
		const std::string err = w.load(kStructures, INI_LOAD_OVERWRITE, "port425.ini");
		REQUIRE_MESSAGE(err.empty(), err);
		SkirmishSetup setup;
		setup.players.push_back({ "Alice", "FactionA", true, 0, 0, 0 });
		setup.players.push_back({ "Bob", "FactionB", false, 1, 0, 1 });
		setup.defaultStartingCash = 2000;
		REQUIRE(players.setupSkirmish(setup).empty());
		logic->skirmishAI().setStore(&store);
		REQUIRE(logic->skirmishAI().newGame(true) == 1); // one AI: Bob
	}
	int indexOf(const std::string &name) { return players.findPlayerWithName(name)->getPlayerIndex(); }
	void runToFrame(unsigned frame)
	{
		while (logic->getFrame() < frame)
		{
			logic->runLogicFrame();
		}
	}
};
} // namespace

TEST_CASE("PORT-172: a captured structure destroyed leaves no stale sampler entry (RW 0x854712 / 0x85474D, RW 0x8E480C)")
{
	Fixture fx;
	GameLogic &logic = *fx.logic;
	const int alice = fx.indexOf("Alice");
	const int bob = fx.indexOf("Bob");
	Team *aliceTeam = fx.teamOf("Alice");
	Team *bobTeam = fx.teamOf("Bob");
	REQUIRE(aliceTeam);
	REQUIRE(bobTeam);
	REQUIRE(logic.skirmishAI().record(alice));
	REQUIRE(logic.skirmishAI().record(bob));
	fx.runToFrame(10); // past kFirstAIFrame: the manager updates

	// A structure for Alice: registered, pending, then added to the sampler by update (RW 0x8E4591 -> RW 0x8E4456).
	Object *s = fx.make("Port425Structure", aliceTeam);
	REQUIRE(s);
	const ObjectID sid = s->getID();
	logic.runLogicFrame();
	const AIStatCollector &aliceRec = *logic.skirmishAI().record(alice);
	REQUIRE(std::find(aliceRec.positions().members().begin(), aliceRec.positions().members().end(), sid) != aliceRec.positions().members().end());

	// Capture to Bob the way RW 0x854712 / 0x85474D does: the manager removal with the old owner, the team
	// change (whose own pair runs too), and the manager add with the new one. RW's plain push_back registers
	// the building twice with the new owner; the port registers an id at most once per list (PORT-172), so the
	// capture lands exactly one sampler registration with Bob.
	logic.skirmishAI().objectRemoved(*s);
	s->setTeam(bobTeam);
	logic.skirmishAI().objectAdded(*s);
	logic.runLogicFrame(); // the deferred sampler adds run
	const AIStatCollector &bobRec = *logic.skirmishAI().record(bob);
	int bobSamplerCount = 0;
	for (ObjectID id : bobRec.positions().members())
	{
		if (id == sid)
		{
			++bobSamplerCount;
		}
	}
	REQUIRE(bobSamplerCount == 1);

	// Destroy the captured structure. The destructor's world exit (RW 0x68C205 -> RW 0x6A99CB -> RW 0x8E480C)
	// must leave the sampler without the id; a second removal (another structure) rebuilds the sampler and
	// must not query a gone object.
	logic.destroyObject(s);
	logic.runLogicFrame();
	REQUIRE(logic.findObjectByID(sid) == nullptr);
	for (ObjectID id : bobRec.positions().members())
	{
		CHECK(id != sid);
	}
	Object *other = fx.make("Port425Structure", bobTeam);
	REQUIRE(other);
	logic.runLogicFrame();
	logic.destroyObject(other);
	logic.runLogicFrame(); // rebuilds the sampler over the survivors
	REQUIRE(logic.findObjectByID(other->getID()) == nullptr);
}

TEST_CASE("PORT-172: neighbouring behaviour — a plain structure add/remove keeps the sampler in step (RW 0x8E4591 / RW 0x8E480C)")
{
	Fixture fx;
	GameLogic &logic = *fx.logic;
	const int alice = fx.indexOf("Alice");
	Team *aliceTeam = fx.teamOf("Alice");
	REQUIRE(aliceTeam);
	fx.runToFrame(10);

	Object *a = fx.make("Port425Structure", aliceTeam);
	Object *b = fx.make("Port425Economy", aliceTeam);
	REQUIRE(a);
	REQUIRE(b);
	const ObjectID aId = a->getID(), bId = b->getID();
	logic.runLogicFrame();
	const AIStatCollector &rec = *logic.skirmishAI().record(alice);
	CHECK(std::find(rec.positions().members().begin(), rec.positions().members().end(), aId) != rec.positions().members().end());
	CHECK(std::find(rec.positions().members().begin(), rec.positions().members().end(), bId) != rec.positions().members().end());
	CHECK(std::find(rec.structures().begin(), rec.structures().end(), aId) != rec.structures().end());
	CHECK(std::find(rec.economy().begin(), rec.economy().end(), bId) != rec.economy().end());

	logic.destroyObject(a);
	logic.runLogicFrame();
	CHECK(std::find(rec.positions().members().begin(), rec.positions().members().end(), aId) == rec.positions().members().end());
	CHECK(std::find(rec.positions().members().begin(), rec.positions().members().end(), bId) != rec.positions().members().end());
	CHECK(std::find(rec.structures().begin(), rec.structures().end(), aId) == rec.structures().end());

	logic.destroyObject(b);
	logic.runLogicFrame();
	CHECK(rec.positions().members().empty());
	CHECK(rec.economy().empty());
}
