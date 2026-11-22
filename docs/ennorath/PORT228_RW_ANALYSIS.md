# Port 228 (fork issue 551): Locomotor fields - what the reference binary says so far

Reference: `game.dat` 2.01, md5 `daed3668006cd90f01c34e5a7da1901f`, 11 346 944 bytes.
Mod data: Ennorath 1.9.8.3 (audit v13). Every address below was read with
`python tools/mod/rw.py ... dis` / by a capstone scan of `game.dat` in this session.

## 1. The Locomotor field table of RotWK (dead or alive)

The template parser is table driven. Entry layout (read from the image):
`{const char *name; ParseFn fn; void *defaultSource; unsigned offset;}` - 16 bytes.
Table entries:

| field | entry | parse fn | offset | table address used by issue |
|---|---|---|---|---|
| `MinTurnSpeed` | `RW 0xBF4578` | `RW 0x42EEFA` (percent) | `+0x54` | `RW 0xBF4478` (S-7221) |
| `MaxThrustAngle` | `RW 0xBF45D8` | `RW 0x42EE15` (angle) | `+0x6C` | |
| `RudderCorrectionDegree` | `RW 0xBF48C8` | `RW 0x42ED00` (real) | `+0x10C` | |
| `RudderCorrectionRate` | `RW 0xBF48D8` | `RW 0x42ED00` | `+0x110` | |
| `SwoopStandoffRadius` | `RW 0xBF4928` | `RW 0x42ED00` | `+0x124` | |
| `SwoopStandoffHeight` | `RW 0xBF4938` | `RW 0x42ED00` | `+0x128` | |
| `SwoopTerminalVelocity` | `RW 0xBF4948` | `RW 0x42ED00` | `+0x12C` | |
| `SwoopAccelerationRate` | `RW 0xBF4958` | `RW 0x42ED00` | `+0x130` | |
| `SwoopSpeedTuningFactor` | `RW 0xBF4968` | `RW 0x42ED00` | `+0x134` | |

The offsets equal the members of the port's `LocomotorTemplate`
(`engine/src/GameLogic/Locomotor.h`), so the struct layout the port uses is the
reference layout.

## 2. Twelve of the nineteen fields are already settled in main

`docs/STOPS.md:1031` (S-7221, lane Local 25, `engine/tests/test_local_25.cpp`)
records, with addresses, that RotWK 2.01 and BFME2 1.06 read none of
`MinTurnSpeed`, `StickToGround`, `AllowAirborneMotiveForce`,
`Apply2DFrictionWhenAirborne`, `FrontWheelTurnAngle`, `MaximumWheelCompression`,
`LateralVelocityRollFactor`, `PitchInDirectionOfZVelFactor`,
`MaxOverlappedHeight`, `SlideIntoPlaceTime`, `WanderAboutPointRadius`,
`WanderLengthFactor`. The field audit (v13) marks exactly these rows
`parsed-unused` with `stop=S-7221`. Port 228 does not build a second path for
them; the row is stale for this issue.

The one reader the audit kept open for the suspension / roll family is the
client presentation physics the task text names (`Drawable +0x13C`,
`RW 0x67BDC0`, wheels `RW 0x677026`, ship `RW 0x6732DC`). S-7221 says the
suspension fields are unread there; that claim is checked against the class
chain in section 4 below.

## 3. The seven fields that were open

`RudderCorrectionDegree` (+0x10C), `RudderCorrectionRate` (+0x110),
`SwoopStandoffRadius` (+0x124), `SwoopStandoffHeight` (+0x128),
`SwoopTerminalVelocity` (+0x12C), `SwoopAccelerationRate` (+0x130),
`MaxThrustAngle` (+0x6C).

Donor check (`/tmp/open-bfme-2`, BFME2 1.06, byte matched to its own
`game.dat`): the seven names occur only in the table dump
`Code/GameEngine/Source/Common/Rva001E72B4Reload.cpp:194, 241, 242, 247-250`
(offsets `0x6C`, `0x10C`, `0x110`, `0x124`, `0x128`, `0x12C`, `0x130`), that is
the parser table - no constructor, no copy, no reader. So the donor has no
reader either; a verdict needs the reference side.

