// OpenBFME. GPL-3.0.
//
// Lane MODS-3: the counters of Object::doCommandButton and its target forms (RW 0x696FD2 / 0x697890 / 0x6979D9, GameLogic/Object/ObjectDoCommandButton.cpp
// holds the target facts). Process-wide diagnostics, not simulation state (never hashed or saved).

#pragma once

namespace ObjectDoCommandButton
{
struct Stats
{
	unsigned long long pressed = 0;                 ///< buttons that passed the disabled gate
	unsigned long long builds = 0;                  ///< queueCreateUnit calls of the build cases
	unsigned long long upgrades = 0;                ///< queueUpgrade calls
	unsigned long long powers = 0;                  ///< doSpecialPower* calls
	unsigned long long toggles = 0;                 ///< TOGGLE_WEAPONSET
	unsigned long long castles = 0;                 ///< castle unpacks / packs started
	unsigned long long gates = 0;                   ///< TOGGLE_GATE opens / closes
	unsigned long long voiceResponsesNotPlayed = 0; ///< the gate case's selection voice (client sound, RW 0x8DEDBB)
	unsigned long long unported = 0;                ///< the cases of stop S-5305 met
	unsigned long long formationToggles = 0;        ///< HORDE_TOGGLE_FORMATION (RW 0x697204 / 0x77BE83)
	unsigned long long fireWeapons = 0;             ///< FIRE_WEAPON (RW 0x697126 / 0x697993 / 0x697A91)
	unsigned long long attackMoves = 0;             ///< ATTACK_MOVE (RW 0x697ACC / 0x696266)
};
const Stats &stats();
void resetStats();
} // namespace ObjectDoCommandButton
