// OpenBFME. PORT-70 (issue #184): the GameData rows the port does not read — a verdict table, no engine code.
//
// docs/ennorath/gamedata-rows.md sorts the rows the field audit v6 lists as `not-read`: for every row its RotWK table
// address (RW 0xBFF580 + 16 * index), its member offset (GameData + off), whether RotWK reads it and where, and its
// class (gameplay, presentation, debug / editor, not read). This file pins the facts the page rests on and the port's
// own side of them; it changes no engine behaviour:
//
//   1. every row the page lists is a row of the RotWK GameData table. The row's address is `RW 0xBFF580 + 16 * index`
//      and the member offset comes with it; BFME2 1.06's own table (Code/GameEngine/Source/Common/GlobalData.cpp,
//      s_GlobalDataFieldParseTable, 457 rows in the same order with the same offsets) fills the index. The fixture
//      engine/tests/data/port70/gamedata-rows.txt is that page's table (class, field, row, offset); the oracle below is
//      the donor's index of the fields this lane needed to prove its shape (Windowed 0x2C first, BaseRegenDelay 275 ->
//      RW 0xC006B0 + 0xB9C, as batch-16#18 cites the image), so a shifted index or a copy-pasted offset fails;
//   2. the port takes none of them with a row: with gamedata.ini's `GameData` block open, every listed field is taken
//      by the block's catch-all row (noteUnapplied, RW 0x6441A5's table RW 0xBFF580 + the S-152 catch-all) and is
//      recorded as `GameData.<field>` in `unappliedFields`, with nothing applied. That is the page's `catch-all` column,
//      and it is the reason the rows are on the page at all: the loader already accepts the field so that a mod's
//      gamedata.ini does not error, and no rule follows from it;
//   3. the rows that already have a `kGameDataFields` row stay out of the page: a field the port reads (SellPercentage,
//      the rows the earlier lanes ported) must not be silently listed as not read, and a row the page does list must
//      fail here the day a later port gives it a row — the page's `gameplay` block are the next ports, so that failure
//      is the signal to move the row out of the page.
#include "doctest.h"

#include "GameEngineDevice/Win32Device/Common/Win32BIGFileSystem.h"
#include "GameLogic/GameLogic.h"

