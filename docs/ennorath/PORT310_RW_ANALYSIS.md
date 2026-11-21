# PORT-310 — ArmyDefinition: the skirmish AI's army settings (issue #702)

All addresses are RotWK 2.01 `game.dat` (md5 `daed3668…`), read with `tools/mod/rw.py … dis`. The donor column is BFME2 1.06
(`/tmp/open-bfme-2`, `Code/GameEngine/Source/GameLogic/SkirmishAI/…`) and is only ever used to understand a mechanism, never as the
source of a behaviour.

## 1. The block and the port's state

| | |
|---|---|
| field table | RW 0xC52B40 (47 rows, `Name/parser/userData/offset`, 16 bytes each) |
| parser | RW 0x8302A0 (`push 0xC52B40` → RW 0x42DB80) |
| object | 0xEC bytes, ctor RW 0x82FF49, create RW 0x83027B, dtor RW 0x830149 |
| registration | RW 0xDADAAC (`ArmyDefinition`, name table RW 0xDADAA8) |
| how the AI gets it | `[player info record + 0x160]` — RW 0x8EE769, 0x9EFB22, 0x9B9BD5, 0x9B9D9D, 0x9BAA43, 0x90B1DC (the getter RW 0x90B1CF) |

The port parses every row (`engine/src/GameLogic/SkirmishAI/SkirmishAIData.cpp:272..320`), carries them in `xfer` and the store hash
(`:780..836`) and reads none of the thirteen. `docs/ennorath/FIELD_GAPS.tsv` lists them as `parsed-unused`, route empty.

## 2. Method

A linear scan of the whole `.text` (capstone, `skipdata`, chunked so that a window may cross a chunk boundary) collected

1. every `mov reg, dword ptr [x + 0x160]` (102 sites) and the record getter RW 0x90B1CF (`mov eax, [eax + 0x160]; ret`),
2. every `mov reg, [x + 0x10]` (the army cache RW 0x9BCF43 writes; 99 sites),
3. every `push [x + 0x160]` / `mov reg, [x + 0x160]` + `push reg` (an army pointer handed to another function),

and followed each pointer through register copies, `[ebp - n]` / `[esp + n]` spills and reloads, and calls (a call kills the volatile
registers, keeps `ebx`/`esi`/`edi` and the stack slots) for 200–400 instructions, reporting every read of any `ArmyDefinition` offset.

The scan reproduces **every** reader the port already has, which is what makes its silence meaningful: +0x1C RW 0x9EDAE1, +0x20
RW 0x9BC9E9, +0x40 RW 0x8EE7DD, +0x50 RW 0x8EE7CC, +0x6C RW 0x90B245, +0x70 RW 0x90B27D, +0x98 RW 0x9BAA80, +0x9C RW 0x8E3F14,
+0xA8 RW 0x8E3F83, +0xB8 RW 0x8F05AD, +0xD0 RW 0x9A1746, +0xD4/0xD8/0xDC/0xE0 RW 0x99FFFE/0x99FFE8/0x99FFDA/0x99FFC4,
+0xE4/0xE8 RW 0x9A1626/0x9A161E. Of the thirteen settings it reports exactly one — `ScavangedResourceBuildings` at RW 0x9B8E03 — and
of the three chance rows only the read RW 0x9EFB45, already known to be unreachable.

A second pass over the same 13 offsets as `lea [reg + off]` (an address taken and passed on) finds nothing, and a flat pass over the
whole `.text` finds no indexed read `[reg + idx*4 + 0x7C]` other than RW 0x9EFB45.

## 3. Reader by reader

### 3.1 `ChanceToUseAllUnitsForDefenseTarget_Small/_Med/_Large` (+0x7C/+0x80/+0x84) — unreachable, as in PORT-153

RW 0x9EFB45 (`fld dword ptr [edi + esi*4 + 0x7C]`) sits in the **base** `AITactic::initializeTeamTemplate` RW 0x9EFAFB, slot 3 of the
base vtable RW 0xC8EE24. `0xC8EE24` occurs in the image only twice, as the immediate of the base constructor RW 0x9EFAE6 and of the
dtor's twin RW 0x9EFAF2; every derived constructor overwrites it (SimpleDefense RW 0x9B4FCC → vtable RW 0xC89890), and every derived
vtable replaces slot 3 (RW 0x8F121F for the plain tactics, RW 0x9B3684, RW 0x9B4EF0). Nothing keeps the base table long enough for the
slot to run — unchanged from S-7336, re-verified here because the issue asks for the chain again.

### 3.2 `ScavangedResourceBuildings` (+0xA4) — reader found, chain CLOSED (new)

RW 0x9B8D81 is a method of the targetless prototype 4, **LumberMillBuild**. The chain, all five links in the image:

