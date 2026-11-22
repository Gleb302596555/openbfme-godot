# PORT-136 — the summoned eagle that hovers in guard: the guard machine, and what actually blocks it

Issue #338. Image: `workspace/RotWK/game.dat`, 11 346 944 bytes, md5 `daed3668006cd90f01c34e5a7da1901f`.
Every address below was read with `python tools/retail_oracle/disasm.py workspace/RotWK/game.dat <va> <bytes>`
and the vtables with a `pefile` slot dump; the mod data is Ennorath 1.9.8.2 (`eagleIni` in
`engine/tests/test_port_136.cpp` is the mod's Gwaihir shape).

Two results, in the order the work found them:

1. **The guard machine RW 0x76A6CC is a copy of the generic `AIGuardMachine`** (section 1–3). Its four states are the
   Return / Idle / Inner / Outer states of the generic machine under the bird's own class names, with the same
   transitions and the same bodies. Porting it cannot change what the eagle does.
2. **The eagle hangs because the generic attack approach never gets a path for a flying object** (section 4). The
   attack state runs, the approach runs, and the internal move reports "arrived" on the first frame because the
   mover's distance-to-goal of a flyer with no path is 0. The aim state then sends it back to the approach, forever.
   The RW branch that gives a flyer its path is the one `AIMover::computeAttackPath` reports as NOT PORTED (S-784).

---

## 1. The machine class

| fact | RW |
|---|---|
| factory of the machine (allocates 0x74 bytes, calls the ctor with the AI) | RW 0x76B453 |
| constructor | RW 0x76A6CC (`mov [esi], 0xC2DDF0`; SEH name 0xB9293A) |
| base constructor (`AIStateMachine`) | RW 0x949A10, arg `0x3EDCCF0B` |
| finalised by | RW 0x8DB9CF (the machine's `setDefaultState`) |
| `addState` | RW 0x8DBE1D |
| vtable | RW 0xC2DDF0 (16 slots; [2] `getMachineName` = RW 0x769C2B → the string at RW 0xC2DE30 "GiantBirdGuardMachine", [3] RW 0x769C70, [9] RW 0x769C31) |
| guard scan (a method of the machine, `this` = the machine: `[this+0x14]` the Object, `[this+0x3C]` the guarded object id, `+0x40` the team, `+0x44` the area, `+0x48` the position to guard, `+0x68` the nemesis, `+0x6C` the guard mode) | RW 0x76BC7D |

`addState` is cdecl with `this` in `ecx`; the pushes run right to left, so the four registrations of RW 0x76A6CC read
`addState(id, state, nextOnSuccess, nextOnFailure, 0)`:

| id | class | ctor | size | vtable | onEnter | onExit | update | success → | failure → |
|---|---|---|---|---|---|---|---|---|---|
| 0xB798 | GiantBirdGuardInnerState | RW 0x76A67D | 0x44 | RW 0xC2E0C0 | RW 0x76C087 | RW 0x94BC05 | RW 0x76C1C2 | 0xB79A | 0xB79A |
| 0xB799 | GiantBirdGuardIdleState | RW 0x769AEC | 0x30 | RW 0xC2DCD0 | RW 0x769CFC | RW 0x9F3A3C | RW 0x76C617 | 0xB798 | 0xB79B |
| 0xB79A | GiantBirdGuardOuterState | RW 0x769B25 | 0x44 | RW 0xC2DD30 | RW 0x76C259 | RW 0x9495C4 | RW 0x76C3FA | 0xB79B | 0xB79B |
| 0xB79B | GiantBirdGuardReturnState | RW 0x769B74 | 0x34 | RW 0xC2DD90 | RW 0x76C479 | RW 0x76B48C | RW 0x76C528 | 0xB799 | 0xB798 |

The class names are the strings that follow each vtable (17 slots then the name, the same layout as RW 0xC2D988
GiantBirdNormalFlightState and RW 0xC2D940 GiantBirdFollowPathState). The first state registered, 0xB79B, is the
default: `GuardReturnState`.

The ctors also give the base classes: RW 0x769B74 (0xB79B) calls **RW 0x7691CC with (machine, 1, 1)** — the
`GiantBirdNormalFlightState` constructor of the port at `GiantBirdAIUpdate.cpp`, i.e. the return state *is* a normal
flight state and flies to its post with the bird's own spline; its `onExit` RW 0x76B48C is `jmp RW 0x76AED0`, the
normal flight's exit. The other three call the plain `AIState` constructor RW 0x8DB7B0.

Field offsets of the states: `+0x18` the machine, `+0x20` a sub-object with vtable RW 0xC2DCC8 on the 0x44-byte
states (the attack's exit conditions), `+0x24` a clock / an exit-conditions word, `+0x28..+0x30` a position, `+0x34`
a radius squared, `+0x38` a deadline, `+0x3C` the `AIAttackState`, `+0x40` "re-run onEnter".

The StateMachine transition rules the port already implements (`AIStateMachine::internalSetState`,
`checkForTransitions`, `checkForSleepTransitions`) are unchanged.

## 2. The four bodies

**0xB79B GiantBirdGuardReturnState** (default state). onEnter RW 0x76C479: the frame from `[RW 0xDE412C]+0x40`;
`RW 0x6D328E(0, [RW 0xDE4B40]+0x18 → +0x44, RW 0xC2E120, 0xB08)` — the per-frame duration of the logic clock — is
added to it into `+0x24`; the machine's `+0x48` position to guard is copied to the state's `+0x28`, then the guarded
object (machine `+0x3C` through RW 0x449681), the guarded team (machine `+0x40` through RW 0x7A1EEE → RW 0x7A0ECD)
and, last, the area (machine `+0x44`, RW 0x70CE80 / RW 0x6E4CA7) override it.
update RW 0x76C528: once `now >= +0x24`, `+0x24 = now + [RW 0xDE4B40]+0x18 → +0x44` and the scan RW 0x76BC7D runs on
the machine; **found → return -2** (STATE_FAILURE → 0xB798 the inner state). Not found: the guarded spot is
recomputed; if it moved more than the constant RW 0xBD83D8 from the state's `+0x28`, the position is copied and
`RW 0x661EFE(The machine, &position)` (**the machine's goal position is set**) followed by RW 0x76AED0(0)
(the normal flight's exit) and RW 0x76AD1D (its enter, i.e. a fresh path to the new post); otherwise RW 0x76AE06 (the
normal flight's update) runs. `-2` on the sleep path is the state's own failure.

**0xB799 GiantBirdGuardIdleState.** onEnter RW 0x769CFC: `+0x20 = [RW 0xDE412C]+0x40 + 2` (scan two frames later).
update RW 0x76C617: while the frame is below `+0x20` it returns the difference (a sleep); then
`+0x20 = now + [RW 0xDE4B40]+0x18 → +0x40` (`AIData GuardEnemyScanRate`); a busy team (`[team+0x238]`) takes the
`AI vslot 0x20(0x138C)` branch instead of the scan (INFERENCE: the callee is not identified from the bytes; the
value 0x138C is passed as is); the scan RW 0x76BC7D **found → -1** (STATE_FAILURE → the inner state). Not found: the
guarded spot is resolved again and compared with `+0x24` and `+0x28` (RW 0xBDBCA0, squared); a spot that moved
returns -2 → the inner state, otherwise the tail returns `+0x20 - now` (a sleep).

**0xB798 GiantBirdGuardInnerState.** onEnter RW 0x76C087: the machine's goal object `+0x68` through RW 0x449681 —
**no goal object → return -1**; the guarded spot into `+0x28`; `+0x24 = 5` (the exit-conditions word:
`ATTACK_ExitIfOutsideRadius | ATTACK_ExitIfNoUnitFound`); `+0x34 = getStdGuardRange(owner)²` (RW 0x949567, squared);
an area guard replaces both with the polygon's radius² (RW 0x70CE80) and centre (RW 0x6E4CA7). Then
`new 0x50` + **RW 0x74CABC(AI, 0, 1, 0, &+0x20)** — the ordinary `AIAttackState`, follow 0, attacking an object,
not forced, with the state's exit conditions — stored at `+0x3C`; its machine's vslot 0x38 gets the goal object and
its vslot 0x10 (onEnter) runs; `neg eax; sbb eax,eax` turns a non-zero result into -1 (failure).
update RW 0x76C1C2: `+0x40` set → cleared and the state re-enters itself (tail call of vslot 0x10); no attack object
→ -1; the guarded spot is re-read; the attack's vslot 0x18 runs (its update); the victim (the attack machine's goal
through RW 0x8DBACE) is tested for the dead bit `[eax+0x458] & 1`; when it is dead or RW 0x769C97 reports the attack
machine done (`cmp [this+0x3C], 0` else the attack's vslot 0x48) → -1, otherwise the attack's own return code.
onExit RW 0x94BC05 is the generic inner state's exit.

**0xB79A GiantBirdGuardOuterState.** onEnter RW 0x76C259: `[machine+0x6C] == 1` (GUARDMODE_GUARD_WITHOUT_PURSUIT) →
-1; the guarded spot; `RW 0x6FF412(owner, 3)` (the adjusted vision range) squared into `+0x34`; a guard area's centre
(RW 0x70CE80 / RW 0x6E4CA7) or the spot with the terrain height; `+0x38 = RW 0xA3CFA4()` — the current time as a
float plus the frame clock's count (`[RW 0xDE4B40]+0x18 → +0x3C`) — the deadline; `+0x24 = 7`
(`… | ATTACK_ExitIfExpiredDuration`); then the same `new 0x50` + RW 0x74CABC(AI, 0, 1, 0, &+0x20) block as the inner
state and its goal/enter sequence. update RW 0x76C3FA is the inner update's shape (re-enter on `+0x40`, the attack's
update, the dead test, RW 0x769C97). onExit RW 0x9495C4 = the generic outer state's exit.

So the bird's machine is the generic `AIGuardMachine` minus the crate state (RotWK has no crates) and minus the
"attack aggressor" state, with the return state flying the bird to its post with the bird's own normal-flight body
instead of an `AIMoveToState`. The generic machine the port already runs has the same transitions
(`AIGuard.cpp`, `AIGuardMachine::AIGuardMachine`: Return → (Idle, Inner), Idle → (Inner, Return), Inner → (Outer,
Outer), Outer → (Crate, Crate)) and the same bodies (`GuardReturnState` RW 0x94A69E, `GuardIdleState` RW 0x94A70A,
`GuardInnerState` RW 0x94A048 / RW 0x94A183, `GuardOuterState` RW 0x94A29F / RW 0x94A407).

## 3. The scan RW 0x76BC7D

Same body as the generic `AIGuardMachine::lookForInnerTarget` RW 0x949CA7 with the same field offsets on the machine:
RW 0x691269 (able to attack), the team's common target (RW 0x8DBACE → RW 0x68D7AB, the team slot `+0x31C` and
RW 0x7A3E36), the guarded spot through RW 0x449681 / RW 0x7A1EEE / RW 0x7A0ECD with the machine's `+0x48` as the
fallback, `RW 0x949567` for the range (an area: RW 0x70CE80 around RW 0x6E4CA7, and the scan rate `+0x40` against
the machine's `+0x70`), the guard-mode filter (`[machine+0x6C] == 2` adds a filter with vtable RW 0xC2D930 and the
constant RW 0xC10E20) and **`RW 0x701443(owner, range, flags = 0x4A, attackInfo, …)`** — the port's
`AI::findClosestEnemy(obj, pos, range, flags, info, nullptr, 0)`, whose `guardQualifiers` produce 0x4A for this
fixture (CAN_ATTACK 0x02 | CAN_SEE 0x08 | AUTO_ACQUIRE 0x40). The found victim's id goes to the machine's `+0x68`
(RW 0x76BCD0 reads the victim's `+0x74`).

## 4. What actually blocks the eagle (port trace)

A traced probe of the mod's eagle in guard (200, 200) with a Swordsman at (600, 200), reproduced with the port's
guard machine, the attack state, the aim state and the internal move instrumented one by one:

```
guard idle f=3 found=1                     GuardIdleState::update -> lookForInnerTarget true
attack enter: initDefaultState=0           AIAttackState::onEnter: the attack machine is made, pursue -> approach
guard inner enter f=3 nemesis=2 attackEnter=0
approach enter f=4: inRange=0 immobile=0 mobile=1 speed=24.000 victim=2
approach startMove f=4: enter=0            AIMoveToState(mover, goal, attack path) enters
move SUCCESS point 3 f=5                   AIMoveToState::update: "arrived" (locomotorDistanceToGoal 0)
setState id=102 (aim) f=5                  the approach returned STATE_SUCCESS
setState(ext) id=101 f=6                   AIAttackAimState::update (AIAttack.cpp:800) back to the approach
... the same two transitions every frame, the object never moves ...
```

The move never gets a path: `AIMover::computeAttackPath` (the port's `AIMover`, RW 0x6668CA) implements only the
ground search `Pathfinder::findAttackPath` (RW 0x6FC18E), which the port's `AIMover::computeAttackPath` itself reports
as S-784 ("NOT ported: the flying branch"). RW chooses between two pathfinder searches on the attacker's template
byte `[object + 0x11F] & 0x80` at RW 0x666D03: clear → the ground vtable slot +0x08 (RW 0x6FC18E), set → the slot
+0x0C (**RW 0x6FDE38**, pathfinder vtable RW 0xC1B8D8, the branch the port calls "the ship search / S-784").
With no path, `AIMover` leaves the goal unregistered, `AIUpdateInterface::locomotorDistanceToGoal` returns 0, and
`AIMoveToState::update` takes the "arrived" branch at once (its third success return), so the approach reports
success, the aim state finds the victim far outside the 20-unit weapon range and sends the machine back to the
approach. The eagle is therefore never moved, never fires and sits in the guard state — with the generic machine and
with the bird's copy alike. The direct attack command (probe L, `AIUpdateInterface::aiAttackObject`) works because
it goes through the bird's own swoop state, which builds the bird's aerial spline and never asks the mover.

Consequences for the fix:

* the guard machine port (RW 0x76A6CC) is a fidelity item (S-4040) and must be done, but it is **not** what makes the
  eagle attack;
* the PORT-136 fix has to give the attack approach a path for a flying attacker — the RW 0x666D03 branch and the
  pathfinder's +0x0C search RW 0x6FDE38 (the same S-784 entry), or, if that search stays out of scope, the port's
  aerial spline wired into `AIMoveToState` for an aircraft with the RW 0x6668CA flying path (INFERENCE marker
  required). The neighbour check stays: a foot unit in guard must keep walking and attacking as it does now.

---

## 5. The fix this lane took (issue #338)

Section 4 ruled out the move machine as the place to fix a flyer: RW 0x666D03's flying branch (the pathfinder's
+0x0C search RW 0x6FDE38) is still S-784, and wiring the port's aerial spline into `AIMoveToState` would have been an
inference in the middle of every unit's move. The species-independent way out is the one RW itself gives the bird:
**the attack state asks its AI for its own attack machine, and the bird's answer is the swoop-then-idle machine.**

* RW 0x769C31 (the giant bird's state machine, its vslot 9) makes RW 0x769335 and hands it to the attack state. The
  port keeps that factory on the AI — `AIUpdateInterface::makeAttackThenIdleMachine`, the same RW 0x769335 body the
  bird's attack move already used (`GiantBirdAIUpdate.cpp:1307`) — and `AIAttackState::createAttackMachine`
  (`engine/src/GameLogic/AI/AIAttack.cpp`) now asks for it first. Every other AI answers null, so every ground unit
  keeps the generic / melee / horde machine it had. INFERENCE on the plumbing (the factory lives on the machine in RW,
  on the AI in the port, because the port has no per-template state machine factory); the behaviour is the bird's.
* The swoop machine ends a swoop in its idle state (state 0, `SwoopIdleState`, RW 0x769EA0). Idle is
  `STATE_CONTINUE`, which `AIAttackState::updateAttackMachine` flattens to "the attack is still running", so the first
  swoop that missed would hold the attack state for ever. The bird's own attack move polls the machine the same way
  (idle → goal object → `setState(10)`), and `updateAttackMachine` now does the same while the victim lives: another
  swoop instead of a hover.

The eagle therefore swoops the victim it finds in guard, dives, fires its weapon and keeps swooping until the victim is
dead — RW's own shape for a bird (the guard machine RW 0x76A6CC only decides *whom* to attack; its inner and outer
states are ordinary `AIAttackState`s, section 1).

Where RW restarts the next swoop is not read (RW's attack state only flattens a positive sleep, RW 0x7515C8 ..
0x7515DA), so the restart is the port's own decision and is recorded as the stop `S-7633`
(`docs/stops.d/S-7633.md`), together with the factory's place on the AI instead of on the machine. The branch merged
`origin/main` after this analysis was written: the attack-machine choice there also has the horde-position kind 7
(PORT-165) and the charge kind 8 (PORT-133), and the bird's branch now stands first as RW 0x74CFCA's giant-bird
precedence — `MADE_SWOOP` is 7.

The tests are `engine/tests/test_port_136.cpp` (lane `engine/cmake/Port136.cmake`):

* `PORT-136: the eagle in the guard state takes off, closes in and hits its target` — the mod's eagle shape with its
  StancesBehavior, an enemy 150 units away: the first hit lands at frame 13 and the enemy is dead by frame 50, with the
  bird airborne (its own `makeAttackThenIdleMachine(10)` is non-null, the foot AI's is null);
* `PORT-136: a foot unit in the guard state still walks to its target and hits it` — the neighbour: the same guard, the
  same enemy distance, a walking unit; it hits at frame 13 too, on the ground.

One measured guard rule the test has to respect (and the earlier probes did not): the guard's attack state leaves when
the *owner* strays further from the guard post than `MaxRetaliateDistance` (`GuardExitConditions::shouldExit`, RW
0x94BD19; 200 in this fixture's GameData). An enemy 400 units out therefore makes the guard attack, exit, return and
re-acquire for ever — with any attack machine — so the guard test puts the enemy inside that range.
