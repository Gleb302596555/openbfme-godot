// OpenBFME unit tests. GPL-3.0. PORT-153 (issue #371): the ArmyDefinition settings the skirmish AI reads and the ones
// RotWK 2.01 never reads. Evidence: docs/ennorath/PORT153_RW_ANALYSIS.md (every address below was read there with
// tools/mod/rw.py … dis on the retail game.dat, md5 daed3668…).
//
// Synthetic INI only, so the lane runs without game files. The three reader proofs are binary-fact tests: they name
// RW_GAME_DAT in the test name, so RetailTestMount's skip mechanism recognises them and they print SKIP when the
// variable is unset (lane CONTRIB-1, engine/tests/RetailTestMount.cpp:186).
//
// The verdict this file pins:
//  * RW 0x9EFAFB, slot 3 of the base AITactic vtable 0xC8EE24, reads ChanceToUseAllUnitsForDefenseTarget[index]
//    at RW 0x9EFB45 — but the base vtable's address occurs in the image only in the two instructions of the base
//    constructor (RW 0x9EFAD6 / 0x9EFAF0) and in the table itself: every tactic constructor that runs installs its
//    own slot 3 (RW 0x8F121F, 0x9B3684, 0x9B4EF0), so the read is unreachable in the shipped game.
//  * Only ScavangedResourceBuildings (+0xA4, RW 0x9B8DA5 / 0x9B8E03) is read through an army pointer. The nine
//    other offsets of the issue have no reader: a linear scan of the whole .text (296 `[reg + 0x160]` loads, each
//    followed through register and stack copies) reproduces the four readers the port already has and finds nothing
//    for them — see the analysis, section 3.3, for the donor comparison that explains why.

#include "doctest.h"

#include "PeImage.h"
#include "RetailTestMount.h"

#include "Common/INI.h"
#include "Common/INIException.h"
#include "Common/NameKeyGenerator.h"
#include "Common/StateHash.h"
#include "GameLogic/SkirmishAI/SkirmishAIData.h"

#include <cstdint>
#include <string>
#include <vector>

using retailtest::PeImage;

namespace
{
std::vector<std::uint8_t> bytesOf(const std::string &s)
{
	return std::vector<std::uint8_t>(s.begin(), s.end());
}

struct Loader
{
	NameKeyGenerator keys;
	SkirmishAIStore store{ keys };
	INIEnvironment env;
	Loader()
	{
		keys.init();
		store.registerBlocks(env.blocks);
	}
	void load(const std::string &text)
	{
		INI ini(env);
		ini.loadMemory("skirmishaidata.ini", bytesOf(text), INI_LOAD_OVERWRITE);
	}
};

const char kPort153Army[] = "ArmyDefinition Port153Army\r\n"
							"  Side = Port153Side\r\n"
							"  EconomyBuilderMinMoney = 150\r\n"
							"  EconomyBuilderPerFarmValue = 60\r\n"
							"  MaxThreatForOpportunityTargets = 20\r\n"
							"  ValueToSetForMaxOnDefenseTeam = 8\r\n"
							"  CombatChainSearchDepthForTeamRecruits_AttackTeams = 5\r\n"
							"  CombatChainSearchDepthForTeamRecruits_DefenseTeams = 7\r\n"
							"  CombatChainSearchDepthForTeamRecruits_ExploreTeams = 6\r\n"
							"  MaxBuildingsToBeDefensiveTarget_Small = 2\r\n"
							"  MaxBuildingsToBeDefensiveTarget_Med = 5\r\n"
							"  ChanceToUseAllUnitsForDefenseTarget_Small = 30%\r\n"
							"  ChanceToUseAllUnitsForDefenseTarget_Med = 60%\r\n"
							"  ChanceToUseAllUnitsForDefenseTarget_Large = 90%\r\n"
							"End\r\n";
} // namespace

TEST_CASE("PORT-153: the nine ArmyDefinition fields without a reader still parse and keep their values (field table RW 0xC52B40)")
{
	Loader l;
	l.load(kPort153Army);
	const ArmyDefinition *army = l.store.findArmy("Port153Side");
	REQUIRE(army != nullptr);
	// the values are the ones the INI above writes: the fields are parsed, hashed and saved, but RotWK 2.01's AI
	// never reads them (analysis section 3.3), so this is what the port owes the player — the same block, no second
	// mechanism beside the parse.
	CHECK(army->economyBuilderMinMoney == 150);
	CHECK(army->economyBuilderPerFarmValue == 60);
	CHECK(army->maxThreatForOpportunityTargets == 20);
	CHECK(army->valueToSetForMaxOnDefenseTeam == 8);
	CHECK(army->maxBuildingsToBeDefensiveTargetSmall == 2);
	CHECK(army->maxBuildingsToBeDefensiveTargetMed == 5);
	CHECK(army->combatChainSearchDepthForTeamRecruits[0] == 5);
	CHECK(army->combatChainSearchDepthForTeamRecruits[1] == 7);
	CHECK(army->combatChainSearchDepthForTeamRecruits[2] == 6);
}

