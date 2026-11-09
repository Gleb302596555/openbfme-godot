# Port 56 / issue #166 — PorcupineFormationBodyModule weapon fields, RW reading and reachability stop

Date: 2026-11-09. Source: RotWK 2.01 `game.dat` (md5 `daed3668006cd90f01c34e5a7da1901f`, caveat S-001), read with `tools/mod/rw.py dis|xref|str` and raw image scans. No game or mod bytes are committed. The BFME2 1.06 decomp (`Open-BFME-2` `PorcupineFormationBodyModuleSlots.cpp` / `PorcupineFormationBodyModuleSlots_AttemptDamage.cpp`) describes the same shape — call the container's slot and return false either way — and is a donor only. Every address below is RotWK.

## Class chain

Runtime factory RW `0x65156D` allocates `0x100` bytes and calls the constructor RW `0x8C619A`. That constructor calls ActiveBody's constructor RW `0x8C3841`, then stores the runtime vtables:

| offset | vtable | role |
|---|---|---|
| `+0x00` | `0xC72FC8` | primary (destructor / name / xfer thunks) |
| `+0x0C` | `0xC72F08` | shared with ActiveBody |
| `+0x10` | `0xC72E58` | body interface |

Data factory RW `0x6515A8` allocates `0x70` bytes and calls RW `0x8C63F1`. That constructor calls the ActiveBody data constructor RW `0x8C373F`, zeroes `+0x64` and `+0x68` as dwords and `+0x6C` as a byte (`mov byte ptr [esi+0x6C], al`), then stores data vtable `0xC73028`.

Body-interface vtable `0xC72E58` compared with ActiveBody's `0xC720F0` (the vtable ends at `+0xB0`, where `0xC72F08` begins). Only two slots differ:

| slot | ActiveBody | Porcupine | what it is |
|---|---|---|---|
| `+0x00` | `0x8C3FA3` | `0x8C63C4` | `attemptDamage` |
| `+0x9C` | `0x9B501B` (`mov al, 1; ret 4`) | `0x8C62BB` | crush-allow check |

Primary-vtable slots that differ from ActiveBody are the destructor thunk RW `0x8C6228`, the name-key thunk RW `0x8C60BF` (`jmp 0x8C35AF`), the name getter RW `0x8C61C4`, and the xfer thunk RW `0x8C5524`. None of them read `+0x64` / `+0x68` / `+0x6C`.

## Parser rows (table `0xC72E18`)

Read as 16-byte field records (name, parser, user, offset):

| name | parser | offset |
|---|---|---|
| `DamageWeaponTemplate` | `0x73AE79` | `+0x64` |
| `CrushDamageWeaponTemplate` | `0x73AE79` | `+0x68` |
| `CrusherLevelResisted` | `0x42EB2A` | `+0x6C` |

`0x73AE79` resolves the token through TheWeaponStore (`0xDE4A1C`, `call 0x6CC5DF`) and stores the template pointer. `0x42EB2A` parses an integer and, when the value is in `-128..127` (`cmp eax, -0x80` / `cmp eax, 0x7F` at `0x42EB42` / `0x42EB47`), stores one byte (`mov byte ptr [ecx], al` at `0x42EB4F`). The ctor zeroes that same byte. The port already parses the three names (`CombatModules.cpp`).

## Gate RW `0x8C6244` — always false

Bytes at `0x8C6244`: `8B 41 08 85 C0 74 1B 8B 80 7C 02 00 00 85 C0 74 11 8B 80 58 02 00 00 85 C0 74 07 8B 10 8B C8 FF 52 7C 32 C0 C3`.

```
mov eax, [ecx+8]            ; module + 8, the Object
je  0x8C6266                ; null -> false
mov eax, [eax+0x27C]        ; Object + 0x27C, the container
je  0x8C6266
mov eax, [eax+0x258]        ; container's contain interface
je  0x8C6266
call [vtable+0x7C]          ; contain slot 0x7C
xor al, al                  ; 0x8C6266: the call's result is discarded
ret
```

Every branch, including the fall-through after the virtual call, lands on `xor al, al; ret`. The function cannot return true.

The virtual call is the contain interface's slot `0x7C`, the same slot RW `0x68C866` tail-calls (`mov ecx, [ecx+0x258]; jmp [vtable+0x7C]`). For the HordeContain family the slot is RW `0x872A6F` (vtable `0xC5B480` and the same slot on `0xC5C110` / `0xC5D268`):

```
lea eax, [ecx-0x20]         ; contain interface is module + 0x20
add ecx, 0xFC               ; module + 0x11C, the horde interface
neg eax / sbb eax, eax / and eax, ecx
ret                         ; pure pointer, no store
```

Every other contain's slot `0x7C` is RW `0x851E97` (`xor eax, eax; ret`). The call has no side effect. Its return is then cleared.

`rw.py xref 0x8C6244` finds exactly two callers: `0x8C62C4` (inside the crush slot) and `0x8C63CD` (inside the damage slot). No absolute pointer to `0x8C6244` exists in the image.

## Damage slot RW `0x8C63C4` (body interface `+0x00`)

