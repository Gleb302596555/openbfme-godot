# Port 94 / issue #242 — SalvageCrateCollide LevelUpRadius / ResourceChance / BannerChance, RW reading and reachability stop

Issue #242 asked: what does a picked-up `SalvageCrateCollide` crate give in RotWK 2.01 — money,
levels to units in the radius, or a banner — and where are `LevelUpRadius` / `ResourceChance` /
`BannerChance` read. Binary: `RotWK/game.dat`, md5 `daed3668…` (caveat S-001), read with
`tools/mod/rw.py` (dis / str / xref) and a covering-instruction raw scan of the crate-code region.
Donor: Open-BFME-2 (`/tmp/open-bfme-2`, BFME2 1.06 decomp), cited as `DONOR BFME2 0x…`; readable C++
across, every step re-confirmed in RW.

Verdict: **nothing to port — none of the three fields has a reachable reader in RotWK 2.01.**
The port already behaves exactly like RW 2.01 (it parses the fields and reads none of them,
`MoneyEventModules.cpp:96`); this lane adds the proof, the stop row S-7234 and the
`PORT-94*` tests. No engine behaviour changes.

## 1. The class chain (who creates it, which vtable, which slots)

- Field table rows `SalvageCrateCollide`: RW `0xC05F80` … `0xC06010` over the shared CrateCollide
  table RW `0xC71390` (base rows: RequiredKindOf +0x08, ForbiddenKindOf +0x24, ForbidOwnerPlayer
  +0x40, BuildingPickup +0x41, HumanOnly +0x42, PickupScience +0x44, ExecuteFX +0x48,
  ExecuteAnimation +0x4C, ExecuteAnimationTime +0x50, ExecuteAnimationZRise +0x54,
  ExecuteAnimationFades +0x58, all parse rows verified by dumping the table from the image):

  | row | name string | parser | offset | parser meaning |
  |---|---|---|---|---|
  | RW `0xC05F80` | PorterChance `0xC05F70` | RW `0x42EEFA` (percent) | +0x5C | `scanPercentToReal` |
  | RW `0xC05F90` | BannerChance `0xC05F60` | RW `0x42EEFA` (percent) | +0x60 | |
  | RW `0xC05FA0` | LevelUpChance `0xC05F50` | RW `0x42EEFA` (percent) | +0x64 | |
  | RW `0xC05FB0` | LevelUpRadius `0xC05F40` | RW `0x42EEFA` (percent) | +0x68 | (yes: the radius is parsed as a percent — RW quirk, see §5) |
  | RW `0xC05FC0` | ResourceChance `0xC05F30` | RW `0x42EEFA` (percent) | +0x6C | |
  | RW `0xC05FD0` | Upgrade `0xBF8CAC` | RW `0x42EE5E` (AsciiString) | +0x78 | |
  | RW `0xC05FE0` | MinResource `0xC05F24` | RW `0x42EC5E` (int) | +0x70 | |
  | RW `0xC05FF0` | MaxResource `0xC05F18` | RW `0x42EC5E` (int) | +0x74 | |
  | RW `0xC06000` | AllowAIPickup `0xC05F08` | RW `0x42E558` (bool) | +0x7C | |

  The four strings `0xC05F70` / `0xC05F60` / `0xC05F40` / `0xC05F30` have **no other reference
  anywhere in the image** (full-image dword scan: each is referenced only by its own row above).
- Module data ctor RW `0x65592C` (vtable `RW 0xC09D50`, 0x80 bytes): writes vtable (RW `0x655951`),
  PorterChance +0x5C = 0.0 (RW `0x65593F`), BannerChance +0x60 = 0.0 (RW `0x655944`),
  LevelUpChance +0x64 = `0.5f` (`RW 0xBD869C`, RW `0x65594C`/`0x655957`), LevelUpRadius +0x68 =
  `100.0f` (`RW 0xBD88D8`, RW `0x655937`/`0x65595C`), ResourceChance +0x6C = `0.5f` (RW `0x655961`),
  MinResource +0x70 = 100 (RW `0x655966`), MaxResource +0x74 = 5000 (RW `0x65596D`), Upgrade +0x78
  empty (RW `0x655974`), AllowAIPickup +0x7C = false (RW `0x655978`). It only WRITES defaults.