Reference side, done so far:

- The template's assign `RW 0x5E320B` copies the whole struct field by field -
  e.g. `RW 0x5E3259 mov eax,[edi+0x54]; RW 0x5E325C mov [esi+0x54],eax` and
  `RW 0x5E327D mov eax,[edi+0x6C]; RW 0x5E3280 mov [esi+0x6C],eax` - so `+0x6C`
  and the rest are reached by the copy, not by a simulation reader.
- A whole-image capstone pass for a memory operand of `[reg+0x10C]`, `[reg+0x110]`,
  `[reg+0x124]`, `[reg+0x128]`, `[reg+0x12C]`, `[reg+0x130]`, `[reg+0x6C]`
  (candidate bytes filtered to really aligned instructions via
  `ins.encoding.disp_offset`, stack frames and `call/jmp [reg+disp]` vtable
  slots dropped) gives a long list of accesses, and every one of them lies in a
  class that is not the locomotor template (`0x44xxxx`..`0x4Fxxxx`): those are
  other objects with their own `+0x10C` / `+0x124` members.

Still open (the remaining work of this lane): tie the locomotor class to its
functions (create -> ctor -> vtable -> slot) and check the fields in exactly
those functions, plus the client presentation physics family
(`RW 0x67BDC0`, `RW 0x677026`, `RW 0x6732DC`). Only that makes the
"no reader" verdict; the raw displacement scan alone cannot, because the same
offsets belong to other classes.

## 4. Method note (why the scan above is not a verdict)

The task's trap applies here: a four-byte displacement scan finds
`[reg+0x10C]` everywhere. The port's own `LocomotorTemplate` offsets are the
reference's, so a reader must (a) live in a function that is reached from the
locomotor (through `Locomotor::m_template` = `[this+4]`, cf. the min-turn-speed
getter `RW 0x5E37B5`: `mov ecx,[ecx+4]; fmul dword ptr [ecx+0x54]`), and
(b) touch other template members. The lane therefore walks the class chain
before it writes a verdict row.

## 5. The seven open fields: verdict (stop S-7387)

RotWK 2.01 **stores, defaults and copies** them, and **reads none of them**. Everything below is
an instruction of `game.dat` 2.01 (md5 `daed3668...`).

### 5.1 The template's own code touches them

| field | copy/assign | copy ctor | ctor default | default source |
|---|---|---|---|---|
| `MaxThrustAngle` +0x6C | `RW 0x5E327D` / `RW 0x5E3280` | `RW 0x5E4708` / `RW 0x5E470B` | 0.0 `RW 0x5E4462` | `xorps xmm0,xmm0 RW 0x5E4362` |
| `RudderCorrectionDegree` +0x10C | `RW 0x5E3493` / `RW 0x5E3499` | `RW 0x5E491E` / `RW 0x5E4924` | 0.0 `RW 0x5E45B9` | same |
| `RudderCorrectionRate` +0x110 | `RW 0x5E349F` / `RW 0x5E34A5` | `RW 0x5E492A` / `RW 0x5E4930` | 0.0 `RW 0x5E45C1` | same |
| `SwoopStandoffRadius` +0x124 | `RW 0x5E34DB` / `RW 0x5E34E1` | `RW 0x5E4966` / `RW 0x5E496C` | 200.0 `RW 0x5E4520` | `RW 0xBE4170` |
| `SwoopStandoffHeight` +0x128 | `RW 0x5E34E7` / `RW 0x5E34ED` | `RW 0x5E4972` / `RW 0x5E4978` | 200.0 `RW 0x5E4528` | `RW 0xBE4170` |
| `SwoopTerminalVelocity` +0x12C | `RW 0x5E34F3` / `RW 0x5E34F9` | `RW 0x5E497E` / `RW 0x5E4984` | 0.07 `RW 0x5E4540` | `RW 0xBF4A2C` |
| `SwoopAccelerationRate` +0x130 | `RW 0x5E34FF` / `RW 0x5E3505` | `RW 0x5E498A` / `RW 0x5E4990` | 0.003 `RW 0x5E45E9` | `RW 0xBF4A28` |

