# PORT-164: the GiantBird contact point through mode 1 — RW map and stop

Issue #387, task 140. Branch `arena/b80f3322-openbfme-godot` (from `main` `d5b594b`).

Status: **research stop.** No engine code is changed in this branch. The one fact the
change hangs on (what RotWK's `preferredPosition` holds when the GiantBird asks for the
contact point in mode 1) cannot be read in this sandbox: there is no `game.dat` here and
no network route to the copy on the owner's Drive (see "Input access" below), so
`tools/mod/rw.py` cannot be run at all. Everything below is either a `DONOR BFME2` fact
(files of `/tmp/open-bfme-2`, BFME2 1.06) or a place in the port, read from this tree.

## Input access (what was tried, 2026-11-13)

- `engine/tests` and the repository: no `game.dat` (a `git rev-list --objects --all`
  scan finds none), `/tmp` and the whole filesystem hold only the donor's BFME2
  `game.dat` (`/tmp/open-bfme-2/baselines/bfme2/workshop-vanilla-1.06/files/game.dat`,
  a different binary — its addresses are the `DONOR` ones below, never RW).
- Google Drive connector: lists the `for-arena` folder (`game.dat`, id
  `1jTwxDvB1quONRFpsFwRrLP7-dl3WUsEB`, 11346944 bytes; `ennorath-ini-1.9.8.2.zip`;
  `ennorath-audit.zip`) but exposes no download tool.
- `curl` from the sandbox: `drive.google.com`, `drive.usercontent.google.com`,
  `example.com`, `raw.githubusercontent.com`, `www.googleapis.com` all answer `000`
  (blocked); only `api.github.com`, `github.com`, `codeload.github.com`,
  `registry.npmjs.org`, `pypi.org` answer. So the 11 MB binary can only reach the
  sandbox as a GitHub object (a repository or a release asset the sandbox can fetch).
- Consequence: **not one instruction of RotWK 2.01 was read for this task.** The
  addresses quoted below as RW are the ones the issue and the existing lane documents
  carry, marked unverified; nothing is claimed as a checked RW fact.

## What RotWK (per the issue, unverified here) and the donor say

The issue's "Known" block, to be checked address by address:

- the GiantBird strike without a matching tag calls the contact point lookup in mode 1:
  RW `0x76B6FF .. 0x76B73F` -> `0x690D20` -> `0xAD3187` and `0xAD3340`;
- mode 0 is RW `0xAD30E0` (already ported);
- mode 1 takes `preferredPosition` — `DONOR BFME2 0x006BEF80`
  (`Code/GameEngine/Source/Common/System/GeometryInfoCollidesWith.cpp:199-203`):

  ```c
  case 1:
    contactCopy(*out, preferredPosition);
    if (out->z > getMaxHeightAbovePosition()) out->z = getMaxHeightAbovePosition();
    return true;
  ```

## DONOR BFME2 1.06 facts (addresses are the donor's, not RW's)

All three GiantBird call sites use `pref = 1` with a logic-random seed and
`skipCollideTest = false`:

| Donor site | File | Call |
|---|---|---|
| attack state, on-enter `0x37B` | `.../AIUpdate/AIGiantBirdAttackStateOnEnter.cpp:367-371` | `findNearestLabeledContactPointOnTarget(victim, &pos, owner->getPosition(), false)`; else `victim->getWorldspaceBestContactPoint(&pos, owner->getPosition(), 0, 1, GetGameLogicRandomValue(0, 12345678, GIANTBIRD_CPP, 0x37B), false)` |
| swoop state, on-enter `0x254` | `.../AIUpdate/AIGiantBirdSwoopStateOnEnter.cpp:386-392` | same, seed `0x254` |
| swoop state, update `0x294` | `.../AIUpdate/AIGiantBirdSwoopStateOnEnter.cpp:437-446` | same, seed `0x294` |

`preferredPosition` is a file-scope `Coord3D` next to `defaultPosition`
(`GeometryInfoCollidesWith.cpp:123`) and **no donor code writes it** — a grep of the whole
`Code/` tree finds only that declaration and the read in `case 1`. `defaultPosition` *is*
filled at geometry parse (`0x006BEF80`'s prologue, `(0, 0, height * 0.67f)`, the port has
it as the mode-3 start). So in BFME2 1.06 mode 1 returns the victim-local **origin** with
`z` clamped to the geometry height; whether RotWK 2.01 does the same, and whether the point
is per-template (a parse-time field) rather than file-scope, is exactly what must be read
in `game.dat` — the donor cannot answer it, and the donor may not be ported where RW is
unread.

## The port today

| Place | Behaviour |
|---|---|
| `engine/src/GameLogic/Object/ObjectGeometry.cpp:338` `ObjectGeometry::getBestContactPoint` | modes 3 (line 354) and 0 (line 407); `preference == 1` falls through to `return false` — no `preferredPosition` exists anywhere in `engine/src` (grep: 0 hits) |
| `engine/src/GameLogic/Object/ObjectGeometry.cpp:457` `worldspaceBestContactPoint` | passes `preference` through; returns `false` for mode 1; BODY_RUBBLE z and the transform back are shared |
| `engine/src/GameLogic/Module/GiantBirdAIUpdate.cpp:174` `seededContactPoint` | the fallback: draws `logic.random().getValue(0, 12345678, ...)` (same range as the donor) but returns `victim position` + `geometry.height * 0.5` — a geometric-centre approximation, not `getBestContactPoint` |
| `GiantBirdAIUpdate.cpp:187` `contactPointOn` | labelled lookup first, else the approximation; catches a `logic_error` with the `[S-1281]` note. Callers at `:622` (seed line `0x254`), `:752` (`0x294`), `:844` (`0x37B`) — the same three seeds as the donor |
| `engine/src/GameLogic/Module/AIUpdateCombat.cpp:225` `findNearestLabeledContactPointOnTarget` | `worldspaceBestContactPoint(target, callerPos, label, 0, 0, preferred, out)` — the 4th argument is the `ignoreCollision`/skip-collide flag in RW's ABI, and the port passes the AI's `preferred` into it |
| `engine/src/GameLogic/BuildStops.cpp:61` (S-1281) | "modes 1 / 3 are refused" — stale for mode 3, which Local 41 ported |