- Module registration: factory site RW `0x65B05C` (`push 0xC0AA68` = the string `SalvageCrateCollide`,
  with the create at RW `0x651052` / data ctor at RW `0x655980`).
- Module ctor RW `0x8C12C2` (over the CrateCollide base ctor RW `0x8C066C`): sets the primary vtable
  RW `0xC715C0`.
- Vtable RW `0xC715C0` (dumped): slot 3 (+0x0C) = xfer RW `0x8C1442`; slot 12 (+0x30) =
  executeCrateBehavior RW `0x8C158B` (pointer stored at RW `0xC715F0`); slot 13 (+0x34) =
  isValidToExecute RW `0x8C145D`.
- The collide vtable RW `0xC715A8` (slot 0 onCollide RW `0x8C06B6`, slot 1 wouldLikeToCollideWith
  RW `0x8C0664`) and the behaviour vtable RW `0xC70CF8` (slot 1 RW `0x863BF3`) — none of these
  touches +0x5C…+0x6C (linear pass over all slots of the three vtables, below).

## 2. What a picked-up crate does in RW 2.01 (the live path)

`CrateCollide::onCollide` RW `0x8C06B6` → `isValidToExecute` (vslot 0x34) → if true,
`executeCrateBehavior` (vslot 0x30, RW `0x8C158B`):

1. **The computer-player refusal** (RW `0x8C159F`): `AllowAIPickup` clear and the other's
   controlling player is a `PLAYER_COMPUTER` (Player +0x5C == 1, RW `0x8C15BA`) → return `false`
   (the crate stays).
2. **One real roll per crate** (RW `0x8C15CA` → RW `0x8C136C` `pickOutcome`):
   `GetGameLogicRandomValueReal(0.0, 1.0, "SalvageCrateCollide.cpp" (RW 0xC715F8), 0x45)` — exactly
   **one** random number (RW `0x8C1372`/`0x8C1374` pushes, RW `0x8C1384` call RW `0x6D332C`).
   Then, reading LevelUpChance (+0x64, RW `0x8C138B`, `fcomip` + `jbe` at RW `0x8C1391`/`0x8C1397`):
   - `roll < LevelUpChance` → **outcome 2**;
   - else `Upgrade` (+0x78, RW `0x8C139E`) is non-empty (AsciiString length at +4, RW `0x8C13A5`)
     → **outcome 4**, else **outcome 3**.
   `pickOutcome` can return **only 2, 3 or 4** — read it twice from the disassembly; the only return
   sites are RW `0x8C1399` (push 2) and RW `0x8C13B0`/`0x8C13B8` (3 or 4 via `setg` + `add ecx, 3`).
3. **The dispatch** (RW `0x8C15CF`…`0x8C15E8`):
   - `cmp eax, 0; jl` (RW `0x8C15CF`/`0x8C15D1`) → money — dead (pickOutcome never returns < 0);
   - `cmp eax, 1; jle` (RW `0x8C15D7`/`0x8C15DA`) → the **LevelUpRadius branch** — dead (outcome ≤ 1
     never occurs);
   - `cmp eax, 2; je` (RW `0x8C15E0`/`0x8C15E3`) → RW `0x8C1625`: the **picking** unit's experience
     tracker (Object +0x26C, RW `0x8C162D`) when it exists gains one level
     (`gainExpForLevel(1, true, false)` RW `0x79DA0A`, RW `0x8C163B`…`0x8C1640`); the "GUI:GainRank"
     floating text is client (S-260);
   - `cmp eax, 4; jne` (RW `0x8C15E5`/`0x8C15E8`) → RW `0x8C171A` → `doMoney` RW `0x8C148B`;
   - else RW `0x8C15EE`: `TheUpgradeCenter->findUpgrade(Upgrade)` (RW `0x66F5E5`, RW `0x8C15F8`);
     a **PLAYER** upgrade (+4 == 0, RW `0x8C1605`) is given to the other's controlling player
     (`addUpgrade(u, UPGRADE_STATUS_COMPLETE, 0)` RW `0x6AEE22`, RW `0x8C1614`…`0x8C161B`). An
     unknown or object upgrade does nothing.