TEST_CASE("PORT-153: the chance table and the scavanged resource list keep their parsed values and both reach the store hash (+ 0x7C / + 0xA4)")
{
	Loader l;
	l.load(kPort153Army);
	const ArmyDefinition *army = l.store.findArmy("Port153Side");
	REQUIRE(army != nullptr);
	// the percent parser (RW 0x42EEFA -> INI::scanPercentToReal, INI/INIMacro.cpp:249) is
	// `NumericState::pc24Mul(scanReal(token), 0.01f)`: the token "30%" scans as 30.0f and is multiplied by 0.01f in a
	// 24-bit x87 register, so the stored value is that float product (0x3E999999), not the decimal 0.3 (0x3E99999A) —
	// the chance is a 0..1 fraction, the range of the draw at RW 0x9EFB40.
	CHECK(army->chanceToUseAllUnitsForDefenseTarget[0] == 30.0f * 0.01f);
	CHECK(army->chanceToUseAllUnitsForDefenseTarget[1] == 60.0f * 0.01f);
	CHECK(army->chanceToUseAllUnitsForDefenseTarget[2] == 90.0f * 0.01f);
	// a block that does not write the rows keeps the constructor's values (RW 0x82FF49)
	CHECK(army->scavangedResourceBuildings.empty());

	// the scavanged list parses in order (parseAsciiStringVector, RW 0x42EED6)
	Loader l2;
	l2.load("ArmyDefinition Port153Scav\r\n"
			"  Side = Port153ScavSide\r\n"
			"  ScavangedResourceBuildings = Port153LumberMill Port153Crypt\r\n"
			"End\r\n");
	const ArmyDefinition *scav = l2.store.findArmy("Port153ScavSide");
	REQUIRE(scav != nullptr);
	REQUIRE(scav->scavangedResourceBuildings.size() == 2);
	CHECK(scav->scavangedResourceBuildings[0] == "Port153LumberMill");
	CHECK(scav->scavangedResourceBuildings[1] == "Port153Crypt");

	// both are simulation state: the store hash moves when they move (PROCESS 15.5)
	auto hash = [&] {
		StateHasher h;
		l.store.crc(h);
		return h.value();
	};
	ArmyDefinition &mutableArmy = const_cast<ArmyDefinition &>(*l.store.findArmy("Port153Side"));
	const std::uint32_t h0 = hash();
	mutableArmy.chanceToUseAllUnitsForDefenseTarget[0] = 0.31f; // a different stored value
	CHECK(hash() != h0);
	const std::uint32_t h1 = hash();
	mutableArmy.scavangedResourceBuildings.push_back("Port153LumberMill");
	CHECK(hash() != h1);
}

TEST_CASE("PORT-153: RW_GAME_DAT binary fact: RW 0x9EFAFB reads the chance table, but no tactic keeps the base vtable that reaches it")
{
	const PeImage *pe = PeImage::fromEnvironment();
	if (pe == nullptr)
	{
		retailtest::printSkip("PORT-153 the chance table's reader RW 0x9EFAFB (set RW_GAME_DAT)");
		return;
	}
	// slot 3 of the base AITactic vtable 0xC8EE24 is the base initializeTeamTemplate, RW 0x9EFAFB
	CHECK(pe->u32At(0xC8EE24 + 3 * 4) == 0x9EFAFB);
	// the eight AITactic-derived vtable tables in .rdata: every one of them replaces slot 3
	struct Vtable
	{
		std::uint32_t table, slot3;
		const char *who;
	};
	const Vtable tables[] = {
		{ 0xC79664, 0x08F121F, "the AITactic base-slotted clone (SimpleAttack-like)" },
		{ 0xC894F4, 0x09B3684, "AIBasePenetrationTroopsTactic" },
		{ 0xC89848, 0x08F121F, "SimpleAttack" },
		{ 0xC89890, 0x09B4EF0, "SimpleDefense (overrides the slot; STOPS S-417)" },
		{ 0xC8EDEC, 0x08F121F, "the targetless tactics" },
		{ 0xC8EEDC, 0x08F121F, "the targetless tactics" },
		{ 0xC8EF10, 0x08F121F, "the targetless tactics" },
		{ 0xC8EE24, 0x09EFAFB, "the base table itself" },
	};
	for (const Vtable &t : tables)
	{
		CAPTURE(t.who);
		CHECK(pe->u32At(t.table + 3 * 4) == t.slot3);
	}
	// the reader itself, RW 0x9EFB45: `fld dword ptr [edi + esi*4 + 0x7C]` (D9 44 B7 7C) right after the draw at RW 0x9EFB40
	CHECK(pe->u32At(0x9EFB45) == 0x7CB744D9);
}

TEST_CASE("PORT-153: RW_GAME_DAT binary fact: only ScavangedResourceBuildings is read on an army pointer (RW 0x9B8DA5 / 0x9B8E03)")
{
	const PeImage *pe = PeImage::fromEnvironment();
	if (pe == nullptr)
	{
		retailtest::printSkip("PORT-153 the scavanged list's reader RW 0x9B8DA5 (set RW_GAME_DAT)");
		return;
	}
	// the picker RW 0x9B8D81: [record + 0x160] -> the army definition, then the vector's begin (+ 0xA4) and end (+ 0xA8)
	CHECK(pe->u32At(0x9B8DA5) == 0x0160878B); // mov eax, dword ptr [edi + 0x160]  (8B 87 60 01 00 00)
	CHECK(pe->u32At(0x9B8DAB) == 0x00A8888B); // mov ecx, dword ptr [eax + 0xA8]   (8B 88 A8 00 00 00)
	// the element read RW 0x9B8E03: mov eax, dword ptr [eax + 0xA4] (8B 80 A4 00 00 00), the vector's begin
	CHECK(pe->u32At(0x9B8E03) == 0x00A4808B);
}
