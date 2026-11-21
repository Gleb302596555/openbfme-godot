// OpenBFME unit tests. GPL-3.0. PORT-310 (issue #702): the thirteen ArmyDefinition settings the audit calls `parsed-unused` —
// ChanceToUseAllUnitsForDefenseTarget_Small/_Med/_Large, CombatChainSearchDepthForTeamRecruits_AttackTeams/_DefenseTeams/_ExploreTeams,
// EconomyBuilderMinMoney, EconomyBuilderPerFarmValue, MaxBuildingsToBeDefensiveTarget_Small/_Med, MaxThreatForOpportunityTargets,
// ValueToSetForMaxOnDefenseTeam and ScavangedResourceBuildings.
//
// Evidence: docs/ennorath/PORT310_RW_ANALYSIS.md. Every address below was read with `tools/mod/rw.py … dis` on the retail RotWK 2.01
// `game.dat` (md5 daed3668…). The tests run on synthetic INI; the three chain tests name RW_GAME_DAT in their title, so
// RetailTestMount's skip mechanism makes them print SKIP when the variable is unset.
//
// The verdict this file pins, and what it adds over PORT-153 (which already owns the thirteen rows' parse, values and hash):
//
//  * the constructor's values are the RW 0x82FF49 ones, field by field — the block is parsed but the AI still starts from them;
//  * ScavangedResourceBuildings (+0xA4) does have a reader, and its chain is CLOSED — the tactics generator RW 0x90C0EE creates the
//    prototype (`push 0x84` RW 0x90C1D4 -> `call RW 0x9B8C32` RW 0x90C1EF), the constructor stores the vtable RW 0xC89F18
//    (RW 0x9B8C83), slot 1 is the applicability RW 0x9B9E06 (it requires the army's list to be non-empty: RW 0x9B9E4E / 0x9B9E54 /
//    0x9B9E5B) and slot 7 the update RW 0x9BA005, which reaches RW 0x9B9B9B (RW 0x9BA071) and the round-robin picker RW 0x9B8D81
//    (RW 0x9B9C01), whose element read is RW 0x9B8E03. The tactic (LumberMillBuild, part of the unported targetless set, S-893) is
//    what keeps the field unread in the port — the reader no longer needs to be searched for;
//  * EconomyBuilderMinMoney (+0x44) and EconomyBuilderPerFarmValue (+0x48) have no reader: the economy builder RW 0x8EE765 loads the
//    army twice, +0x50 (RW 0x8EE7CC) and +0x40 (RW 0x8EE7DD), and compares built + inFlight with +0x40 (RW 0x8EE7F2); the donor's
//    affordability test (`money + total * perFarm <= limit`) does not exist in this build.

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

// every row of the issue, with the values the audit reports from the mod's data
const char kPort310Army[] = "ArmyDefinition Port310Army\r\n"
							"  Side = Port310Side\r\n"
							"  MaxThreatForOpportunityTargets = 10\r\n"
							"  ValueToSetForMaxOnDefenseTeam = 6\r\n"
							"  CombatChainSearchDepthForTeamRecruits_AttackTeams = 3\r\n"
							"  CombatChainSearchDepthForTeamRecruits_DefenseTeams = 8\r\n"
							"  CombatChainSearchDepthForTeamRecruits_ExploreTeams = 5\r\n"
							"  EconomyBuilderMinMoney = 200\r\n"
							"  EconomyBuilderPerFarmValue = 35\r\n"
							"  MaxBuildingsToBeDefensiveTarget_Small = 3\r\n"
							"  MaxBuildingsToBeDefensiveTarget_Med = 6\r\n"
							"  ChanceToUseAllUnitsForDefenseTarget_Small = 30%\r\n"
							"  ChanceToUseAllUnitsForDefenseTarget_Med = 60%\r\n"
							"  ChanceToUseAllUnitsForDefenseTarget_Large = 90%\r\n"
							"  ScavangedResourceBuildings = Port310LumberMill Port310Crypt\r\n"
							"End\r\n";
} // namespace

