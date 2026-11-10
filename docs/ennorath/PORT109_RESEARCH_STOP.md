# PORT-109: Upgrade Cursor — research blocked

Issue #269, task 85, 2026-11-10. **Not ported; not a no-reader verdict.**

## Evidence boundary

No RotWK `game.dat` is available in this checkout. The Drive download interface
returns binary data as base64 in the tool response rather than a workspace file;
the sandbox cannot fetch Drive URLs directly. No `rw.py dis` instruction has
been read for this task. Therefore no RotWK reader, branch or class chain is
claimed proven. The issue supplies RW 0xC10A54 (field name), RW 0xC108F8
(table entry), and RW 0xB5577C (reference to investigate); these are leads,
not independently checked evidence here.

## Donor leads (INFERENCE for RotWK; do not implement from these alone)

Read in Open-BFME/Open-BFME-2, BFME2 1.06, `Code/`:

- `GameEngine/Source/GameClient/MessageStream/CommandXlat.cpp`,
  `CommandTranslator::evaluateContextCommand`: after action 12, action 18
  with CONTEXT_ANY gates the upgrade-target branch. CONTEXT_COMMAND looks
  up command 51 and issues a special-power command. The other branch emits
  message 0x7D6, target ID, and a cursor index. It chooses the first selected
  object, looks up an upgrade from its mask at +0x284, and resolves that
  template's string at +0x74 through TheMouse. A null template gives index 1.
  Function/branch addresses must still be established in RW; do not confuse
  object +0x284 in this donor with the port's documented RW upgrade mask +0x28C.
- DONOR BFME2 0x0026F0F0 (`GameEngine/Source/Common/System/Upgrade.cpp`):
  first template whose assigned bit is in the supplied mask, via linked list.
- DONOR BFME2 0x002A3273 (`GameEngine/Source/GameClient/InGameUICreateCommandHint.cpp`):
  case 0x7D6 consumes integer argument 1 as cursor index.
- DONOR BFME2 0x0029C823 (`GameEngine/Source/GameClient/InGameUICanSelectedObjectsDoAction.cpp`):
  action 18 calls ActionManager's check below.
- DONOR BFME2 0x0041CE27 (`GameEngine/Source/Common/RTS/ActionManager.cpp`):
  checks source/target, relationship, target data and kind flags,
  SpecialAbilityGiveUpgrade, and ability type 0x33; forwards to
  DONOR BFME2 0x0049C6E1 (GiveUpgradeUpdate target check).
  Full GiveUpgradeUpdate validation and its class chain remain unread/unproven.

These are donor source observations, not disassembly performed on RotWK.
In particular the donor relationship predicate must not be copied as a RotWK fact.

## Port inspection and resumption

`UpgradeTemplate::m_cursor` (+0x74) is parsed in `Common/Upgrade.cpp` but has
no runtime reader. `GameClient/MessageStream/CommandXlat.cpp` already owns
cursor hints, but its `evaluateContextCommand` has no corresponding
upgrade-target branch. `Common/Upgrade.h` exposes template lookup by bit;
selection/validation and template choice must be proved before changing this
existing path. Do not add a second HUD cursor handler.

Resume with `workspace/RotWK/game.dat`: inspect the supplied references,
map the donor caller and consumer, prove class identity and every branch with
`rw.py dis`, including empty Cursor, missing upgrade and command priority.
Only then implement and add `test_port_109.cpp` / `cmake/Port109.cmake`.

## Validation and remaining work

Engine code, saves, hashes and existing tests are unchanged. No PORT-109 tests
were added or run; the required full `engine/build/openbfme_tests` run has not
completed. No PR or completion claim is appropriate.

Осталось: obtain an accessible RotWK binary, prove the RW chain, implement,
add synthetic regression and neighbour tests, build the full test target,
run `-tc="PORT-109*"`, and open the requested implementation PR.

## Owner-confirmed RW evidence supersedes input-access stop

The owner supplied `rw.py dis` results in issue #269 on 2026-11-10 and
explicitly authorized using them without a local binary. The previous
input-access stop is resolved for the cursor path, not for target validation.

- RW 0x81F1E0..0x81F1F8: action 0xC takes priority.
- RW 0x81F29D..0x81F2A9: action 0x12 gates the upgrade branch; failure
  continues to action 0x11 at RW 0x81F33B.
- RW 0x81F2AF..0x81F2CD: DoCommand finds command type 0x33; absent button
  ends evaluation, present button uses shared issue path RW 0x81F426.
- RW 0x81F2D2..0x81F332: other evaluation types append 0x7D6, target ID,
  and resolve the first selected object's +0x28C mask via RW 0x66F468.
  Lookup walks the template list and returns the first matching bit.
- RW 0x5EE9CA: cursor lookup is case-insensitive; empty (RW 0x5EE9D7)
  and unknown names return -1. A missing upgrade instead uses 1
  (RW 0x81F337).
- RW 0x6A405D..0x6A4081 consumes argument 1 through RW 0x69BD53;
  selecting/scrolling suppress changes, otherwise RW 0x69B06D sets it.
- RW 0xB5577B is unrelated APT/Lua string concatenation, not an upgrade reader.

### Remaining shared-mechanism blocker (not an input-access blocker)

The owner's comment explicitly leaves action 0x12 validation to port-side
inspection. That inspection found `ActionManager.cpp`,
`canDoSpecialPowerAtObject`, switch case 0x33 (RW 0x82D780): it calls
`unportedObjectTypeTest`, returning false and reporting S-7140.
`GiveUpgradeUpdate.h` exposes no equivalent of target validation RW 0x89FE64;
`GiveUpgradeUpdate.cpp` documents missing contained-target providers and
paired reservations as S-6011. Its automatic search is not an equivalent
of the context action's predicate.

This is a shared mechanism, also used by CommandButtonHuntUpdate and
special-power object validation. A cursor-only condition based on module
presence, relationship or affectedByUpgrade would bypass it. Calling the
existing type-0x33 validator would always reject and make the cursor branch
dead. Neither is a correct implementation.

Needed: disassembly/branch semantics of RW 0x82D780 and RW 0x89FE64,
including provider/contained-target decisions and the action-0x12 dispatch
in RW 0x69D27E. With those, implement one shared predicate and reuse it
in the existing ActionManager and context paths; no HUD-only approximation.
No code or tests changed, and PORT-109* has not run. The three requested
cursor tests plus priority/selection suppression remain to be implemented.

## Predicate implementation in progress

The owner's second confirmation in issue #269 resolves the previous shared
predicate proof gap. RW 0x82D780 checks nulls, ALLIES, target AI (+0x260),
not IMMOBILE, the named SpecialAbilityGiveUpgrade template, then source
update type 0x33. RW 0x89FE64 uses mask lookup RW 0x66F468; no upgrade
refuses, contained targets use producer (+0x78) contain slot 0x7C and
provider slot 0xAC, all other targets call affectedByUpgrade RW 0x694914.
Implemented the shared predicate and head-first template lookup. The owner
explicitly allows a contained-target-only stop; that branch reports S-6011.
The cursor integration and PORT-109 tests are still pending. No passing
build or test claim yet. These paragraphs supersede the prior blocker.
