// OpenBFME unit tests. GPL-3.0.
// PORT-323 (issue #840): the Object class' open stops of the 25 named rows (S-070, S-074, S-077, S-116, S-1186, S-121, S-1260, S-1363, S-166,
// S-1954, S-1985, S-2456, S-323, S-380, S-390, S-410, S-4554, S-5142, S-5145, S-560, S-581, S-610, S-7175, S-785, S-922).
//
// Sources of the expectations:
//  * docs/STOPS.md's own rows for the 25 ids (read at the branch point db85d57 and rewritten by this lane, RW addresses as recorded there); and
//  * RW 0x7C38F6 (SET_PLAYER_KILLS_OF_TYPE_TO_COUNTER, ported by lane SCRIPT-4): ScoreKeeper + 0x1FC, the per-player destroyed-by-template maps
//    (RW 0x79F303, over-all-players RW 0x79E4D8), the counter get-or-create (RW 0x60817A) — NOT re-read in this sandbox (the RotWK game.dat is
//    absent and the Google Drive connection is broken), so the expectations here are text of the SCRIPT-4 port, not new disassembly.
//  * RW 0x5F1673 (an OCL's created object registers at the map's registration): this is the ObjectCreationList path (the STRUCTURE template
//    enters via RW 0x6E85E9 in engine/src/GameLogic/ObjectCreationList.cpp), i.e. the S-610 narrowing.
//  * RW 0x695684..0x6956CE / 0xDE8A98 (the StatisticsCollector block of Object::scoreTheKill), RW 0x7BECF6 (NAMED_RECEIVE_UPGRADE on a horde) —
//    the two narrowed remainders of S-323 / S-1363.

#include "doctest.h"

#include "GameLogic/ScriptEngine/ScriptActions.h"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#ifndef OPENBFME_DOCS_DIR
#define OPENBFME_DOCS_DIR "../docs"
#endif

namespace
{
const std::string ids[] = { "S-070", "S-074", "S-077", "S-116", "S-1186", "S-121", "S-1260", "S-1363", "S-166", "S-1954", "S-1985", "S-2456", "S-323", "S-380", "S-390", "S-410", "S-4554", "S-5142", "S-5145", "S-560", "S-581", "S-610", "S-7175", "S-785", "S-922" };

std::string stopsDoc()
{
	std::ifstream in(std::string(OPENBFME_DOCS_DIR) + "/STOPS.md");
	REQUIRE_MESSAGE(in.good(), "cannot read docs/STOPS.md (OPENBFME_DOCS_DIR = " << OPENBFME_DOCS_DIR << ")");
	std::stringstream ss;
	ss << in.rdbuf();
	return ss.str();
}

std::vector<std::string> splitPipe(const std::string &row)
{
	std::vector<std::string> cells;
	std::istringstream iss(row);
	std::string cell;
	while (std::getline(iss, cell, '|'))
	{
		cells.push_back(cell);
	}
	return cells;
}

std::string rowOf(const std::string &doc, const std::string &id)
{
	const std::string head = "\n| " + id + " | ";
	const size_t i = doc.find(head);
	REQUIRE_MESSAGE(i != std::string::npos, "docs/STOPS.md lacks the row of " << id);
	const size_t j = doc.find('\n', i + 1);
	return doc.substr(i + 1, j == std::string::npos ? std::string::npos : j - (i + 1));
}

std::string trim(const std::string &s)
{
	const size_t b = s.find_first_not_of(" \t");
	const size_t e = s.find_last_not_of(" \t");
	return b == std::string::npos ? std::string() : s.substr(b, e - b + 1);
}
} // namespace

