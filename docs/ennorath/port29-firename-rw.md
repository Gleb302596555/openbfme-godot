# Port 29 — `BuildingBehavior/FireName`: what RotWK 2.01 does with it (issue #101)

Binary: retail RotWK 2.01 `game.dat`, md5 `daed3668006cd90f01c34e5a7da1901f`, disassembled with
`tools/mod/rw.py` / capstone + pefile. All addresses are virtual (the image base is included).

**Verdict: RotWK reads `FireName`.** The one reader is `BuildingBehavior::update` (`RW 0x85935C`),
instruction `RW 0x85944B` (`lea eax, [edi + 0x2c]`, `edi` = the module data). `FireName` is a **list of
sub-object names of the building's drawable**, in the same four-list loop as `NightWindowName`,
`FireWindowName` and `GlowWindowName`; it is not an object template, not an FX and not a model condition.

This contradicts the 2026-11-08 correction in issue #101, which expected the verdict "RotWK does not read
`FireName`". The correction's two candidate readers (`RW 0x852694`, `RW 0x8532E9`) are indeed not readers
of this field; the reader is the class's own update slot, in the **secondary** vtable at module `+0x10`
(`RW 0xC56EF0`), which is not the table the correction's recipe enumerates (it enumerates the primary
vtable `RW 0xC56EFC`).

---

## 1. The field row and the module data layout

Field table of `BuildingBehaviorModuleData` (`RW 0xC56F70`, rows of 16 bytes: name, parser, userData, offset):

| row | name (string) | parser | offset | port field |
|---|---|---|---|---|
| `RW 0xC56F70` | `NightWindowName` (`RW 0xC56F5C`) | `RW 0x42EED6` | `+0x08` | `m_nightWindowName` |
| `RW 0xC56F80` | `FireWindowName` (`RW 0xC56F4C`) | `RW 0x42EED6` | `+0x14` | `m_fireWindowName` |
| `RW 0xC56F90` | `GlowWindowName` (`RW 0xC56F3C`) | `RW 0x42EED6` | `+0x20` | `m_glowWindowName` |
| **`RW 0xC56FA0`** | **`FireName` (`RW 0xC56F30`)** | **`RW 0x42EED6`** | **`+0x2C`** | **`m_fireName`** |

- The parser `RW 0x42EED6` is the same for all four rows: it grows the list (`RW 0x42CA04`) and appends one
  parsed ASCII string (`RW 0x42E59E`), i.e. `INI::parseAsciiStringVectorAppend` — the port's row
  (`engine/src/GameLogic/Module/ConstructionModules.cpp:91`) already matches.
- `BuildingBehaviorModuleData` is `0x38` bytes: the data create `RW 0x64A89C` allocates `0x38`, the data
  constructor `RW 0x8594ED` sets the vtable `RW 0xC56FC0` and constructs **four** 12-byte lists at
  `+8`, `+0x14`, `+0x20`, `+0x2C` (`RW 0xA3D28E`, element size `0xC`, count `4`) — one row per field row.
- The table is registered by the data create: `RW 0x64A8D3` pushes the parse function `RW 0x85926A`
  (`RW 0x859271`: `push 0; push 0xc56f70; call 0x42b8d7`).

## 2. Binding the class (name → create → constructor → vtable)

1. `"BuildingBehavior"` lives at `RW 0xC0BE70` (`rw.py str BuildingBehavior`: xrefs `0x657AF3`,
   `0x859220`, `0x85924B`).
2. The name getter is `RW 0x85921F`: `mov eax, 0xc0be70; ret` — **vtable `RW 0xC56EFC`, slot `+0x08`**.
3. The runtime module create is `RW 0x64A864`: `push 0x24` (module size `0x24`) → `operator new` → ctor
   **`RW 0x85927B`**.
4. The ctor `RW 0x85927B`: base ctor `RW 0x653114` (→ `RW 0x6530B4`, `BehaviorModule`), then
   `mov [esi], 0xc56efc` (primary vtable), `mov [esi+0xC], 0xc67300`,
   `mov [esi+0x10], 0xc56ef0` (**the UpdateModule interface vtable**), `mov byte [esi+0x20], 0`
   (`m_aflame`), `mov byte [esi+0x21], 1` (`m_firstUpdate`), `call 0x850c32` (`setWakeFrame(obj, 1)`).
