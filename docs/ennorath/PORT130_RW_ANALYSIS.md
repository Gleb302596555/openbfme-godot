# PORT-130: CommandButton `AffectsKindOf` — which objects a command button applies to

Issue #332. Image: RotWK 2.01 `game.dat`, md5 daed3668006cd90f01c34e5a7da1901f
(caveat S-001). Disassembly read with `tools/mod/rw.py workspace/RotWK/game.dat dis`.
Donor: BFME2 1.06 decompilation `/tmp/open-bfme-2/Code` (DONOR BFME2 facts, never
the source of truth). Mod data: `ennorath-ini-1.9.8.2.zip`
(`data/ini/commandbutton.ini`), field audit v7 `field-audit.tsv` row 573.

## 1. Where the field lives

* Parse row RW 0xC2BD28 (`AffectsKindOf`, parser RW 0x6564E7, `KindOf` bit mask,
  CommandButton + 0x110). Table RW 0xC2BAC8 (55 rows). The port already has the row
  in `ControlBarCommands.cpp` and the member `CommandButton::m_affectsKindOf`
  (`+0x110`, `KindOfMaskType`, 7 words = 28 bytes).
* Constructor RW 0x75D516 installs vtable RW 0xC2C890; RW 0x75D721 `push 0x1C`,
  RW 0x75D723 `lea eax,[esi+0x110]`, RW 0x75D731 `call 0xA3CF28` zeroes the whole
  28-byte mask. DONOR BFME2 0x75D516 (`CommandButton.h:133`,
  `CommandButton.cpp`/`Rva0035BB4BCtor.cpp:90 memset(&affectsKindOf,0,28)`) has the
  same member and size.
* Not the module row: RW 0xC676A0 / parser RW 0x89ECD7 only prints
  "AffectsKindOf is obsolete" (`HeroModules.cpp:126` already does that).

## 2. The reader (RotWK, `ControlBar::getCommandAvailability` RW 0x942733)

Read linearly from RW 0x942733 (the function the port's `ControlBar::evaluate`
models, see `docs/re/rotwk/RW_0x942733.md`). The `NEEDS_CASTLE_KINDOF` option is
bit 11 = 0x800 of `Options` at CommandButton + 0x1C (name index 11 in the port's
`kOptionNames`, RW 0xDA4C88).

| RW | instruction | meaning |
|---|---|---|
| 0x942993 | `mov eax,[edi+0x1c]` | the button's Options |
| 0x942996 | `test ah,8` | bit 11 (0x800) = NEEDS_CASTLE_KINDOF |
| 0x942999 | `je 0x942a24` | without the option the whole filter is skipped (allowed) |
| 0x9429A0 | `call 0x797bef` | find `CastleMemberBehavior` on the selected object (name at RW 0xC0BE84) |
| 0x9429A8 | `je 0x942a24` | no member module: skipped (allowed) |
| 0x9429AA | `push [eax+0x18]` | the member's castle id (module + 0x18) |
| 0x9429B3 | `call 0x449681` | `GameLogic::findObjectByID` |
| 0x9429BC | `je 0x942c5d` | castle object missing -> availability 3 |
| 0x9429C2 | `call 0x797ac1` | the `CastleBehavior` module name |
| 0x9429CA | `call 0x68bda5` | `Object::findModule` on the castle object |
| 0x9429D4 | `je 0x942a24` | no CastleBehavior on it: skipped (allowed) |
| 0x9429D6 | `push 0x1c`; 0x9429DE `call 0xa3cf28` | zero the filter's 28-byte mask |
| 0x9429E9 | `add eax,0x110` | **the button's `AffectsKindOf` mask** |
| 0x9429F6 | `call 0x444d65` | RW 0x444D65 copies 0x1C bytes (RW 0xA3CF22) into the filter |
| 0x942A08 | `rep movsd` (7 dwords, 0x9429FB `push 7`) | the 28-byte mask copied by value |
| 0x942A0D | `push 0x942348` | the per-object callback |
| 0x942A12 | `call 0x7999e2` | `CastleBehavior::apply` over the castle's objects |
| 0x942A17 | `cmp byte ptr [ebp-0x4c],0` | the filter's `matched` byte |
| 0x942A1B | `je 0x942c5d` | no object matched -> availability 3 |
| 0x942C5D | `push 3; pop eax` | **COMMAND_HIDDEN (3)** |
| 0x942348 | `call 0x70c548` | the callback: `isAnyKindOf(object, mask)` |
| 0x94235B | `mov byte ptr [esi+0x1c],1` | matched = true; return 0 stops the iteration |
| 0x942363 | `xor eax,eax; inc eax` | not matched; return 1 continues |
| 0x70C548 | `mov eax,[ecx+4]; add eax,0x108` | the object's ThingTemplate KindOf mask |
| 0x70C555 | `call 0x661359` | RW 0x661359: `test [ecx],esi` over 7 dwords -> ANY intersection |

