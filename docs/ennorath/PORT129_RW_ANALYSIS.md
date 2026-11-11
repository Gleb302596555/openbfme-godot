# Port 129 (issue #331): `RespawnUpdate` / `DeathAnimationTime`

Question: what does RotWK 2.01 do with `RespawnUpdate/DeathAnimationTime` (how long a dead hero's death animation holds before he leaves the map)?

Every RW address below was read with `tools/mod/rw.py ... dis` or with a linear capstone sweep of the
whole `.text` section (RW 0x401000..0xBD0000, 2 701 800 instructions) of `game.dat` (md5 `daed3668...`). DONOR = Open-BFME-2 (BFME2 1.06 decompile).

## 1. The field

| what | where |
|---|---|
| parse table of `RespawnUpdateModuleData` | RW 0xC6CE80 (14 rows) |
| row `DeathAnimationTime` | RW 0xC6CEA0: parser RW 0x73A429 (`parseDurationUnsignedInt`), offset `+0xFC` |
| neighbours | `DeathFX` `+0xF0`, `RespawnFX` `+0xF4`, `InitialSpawnFX` `+0xF8`, `RespawnAnimationTime` `+0x100`, `InitialSpawnAnimationTime` `+0x104` |
| DONOR | `Code/GameEngine/Source/Common/ConstIntGettersAddr.cpp:690` (`{ "DeathAnimationTime", parseDurationUnsignedInt, 0, 0xFC }`), member `m_fieldFC` |

## 2. The class chain

* module factory row: RW 0x65A002 pushes the name `"RespawnUpdate"` (RW 0xC0B098) with the two create procedures RW 0x64F68C / RW 0x64F6C4
  (`addModule` RW 0x6570FE);
* RW 0x64F68C calls the module constructor RW 0x8B3244 (call at RW 0x64F6B3); RW 0x64F6C4 calls the module data constructor RW 0x8B3C54 (call at
  RW 0x64F6E8);
* the module data constructor RW 0x8B3C54 writes `+0xFC` once: RW 0x8B3CC4 `mov [esi+0xFC], ebx` (zero);
* the module's functions (the translation unit RW 0x8B307E..0x8B4271; virtual slots found through the data references RW 0xC6C354..0xC6C3A8, direct
  callers through a rel32 call scan): RW 0x8B307E, 0x8B310D, 0x8B316B, 0x8B319D, 0x8B3244, 0x8B32C4, 0x8B3309, 0x8B330F, 0x8B3313, 0x8B332D,
  0x8B3349, 0x8B3395, 0x8B33C2, 0x8B33E4, 0x8B34C3, 0x8B34EC, 0x8B3574, 0x8B35AC, 0x8B3637, 0x8B36B2, 0x8B3744, 0x8B38A2, 0x8B3A35, 0x8B3C07,
  0x8B3C1C, 0x8B3C31, 0x8B3C54, 0x8B3CE9, 0x8B3D05, 0x8B3D65, 0x8B3F72.

## 3. Who can reach the module data

The whole `.text` has 753 instructions with a `[reg + 0xFC]` operand (stack frames excluded). In the RespawnUpdate translation unit there is exactly
one: the constructor's zero write RW 0x8B3CC4. No other form of the offset (`add` / `lea` with 0xFC, a `lea` of a neighbour) exists in that unit.

Outside the unit the module is reached only by its name key. The string RW 0xC0B098 has ten references:

