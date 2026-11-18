# PORT-239 — player orders (fork issue 562)

## State of investigation

Baseline: `80803fe481c19a3e22d6d1b71fd6e93d12ea4742`.
This is an **interim investigation**, not a completed port or acceptance report.
Input changes implemented so far: PLANNING CommandMap applicability and shared
RotWK installation loading through the subsystem legend, then the root override.
No new simulation state. No stop is closed yet; S-3803 is narrowed by the
owner-notification verdict below. Eleven asset-free PORT-239 tests are registered
in the full target. They passed syntax validation only; the full target is still
building. No passing execution or failing-before-fix result is claimed.

The RotWK 2.01 executable was checked against the supplied MD5. Addresses below
were read with `tools/mod/rw.py ... dis`; the donor is explanatory, not the
source of retail expectations. No game data or disassembly dumps are included.

## Item inventory

| Stop | Existing port / investigation entry | Outcome so far / next proof |
|---|---|---|
| S-146 | `GameLogic/Player*`, `PlayerList::newGame` | Unresolved: broad per-lane row; do not interpret its old summary as current dispatcher coverage. |
| S-208 | `GameLogicDispatch`, registered lane handlers | Unresolved: enumerate actual handlers against the retail dispatcher before narrowing. |
| S-223 | `AICommands`, `AIGroup`; RW 0x774897 | Unresolved: remaining tighten and formation-drag branches need linear reading. |
| S-281 | `MessageStream/MetaEvent.cpp`, `GodotInGameHud.cpp` | Shared legend/root loader and planning mask implemented; full-target execution and red/green pending. German scan-code swap already exists. |
| S-286 | `MessageStream/CommandXlat.cpp::evaluateContextCommand` | Unresolved: port explicitly labels context priority as inference; inventory each branch against retail, not just attack/enter. Keep off force-attack and special-power functions owned by other tasks. |
| S-287 | `CommandXlat.cpp` meta switch | Unresolved: compare entire switch with RW 0x81F8D8; do not remove the deliberate force-attack addition (S-1675) without the owner's decision. |
| S-1201 | Context resume/repair | Unresolved: read retail ActionManager eligibility and context priority before changing builder clicks. |
| S-3301 | `SelectionXlat`, `HudInput`; point collector / polygon test | Real missing mechanism indicated by current plain-box implementation; numerical branches read below, caller/class chain and integration still pending. |
| S-3803 | `AptPalantir` stance submenu | Unresolved: notifications, press context and inferred lifecycle need tracing; existing submenu must be extended, not duplicated. |
| S-4360 | Same Ctrl-selection mechanism as S-3301 | Treat together with S-3301, not as a separate selection handler. S-1675 remains a distinct owner-decision row. |
| S-4531 | `GUI/AptScreens/AptPalantir.cpp:477–490` | The ordinary left-press gate and disabled cue already exist in baseline. The old stop's statement “every press reaches ControlBar” is stale for this path. Verify synthetically before closing/narrowing; toggle-stance uses another virtual path. |

## Ordinary command-button press: retail branches read

DONOR BFME2 0x005C3427
(`Code/GameEngine/Source/Common/Rva005C3427Box.cpp`): getter, compare states
1/3/5, disabled sound and return, otherwise tail-call notification.

Retail chain read:

- RW 0x9D6E54: reads AptWindowManager + 0x318. Mouse button 0 takes owner
  slot +0xC for clip state 6, otherwise +0x10. Button 2 uses +0x14/+0x18;
  other values do nothing. Thus the left gate is not the right-click gate.
- RW 0xA00C0E: returns the controller's content clip state at +0x50.
- RW 0x9D285E: compares 1, 3, 5 at 0x9D2866/0x9D286B/0x9D2870.
  Matching states reach the sound call at 0x9D2882 and return; all other
  states tail-jump to RW 0x9D2814 at 0x9D2878.
- RW 0x9D2814: if the window owner exists, sends window message 0x4008
  (push at 0x9D284E, virtual call at 0x9D2854); no native enabled-click sound.
- Baseline `AptPalantir::syncButtons` already rejects Restricted, CantAfford,
  Purchased and NotReady with one disabled cue, otherwise calls `m_press`.
  `commandStateName` maps Purchased and CantAfford to the same `_cantAfford`.
  The toggle-stance check precedes this gate, reflecting a separate controller;
  its eligibility must be checked separately under S-3803.