`esi` is the interface `this` (module `+0x10`). `lea edi, [esi-0x10]` is the module.

1. `0x8C63CD`: call the gate.
2. `0x8C63D2`: `test al, al`.
3. `0x8C63D4`: push the `DamageInfo *` (the stack argument; the push is before the branch, so the live path still has an argument).
4. `0x8C63D8`: `je 0x8C63E5`. The gate returned 0, so this jump is taken.
5. Dead, `0x8C63DA`: `call 0x8C633D` — the only caller of the field reader (`rw.py xref`).
6. Live, `0x8C63E5`: `mov ecx, esi; call 0x8C3FA3` — ActiveBody's body-interface `attemptDamage` (ActiveBody vtable `0xC720F0` slot 0). `0x8C3FA3` itself adjusts `this` by `-0x10`.

So a hit on a porcupine body is ActiveBody's hit. The reflected weapon never runs.

### Dead reader RW `0x8C633D` (not reached)

For the record, if the gate ever returned true the helper would:

- return immediately when the object (`[module+8]`), the data (`[module+4]`) or `data+0x64` is null (`0x8C634C` / `0x8C6354` / `0x8C635A`);
- resolve `DamageInfo+0x08` (the source id) through TheGameLogic `0xDE412C` / `findObjectByID` RW `0x449681` (`0x8C6366`);
- skip a null source, a dead source (`object+0x458` bit 0, `0x8C6374`) or a source whose template KindOf bit 25 is set (`test byte ptr [template+0x10B], 2` at `0x8C6380`; KindOf mask base is template `+0x108`, TheKindOfNames `0xDA0E68` index 25 is `PROJECTILE`);
- compare the weapon's unmodified range (`fld [weapon+0x14]`, RW `0x9E68DC`) with the distance of the two positions (`0x8C6269`: `fcomip` / `jb` returns false when the range is below the distance) and, only when in range, call TheWeaponStore `0xDE4A1C` / `createAndFireTempWeapon` RW `0x6CF590` (`0x8C63B8`) with `(DamageWeaponTemplate, self, source)`.

The helper does not replace the incoming damage. Even on this dead path ActiveBody's `attemptDamage` still runs afterwards.

## Crush slot RW `0x8C62BB` (body interface `+0x9C`)

`lea ebp, [esi-0x10]` is the module. `0x8C62C4` calls the gate. `0x8C62C9` `test al, al` / `jne 0x8C62D1` is not taken. `0x8C62CD` `inc al` turns the gate's 0 into 1, and `0x8C62CF` jumps to the `ret 4`. The slot returns true: the crush is allowed. That is the same answer as ActiveBody's slot RW `0x9B501B` (`mov al, 1; ret 4`). SquishCollide already treats the slot as always yes (S-580).

### Dead reader (not reached)

If the gate returned true, `0x8C62D1` onward would:

- return 1 (allow) when the object, the data, or byte `data+0x6C` is 0 (`0x8C62D5` / `0x8C62E0` / `0x8C62E4`);
- call `crusherLevel` RW `0x695070` on the other object and `cmp byte ptr [data+0x6C], al` / `jl` (`0x8C62F6`): a signed compare; `CrusherLevelResisted < crusherLevel` allows the crush;
- otherwise load `data+0x68`. A null weapon returns 0 (refuse). A set weapon is range-tested the same way as the damage helper (`0x9E68DC` / `0x8C6269` at `0x8C6319`); in range it is fired through RW `0x6CF590` (`0x8C632D`). Either way the slot then returns 0 (refuse the crush).

`rw.py xref 0x8C6269` finds only `0x8C6319` and `0x8C63A2`, both inside these dead branches. No absolute pointer to `0x8C6269` or `0x8C633D` exists. The crush slot's only pointer is vtable `0xC72E58+0x9C`.

## Xfer

RW `0x8C5524` (primary vtable slot `+0x0C`) calls the version prologue RW `0x404F15` (window bytes 1, 1) and then ActiveBody's xfer RW `0x8C3388`. It does not transfer `+0x64` / `+0x68` / `+0x6C`. Those live on the module data, not on the runtime object. The port's `PorcupineFormationBody::xfer` already calls `ActiveBody::xfer` only. No new saved state.

## What the port already does

`PorcupineFormationBody` is an `ActiveBody` with no `attemptDamage` override and no crush-refuse override (`CombatModules.h`). The three fields are parsed and have no reader in the port (`porcupineData()` has no caller). That is retail's reachable behaviour: the weapons are never fired, a crush is never refused by this body, and nothing extra is saved. S-580 already records the gate. Adding the dead branches would be a second path.

Ennorath writes the three fields (field audit: parsed-unused, 50 definitions each). They have no effect in RotWK 2.01, so they have no effect to show in the game. Pikemen still resist cavalry through the porcupine formation's attribute modifiers and `CrushRevengeWeapon`, which are not these fields (S-580).

## Stop

Registered as S-7191. No port code, no saved state, no `PORT-56` test: a test of the dead branches would have to call code retail never calls.

INFERENCE: a runtime-generated patch of the `xor al, al` at `0x8C6266`, or an external hook outside the static image, was not excluded. The stop is the loaded `game.dat` image.
