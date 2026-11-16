# Port 238: Frodo / dropped ring, partial trace

This is a bounded trace of literal Ennorath 1.9.8.3 declarations, not a
complete effective-object or ability/visual audit. References are relative to
`data/ini`; archived INI MD5 `c53b6dea885cab044f80e783565f41f3`.

## Literal links

* `object/goodfaction/units/men/frodo.ini:1085` declares `RohanFrodo_mod`;
  its direct `CommandSet` at 1911 is `RohanFrodoModRingCommandSet_Galadriel`.
  That set's slot 3 is `Command_OneRing` (`includes/commandset.inc:9975`;
  button definition `includes/commandbutton.inc:20613`).
* The object declares `StealthUpdate` at 2242 and `OneRingPenaltyUpdate` at
  2262. Its penalty module names `EyeOfSauronFromRing`, declared in
  `object/evilfaction/units/mordor/eyeofsauron.ini:146`.
* `GaladrielRohanFrodo_mod` is a child at 2369. It declares a `GrantUpgradeCreate`
  with `Upgrade_RingHero` and `Upgrade_FortressRingHero`, the same command set,
  and `CreateObjectDie` with `OCL_TheOneRing`. `RohanFrodoRing` is a separate
  object at 2385, with its own stealth/penalty and death OCL declarations;
  inheritance and the circumstances that spawn these forms remain unresolved.
* `objectcreationlist.ini:9566` defines `OCL_TheOneRing` as creating
  `TheDroppedRing`; `crate.ini:1039` declares it with `ONE_RING`, an
  `AttachUpdate` setting parent status `HOLDING_THE_RING`, and a death OCL.
  This is a definition graph, not proof that every route creates, drops or
  transfers the ring in a running game.

## Bounded behavior status

| Piece | Status and evidence | Remaining limits |
| --- | --- | --- |
| ONE_RING command, single eligible stealth module | Ported in this branch for availability and dispatch: RW `0x942733`, `0x940435`, `0x7761DD`; `PORT-238*` passed in isolated runner | Multi-module selection S-7426; full suite and in-game test pending; visuals S-1041 |
| Dropped ring attachment | Existing port implements the scan and parent status (`AttachUpdate.cpp`); RW `0x89515C` selects and records parent, `0x8953BB` follows or kills the object after parent death. Isolated `PORT-124*`: 3 cases, 68 assertions passed | S-1361 covers horde member selection / anchor details and Eva name validation; PORT-124 uses a synthetic name-only filter, not the Ennorath macro; no Ennorath end-to-end claim |
| Ring-wear penalty and Eye lifecycle | Existing port `OneRingPenaltyUpdate.cpp`; RW `0x89D647` schedules spawn/movement based on ring state and `0x89D41B` takes off the ring, clears CAN_STEALTH and kills the Eye | `modgap1 OneRingPenaltyUpdate*` passed in an isolated runner: 2 cases, 36 assertions. Lookup S-7426 and client effects S-1041 remain; not a full visual audit |
| `TimeFrozenFromPenalty` | Existing closed S-5604 / PORT-30 verdict: parsed data `+0x1C`, no RW reader found by that prior audit; donor's data view has the slot and constructor zero | We re-read RW `0x89D647`, `0x89D41B`, `0x89D4D2` and donor data/constructor, **not** a new complete whole-image proof. Isolated `PORT-30*`: 3 cases, 1014 assertions passed; tests alone do not prove no reader |
| CreateObjectDie / OCL | Literal declaration and OCL link identified; port has the module and OCL system | S-980: generic object-pair death context is incomplete. Whether it matters to this exact OCL and every death route is not established |
| Two names in `GrantUpgradeCreate` | Ported bounded parser/create behavior: RW field table `0xC70158` calls `0x42EE5E` / `0x42E757` (one unquoted token), `0x8BD15E` checks the construction mask; the synthetic `PORT-238*` test exercises two tokens, default and explicit mask, and build completion | Test uses synthetic OBJECT upgrades and explicitly calls build completion; effective PLAYER upgrade types, ChildObject load and actual creation/build lifecycle remain to be checked in context |
| Other commands, upgrades and model states | Not yet audited | Enumerate command set / module links and compare each with RW; do not treat naming or a parser entry as proof of behavior |

The isolated runner in this file is a small temporary executable linked against
`libopenbfme_core.a`, not the required complete `openbfme_tests` binary. No game
or mod files were committed and no in-game run has been performed.