No claim yet about complete factory → constructor → vtable ownership proof or
all clip-state mappings. In particular, 0x9D2BEE is an **interior address**, not
a function entry; it must not be cited as a newly ported function's start.

## Ctrl selection: numerical branches read

RW 0x6A091A (point collection, read through return):

- Empty point list at +0x910 initializes all four bounds at +0x914…+0x920.
- For subsequent points, computes the displacement from the last accepted
  point; unsigned integer tolerance at GlobalData +0x12E8 is divided by 3
  before conversion to float (0x6A0999…0x6A09B6).
- Distance below that threshold returns without insertion (0x6A09C5).
- Larger displacements can be normalized and shortened before truncating to
  integer pixel coordinates (0x6A09D3…0x6A0A16). The multiplier constants and
  called vector helpers still need verification; do not replace this with a
  guessed fixed-distance sampler.
- Updates bounds, then inserts the accepted point (0x6A0A4B…0x6A0A52).

RW 0x69CBBE (inside test, read through return):

- Rejects outside the inclusive accumulated bounds (0x69CBC9…0x69CBF0).
- Walks each edge, including last → first. Horizontal edges do not cross.
- Y crossing is asymmetric at the endpoint (0x69CC36…0x69CC45), so a generic
  point-in-polygon helper must not be substituted without matching boundaries.
- Interpolates X using integer product then scalar float divide/add
  (0x69CC53…0x69CC7C); intersection X >= query X toggles parity.

RW 0x6A0A5E (selection iteration, read through return):

- Fewer than three points falls back to a single region pick (0x6A0A84).
- The degenerate-bounds fallback requires **both** height and width zero
  (0x6A0A89…0x6A0A9E), not either dimension zero.
- Polygon path iterates drawables, skips the callback's exclusion mask,
  projects a derived world position through view vslot +0x160, then calls
  RW 0x69CBBE (0x6A0B05…0x6A0B86). The position helpers and mask semantics
  remain to trace before porting this path.
- Accepted drawables go through the supplied callback; the point-pick branch
  processes at most its picked drawable (0x6A0B88…0x6A0BA6).

DONOR BFME2: `SelectionXlatTranslateGameMessage.cpp` explains the integration:
Ctrl left-down starts the point list; area selection dispatches via the custom
iterator when the mode byte +0x924 is set, clears it afterwards, otherwise uses
the ordinary region iterator. This caller integration still needs confirmation
in RW 0x83C29E before code changes. No donor-only behaviour has been ported.

## CommandMap loader: next link found

RW 0x63874B is a subsystem wrapper, **not** the path resolver. It creates a
wrapper object with vtable 0xBFE490 and calls RW 0x5B4A7C at 0x6387A1.
RW 0x5B4A7C initializes/registers the subsystem, then, unless its virtual +8
returns true, loads up to two supplied file paths using RW 0x42D97D in order,
and a supplied directory using RW 0x42E343. This does not by itself establish
which CommandMap paths the caller passes, nor whether the legend bypasses
these loads. Next read the donor `Code/Libraries/Source/subsystem/SubsystemInterface.cpp`,
the CommandMap call site, and legend callback before changing either existing
CommandMap load site (`MetaEvent.cpp` and `GodotInGameHud.cpp`).

## Remaining work

Finish the branch/class/caller proof for all eleven items; implement actual
independent gaps; add `test_port_239.cpp` / `Port239.cmake`, including regression
and neighbour tests and a demonstrated failure without the fix. Run the full
`engine/build/openbfme_tests -tc="PORT-239*"` binary. Only then narrow/close stops,
add night-watch and any justified shared-mechanism stop fragments, and open PR.
No upstream PR is eligible at this investigation-only stage.

## Follow-up: S-3803 owner notifications are no-ops on tactical Palantir

Rebased onto `origin/main` at `dbea96b5` before continuing; no input code from
other tasks was replaced. The following establishes the previously missing
class chain (not an inference from nearby addresses):

1. RW 0x93023D (`OnSubMenuLoaded`) allocates **8** bytes at 0x93029B and calls
   RW 0x9D353B at 0x9302FA with level and clip path. It does not construct a
   derived strategic-HUD listener.
2. RW 0x9D353B writes vtable **0xC8C46C** at 0x9D354B, allocates its 0x48-byte
   implementation and passes `this` as the owner at 0x9D356B to RW 0x9D3380.