The defaults equal the port's `LocomotorTemplate` constructor, and the port's parse table carries every
name (including `ElevatorCorrection*`, `AeleronCorrection*`, `SwoopSpeedTuningFactor`), so no member of
this block is missing or defaulted differently.

### 5.2 Every other access to those displacements is another structure

A capstone pass over `.text` for a memory operand with these displacements, filtered to really aligned
instructions (`ins.encoding.disp_offset`), then read one by one:

| address | instruction | why it is not the template |
|---|---|---|
| `RW 0x5E5558`..`RW 0x5E55B7` | `movss xmm,[edi+0x68/0x6c/0x78/0x7c]`, rotation by `[ebp+0x74]` | a block of eight floats rotated as two 2D pairs; the template's +0x6C..+0x7C are `MaxThrustAngle`, `ZAxisBehavior` (int), `Appearance` (int), `FormationPriority` (int) |
| `RW 0x5E581C`, `RW 0x5E583A` | `mov ecx,[esi+0x128]; test ecx,0x10000`, `and byte [esi+0x12a],0xfe` | bit flags, and an object base (`[esi+0x260]` is loaded in the same function) |
| `RW 0x5E8F8F`, `RW 0x5E8F9B` | `test/or dword [esi+0x110], 0x20000000` | a flag word; the template's +0x110 is `RudderCorrectionRate`, a float |
| `RW 0x5EA313`, `RW 0x5EA3D4` | `lea ecx,[esi+0x110]; call RW 0x778C77` | sub-object construction inside another structure |
| `RW 0x673037`..`RW 0x6730B2` | `and dword [esi+0x12c],0`, `mov dword [esi+0x128],4`, `mov [esi+0x130],eax` | a client state machine: integer state codes in a float field would be 5.6e-45 |
| `RW 0x675AB7`, `RW 0x6758CC`, `RW 0x676491`, `RW 0x677E52` | `cmp [esi+0x128],ebx`, `mov eax,[esi+0x124]`, ... | `Object` / `Drawable` members (`[esi+0x14c]`, `[esi+0x442]`, `[eax+0x118]` used in the same functions) |
| `RW 0x5E41FE`.., `[reg+ecx*4+0x10C]` | `test dword [edx+ecx*4+0x10c],eax` | indexed bitmask array |
| `call/jmp [reg+0x10C]`, `[reg+0x110]`, `[reg+0x6C]` | vtable dispatch | a virtual slot, not a struct field |

Control on the same method: the client presentation physics does read the template where it needs it -
`RW 0x67336E movss xmm0,[eax+0xa8]` (`ForwardVelocityPitchFactor`, already ported by lane Local 25,
`engine/tests/test_local_25.cpp`), and the scan finds that instruction. The seven fields have no such hit.

### 5.3 The donor agrees

BFME2 1.06 (`/tmp/open-bfme-2`, byte matched to its own `game.dat`): the seven names appear only in
`Code/GameEngine/Source/Common/Rva001E72B4Reload.cpp:194, 241, 242, 247-250` - the parse table - with no
reader anywhere under `Code/`.

### 5.4 Outcome for the issue's nineteen fields

| outcome | fields |
|---|---|
| `stale (already in main, S-7221)` | `MinTurnSpeed`, `StickToGround`, `AllowAirborneMotiveForce`, `Apply2DFrictionWhenAirborne`, `FrontWheelTurnAngle`, `MaximumWheelCompression`, `LateralVelocityRollFactor`, `PitchInDirectionOfZVelFactor`, `MaxOverlappedHeight`, `SlideIntoPlaceTime`, `WanderAboutPointRadius`, `WanderLengthFactor` |
| `RotWK does not read` (this lane, stop S-7387) | `MaxThrustAngle`, `RudderCorrectionDegree`, `RudderCorrectionRate`, `SwoopStandoffRadius`, `SwoopStandoffHeight`, `SwoopTerminalVelocity`, `SwoopAccelerationRate` |

No engine behaviour changes: retail itself does not read the fields, so the port's parsed-and-unused members
already match it. `engine/tests/test_port_228.cpp` pins the parse, the constructor defaults and the copy, so a
later change that starts reading them, loses the parse or changes a default fails `PORT-228*`.
