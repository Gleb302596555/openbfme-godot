# Ennorath game.dat patches

## `upgrade_limit_guard.py`: stop at RotWK's 1152-upgrade limit (fork issue 514, Local 69)

**What it does.** RotWK 2.01 gives every distinct `Upgrade <name>` one bit in a fixed 0x24-dword (1152-bit) mask. `UpgradeCenter::newUpgrade` (RW 0x66FC27) hands out the next bit without a ceiling. From upgrade number 1153 on, every mask that contains that upgrade gets written past its end: into the neighbouring mask, or into `Object+0x31C`, the object's `Team *`. pySAGE describes this in `sage_patch/docs/upgrade-mask-limit.md`.

This patch keeps the limit. When the bit would be 1152 or more, the game stops loading with a message box naming the upgrade:

> Too many upgrades: the INI defines upgrade 'X' as number 1153, but RotWK's upgrade masks hold only 1152.

Then `ExitProcess(1)`, so memory is never corrupted. Below the limit the game runs exactly as before (same bit, same counter), so saves, replays and multiplayer with unpatched players still work.

The limit is not raised. The analysis in fork issue 514 explains why there is no safe way to raise it:
- the masks sit mid-struct;
- they are built inline by STLport `bitset` code in hundreds of places, including stack temporaries and whole-struct copies;
- an extension kept outside the masks would silently lose or leak bits.

**Ennorath 1.9.8.3 budget.**
- At start-up: 3 veterancy bits (RW 0x66FDD3) + 1095 INI blocks = **1098 of 1152**. The 1095 blocks: `default/upgrade.ini` 17, `upgrade.ini` 402, `includes/upgrade.inc` 428, `createaheroupgrades.inc` 248.
- Maps add their own upgrades on load: `map mp fornost siege` +11, the seven `randomizer game of chances` maps +16 between them (shared names).
- The counter never goes back during a session (`UpgradeCenter::reset`, RW 0x63F3BF, is a bare `ret`). Worst case in one session: **1125**, which leaves **27 free** bits.

**The sites.**

| where | original | patched |
|---|---|---|
| RW 0x66FCB7 (9 bytes) | `8B 47 10` `mov eax,[edi+0x10]` / `89 46 38` `mov [esi+0x38],eax` / `FF 47 10` `inc dword ptr [edi+0x10]` | `jmp .upglim` + 4 `nop` |
| new section `.upglim`, after pySAGE's `.cmdext` (VA 0xED7000, 0x800 bytes, RWX like `.spstore`) | — | the three instructions behind `cmp eax,0x480 / jae`. Then `jmp 0x66FCC0`, or `wsprintfA` (IAT 0xBD07F0), `MessageBoxA` (IAT 0xBD07FC, `MB_ICONERROR` / `MB_TOPMOST` / `MB_SYSTEMMODAL`), `ExitProcess(1)` (IAT 0xBD0268) |

- The name comes from `newUpgrade`'s `AsciiString &` argument (`[ebp+8]`). Its characters start 8 bytes into its data, the same way RW 0x66F6EE .. 0x66F6FA reads it.
- Nothing else changes. pySAGE's sections, its hooks and the PE checksum (which pySAGE also leaves alone) stay byte-identical.

**Apply.**

```
python tools/ennorath_patch/upgrade_limit_guard.py <Ennorath 1.9.8.3 game.dat> <game-upgrades.dat> [--check]
```

The script refuses before writing anything if:
- the input's md5 is not `9e999515651ab72609c621d8847a8646`;
- the 27 bytes around the hook differ from the contract;
- the three IAT slots import something else;
- the last sections are not pySAGE's four;
- `.upglim` already exists.

So applying it twice, or to another file, fails. `--check` also prints the hook site before and after, the new section's code and a per-section comparison. It needs `capstone`.

**Install / undo.** Keep a copy of the original `game.dat`. Put the patched file in its place under the name `game.dat`. To undo, put the copy back.

**Tests.**

```
python -m pytest -q tools/ennorath_patch/
```

- The tests build a synthetic PE that carries only the quoted contract. They cover the refusals and the section's placement.
- With `unicorn` installed, they run the hooked x86 below and above 1152, and with an empty and a 600-character name.
- With `ENNORATH_1983_GAME_DAT=<file>` set, they also patch the real file in memory.

**In-game check.**
1. With the patched `game.dat`, a skirmish and a save / load work as before, with 60 FPS and the chat hotkeys.
2. Over the limit:
   - Extract Ennorath's `data/ini/upgrade.ini` from `____enrth_data.big`.
   - Run `python tools/ennorath_patch/make_test_ini.py 200 upgrade.ini > <folder>/data/ini/upgrade.ini`.
   - Start the game with `-mod <folder>`.
   - The patched game stops with the message naming `Test_0055` as number 1153.
   - Remove the folder afterwards.