3. RW 0x9D3380 stores that owner at impl +0 (0x9D3398). Reading the verified
   executable's vtable yields `{0x9D3364, 0x63F3BF, 0x63F3BF}`.
4. RW 0x63F3BF is one instruction, `ret`. Thus owner vslots +4 and +8 have
   **no effects** on this concrete Palantir owner.
5. RW 0x9D32F9 sets open and tail-calls owner +4 at 0x9D3331. RW 0x9D3235
   (_OnClosed), only when fading and open, drops the items, clears open and
   calls owner +8 at 0x9D327D. RW 0x9D2F21 (Hide), only when open, drops the
   items and clears fading/open before tail-calling owner +8 at 0x9D2F8D.

DONOR BFME2 0x005C40E7 (`Code/GameEngine/Source/Common/Rva005C40E7Ctor.cpp`)
confirms the wrapper has empty slot1/slot2; the tactical factory uses that base
wrapper (`PalantirCommandInterface.cpp::OnSubMenuLoaded`, BFME2 0x005297DD).
The strategic HUD donor has a **different, derived** `ButtonSubMenu` listener
that keeps a current submenu and closes another when shown. That behaviour is
not evidence for adding exclusivity to the tactical Palantir.

Outcome: **S-3803 narrowed**, not closed. No callback with invented effects is
needed. The context-object press checks and other explicitly listed inferences
remain unresolved. PORT-239 tests will pin the existing independently opened
menus and their fade/hide lifecycle without retail assets; they are neighbour /
verdict coverage, not the required failing-before-fix test for a future real gap.

## Follow-up: CommandMap direct call site

The prior inventory entry was too quick to describe 0x63C4F6 as a subsystem
entry: it is an argument to a **direct INI load**, RW 0x42D97D at 0x63C506,
following a separate subsystem registration call at 0x63C4E5. The actual
subsystem name at that registration and its legend paths need tracing. Do not
base a new CommandMap resolver on the old stop's wording alone.

## Current handoff

Commits `f80affc4`, `9fc8e396`, `ce2a9393` preserve the owner proof, narrow
S-3803, and add three test cases in `engine/tests/test_port_239.cpp` through
`engine/cmake/Port239.cmake`. No NIGHT_WATCH row is needed yet: gameplay has
not changed. The missing regression that fails without an actual fix is still
required before review. The full Release build is running with `-j1`; do not
start a second Ninja on the same build directory. After it finishes, run the
build command again so CMake picks up the newly added lane, then execute
`engine/build/openbfme_tests -tc="PORT-239*"`. No auxiliary runner was used.

## S-281 / S-287: actual gap — planning UseableIn mask

RW **0x5DA83D** is MetaEventTranslator's complete entry. The key branch was
read linearly from entry through the map loop. At 0x5DA8C3…0x5DA8F0 the active
mask starts as SHELL (1); without an active shell it becomes GAME (2), or
GAME|PLANNING (6) when the local player exists and +0x770 equals **2**.
0x5DA910 tests each record's usable-in against that mask **before both** the
KEY_NONE modifier-transition branch and the ordinary key branch. The exact
GAME-only frame-zero suppression at 0x5DA8FA…0x5DA90E is independent and remains.

DONOR BFME2 **0x005DB0B5** (`Code/GameEngine/Source/GameClient/MessageStream/MetaEvent.cpp`,
`MetaEventTranslator::translateGameMessage`) has the same mask selection but
player offset +0x750. RotWK's verified offset is +0x770, not copied from donor.
The existing port exposes that state as `Player::orderMode()` (already hashed /
snapshotted); no new simulation state is needed. The existing translator
instead rejects every PLANNING-only record unconditionally. Fix the single
shared filter before the two match branches, not separate key handlers.

Planned regression: synthetic parsed CommandMap with PLANNING-only, GAME,
GAME|PLANNING, SHELL, SHELL|PLANNING records; all masks 0…7, modes 0/1/2, shell
and frame-zero combinations, null player, modifier DOWN/UP and autorepeat.
Expectations come from the two retail predicates above, with a separately
asserted planning-only message so the regression cannot simply mirror the
implementation. Must demonstrate failure without the fix on the full binary.

### Planning-mask implementation checkpoint

