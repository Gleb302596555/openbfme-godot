// OpenBFME. GPL-3.0.
//
// BannerCarrierUpdate (lane HORDE-2): the module of a horde's banner carrier. While its horde has been out of combat long enough it brings the horde back to strength, one new
// member every IdleSpawnRate. The banner carrier itself is spawned by its horde (HordeContain, HordeBanner.cpp).
//
// TARGET FACTS (RotWK game.dat, caveat S-001; read with Ghidra):
//   * registry: create RW 0x64DE4C (ctor RW 0x89A258, 0x20 bytes), data table RW 0xC66090: IdleSpawnRate +8, MeleeFreeUnitSpawnTime +0xC, DiedRespawnTime +0x10,
//     MeleeFreeBannerReSpawnTime +0x14 (durations), MorphCondition +0x18 (RW 0x89A90F), ExpLevelDraw +0x24 (RW 0x89AA69), BannerMorphFX / UnitSpawnFX +0x30 / +0x34 (FX lists),
//     ReplenishNearbyHorde +0x38, ReplenishAllNearbyHordes +0x39 (Bool), ScanHordeDistance +0x3C (Real), UpgradeRequired +0x40 (AsciiString);
//   * update RW 0x89ACE4 (sleeps IdleSpawnRate; BFME2 0x897524, byte-matched C++ in Open-BFME-2): with UpgradeRequired empty or owned, the carrier not in model
//     condition 69 / 544, not PENDING_CONSTRUCTION (87), not TEMPORARILY_DEFECTED (62), not moving and not waiting for a path, and the emotion tracker (Object + 0x254)
//     of its horde object (RW 0x693A1A: itself when a HORDE, else its HORDE container; the carrier's own without one) not running a POINT / FEAR /
//     UNCONTROLLABLE_FEAR / TERROR / DOOM / BRACE_FOR_BEING_CRUSHED nugget (RW 0x8B50A5 -> 0x8B4F39): with ReplenishNearbyHorde RW 0x89AB8F scans allied hordes
//     nearest first, skips UNDER_CONSTRUCTION, replenishes one member per horde (one/all), updates shroud and emits UnitSpawnFX; a full horde's banner countdown
//     drops to 1. Without it, when the carrier is not in model condition ATTACKING (37) / MOVING (61), its CONTAINER has not fired after frame -
//     MeleeFreeUnitSpawnTime (RW 0x68B645 on Object + 0x27C, unsigned) and the container's horde was not attacked within MeleeFreeUnitSpawnTime (HordeContainInterface
//     slot 0x90, RW 0x86EE52: a member removed within the window, or a member damaged within 4 seconds, RW 0x68C933), the horde gets a member (RW 0x89A392: fewer
//     members than slots, RW 0x89A308 (a GARRISON top container needs its AllowBannerSpawnUpgrade upgraded), the container's player's ScoreKeeper off (RW
//     0x79DC9E), HordeContainInterface slot 0x18C RW 0x873AE3 at the carrier's matrix, RW 0x68C7E9 (shroud), UnitSpawnFX at the member).
//   * lane BANNER-1 (owner, nightly 2026-10-22: "banner carriers don't auto replace units if they are lvl 2"): HORDE-2 asked the carrier's own weapons and a stand-in
//     combat test (a member IS_ATTACKING, or damaged within MeleeFreeUnitSpawnTime), skipped the emotion and garrison gates and the spawn's hand-downs: the
//     replacement arrived at level one, without UnitSpawnFX.
//   * lane PORT-75 (issue #207), MorphCondition RW 0x89A90F / BannerMorphFX + 0x30 (RW 0xC6600C): the grammar is
//     `UnitType:<ThingTemplate> [Locomotor:<LocomotorSet>] ModelState:"<model conditions>"` (DONOR BFME2 0x0049714F, 345 B, same three tokens) and the parsed
//     entry is {ThingTemplate name +0x00, 19-word model-condition set +0x08, LocomotorSet name +0x54} (DONOR BFME2 0x00496ADE ctor, sizeof 0x5C).
//     The morph step RW 0x89A5C5 (DONOR: BFME2 0x00496E05, 299 B, ret 8; the address follows from the TU's constant shift 0x4037C0, matched by the six RW
//     addresses above): look the entry up by the template name (miss = an all-zero set, DONOR BFME2 0x00496D43 -> Rva0028F59A(0, 0xb8)), clear the 81
//     morph bits (DONOR BFME2 .rdata 0x00C4F6E0 = MOUNTED, PASSENGER_VARIATION_1..5, USER_1..USER_75 of RW 0xD9FAD8) and set the entry's,
//     clearAndSetModelConditionFlags RW 0x68D607, updateShroudNow RW 0x68C7E9 when the position is non-zero, BannerMorphFX (RW 0x4B1B5A) when the
//     caller asks (the FX plays even when no line matched), then the entry's LocomotorSet through the AI (DONOR BFME2 0x00496DA3 -> index).
//     Its only caller is the contain's accept path (DONOR BFME2 0x00473212: an object of BannerCarriersAllowed -> 0x00473197) with the template of the
//     object the carrier joins for, i.e. the horde it leads (DONOR 0x0046A521 getBannerCarrierIndexToUse); ported as HordeContain::bannerCarrierMorph.
// INFERENCE / NOT PORTED (stop S-587): ExpLevelDraw keeps its raw tokens (the grammar RW 0x89AA69 is not read; drawing only); the model
// condition / status gates are read by number from the binary's registries; the FX are the client's; the morph step's own address is derived by the TU
// shift (the binary itself was not in this sandbox's reach: port75-analysis.md).

