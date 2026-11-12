# Port 129 (issue #331): `RespawnUpdate` / `DeathAnimationTime` has no reader in RotWK 2.01

Verdict: RotWK 2.01 parses the field into the module data at `+0xFC` and never reads it. Nothing waits for it: not the body, not the respawn clock,
not the revive button. The port already behaves the same (it parses the field and reads it nowhere), so nothing is ported: stop S-7323.

Method. Every RW address below was read with `tools/mod/rw.py <game.dat> dis` or taken from a linear capstone sweep of the whole `.text` section
(RW 0x401000..0xBD0000, 2 701 800 instructions) of `game.dat` (md5 `daed3668...`): every instruction with a `[reg + 0xFC]` operand (753, stack
frames excluded), every rel32 caller of each function, every data reference to a function (vtable slots), every reference to the name string and to
the name-key globals. DONOR = Open-BFME-2 (the BFME2 1.06 decompile), read for the mechanism only.

## 1. The field

| what | where |
|---|---|
| parse table of `RespawnUpdateModuleData` | RW 0xC6CE80 (14 rows) |
| row `DeathAnimationTime` | RW 0xC6CEA0: parser RW 0x73A429 (`parseDurationUnsignedInt`), offset `+0xFC` |
| neighbour rows | `DeathFX` `+0xF0`, `RespawnFX` `+0xF4`, `InitialSpawnFX` `+0xF8`, `RespawnAnimationTime` `+0x100`, `InitialSpawnAnimationTime` `+0x104` |
| DONOR | `Code/GameEngine/Source/Common/ConstIntGettersAddr.cpp:690` (`{ "DeathAnimationTime", parseDurationUnsignedInt, 0, 0xFC }`), member `m_fieldFC` |

## 2. The class chain

* module factory row: RW 0x65A002 pushes the name `"RespawnUpdate"` (RW 0xC0B098), RW 0x65A014 / RW 0x65A019 the two create procedures
  RW 0x64F6C4 / RW 0x64F68C, RW 0x65A027 calls the registration RW 0x6570FE;
* RW 0x64F68C calls the module constructor RW 0x8B3244 (call at RW 0x64F6B3); RW 0x64F6C4 calls the module data constructor RW 0x8B3C54 (call at
  RW 0x64F6E8);
* module constructor RW 0x8B3244: primary vtable RW 0xC6C360 (RW 0x8B3286), the inherited behaviour interface RW 0xC67300 at `+0xC`, the update
  interface RW 0xC6C354 at `+0x10`; it reads nothing of the data and sleeps forever (RW 0x8B32AE);
