# PORT-81: `HordeContain/RanksToJustFreeWhenAttacking` — reader search in RotWK 2.01 (issue #213)

Verdict: **no reachable reader of module data `+0x1C4` exists in RotWK 2.01**, so there is no behaviour to port. The field is
parsed and stored exactly like its neighbour `RanksToReleaseWhenAttacking` (`+0x1B8`), whose readers *are* in the binary; the
two fields differ only in that the `+0x1C4` set is never consulted. This is the same class of stop as Port 42 (`S-7186`) and
the `RadiateFearUpdate/WhichSpecialPower` verdict (`S-7193`), and the port already parses the field
(`engine/src/GameLogic/Object/Contain/HordeContain.cpp:440`, member `engine/src/GameLogic/Module/HordeContain.h:128`).

All addresses are RotWK 2.01 `game.dat`, md5 `daed3668006cd90f01c34e5a7da1901f`. Every instruction below was read with
`tools/mod/rw.py dis`.

## 1. The field belongs to `HordeContainModuleData`, and the table is the module's field table

* Field row RW `0xC5BB90`: name pointer RW `0xC5BACC` = ASCII `"RanksToJustFreeWhenAttacking"`, parser RW `0x86DED1`, user data `0`,
  offset `0x1C4`.
* Its neighbours in the same table are the other `HordeContain` module-data rows, all with the documented offsets:

  | row | name string | parser | offset |
  |---|---|---|---|
  | RW 0xC5BB70 | `RanksThatStopAdvance` | RW 0x86DF0B | +0x1B4 |
  | RW 0xC5BB80 | `RanksToReleaseWhenAttacking` | RW 0x86DED1 | +0x1B8 |
  | RW 0xC5BB90 | `RanksToJustFreeWhenAttacking` | RW 0x86DED1 | +0x1C4 |
  | RW 0xC5BB60 | `RankInfo` | RW 0x877385 | +0x18C |
  | RW 0xC5BBA0 | `ComboHorde` | RW 0x872654 | +0x198 |
  | RW 0xC5BBD0 | `LeaderPosition` | RW 0x42F247 | +0x200 |

  The shared parser RW `0x86DED1` is the `set<int>` inserter of the donor `parseHordeContainRankSets`
  (DONOR BFME2 0x0046E8xx `Code/GameEngine/Source/GameLogic/Object/Contain/HordeContainParseRankCollections.cpp:62`), matching
  the port's `HordeContainModuleData::parseRankSet`.
* The table is handed to the class by `HordeContainModuleData`'s parse entry RW `0x878B63` (`push 0xC5BB50` at RW `0x878B73`,
  `call 0x42B8D7` = `INI::initFromINIMulti`) — i.e. RW `0xC5BB50` is *the* `MultiIniFieldParse` of this module data, and
  `+0x1C4` is a member of the object the module reads (`HordeContain` finds its `HordeContainModuleData` at module `-0x118`,
  RW `0x87525E` / RW `0x87549B` / RW `0x875582`, which is how the port's `hordeData()` maps).

## 2. What the class does with the two sets: `+0x1B8` is read at exactly three places, `+0x1C4` at none

Constructor / copier / destructor treat the two sets identically (both are STL sets):

* data constructor RW `0x878F4E` (`lea ecx,[esi+0x1B8]`, `call 0x46C1DA`) and RW `0x878F5D` (`lea ecx,[esi+0x1C4]`, `call 0x46C1DA`) —
  the same inline set default-constructor.
* copy/lifecycle RW `0x878C86` → RW `0x4728D3`, RW `0x878C91` → RW `0x4728D3` (the pair for `+0x1B8` / `+0x1C4`),
  RW `0x878DA6` → RW `0x472980`, RW `0x878D97` → RW `0x472980`.
* the second constructor-like function RW `0x878F10 .. 0x878F67`: RW `0x878F4E` / RW `0x878F5D` again, then
  RW `0x878F58` → `+0x1B8` and RW `0x878F67` → `+0x1C4` with the same callee.

The *readers* of `+0x1B8` (all inside `HordeContain`, all taking the module data at `[edi-0x118]` / `[esi-0x118]`):

| function | address of the `+0x1B8` use | role in the port |
|---|---|---|
| `HordeContain::attackTargetNow` (slot 4, RW 0x875221) | RW `0x875259` → RW `0x875264` `add eax,0x1B8` (`mov eax,[edi-0x118]; add eax,0x1B8`) | `HordeMemberPass.cpp` `memberRankReleased` |
| `HordeContain::attackPositionNow` (slot 0, RW 0x87545C) | RW `0x87549B` / RW `0x8754A3` `add esi,0x1B8` | `HordeContain::attackPositionNow` |
| third `HordeContain` slot (RW 0x875562) | RW `0x875582` / RW `0x87558A` `add esi,0x1B8` | (same pattern, rank → member map) |

The `+0x1C4` set is never looked up: in the whole `HordeContain` region (RW `0x860000 .. 0x890000`) the only instructions that
name `+0x1C4` off a module-data pointer are the three constructor/copy sites above. Beyond them, all `+0x1C4` instruction
operands in the image are someone else's offset (AI vtable slot `[eax+0x1C4]`, e.g. RW `0x875327`, is a virtual call, not a data
read — see §4).

