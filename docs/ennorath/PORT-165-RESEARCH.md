# Port 165: horde position-attack machine investigation

Issue #394. Investigation only; this does not implement or verify the fix.
Addresses are RotWK 2.01 (RW), with BFME2 donor paths under
`Code/GameEngine/Source/GameLogic/AI`.

## Selection and state rows

`AIAttackState::onEnter` RW 0x74CF69–0x74CFCA computes its local melee
flag only for an object attack with a goal object, and clears it for an
immobile owner. At 0x74CFF1–0x74D019, after earlier special-case branches,
a HORDE with that flag clear and the attacking-object flag clear selects
kind 7; other HORDE attacks select kind 6. The port's
`AIAttackState::createAttackMachine` selects the horde machine based on the
AI module without a separate position branch.

The kind-7 constructor RW 0x74548D defines these rows (also BFME2
`AttackPositionHordeStateMachine.cpp`, 0x00343A86):

| State | Constructor / vtable | Success | Failure |
| --- | --- | --- | --- |
| 0x2BC | RW 0x744379 / 0xC28CD8 | 0x2BD | 0x2C0 |
| 0x2C0 | RW 0x740E16 / 0xC277E8 | 0x270F | 0x270F |
| 0x2BD | RW 0x740E67 / 0xC278B8 | 0x2BE | 0x2BC |
| 0x2BE | RW 0x740E16 / 0xC277E8 | 0x2BC | 0x2BC |

The two 0x740E16 states are the *same class*: onEnter 0x747999 and update
0x742CEA. They are not the generic weapon-finish wait state. On entry,
0x747999 returns -2 or 0 according to the template bit at +0x114, mask
0x2000; this is KindOf HORDE (bit 109, also +0x115 mask 0x20).
Hordes return 0 (continue) on entry; non-hordes return -2 (failure).

Update delegates to RW 0x742C8C. With no goal object it calls the horde
interface slot 0x14C (RW 0x86D562): all living members with an AI must be
attacking; members without an AI or marked dead are skipped and an empty
list passes. A true result keeps the state running, while false returns
failure. With a live goal object, slot 0x148 (RW 0x86D438) additionally
checks the members' current victims; a destroyed goal returns failure.
This matters for both rows: a successful position shot returns to approach
only after the members no longer all attack. An unconditional failure or
`AIWaitUntilFinishedFiringState` substitution is not equivalent.

## Approach and position fire

The 0x2BC state's RW vtable has onEnter 0x74E9DD, update 0x74A49B and
onExit 0x74A4EE. OnEnter calls the weapon's position-range test 0x6CC622
at 0x74EA42; that call is **not a shot**. BFME2's position-approach donor
is `AIAttackApproachTargetStateOnEnter.cpp` at 0x0034D049, with update in
`AIAttackApproachTargetStateOnExit.cpp` at 0x003489F2. RW update's internal
range/repath logic is 0x74A3E2. It has distinct state and path behaviour
that must be compared with the port's generic `AIAttackApproachState`.

The 0x2BD state has onEnter 0x74C911, update 0x74C9FF and onExit 0x74C8BE.
It uses position-range testing at 0x74C99F; the pre-fire call is 0x69213E
at 0x74C9F3. Its vtable is distinct from generic `AIAttackFireState`
(0xC27858), so simply reusing the generic state would require a full
behavioural proof, not just matching the transition graph.

## Required next steps

* Port these states and selection precedence against the RW routines, not
  a generic-state substitute; include each new simulation field in xfer.
* Recreate synthetic PORT-165 coverage, including in-range and out-of-range
  position attacks, non-position horde attacks, save/load, the toggle,
  projectile impact/FX, and an archer regression. The previous diagnostic
  reproduction saw the corsair leave state 9 without a shot; it was not
  the required full projectile/FX acceptance test.
* Check other owners of the shared states and finish the issue's review
  and upstream eligibility requirements. No game or mod data is committed.