4. **The money** (RW `0x8C148B`): `amount = MinResource` (+0x70, RW `0x8C149E`) when
   Min == Max, else a **second, integer** roll in `[Min, Max]` (RW `0x6D328E`, line `0xDB`);
   `amount > 0` required (RW `0x8C14C8`); in a multiplayer game
   (RW `0x625456`) `amount = _ftol(amount * MultiPlayMoneyMult[live playable count − 1])`
   (RW `0x6A8630`, RW `0x642002`, PC24); then the controlling player's handicap
   (RW `0x6AA858`) and `Money::deposit(amount, ScoreKeeper (Player +0x3DC), sound)`
   (RW `0x7B18B8`), the "GUI:AddCash" text via RW `0x8C13BB` (client), and the port keeps it as
   `Economy::noteIncome`.
5. `xfer` RW `0x8C1442`: version (1, 1) then CrateCollide's RW `0x8C0760`.

So for Ennorath's data (`crate.ini`: `LevelUpChance = 100%` or `0%`, `ResourceChance = 20%/100%`,
`BannerChance = 0%/10%`, `LevelUpRadius = 0.0`/`100.0`):

- `LevelUpChance = 100%` crates → the **picker** gains a level, nothing else (ResourceChance is
  never consulted);
- `LevelUpChance = 0%` crates → the **money** branch (the player is paid `MinResource..MaxResource`,
  or the upgrade when one is set — treasure chests never set `Upgrade` in the mod), regardless of
  `ResourceChance`;
- no banner is ever given; no radius is ever applied.

That is also what the *original game* does — the port must not make `ResourceChance = 20%` pull
a chest with `LevelUpChance = 100%` into the money outcome.

## 3. The three fields, one by one

### ResourceChance (+0x6C, row RW `0xC05FC0`)

- String `0xC05F30` is referenced only by its own row (full-image scan).
- **Zero** instructions in the crate-code region RW `0x8C0600`…`0x8C1800` access a disp `0x6C`
  byte (raw scan: every byte `0x6C`, covering-instruction check, all encodings incl. SIB and
  disp32; only hits elsewhere, e.g. RW `0x8C29A8`/`0x8C29F8`, are virtual-call slots `[eax+0x6C]`
  and `add ebp, 0x6C` before `leave` at RW `0x8C0428`, both non-data). All SalvageCrateCollide
  slot functions (vtables RW `0xC715C0`, `0xC715A8`, `0xC70CF8`) and helpers
  (RW `0x8C136C`, `0x8C1419`, `0x8C148B`, `0x8C13BB`, `0x8C080B`, base ctor/dtor) were also read
  linearly: none touches +0x6C.
- Image-wide there are ~2323 disp-`0x6C` instructions; none belongs to this module's data (the
  module's data pointer only flows into the functions above, all re-read).
- **Verdict: never read.** Same in the donor: `ModuleFactory.cpp:581` parses it;
  `grep -rn ResourceChance Code/` finds only the parse table and the unnamed `+0x6C..0x70` gap.
- The audit row batch-4 #28 ("RotWK не использует", 2026-11-06) said the same, limited to this region;
  the region proof above is exhaustive for it.

### BannerChance (+0x60, row RW `0xC05F90`)

- String `0xC05F60` is referenced only by its own row.
- The region RW `0x8C0600`…`0x8C1800` contains exactly one disp-`0x60` instruction:
  RW `0x8C102C` `cmp byte ptr [eax+0x60], 0`, inside RW `0x8C1025` — the **VeterancyCrateCollide**
  path: that class's ctor RW `0x8C0FAB` sets vtable RW `0xC71554`, and its `AddsOwnerVeterancy` is
  a byte at +0x60 (its rows are RW `0xC05EB8`…, table of the Veterancy class). UnitCrateCollide
  uses +0x60 for its `UnitName` string (RW `0x8C0E24` `add eax, 0x60`, table RW `0xC05E5C`).
- All the SalvageCrateCollide slot functions re-read linearly: none touches +0x60.
- **Verdict: never read; there is no banner outcome anywhere in the crate code.** Same in the
  donor: `grep -rn BannerChance Code/` → the parse table only.

### LevelUpRadius (+0x68, row RW `0xC05FB0`)