RW 0x7999E2 (the iteration) calls the callback on the castle object itself
(RW 0x7999EC `mov eax,[esi+8]`), on the keep (RW 0x7999FA `lea ecx,[esi+0x20]`,
vtable slot 0x18), then on the four ID lists RW +0x74 (RW 0x799A19),
+0x50 (RW 0x799A42), +0x68 (RW 0x799A6F), +0x5C (RW 0x799A98); each ID is
resolved with RW 0x449681 (RW 0x799A26/0x799A53/0x799A7C/0x799AA5), a null
object is skipped, and a callback returning 0 ends the walk (RW 0x799A38 /
0x799AAB). The port's `CastleBehavior::forEachMemberId` (keep + foundations +
members + crew + defenseFoundations + the castle object) is the same set.

DONOR BFME2 0x53B946 (`Code/GameEngine/Source/GameClient/GUI/ControlBar/ControlBarCommand.cpp:446`)
is the callback `Rva0053B946`: `((Thing*)object)->isAnyKindOf(filter->mask)`;
DONOR BFME2 `ControlBarRappellerCount.cpp:525` `if (command->getOptions() & 0x800)`
is the same branch, `:536..539` builds `CommandCastleKindFilter` with
`getAffectsKindOf(command)` (`:362`) and applies it to `CastleBehavior`;
`if (!filter.matched) return COMMAND_HIDDEN`. Donor and RW agree on every step
above; RW's extra `findObjectByID` null branch (0x9429BC) and the exact
iteration order are RW-only and were taken from RW.

**Verdict:** RotWK reads the field. It is an ANY-over-KindOf filter over the
objects owned by the castle of the selected object's `CastleMemberBehavior`,
gated by `Options = NEEDS_CASTLE_KINDOF` (0x800). With no match the whole
button is HIDDEN (availability 3), not merely greyed. It is a client
availability check, not a cursor/target validity check and not a build
placement filter (triages' INFERENCE in `arena-triage/batch-5.md` section F
about "enabling/disabling buttons" is a different, unverified path: see 4).

## 3. Ennorath data (v7 audit: 20 buttons)

`Command_ConstructGondorStoneMaker`, `Command_ConstructArnorStoneMaker` and
`Command_PorterConstructMenStoneMaker` use
`AffectsKindOf = WALK_ON_TOP_OF_WALL DEFENSIVE_WALL`; the two `*_StoneMaker`
buttons also carry `Options = NEEDS_CASTLE_KINDOF`.
`Command_SpecialAbilityMordorBalrogWhipAI` uses
`AffectsKindOf = STRUCTURE HERO MONSTER PORTER` but **no** NEEDS_CASTLE_KINDOF:
for it RotWK skips the filter (RW 0x942999), so this button is not affected by
this port. The other 15 `STRUCTURE` buttons are the castle-kindof ones.

## 4. Not ported (stop S-7319), and INFERENCE

The other instructions that touch a +0x110 dword in the ControlBar code region
(RW 0x7490B6/0x7490BE, RW 0x74D189..0x74D216, RW 0x74DF09/0x74DF11,
RW 0x7506D7/0x7506DF, RW 0x7514FE..0x751591) are read-modify-write of the
first word (and, at RW 0x7506F1, the last word +0x128) of a 28-byte mask,
followed by RW 0x68B53C; RW 0x7514F7 compares a value with 0xE4. They are a
runtime button-alteration path, not the filter reader. Their owner class was
not established by a create -> constructor -> vtable -> slot chain in this
session, so no behaviour was inferred from them and no second path was added.
INFERENCE (not ported): they belong to `CommandButton` (the constructor at
RW 0x75D516 owns the +0x110 mask); the value 0xE4 is a command/button id.
The port keeps the parsed mask untouched; runtime mutation of `AffectsKindOf`
remains a stop.

## 5. What PORT-130 changes

One branch in the existing `ControlBar::evaluate` (RW 0x942993..0x942A1B),
factored into the public predicate `ControlBar::castleKindOfAllows` so the
tests can drive it: option gate, member module, castle object, CastleBehavior,
the `forEachMemberId` walk with a 7-dword ANY intersection (RW 0x661359), and
HIDDEN on no match (RW 0x942C5D). No new state is added: no `xfer`, no hash,
no CRC entry, no new message or handler. The parsed member already existed.