TEST_CASE("PORT-310: a block that writes none of the thirteen rows keeps the constructor's values, field by field (RW 0x82FF49)")
{
	Loader l;
	l.load("ArmyDefinition Port310Defaults\r\n"
		   "  Side = Port310DefaultSide\r\n"
		   "End\r\n");
	const ArmyDefinition *army = l.store.findArmy("Port310DefaultSide");
	REQUIRE(army != nullptr);
	// RW 0x82FF84 / 0x82FFA5 clear +0x2C and +0x30, RW 0x8300B7 / 0x8300D7 zero the 12 bytes at +0x34 and at +0x7C (memset 0xA3CF28),
	// RW 0x82FFB6 / 0x82FFB6 give +0x44 = 0x12C and +0x48 = 0x46, RW 0x82FFF7 / 0x82FFFE give +0x74 = 1 and +0x78 = 4 (RW 0x83002B
	// builds the empty list at +0xA4)
	CHECK(army->maxThreatForOpportunityTargets == 0);
	CHECK(army->valueToSetForMaxOnDefenseTeam == 0);
	CHECK(army->combatChainSearchDepthForTeamRecruits[0] == 0);
	CHECK(army->combatChainSearchDepthForTeamRecruits[1] == 0);
	CHECK(army->combatChainSearchDepthForTeamRecruits[2] == 0);
	CHECK(army->economyBuilderMinMoney == 300);
	CHECK(army->economyBuilderPerFarmValue == 70);
	CHECK(army->maxBuildingsToBeDefensiveTargetSmall == 1);
	CHECK(army->maxBuildingsToBeDefensiveTargetMed == 4);
	CHECK(army->chanceToUseAllUnitsForDefenseTarget[0] == 0.0f);
	CHECK(army->chanceToUseAllUnitsForDefenseTarget[1] == 0.0f);
	CHECK(army->chanceToUseAllUnitsForDefenseTarget[2] == 0.0f);
	CHECK(army->scavangedResourceBuildings.empty());
}

TEST_CASE("PORT-310: the thirteen rows keep their written values and all of them reach the store hash (guard: no reader may be added without the hash moving)")
{
	Loader l;
	l.load(kPort310Army);
	const ArmyDefinition *army = l.store.findArmy("Port310Side");
	REQUIRE(army != nullptr);
	CHECK(army->maxThreatForOpportunityTargets == 10);
	CHECK(army->valueToSetForMaxOnDefenseTeam == 6);
	CHECK(army->combatChainSearchDepthForTeamRecruits[0] == 3);
	CHECK(army->combatChainSearchDepthForTeamRecruits[1] == 8);
	CHECK(army->combatChainSearchDepthForTeamRecruits[2] == 5);
	CHECK(army->economyBuilderMinMoney == 200);
	CHECK(army->economyBuilderPerFarmValue == 35);
	CHECK(army->maxBuildingsToBeDefensiveTargetSmall == 3);
	CHECK(army->maxBuildingsToBeDefensiveTargetMed == 6);
	// the percent parser RW 0x42EEFA multiplies by 0.01f in a 24-bit x87 register: the stored value is the float product, not 0.3
	CHECK(army->chanceToUseAllUnitsForDefenseTarget[0] == 30.0f * 0.01f);
	CHECK(army->chanceToUseAllUnitsForDefenseTarget[1] == 60.0f * 0.01f);
	CHECK(army->chanceToUseAllUnitsForDefenseTarget[2] == 90.0f * 0.01f);
	REQUIRE(army->scavangedResourceBuildings.size() == 2);
	CHECK(army->scavangedResourceBuildings[0] == "Port310LumberMill");
	CHECK(army->scavangedResourceBuildings[1] == "Port310Crypt");

	// PROCESS 15.5: the rows are simulation state even while only one of them has a (not yet ported) reader
	auto hash = [&] {
		StateHasher h;
		l.store.crc(h);
		return h.value();
	};
	ArmyDefinition &mutableArmy = const_cast<ArmyDefinition &>(*l.store.findArmy("Port310Side"));
	std::uint32_t before = hash();
	mutableArmy.maxThreatForOpportunityTargets = 11;
	CHECK(hash() != before);
	before = hash();
	mutableArmy.valueToSetForMaxOnDefenseTeam = 7;
	CHECK(hash() != before);
	before = hash();
	mutableArmy.combatChainSearchDepthForTeamRecruits[2] = 6;
	CHECK(hash() != before);
	before = hash();
	mutableArmy.economyBuilderMinMoney = 201;
	CHECK(hash() != before);
	before = hash();
	mutableArmy.economyBuilderPerFarmValue = 36;
	CHECK(hash() != before);
	before = hash();
	mutableArmy.maxBuildingsToBeDefensiveTargetMed = 7;
	CHECK(hash() != before);
	before = hash();
	mutableArmy.chanceToUseAllUnitsForDefenseTarget[2] = 0.75f;
	CHECK(hash() != before);
}

