# Port 462: unit voice gates — RW analysis and verification

Final status: implemented and verified on the full openbfme_tests target.
The checkpoint sections below preserve the research chronology, including
corrections to the initial issue hypothesis; the final table is authoritative.
Binary: RotWK 2.01, MD5 daed3668006cd90f01c34e5a7da1901f.
Read using tools/mod/rw.py dis in the sandbox.

## Confirmed branches

* RW 0x671ED8–0x671F15, read linearly from 0x671ED8: absent logic
  pointer (0xDE412C) or absent held object (receiver +0xFC) returns false.
  Correction to the first checkpoint: receiver is the drawable, not the object;
  see caller proof below. Do not implement this as Object::clientHooks()!=null.
  0x671EF2 passes model condition 0x25 to 0x694154. If true, return false;
  otherwise unsigned current frame at logic +0x40 must be >= drawable +0x384.
* RW 0x694154–0x694180: first checks the object's condition via 0x46E918;
  failing that, resolves its horde via 0x693A1A(0) and tests that condition.
* RW 0x671F16–0x671F31: if logic exists, charge timeout becomes current
  frame plus template +0x568. This neighboring behavior must remain intact.
* RW move helper 0x8DE838, read linearly through 0x8DEB2E so far:
  0x8DE876 classifies enter-state messages. At 0x8DE8F3–0x8DE90E,
  planning replacement requires non-enter-state, controlling player,
  player +0x770 == 2, and false third argument. 0x8DE924 obtains MiscAudio;
  +0xE0 is copied into the choice through 0x8DDA33 at 0x8DE94B, then exits.
* RW 0x8DEA22–0x8DEA5C: absent AI (+0x260) exits. Movement is
  0x664485() OR AI byte +0x3B1. Only non-enter-state orders test global
  0xDE4830 +0x8B0; if set and moving, exit. This is NOT an unconditional
  busy-unit mute. Earlier special choice remains populated on this return.

## Donor cross-check (not authority for RW)

DONOR BFME2 0x004D9CF3, Code/GameEngine/Source/GameClient/Drawable/PickAndPlayUnitVoiceResponse.cpp:
attack helper has planning replacement before attack-specific voices.
The same file's doPickAndPlayForMoveCommand shows the movement gate and
identifies its global as TheInGameUI, NOT GameData. Its +0x8B0 is
m_waypointMode in Code/GameEngine/Source/GameClient/InGameUICtor.cpp.
This contradicts the provisional GameData label in S-704; confirm the RW
identity and callers before choosing any API or field name in the port.

## Remaining research before implementation

Read the rest of the move helper, attack and enter helpers and callers;
confirm third argument semantics and RW global/class identities (including
object/drawable ownership); trace planning sound parser and live wiring.
Player::orderMode already exists; do not add duplicate simulation state.
Check existing drawable presence and waypoint-mode services and all callers.
Retrieve Ennorath 1.9.8.3 INI before making data claims.
Add PORT-462 tests only after expectations and integration are confirmed.

## Second checkpoint: caller proof and additional branches

RW picker 0x8DEDBB read linearly (1450 instructions). At 0x8DF045,
object EDI is passed to 0x70E013; returned EBX must be nonnull (0x8DF04E).
0x8DF065 pushes EBX, 0x8DF066 pushes object EDI, and 0x8DF067 calls
choice constructor 0x8DD819 (previously read). Constructor stores object
at choice +0x10, EBX at +0x14. Attack helper loads choice +0x14 at
0x8DDF50 into its local -0x20; 0x8DE15C loads that local as the receiver
of 0x671ED8. Thus +0xFC in the charge helper is the drawable's object,
NOT Object's drawable. DONOR BFME2 0x00271BDE
(Code/GameEngine/Source/GameClient/Drawable.cpp) corroborates this:
TheGameLogic, m_fc, m_fc->condition(0x25), drawable deadline m_384.
The port already operates on live Objects, not unbound drawables. Adding
an Object::clientHooks presence test would be a different condition and
would incorrectly mute headless objects. Need finish binding proof before
choosing whether any charge-gate runtime change is actually required.

RW attack helper now read through return 0x8DE1BC. Planning branch at
0x8DDFCD–0x8DE04C matches move's conditions. Weapon override precedes
charge (0x8DE0F4–0x8DE150); charge helper is called at 0x8DE15F; air
fallback follows at 0x8DE17D, base at 0x8DE19C. No waypoint gate in attack.
RW move helper remainder read through 0x8DED5B: existing camp, elevation,
while-attacking, wall-climbing and base rules follow the movement gate.

RW 0x8DEE0E–0x8DEE1A: the helpers' third bool is original message ==
0x46A (not generic force attack). Passed from picker at 0x8DFC77 and
0x8DFCA8 / 0x8DFCC1. RW 0x8DF416–0x8DF464 has the planning branch in
enter handling too. Another planning consumer exists for a command-button
case (0x8DFB11–0x8DFB5F); inspect its scope before claiming full mechanism.