5. Inherited vtables: `BehaviorModule` `RW 0xC04DDC` (ctor `RW 0x6530B4`), the intermediate base
   `RW 0xC062E4` (ctor `RW 0x653114`, its update interface `RW 0xC8746C`), and the data class
   vtable `RW 0xC56FC0` (ctor `RW 0x8594ED`, dtor `RW 0x85952E` → `RW 0x85954A`).

Method tables actually scanned for a read of `module data + 0x2C`:

| vtable | entries | verdict |
|---|---|---|
| `RW 0xC56EFC` (BuildingBehavior, primary) | `0x8592D4` (scalar deleting dtor → `0x859206`), `0x8B88B0`, `0x85921F` (name), `0x859324` (xfer), `0x859225`, `0x63F3BF` ×4, `0x8851E4`, `0x851E97` ×2, `0x64DFDA` | no `+0x2C` read |
| `RW 0xC56EF0` (update interface, module `+0x10`) | **`0x85935C` (update)**, `0x6530FA`, `0x850C4E` | **`0x85935C` reads `+0x2C` at `RW 0x85944B`** |
| `RW 0xC062E4` (intermediate base) | `0x653157`, `0x4D0EAF`, `0x653151`, `0x850C60`, `0x43B160`, `0x63F3BF` ×4, `0x8851E4`, `0x851E97` ×2, `0x64DFDA` | no `+0x2C` read of the data (only `push 0x2c` sizes) |
| `RW 0xC8746C` (base update interface) | `0x43B160`, `0x6530FA`, `0x850C4E`, `0x851E97` ×5 | no |
| `RW 0xC04DDC` (BehaviorModule) | `0x6530DE`, `0x63F3BF`, `0x64A48C`, `0x8CD4AA`, `0x43B160`, `0x63F3BF` ×4, `0x8851E4`, `0x851E97` ×2 | no |
| `RW 0xC56FC0` (module data) | `0x85952E` (dtor → `0x85954A`), `0x63F3BF`, `0x464A09`, `0x9F3A3C`, `0x9188EB` ×8 | no |

Two `+0x2C` hits elsewhere are **not** readers of this field: `RW 0x851E9A` (`and dword [ecx+0x2c], 0`) is a
two-instruction stub that writes a module-sized object, and `RW 0x8851E4` / `RW 0x850C4E` touch `+0x2C` of
their own receiver. The issue's two candidates were re-checked: `RW 0x8532E9` is
`SpecialAbilityUpdate::grabFinish` (as the correction says) and `RW 0x852694` reads no `+0x2C` of this data.

## 3. `BuildingBehavior::update` (`RW 0x85935C`), branch by branch

Registers: `esi` = `this` = module `+0x10` (the update interface sub-object), so `esi + 0x10` is
module `+0x20` (`m_aflame`) and `esi + 0x11` is module `+0x21` (`m_firstUpdate`);
`edi` = `[esi - 0xC]` = module `+4` = the `BuildingBehaviorModuleData`; `ebx` = `[esi - 8]` = module `+8`
= the `Object`; `[ebp-4]` = the drawable.

1. `RW 0x859366..0x859372`: no object → `RW 0x859398` `mov eax, 0x3fffffff` (sleep forever).
2. `RW 0x859374`: `test byte [ebx+0x458], 1` (effectively dead) → when dead, `RW 0x85937D`
   `push 0x58; call 0x44ddec` (`testStatus(PHANTOM_STRUCTURE)`); without it → sleep forever.
3. `RW 0x85938A..0x859396`: `call 0x70e013` (`Object::getDrawable`); no drawable → sleep forever.
4. `RW 0x8593A2..0x8593BB`: `eax = [obj + 0x114]` (model condition bits 64..95); `>>3 & 1` = bit 67
   `AWAITING_CONSTRUCTION`, `>>5 & 1` = bit 69 `ACTIVELY_BEING_CONSTRUCTED`; either set → `RW 0x8594E5`
   (return `UPDATE_SLEEP_NONE`, no sub-object work).
