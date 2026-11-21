// OpenBFME unit tests. GPL-3.0. Native save-load domains; all game data here is synthetic.
#include "doctest.h"

#include "IniTestUtil.h"
#include "MoveTestUtil.h"
#include "Common/GameState.h"
#include "Common/Snapshot.h"
#include "Common/XferLoad.h"
#include "Common/XferSave.h"
#include "GameLogic/AI/AIAttackCharge.h"
#include "GameLogic/AI/AIGroupOrders.h"
#include "GameLogic/Object/RetailObjectWorld.h"
#include "GameLogic/SkirmishAI/SkirmishAIManager.h"
#include "GameLogic/System/TaintManager.h"

namespace
{
using Bytes = std::vector<std::uint8_t>;
using doctest::Contains;

template <class F>
Bytes save(F xfer)
{
	XferSave x;
	xfer(x);
	return x.data();
}

template <class F>
void load(const Bytes &bytes, F xfer)
{
	XferLoad x(bytes);
	xfer(x);
	REQUIRE(x.atEnd());
}

// Replace just the named little-endian word, leaving all surrounding framing intact.
Bytes word(Bytes bytes, size_t at, std::uint32_t value)
{
	for (size_t i = 0; i < 4; ++i) bytes.at(at + i) = static_cast<std::uint8_t>(value >> (8 * i));
	return bytes;
}
} // namespace

TEST_CASE("PORT-284 Player v4 validates both bare KindOf masks and keeps battle plan quantities")
{
	logictest::LogicWorld w;
	auto &player = *w.players.findPlayerWithName("Alice");
	auto transfer = [&](Xfer &x) { player.xfer(x, w.teams); };
	const Bytes previous = save(transfer);
	BattlePlanBonuses bonus;
	bonus.bombardment = 7;
	bonus.holdTheLine = -1;
	bonus.searchAndDestroy = 11;
	bonus.validKindOf.back() = 1u << 29;   // last retail name, bit 221 (RW 0x6AAD1A)
	bonus.invalidKindOf.front() = 1u;
	player.setBattlePlanCounts(2, 1, 3);
	player.applyBattlePlanBonusesForPlayerObjects(bonus, *w.logic);
	const Bytes valid = save(transfer);
	REQUIRE(valid.front() == 4);
	REQUIRE(previous.front() == 2);
	load(previous, transfer);
	CHECK_FALSE(player.hasActiveBattlePlans());
	load(valid, transfer);
	CHECK(player.battlePlanBonuses().validKindOf == bonus.validKindOf);
	CHECK(player.battlePlanBonuses().holdTheLine == -1);
	CHECK(save(transfer) == valid);

	// v4 ends with two 7-word masks: no size prefix. Bit 222 is not in retail's table.
	const size_t invalidLast = valid.size() - 4, validLast = invalidLast - 7 * 4;
	CHECK_THROWS_WITH_AS(load(word(valid, validLast, 1u << 30), transfer), Contains("battle plan: valid KindOf"), XferException);
	CHECK_THROWS_WITH_AS(load(word(valid, invalidLast, 1u << 31), transfer), Contains("battle plan: invalid KindOf"), XferException);
}

TEST_CASE("PORT-284 charge remembers a locomotor set, including no set, not an arbitrary integer")
{
	movetest::MoveWorld w;
	auto *ai = w.spawn("Walker", 20, 20)->getAIUpdateInterface();
	REQUIRE(ai);
	AIChargeTargetState charge(ai->stateMachine());
	auto transfer = [&](Xfer &x) { charge.xfer(x); };
	const Bytes original = save(transfer);
	// v2: version, goal (three reals), done, count, saved set, no nested move.
	constexpr size_t set = 1 + 12 + 1 + 4;
	REQUIRE(original.size() == set + 4 + 1);
	for (int entry : { -1, static_cast<int>(LOCOMOTORSET_NORMAL), static_cast<int>(LOCOMOTORSET_BURNINGDEATH) })
	{
		const auto valid = word(original, set, entry);
		load(valid, transfer);
		CHECK(save(transfer) == valid);
	}
	for (int bad : { -2, static_cast<int>(LOCOMOTORSET_COUNT) })
	{
		CHECK_THROWS_WITH_AS(load(word(original, set, bad), transfer), Contains("locomotor set before charging"), XferException);
	}
}