Code `8ec77680`, tests `b3c618d7`: the shared filter is implemented with no
new state (`Player.cpp` already hashes `m_orderMode` and transfers it in xfer).
Six PORT-239 test cases are now registered, not three. Syntax validation only;
full executable and red/green comparison remain outstanding. Correction to the
initial matrix plan: `PlayerList::setLocalPlayer(nullptr)` selects Neutral, so
the matrix explicitly tests Neutral fallback (and asserts the actual pointer),
not a nonexistent null-player fixture. No private-state test backdoor is added.

## S-281: CommandMap file-resolution proof completed

The stop's old premise was wrong: RW 0x63874B is the preceding AI-target
heuristic subsystem wrapper, not CommandMap's loader. The actual chain is:

- GameEngine::init call sites, call site 0x63C1CC creates MetaMap through
  RW 0x5DAB70; constructor installs vtable 0xBF1BF4. Its init slot +4 is the
  empty RW 0x63F3BF, load-from-legend slot +8 is RW 0x5B4B9A.
- The registration at 0x63C1F3 calls RW 0x638244 with name `TheMetaMap` and
  no hardcoded paths. RW 0x638244 forwards to RW 0x5B4A7C (0x63829A).
- RW 0x5B4A7C sets the name, initializes, and invokes slot +8. Linear reading
  of RW 0x5B4B9A through its return proves InitFile entries load in order
  (0x5B4C67…0x5B4CA7), then InitPath entries (0x5B4CA9…0x5B4CE2), applying
  exclusions. The engine's existing `SubsystemLegend` implements this path.
- Later in the **same** GameEngine::init, 0x63C4F6 pushes `CommandMap.ini`
  into a direct RW 0x42D97D load at 0x63C506. This is the language override,
  not another subsystem registration or a path expanded by RW 0x63874B.

DONOR BFME2: `Code/Libraries/Source/subsystem/SubsystemInterface.cpp`'s
`initSubsystem` and loadIniFilesFromLegend helper describe the same dispatch
and ordered file/path loops; RW above confirms each link independently.
The mounted Ennorath 1.9.8.3 expansion legend was checked: its TheMetaMap
entry names **two** InitFiles, so hardcoding only the first omits a real input.
No INI contents are copied into this report.

Required fix: route both real installation readers (`GodotInGameHud` and
`ListCommandMap`) through one MetaMap installation loader; reuse the existing
legend parser/loader, then load the root override. Preserve other-edition
paths, report missing/malformed files, and do not add another command handler.

## Latest implementation checkpoint

Commits e896f06b/ea4ebfcd implement the shared installation loader and three
additional production-reader tests. Both HUD and ListCommandMap now use the
existing SubsystemLegend, followed by the root override. Non-RotWK paths are
preserved. Nine PORT-239 cases syntax-check; none has been executed yet.
S-281 is not marked closed until full-target execution and red/green succeed.
Correction: 0x63AF2D was an interior init call site, not a proven function
entry; the proven loader entry is 0x5B4B9A and the call sites are listed above.
Full single-thread build remains running (last observed 514/1683); do not
start a second ninja in engine/build. Other issue-562 stops remain unfinished.

## S-3803: eligibility chain, further linear reading

RW entry 0x96FAC6 resolves the context object through 0x9D29CE -> 0x449681.
A missing object jumps directly to the menu open/close branch (0x96FB11).
An existing object requires PlayerList (global 0xDE4928) and RW 0x6A8803.
The latter returns false without local player; otherwise RW 0x6AAC52 returns
active iff **both** observer byte +0x35A and defeated byte +0x754 are zero.
Inactive local players bypass the relationship restriction; active players
require relationship 2 to the object's team (+0x31C), through 0x6ADBEB.
RW entry 0x6ADBEB first checks a team override (+0x354), then calls 0x6ACEAF
for the team's player. RW entry 0x6ACEAF checks player overrides (+0x350)
and defaults to 1 (neutral), not to allied. All bodies read through return.
DONOR BFME2: Common/RTS/Player.cpp::isPlayerActive uses observer/dead at
+0x33A/+0x734, confirming the names but not RotWK offsets; the target proof
above is independent. The port already has observer, defeated, team/player
relationships and ALLIES=2. Do not replace this by a selected-owner equality
check or reuse the ordinary disabled-clip gate: neither is retail's predicate.
Integration and asset-free press tests are still outstanding; no stance code
was changed on this evidence alone.

Additional loader neighbour tests now cover the unchanged BFME1/BFME2 paths
and macro lifetime across the temporary manifest registry (11 total cases,
syntax-only). The latter also checks no dangling LoadSubsystem callback is
installed in the caller's persistent registry.