## 3. The exhaustive offset scan (no unaligned/8-bit form is missed)

Two independent scans over the complete `.text` (RW `0x1000 .. 0x7D0000`, 43 078 decoded instructions of the linear sweep, plus
a byte-by-byte resynchronisation sweep so that no instruction boundary is trusted):

1. the raw 4-byte little-endian pattern `C4 01 00 00` is searched in the whole `.text`; each hit is decoded from the 1..11
   possible instruction starts that end at it, and the decoded operand is inspected. 185 real instruction sites carry `0x1C4`;
   they are listed by category in §4.
2. every decoded instruction whose operand names `+ 0x1C4` (disp32 *and* disp8 forms, stack-relative operands excluded) is
   collected and grouped by address.

Both scans agree, and neither finds a `HordeContainModuleData`-relative access other than a constructor/destructor/copy of the
set. In particular there is **no** `mov`/`push`/`cmp` that loads the set's head pointer from a module-data pointer at `+0x1C4`.

## 4. Every other `+0x1C4` in the binary is a different object

Category by instruction shape (from scan 1):

* `call dword ptr [eax+0x1C4]` (RW 0x47A7AD, 0x4C4E38, 0x6658E7, 0x669767, 0x66C7D0, 0x69638D, 0x69676B, 0x6A46AC, 0x6C..., 0x76FD50,
  0x79..., 0x7FCF65, 0x874792, 0x875327, 0x8759C5, 0x875D44, 0x8762F4, 0x877B53, 0x879C36, 0x9E689B) — **virtual calls**: AIUpdate
  slot `+0x1C4` (`isBusy`, ported as `ai->isBusy()` at RW `0x875327`, `HordeMemberPass.cpp`). A vtable slot is not a member read.
* `lea ecx,[..+0x1C4]` on function-local or unrelated objects: RW 0x4CAAE0, 0x54212C, 0x5FD91D, 0x5FDF14 / 0x5FDF1A, 0x5FE2CD / 0x5FE2D4,
  0x62CD0F, 0x630296, 0x69364D, 0x7848FF, 0x784A18 / 0x784A1F, 0x7855A1 / 0x7855A7, 0x903279, 0x903560, 0x95C8A0.
  Of these, RW `0x903260` (prologue RW `0x903500`-region class) and RW `0x9036xx` belong to a **different class** that keeps a
  three-word pointer triple at `+0x1BC .. +0x1C8` (RW `0x903522` `mov [esi+0x1C4],edi`, RW `0x9035C?` `call 0x435D50` = a
  `std::string`-like release); RW `0x911455` reads the same `+0x1C4/+0x1C8` pair as **two pointers into a global at RW 0xDE412C**
  (`mov eax,[esi+0x1C4]; mov edx,[esi+0x1C8]; add eax,[global+0x40]`) — a coordinate/stream object, not module data.