TEST_CASE("PORT-284 order images separate cached object IDs, order IDs, formation indices and counters")
{
	using namespace AIGroupOrders;
	// The data contains two templates; 1 is the last index, not the end or a built-in enum limit.
	initest::Fixture files(initest::FileList{ { "Data\\INI\\FormationAssistant.ini",
		"FormationAssistant\n UnitDefinition Unit\n ObjectFilter = ALL\n End\n"
		" FormationTemplate Line\n Rows\n Row = Unit\n End\n End\n FormationTemplate Pair\n Rows\n Row = Unit Unit\n End\n End\nEnd\n" } });
	RetailObjectWorld data(files.fsys);
	std::string error;
	REQUIRE_MESSAGE(data.load(&error), error);
	REQUIRE(data.formations().templates.size() == 2);
	movetest::MoveWorld w;
	const ObjectID member = w.spawn("Walker", 20, 30)->getID();
	auto *gone = w.spawn("Walker", 40, 30);
	const ObjectID cached = gone->getID();
	w.logic->destroyObject(gone);
	w.logic->processDestroyList();
	REQUIRE(w.logic->findObjectByID(cached) == nullptr);
	const ObjectID neverIssued = w.logic->getNextObjectID();

	Image image;
	image.nextId = 37; // the most recently issued ORDER id, independent of the object IDs
	OrderImage order;
	order.id = image.nextId;
	order.objects = { member };
	order.move.positions[cached] = { 10, 20, 30 };
	order.move.stuck = 7;
	order.formation.formationType = 1;
	order.formation.positions[cached] = { 11, 21, 31 };
	order.formation.stuck = 9;
	image.orders = { order };
	image.queues = { { member, { order.id }, order.id, 0, 0 } };
	auto encode = [&](Image i) { return save([&](Xfer &x) { xfer(x, i, *w.logic); }); };
	AIOrdersManager manager;
	auto restore = [&](const Bytes &bytes) {
		Image parsed;
		load(bytes, [&](Xfer &x) { xfer(x, parsed, *w.logic); });
		install(manager, std::move(parsed), *w.logic);
	};
	for (OrderKind kind : { OrderKind::Move, OrderKind::Formation })
	{
		image.orders[0].kind = kind;
		const Bytes valid = encode(image);
		restore(valid);
		CHECK(encode(capture(manager)) == valid); // also preserves stale positions, planned and both open counters
		for (ObjectID bad : { ObjectID(0), neverIssued })
		{
			Image broken = image;
			auto &positions = kind == OrderKind::Move ? broken.orders[0].move.positions : broken.orders[0].formation.positions;
			positions.clear();
			positions[bad] = {};
			CHECK_THROWS_WITH_AS(restore(encode(broken)), Contains("cached position's object id"), XferException);
		}
	}
	for (int bad : { -1, 2 })
	{
		Image broken = image;
		broken.orders[0].formation.formationType = bad;
		CHECK_THROWS_WITH_AS(restore(encode(broken)), Contains("formation template index"), XferException);
	}
	FormationAssistant oneTemplate;
	oneTemplate.templates.resize(1);
	w.aiCommands.setFormationAssistant(&oneTemplate);
	CHECK_THROWS_WITH_AS(restore(encode(image)), Contains("formation template index"), XferException);
	image.orders[0].formation.formationType = 0;
	restore(encode(image));
	w.aiCommands.setFormationAssistant(nullptr);
	image.orders[0].kind = OrderKind::Move;
	Bytes move = encode(image);
	// v2: version, nextId, order count, id, clone, member count, one member, then the tag.
	constexpr size_t tag = 1 + 6 * 4;
	REQUIRE(move.at(tag) == static_cast<unsigned>(OrderKind::Move));
	Bytes badTag = move;
	badTag[tag] = 0xff;
	CHECK_THROWS_WITH_AS(restore(badTag), Contains("the order kind"), XferException);
	move.front() = 1;
	move.erase(move.begin() + tag); // v1 has no tag; its body is a Move
	restore(move);
	CHECK(encode(capture(manager)) == encode(image));

	// The existing graph checks run at install, not against the unrelated object-id allocator.
	Image broken = image;
	--broken.nextId;
	CHECK_THROWS_WITH_AS(restore(encode(broken)), Contains("beyond the next id"), XferException);
	broken = image;
	broken.orders[0].clone = order.id;
	CHECK_THROWS_WITH_AS(restore(encode(broken)), Contains("names the clone"), XferException);
	broken = image;
	broken.queues[0].planned = order.id - 1;
	CHECK_THROWS_WITH_AS(restore(encode(broken)), Contains("is inconsistent"), XferException);
	CHECK(encode(capture(manager)) == encode(image));
}

TEST_CASE("PORT-284 the core v5 sandbox masks name players in this game, not unused seats")
{
	logictest::LogicWorld w;
	auto transfer = [&](Xfer &x) { w.logic->xferCore(x); };
	auto &cheat = w.logic->cheatMode();
	const int count = w.players.getPlayerCount();
	REQUIRE(count == 3); // neutral, Alice, Bob
	cheat.configure(0, count - 1);
	cheat.setAIPaused(count - 1, true);
	cheat.setRevealed(0, true);
	const Bytes valid = save(transfer);
	REQUIRE(valid.front() == 5);
	cheat.clearSandbox();
	load(valid, transfer);
	CHECK(cheat.aiPaused(count - 1));
	CHECK(cheat.instantBuild(count - 1));
	CHECK(cheat.revealed(0));
	CHECK(save(transfer) == valid);
	const auto absent = 1u << count;
	CHECK_THROWS_WITH_AS(load(word(valid, valid.size() - 8, absent), transfer), Contains("paused AIs"), XferException);
	CHECK_THROWS_WITH_AS(load(word(valid, valid.size() - 4, absent), transfer), Contains("instant builders"), XferException);
}