TEST_CASE("PORT-323 every one of the 25 own stops carries a PORT-323 verdict in its row")
{
	const std::string doc = stopsDoc();
	for (const std::string &id : ids)
	{
		const std::string row = rowOf(doc, id);
		const std::vector<std::string> cells = splitPipe(row);
		REQUIRE(cells.size() >= 9); // leading empty cell, then the 8 fields; getline does not emit the trailing empty token
		const std::string status = trim(cells[2]);
		const bool okStatus = (status == "code" || status == "closed");
		CHECK_MESSAGE(okStatus, "status of " << id << ": " << status);
		const std::string tail = trim(cells.back());
		const bool verdict = tail.rfind("left:", 0) == 0 || tail.rfind("narrowed", 0) == 0 || tail.rfind("closed", 0) == 0;
		CHECK_MESSAGE(verdict, "the trailing cell of " << id << " is not a verdict: " << tail.substr(0, 80));
	}
}

TEST_CASE("PORT-323 S-1363: the kills action is ported since lane SCRIPT-4; only the horde half is left")
{
	const std::string doc = stopsDoc();
	const std::string row = rowOf(doc, "S-1363");
	CHECK(row.find("SET_PLAYER_KILLS_OF_TYPE_TO_COUNTER (RW 0x7C38F6) is ported since lane SCRIPT-4") != std::string::npos);
	CHECK(row.find("stays unported (counted)") == std::string::npos); // the stale note is gone
	CHECK(row.find("slots 0xB4 / 0xB8") != std::string::npos);         // the remaining horde half is named
		const bool namesHordeOffLimits = (row.find("Port 292") != std::string::npos || row.find("#684") != std::string::npos);
		CHECK(namesHordeOffLimits);
	CHECK(trim(splitPipe(row).back()).rfind("narrowed", 0) == 0);
}

TEST_CASE("PORT-323 S-1363: the campaign stop line names the port, not the stale unported claim")
{
	const std::vector<std::string> lines = ScriptActions::campaignStopLines();
	size_t i1363 = std::string::npos, i1365 = std::string::npos;
	for (size_t i = 0; i < lines.size(); ++i)
	{
		if (lines[i].rfind("[S-1363]", 0) == 0)
		{
			i1363 = i;
		}
		if (lines[i].rfind("[S-1365]", 0) == 0)
		{
			i1365 = i;
		}
	}
	REQUIRE(i1363 != std::string::npos);
	CHECK(i1365 != std::string::npos);
	CHECK(i1363 < i1365); // the 12 pinned campaign lines keep their order (test_camp1.cpp)
	CHECK(lines[i1363].find("is ported since lane SCRIPT-4") != std::string::npos);
	CHECK(lines[i1363].find("stays unported (counted)") == std::string::npos);
	CHECK(lines[i1363].find("NAMED_RECEIVE_UPGRADE") != std::string::npos);
}

TEST_CASE("PORT-323 S-610: the OCL site is registered; the untraced sites are named")
{
	const std::string doc = stopsDoc();
	const std::string row = rowOf(doc, "S-610");
	CHECK(row.find("RW 0x005F1673) IS registered") != std::string::npos); // RW 0x6E85E9 via RW 0x62FBF1 (ObjectCreationList.cpp)
	CHECK(row.find("objectEnteredWorld itself stays a no-op") != std::string::npos);
	CHECK(trim(splitPipe(row).back()).rfind("narrowed", 0) == 0);
}

TEST_CASE("PORT-323 S-323: scoreTheKill is narrowed to the StatisticsCollector block")
{
	const std::string doc = stopsDoc();
	const std::string row = rowOf(doc, "S-323");
	CHECK(row.find("RW 0x695684..0x6956CE") != std::string::npos);
	CHECK(row.find("RW 0xDE8A98") != std::string::npos);
	CHECK(trim(splitPipe(row).back()).rfind("narrowed", 0) == 0);
}

TEST_CASE("PORT-323 S-1985: the row is closed (it was closed by lane MODCOMPAT-MOD1)")
{
	const std::string doc = stopsDoc();
	const std::string row = rowOf(doc, "S-1985");
	CHECK(trim(splitPipe(row)[2]) == "closed");
}