- String `0xC05F40` is referenced only by its own row.
- The region contains exactly one read of a +0x68 field of the module data:
  **RW `0x8C16BF`** `fld dword ptr [esi+0x68]` — inside the branch at RW `0x8C168D`…`0x8C1718` of
  `executeCrateBehavior`:
  - RW `0x8C169C` `call RW 0x68B678` (GetControllingPlayer) — the filter's player;
  - RW `0x8C16A4` `mov [ebp-0x28], 0xC10E14` (a filter object on the stack, vtable RW `0xC10E14`),
    RW `0x8C16AB` `mov [ebp-0x20], eax` (the player), RW `0x8C16B3`/`0x8C16AE` → RW `0x8C16BA`
    `call RW 0xA394C0` (list push_back) after RW `0x8C1690` `mov [ebp-0x1c], 0xC7159C`
    (static list head);
  - RW `0x8C16BF` loads **+0x68 as the radius**, RW `0x8C16C6` `ecx = [0xDE4354]`
    (ThePartitionManager), RW `0x8C16CC` the radius is pushed, RW `0x8C16CF` `lea eax, [edi+0x38]`
    (the other's position), RW `0x8C16D7` `call RW 0xA39340` — a range query around the
    **picker's** position, distCalc 1, order 0;
  - RW `0x8C16E7`…`0x8C16F3`: `size = (end − begin) / 8` over an 8-byte-stride list;
    **non-zero** → RW `0x8C1419`: each hit with an experience tracker gains one level
    (RW `0x8C141B`…`0x8C1430`, `gainExpForLevel(1, true, false)` RW `0x79DA0A`);
    **zero** → push the other itself → `doMoney` RW `0x8C148B` (RW `0x8C1706`/`0x8C1707`).
  - DONOR (BFME2 0x004BD442 `executeCrateBehavior`, `case 0: case 1:`): the same branch —
    `ThePartitionManager->iterateObjectsInRange(other->getPosition(), data->m_unitRadius, 1,
    Rva00260E2AFilter(other->getControllingPlayer()).link(&Rva00260E44Filter()), 0)`; non-empty →
    each hit levels; empty → `doMoney`. The player filter allows only the picker's **own player's**
    objects (DONOR `0x00260E2A`: `m_player == object->getControllingPlayer()`), the second filter
    requires the template to have an experience-level chain (DONOR `0x00260E44`).
- **Reachability — the whole question.** The branch is entered only from RW `0x8C15D7`/`0x8C15DA`
  (`cmp eax, 1; jle`) after `call RW 0x8C136C` at RW `0x8C15CA`. `pickOutcome` returns only
  2 / 3 / 4 (§2), so `outcome ≤ 1` never happens. The preceding `jl` (RW `0x8C15CF`/`0x8C15D1`) is
  dead the same way. There is no other caller of RW `0x8C136C` in the image
  (`rw.py xref 0x8C136C` → only RW `0x8C15CA`), no other slot points into `0x8C168D`, and
  RW `0x8C158B` is only called through the vtable (no rel32 xrefs).
- **Verdict: no reachable reader.** The only instruction that reads the field sits behind a
  comparison that never branches in 2.01; BFME2 1.06 has the identical shape (DONOR
  `0x004BD281` pickOutcome returns 2/3/4, and the `case 0: case 1:` of `0x004BD442` is dead there
  too). The audit batch-4 row 19 flagged the reader RW `0x8C16BF` as "reads-logic" without the
  context check the audit's own README requires; checked in context, the branch is dead.

### PorterChance (+0x5C, row RW `0xC05F80`) — not in the issue, same picture

No instruction in the module reads it either: RW `0x8C15BA` (`cmp [eax+0x5c], 1`) is
**Player+0x5C** (the `PLAYER_COMPUTER` test), and `+0x5C` of the module data has no reader in the
region (the other disp-`0x5C` hits belong to the UnitCrateCollide class / its `Owner` checks and
to `[ecx+4]+0x5C` style reads in other classes). Audit batch-24 row 6 calls it `undetermined`;
checked the same way, it is unread as well.

## 4. What the mod sets, and what it gets (RotWK 2.01 behaviour, the port included)

`data/ini/crate.ini` (Ennorath 1.9.8.2): `SalvageCrate` sets `BannerChance = 10%`,
`LevelUpChance = 100%`, `LevelUpRadius = 100.0`, `ResourceChance = 20%`, `MinResource = 25`,
`MaxResource = 75`, `AllowAIPickup = No`. The `TreasureChest*` children set `LevelUpChance` to
`0%` (give money) with `ResourceChance = 100%`, and the level-up chests set
`LevelUpChance = 100%` with `ResourceChance = 20%`. What 2.01 does with those:

- `LevelUpChance = 100%`: every roll `< 1.0f` (the roll is in `[0.0, 1.0)`) → outcome 2 →
  the **picking** unit gains one level. Money is **never** given (ResourceChance is not
  consulted), no banner, no radius, no group level-up.
- `LevelUpChance = 0%`: outcome 4 (only if `Upgrade` set) or outcome 3 → money `25..75`.
  `ResourceChance = 100%` does not pull it into "give money" — it is already money;
  `ResourceChance = 20%` does not block it either.
- Note the parse quirk: `LevelUpRadius = 100.0` parses through the percent parser
  (RW `0x42EEFA`, `scanReal * 0.01`) into `1.0f` — the raw default from the ctor
  (`RW 0xBD88D8` = 100.0f) is the only scale the field ever holds when untouched. The dead branch
  would hand that 1.0 to the range query; another reason not to "fix" anything here.

The port does all of this already (`MoneyEventModules.cpp` parses with RW offsets,
`SalvageCrateCollide.cpp`: outcome from LevelUpChance, the self-level via
`ExperienceTracker::gainExpForLevel(1, true, false)` (RW `0x79DA0A`), the upgrade via
`TheUpgradeCenter`, the money via `Economy::applyHandicapMoney` + `Money::deposit`; the
`"GUI:GainRank"`/`"GUI:AddCash"` texts are client-only and not shown (S-260)).

## 5. Why the branch is still in the 2.01 binary, and what was NOT done

Zero Hour's `SalvageCrateCollide` rewarded the *group* (units of your player within a radius get
a level). RotWK 2.01 (like BFME2 1.06) rewrote the reward to the picker and left the group branch
unreachable — the data rows and the default `100.0f` radius remained. Ennorath inherited ZH's INI
comments ("Level + Money = 100%"), which describe the ZH behaviour, not 2.01's. The port keeps 2.01's
behaviour; the dead branch is deliberately **not** ported as live code.