#pragma once

#include "Common/INI.h"
#include "Common/ModelState.h"
#include "GameLogic/Module/UpdateModule.h"

#include <string>
#include <vector>

class ModuleFactory;

class BannerCarrierUpdateModuleData : public ModuleData
{
public:
	unsigned m_idleSpawnRate = 0;              ///< +8 (frames)
	unsigned m_meleeFreeUnitSpawnTime = 0;     ///< +0xC
	unsigned m_diedRespawnTime = 0;            ///< +0x10
	unsigned m_meleeFreeBannerReSpawnTime = 0; ///< +0x14
	/// lane PORT-75: one MorphCondition line (DONOR BFME2 0x00496ADE, the record RW 0x89A90F's parser builds)
	struct BannerMorphCondition
	{
		std::string unitType;           ///< +0x00: the ThingTemplate name the line matches
		ModelConditionFlags modelState; ///< +0x08: the quoted ModelState as a bit set (19 words / 0x4C, RW 0x4B5E39; names RW 0xD9FAD8)
		std::string locomotor;          ///< +0x54: the LocomotorSet name, empty when the line names none
	};
	std::vector<BannerMorphCondition> m_morphConditions; ///< +0x18
	std::vector<std::string> m_expLevelDraws;   ///< +0x24 raw lines
	std::string m_bannerMorphFX;               ///< +0x30
	std::string m_unitSpawnFX;                 ///< +0x34
	bool m_replenishNearbyHorde = false;       ///< +0x38
	bool m_replenishAllNearbyHordes = false;   ///< +0x39
	float m_scanHordeDistance = 0.0f;          ///< +0x3C
	std::string m_upgradeRequired;             ///< +0x40
	static void buildFieldParse(MultiIniFieldParse &p);
	static void parseMorphCondition(INI *ini, void *instance, void *store, const void *userData); // RW 0x89A90F (table row 0xC66090 + 0x18)
	static void parseRawLine(INI *ini, void *instance, void *store, const void *userData);
};

class BannerCarrierUpdate : public UpdateModule
{
public:
	OBFME_MODULE_XFER(BannerCarrierUpdate) // lane SAVE-4b
	BannerCarrierUpdate(Thing *thing, const BannerCarrierUpdateModuleData *data);
	static void registerClass(ModuleFactory &modules);
	UpdateSleepTime update() override; // RW 0x89ACE4
	// lane PORT-75, RW 0x89A5C5 (DONOR BFME2 0x00496E05): the morph step. `unitType` is the ThingTemplate name the MorphCondition lines are matched by;
	// retail applies it from the contain's accept path when a banner carrier joins, with the template of the horde it leads.
	void morphForTemplate(const std::string &unitType, bool playFX);
	// lane BANNER-1: the update interface vtable RW 0xC65ED4 slot 1 is RW 0x855830: the carrier also runs while DISABLED_HELD (3), e.g. inside a garrison
	DisabledMaskType getDisabledTypesToProcess() const override { return (DisabledMaskType)(1u << 3); }
	const BannerCarrierUpdateModuleData &data() const { return *m_data; }
	unsigned long long membersSpawned() const { return m_spawned; }
	void crc(StateHasher &hasher) const override;
	static const char *stopLine();

	friend struct Horde2HashAccess; // the HORDE-2 hash mutation tests (test_horde2_hash.cpp)

private:
	bool spawnMember(); // RW 0x89A392
	const BannerCarrierUpdateModuleData *m_data;
	unsigned long long m_spawned = 0;
};