#include <cstdlib>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace
{
namespace L = GameLogicSettingsLoader;

// The trees the build points the tests at: OPENBFME_TESTDATA_DIR (engine/tests/data) holds the page's fixture, and
// OPENBFME_DOCS_DIR the page itself, so the two are compared where both exist. A build that does not define them falls
// back to this file's own path; when neither is usable the file tests say so instead of failing.
std::string fixturePath()
{
#ifdef OPENBFME_TESTDATA_DIR
	return std::string(OPENBFME_TESTDATA_DIR) + "/port70/gamedata-rows.txt";
#else
	const std::string file = __FILE__; // <repo>/engine/tests/test_port_70.cpp, unless the build remapped the prefix
	const std::string marker = "/engine/tests/test_port_70.cpp";
	const std::size_t at = file.rfind(marker);
	return at == std::string::npos ? std::string() : file.substr(0, at) + "/engine/tests/data/port70/gamedata-rows.txt";
#endif
}

std::string pagePath()
{
#ifdef OPENBFME_DOCS_DIR
	return std::string(OPENBFME_DOCS_DIR) + "/ennorath/gamedata-rows.md";
#else
	const std::string file = __FILE__;
	const std::string marker = "/engine/tests/test_port_70.cpp";
	const std::size_t at = file.rfind(marker);
	return at == std::string::npos ? std::string() : file.substr(0, at) + "/docs/ennorath/gamedata-rows.md";
#endif
}

struct Row
{
	std::string cls;
	std::string field;
	unsigned row = 0;
	unsigned off = 0;
};

unsigned parseHex(const std::string &s)
{
	if (s.size() < 3 || s[0] != '0' || (s[1] != 'x' && s[1] != 'X'))
	{
		throw std::runtime_error("not a hex constant: " + s);
	}
	return (unsigned)std::strtoul(s.c_str() + 2, nullptr, 16);
}

// the page's table: class, field, row address, member offset (one row per line, '#' comments)
std::vector<Row> readPage(const std::string &path)
{
	std::ifstream in(path);
	const std::string cannot = "cannot open " + path;
	REQUIRE_MESSAGE(in.good(), cannot.c_str());
	std::vector<Row> rows;
	std::string line;
	while (std::getline(in, line))
	{
		if (line.empty() || line[0] == '#')
		{
			continue;
		}
		std::vector<std::string> cells;
		std::string cell;
		std::istringstream ls(line);
		while (std::getline(ls, cell, '\t'))
		{
			cells.push_back(cell);
		}
		const std::string bad = "a page line with " + std::to_string(cells.size()) + " cells: " + line;
		REQUIRE_MESSAGE(cells.size() == 4, bad.c_str());
		Row r;
		r.cls = cells[0];
		r.field = cells[1];
		r.row = parseHex(cells[2]);
		r.off = parseHex(cells[3]);
		rows.push_back(r);
	}
	return rows;
}

// BFME2 1.06 s_GlobalDataFieldParseTable: the index of a field in the RotWK GameData table's order, and its member
// offset — the rows this lane cites from the field-triage reports, so the page's derived addresses have an oracle that
// does not come from the page. Index 0 is `Windowed` (+0x2C), the last row is index 456 (RW 0xBFF580 + 16*456 = RW 0xC01200,
// the table's own end RW 0xC01210 after one more row).
const std::map<std::string, std::pair<unsigned, unsigned>> &donorIndex()
{
	static const std::map<std::string, std::pair<unsigned, unsigned>> index = {
		{ "Windowed", { 0, 0x2C } },
		{ "BaseRegenDelay", { 275, 0xB9C } },
		{ "BaseRegenHealthPercentPerSecond", { 274, 0xB98 } },
		{ "SellPercentage", { 273, 0xB94 } },
		{ "BuildSpeed", { 210, 0xA64 } },
		{ "CommandCenterHealAmount", { 220, 0xA90 } },
		{ "CommandCenterHealRange", { 219, 0xA8C } },
		{ "AnimationSharingCap", { 307, 0xC40 } },
		{ "DefaultStructureRepairBuffFxList", { 53, 0x9E0 } },
		{ "GetHealedAnimationZRise", { 91, 0x130 } },
		{ "AmmoPipScaleFactor", { 73, 0x104 } },
		{ "Gravity", { 68, 0xC4 } },
		{ "WaitToForceMemberToEnterDelay", { 456, 0x1234 } },
	};
	return index;
}

const std::set<std::string> &portRows()
{
	// the names kGameDataFields (engine/src/GameLogic/GameLogicSettings.cpp) has a row for, of the blocks this lane
	// touches; the probe below asks the loader about any other name, these are the ones a refactor of the table could
	// drop silently, so they are named here
	static const std::set<std::string> rows = {
		"SellPercentage", "Gravity", "DefaultStructureRubbleHeight", "SupplyBuildBorder", "MinDistFromEdgeOfMapForBuild",
		"AllowedHeightVariationForBuilding", "MaxLineBuildObjects", "BuilderMoveFromNewStructureDistance",
		"BuilderFadeOutTime", "BuilderFadeInTime", "EnableHouseColor", "AttributeModifierArmorMaxBonus",
		"DamageRadiusMinimumForSplash", "UnitDamagedThreshold", "UnitReallyDamagedThreshold", "DefaultStartingCash",
		"ForceModelsToFollowTimeOfDay", "ForceModelsToFollowWeather", "TimeAfterDamageUntilRepairAllowed",
	};
	return rows;
}

// True when the loader gives the GameData field a kGameDataFields row: with the block open, a field a row takes is
// parsed into the settings and never reaches the catch-all; a field only the catch-all takes is recorded as
// `GameData.<name>` in unappliedFields and applies nothing. The block's table also refuses a value the field's own
// parser cannot read, which is the second signal used where a row's parser is picky (a filter list, a duration).
// The block's required rows (kGameDataNeeds, RW: gamedata.ini must define them or the load fails, which would look like
// a refusal of the probed field): every probe carries them so that a `false` from the loader is the probed field's own row.
const char *const kRequiredGameDataRows =
	"  ForceModelsToFollowTimeOfDay = Yes\n"
	"  ForceModelsToFollowWeather = Yes\n"
	"  UnitDamagedThreshold = 0.5\n"
	"  UnitReallyDamagedThreshold = 0.25\n"
	"  DefaultStartingCash = 1000\n";

bool loaderHasRow(const std::string &field)
{
	const std::string text = "GameData\n" + std::string(kRequiredGameDataRows) + "  " + field + " = 1\nEnd\n";
	GameLogicSettings g;
	std::string err;
	if (!L::scanGameData(text, g, &err))
	{
		return true; // a row refused the value: the loader knows the field
	}
	return g.unappliedFields.count("GameData." + field) == 0;
}
} // namespace

