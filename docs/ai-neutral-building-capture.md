# AI capture of neutral buildings

`FlagCaptureSquad` now uses the existing capture special power to take flags
linked to inns, outposts and other capturable buildings. This restores a
missing targetless tactic; it does not change unit command-point limits.

The requested donor reference is Open-BFME/openbfme2's
`Code/GameEngine/Source/GameLogic/SkirmishAI/AITacticalAI/AITargetChooser/AITargetHeuristics/AITargetHeuristicTechBuilding.cpp`.
The consulted donor checkout is commit `9b178fd8f50e6f0635c44896b0177463877eb214`.
Its tech-building selector supplies the reference for registered capture
targets, rejecting allies and preserving the first nearest-distance tie.
Its `AIFarmKillSquad.cpp` supplies the related targetless squad lifecycle.

RotWK differs from BFME2: the OPPORTUNITY tech-building heuristic skips
objects still marked CAPTUREFLAG, and its capture special-power picker
returns false. Consequently this change ports RotWK's dedicated squad
rather than enabling BFME2's nearby automatic capture picker.

The RotWK reference functions are:

| Function | Address | Behavior |
| --- | --- | --- |
| Manager registration | 0x6A9D9C | Append each capture flag once; unregister at 0x6A99CB retains history |
| Applicability | 0x9BC39F / 0x9BC303 | No running squad, cooldown elapsed, non-allied capture flag available |
| Setup | 0x9BC1A0 | One team, priority 30, minimum 1, random maximum 1–3; forbid HERO, MACHINE and DOZER |
| Eligibility | 0x9BC0EE | A living CAN_ATTACK member and no member with status 0x66, SUMMONING_REPLACEMENT |
| Selection | 0x9BC213 | Nearest flag to team 0, squared 2D distance, first tie, no maximum distance |
| Order | 0x8F1933 | Command_CaptureBuilding through the ordinary object command path |
| Update | 0x9BC3F4 | Keep an active non-allied target; otherwise retarget when idle or finish |
| Cleanup | 0x9BBFDC | Clear running counter; next attempt after a random 20–90 seconds |

These addresses use the existing RW reference and its S-001 provenance
caveat; they do not establish parity with a verified clean retail binary.
Inference: because this port installs world hooks after creating map
objects, initial flags are seeded in ObjectID order to recover creation
order. Later registration preserves event order. Dead IDs remain in the
registry and are skipped during selection.

The registry is hashed and saved in SkirmishAI snapshot version 3. Earlier
versions are rejected because they do not contain registration history.
Squad state uses the existing serialized tactic target and named counters.
The separate OPPORTUNITY tech-building heuristic remains unported.

The common tactic lifecycle also preserves pending recruitment teams until
handover. Retail prunes its active team list at 0x8F28DD; pending prototypes
live separately at tactic +4 (0x8F232F), before handover via 0x6C779B.
The port stores both in one vector and previously deleted empty pending
teams. This prevented squads created before troops were available from
ever recruiting them.

Unit tests cover registration, ownership changes, destroyed IDs, snapshot
round trips and malformed registry data. Match scenarios are outside the
requested validation scope.