TEST_CASE("PORT-310: RW_GAME_DAT binary fact: the LumberMillBuild tactic's create -> constructor -> vtable -> slot chain is closed, and its applicability requires the scavanged list to be non-empty")
{
	const PeImage *pe = PeImage::fromEnvironment();
	if (pe == nullptr)
	{
		retailtest::printSkip("PORT-310 the LumberMillBuild tactic chain (set RW_GAME_DAT)");
		return;
	}
	// (1) the tactics generator RW 0x90C0EE: `push 0x84` (RW 0x90C1D4, 68 84 00 00 00) then `call RW 0x9B8C32` (RW 0x90C1EF, E8 3E CA 0A 00)
	CHECK(pe->u32At(0x90C1D4) == 0x00008468);
	CHECK(pe->u32At(0x90C1EF) == 0x0ACA3EE8);
	// (2) the constructor stores the vtable: `mov dword ptr [esi], 0xC89F18` (RW 0x9B8C83, C7 06 18 9F C8 00)
	CHECK(pe->u32At(0x9B8C83) == 0x9F1806C7);
	// (3) the table's slots: 1 the applicability RW 0x9B9E06, 6 the setup RW 0x9B8E1E, 7 the update RW 0x9BA005, 12 the clone RW 0x9B8CD2
	CHECK(pe->u32At(0xC89F18 + 0x04) == 0x009B9E06);
	CHECK(pe->u32At(0xC89F18 + 0x18) == 0x009B8E1E);
	CHECK(pe->u32At(0xC89F18 + 0x1C) == 0x009BA005);
	CHECK(pe->u32At(0xC89F18 + 0x30) == 0x009B8CD2);
	// (4) the update RW 0x9BA005 calls RW 0x9B9B9B (RW 0x9BA071, E8 25 FB FF FF), which calls the picker RW 0x9B8D81 (RW 0x9B9C01, E8 7B F1 FF FF)
	CHECK(pe->u32At(0x9BA071) == 0xFFFB25E8);
	CHECK(pe->u32At(0x9B9C01) == 0xFFF17BE8);
	// (5) the applicability RW 0x9B9E06 on the army's list: `mov eax, [edi + 0x160]` (RW 0x9B9E4E), `add eax, 0xA4` (RW 0x9B9E54),
	//     `mov ecx, [eax]; cmp ecx, [eax + 4]; je` (RW 0x9B9E59 / 0x9B9E5B): begin == end -> not applicable
	CHECK(pe->u32At(0x9B9E4E) == 0x0160878B);
	CHECK(pe->u32At(0x9B9E54) == 0x0000A405);
	CHECK(pe->u32At(0x9B9E59) == 0x483B088B);
	CHECK(pe->u32At(0x9B9E5B) == 0x7404483B);
	// (6) the picker's element reads: begin `mov eax, [eax + 0xA4]` (RW 0x9B8E03), `lea eax, [eax + ecx*4]` (RW 0x9B8E09)
	CHECK(pe->u32At(0x9B8E03) == 0x00A4808B);
	CHECK(pe->u32At(0x9B8E09) == 0x8B88048D);
	// and the list's end at RW 0x9B8DAB, inside the same picker
	CHECK(pe->u32At(0x9B8DAB) == 0x00A8888B);
}

TEST_CASE("PORT-310: RW_GAME_DAT binary fact: the economy builder reads EconomyBuilderMinFarmsOwned only, never EconomyBuilderMinMoney or PerFarmValue")
{
	const PeImage *pe = PeImage::fromEnvironment();
	if (pe == nullptr)
	{
		retailtest::printSkip("PORT-310 the economy builder's army read (set RW_GAME_DAT)");
		return;
	}
	// RW 0x8EE7C6 `mov edx, [eax + 0x160]` then RW 0x8EE7CC `fmul dword ptr [edx + 0x50]` — EconomyBuilderMinTimeBetweenFarms_Rush
	CHECK(pe->u32At(0x8EE7C6) == 0x0160908B);
	CHECK(pe->u32At(0x8EE7CC) == 0xD9504AD8);
	// RW 0x8EE7D7 `mov edx, [eax + 0x160]`, RW 0x8EE7DD `mov edx, [edx + 0x40]` — EconomyBuilderMinFarmsOwned
	CHECK(pe->u32At(0x8EE7D7) == 0x0160908B);
	CHECK(pe->u32At(0x8EE7DD) == 0x8B40528B);
	// RW 0x8EE7E4 `mov dword ptr [ebx], 0x7D0` writes the 2000-money reserve the caller asks for, then RW 0x8EE7EA.. sums the built and
	// in-flight farm counts and RW 0x8EE7F2 `cmp esi, edx` (3B F2) decides against +0x40; there is no money * perFarm term anywhere in the body
	CHECK(pe->u32At(0x8EE7E4) == 0x07D003C7); // C7 03 D0 07 00 00
	CHECK(pe->u32At(0x8EE7F0) == 0xF23BF703); // 03 F7 3B F2
	CHECK(pe->u32At(0x8EE7F2) == 0x0473F23B);
}
