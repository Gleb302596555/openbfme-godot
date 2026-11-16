// OpenBFME. GPL-3.0.
//
// The acceptance stops of lane ECON-1 (S-250 .. S-269, docs/STOPS.md) as report lines. Reported by Economy::report(); pinned by tests/test_econ_*.cpp.

#pragma once

#include <string>
#include <vector>

namespace EconomyStops
{
inline std::vector<std::string> lines()
{
	return {
		"[S-250] TerrainResourceManager: the extent comes from TerrainLogic::getExtent (lo (0, 0), hi = the active boundary * 10) as W3DTerrainLogic does in ZH; retail's body behind TerrainLogic vslot 0x180 "
		"was not read. The blocked cells (cell types 1, 2, 5, 7 of the pathfinder's ground layer) need a Pathfinder: the live game has none yet, so a map's grid is built from a private "
		"Pathfinder (PathfinderResourceTerrain). The client half is ported since lane S-5030 (the claimable share of RW 0x75C02D with RW 0x75C1A9's vertex alphas, the selected resource building's "
		"claim decals, the placement's arcs over RW 0x75B8CC's claimants); the dead-owner erase that RW 0x75C02D does when a client asks with a player filter still never runs",
		"[S-251] ScoreKeeper is ported (lane END-1 / END-2 / SPELL-1: the money counters of RW + 4 / + 8 / + 0x114 and the rest of the class, calculateScore RW 0x79DFFA, the per-frame "
		"statistics RW 0x79F704, the favourite unit RW 0x79E3A8); the keep-score switch (GameLogic + 0x98) defaults to on and is cleared when one alliance is left (RW 0x80902C, unless the game is "
		"a sandbox), and the game kinds that switch it off were not recovered",
		"[S-253] the BOUNTY_PERCENTAGE (0x11) attribute modifier sum is installed (the seam answers any type as RW 0x68C818 does, so RW 0x6AC0B5 .. 0x6AC0DB's killer sum is active) and "
		"AutoDepositUpdate's RUBBLE / POST_RUBBLE / POST_COLLAPSE / GARRISONED tests read the object's own model condition flags (RW 0x89DBA9 bit 10 and RW 0x89DBF0 .. 0x89DC25 the bits "
		"5 / 59 / 60, both through RW 0x46E918). Left: the two construction tests are approximated by the construction statuses — `completedOnly` (RW 0x7A075A tests AWAITING_CONSTRUCTION / "
		"PARTIALLY_CONSTRUCTED) and CostModifierUpgrade's cost gate (RW 0x6AD8F3, model condition 0x44)",
		"[S-254] game context: that GameLogic + 0x114 is 3 in a skirmish (RW 0x6A7C86 and 0x602E64 branch on it) is inference, the new-game message handler (RW 0x779CB4) writes it from "
		"message argument 1 and the sender was not located; the Living World / campaign branches of the command point initialisation (RW 0x6A7CEB, 0x6A7ED1 bonuses) and the "
		"campaign money handicap (RW 0x6AA858 .. 0x6E1F2F) are not ported; the replay clause of the multiplayer test (RW 0x625456) is not ported",
		"[S-255] MultiPlayMoneyMult is parsed for MP1 .. MP8; the retail lookup reads 20 slots (RW 0x642002), the slots past MP8 keep GlobalData's constructor value, assumed 1.0",
		"[S-256] the experience a resource building earns from its income (ExperienceTracker, RW 0x79D833) is dropped unless Economy::setExperienceHook is installed (counted); the "
		"original owner of a TEMPORARILY_DEFECTED object (RW + 0x320, the DefectionHelper's) is not tracked, so its command points leave the CURRENT owner",
		"[S-259] the live playable player count (the MPn command point tier and the MultiPlayMoneyMult index, RW 0x6A8630) counts every player of the map whose template has "
		"PlayableSide and that is not an observer or defeated: the map's placeholder skirmish sides (SkirmishElves .. SkirmishAngmar) count unless a map script defeated or "
		"re-templated them first; map scripts do not run in this build and the lobby's slot count is not consulted, so the tier follows the map's own player list (RotWK 2.01 "
		"retail behaviour on that point was not verified in a running game)",
		"[S-260] SalvageCrateCollide (RW create 0x651052, data 0x655980) is ported (lane MODCOMPAT-MOD1: onCollide RW 0x8C06B6, isValidToExecute RW 0x8C080B / "
		"0x8C145D, executeCrateBehavior RW 0x8C158B): a unit that touches a crate gains a level, gets its player's upgrade or pays its player the crate's money "
		"(the multiplayer money multiplier and the handicap as RW 0x8C148B), and the crate is destroyed. Not ported: the crate's contacts come from AIWorld's "
		"crate pass (each crate against the overlapping AI objects, not RW's collision manager: S-780); the \"GUI:GainRank\" floating text and the ExecuteAnimation "
		"(client) are not shown; the porter / banner branch (RW 0x8C168D) is unreachable in 2.01 and not ported" };
}
} // namespace EconomyStops
