// OpenBFME unit tests. GPL-3.0.
// PORT-72 (issue #186): FactionVictoryData — the per-faction scale factors of TheVictorySystem.
//
// Verdict (2026-11-09, docs/ennorath/PORT72_RW_ANALYSIS.md, stop S-7197 in docs/STOPS.md): RotWK 2.01 reads the
// factors only inside TheVictorySystem, the LoTR-RTS "large battle" subsystem (cell-grid victory; bonus XP for the
// units that took part). Every reference to the global TheVictorySystem (RW 0x00DE897C) — parse RW 0x6159DC/0x615AAD,
// init registration RW 0x63C301, vtable slot +0x28 at RW 0x62E6D3, vtable slot +0x38 at RW 0x631395, the snapshot
// writer RW 0x6DF73E, the battle-XP caller RW 0x698FC4, the 20-bit victory mask RW 0x806909 (kills*EnemyKillScaleFactor
// − deaths*AllyDeathScaleFactor > VictoryThreshold, RW 0x80695F..RW 0x806988) and the cell scan RW 0x973C81
// (VictoryThreshold * MapToCellVictoryRatio) — belongs to the subsystem. The port has no such subsystem at all
// (SubsystemInitOrder.cpp:128 is a legend-only stub; there is no CellGrid, no records vector, no kill/death feed,
// no bonus grant, and CHUNK_VictorySystem is not saved), and the port's victory path — VictoryConditions, the
// elimination check — shares no code with TheVictorySystem and never reads the factors.
//
// So there is nothing to port on the existing victory path, and wiring the factors into it would add a second,
// divergent victory rule. These tests pin the verdict's conditions instead: the block stays a known recording
// stub (no store, no reader), the keyword stays in the RotWK 2.01 table, and the load fails loudly rather than
// silently dropping the block if the stubs are ever removed without a real parser landing.
// They fail if the block is unstubbed or the keyword drops out of the registration list.

#include "doctest.h"
#include "IniTestUtil.h"

#include "Common/INI/INIBlockStubs.h"

#include <algorithm>
#include <set>

using namespace initest;

namespace
{

bool hasKeyword(const std::vector<std::string> &keywords, const char *keyword)
{
	return std::find(keywords.begin(), keywords.end(), keyword) != keywords.end();
}

// a synthetic default/victorysystem.ini: VictorySystemData (four fields) and two FactionVictoryData blocks whose
// values include the Ennorath extremes (Rohan's EnemyKillScaleFactor = 20.1, the thresholds around zero).
const char *kSyntheticVictorySystem =
	"VictorySystemData default\n"
	"\tCellSize\t\t\t= 450\n"
	"\tScalePerLogicFrame = 1.0\n"
	"\tSubtractPerLogicFrame = 0.0\n"
	"\tCellBonusRadius\t= 1.0\n"
	"End\n"
	"FactionVictoryData Gondor\n"
	"\tAllyDeathScaleFactor  = 1.0\n"
	"\tEnemyKillScaleFactor  = 1.1\n"
	"\tVictoryThreshold\t  = 100.0\n"
	"\tMajorUnitValue\t\t  = 10.0\n"
	"\tMapToCellVictoryRatio = 0.8\n"
	"End\n"
	"FactionVictoryData Rohan\n"
	"\tAllyDeathScaleFactor  = 1.0\n"
	"\tEnemyKillScaleFactor  = 20.1\n"
	"\tVictoryThreshold\t  = 10.0\n"
	"\tMajorUnitValue\t\t  = 10.0\n"
	"\tMapToCellVictoryRatio = 0.8\n"
	"End\n";

} // namespace

TEST_CASE("PORT-72 FactionVictoryData: the RotWK 2.01 keyword table keeps FactionVictoryData and VictorySystemData (spec 3.2)")
{
	const std::vector<std::string> &k = RotwkBlockKeywords();
	CHECK(hasKeyword(k, "FactionVictoryData"));
	CHECK(hasKeyword(k, "VictorySystemData"));
	const std::set<std::string> unique(k.begin(), k.end());
	CHECK(unique.count("FactionVictoryData") == 1);
	CHECK(unique.count("VictorySystemData") == 1);
}

TEST_CASE("PORT-72 FactionVictoryData: victorysystem.ini loads as recording stubs and lands nowhere — no store, no reader")
{
	Fixture fx;
	INIBlockRecorder rec;
	RegisterRecordingBlockStubs(fx.env.blocks, rec, {});

	REQUIRE(loadError(fx.env, "victorysystem.ini", kSyntheticVictorySystem).empty());

	// every block of the file, recorded as ended at its own End line, fields carried nowhere
	REQUIRE(rec.records.size() == 3);
	CHECK(rec.records[0].keyword == "VictorySystemData");
	CHECK(rec.records[0].name == "default");
	CHECK(rec.records[0].endsWithEnd);
	CHECK(rec.records[0].extentLines == 5);
	CHECK(rec.records[1].keyword == "FactionVictoryData");
	CHECK(rec.records[1].name == "Gondor");
	CHECK(rec.records[1].endsWithEnd);
	CHECK(rec.records[1].extentLines == 6);
	CHECK(rec.records[2].keyword == "FactionVictoryData");
	CHECK(rec.records[2].name == "Rohan");
	CHECK(rec.records[2].endsWithEnd);
	CHECK(rec.records[2].extentLines == 6);
	CHECK(rec.countByKeyword["VictorySystemData"] == 1);
	CHECK(rec.countByKeyword["FactionVictoryData"] == 2);
}

TEST_CASE("PORT-72 FactionVictoryData: without the recording stub the load fails loudly instead of dropping the block")
{
	Fixture fx;
	INIBlockRecorder rec;
	// simulate removing the stub without landing a real parser: the retail file must not traverse silently
	RegisterRecordingBlockStubs(fx.env.blocks, rec, { "FactionVictoryData" });

	int code = 0;
	const std::string err = loadError(fx.env, "victorysystem.ini", kSyntheticVictorySystem, INI_LOAD_OVERWRITE, &code);
	CHECK(code == 5);
	CHECK(err.find("Unknown block 'FactionVictoryData'.") == 0);

	// and with only the Gondor block skipped, the same holds for the first unstubbed block the loader reaches
	Fixture fx2;
	INIBlockRecorder rec2;
	RegisterRecordingBlockStubs(fx2.env.blocks, rec2, { "VictorySystemData" });
	code = 0;
	const std::string err2 = loadError(fx2.env, "victorysystem2.ini",
		"VictorySystemData default\n\tCellSize = 450\nEnd\nFactionVictoryData Gondor\nEnd\n", INI_LOAD_OVERWRITE, &code);
	CHECK(code == 5);
	CHECK(err2.find("Unknown block 'VictorySystemData'.") == 0);
}
