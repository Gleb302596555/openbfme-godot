# PORT-239 — player orders (fork issue 562)

## State of investigation

Baseline: `80803fe481c19a3e22d6d1b71fd6e93d12ea4742`.
This is an **interim investigation**, not a completed port or acceptance report.
No simulation or input behaviour has been changed. No stop is closed yet.
The full `openbfme_tests` target is building; PORT-239 tests do not exist yet.

The RotWK 2.01 executable was checked against the supplied MD5. Addresses below
were read with `tools/mod/rw.py ... dis`; the donor is explanatory, not the
source of retail expectations. No game data or disassembly dumps are included.

## Item inventory

| Stop | Existing port / investigation entry | Outcome so far / next proof |
|---|---|---|
| S-146 | `GameLogic/Player*`, `PlayerList::newGame` | Unresolved: broad per-lane row; do not interpret its old summary as current dispatcher coverage. |
| S-208 | `GameLogicDispatch`, registered lane handlers | Unresolved: enumerate actual handlers against the retail dispatcher before narrowing. |
| S-223 | `AICommands`, `AIGroup`; RW 0x774897 | Unresolved: remaining tighten and formation-drag branches need linear reading. |
| S-281 | `MessageStream/MetaEvent.cpp`, `GodotInGameHud.cpp` | Loader wrapper and subsystem loader read (below); caller paths and subsystem legend resolution remain to trace. German scan-code swap already exists. |
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