* `mov dword ptr [esi+0x1C4], edi` at RW `0x903523` is a store of an integer 0x1C4 into a field of that other class (the same
  trap as the `mov eax,0x1C4` immediates RW `0x5F64C6`, 0x4CBAB3, 0x79F61D, 0x903523, 0x95F577, 0x9CCD22, 0x9CCD34).
* `push dword ptr [.. +0x1C4]` at RW `0x505DC8`, 0x881988 (member of a **different** class: RW `0x881960` reads
  `[esi+0x1BC]`, `lea eax,[esi+0x1B8]`/`+0x18C` stride 0x18 = its own struct array, and `[esi+0x1C4]` as a scalar), 0x881B24,
  0x95F5A1.
* `imul eax,eax,0x1C4` / `add reg,0x1C4` / `sub esp,0x1C4` (RW 0x451C72 .. 0x461F9C, 0xA76B8A, 0x80197B, 0xB766B2 .. 0xBA8127) —
  structure strides of unrelated arrays and stack frames.

None of them is preceded by a load of `HordeContain`'s module data (`[reg-0x118]` for a module `this`, `[module+0x2C8]->...`), which
every real `HordeContainModuleData` use in this class has (see §2).

## 5. What "just free" would mean, and why nothing in RW 2.01 can use it

In the ported attack path the released ranks (`+0x1B8`) are the only set consulted: RW `0x875221` builds the member map
(RW `0x865598`), looks the member's slot up (RW `0x8752D3` → RW `0x6B4E57`), takes the slot's rank (`imul eax,eax,0x1C` and
`mov eax,[eax+ecx]`, RW `0x87536C`/`0x875370` region of `attackTargetNow`; slot record stride `0x1C`, rank at `+0` — ported as
`HordeContainCore::Slot::rank`) and tests it against `+0x1B8` (RW `0x8754EE`, `call 0x6B4E57` on the `+0x1B8` set). The same
lookup appears in `attackPositionNow` (RW `0x8754A3`) and in the third slot (RW `0x87558A`). A second set that changed the *state*
of a member (rather than its order to attack) would have to be read next to those three lookups, or in the member/move hub
RW `0x87468B`; the move hub and its near arm (RW `0x874F09 .. 0x8751C1`, ported in `HordeMemberPass.cpp`) read `+0x2A0`,
`+0x2C8`, the slot records and the rank spine, and never `+0x1C4`. There is therefore no branch in 2.01 that can distinguish a horde
with `RanksToJustFreeWhenAttacking = 1 2` from one without the line.

The BFME2 1.06 decomp has the same shape: the field table rows are identical
(DONOR `Code/GameEngine/Source/Common/ConstIntGettersAddr.cpp:789-790`), and `HordeContain::attackTargetNow`
(DONOR 0x00471FFA, `Code/GameEngine/Source/GameLogic/Object/Contain/HordeContainIface11CSlots.cpp:4122ff`) consults only the
module data's `+0x1B8` map; `HordeContainIface11CSlots.cpp` never mentions `+0x1C4` as module data (its `+0x1C4` hits are the AI
vtable slot). So the second set is dead in the donor line as well — no BFME2 code exists to port, which removes the "the donor has
it, confirm in RW" route.

## 6. Data check (Ennorath 1.9.8.2)

`RanksToJustFreeWhenAttacking` occurs 82 times in the mod data with the values `0`, `1 2` and `1 2 3 4`
(issue #213, `field-audit-v6` row 2.10). Because the value cannot change any branch, those hordes behave in game.dat exactly like
hordes without the line — which is what the port already does.

## 7. Verdict

* RW: no reader → nothing to port (stop `S-7213`, `docs/STOPS.md`).
* The port keeps the parse line and the store; no second path is added, no state, no xfer change (`+0x1C4` is module *data*, not
  simulation state).
* Tests `PORT-81*` (`engine/tests/test_port_81.cpp`) pin that the field parses like `RanksToReleaseWhenAttacking` and that the
  attack path is unchanged by it (a horde with `1 2` just-free and `RanksToReleaseWhenAttacking = 1 2` behaves identically to one
  without the just-free line; `0` is not rank 0 either).
