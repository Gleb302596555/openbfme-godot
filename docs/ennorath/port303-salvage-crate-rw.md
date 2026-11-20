# PORT-303: SalvageCrateCollide — the three uncited functions and the four unread fields

Fork issue 695 (task 283). Image: RotWK 2.01 `game.dat`, md5 `daed3668006cd90f01c34e5a7da1901f` (caveat S-001), the file
`docs/ennorath/PLACEMENT_SEAL.md` names, read through `tools/mod/rw.py dis|xref|str` and capstone walks of one contiguous
`.text` range (no address arithmetic). **This sandbox had no copy of the image** (the gitignored `workspace/` tree is not part of
the branch): the instructions quoted below were read earlier, in the sessions that produced `docs/ennorath/port94-salvage-crate-rw.md`
(Port 94, issue 242) and the port's own target-facts comment in `engine/src/GameLogic/Module/SalvageCrateCollide.h`; everything
that is new here comes from those recorded reads and from `docs/SAVES-RETAIL.md`, whose inventory was generated from the same
image by `tools/save/xfer_inventory.py`. No new instruction is claimed.

The earlier reading established that the four fields are parsed and unread (stop S-7234). This lane answers the other half of the
issue — the three functions the module map still lists as `none` — and records the one defect the reading found.

## 1. The three uncited functions

| function (vtable slot) | what it is | instructions (as read in the earlier sessions) | verdict |
|---|---|---|---|
| RW `0x8C12EC` (RW `0xC715C0` slot 2) | the class-name getter | `mov eax, 0xC0AA68; ret` — the ASCII string `SalvageCrateCollide` at RW `0xC0AA68` | **not behaviour**: the port answers the same call as `Module::getModuleClassName` (`engine/src/Common/Module.h`), the name the `ModuleFactory` put there at creation |
| RW `0x8C130B` (slot 4) | the lazy name-key accessor | the RW `0xA3CEF0` guard on the once-only slot `[0xDE9ACC]`, RW `0x5487EC` (the retail name hash `h * 33 + c`, RW `0x548538`) of the same string `0xC0AA68`, the key left in `[0xDE9ACC]` and returned | **not behaviour**: the port resolves the same name once, in `ModuleFactory::friend_setModuleClass` (`engine/src/Common/Thing/ModuleFactory.cpp`, `m_keys.nameToKey(name)`), and returns the key as `Module::getModuleNameKey` |
| RW `0x8C1350` (slot 0) | the scalar deleting destructor | the data vtable RW `0xC09D50` goes back to the base's before the compiler's `operator delete` (RW `0x42F6A0`) | **not behaviour**: C++ makes the same function; the port has no code for it and needs none |

The remaining slots of the class are already cited in the port (`onCollide` RW `0x8C06B6`, `wouldLikeToCollideWith` RW `0x8C0664`,
`xfer` RW `0x8C1442`, `executeCrateBehavior` RW `0x8C158B`, `isValidToExecute` RW `0x8C145D`). The three citations now sit in
`SalvageCrateCollide.h`'s and `SalvageCrateCollide.cpp`'s target-facts comments and at the factory line that resolves the key
(`Common/Thing/ModuleFactory.cpp`), which is where the module map's citation search looks.

## 2. The four fields the issue names

The rows and defaults (as recorded in `SalvageCrateCollide.h` and `docs/ennorath/port94-salvage-crate-rw.md` §1):

| field (data offset, row) | parser | reader | verdict |
|---|---|---|---|
| `LevelUpRadius` (+ 0x68, RW `0xC05FB0`, default 100.0 RW `0xBD88D8`) | RW `0x42EEFA` (`scanPercentToReal`: RW `0x42EB18` = `scanReal` RW `0x42EAAD` × 0.01) | its only reader is RW `0x8C16BF` `fld dword ptr [esi+0x68]`, inside the outcome ≤ 1 branch RW `0x8C168D` … RW `0x8C1718` of `executeCrateBehavior` | **parsed and unread**: the branch is unreachable (§2.1) |
| `BannerChance` (+ 0x60, RW `0xC05F90`, default 0.0) | RW `0x42EEFA` | none of this class; the one disp-`0x60` byte read of the crate region, RW `0x8C102C`, is `VeterancyCrateCollide`'s `AddsOwnerVeterancy` (its data ctor RW `0x8C0FAB` sets vtable RW `0xC71554`) | **parsed and unread** |
| `ResourceChance` (+ 0x6C, RW `0xC05FC0`, default 0.5) | RW `0x42EEFA` | no instruction of the crate region touches disp `0x6C` (covering-instruction scan, all encodings) | **parsed and unread** |
| `PorterChance` (+ 0x5C, RW `0xC05F80`, default 0.0) | RW `0x42EEFA` | no instruction of this class reads + 0x5C; the disp-`0x5C` hits of the region (RW `0x8C08E5`, `0x8C0B4E`, `0x8C0E20`, `0x8C10EE`, `0x8C10F1`, `0x8C15BA`) belong to the sibling crate classes and to this class's `AllowAIPickup` (+ 0x7C) / controlling-player reads | **parsed and unread** |