TEST_CASE("PORT-70 the page's rows are GameData rows of the RotWK table (RW 0xBFF580 + 16 * index)")
{
	const std::string fixture = fixturePath();
	if (fixture.empty())
	{
		MESSAGE("SKIP: neither OPENBFME_TESTDATA_DIR nor __FILE__ locate the page's fixture");
		return;
	}
	const std::vector<Row> rows = readPage(fixture);
	REQUIRE(rows.size() == 78); // the page's table, one line per row (the summary's four class counts add up to it) // PORT-66 moved four presentation rows into kGameDataFields

	std::map<std::string, unsigned> classes;
	std::set<std::string> seen;
	std::set<unsigned> addresses;
	for (const Row &r : rows)
	{
		INFO("row " << r.field);
		// a field appears once, on one class, and two fields never share a table row (they may share an offset: a name
		// list row and the rows a class reuses, e.g. GoodCommandPointsMP56 / MP6)
		CHECK(seen.insert(r.field).second);
		CHECK(addresses.insert(r.row).second);
		++classes[r.cls];
		// the table's address must be a row of the 16-byte-row table: RW 0xBFF580 <= row < RW 0xC01210, aligned
		CHECK(r.row >= 0xBFF580u); // tolerance: the table's extent is the claim (RW 0xBFF580 .. RW 0xC01210); the exact address is checked against the donor index below
		CHECK(r.row < 0xC01210u);  // tolerance: the same extent's end (the block's last row is RW 0xC01200, one row before the NULL row)
		CHECK((r.row - 0xBFF580u) % 16u == 0u);
		// the member offset is inside the GameData object the constructor allocates (RW 0x64410D `push 0x1254`)
		CHECK(r.off < 0x1254u); // tolerance: the GameData object's size (RW 0x64410D allocates 0x1254 bytes); exact offsets are checked against the donor index
		// and the index in the donor table reproduces the address exactly
		const auto it = donorIndex().find(r.field);
		if (it != donorIndex().end())
		{
			CHECK(r.row == 0xBFF580u + 16u * it->second.first);
			CHECK(r.off == it->second.second);
		}
	}
	const std::map<std::string, unsigned> expected = { { "gameplay", 23 }, { "presentation", 12 }, { "debug / editor", 22 }, { "not read", 21 } };
	CHECK(classes == expected);

	// the oracle's own arithmetic: the donor's first row is `Windowed` at +0x2C, so the block starts at RW 0xBFF580 and a
	// row's address is base + 16 * index; BaseRegenDelay (index 275) lands on RW 0xC006B0, the address batch-16 cited
	CHECK(donorIndex().at("Windowed").first == 0u);
	CHECK(0xBFF580u + 16u * donorIndex().at("BaseRegenDelay").first == 0xC006B0u); // batch-16#18's row of the image
	CHECK(0xBFF580u + 16u * donorIndex().at("SellPercentage").first == 0xC00690u);   // a ported row, batch FACT-A
	CHECK(0xBFF580u + 16u * donorIndex().at("CommandCenterHealAmount").first == 0xC00340u);
	CHECK(0xBFF580u + 16u * donorIndex().at("WaitToForceMemberToEnterDelay").first == 0xC01200u); // the last row
}

TEST_CASE("PORT-70 none of the page's rows has a kGameDataFields row: the S-152 catch-all takes each one")
{
	const std::string fixture = fixturePath();
	if (fixture.empty())
	{
		MESSAGE("SKIP: neither OPENBFME_TESTDATA_DIR nor __FILE__ locate the page's fixture");
		return;
	}
	const std::vector<Row> rows = readPage(fixture);
	GameLogicSettings g;
	std::string err;
	std::string text = "GameData\n" + std::string(kRequiredGameDataRows);
	for (const Row &r : rows)
	{
		text += "  " + r.field + " = 1\n";
	}
	text += "End\n";
	REQUIRE_MESSAGE(L::scanGameData(text, g, &err), err.c_str());
	// every row the page lists reaches the catch-all, by its `GameData.<field>` name: the loader parses the field and
	// applies nothing (the page's whole point), and no row of the block claims it
	for (const Row &r : rows)
	{
		INFO("row " << r.field);
		CHECK(g.unappliedFields.count("GameData." + r.field) == 1);
	}
	// and the neighbour test: the block's real rows are not in that set — the catch-all did not swallow everything
	// (a GameData block whose table failed to register would record every field the same way)
	for (const char *field : { "SellPercentage", "Gravity", "DefaultStructureRubbleHeight", "DefaultStartingCash" })
	{
		INFO("row " << field);
		CHECK(g.unappliedFields.count(std::string("GameData.") + field) == 0);
	}
}

