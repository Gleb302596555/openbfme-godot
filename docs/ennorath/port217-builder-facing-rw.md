# Port 217 / issue #507 — a builder turns to face the structure it builds or repairs (P-123)

Research checkpoint, 2026-11-15. Addresses are in vanilla RotWK 2.01 `game.dat`
(md5 `daed3668006cd90f01c34e5a7da1901f`), read with `tools/mod/rw.py dis`, caveat
S-001. The BFME2 1.06 decomp (`/tmp/open-bfme-2`) is used only as the readable
twin of the same code; every link below was confirmed in RW.

## The mechanism: a dozer dock sub task that faces the goal

`DozerAIUpdate`'s action machine (`DozerActionState` chain) runs the same body for
every task. In `DozerActionDoActionState::update` (RW 0x88D993) the dock sub tasks
are, in order: 0 select the dock location, 1 move to it, **2 face the goal**, 3 do
the work. The port already has the same four sub tasks (`DOZER_SELECT_BUILD_DOCK_LOCATION`,
`DOZER_MOVING_TO_BUILD_DOCK_LOCATION`, `DOZER_FACING_BUILD`, `DOZER_DO_BUILD_AT_DOCK`)
— the third one is the one that does nothing.

Build branch, RW 0x88DD1C .. 0x88DD43:

| RW | instruction | meaning |
|---|---|---|
| 0x88DD1C | `cmp eax, 1` / `jne 0x88dd43` | the dock sub task is 1 (moving)? |
| 0x88DD25 | `call [eax+0x1b8]`, `test al,al` | the dozer's AI vslot 0x1B8 (`isIdle`, RW 0x6643FC) |
| 0x88DD2F | `push 2` / `call [eax+0x48]` | the dock interface's vslot 0x48: the sub task := 2 (facing) |
| 0x88DD38 | `push 2` / `push edi` | the command's two stack arguments: source `CMD_FROM_AI` (2), the goal object |
| 0x88DD3B | `lea ecx, [esi+0x20]` | `this` = the AI's command interface (AI + 0x20) |
| 0x88DD3E | `call 0x7C81B9` | `AICommandInterface::aiFaceObject(goal, CMD_FROM_AI)` — AI command 0x26 |
| 0x88DD43 | `call [eax+0x4c]`, `cmp eax, 2` / `jne 0x88ddea` | the dock sub task is 2 (facing)? |
| 0x88DD5D | `test al, al` / `je 0x88ddea` | the AI is idle again — the turn finished |
| 0x88DD65 | `push 3` / `call [eax+0x48]` | the sub task := 3 (work); then the building sound (0x88DD74) and, for a porter, the fade (0x88DDC1) |

The repair branch is identical, RW 0x88DA58 .. 0x88DA7A: sub task 1 and idle ->
sub task 2, `push 2`, `push edi`, `lea ecx, [esi+0x20]`, `call 0x7C81B9`; sub task 2
and idle (RW 0x88DA86 .. 0x88DA9B) -> sub task 3 (RW 0x88DAA1) and the repair
sound (`UnderRepairFromRubble` 0x88DAC3 / `UnderRepairFromDamage` 0x88DB0C).

`0x7C81B9` is the AI's face command 0x26 body: it builds the command record with id
0x26 (`push 0x26` / `call 0x7536DD`, the command constructor), stores the goal
object in it, and posts it through AI vslot 0 (`call [eax]`). RW 0x7536DD registers
id 0x26 on the same state as 0x27 (36 / 37). The other xrefs of 0x7C81B9 are the
script's `NAMED_FACE_NAMED` / `TEAM_FACE_NAMED` (0x7CA4FD, 0x7CA5E3) and the
GiantBird (0x854D55) — none of them is the dozer's.

So the turning is neither a special dozer animation nor a mover side effect of the
final move: the dozer is idle at the dock, is issued the ordinary face-object
command, and the work sub task starts only once that command has finished (the AI
is idle again). The result is that a build, an add-on and a repair all leave the
builder facing the structure, whatever his facing was on arrival.

The port's twin of the same body is `DozerAIUpdate::updateDoAction`
(`engine/src/GameLogic/Module/DozerAIUpdate.cpp`): the sub task 1 -> 2 transition
is there, but instead of the face command it records the stop

```
// RW 0x88DD3E: AI command 0x26 (face the goal, AI state 36) is not ported (S-1282): the dozer does not turn
logic.noteStop("[S-1282] dozer: the face command 0x26 before the work ...");
```

That is the whole defect: the two sub task 2 gates are ported, the command between
them is not, so the builder walks into the dock with whatever facing he had and
starts working.

`AIUpdateInterface::aiFaceObject` (RW 0x6643B6, ported) and state 36
(`AIFaceState`, ported) already exist and are used by the script and the GiantBird;
nothing in them is new. The face state returns to idle only once the relative angle
is below 0.035 rad (RW 0xC2A1BC) when the dozer has a locomotor whose MinSpeed is 0
(RW 0x5E3796), else it walks the goal — so the "idle again" gate of RW 0x88DD5D is
what makes the work wait for the turn.

## Auto-repair from idle

The owner's P-123 also lists an idle builder that starts repairing by itself. In the
port that path is an ordinary repair task: the message reaches
`DozerAIUpdate::repair` (RW 0x88BD8D), which calls `newTask(DOZER_TASK_REPAIR)`; the
task then runs the same action machine and the same repair branch above, so the
face step covers it. The separate retail mechanism that makes an **AI player's**
bored dozer look for something to repair (BoredTime / BoredRange, RW 0x88E17F) is
not ported at all (S-1282, noted at RW 0x88E64C) and is not what the owner sees with
his own builder; RW 0x88E17F's body starts by taking a filter set up from the
dozer's own vision radius (RW 0x68E43B) and is a search, not a facing, so it is left
to S-1282.

## Left unported by this change

Only the search of RW 0x88E17F (S-1282 keeps it) and the bridge-tower branch of the
dock position (also S-1282). Everything that starts a build or repair task ends in
the face step and is fixed by this one change.
