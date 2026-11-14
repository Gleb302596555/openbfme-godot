// OpenBFME unit tests. GPL-3.0.
// PORT-171 (fork issue 400, P-37): the temporary folder of an online test (OnlineTestTemp.h). Test infrastructure: no game data and no RotWK
// behaviour is involved, so there is no RW address here; the expectations are the operating system's rules for deleting an open file.
//
// The Windows failure "remove_all: The process cannot access the file because it is being used by another process" of the online tests (online3
// rejoin, online16-fix, online3 spectators, onlineux1 r2) came from the test process itself: a live RelayCore keeps its games' logs open.

#include "doctest.h"

#include "OnlineTestTemp.h"
#include "OnlineTestWait.h"
#include "TestChild.h"

#include "GameNetwork/RelayProtocol.h"
#include "RelayCore.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
const char *const kPartLog = "0011223344556677.obfmerlog.part"; // RelayCore::hello: <logDir>/<game id, 16 hex digits>.obfmerlog.part
const char *const kDoneLog = "0011223344556677.obfmerlog";      // RelayCore::close renames it

std::vector<std::uint8_t> port171Key()
{
	std::vector<std::uint8_t> k;
	for (int i = 0; i < 32; ++i)
	{
		k.push_back((std::uint8_t)(i * 3 + 1));
	}
	return k;
}
// a first-incarnation HELLO of slot 0 of a one-player game
std::vector<std::uint8_t> port171Hello()
{
	RelayProtocol::Ticket t;
	t.keyId = 1;
	t.gameId = 0x0011223344556677ull;
	t.slot = 0;
	t.slotMask = 0x01;
	t.expires = 2000000000u;
	const auto signedTicket = RelayProtocol::signTicket(t, port171Key());
	return RelayProtocol::encodeHello(std::vector<std::uint8_t>(signedTicket.begin(), signedTicket.end()));
}
RelayCore::Options port171Relay(const onlinetest::TempDir &dir)
{
	RelayCore::Options o;
	o.keys = { { 1, port171Key() } };
	o.logDir = dir.string();
	return o;
}
// the type byte of the relay's answer (RelayProtocol: magic, version, type), -1 when it did not answer
int replyType(const RelayCore::Action &a)
{
	return a.kind == RelayCore::Action::REPLY && a.reply.size() >= 4 ? (int)a.reply[3] : -1;
}
// what a throwing removal says ("" when it did not throw)
std::string removalFailure(onlinetest::TempDir &dir)
{
	try
	{
		dir.remove();
	}
	catch (const std::exception &e)
	{
		return e.what();
	}
	return std::string();
}
#ifndef _WIN32
std::string fileText(const std::filesystem::path &p)
{
	std::ifstream f(p);
	return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}
#endif
} // namespace

TEST_CASE("PORT-171 online temp folder: a live RelayCore keeps its game log open in the test process itself; removing the folder under it fails by the "
		  "file's name, and the folder goes cleanly once the relay let go")
{
	std::filesystem::path where;
	{
		onlinetest::TempDir dir("openbfme_port171_relay");
		where = dir.path();
		REQUIRE(std::filesystem::is_directory(where));
		RelayCore relay(port171Relay(dir));
		std::vector<std::uint8_t> hello = port171Hello();
		const RelayCore::Action welcome = relay.handle(RelayAddress{ 0x0A000001u, 10 }, hello.data(), hello.size(), 1000, 1700000000u);
		REQUIRE(replyType(welcome) == RelayProtocol::WELCOME);
		// a client reads the log back (a rejoin, a spectator): the relay's second handle on the same file
		std::vector<std::uint8_t> request = RelayProtocol::encodeLogRequest(0);
		const RelayCore::Action chunk = relay.handle(RelayAddress{ 0x0A000001u, 10 }, request.data(), request.size(), 2000, 1700000000u);
		REQUIRE(replyType(chunk) == RelayProtocol::LOG_CHUNK);
		REQUIRE(std::filesystem::exists(dir / kPartLog));
		REQUIRE(relay.stats().gamesClosed == 0);

		// the cause of P-37: no other process exists here, and the log is open
		if (onlinetest::canSeeOpenFiles())
		{
			const std::vector<std::string> open = onlinetest::openFilesUnder(dir);
			REQUIRE(open.size() == 1);
			CHECK(open[0] == kPartLog);
		}

		SUBCASE("the relay goes out of scope before the folder: nothing to do in the test")
		{
		}
		SUBCASE("what the online tests did (remove the folder with the relay alive) fails by the file's name on every platform")
		{
			if (onlinetest::canSeeOpenFiles())
			{
				const std::string failure = removalFailure(dir);
				CHECK(failure.find(std::string("still open: ") + kPartLog) != std::string::npos);
				CHECK(failure.find(where.string()) != std::string::npos);
				CHECK(dir.removed()); // one attempt, one report: the destructor does not fail the test a second time
			}
		}
		SUBCASE("a registered holder is released by the removal, before the folder is touched")
		{
			bool finishedLogSeen = false;
			dir.holds("the relay's game logs", [&]() {
				relay.shutdown(3000);
				finishedLogSeen = std::filesystem::exists(dir / kDoneLog) && !std::filesystem::exists(dir / kPartLog);
				return std::string();
			});
			CHECK(removalFailure(dir) == "");
			CHECK(relay.stats().gamesClosed == 1); // RelayCore::shutdown closed the game: END record, log closed and renamed
			CHECK(finishedLogSeen);
			CHECK(!std::filesystem::exists(where));
		}
		SUBCASE("a holder that does not release fails the removal by its name")
		{
			relay.shutdown(3000);
			dir.holds("lobby service", []() { return std::string("still running"); });
			const std::string failure = removalFailure(dir);
			CHECK(failure.find("lobby service: still running") != std::string::npos);
			CHECK(failure.find("still open") == std::string::npos); // the relay's files are closed
			CHECK(!std::filesystem::exists(where));
		}
	}
	// whichever way: nothing is left behind
	CHECK(!std::filesystem::exists(where));
}