TEST_CASE("PORT-70 the rows the port has keep their rows; a page row that gains one is reported")
{
	const std::string fixture = fixturePath();
	if (fixture.empty())
	{
		MESSAGE("SKIP: neither OPENBFME_TESTDATA_DIR nor __FILE__ locate the page's fixture");
		return;
	}
	const std::vector<Row> rows = readPage(fixture);
	std::set<std::string> listed;
	for (const Row &r : rows)
	{
		listed.insert(r.field);
	}
	// the port's rows on this side of the page: parsed, nothing recorded as unapplied
	for (const std::string &field : portRows())
	{
		INFO("port row " << field);
		CHECK_MESSAGE(loaderHasRow(field), "the loader lost the kGameDataFields row of " << field);
		CHECK(listed.count(field) == 0);
	}
	// the page is a verdict table: a row listed there must stand out of the block's table, or the page is stale and the
	// next port of its `gameplay` block must move the row to docs/ennorath/NIGHT_WATCH.md's style (a ported row) instead
	for (const Row &r : rows)
	{
		INFO("page row " << r.field);
		CHECK_MESSAGE(!loaderHasRow(r.field), r.field << " now has a kGameDataFields row: move it out of docs/ennorath/gamedata-rows.md");
	}
}

TEST_CASE("PORT-70 the page and its fixture are the same table")
{
	// docs/ennorath/gamedata-rows.md is the deliverable and engine/tests/data/port70/gamedata-rows.txt its parsed form:
	// every `| field | row | off | verdict | class | port | ... |` line of the page must equal the fixture's line, name
	// for name, address for address, so the page cannot drift from what the other tests pin (and a row added to one file
	// and not the other fails here).
	const std::string fixture = fixturePath();
	const std::string page = pagePath();
	if (fixture.empty() || page.empty())
	{
		MESSAGE("SKIP: this build does not name the test data / docs folders");
		return;
	}
	std::ifstream in(page);
	if (!in.good())
	{
		MESSAGE("SKIP: the page is not in this build's tree (" << page << ")");
		return;
	}
	std::vector<Row> pinned = readPage(fixture);
	std::vector<Row> fromPage;
	std::string line;
	while (std::getline(in, line))
	{
		if (line.rfind("| `", 0) != 0)
		{
			continue;
		}
		std::vector<std::string> cells;
		std::string cell;
		std::istringstream ls(line);
		while (std::getline(ls, cell, '|'))
		{
			const std::size_t b = cell.find_first_not_of(' ');
			const std::size_t e = cell.find_last_not_of(' ');
			cells.push_back(b == std::string::npos ? std::string() : cell.substr(b, e - b + 1));
		}
		// the line starts with '|', so cells[0] is empty and the row's cells start at 1; the summary table's own lines
		// (`| \`gameplay\` | 23 | ...`) also start with "| \`" and are skipped by the address cell below
		if (cells.size() != 9 || cells[2].size() < 3 || cells[2].compare(0, 2, "0x") != 0)
		{
			continue;
		}
		REQUIRE(cells.size() == 9); // the table's eight columns: field, row, off, verdict, class, port, evidence, batch
		Row r;
		r.field = cells[1].size() > 2 ? cells[1].substr(1, cells[1].size() - 2) : std::string();
		r.row = parseHex(cells[2]);
		r.off = parseHex(cells[3]);
		r.cls = cells[5];
		fromPage.push_back(r);
	}
	REQUIRE(fromPage.size() == pinned.size());
	for (std::size_t i = 0; i < pinned.size(); ++i)
	{
		INFO("row " << pinned[i].field);
		CHECK(fromPage[i].field == pinned[i].field);
		CHECK(fromPage[i].row == pinned[i].row);
		CHECK(fromPage[i].off == pinned[i].off);
		CHECK(fromPage[i].cls == pinned[i].cls);
	}
}