* primary vtable RW 0xC6C360, 16 slots: 0 RW 0x8B332D, 1 RW 0x8B88B0, 2 RW 0x8B3309 (the name), 3 RW 0x8B319D, 4 RW 0x8B32C4, 5..8 RW 0x63F3BF,
  9 RW 0x8851E4, 10 / 11 RW 0x851E97, 12 RW 0x64DFDA, 13 RW 0x8B310D, 14 RW 0x8B33E4 (the dead image), 15 RW 0x8B330F; update interface
  RW 0xC6C354: RW 0x8B3A35 (update), RW 0x8B3313, RW 0x850C4E. The slots outside the unit are shared bodies: RW 0x8B88B0 (a jump to
  RW 0x4D0EAF, itself a jump to RW 0x63F3BF), RW 0x63F3BF (`ret`), RW 0x8851E4 (`ret 8`), RW 0x851E97 (returns null), RW 0x64DFDA (returns 2),
  RW 0x850C4E (the interface's thunk to the wake-frame setter RW 0x850C32); none addresses `+0xFC`;
* module data constructor RW 0x8B3C54: vtable RW 0xC6C3A8 (RW 0x8B3C6E); it zeroes `+0xF0` (RW 0x8B3CB2), `+0xF4` (RW 0x8B3CB8), `+0xF8`
  (RW 0x8B3CBE), **`+0xFC` (RW 0x8B3CC4)**, `+0x100` (RW 0x8B3CCA), `+0x104` (RW 0x8B3CD0);
* module data vtable RW 0xC6C3A8, 31 slots: 0 RW 0x8B3CE9 (destructor), 13 (`+0x34`) and 30 RW 0x8BD372 (`mov al, 1; ret`), 14 RW 0x4B612F,
  28 (`+0x70`) RW 0x8CEF91 (`mov eax, ecx; ret`); the others are the shared stubs RW 0x63F3BF (`ret`), RW 0x464A09, RW 0x9F3A3C (`ret 4`),
  RW 0x9188EB (`xor al, al; ret`) and RW 0x851E97 (`xor eax, eax; ret`).

## 3. The access set of data `+0xFC`

### 3.1 Inside the RespawnUpdate unit (RW 0x8B307E..0x8B4271)

Functions (virtual slots by data reference, the rest by their callers): RW 0x8B307E, 0x8B310D, 0x8B316B, 0x8B319D, 0x8B3244, 0x8B32C4, 0x8B3309,
0x8B330F, 0x8B3313, 0x8B332D, 0x8B3349, 0x8B3395, 0x8B33C2, 0x8B33E4, 0x8B34C3, 0x8B34EC, 0x8B3574, 0x8B35AC, 0x8B3637, 0x8B36B2, 0x8B3744, 0x8B38A2,
0x8B3A35, 0x8B3C07, 0x8B3C1C, 0x8B3C31, 0x8B3C54, 0x8B3CE9, 0x8B3D05, 0x8B3D65, 0x8B3F72.

The unit holds exactly one instruction that addresses `+0xFC`: the constructor's zero write RW 0x8B3CC4. No `add` / `lea` with 0xFC and no `lea` of
a neighbour offset (0xF0..0x104) exists in it. Control: the same listing shows the readers of every neighbour row: `+0xF0` RW 0x8B3806; `+0xF4`
RW 0x8B393C and RW 0x8B3B84; `+0xF8` RW 0x8B3907; `+0x100` RW 0x8B3933, RW 0x8B3B97 and RW 0x8B3BFC; `+0x104` RW 0x8B38FE.

### 3.2 Outside the unit, the module by its name key

The module is found by `Object` RW 0x68BDA5 with a name key made from the string RW 0xC0B098. The string has ten references:

| reference | function | what it does with the module | `+0xFC` |
|---|---|---|---|
| RW 0x65A002 | module factory registration | registers the create procedures | no |
| RW 0x691806 | `Object`, RW 0x6917BB (key RW 0xDE477C) | RW 0x691830 calls RW 0x8B316B (the respawn template), copies that template's `+0x64` (RW 0x691844) | none in the function |
| RW 0x780ED1 | hero record of a dying object, RW 0x780DD4 (key RW 0xDE7CE4) | slot 14 (RW 0x780F00), RW 0x8B3C07 cost (RW 0x780F08), RW 0x8B3C1C time (RW 0x780F16), RW 0x8B316B (RW 0x780F67) | none |
| RW 0x7815E7 | hero list (key RW 0xDE7CFC) | writes the module's `+0x41` (RW 0x781614) | none |
| RW 0x8A16B9 | ProductionUpdate, RW 0x8A1666 (key RW 0xDE9594) | null test only (RW 0x8A16D9): without the module, slot `+8` of the interface in `edi` gets the object (RW 0x8A16E4) | none |
| RW 0x8A1BCE | ProductionUpdate update, RW 0x8A1B9F (key RW 0xDE95A8, used at RW 0x8A2BEB and RW 0x8A2DC6) | RW 0x8A2C07 calls RW 0x8B38A2 (revived); RW 0x8A2DDA null test | RW 0x8A22E7, on another object (below) |
| RW 0x8B32E9 | the module itself, RW 0x8B32C4 (key RW 0xDE97BC) | own slot 4 | none |
| RW 0x8B3309 | the module's name getter (primary slot 2) | returns the string | no |
| RW 0x8C45DC | ActiveBody, RW 0x8C3FA3 (key RW 0xDE9B10) | null test only (RW 0x8C4604): an object without the module gets `Object::onDie` RW 0x698F06 here (RW 0x8C4645) | none |
| RW 0x8C55FA | RespawnBody, RW 0x8C553F (key RW 0xDE9BA8) | calls RW 0x8B3349 / RW 0x8B3744, writes the module's `+0x2C` (section 4) | none |

Each key global is a function-local static: a byte scan of `.text` finds it only at its initialisation and at the lookups listed.

RW 0x8A22E7 (`mov eax, [eax+0xFC]`): `eax` is the result of the virtual call RW 0x8A22DD (the object at `[0xDE4830]`, vtable `+0x12C`), not the
module data of RespawnUpdate: another class with a member at the same offset.

### 3.3 Outside the unit, the module data by its typed accessor

The module data vtable has a typed accessor at slot 28 (`+0x70`): RW 0x8CEF91 returns `this`. Among the 380 vtables of the image that share the
bodies of slots 1 and 3 (RW 0x63F3BF, RW 0x9F3A3C), RW 0xC6C3A8 is the only one with RW 0x8CEF91 in slot 28; 215 of them have the null body
RW 0x851E97 there. Two functions call it; both fill a hero record from a template (in the port: the Living World's dead hero records, `HeroSystem.cpp`):

* RW 0x6E2D92: the module data of each template entry (RW 0x73C667 at RW 0x6E2DC3), slot 13 (RW 0x6E2DD2), slot 28 (RW 0x6E2DEF); the result goes
  to RW 0x8B3637 (the cost of a level, RW 0x6E2DF9) and RW 0x8B36B2 (its time, RW 0x6E2E09) and nowhere else;
* RW 0x6E2E18: the same lookup (RW 0x6E2F34 / 0x6E2F47 / 0x6E2F56), kept in the frame (RW 0x6E2F62); it reads `+0x118` (`ButtonImage`,
  RW 0x6E2F82) and calls RW 0x8B3637 (RW 0x6E2FB9) and RW 0x8B36B2 (RW 0x6E2FCA).

In both functions the pointer has no other use: RW 0x6E2D92 keeps it in `esi` for the two calls and returns (RW 0x6E2E17); RW 0x6E2E18 keeps it
in the frame (`[ebp+0x10]`) for the two calls and does not use it after the loop (RW 0x6E2FE9..0x6E301E).

Cross-checks over the whole image (mechanical, by script):

* of the 44 calls of RW 0x73C667 only these two are followed by a `call [reg+0x70]`;
* the sweep's trace of the base register of each of the 753 `[reg + 0xFC]` instructions back to the instruction that defines it finds none defined
  by the result of a `call [reg+0x70]`;
* seven functions hold both a `call [reg+0x70]` and a `[reg + 0xFC]` instruction (RW 0x647ABF, 0x65ED04, 0x66A7EC, 0x71EBDA, 0x79EF96, 0x94397F,
  0x9B6D01; eight instructions). None of the eight takes its base from that call: RW 0x65ED56 and RW 0x66AE56 are virtual calls through `+0xFC`,
  RW 0x79EFB5 and RW 0x9B6ED0 are `lea` of a member of `edi`, RW 0x647BB2 uses the result of a `call [reg+0x8C]`, RW 0x9439A0 the member
  `[edi+0x6C]`, RW 0x71EC71 and RW 0x71EDB0 the `+8` member of a list entry or `+0x10` of the global RW 0xDE4928 (in RW 0x71EBDA the slot `+0x70`
  call RW 0x71ED16 returns a count that is compared with zero).

### 3.4 The other `[reg + 0xFC]` instructions of the shape `[x+4]`, then `+0xFC`

Twenty instructions outside the unit read `[reg + 0xFC]` from a register loaded by `mov reg, [x+4]`: RW 0x470208, 0x47031B, 0x4A0640, 0x50803E,
0x5BB9F9, 0x69C3D4, 0x741270, 0x74844B, 0x75CD01, 0x75CDFC, 0x75D251, 0x75D285, 0x81DC08, 0x88416A, 0x8DD75D, 0x8DE9A4, 0x8DF319, 0x8E2D80,
0x8E33CC, 0x8F3D5E. `[x+4]` is the module data of every module class and the template of every object (RW 0x780F21 `mov eax, [edi+4]`, then the
template's `+0x11F`), so the shape names no class. None of the twenty lies in a function of 3.2 or 3.3, the functions outside the unit that this
analysis found to hold a pointer typed as this module (name key) or as its data (slot 28): they read other classes.

## 4. What RotWK does when the hero dies

RespawnBody RW 0x8C553F (a health change `amount`, the damage info):

* RW 0x8C5578..0x8C5586: lethal when `amount <= -health`;
* RW 0x8C5588: module data `+0x68` zero: the death is permanent; else RW 0x8C5598..0x8C55CA: permanent when the dealer passes the object filter at
  `+0x64` (RW 0x7640C1);
* RW 0x8C55DB: the base health change RW 0x8C31A5;
* RW 0x8C5618: the RespawnUpdate of the object;
* permanent (RW 0x8C566E): RW 0x8B3349 and return;
* lethal and not permanent (RW 0x8C562F..0x8C5667): status 0x3E set: RW 0x69ABA7; status 0x50 set: RW 0x8B3349 and the module's `+0x2C = 0`,
  else RW 0x8B3744 (below); then `Object::onDie` RW 0x698F06 (the damage info), which runs the object's die modules.

RespawnUpdate RW 0x8B3744 (the death before a respawn), only in state 0 or 4 (RW 0x8B374E..0x8B3761):

* RW 0x8B3767..0x8B37ED: the rule of the object's level (RW 0x603AF6), else the rule of level 1; without a rule: sleep forever (RW 0x850C32 with
  0x3FFFFFFF) and return;
* RW 0x8B37F2: the `DeathAnim` conditions (`+0x0C`) set on the object (RW 0x5E3BA5);
* RW 0x8B3800: `DeathFX` (`+0xF0`) played on the object (RW 0x4B1B5A);
* RW 0x8B3811: the in-game UI notified (RW 0x6A1CE2);
* RW 0x8B3824: disabled type 4 with no end frame (RW 0x692432); RW 0x8B382D: effectively dead (RW 0x68D950); RW 0x8B3836: status 3 set
  (RW 0x62684D);
* RW 0x8B3841..0x8B385C: `+0x40` = the rule is not AutoSpawn, `+0x38` = its time, `+0x3C` = its cost, `+0x20` = its health;
* RW 0x8B385F..0x8B3897: with a controlling player the hero list (player `+0x758`) gets the dead hero (RW 0x781792) and the record its level cap
  (`+0xD8`).

Neither function reads `+0xFC`, sets a wake frame from it or stores it. The revive record joins the hero list in the frame of death, and the object
goes to `Object::onDie` in the same call. What holds the body after that is `Object::onDie` and the object's die modules; they were not read
here, and they cannot reach this field (section 3).

## 5. RespawnUpdate update RW 0x8B3A35 (update interface RW 0xC6C354 slot 0), every branch

`this` is the update interface (module `+0x10`): the state is `[ebx+0x1C]` (module `+0x2C`), the object `[ebx-8]`, the module data `[ebx-0xC]`.

* state 2, and every state other than 3 and 4 (RW 0x8B3A51 / RW 0x8B3A5F): return 0x3FFFFFFF (sleep forever, RW 0x8B3AFE);
* state 4 (RW 0x8B3A65..0x8B3AF9): the `RespawnAnim` (`+0x58`) and `InitialSpawnAnim` (`+0xA4`) conditions cleared (RW 0x5E3B79), status 3 cleared
  (RW 0x62684D), disabled type 4 cleared (RW 0x692443), state 0; then, for the object found by the id at this object's `+0x78` (RW 0x449681), when
  this object's AI (`+0x260`) answers true through slot 0x1B8 or RW 0x660AC1 returns 16, the interface RW 0x68BB14 of the found object gets this
  object (slot `+8`, RW 0x8B3AEA) and, with this object's `+0x47C` set, RW 0x625E0A runs (RW 0x8B3AF9); sleep forever;
* state 3 (RW 0x8B3B08..0x8B3C02): the object with the id at the module's `+0x30` (RW 0x449681), else the one RW 0x8B307E finds; with one, its
  position and the point its interface RW 0x68BB14 gives through slot 0x24 (RW 0x8B3B62); this object moved there (RW 0x697F0F), `RespawnAnim` set
  (RW 0x5E3BA5), `RespawnFX` (`+0xF4`, RW 0x4B1B5A), disabled type 4 until frame + `RespawnAnimationTime` (`+0x100`, RW 0x8B3B97 / RW 0x6907F1),
  state 4, the body's slot 0x58 with the module's `+0x20` times the constant RW 0xBD88D8 (RW 0x8B3BCE), `+0x34 = +0x38 = -1`, for the local
  player's object the flag `+0x28` of the global RW 0xDE7744 (RW 0x8B3BEC); returns `RespawnAnimationTime` (RW 0x8B3BFC).

The update reads `+0x58`, `+0xA4`, `+0xF4` and `+0x100` of the data; never `+0xFC`. No state waits for a death time, and RW 0x8B3744 (section 4)
sets no wake frame from the field.

## 6. DONOR (BFME2 1.06, Open-BFME-2)

`git grep m_fieldFC -- Code`: `RespawnUpdate.cpp:206` (the member, `+0xFC`) and `RespawnUpdate.cpp:830` (the module data constructor 0x004AFB4E:
`m_fieldFC = 0`); the two other hits are another class (`LivingWorldLogicPlayerObserver`). `git grep DeathAnimationTime -- Code`: the parse row only
(`ConstIntGettersAddr.cpp:690`). `triggerDeathBeforeRespawn` (0x004AF63E) is RW 0x8B3744 step for step and `triggerRevival` (0x004AF79C) is
RW 0x8B38A2; neither reads the member. DONOR agrees: parse row and constructor are the whole access set.

## 7. The port

`HeroModules.cpp` parses the field into `RespawnUpdateModuleData::m_deathAnimationTime` and no code reads it. `RespawnUpdate::onDeath` (RW 0x8B3744)
takes the same steps and sets no wake frame; RespawnBody (RW 0x8C553F, `StructureModules.cpp`) then calls `Object::onDie`. Nothing to port: no getter,
no member and no test are added (issue #331: no test for a field nobody reads). The death and revive path the field would sit on is held by
`LOCAL-8*` (`test_local_8.cpp`, its synthetic hero carries a `DeathAnimationTime`) and `hero death and revive*` (`test_hero_core.cpp`).

## 8. Limits

INFERENCE: a static read cannot exclude an access whose address is computed at run time (an indexed read with a computed offset) or code outside
the image. The mechanical cross-checks of 3.3 rest on the function starts of the call-target scan and on a register trace inside one function; the
699 `call [reg+0x70]` sites of the image were not typed one by one.