What is not established / not claimed:

- The exact semantics of the RotWK-side filter objects at RW `0xC10E14` (the BFME2 donor's
  equivalents are pinned: same-player + has-level-chain). It does not matter: the branch never runs.
- Image-wide attribution of every one of the ~2323 disp-`0x6C` / disp-`0x60` instructions (audit
  rows 27/28 limit the claim to the crate region; we re-proved it for the region and for the
  module's three vtables + the string references; the module's data pointer never escapes those
  functions).
- Nothing about the client FX ("GUI:GainRank" floating text, the `ExecuteAnimation` bloop) — S-260.

## 6. Tests

`engine/tests/test_port_94.cpp` (`PORT-94*`), synthetic INI only, no game files:

1. the three fields parse into the module data (mod values; RW defaults incl. the 100.0f radius
   and the percent quirk of the parse);
2. `LevelUpChance = 100%` with `ResourceChance = 100%` → outcome 2, the picker gains a level, no
   money (`lastMoney == 0`);
3. `LevelUpChance = 0%` with `ResourceChance = 0%` → outcome 3, money 250 deposited;
4. `BannerChance = 100%` changes nothing (level with LevelUpChance = 100%; money with 0%);
5. `LevelUpRadius = 100.0`: a bystander of the picker's player within the radius does **not**
   gain a level (only the picker does; an enemy bystander does not either);
6. the outcome is never ≤ 1 (with `Upgrade` set and LevelUpChance = 0%, the outcome is 4 —
   the radius branch of RW `0x8C168D` stays unreachable).

## 7. Stop

- S-7234 (docs/STOPS.md) — the three fields are parsed and never read; no code change. Options for
  the owner: close as unused (as Port 42 / 56 / 57 / 81), or say which RotWK behaviour the mod
  wants (it already gets it: the picker levels, otherwise the player is paid).
- The audit's batch-4 row 19 ("reads-logic" for LevelUpRadius) should be re-read in context;
  rows 27/28 ("не использует") stand as written.
