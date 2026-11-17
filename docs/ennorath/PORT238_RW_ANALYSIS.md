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

## Ring callback and the queued order

* CitadelSlaughterHordeContain's factory `RW 0x64BDAE` calls constructor
  `RW 0x883801`, which installs primary vtable `0xC5F470`. Its slot `+0x80`
  points to `RW 0x8840FC`, invoked from the non-slaughter branch of the shared
  entry function `RW 0x883B4A` at `0x883F73`.
* `RW 0x8840FC` idles the carrier, then at `0x88413C` calls
  `RW 0x756CD7` with its object ID. That function finds its queue and calls
  `RW 0x94EF1A`: a completed current order is removed and its successor can be
  activated; an incomplete or planned current order remains. Only afterward does
  the ring callback clear the configured status, destroy the nearest filtered
  object, apply the configured player/object upgrades, emit FX and store the
  exiting carrier ID. DONOR BFME2 `CitadelSlaughterHordeContainRva00480E9A.cpp`
  provides the corresponding source; the observations above are also confirmed
  by the RotWK instructions.
* The port already had the rest of this callback. `deliverRing` now calls the
  existing `AIOrdersManager::process`, without creating new saved state. The
  synthetic `PORT-238*` cases exercise a complete order and its successor as well
  as an incomplete order and another member's queue. Their full-executable
  run is pending; an isolated negative-control run is recorded below.

## Queue callback reentrancy (not resolved)

`RW 0x94EF1A` calls the current order's completion vslot `+0x14` at
`0x94EF58`, then removes that order and activates the next via vslot `+0x10`
at `0x94EFE0`. No generic reentrancy guard is visible in this function.
The port similarly invokes `GroupOrder::complete` and `activate` from
`ObjectOrderQueue::process`; it has no guard against an order callback
recursively mutating the same queue. Such mutation could invalidate the
outer operation, but a live supported route that does this has not been shown.

The ordinary port garrison order's `complete` may call `aiEnter` while idle.
That sets the move-to-enter state; `AIMoveToState::onEnter` returns continue or
failure, while `AIEnterState::update` performs `addToContain` on an update,
not directly in the ordinary garrison `aiEnter` path. This is not an exhaustive
proof for all callbacks, horde paths, effects or nested state transitions. The
existing probe tests check ordering, not reentrancy. The remaining shared queue
and live callback audit is tracked by S-7427; do not add an unverified guard
that might diverge from RW or alter other orders.

## The ONE_RING button

`Command_OneRing` is present in the Frodo ring command sets of the mod. In
`RW 0x942733`, case 39 at `0x94387B` first tests status `CAN_STEALTH`; without
it the result is restricted. With it, `RW 0x68FBD3` finds an eligible
StealthUpdate; no match is enabled, and a match is enabled only when
`RW 0x7761DD` reports the current frame is at least its `+0x28` ready frame.
The button dispatcher `RW 0x940435` uses jump tables at `0x941B63` / `0x941AC3`
to reach `0x941A6B` for command 39, which appends `MSG_ONE_RING` (0x454).
`RW 0x7729A6` idles each selected object's AI and invokes `RW 0x776B50` for
its eligible StealthUpdate. DONOR BFME2 `ControlBarCommandProcessing.cpp`
case 0x27 also appends that message. Before this change the port fell through to its default enabled availability
and did not dispatch this command; it now checks availability and dispatches
it. Synthetic tests exist; the full-executable run is pending.

The shared port lookup `StealthUpdate::of` returns only the first module by name,
whereas RW 0x68FBD3 iterates the cached modules and applies upgrade masks. This
affects multiple non-ring systems too; the remaining common-mechanism work is
S-7426, not a ring-only workaround. Consequently multi-module readiness and
activation are not yet claimed equivalent.

## Other checked pieces

* `RW 0x8842AF` constructs SlaughterHordeContain data, loading the float at
  `0xBD869C` (0.5) into `+0xD4` at `0x8842E2`. The port's default is 0.5 in
  `GarrisonContain.h`. `0xC5EFE8` is the field table, not the constructor.
* `RW 0xC70158` has `UpgradeToGrant` at `+0x08` with parser `0x42EE5E`;
  that calls `0x42E757`, which takes one unquoted token. DONOR BFME2's
  `GrantUpgradeCreate.cpp` also lists `parseAsciiString` for that field. The
  isolated synthetic `PORT-238*` test confirms that with two unquoted upgrade
  names the port grants only the first when the other conditions permit it.
  This is not an end-to-end verdict on the mod's object lifecycle.
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

## Isolated synthetic test run (not the full executable)

A temporary doctest runner linked `test_port_238.cpp` and the built
`libopenbfme_core.a` plus its required libraries, without the full suite.
The source and build-tree core archive contained the fixes. `-tc="PORT-238*"`
reported:

```
[doctest] test cases:  10 |  10 passed | 0 failed | 0 skipped
[doctest] assertions: 226 | 226 passed | 0 failed |
[doctest] Status: SUCCESS!
```

For negative controls, temporary source copies (outside the checkout) were
compiled and linked ahead of that same archive: one omitted only the queue
`process` call; the other omitted both ONE_RING cases in ControlBar availability
and dispatch. With the former, `-tc="PORT-238 delivery*"` exited 1:

```
[doctest] test cases:  2 |  0 passed | 2 failed | 6 skipped
[doctest] assertions: 45 | 39 passed | 6 failed |
[doctest] Status: FAILURE!
```

With the latter, `-tc="PORT-238 One Ring button*"` exited 1:

```
[doctest] test cases:  1 |  0 passed | 1 failed | 7 skipped
[doctest] assertions: 15 | 14 passed | 1 failed |
[doctest] Status: FAILURE!
```

The independently scoped `-tc="PORT-238 adjacent stop*"` passed in both
negative-control executables:

```
[doctest] test cases:  1 |  1 passed | 0 failed | 7 skipped
[doctest] assertions: 17 | 17 passed | 0 failed |
[doctest] Status: SUCCESS!
```

These are isolated tests, **not** the required full `openbfme_tests` run and
not an in-game test. The separate status-gate test is intentionally a regression
test for ONE_RING, not the independent STOP control: without the availability
fix it fails because the old default was enabled.

## Remaining work

Enumerate all affected mod objects and powers, verify inheritance and every
relevant RW path, locate any independent ring-specific gap and add a failing
synthetic test plus its neighbor test. Read the remaining HUD dispatch and
special-result callers. Build and run the full `openbfme_tests` target with
`-tc="PORT-238*"` and verify failure without the change. No retail-absence verdict or
complete in-game acceptance is claimed here.
