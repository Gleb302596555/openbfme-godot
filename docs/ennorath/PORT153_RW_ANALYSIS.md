# PORT-153 — SkirmishAI ArmyDefinition: economy, defence and opportunity-target settings

Issue #371. Branch `arena/08c9c258-openbfme-godot`. All addresses are RotWK 2.01 `game.dat`
(md5 `daed3668…`), read with `tools/mod/rw.py … dis`. Verdicts are per field: reader or no reader,
with the chain that proves it.

## 1. Where the 13 fields live and how the port has them

`ArmyDefinition` (size 0xEC, ctor RW 0x82FF49, create RW 0x83027B, field table RW 0xC52B40
parsed by RW 0x8302A0, registered RW 0xDADAAC) — the port's `ArmyDefinition` in
`engine/src/GameLogic/SkirmishAI/SkirmishAIData.{h,cpp}` already parses every one of them and
carries them into `xfer`/the hash (`SkirmishAIData.cpp:282..304`, `:793..812`). The port's AI reads
the army definition through `ai.army` (`AISkirmishPlayer`), the analogue of RW's army pointer.

The AI player record holds the parsed army definition at `+0x160` (RW 0x8EE769, 0x9EFB22,
0x9B9BCF, 0x90B1DC …): `mov reg, [record + 0x160]` → the `ArmyDefinition *`.

## 2. Method and its verification

A linear scan over the whole `.text` (90 000+ instruction stream, function starts from every
`E8/E9` rel32 target plus `55 8B EC` prologues) found every `mov reg, [x + 0x160]` load (296
`disp32 = 0x160` sites), then followed the register (including spills to `[ebp - n]` and reloads)
for up to 120 instructions per load and reported every `[army + 0x<field>]` / `[army + idx*4 + 0x7C]`
read.

The scan reproduces the readers the port already has, which is what makes its silences meaningful:

- `+0x40` `EconomyBuilderMinFarmsOwned` → RW **0x8EE7DD** (ported: `AIBaseBuilder.cpp` `wantsFarm`);
- `+0x50` `EconomyBuilderMinTimeBetweenFarmsRush` → RW **0x8EE7CC** (ported: same);
- `+0xD0` `ChanceForUnitsToUpgrade` → RW **0x9A1746** (ported: `AIUpgrade.cpp`);
- `+0x98` `OffensiveBuildings` → RW **0x9BAA80** (ported: `AIStatCollector.cpp`).

A 4-byte-only search would miss these (the fields are read as `[reg + 0x3C]`, i.e. a one-byte
disp8), and reading only inside the function that calls the record getter would miss the thunk
RW 0x90B1CF (`mov eax, [eax + 0x160]; ret`, called from RW 0x90B238 / 0x90B278 / 0x90B4F8) —
the trap the port's own history records for `SecondsTillTargetsCanExpire` / `ChanceForTargetToExpire`.

## 3. Readers found

### 3.1 `ChanceToUseAllUnitsForDefenseTarget_Small / _Med / _Large` (+0x7C, +0x80, +0x84) — READ

RW **0x9EFAF0** (the base `AITactic::initializeTeamTemplate`, slot 3 of the base tactic vtable):

```
009EFAF0  mov  dword ptr [ecx], 0xc8ee24      ; base vtable
009EFAF6  jmp  0x8f26c3                       ; the common team-template init
009EFB08  call 0x8f121f                       ; the tactic's applicability / minimum test
009EFB18  push dword ptr [esi + 0x24]         ; the AI's player index
009EFB1B  call 0x6a950b                       ; TheSkirmishAIManager -> player info record
009EFB22  mov  edi, dword ptr [eax + 0x160]   ; the record's ArmyDefinition
009EFB28  mov  eax, dword ptr [esi + 0x20]
009EFB2B  mov  esi, dword ptr [eax + 0x2c]    ; the prototype's type index (0 / 1 / 2)
009EFB40  call 0x6d332c                       ; GetGameLogicRandomValueReal(0, 1, "…", 0x2B)
009EFB45  fld  dword ptr [edi + esi*4 + 0x7c] ; ChanceToUseAllUnitsForDefenseTarget[index]
009EFB4F  fcompi st(1)                        ; draw  vs  chance
009EFB53  jae  0x9efb5f                       ; draw >= chance -> keep the size
009EFB55  mov  dword ptr [ebx + 0x2d0], 4     ; else the team's maximum (+0x2D0) = 4
009EFB5F  mov  dword ptr [ebx + 0x21c], 0x28  ; the team's priority (+0x21C) = 40, always
```

