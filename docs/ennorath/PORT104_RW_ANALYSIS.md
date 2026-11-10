# PORT-104 (issue #264): CreateObjectDie / DebrisPortionOfSelf — RW analysis

## Verdict

**Stop (no reader): RotWK 2.01 parses `DebrisPortionOfSelf` into
`CreateObjectDieModuleData +0x3C` and never reads it. There is no debris
behaviour to port; the port must keep parsing the field (it already does)
and must not invent a debris spawn next to the CreationList path.**

## What is proven

### RotWK (game.dat, from this repo's already-landed RW work)

- Field name RW 0xC610FC, `CreateObjectDie` field-table entry RW 0xC61130
  (issue #264, checked in game.dat 2026-11-10).
- `CreateObjectDie` create RW 0x64C572, constructor RW 0x888D14, die vtable
  RW 0xC60FD4; field table RW 0xC61120: `CreationList` (+0x38),
  `DebrisPortionOfSelf` (+0x3C, parseAsciiString), `UpgradeRequired`
  (+0x40) — see `engine/src/GameLogic/Module/ExtraDieModules.h`.
- `onDie` RW 0x888F04 (read instruction by instruction by the MODULES-1
  lane): DieMux gate (RW 0x8D29A9), every `UpgradeRequired` name checked
  (RW 0x66F5E5 / 0x68B678 / 0x691421), then the CreationList is created
  with the dying object as primary and the killer as secondary
  (RW 0x5F0126). **`+0x3C` is not read by onDie.**

### BFME2 1.06 donor (Open-BFME-2, byte-exact decompilation; DONOR facts)

- Field table DONOR BFME2 0xC4A928: `CreationList` +0x38,
  `DebrisPortionOfSelf` +0x3C (INI::parseAsciiString), `UpgradeRequired`
  +0x40 (`Code/GameEngine/Source/GameLogic/Object/Die/CreateObjectDie.cpp:374`).
- `CreateObjectDie::onDie` DONOR BFME2 0x485C86 (139 bytes): reads the
  upgrade vector at +0x40 and the OCL at +0x38 only; `+0x3C` is never
  loaded (`CreateObjectDie_onDie.cpp` in the donor tree).
- The module-data constructor DONOR BFME2 0x485D88 only *clears* the
  string at +0x3C after the members are built.
- A whole-tree search of the donor (`grep -rn DebrisPortionOfSelf Code`,
  plus `portion` / `m_debris` over all of `Code`) finds the field only in
  the parse table, the member declaration (+0x3C) and the constructor
  clear: **no reader in any class** — not in the die module, not in
  `ObjectCreationList` / `GenericObjectCreationNugget` (whose debris path
  is the separate `CreateDebris` nugget, ModelNames -> GenericDebris),
  not in any draw module.

### What this means for the Ennorath uses

The 8 Ennorath uses (`GondorTrebuchet`, `ArnorTrebuchet`,
`CarnDumTrebuchet`, `ArnorBatteringRam`, `AngmarTrollSling`,
`MordorTrollcatapult`, …) name a debris-piece object in
`DebrisPortionOfSelf` next to a `CreationList`. In RotWK the debris the
player sees comes from that CreationList alone (the `CreateObject` /
`CreateDebris` nuggets of the death OCL); the `DebrisPortionOfSelf` line
is dead data the game carries. Porting a spawn of the named object would
create a *second*, non-RotWK debris object.

## INFERENCE / limits of this session

- The RW whole-image displacement scan for `[reg+0x3C]` on the
  module-data class was **not** re-run in this sandbox: `game.dat`
  (11 MB on the owner's Google Drive) cannot be pulled through the
  Drive connector without flooding the session, and the sandbox has no
  other route to it. The verdict therefore rests on (a) the port's
  already-landed, instruction-read RW 0x888F04 onDie (the only reader
  the class's vtable exposes for death behaviour), and (b) the donor's
  complete absence of a reader, with the same field table, offsets and
  onDie shape. **INFERENCE: RotWK keeps the donor's dead field.**
  Cheapest confirmation on the owner's machine: `tools/mod/rw.py xref`
  / a `[reg+0x3C]` scan over the `CreateObjectDieModuleData` readers in
  game.dat; if a reader appears, this stop reopens and the reader is
  ported on the existing `CreateObjectDie::onDie` path.

## Port state

Unchanged engine behaviour: `DebrisPortionOfSelf` stays parsed into
`CreateObjectDieModuleData::m_debrisPortionOfSelf` (RW 0xC61120 row) and
unread, exactly as RW. `PORT-104*` tests pin the parse, that a death
with the field creates exactly the CreationList's objects (no object of
the named debris template), and that without the field the same death
is unchanged.