5. **First update** (`RW 0x8593C1`: `cmp byte [esi+0x11], 0`), `RW 0x8593C7..0x859419`:
   - `RW 0x8593C7` `eax = edi + 8`, `RW 0x8593D0` count `= 4`, loop `RW 0x8593D7..0x8593ED`:
     `push 0` (hide) + `push drawable` + `push list` + `call 0x8592f0`; the list pointer grows by `0xC`,
     so **all four lists are hidden: `+8` night, `+0x14` fire window, `+0x20` glow, `+0x2C` fire name**.
   - `RW 0x8593EF`: night test `[TheWritableGlobalData 0xDE4364] + 0x134 == 4` (NIGHT) → `RW 0x8593FD`
     shows list `0` (the night windows).
   - `RW 0x859410`: `m_firstUpdate = 0`; `RW 0x859414`: `call 0x672713` (`Drawable::updateSubObjects`).
6. **Fire start, AFLAME 0 → 1** (`RW 0x85941C`: `m_aflame == 0`), `RW 0x859422..0x859486`:
   - `RW 0x859422` `push 0xa; call 0x44ddec` (`testStatus(AFLAME)`); **not** aflame → `RW 0x859488`
     (`cmp byte [esi+0x10], 0` → `RW 0x8594E5`: return `UPDATE_SLEEP_NONE`, nothing is applied and
     `updateSubObjects` is **not** called).
   - `RW 0x85942F`: `m_aflame = 1`; `RW 0x859433`: `i = 0`; `RW 0x859435`: `esi -= 0x10` (the module).
   - Loop over `i = 0..3` (`RW 0x859438..0x859481`), the list is `edi + i*0xC + 8`:
     - `RW 0x85943D`: night again? if not → `RW 0x859450`.
     - `RW 0x859446`: `i == 3`? if not → `RW 0x859450`.
     - `RW 0x85944B`: **`eax = edi + 0x2C`** — the `FireName` list; `RW 0x85944E` jumps to
       `RW 0x859470` (`push 1`) → **the fire list is shown, and only at night**.
     - `RW 0x859450`/`RW 0x859455`: `i == 1` (`FireWindowName`) or `i == 2` (`GlowWindowName`) →
       `RW 0x859467` (`push 1`) → **shown**.
     - otherwise (`i == 0`, or `i == 3` on a day map) → `RW 0x85945A..0x859465` (`push 0`) → **hidden**.
     - `RW 0x859472..0x859478`: `push drawable`, `ecx = esi`, `push eax`, `call 0x8592f0`.
   - `RW 0x859483`: `edi += 0x14` (`edi` = data `+0x14` = `FireWindowName`); `RW 0x859486` jumps into the
     shared tail `RW 0x8594D0` → `push 1` → **the fire window list is shown a second time**, then
     `RW 0x8594DD` `call 0x672713` (`Drawable::updateSubObjects`).
7. **Fire out, AFLAME 1 → 0** (`RW 0x85948E`), `RW 0x85948E..0x8594E0`:
   - `RW 0x85948E` `push 0xa; call 0x44ddec`; still aflame → `RW 0x8594E5` (return `UPDATE_SLEEP_NONE`,
     no sub-object work, no `updateSubObjects`).
   - `RW 0x85949B`: `m_aflame = 0`; `RW 0x8594A1`: `edi += 8`; `RW 0x8594A6`: count `= 4`; loop
     `RW 0x8594AD..0x8594C0`: **all four lists are hidden** (the fire name list included);
     `RW 0x8594C2`: if night → the shared tail `RW 0x8594D0` shows `edi` (the night window list);
     `RW 0x8594DD`: `Drawable::updateSubObjects`.
8. `RW 0x8594E5`: `xor eax, eax; inc eax` → return `UPDATE_SLEEP_NONE` (1).

### What a name becomes