TEST_CASE("PORT-171 online temp folder: a folder nothing holds is removed with all it contains; one name gives distinct folders; a failed test "
		  "leaves nothing behind; a stream the test forgot is named")
{
	std::filesystem::path a, b, c;
	{
		onlinetest::TempDir first("openbfme_port171_plain"), second("openbfme_port171_plain");
		a = first.path();
		b = second.path();
		CHECK(a != b);
		CHECK(std::filesystem::is_directory(a));
		CHECK(std::filesystem::is_directory(b));
		std::filesystem::create_directories(first / "data" / "deep");
		{
			std::ofstream f(first / "data" / "deep" / "closed.txt");
			f << "closed before the removal";
		}
		CHECK(onlinetest::openFilesUnder(first).empty());
		CHECK(removalFailure(first) == "");
		CHECK(!std::filesystem::exists(a));
		CHECK(removalFailure(first) == ""); // a second call does nothing
		CHECK(std::filesystem::is_directory(b)); // the neighbour of the same name is untouched

		// a failed REQUIRE is an exception: the folder goes with the scope
		try
		{
			onlinetest::TempDir third("openbfme_port171_plain");
			c = third.path();
			std::ofstream f(third / "written.txt");
			f << "x";
			f.close();
			throw std::runtime_error("a failed REQUIRE");
		}
		catch (const std::runtime_error &)
		{
		}
		CHECK(!c.empty());
		CHECK(!std::filesystem::exists(c));

		// the test's own stream, still open at the removal
		if (onlinetest::canSeeOpenFiles())
		{
			std::filesystem::create_directories(second / "data");
			std::ofstream forgotten(second / "data" / "forgotten.log");
			forgotten << "x" << std::flush;
			const std::string failure = removalFailure(second);
			CHECK(failure.find("still open: data/forgotten.log") != std::string::npos);
			forgotten.close();
		}
	}
	CHECK(!std::filesystem::exists(b));
}

#ifndef _WIN32
TEST_CASE("PORT-171 online temp folder: a helper process that does not exit fails the removal by its name, not by remove_all; one that exits on "
		  "SIGTERM is waited for and the folder goes")
{
	onlinetest::TempDir dir("openbfme_port171_child");
	const std::filesystem::path where = dir.path();
	bool stubborn = false;
	SUBCASE("the helper ignores SIGTERM") { stubborn = true; }
	SUBCASE("the helper exits on SIGTERM") {}
	// TestChild points the helper's output at a file of the folder, as the tests' relay and lobby processes have theirs
	const std::string script = stubborn ? "trap '' TERM; echo ready; while :; do sleep 1; done" : "trap 'exit 0' TERM; echo ready; while :; do sleep 1; done";
	TestChild child("/bin/sh", { "-c", script }, (dir / "helper.out").string());
	REQUIRE(child.started());
	// the trap is set once the helper wrote its line
	REQUIRE(onlinetest::waitUntil([&]() { return fileText(dir / "helper.out").find("ready") != std::string::npos; }));
	dir.holdsProcess("openbfme_relay helper", child, std::chrono::milliseconds(stubborn ? 300 : 60000));
	const std::string failure = removalFailure(dir);
	if (stubborn)
	{
		CHECK(failure.find("openbfme_relay helper: the helper process did not exit within 300 ms of SIGTERM") != std::string::npos);
		CHECK(failure.find("remove_all") == std::string::npos);
	}
	else
	{
		CHECK(failure == "");
	}
	CHECK(!child.started()); // stopped and reaped either way: it holds nothing now
	CHECK(!std::filesystem::exists(where));
}
#endif