`ExecuteAnimation` and the base fields are not this issue's (S-260 keeps the animation).

### 2.1 Why the LevelUpRadius branch is dead

`pickOutcome` RW `0x8C136C` has one caller, RW `0x8C15CA` inside `executeCrateBehavior` RW `0x8C158B`, and returns 2 (roll
`< LevelUpChance` + 0x64, RW `0x8C138B`), else 4 when `Upgrade` + 0x78 is not empty, else 3. The dispatch after it tests `jl`
(RW `0x8C15CF`), then `jle 1` (RW `0x8C15D7`), then `je 2` (RW `0x8C15E0`), so the two arms that would read + 0x68 (outcome ≤ 1)
can never be entered. BFME2 1.06 has the same shape (DONOR `0x004BD281` returns 2/3/4; the case 0/1 of `executeCrateBehavior`
`0x004BD442` is dead there too). RotWK's salvage crate rewards the **picker**: Zero Hour's group reward survived only as dead code,
and the data rows (and the 100.0 default) came along with it.

## 3. What this lane changes

| change | RotWK address / source | test |
|---|---|---|
| `CrateCollide::xferCrateCollide` writes and reads the version **window (1, 2)** instead of (2, 2), and gates the ever-executed byte + 0x14 and the port's counter on version ≥ 2, so a retail version-1 snapshot loads | RW `0x8C0760`; the window is the inventory row of `docs/SAVES-RETAIL.md`: `0x8C0760 … CrateCollide::xfer … (1, 2) … ver(1, 2) … bool`; the byte's version-2 gate is the port's reading of the same row | `PORT-303: a retail version-1 CrateCollide snapshot loads without the ever-executed byte` — on the parent branch the earliest version is 2 and the load throws `Found version 1 but the earliest supported is 2` |
| the three uncited functions are cited at their ported definitions (no code) | RW `0x8C12EC`, RW `0x8C130B`, RW `0x8C1350` | the three identity tests |

Nothing else moves. The four fields stay parsed and unread; the crate's live behaviour (outcome, level, upgrade, money, FX) is
unchanged; a **version-2** stream writes exactly the bytes the parent branch wrote, so no save pin, hash or snapshot layout moves.

## 4. Tests (`engine/tests/test_port_303.cpp`, lane `engine/cmake/Port303.cmake`)

| test | what it holds |
|---|---|
| the name getter | `getModuleClassName()` is `SalvageCrateCollide` on the crate, and a plain unit has no crate collide |
| the lazy name key | the module's key is the world name table's key for the class name; the retail name hash equals the module's class hash; an unknown name has no key |
| the destructor's identity | the instance's `xferClass()` is `SalvageCrateCollide`, and its data is the class's own layout |
| the xfer pair | the five version bytes in order (1, 2, 1, 1, 1), then the flag and the counter; a round trip |
| the version-1 snapshot | five bytes load and an ever-executed byte is not asked for; a byte outside the window (9 at either slot, 0 at CrateCollide's) is refused |
| the four fields | with LevelUpChance 0% and the three chances and the radius at 100%: one draw at line 0x45, outcome 3, +250 money, and neither the picker nor a bystander inside the radius levels |
| the money rule | Min == Max pays 250 with no second draw |
| ExecuteFX | one `CrateCollide` event with the data's name for the collecting unit, and the crate gone |
| the neighbour | LevelUpChance 100% still levels the picker and pays nothing |

## 5. Stops

No new stop, and `docs/STOPS.md` is not touched. The four fields already have S-7234 (Port 94, issue 242), and that row already carries
every point this reading re-establishes: the per-field rows and defaults, ResourceChance's zero disp-`0x6C` hits in the crate region,
BannerChance's single disp-`0x60` hit being VeterancyCrateCollide's, LevelUpRadius's only reader RW `0x8C16BF` inside the unreachable
outcome ≤ 1 arm with the `pickOutcome` proof, and the BFME2 1.06 agreement. What this lane adds is the other half of the issue: the three
functions, cited at their ported definitions, and the xfer window defect. The reserved ids S-7551 / S-7552 / S-7553 are therefore not
taken; the reasoning is recorded here so a later reader does not re-open the question.

## 6. What is not established

- The whole-image scan for disp `0x5C` / `0x68` is not repeated; the claim is the crate region RW `0x8C0600` .. RW `0x8C1800`, the
  class's three vtables and the module-data pointer's escape set, exactly as Port 94 stated it.
- The instruction that gates the base xfer's `bool` in retail's RW `0x8C0760` was not separated from the window evidence of
  `docs/SAVES-RETAIL.md`'s inventory: the port's gate is that row's transfer list read as "… ver(1, 2) … bool".
- The full suite is not all green on this branch or on its base: `local_66*` (SIGSEGV), five `audio decode*` cases (the MP3 fixtures need
  `lame`) and `bfme1 fields*` (a BFME1 gap row, `AODHordeContain DamageAmountRequired`) fail identically with this lane's changes stashed,
  so they are pre-existing and unrelated; every crate, save and xfer lane above them is green.
- The in-game half of a crate pickup (the `GUI:GainRank` / `GUI:AddCash` client texts) is S-260's; this lane adds no client code.
