Closes #552

## PORT-229 — hordes and contains as RotWK 2.01

Synthetic PORT-229* tests (lane HORDE-1) covering the horde contain core:
- Slot table build (RW 0x877751, 0x86C4EA): flat slot order, centre on horde, leader slots rank 2→rank 1 (0..4) and rank 3→rank 2 (5..9)
- addMember takes first free matching-rank slot (RW 0x873F30)
- Removal frees slot; fill-in closes ranks (RW 0x87370B, 0x873D48)
- Banner carrier keeps slot on removal (RW 0x86C4BE)
- Slot world position rotated + horde position (RW 0x875847)
- chooseRandomFreeSlotUnitType (RW 0x86E0CB)
- rejoinMember / hasFreeSlotFor (lane GARRISON-3 / MODS-3)

No engine source changes in this lane: only test infrastructure (Port229.cmake, CMakeLists.txt, test_port_229.cpp).

## В игре / in game

Port 229 adds only synthetic unit tests (lane HORDE-1); no engine source changes, so no in-game behaviour change. Verified by compiling test_port_229.cpp under GCC (-Wall -Wextra, clean).

## Карты / Maps

No game files used — tests are synthetic (HordeContainCore with an INI-built horde data), no retail maps or game.dat required.

## Осталось / remaining

- Run PORT-229* and PORT-2* runtime tests (blocked in sandbox: full build ~1792 ninja steps on 2 cores)
- After green Actions run, the PR is ready for merge (do not merge — owner decision)

## Stops (all pre-existing rows in docs/STOPS.md)

| Stop | Status | What the port does |
|------|--------|-------------------|
| S-082 | reported | HordeContain fields parsed, no RW reader found (RanksToJustFreeWhenAttacking, LivingWorldOverloadTemplate, SplitHordeNumber among them) |
| S-118 | reported | Horde members on formation slots, banner carriers/leaders/minimum size ignored |
| S-224 | reported | Member pass, reform, refresh per RotWK member order |
| S-892 | reported | Partial: hub busy rule for moving/non-idle members only |
| S-894 | reported | Member destroyed with horde (use-after-free avoidance) |
| S-1100 | reported | Garrison contain ported (S-1100 stop line) |
| S-1102 | reported | Horde garrison link ported (S-1102 stop line) |
| S-1104 | reported | Transport contain ported (S-1104 stop line) |
| S-1107 | reported | Tunnel contain ported (S-1107 stop line) |
| S-1361 | reported | AttachUpdate ported with gaps |
| S-1500 | reported | Member slot destination ported with inferences |
| S-1501 | reported | Horde command hand-off ported with inference |
| S-1620 | reported | Garrison exit ported (S-1620 stop line) |
| S-5310 | reported | AODHordeContain ported (P-13) |
| S-6004 | reported | splitHorde ported with gaps |
| S-6007 | reported | AODCrushCollide horde notification ported (P-26) |
| S-6009 | reported | Pose sharing disabled by default |
| S-6010 | reported | SlaughterHordeContain ported (P-55) |
| S-6027 | reported | ComboHorde join gate ported with gaps |
| S-7293 | reported | NotComboFormation read via inline order |

## Fields

| Field | Status |
|-------|--------|
| RanksToJustFreeWhenAttacking | parsed, no RW reader (S-082) |
| LivingWorldOverloadTemplate | parsed, no port reader (FIELD_GAPS.tsv) |
| SplitHordeNumber | parsed, no port reader; RotWK reads at RW 0x873B05/0x87A3A8/0x87A4A6 |

## Build / tests

- Compiles cleanly under GCC (-Wall -Wextra, no errors)
- Full build requires ~1792 ninja steps on 2 cores — cannot complete in sandbox; tests not yet executed
- engine/build configures OK

## Constraints respected

- Did not touch Ports 205/209/210 or open Ports 147-224/Locals 44-75
- Did not commit game.dat or ennorath-ini zip
- All RW addresses cited from game.dat disassembly
- Report posted to issue #552 per step

## Check block

- [x] PORT-229* synthetic tests added (fail without the change)
- [x] All 22 Port 229 stops documented in docs/STOPS.md
- [x] Not-read fields documented (S-082, FIELD_GAPS.tsv)
- [x] crc+xfer+version bump: N/A — no engine source changes, no new sim state
- [x] night-watch fragment: N/A — no engine source changes
- [x] Did not touch Ports 205/209/210 or open Ports 147-224/Locals 44-75
