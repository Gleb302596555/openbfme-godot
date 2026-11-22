# PORT-220: availability investigation (in progress)

Image: RotWK 2.01, MD5 `daed3668006cd90f01c34e5a7da1901f`. Instructions
read with `tools/mod/rw.py`; DONOR BFME2 0x0053BD66 is
`Code/GameEngine/Source/GameClient/GUI/ControlBar/ControlBarRappellerCount.cpp`.
The following is a partial investigation, **not** a complete comparison of the
function. See `docs/re/rotwk/RW_0x942733.md` for the existing arm inventory.

## Matrix census

The checked-in matrix has 587 DISABLED rows for 177 distinct buttons. Resolving
those buttons in the Ennorath 1.9.8.3 INI snapshot with `ennorath_graph.load_db`
finds all of them: 423 object-upgrade, 138 special-power, 19 horde-formation,
and 7 fire-weapon rows. Counts are rows, not distinct buttons. Of the 423
object-upgrade rows, 391 have a NeededUpgrade field. These counts do not establish
why any particular button was disabled: upgrade ownership, object state, other
fields and the actual runtime evaluator still need to be checked.

## Common gates checked so far

| RotWK addresses | Decision | Current port comparison |
|---|---|---|
| 0x942733–0x942798 | Set progress to one, resolve local player and substitute a command center for command types 0x20 and 0x26; hide without player or object. | `evaluate` receives an object and uses its controlling player; center substitution and progress require a caller-level comparison. Not yet classified. |
| 0x9427A0–0x9427B7 | Script-status bits 0/1 or disabled mask bit 5 hide. | Same object gates in `evaluate`; player selection still needs comparison. |
| 0x9427BD–0x9427E5 | Mounted-only / unmounted-only mismatches restrict. | Same in `evaluate`. |
| 0x9427EB–0x94280D | Option 0x40000000 asks AI virtual slot 0x228; nonzero restricts. | Missing in `evaluate`; impact on matrix not yet established. |
| 0x94280D–0x942826 | Except command 0x18, model-condition helper 0x942490 returning hidden causes restricted. | Same structure in `evaluate`; helper and command identity still need full comparison. |
| 0x942826–0x94282D | Nonzero object byte 0x45B restricts. | No corresponding check visible in `evaluate`; byte's semantics and matrix impact unresolved. |
| 0x942833–0x94284E | Missing experience tracker or positive required rank above current rank hides. | Same in `evaluate`. |
| 0x942854–0x9428C6 | Disabled check, sole bit-8 exception, six command exceptions and recursive hidden-vs-restricted test. | Same structure in `evaluate`; bit and enum mappings require further verification. |
| 0x9428CB–0x942993 | NEED_UPGRADE: player upgrades query 0x6AC2AF; object upgrades query 0x691421; ANY short-circuits on an owned upgrade, ALL restricts on a missing entry. Missing object upgrade also checks buildability; null entries are skipped. | Same structure in `evaluate` for player and object entries; unknown upgrade-type handling and selected player need further comparison. |
| 0x942993–0x942A24 | NEEDS_CASTLE_KINDOF tests castle members, hides if no match. | Same structure in `evaluate`; detailed prior analysis: `PORT130_RW_ANALYSIS.md`. |
| 0x942A24–0x942A58 | Production slot 0x54 and NOT_QUEUEABLE restrict; slot 0x44 equal to 20 marks queue full. | NOT_QUEUEABLE matches; queue handling must be checked separately per switch arm. |

DONOR BFME2 0x0053BD66 (`ControlBarRappellerCount.cpp:405`) shows the same
broad common-gate sequence. The RotWK instructions above, not donor names,
are the authority for these classifications.

## Outstanding

Read all switch arms and their call sites, distinguish fixture restrictions from
port differences using the actual gating fields, and test every changed branch.
PR 518 is open and changes the construction arms of `ControlBar::evaluate`;
its diff was inspected, but no overlapping construction change is made here.
No claim of a complete port or of a test pass is made by this note.

## Common gates: retail evidence for the two unported branches

Retail disassembly (`tools/mod/rw.py dis`, `workspace/RotWK/game.dat`) plus a whole-`.text`
linear scan (capstone, `skipdata`) for every `[reg + 0x45B]` access. The scan is the reason the
claims below are exhaustive rather than sampled: five accesses exist, and one of them
(0x699C88) only decodes as an instruction from the aligned start 0x699C82, where it is the
middle of the constructor's byte run `+0x45A`, `+0x45B`, `+0x45C` before `call 0x691BAB`.

### Object +0x45B: the single-use-command flag

| RW | Instruction | Meaning |
|---|---|---|
| 0x699C88 | `mov byte ptr [esi + 0x45B], bl` | `Object::Object` clears it (bl = 0) with the neighbouring flags |
| 0x940397 | `mov byte ptr [ecx + 0x45B], 1` / `ret` | the two-instruction setter; `rw.py xref 0x940397` finds no rel32 caller |
| 0x9422C2 | `mov al, byte ptr [ecx + 0x45B]` / `ret` | the one-byte accessor, next to the accessors for +0xEC and +0x98 |
| 0x94066B | `mov byte ptr [ebx + 0x45B], 1` | inline write in the command path: RW 0x940625 calls `getCommandAvailability` (0x942733), accepts results 1 / 2 / 6 / 7, then RW 0x940665 tests `byte ptr [esi + 0x1E] & 2`, i.e. button option bit 0x20000 = `SINGLE_USE_COMMAND` (`kOptionNames` index 17) |
| 0x942826 | `cmp byte ptr [ebx + 0x45B], 0` / `jne 0x9438C8` | the availability gate: a set flag returns 0 (disabled) for **every** command, before the experience-tracker gate at 0x942833 |

