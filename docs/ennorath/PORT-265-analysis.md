# PORT-265 — HordeContain own functions (working notes)

Fork-only working notes for issue 613 (branch `arena/01de5908-openbfme-godot`). Not part of the
upstream change; the PR body carries the table.

## The field table of HordeContainModuleData (RW 0x00C5BB50, 43 rows)

Read with `rw.py` / pefile: entry = { name ptr, parse proc, 0, offset } (16 bytes). The offsets are the
RotWK ones, `HordeContain.h` in the port already uses them.

| # | offset | parse proc | field |
|---|---|---|---|
| 0 | 0x1D8 | 0x0042E558 | ThisFormationIsTheMainFormation |
| 1 | 0x18C | 0x00877385 | RankInfo |
| 2 | 0x1B4 | 0x0086DF0B | RanksThatStopAdvance |
| 3 | 0x1B8 | 0x0086DED1 | RanksToReleaseWhenAttacking |
| 4 | 0x1C4 | 0x0086DED1 | RanksToJustFreeWhenAttacking |
| 5 | 0x198 | 0x00872654 | ComboHorde |
| 6 | 0x1B0 | 0x0042EE5E | AlternateFormation |
| 7 | 0x1D0 | 0x0042F298 | RandomOffset |
| 8 | 0x200 | 0x0042F247 | LeaderPosition |
| 9 | 0x1F4 | 0x0042EED6 | LeadersAllowed |
| 10 | 0x20C | 0x0087242F | BannerCarrierPosition |
| 11 | 0x218 | 0x0042EED6 | BannerCarriersAllowed |
| 12 | 0x224 | 0x0042E558 | BannerCarrierDestroyHordeOnDeath |
| 13 | 0x228 | 0x0073A68A | BannerCarrierHordeDeathType |
| 14 | 0x208 | 0x0042EC5E | LeaderRank |
| 15 | 0x1DC | 0x0073A429 | BackUpMinDelayTime |
| 16 | 0x1E0 | 0x0073A429 | BackUpMaxDelayTime |
| 17 | 0x1E4 | 0x0042ED00 | BackUpMinDistance |
| 18 | 0x1E8 | 0x0042ED00 | BackUpMaxDistance |
| 19 | 0x1EC | 0x0042EEFA | BackupPercentage |
| 20 | 0x1F0 | 0x0042ED00 | CowerRadius |
| 21 | 0x22C | 0x0042EED6 | AttributeModifiers |
| 22 | 0x238 | 0x0042E558 | IsPorcupineFormation |
| 23 | 0x23C | 0x0042E956 | ForcedLocomotorSet |
| 24 | 0x240 | 0x0042E558 | MachineAllowed |
| 25 | 0x244 | 0x0042EE5E | MachineType |
| 26 | 0x248 | 0x0042E558 | UseSlowHordeMovement |
| 27 | 0x1A4 | 0x0087253E | SplitHorde |
| 28 | 0x24C | 0x0042ED00 | MeleeAttackLeashDistance |
| 29 | 0x250 | 0x005DE588 | EvaEventLastMemberDeath |
| 30 | 0x254 | 0x0042E558 | RankSplit |
| 31 | 0x258 | 0x0042EC5E | SplitHordeNumber |
| 32 | 0x25C | 0x0042E558 | NotComboFormation |
| 33 | 0x25D | 0x0042E558 | UseMarchingAnims |
| 34 | 0x264 | 0x0042ED00 | FrontAngle |
| 35 | 0x268 | 0x0073A429 | FlankedDelay |
| 36 | 0x26C | 0x0073A429 | FlankedDuration |
| 37 | 0x260 | 0x0086C30A | MeleeBehavior |
| 38 | 0x270 | 0x0042ECB2 | MinimumHordeSize |
| 39 | 0x274 | 0x0042EEFA | VisionRearOverride |
| 40 | 0x278 | 0x0042EEFA | VisionSideOverride |
| 41 | 0x27C | 0x0042EB75 | BannerCarrierMinLevel |
| 42 | 0x280 | 0x0042EE5E | LivingWorldOverloadTemplate |