TEST_CASE("PORT-284 skirmish v10 installs one army breaker cooldown per player independently of assist")
{
	logictest::LogicWorld w;
	SkirmishAIManager writer(*w.logic), reader(*w.logic);
	auto encode = [&] { return save([&](Xfer &x) { writer.xfer(x); }); };
	auto transfer = [&](Xfer &x) { reader.xfer(x); };
	reader.armyBreakerTimers() = { { 0, 0 } }; // a game that already supports v10
	const int count = w.players.getPlayerCount();
	writer.spellBookAssistTimers() = { { count - 1, 123 } };
	const auto older = encode(); // no army list: the lower version must still clear any old v10 list
	REQUIRE(older.front() == 5);
	writer.armyBreakerTimers() = { { 0, 0 }, { count - 1, 0xffffffffu } };
	const auto timers = writer.armyBreakerTimers();
	const Bytes valid = encode();
	REQUIRE(valid.front() == 10);
	load(valid, transfer);
	CHECK(reader.spellBookAssistTimers() == writer.spellBookAssistTimers());
	CHECK(reader.armyBreakerTimers() == timers);
	CHECK(save(transfer) == valid);
	for (int bad : { -1, count })
	{
		writer.armyBreakerTimers().back().first = bad;
		CHECK_THROWS_WITH_AS(load(encode(), transfer), Contains("player of a spell book army breaker timer"), XferException);
	}
	writer.armyBreakerTimers().back().first = timers.front().first;
	CHECK_THROWS_WITH_AS(load(encode(), transfer), Contains("two spell book army breaker timers"), XferException);
	CHECK(save(transfer) == valid); // no partially installed list
	load(older, transfer);
	CHECK(reader.armyBreakerTimers().empty());
	CHECK(save(transfer) == older);
}

TEST_CASE("PORT-284 taint cells validate state and area object owner before publishing the grid")
{
	movetest::MoveWorld w;
	auto *area = w.spawn("Walker", 20, 30);
	const ObjectID owner = area->getID();
	w.logic->destroyObject(area);
	w.logic->processDestroyList();
	REQUIRE(w.logic->findObjectByID(owner) == nullptr);
	auto &grid = w.logic->taint();
	grid.setRegion({ 0, 0, 0, 16, 8, 0 }, 8);
	grid.paint({ 12, 4, 0 }, 0, 64, true, owner);
	const auto *block = w.logic->gameState().findBlock("obfme:TaintManager");
	REQUIRE(block);
	REQUIRE(block->snapshot);
	auto transfer = [&](Xfer &x) { block->snapshot->xfer(&x); };
	const Bytes valid = save(transfer);
	// Manager + grid versions, 8 floats, 2 dimensions, ignored checksum; two v2 cells of 10 bytes.
	constexpr size_t checksum = 2 + 8 * 4 + 2 * 4, second = checksum + 4 + 10;
	REQUIRE(valid.size() == second + 10);
	int notifications = 0;
	grid.setCallback([&](int, int, int) { ++notifications; });
	load(word(valid, checksum, 0xabcdef01), transfer);
	CHECK(grid.cell(0, 0).owner == -1);
	CHECK(grid.cell(1, 0).state == TaintManager::STATE_TAINT);
	CHECK(grid.cell(1, 0).owner == static_cast<int>(owner));
	CHECK(notifications == 2);
	const size_t state = second + 2, areaId = state + 4;
	for (int bad : { -1, 3 })
	{
		CHECK_THROWS_WITH_AS(load(word(valid, state, bad), transfer), Contains("cell state"), XferException);
	}
	for (ObjectID bad : { ObjectID(0), w.logic->getNextObjectID(), ObjectID(0xfffffffeu) })
	{
		CHECK_THROWS_WITH_AS(load(word(valid, areaId, bad), transfer), Contains("cell owner"), XferException);
	}
	// Even an empty issuer range cannot turn off the game check. Without a game, a standalone
	// grid still reads the full unsigned ObjectID domain (a negative int other than -1 is an ID).
	CHECK_THROWS_WITH_AS(load(valid, [&](Xfer &x) { grid.xfer(x, 0); }), Contains("cell owner"), XferException);
	TaintManager standalone;
	load(word(valid, areaId, 0xfffffffeu), [&](Xfer &x) { standalone.xfer(x); });
	CHECK(standalone.cell(1, 0).owner == -2);
	CHECK(notifications == 2);
	CHECK(save(transfer) == valid);
	Bytes v1 = word(valid, state, TaintManager::STATE_NONE);
	v1[second] = 1;
	v1.resize(areaId); // a v1 cell has no owner; don't read the next four bytes
	load(v1, transfer);
	CHECK(grid.cell(1, 0).owner == -1);
	CHECK(grid.cell(1, 0).state == TaintManager::STATE_NONE);
	CHECK(grid.cell(1, 0).value == 64); // do not re-derive the stored state from the byte
	CHECK(notifications == 4);
}
