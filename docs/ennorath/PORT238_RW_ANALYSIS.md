# Port 238: ring hand-over investigation (in progress)

This is a partial investigation, not a claim that every ring hero, ability or
appearance has been checked. Addresses refer to RotWK 2.01 `game.dat`.

## HUD admission and capacity

* `RW 0x81E60B` performs the context-command dispatch; at `0x81EEBF..0x81EECA`
  it calls the selected-object dispatcher `RW 0x69D27E` with action 8 and
  additional checking enabled. The jump table at `0x69D5AD` maps action 8 to
  `0x69D3CB` and action 9 to `0x69D401`.
* Both branches call `RW 0x82CBD5` with path checking enabled and capacity
  mode selected from additional checking. They examine opposite values of the
  special-result byte: the two actions are not interchangeable. DONOR BFME2
  `InGameUICanSelectedObjectsDoAction.cpp`, `canSelectedObjectsDoAction`, has
  the corresponding actions 8 and 9. Do not infer from that alone which screen
  route invokes action 9 in RotWK.
* `RW 0x82CBD5` rejects a zero transport-slot count at `0x82CE90..0x82CEA0`
  when capacity checking remains enabled, before calling the contain's entry
  test at `0x82CEB2..0x82CEC0`. `RW 0x69029B` reads the signed byte at
  template offset `0x601`, unless the contain's slot `0x14` requests a
  recursive sum of its occupants. The template constructor `RW 0x73FB7A`
  initializes that byte to zero at `0x73FFD9`.
* `RW 0x88397F` first calls the inherited admission test `RW 0x8838C3`, then
  tests the controlling player and the special ring-related branch; when path
  checking is requested it calls `RW 0x6F5BB0` and, if needed, `RW 0x6F5F65`.
  `RW 0x883A29` prevents slaughter of the ring-status object or a member whose
  producer has the status.
* `RW 0x82CEFB` chooses a contest message first for an occupied enemy contain
  under its applicable conditions; otherwise it checks a command-center target
  and the selection for the ring status, then falls back to slaughter/enter.
  The corresponding port code is `ActionManager.cpp:getEnterMessage`.

The current port's `CommandXlat.cpp` calls `GarrisonRules::canEnterObject` in
capacity mode with `checkPath=false`; the same generic HUD path is used for
non-ring containers. `AIGarrisonStates.cpp` has the capacity gate, while
`CitadelSlaughterHordeContain::isValidContainerFor` has the ring/path branch.
Correcting the generic HUD dispatch requires examining its other actions and
callers, not a ring-only workaround. The path and action-9 differences remain
open in S-7321; this investigation does not establish that they cause a failed
hand-over in a particular game situation.

## Other checked pieces

* `RW 0x8842AF` constructs SlaughterHordeContain data, loading the float at
  `0xBD869C` (0.5) into `+0xD4` at `0x8842E2`. The port's default is 0.5 in
  `GarrisonContain.h`. `0xC5EFE8` is the field table, not the constructor.
* `RW 0x8BD15E` grants the one configured upgrade during creation only when
  `GiveOnBuildComplete` is false, the exempt mask contains bit 2 and the object
  does not have bit 2. `UpgradeModuleClasses.cpp` implements these checks.
  DONOR BFME2 `GrantUpgradeCreate_onCreate.cpp` documents the same bit test;
  it does not justify granting multiple names from an INI field read as one
  string.
* `RingOwnershipBias` is parsed in `SkirmishAIData.cpp` and consumed in
  `AITacticalAI.cpp`; this observation alone is not a full RW comparison.
  `ChildObject`, `ModelConditionUpgrade`, ring ownership and command-set paths
  exist in the port, but existence is not proof of complete hero behavior.

## Remaining work

Enumerate all affected mod objects and powers, verify inheritance and every
relevant RW path, locate any independent ring-specific gap and add a failing
synthetic test plus its neighbor test. Read the remaining HUD dispatch and
special-result callers. Build and run the full `openbfme_tests` target with
`-tc="PORT-238*"`; no PORT-238 test exists yet. No retail-absence verdict or
complete in-game acceptance is claimed here.