## The three unknowns that block the change

1. **What mode 1 returns in RotWK.** RW `0xAD3187` / `0xAD3340`: is the branch
   `out = preferredPosition; clamp z <= height; return true` (donor shape), and is
   `preferredPosition` a `GeometryInfo` member (`+0x44` per
   `docs/ennorath/LOCAL41_RW_ANALYSIS.md:34`, where Local 41 saw it set to `(0,0,0)` and
   never read) or a file-scope temporary? If it is the member and nothing writes it, mode 1
   is the victim's origin for every caller, and the GiantBird's fallback is the victim's
   position (z-clamped) instead of the port's position + `height * 0.5`.
2. **Who writes it before the GiantBird calls.** The parse of `Geometry` rows
   (`RW 0xAD4040`), the ctor `RW 0xAD25B0` and any `GeometryInfo` copy must be checked for a
   store to `+0x44`; a store between the ctor and the bird's call would make mode 1 a
   template-authored point that the mod's ram and horde rows could use, and the port would
   have to parse it.
3. **RW `0x667C76`'s fourth argument.** The port passes the AI's `preferred` into
   `ignoreCollision`; the donor's `AIGiantBirdAttackStateOnEnter.cpp:221` names that
   parameter `skipCollideTest`. One of the two is a misreading of the same stack argument,
   and it changes the overlap test on the labelled branch as well.

## Why this decides the reported bugs (and what else would)

- With the donor shape (`out` = victim-local origin, z clamped to the geometry height), a
  swoop on a target **without** the `Swoop` tag (rams, hordes) aims at the victim's own
  centre line, not at `height * 0.5` above the position. Both are inside the target's
  footprint, so mode 1 alone **does not** explain "zero damage to rams" reported as P-64:
  the damage point moving by half a geometry height does not put a nugget outside a ram's
  body. The issue says as much ("the point it picks may put the damage outside the target
  or the radius. Prove it with a test before claiming it").
- The tests the task asks for ("a synthetic giant bird swooping on a synthetic ram and on a
  horde deals the weapon's damage at that point") would then pass or fail on the weapon
  radius / nugget filter / armor, not on the mode. So before porting mode 1, the actual cause
  of the zero damage must be established — it is a second mechanism (weapon nugget filter,
  ram armor `KindOf`, or the swoop's own damage application in
  `GiantBirdAIUpdate.cpp`), and each of those needs RW reads as well.

## Proposed change once unblocked (not implemented)

1. `ObjectGeometry.h/.cpp`: add `preferredPosition` to the geometry data path the port uses
   (`contactPointsOf` next to the points, or a `Coord3D` parameter) and implement
   `case 1` exactly as RW's `0xAD3187` / `0xAD3340` reads, with the address in the comment;
   `preference == 1` then returns `true`.
2. `GiantBirdAIUpdate.cpp`: replace `seededContactPoint`'s approximation with
   `ObjectGeometry::worldspaceBestContactPoint(victim, ownerPos, "", 1, seed, false, out)`
   at the same three seeds (`0x254`, `0x294`, `0x37B`), keeping one path (PROCESS 15.3).
3. Fix the `findNearestLabeledContactPointOnTarget` argument only if RW `0x667C76` says the
   fourth stack argument is the skip-collide flag (unknown 3 above) — a change there touches
   every caller (`AIPathfindMelee.cpp:341`, `DozerAIUpdate.cpp:287`, the bird).
4. Tests `PORT-164*` in a new `engine/tests/test_port_164.cpp` + `engine/cmake/Port164.cmake`:
   mode 1 returns RW's point with the z clamp; a labelled target takes mode 0 and an
   unlabelled one mode 1; the bird's three call sites pass `pref = 1` and the same seeds;
   a swoop on a synthetic ram and on a synthetic horde lands the weapon's damage; plus a
   neighbour test that keeps mode 0 and mode 3 behaviour.

## What the owner can unblock with

Any one of:

- paste the `rw.py dis` listings for RW `0xAD3187` (48 instructions), `0xAD3340` (32),
  `0x76B6FF` (48), `0x690D20` (24), plus `rw.py xref 0xAD3187` and `xref 0xAD3340`, into
  issue #387 (the way issue #269 authorized PORT-109); or
- give the sandbox a way to fetch `game.dat` (a private GitHub repository branch the sandbox
  can `git fetch`, or a connector download route); or
- decide that the donor shape is enough for mode 1 (`INFERENCE`, the branch cannot be
  proven from `game.dat` here) and say so in the issue.

Without one of the three, this task stays a stop: the fork's rule is that every changed
behaviour carries a RotWK address read with `rw.py dis`, and a donor-only branch may not be
ported.