RW 0x94280D (`cmp dword ptr [edi + 0x14], 0x18` / `je 0x942826`) skips only the script
availability call RW 0x942490 for command 0x18; the 0x45B gate still runs for it.

Donor corroboration (not retail proof): BFME2 `gamelogic/object.h:402-403` names the pair
`markSingleUseCommandUsed` / `hasSingleUseCommandBeenUsed`, `ObjectDoXfer.cpp:392` puts the
same byte at +0x43B and line 556 serializes it, and `ControlBarRappellerCount.cpp:445`
returns `COMMAND_RESTRICTED` from it in the same position between the script availability
call and the experience tracker.

Port status: `engine/src/GameLogic/Object/Object.h` has no such byte, so `ControlBar::evaluate`
cannot test it. Adding it is new simulation state and has to be xfer'd in the same commit.

### Button option 0x40000000 = ON_GROUND_ONLY, AI vslot 0x228

RW 0x9427EB .. 0x942807: with option bit 0x40000000 (`kOptionNames` index 30) set, the button
reads `obj + 0x260` — the `AIUpdateInterface` already documented at `Object.h:674`
(PORT-109, RW 0x82D79F) — and calls its virtual slot 0x228; a nonzero result jumps to
0x9438C8 (disabled).

Slot 0x228 resolves to **RW 0x66288A** in all twelve `AIUpdateInterface` vtables in the image.
The vtables were located by anchoring three slots the port already documents —
0x30 = 0x851E97, 0x118 = 0x667290, 0x13C = 0x66491A — and every anchor hit agreed.

```
0066288A  cmp  dword ptr [ecx + 0x1F0], 0   ; zero -> false
00662896  mov  eax, dword ptr [ecx + 0x1FC] ; dec eax; je 1 -> case 1; sub eax,3; jne -> false (case 4)
006628A5  mov  esi, dword ptr [ecx + 0x140] ; null -> false
006628B1  call 0x5E2DF4                     ; true -> true
006628C0  call 0x661E50                     ; true -> true, else false
```

The donor's `AIUpdateInterface::rva00262BA9`
(`Code/GameEngine/Source/GameLogic/Object/Update/AIUpdateInterface_rva00262BEC.cpp`) has the
same shape over `m_field1F0`, `m_locomotorGoalType` (+0x1FC), `m_path` (+0x140) and two path
queries — corroborating the field roles, not proving them.

Port status: `AIUpdate.h` / `AIUpdate.cpp` hold no counterpart for +0x1F0, the +0x1FC goal
type, +0x140 or the queries RW 0x5E2DF4 / 0x661E50 (the only `0x1FC` hit is the unrelated
virtual-slot number of `enterTargetOf`). The ON_GROUND_ONLY branch therefore stays
unimplemented pending that AI state; it is a stop candidate, not a guess.

### Tool status

`tools/mod/matrix_grey.py` now names explicit model-condition setters (`SetModelCondition`
special-power fields, `ToggleMountedSpecialAbilityUpdate`, the `TOGGLE_WEAPONSET` button) for
the selected object only; animation `ModelConditionState` blocks are readers and are not
reported. On Ennorath INI 1.9.8.3 that identifies a candidate setter for 4 of the 49 model
rows; the rest stay `-`, and a `-` never means the button is ungated.

## What this session changed in the engine

`ControlBar::evaluate` now carries the Object +0x45B gate at retail's position
(commit `93589f8a`): after the script availability call (RW 0x942490) and before
the experience tracker's gate (RW 0x942833), a set flag returns
`ButtonState::Restricted` (RW 0x9438C8's availability 0) for every command. The
flag itself is new simulation state — `Object::markSingleUseCommandUsed`
(RW 0x940397) / `Object::hasSingleUseCommandBeenUsed` (RW 0x9422C2) — saved by
`Object::xfer` version 6 and hashed by `crcOmittingModules` when set, so an
object that never pressed such a button keeps both its snapshot layout and its
hash. `ControlBar::pressButton` writes it where processCommandUI does
(RW 0x94066B): the button holds Options bit 17 (`COMMAND_OPTION_SINGLE_USE_COMMAND`,
the name RW 0xDA4C88's table gives index 17) and the bar's last evaluation was
Enabled or Active, retail's availability results 1 and 2. Retail's results 6 and
7 leave through their own paths and write nothing; the button's +0xF8 that
RW 0x940656 clears is client state this port does not carry (S-290).

The ON_GROUND_ONLY half stays unimplemented and is stop **S-7427**
(`docs/stops.d/S-7427.md`): AI +0x1F0 / +0x1FC / +0x140 and the path queries
RW 0x5E2DF4 / RW 0x661E50 have no counterpart in `AIUpdate.h`, and reading
RW 0x66288A alone does not prove what its fields mean.

Ennorath INI 1.9.8.3 contains no `SINGLE_USE_COMMAND` button, so none of the
mod's 587 `DISABLED` rows is explained by this gate; it is one of the common
branches the issue asks `evaluate` to be compared against, and the census rows
it does explain are retail's.