| reference | function | what it does with the module | `+0xFC` |
|---|---|---|---|
| RW 0x65A002 | module factory registration | registers the create procedures | no |
| RW 0x691806 | Object, RW 0x6917BB..0x69192F (key RW 0xDE477C) | calls RW 0x8B316B | no hit in the function |
| RW 0x780ED1 | hero list, RW 0x780EB6.. (key RW 0xDE7CE4) | calls RW 0x8B3C07 / 0x8B3C1C / 0x8B316B | no hit |
| RW 0x7815E7 | hero list (key RW 0xDE7CFC) | writes the module's `+0x41` (RW 0x781614) | no hit |
| RW 0x8A16B9 | ProductionUpdate slot RW 0x8A1666 (key RW 0xDE9594) | null test only (RW 0x8A16D9) | no hit |
| RW 0x8A1BCE | ProductionUpdate update RW 0x8A1B9F (key RW 0xDE95A8, used at RW 0x8A2BEB and RW 0x8A2DC6) | RW 0x8A2C07 calls revived RW 0x8B38A2; RW 0x8A2DDA null test | one hit, RW 0x8A22E7, on another object (below) |
| RW 0x8B32E9 | the module itself (RW 0x8B32C4, key RW 0xDE97BC) | own slot | no hit |
| RW 0x8B330A | the module's name getter RW 0x8B3309 | returns the string | no |
| RW 0x8C45DC | ActiveBody RW 0x8C3FA3..0x8C47DE (key RW 0xDE9B10) | null test only (RW 0x8C4604): an object without the module gets `Object::onDie` RW 0x698F06 here | no hit |
| RW 0x8C55FA | RespawnBody RW 0x8C553F..0x8C5689 (key RW 0xDE9BA8) | calls RW 0x8B3349 / RW 0x8B3744, writes the module's `+0x2C` | no hit |

Every key global is a function-local static: its references are only the initialisation and the lookups listed (byte scan of `.text` for each global).

RW 0x8A22E7 (`mov eax, [eax+0xFC]`): `eax` is the result of the virtual call RW 0x8A22DD (`[0xDE4830]` vtable + 0x12C), not the module data of
RespawnUpdate. It is another class with a member at the same offset.

The module data pointer is also passed as `this` to RW 0x8B3637 / RW 0x8B36B2 (the cost and the time of a level) from RW 0x6E2DF9 / 0x6E2E09 and
RW 0x6E2FB9 / 0x6E2FCA; those two functions (RW 0x6E2D92..0x6E2E18, RW 0x6E2E18..0x6E3000) contain no `[reg + 0xFC]` instruction.

## 4. What RotWK does when the hero dies (the path the field would have to be on)

RespawnBody RW 0x8C553F (a health change of `amount`, damage info):

* RW 0x8C5578..0x8C5586: lethal when `amount <= -health`;
* RW 0x8C5588: module data `+0x68` zero: the death is permanent; else RW 0x8C5598..0x8C55CA: permanent when the dealer passes the object filter at
  `+0x64` (RW 0x7640C1);
* RW 0x8C55DB: the base health change RW 0x8C31A5;
* RW 0x8C5618: the RespawnUpdate of the object (RW 0x68BDA5 by key);
* permanent (RW 0x8C566E): RW 0x8B3349 and return;
* lethal and not permanent (RW 0x8C562F..0x8C5667): status 0x3E set: RW 0x69ABA7; status 0x50 set: RW 0x8B3349 and the module's `+0x2C = 0`,
  else RW 0x8B3744 (below); then `Object::onDie` RW 0x698F06(damage info), which runs the object's die modules.

RespawnUpdate RW 0x8B3744 (the death before a respawn), only in state 0 or 4 (RW 0x8B374E..0x8B3761):

* RW 0x8B3767..0x8B37ED: the rule of the object's level (RW 0x603AF6), else the rule of level 1; without a rule: sleep forever (RW 0x850C32 with
  0x3FFFFFFF) and return;
* RW 0x8B37F2: the `DeathAnim` conditions (`+0x0C`) set on the object (RW 0x5E3BA5);
* RW 0x8B3800: `DeathFX` (`+0xF0`) played on the object (RW 0x4B1B5A);
* RW 0x8B3811: the in-game UI notified (RW 0x6A1CE2);
* RW 0x8B3824: disabled type 4 with no end frame (RW 0x692432); RW 0x8B382D: effectively dead (RW 0x68D950); RW 0x8B3836: status 3 set (RW 0x62684D);
* RW 0x8B3841..0x8B385C: `+0x40` = the rule is not AutoSpawn, `+0x38` = its time, `+0x3C` = its cost, `+0x20` = its health;
* RW 0x8B385F..0x8B3897: with a controlling player the hero list (player `+0x758`) gets the dead hero (RW 0x781792) and the record its level cap
  (`+0xD8`).

