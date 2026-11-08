# Port 42 / issue #127 — ScoreKeeper victory rating, RW reading and reachability stop

Date: 2026-11-08. Source: RotWK 2.01 `game.dat` (caveat S-001), read with
`tools/mod/rw.py dis|xref|str` and raw image scans. No game or mod bytes are
committed. Ennorath's merged INI set was checked locally: `GameData` writes the
three `ScoreKeeper_*Victory*` rows named below; the field audit carries them as
`parsed-unused` in the port.

## Parser rows

The binary row records were read directly (name pointer, parser, user data,
offset):

| row VA | field name | parser | GlobalData offset |
|---|---|---|---|
| `0xC01020` | `ScoreKeeper_TotalVictoryRequiredScore` | `0x42ED00` (`INI::parseReal`) | `+0x11A8` |
| `0xC01030` | `ScoreKeeper_NormalVictoryRequiredScore` | `0x42ED00` | `+0x11AC` |
| `0xC01040` | `ScoreKeeper_NormalVictoryRequiredObjectivesPercentage` | `0x42ED00` | `+0x11B0` |

The parser at `0x42ED00` calls the token reader, converts with the x87 helper
and stores a float32 (`fstp dword ptr [eax]`, `0x42ED19`). The port already has
these rows in `GameLogicSettings.cpp` and hashes them in
`GameLogicSettings::crc`.

## Objective helper used by the rating

`ScoreKeeper` objective helper `0x79DE90`:

- `0x79DE95`: reads `TheMissionObjectiveTracker` (`0xDE8C94`); no tracker ->
  total = 0, completed = 0.
- `0x79DEBD..0x79DEEA`: walks the objective list. For each entry it calls the
  shown test (`0x835F90`), bonus test (`0x83617a`), then completed test
  (`0x836079`). Only shown bonus objectives increment total; completed shown
  bonus objectives increment the completed count.
- `0x79DEF6..0x79DEF9`: writes total through the out argument.
- `0x79DEFB`: returns completed in `eax`.

This is the same shown-bonus objective set already used by the port's
`MissionObjectiveTracker::countShownBonus` and the score's objectives term.

## Rating body `0x79E175`

`esi = this` (ScoreKeeper). The function does not branch on victory conditions;
it returns a small integer rating.

1. `0x79E17D`: calls `calculateScore` (`0x79DFFA`); `edi = score`.
2. `0x79E182`: local completed = 0.
3. `0x79E18E`: calls the objective helper; `eax = completed`, local = total.
4. `0x79E193`: `ecx = TheGameData` (`0xDE4364`).
5. `0x79E199`: loads `TotalVictoryRequiredScore` (`+0x11A8`).
6. `0x79E1A1..0x79E1A3`: `edx = 2` (initial rating).
7. `0x79E1A4`: converts score to float32.
8. `0x79E1A8 / 0x79E1AD`: `comiss totalRequired, score`; if
   `totalRequired > score`, jump to `0x79E1B4` and set rating = 1.
9. `0x79E1AF / 0x79E1B2`: otherwise compare total objectives (`eax`) with
   completed objectives; if `total < completed` is false (`jge` not taken),
   fall through to `0x79E1B4` and set rating = 1. Thus rating 2 survives only
   when score is at least the total-victory threshold and every counted
   objective is completed.
10. `0x79E1B7`: loads `NormalVictoryRequiredScore` (`+0x11AC`).
11. `0x79E1BF / 0x79E1C2`: if `normalRequired > score`, jump to
    `0x79E1E2` and set rating = 0.
12. `0x79E1C4`: converts completed objectives to float32.
13. `0x79E1C9`: multiplies by `NormalVictoryRequiredObjectivesPercentage`
    (`+0x11B0`).
14. `0x79E1D1`: multiplies by the float32 constant at `0xBE5600` (0.01).
15. `0x79E1D9`: converts total objectives to float32.
16. `0x79E1DD / 0x79E1E0`: `comiss requiredObjectives, total`; if the
    required amount is <= total, keep the current rating (2 or 1); otherwise
    fall through to `0x79E1E2` and set rating = 0.
17. `0x79E1E4..0x79E1E7`: return `edx`.

The comparisons are ordered SSE comparisons (`comiss`) and the multiplications
are SSE float32 operations in the written order. The objective condition is
`completed * percentage * 0.01 <= total`, not a comparison against the score.

## Wrapper, stored fields and getter

Wrapper `0x79E218` (SEH prologue at `0x79E218..0x79E222`):

- `0x79E22C..0x79E233`: copies an `AsciiString` argument into ScoreKeeper
  `+0x310`.
- `0x79E23A`: calls `calculateScore`.
- `0x79E241`: stores the score at `+0x314`.
- `0x79E247`: calls the rating body `0x79E175`.
- `0x79E253`: stores the rating at `+0x318`.

Getter/copy helper `0x79E26D` copies the 12-byte record at `+0x310` (string
pointer plus the two integers) into an out argument via `0x6B59A9`.

Constructor initialization `0x79F093..0x79F0A0` sets `+0x310 = 0`,
`+0x314 = 0`, `+0x318 = 3`.

## Reachability checks

The rating body has exactly one direct caller in the loaded image:
`0x79E247`, inside wrapper `0x79E218`.

For `0x79E218`, its post-prologue body `0x79E222`, and getter `0x79E26D`:

- `rw.py xref` finds no direct call/jump to them.
- A raw scan of every executable section for `E8/E9/EB/0F8x` branches targeting
  those addresses finds none.
- A raw scan of the loaded image finds no absolute pointer or image-relative
  RVA to those addresses.
- They are not present in the export table.
- ScoreKeeper's vtable at `0xC314F0` has the slots `0x79EAF4` (destructor),
  `0x79DCAB` (set counting), `0x79E8B9` (class name), `0x79ECCB` (xfer); none
  is the wrapper or getter.

ScoreKeeper's retail xfer `0x79ECCB` (version window `(1, 12)`) transfers the
counters, maps, per-frame vector and fortress marks, but does not transfer
`+0x310`, `+0x314` or `+0x318`.

The score-screen entry builder `0x9270AD` reads the player's name, color, side,
victory/defeat result, per-frame vector at Player `+0x6F8` (ScoreKeeper
`+0x31C`) and fortress marks at Player `+0x704` (ScoreKeeper `+0x328`). It does
not read ScoreKeeper `+0x314`/`+0x318` (Player `+0x6F0`/`+0x6F4`). Raw scans for
the inline Player offsets `+0x6F0`/`+0x6F4` found no ScoreKeeper final-rating
reader in the score/victory UI ranges.

## Stop

The port already parses and hashes the three fields. The RW rating arithmetic
is read, but no reachable caller of the wrapper/getter and no reader of the
stored rating were found in the static RotWK image. Porting the wrapper and a
saved rating field now would add unused simulation state and a second path
without an in-scope consumer. This is registered as S-7186.

INFERENCE: a runtime-generated pointer or external hook outside the static
image was not excluded; the stop is based on the loaded `game.dat` image only.
Owner decision needed: close as unused in RotWK, or provide the missing caller
/ consumer (or approve porting the dead rating API and state without a live
consumer).
