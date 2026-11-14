# Spell-book casting: investigation in progress (task 155)

This is a partial map, not a diagnosis of the intermittent failures. No game or
mod data is reproduced here. Data was checked in the merged Ennorath INI; the
Gondor book uses an object-targeted Rebuild command and a no-target Cloud Break
command. The older `Command_SpellBookRebuild` is a different, position-targeted
button: follow the actual book command set, not the similarly named button.

| Step | Retail evidence read with `rw.py dis` | Port | Status |
|---|---|---|---|
| Movie callback | RW 0x930DE5..0x930E21 bounds-checks the slot, reads its button and calls ControlBar; DONOR BFME2 0x0052A3AA (`AptInGameSpellBookInterface.cpp`) | `AptPalantir::pressSpellSlot`, `InGameHud::pressSpell` | Callback and slot routing exist; movie input failures not yet isolated. |
| Availability | RW 0x942775..0x942795 selects the local player's book for command type 0x26; RW 0x943343..0x943433 checks button visibility, owner, sciences and prevention; RW 0x943569..0x9435D2 checks module requirements, readiness and pause. DONOR BFME2 0x0053BD66 (`ControlBarRappellerCount.cpp`). | `InGameSpellBookModel::refresh` computes ownership, module readiness and `canUseSpecialPower`; the separate `ControlBar::evaluate` also handles visibility and disabled/hidden sciences. | Not the same implementation; conditions are still being traced end-to-end, and no connection to the intermittent failure has been demonstrated. |
| Target mode | RW 0x94086D..0x9408B1 tests targeting options and clears the current command when it is pressed again; DONOR BFME2 0x00539A65 (`ControlBarCommandProcessing.cpp`) | `InGameSpellBookModel::press` always assigns the index on a targeted press. `ControlBarCommandProcessing.cpp` already toggles selected-object powers. | Apparent divergence in the separate spell-book path; its role in the reported failure is **not established**. Check interactions with other commands and readiness before changing it. |
| No-target button | RW 0x9408B3..0x9408C9 dispatches via the command-type tables; type 0x26 reaches RW 0x941423. RW 0x94142B..0x941456 posts message 0x456 with power ID, button options and invalid object; RW 0x94145B..0x9414E0 sends the chained commands. DONOR BFME2 0x00539A65 (`ControlBarCommandProcessing.cpp`), case 0x26. | `InGameSpellBookModel::press` emits `MSG_DO_SPECIAL_POWER` with zero options and the book ID, without chaining. | Divergence established in routing; whether it causes either reported failure is **not established**. Trace the message handlers and all affected commands before a change. |
| Object message | RW 0x77A5D7..0x77A5FE resolves the target and exits if it is missing. | `SpellCommands::doSpecialPower`, kind 2 | One refusal confirmed; remaining branches not yet mapped. |
| Special power gate | RW 0x8980A3..0x8980EC and RW 0x89816C..0x8981BB bypass pause/disabled checks for the force option; otherwise they return on pause or disabled. They call RW 0x897E87 and, unless the update-module flag is set, RW 0x897987. | `SpecialPowerModule::initiate` / `baseDo` | Gate present; subclass-specific checks and all option paths still need comparison. |
| Recharge | RW 0x8979AE..0x8979C2 conditionally calls the recharge virtual slot. | `SpecialPowerModule::triggerSpecialPower` | Further trace of its conditions remains. |

## Unread / unresolved

* Remaining common availability gates in RW 0x942733, movie state transitions
  and pointer dispatch. Check whether a shared control-bar mechanism, rather
  than a spell-book-local one, would need modification before proposing a fix.
* Every message/refusal path, source ownership differences, object targeting,
  special-power requirements and subclass behavior; no deterministic reproduction
  or failing synthetic test exists yet. No claim that the observed divergences
  explain Cloud Break or the intermittent runs is justified yet.
* Baseline full `openbfme_tests` build is in progress; no `PORT-432*` tests have
  been added. The cause, fix, NIGHT_WATCH entry and PR remain outstanding.

## Follow-up trace: distinguish routing from the observed failure

* The mod's `SpellBookRebuild` has enum `SPECIAL_AT_VISIBLE_OBJECT` (0x81),
  not the commented-out `SPECIAL_SPELL_BOOK_REBUILD` (0x57). Its object
  filter and the actual button's ally-object option are both relevant.
  For an outside-GUI left press, `InGameHud::mouseButton` picks a drawable;
  failure to pick it, a failed `objectValid` relationship/filter test, or
  disabled script input sends no message and leaves targeting pending.
  `objectValid` is explicitly an inference (S-2000), not a complete RW
  object-target comparison. A test must distinguish these cases rather than
  attributing all of them to `SpellCommands`.
* The no-target Gondor Cloud Break button in the mod names neither an option
  nor a `CommandTrigger`. Therefore the missing chained-message behaviour
  cannot by itself explain this particular button. RW 0x77B886..0x77B8D6
  resolves the issuing player's spell book and calls the same group function
  0x76F5DB as RW 0x77A4A8 for an explicit source. The port's two message kinds
  also converge on `castWithGroup`; a controlled comparison is still needed
  before treating the message-kind difference as causal.
* RW 0x82D5DA..0x82D6B1 gates targetless actions by module, unit cost, moving
  on an upper layer, readiness/requirements and power type. RW 0x68E620..
  0x68E666 adds disabled and `canUseSpecialPower` checks. The port's
  `SpellCommands::castWithGroup` checks module/readiness/requirements then
  delegates to `SpecialPowerModules`, but does not call the full ActionManager
  action test there (S-531 in `SpellCommands.h`). This is a shared-path
  discrepancy, **not** a demonstrated cause of a cast that should succeed:
  the missing tests generally refuse more inputs. The task requires a stop,
  not a local workaround, if a shared unported mechanism is shown to be the
  cause.
* For Rebuild, RW 0x8CC594..0x8CC5B1 calls the base object cast and then heals
  at the object's position; RW 0x8CC4B4 checks disabled/owner before the
  radius scan. The port uses `PlayerHealSpecialPower`; check the cast counter,
  recharge and health independently, since absence of a heal alone does not
  show that the message was lost.

## Reproduction plan

Use synthetic INI without installed archives: a spell-book object with the
mod's two power *types*, module classes and target options, a damaged allied
structure and an enemy/invalid neighbour. Drive `refresh` and `press`, inspect
messages and `SpellCommands` counters, dispatch them, then inspect health and
recharge. Repeat after readiness is restored; compare both targetless message
kinds under identical state. Separately exercise the actual HUD/Apt mouse path
at deliberately varied movie-frame boundaries: model-only tests cannot
establish whether the intermittent click reached the callback. This last test
needs an appropriate movie fixture; do not claim the queued-input hypothesis
without a deterministic reproduction and RW comparison.

The prescribed full one-job test build was still running (187/1721 objects at
last check); no `PORT-432*` tests, fix, green/red result or PR are available yet.
