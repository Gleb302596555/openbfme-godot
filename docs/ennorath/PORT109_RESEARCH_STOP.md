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