`ebx` is the team template (the `initializeTeamTemplate` argument); `+0x2D0` is its maximum team
size (the port's `AITacticTeam::maximum`) and `+0x21C` its priority (`AITacticTeam::priority`).
So the semantic is: the chance is the probability of *not* taking the whole army — with the draw
below the chance the team keeps its size, otherwise the size is capped at 4 units.

The field table's three rows are indexed by the *prototype's* type index `[prototype + 0x2C]`
(RW 0x9EFB2B), which is `0 / 1 / 2` for the small / medium / large defensive target — i.e. the
same index the port passes around as `AITarget::size`.

SimpleDefense overrides slot 3 (RW 0x9B4EF0) and therefore never reads the table — that is the
one tactic the port already had (STOPS S-417); every other tactic's team setup goes through this
base slot, so the field matters for those.

### 3.2 `ScavangedResourceBuildings` (+0xA4) — READ

RW **0x9B8D81** (a method, called from RW 0x9B9C01):

```
009B8D81  mov  esi, ecx
009B8D93  push dword ptr [esi + 0x24]          ; the AI's player index
009B8D96  call 0x6a950b                        ; -> player info record
009B8D9D  mov  eax, dword ptr [esi + 0x78]     ; the current index (-1 = none yet)
009B8DA0  cmp  eax, -1
009B8DA3  jne  0x9b8dd6
009B8DA5  mov  eax, dword ptr [edi + 0x160]    ; the ArmyDefinition
009B8DAB  mov  ecx, dword ptr [eax + 0xa8]     ; vector end
009B8DB1  add  eax, 0xa4                       ; vector begin
009B8DB6  sub  ecx, dword ptr [eax]
009B8DB8  push 0x141
009B8DBD  sar  ecx, 2
009B8DC0  push 0xc89f60                        ; file literal
009B8DC5  dec  ecx                             ; size - 1
009B8DC9  call 0x6d328e                        ; GetGameLogicRandomValue(0, size - 1, …)
009B8DD1  mov  dword ptr [esi + 0x78], eax
009B8DD6  lea  ecx, [eax + 1]
009B8DD9  mov  dword ptr [esi + 0x78], ecx     ; next time the following element
…
009B8DF2  cmp  ecx, edx
009B8DF4  jb   0x9b8dfa
009B8DF6  and  dword ptr [esi + 0x78], 0       ; wrap to element 0
009B8E03  mov  eax, dword ptr [eax + 0xa4]     ; vector begin
009B8E09  lea  eax, [eax + ecx*4]              ; element [index]
009B8E10  call 0x435f30                        ; -> AsciiString of the template name
```

The list's elements are `std::string` (0x18 bytes in this build's STL) — RW builds
`begin + index*4`, i.e. the vector is stored as an array of 4-byte string nodes; the row is
`parseAsciiStringVector`. The method is a round-robin picker: a random first index out of the
army definition's list, then the next index each call, wrapping to 0.

### 3.3 The other nine — no reader found

Checked and found no read of `+0x2C` (`MaxThreatForOpportunityTargets`), `+0x30`
(`ValueToSetForMaxOnDefenseTeam`), `+0x34 / +0x38 / +0x3C`
(`CombatChainSearchDepthForTeamRecruits_*`), `+0x44` (`EconomyBuilderMinMoney`), `+0x48`
(`EconomyBuilderPerFarmValue`), `+0x74 / +0x78` (`MaxBuildingsToBeDefensiveTarget_*`) through an
army pointer, anywhere in `.text`:

- the whole-image scan (296 `+0x160` sites, register-and-stack taint, 120 instructions each)
  reports none of those offsets on a tainted register;
- the sites it does report for those offsets (`0x6118AE [eax + 0x38]`, `0x8457F2 [eax + 0x30]`)
  are a vtable call and a desynced listing, not army reads — both were read linearly and discarded;
- the economy builder RW 0x8EE765 reads `+0x50` and `+0x40` only, and its affordability test is
  `money + total * perFarm <= limit` nowhere: RW 0x8EE7D7..0x8EE7F8 compares `built + inFlight`
  against `+0x40` and nothing else. `+0x44` / `+0x48` are not in the function at all.

Donor check (BFME2 1.06, `Code/GameEngine/Source/GameLogic/SkirmishAI/`): `EconomyBuilderMinMoney`
and `EconomyBuilderPerFarmValue` appear only in the field table and the copy ctor of
`ArmyDefinitionManager.cpp` — the donor's own `AIEconomyBuilder::needMoreFarms` reads `+0x40`,
`+0x48` and `+0x50` with a *different* meaning of `+0x48` (`perFarm` used as a money-per-farm cap
`money + total*perFarm <= limit`), which RotWK's rebuild of the same function no longer does.
That is exactly the case the rules call out: the donor reads it, RotWK's version does not, so the
donor behaviour is not ported and the fields get a verdict with the addresses above.

`MaxThreatForOpportunityTargets`, `ValueToSetForMaxOnDefenseTeam` and the three recruit depths are
likewise absent from the donor's tactics/team builder code (only the field table and the ctor), so
there is no donor reader whose loss in RotWK could be tracked; the RW scan above is the evidence.

## 4. What follows for the port

Ported (this issue): the base tactic team setup's chance table (3.1) and the round-robin
`ScavangedResourceBuildings` picker (3.2), each where the port already has its equivalent
(`AITacticalAI::setupTeam` / its tactic-team init, `AIStatCollector`'s army-list lookups).

Stopped, with the addresses above: the nine fields of 3.3 — parsed, kept in `xfer`/hash as the
rest of the block, never read by RotWK 2.01's AI.