Existing port services: Player::orderMode(), InGameUI::isInWaypointMode(),
AIUpdate::isMoving(), AIMover::isWaitingForPath(), MiscAudio::get().
CommandXlat already cites RW 0x8200FA for waypoint mode. RW instruction
at 0x8200FA sets receiver +0x8B0, 0x82011D clears it (read this range).
No new simulation state is needed for these services. Live HUD creates
one UnitVoiceResponse and uses it for translator, stance and object voices;
new state must not be refreshed only on one of those routes.

## Third checkpoint: getter and scope confirmation

RW 0x70E013 read directly: `mov eax,[ecx+0x84]; ret`.
Together with the picker/choice-constructor chain above, this establishes
that the charge helper is reached through the object's drawable, and then
checks the drawable's back-reference. The port's Object-based candidate
path already rejects missing/destroyed objects. Do not add a clientHooks
presence condition merely to make a test fail: that is not this RW gate.

RW enter branch read linearly from 0x8DF40A through 0x8DF56F: planning
replacement precedes VoiceEnterUnit and all enter-kind dispatch. RW
0x8DF4FF skips the rest once the choice is complete. The stance-button
planning case is already separately recorded as S-703 in this port; it is
not one of the move/attack/enter helper gates assigned here.

Implementation plan: use existing Player::orderMode; query live
InGameUI::isInWaypointMode rather than introduce a GameData INI field;
movement predicate is AIUpdate::isMoving OR AIMover::isWaitingForPath.
Read the planning MiscAudio slot through the audio service and the same
existing choice/collection path, never play an extra sound alongside it.
Keep empty/NoSound planning slots silent, without falling through to the
unit voice. Cover command versus enter-state, moving versus stationary,
planning precedence, neighboring selection/attack, and charge timeout.

Blocked before data validation/implementation: Google Drive download of
ennorath-ini-1.9.8.3.zip returned auth_error: the connection is no longer
valid. No further connector request was made. The archive is not present
in this sandbox. Owner must reconnect Google Drive before the required
mod-first validation can proceed.

## Data access restored and implementation

Drive access restored; downloaded ennorath-ini-1.9.8.3.zip and extracted
only in /tmp. Verified RohanRohirrim in
object/goodfaction/units/men/rohanrohirrim.ini has charge/move/attack voices;
MiscAudio.ini assigns PlanningModeOrderGiven. No mod bytes are committed.

Implemented planning replacement for move/attack/enter through existing
choice collection (including NoSound early exit). Waypoint gate queries
existing live UI, not a cached simulation flag. No new simulation state.
The charge back-reference guard is implicit in the Object-based picker;
its erroneous comment is corrected, not replaced with a headless-object mute.


## Final behavior/test map

| Behavior | RotWK instruction | Synthetic regression |
|---|---|---|
| Mode 2 replaces move/attack/enter before unit-specific rules; empty slot stays silent | 0x8DE901, 0x8DDFD7, 0x8DF416; copy/exit 0x8DE94B / 0x8DE974 | PORT-462 planning replaces move attack and enter exactly once |
| Planning does not replace enter-state move or selection; mode 1 remains ordinary | 0x8DE8F3, 0x8DE908, picker selection case | PORT-462 planning does not replace selection or enter-state movement |
| Waypoint suppression is movement-sensitive, live, and after planning | 0x8DEA45–0x8DEA5C | PORT-462 waypoint movement gate is live and does not mute attacks or planning |
| Waiting for path counts even before moving; enter-state bypasses waypoint suppression | 0x8DEA3B / 0x8DEA49 | PORT-462 waypoint gate includes waiting for path but excludes enter-state |
| Charge helper checks drawable's Object, not Object's client hooks; existing timeout preserved | 0x8DE15C, 0x671EE4, 0x671F0B, 0x671F16 | PORT-462 charge guard is a drawable Object back-reference not clientHooks presence |

Full Release target built with Godot disabled, -j1, after rebase onto
origin/main 10bdabc1. Final filter: -tc="PORT-462*".
5 test cases passed, 90 assertions passed, 6717 other cases filtered out.

Negative control used the SAME full engine/build/openbfme_tests, never a
narrow runner: disabled only the planning replacement and waypoint gate,
compiled the affected engine object and rebuilt its archive and full link
using the commands emitted for the CMake openbfme_tests target. Result:
2 test cases passed, 3 failed; 76 assertions passed, 14 failed. Restored
source byte-for-byte, rebuilt the same full binary: 5/5, 90/90 passed.
The tests and expectations were unchanged during this control.

First execution found a fixture error (LogicWorld did not register live AI
modules); switched to the existing MoveWorld. No expectations were changed.
No foreign tests, hashes, workflow files or simulation snapshots changed.

Remaining: VoiceCrush stays S-704 as explicitly excluded; stance-button
planning and planned-action message handling remain the separate S-703
scope. The latter is why the helper's 0x46A bypass is documented but this
port does not introduce an additional message dispatch path.
No player-visible INFERENCE was introduced. Drawable back-reference is
represented by the existing live Object candidate, not by a new boolean.
Upstream PR not proposed: this change affects sound only and the required
before/after captures are not available in this sandbox. Owner performs
in-game listening checks in the fork PR.