```
RW 0x90C0EE   the tactics generator's constructor: `push 0x84` (RW 0x90C1D4) -> `call RW 0x9B8C32` (RW 0x90C1EF)
RW 0x9B8C32   the prototype's constructor; it stores the vtable: `mov dword ptr [esi], 0xC89F18` (RW 0x9B8C83)
RW 0xC89F18   the vtable: slot 1 (+0x04) = RW 0x9B9E06, slot 6 (+0x18) = RW 0x9B8E1E, slot 7 (+0x1C) = RW 0x9BA005,
              slot 12 (+0x30) = RW 0x9B8CD2
RW 0x9B9E06   the applicability (vslot 1): the AI's record (RW 0x9B9E37), `mov eax, [edi + 0x160]` (RW 0x9B9E4E),
              `add eax, 0xA4` (RW 0x9B9E54), `mov ecx, [eax]; cmp ecx, [eax + 4]; je` (RW 0x9B9E59 / 0x9B9E5B):
              "the army's ScavangedResourceBuildings list is not empty"
RW 0x9BA005   the update (vslot 7): when it has no team it calls RW 0x9B9B9B (RW 0x9BA071)
RW 0x9B9B9B   picks a name per entry of the list and matches it against the AI's build requests (RW 0x9B9C01)
RW 0x9B8D81   the round-robin picker: first index GetGameLogicRandomValue(0, size - 1) (RW 0x9B8DC9), then +1 with wrap
              (RW 0x9B8DD6 .. RW 0x9B8DF6), element `[eax + ecx*4]` (RW 0x9B8E03 / RW 0x9B8E09)
```

RW 0x9B9E5B `je 0x9B9EC0` skips to the false exit when begin == end, i.e. the list must be non-empty for the tactic to apply; RW 0x9B8E03
reads the vector's begin. So the field's meaning in retail is: the resource buildings whose build request lets the LumberMill tactic run
earlier, and none of the six sibling targetless prototypes reads it.

**Not ported, and that is the whole of what remains for this field**: the LumberMillBuild tactic's applicability RW 0x9B9E06, setup
RW 0x9B8E1E, update RW 0x9BA005, pending-request helper RW 0x9B9B9B, dozer helper RW 0x9B9056 and the picker RW 0x9B8D81 are exactly
the "other 6 prototypes" S-893 (lane AI-2 r3) left out. Porting them is that stop's work, not this issue's: the mechanism is the whole
targetless-tactic body plus the dozer request path, which is far past the one-third-of-the-task rule and overlaps the open targetless
area (`Port 16 #72`, `Port 33 #105` landed there). The stop row S-7336 now names the reader and the condition to reopen
("when RW 0x9BA005 is ported"), and the already open S-893 (which lists these six prototypes by name and asks for `ArmyDefinition + 0x98 / + 0xA4`) is where the body belongs.

### 3.3 The other twelve — no reader

`MaxThreatForOpportunityTargets` (+0x2C), `ValueToSetForMaxOnDefenseTeam` (+0x30), the three
`CombatChainSearchDepthForTeamRecruits_*` (+0x34/0x38/0x3C), `EconomyBuilderMinMoney` (+0x44), `EconomyBuilderPerFarmValue` (+0x48),
`MaxBuildingsToBeDefensiveTarget_Small/_Med` (+0x74/+0x78) and the three chance rows are read nowhere through an army pointer.

Two independent confirmations for the specific pairs the issue's text hints at:

* **the economy builder** RW 0x8EE765 reads the army twice: RW 0x8EE7CC (`fmul [edx + 0x50]`,
  `EconomyBuilderMinTimeBetweenFarms_Rush`) and RW 0x8EE7DD (`mov edx, [edx + 0x40]`, `EconomyBuilderMinFarmsOwned`), then compares
  `built + inFlight` with it (RW 0x8EE7F2). There is **no money term**: the donor's `money + total * perFarm <= limit`
  (DONOR BFME2 `AIEconomyBuilder::needMoreFarms`) was dropped in RotWK's rebuild, so +0x44 and +0x48 lost their only reader.
* **the defence sizes and team values** live in the same class but the candidates the audit's route column suggests
  (`RW 0x8F15BB`, `RW 0x8F836C`, `RW 0x9BE28D`, RW 0x9F6FD8…) all read a *different* `+0x10` object: the generator's prototype list,
  the player info record's army list, and a debug HUD structure — each read was followed to its base and discarded.

Donor check (BFME2 1.06): the names of all thirteen occur only in `ArmyDefinitionManager.cpp` (field table, constructor, copy
constructor); the two economy values additionally in `AIEconomyBuilder.cpp` with the affordability test RotWK no longer has. Nothing a
donor reader in RotWK could have lost.

## 4. What the port owes

Nothing to change: the block is parsed, saved and hashed as RotWK does, and every one of the thirteen is either unread or read by a
tactic the port does not have. `engine/tests/test_port_310.cpp` pins the parse, the defaults, the hash contribution and — behind
`RW_GAME_DAT` — the closed LumberMillBuild chain and the economy builder's single army read, so the next lane that ports S-893's
tactics finds the reader already located. The stop row S-7336 is narrowed to that condition.