The helper `RW 0x8592F0`(list, drawable, visible) walks the list (`RW 0x8592F6..0x85931A`) and calls
`Drawable::showSubObject` (`RW 0x672823`) once per name: `RW 0x67282F..0x672857` takes every draw module of
the drawable (`[ecx+0x14C]`), asks it for its `ObjectDrawInterface` (vslot `0xA8`) and calls **vslot `0x88`
= `setSubObject`** with `(name, visible, permanent = 1, 0, 0, 0)` — the literal `1` is pushed at
`RW 0x85930B`. So `FireName` names **sub-objects of the building's model**; showing them makes the fire
geometry appear on the burning building. An empty name is a no-op (`RW 0x401e64`, the empty-`AsciiString`
test inside `setSubObject`).

## 4. Ennorath 1.9.8.2 data (mod archives, values only — no mod text is committed)

Active (uncommented) `FireName` in the mod:

| object | file | values | `FlammableUpdate`? |
|---|---|---|---|
| `ElvenBarracksBFME1` | `data/ini/object/civilian/civilianbuildings.ini` | `FIRE01`, `FIRE02` | no |
| `GondorHseHeal` | `data/ini/object/obsolete/hseheal.ini` | `FIRE01`, `FIRE02` | yes |

`FireWindowName` (same loop, so the same fix applies it) is active on `ElvenBarracksBFME1`
(`WINDOW_F01`), `ArnorStable` (`WINDOW_F01`), `GondorStable` (`WINDOW_F01`), `ImladrisCitadel`
(`GLOW_WIN_V2`) and `GondorHseHeal` (`WINDOW_F01`). Every other `FireName` / `FireWindowName` line in the
mod is commented out.

`AFLAME` (status `0xA`) is set in the port only by `FlammableUpdate` (`FlammableUpdate.cpp:439`), so of the
objects above only `GondorHseHeal` (in the mod's `obsolete/` folder) can carry a building through the
AFLAME 0 → 1 transition that shows `FireName`.

## 5. The parse row: `RW 0x42EED6` clears the list first

Every row of the table uses the parser `RW 0x42EED6`:

```
0042EED6  push  esi
0042EED7  mov   esi, [esp + 0x10]     ; the list (the row's offset)
0042EEDB  push  dword [esi + 4]       ; end
0042EEE0  push  dword [esi]           ; begin
0042EEE2  call  0x42ca04              ; erase(begin, end): destroys the range and stores the new end (RW 0x42BC15 / 0x42C86C, [esi+4] = result)
0042EEE7  push  0
0042EEE9  push  esi                   ; the list
0042EEEA  push  0
0042EEEC  push  [esp + 0x14]          ; the ini
0042EEF0  call  0x42e59e              ; append every token of the line (macro expanded, split on whitespace; the loop head is RW 0x42E626)
0042EEF9  ret
```

So the row's parser is `INI::parseAsciiStringVector` (**clear, then append all tokens of the line**), not the appending
`INI::parseAsciiStringVectorAppend` (`RW 0x42E59E` alone). The port already maps `RW 0x42EED6` to
`parseAsciiStringVector` for the other rows of the same parser (`FlagSubObjectNames`, `RequiredUpgradeNames`,
`LeadersAllowed`, `BannerCarriersAllowed`; `PlayerTemplate.h` names the two addresses outright).

In Ennorath only `FireName` is written twice on the same object (`FIRE01`, then `FIRE02`), so this changes exactly one
thing in the mod's data: RotWK's `FireName` list is `[FIRE02]`, not `[FIRE01, FIRE02]`.

## 6. State of the port before this lane

- `BuildingBehaviorModuleData::m_fireName` is parsed (`ConstructionModules.cpp:91`,
  `INI::parseAsciiStringVectorAppend`) and never read — the field audit's `parsed-unused`.
- `BuildingBehavior::update` (lane PORT-83, issue #231) applies **two** of the four lists
  (`m_nightWindowName`, `m_glowWindowName`); `RW 0x8593D0`'s four-iteration hide loop, `RW 0x85944B`'s
  fire-list show and `RW 0x8594D0`'s fire-window show were left out on purpose for this lane.
- No new simulation state: the module's two latches (`m_aflame`, `m_firstUpdate`) already round-trip
  through `xfer` version 2 and `crc` (lane PORT-83), so this lane adds no `xfer` and no hash change.