Caution: object offsets in the same functions (0x258 contain module on the **Object**, 0x260 the
member's AI, 0x26C, 0x27C) collide with module-data offsets. The module data is `[interface + 4]`,
the object `[interface + 8]`; the contain interface is `this + 0x11C`.

## Found readers among the 65 uncited own functions

### RW 0x0086DF44 — the split check, reads SplitHordeNumber (+0x258) and EvaEventLastMemberDeath (+0x250)

- `0x86DF4E..0x86DF75`: `isKindOf(obj, 2)` (0x44DDEC) and an empty contain list (0x865598) and
  `[interface+0x174] == 0` → `Eva(...)` 0x5DD9EE with `[mdata+0x250]` (0x86DF99), then 0x62BBAB(obj);
  returns false.
- otherwise `0x86DFB5`: `vcall +0x1FC` (the split-allowed test), `[[obj+0x26C]+0x24] <= 1`,
  count of the contain list (0x5FFE1E / `vcall +0x10C`), then
  `0x86E013: cmp [mdata+0x258], 0` / `0x86E01C: cmp count, [mdata+0x258]` — **the SplitHordeNumber
  reader**; with 0, every member must be the same template (0x73D5C2 isEquivalentTo) and the list
  must not be empty. Then `vcall +0x1F8` with an ObjectID out, `0x430170(id)` when it is set.
- present in three vtables: 0x00C5B674, 0x00C5C304, 0x00C5D46C.

## Not yet classified

The remaining uncited functions were scanned linearly (length, calls, displacements) — the scan
output is the basis of the PR table; the tiny ones are object-field getters (0x872A69, 0x872A8D,
0x872A9A, 0x872AAE, 0x872ACB, 0x872AD2, 0x872AE7, 0x872AEE, 0x872AF5, 0x872B02, 0x86C9E7,
0x86C9F3, 0x872B76, 0x8736EF, 0x87737A, 0x872B24).

## RW 0x0086DF44, branch by branch (the contain interface `this + 0x11C`, vtable RW 0x00C5B1F8)

Interface layout used here: `[this+4]` = module data, `[this+8]` = the Object; the contain list is
RW 0x865598; `0x44DDEC(obj, bit)` is `Object::testStatus(bit)` (mask at Object + 0x94, bit index
shifted by 5); `0x5DD9EE(ecx = TheEva RW 0xDE3670)` is `reportEvent(event, position, null)`;
`0x62BBAB(ecx = TheGameLogic RW 0xDE412C, obj)` is `destroyObject`; `0x73D5C2` is
`ThingTemplate::isEquivalentTo`.

Two slots of the same vtable, resolved from RW 0x00C5B1F8:

| slot | address | what the port calls it |
|---|---|---|
| +0x1F8 | 0x008791C5 | `HordeContain::splitHorde` (ported, `RW 0x8791C5`) |
| +0x1FC | 0x0086C9CD | `HordeContain::canSplit` (ported, `RW 0x86C9CD`) |

1. `0x86DF51..0x86DF89` — the death branch. `testStatus(obj, 2)` false, the contain list empty and
   `[this + 0x174] == 0` (the port's `m_flag174`): `TheEva->reportEvent([data+0x250] =
   EvaEventLastMemberDeath, obj + 0x38, null)` at 0x86DF99, then `TheGameLogic->destroyObject(obj)`
   at 0x86DFAB. Returns false.
2. `0x86DFB5` — otherwise the split branch: `canSplit()` (+0x1FC), then
   `[[obj + 0x26C] + 0x24] <= 1`, then the size of the contain list. With
   `[data+0x258] = SplitHordeNumber > 0` the split runs only while the horde holds **no more** than
   that number of members (`0x86E013`, `0x86E01C`: `ja` -> only the BANNER line, split and stop);
   with 0, every member must be the same template (0x86E044..0x86E06D, the BANNER bit of the
   template at +0x11C & 0x10 is skipped) and the list must not be empty. Then
   `splitHorde(&id)` (+0x1F8) and `0x430170(id)` when an id came back.