Neither function reads `+0xFC`, sets a wake frame from it or stores it: the revive record joins the hero list in the same call, and the object then
dies through `Object::onDie` (RW 0x698F06), so the time the body stays is the time of the object's own die modules.

## 5. RespawnUpdate update RW 0x8B3A35 (update interface RW 0xC6C354 slot 0), every branch

`this` is the update interface (module + 0x10): the state is `[ebx+0x1C]` (module `+0x2C`), the object `[ebx-8]`, the module data `[ebx-0xC]`.

* state 2, and every state other than 3 and 4 (RW 0x8B3A51 / RW 0x8B3A5F): return 0x3FFFFFFF (sleep forever, RW 0x8B3AFE);
* state 4 (RW 0x8B3A65..0x8B3AF9): the `RespawnAnim` (`+0x58`) and `InitialSpawnAnim` (`+0xA4`) conditions cleared (RW 0x5E3B79), status 3 cleared
  (RW 0x62684D), disabled type 4 cleared (RW 0x692443), state 0, the producer's exit interface (RW 0x68BB14) gets the object (slot `+8`,
  RW 0x8B3AEA) when its AI is idle (slot 0x1B8) or in state 16 (RW 0x660AC1); sleep forever;
* state 3 (RW 0x8B3B08..0x8B3C02): the respawn object (`+0x30`, else RW 0x8B307E), its rally point (exit interface slot 0x24), the object moved
  there (RW 0x697F0F), `RespawnAnim` set (RW 0x5E3BA5), `RespawnFX` (`+0xF4`, RW 0x4B1B5A), disabled type 4 until frame + `RespawnAnimationTime`
  (`+0x100`, RW 0x8B3B97 / RW 0x6907F1), state 4, the body's health (slot 0x58), `+0x34 = +0x38 = -1`, the local player's control bar marked
  (RW 0x8B3BEC); returns `RespawnAnimationTime` (RW 0x8B3BFC).

The update reads `+0x58`, `+0xA4`, `+0xF4` and `+0x100` of the data; never `+0xFC`. No state waits for a death time: the death (section 4) leaves the
module asleep and the state unchanged.

## 6. DONOR (BFME2 1.06, Open-BFME-2)

`git grep m_fieldFC -- Code`: `RespawnUpdate.cpp:206` (the member, `+0xFC`) and `RespawnUpdate.cpp:830` (the module data constructor 0x004AFB4E:
`m_fieldFC = 0`); the two other hits are another class (`LivingWorldLogicPlayerObserver`). `git grep DeathAnimationTime -- Code`: the parse row only
(`ConstIntGettersAddr.cpp:690`). `triggerDeathBeforeRespawn` (0x004AF63E) is RW 0x8B3744 line for line and `triggerRevival` (0x004AF79C) is RW 0x8B38A2;
neither reads the member. DONOR agrees: parse row and constructor are the whole access set.

## 7. Verdict

RotWK 2.01 parses `RespawnUpdate/DeathAnimationTime` into module data `+0xFC` and never reads it:

* the module data constructor zeroes it (RW 0x8B3CC4), the parser RW 0x73A429 writes it, nothing else in the image addresses it;
* the RespawnUpdate unit (RW 0x8B307E..0x8B4271) has no other access to `+0xFC`, in any addressing form;
* every function outside the unit that reaches the module (ten references to the name key, section 3) or its data (RW 0x6E2D92 / RW 0x6E2E18) was
  read; none reads `+0xFC` of it; the one `[reg+0xFC]` among them (RW 0x8A22E7) is on another object;
* the death path RespawnBody RW 0x8C553F -> RW 0x8B3744 -> `Object::onDie` RW 0x698F06 and the update RW 0x8B3A35 wait for nothing: the revive
  record joins the hero list in the frame of death, and the body stays as long as the object's die modules keep it.

The port agrees already: `HeroModules.cpp` parses the field into `m_deathAnimationTime` and no code reads it. Nothing to port; no getter, no member and
no test are added.
