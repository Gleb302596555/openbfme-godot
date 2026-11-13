# Port 431: RW analysis (fork issue 431, P-76: Isengard's tainted-ground ambient is not heard)

Status: in progress. Every address below was read with `tools/mod/rw.py workspace/RotWK/game.dat dis` (RotWK 2.01 `game.dat`, md5 `daed3668...`).
Mod data (event, template, AudioSettings rows) is not quoted here: see fork issue 431 and P-76 in `PROBLEMS.md`.

## 1. The volume of an event: RW 0x4591D4 (`getEffectiveVolume`: `this` = the audio manager, arguments = event, applySliders)

| RW | what the instructions do | port (`GameAudio.cpp`) |
|---|---|---|
| 0x4591DF .. 0x45920A | view gate: event + 0x30 (0 or 1) against manager + 0x678; a mismatch returns 0.0 | `getEffectiveVolume`, same |
| 0x45920E .. 0x459222 | volume = `getVolume` (RW 0x6DB221) x event + 0x60 x event + 0x5C (RW 0x6DA664) | `getVolume() * getVolumeShift()` |
| 0x6DB221 .. 0x6DB26F | `getVolume` returns 0.0 when the owner is an object (event + 0x38 == 1, TheGameLogic vslot 0x40) whose byte + 0x44A is 0, or a drawable (== 2, RW 0x449681) whose + 0x98 has bit 20 set | to compare next |
| 0x459225 | positional test RW 0x6DAD89; a non positional event goes straight to the sliders | same |
| 0x459232 .. 0x45924B | volume > 0.0 (0xC1B594): volume x the distance factor RW 0x453227 | same |
| 0x453227 .. 0x4532FC | distance factor: no position -> 0.0; d = manager + 0x18 .. + 0x20 (the microphone) - position, 3D length (RW 0x403111); type bit 8 (info + 0x48) takes AudioSettings + 0x70 / + 0x74, else info + 0x94 / + 0x98; dist >= max -> 0.0; dist > min and max > min -> (max - dist) / (max - min); else 1.0 | same (3D distance from the microphone) |
| 0x45924E | minVolume = RW 0x6DB2A5 (the same owner gates) | same |
| 0x459258 .. 0x45937B | zoom fade: only with manager + 0x8C > 0 and (volume > 0 and info + 0x9C != 1.0, or minVolume > 0 and info + 0xA0 != 1.0), a valid position that vslot 0x1AC reports off screen: f = (RW 0x45305C distance^2 >= view row + 0x160 ? 1 : sqrt / view row + 0x15C) x manager + 0x8C; volume x (1 - (1 - info + 0x9C) f), minVolume x (1 - (1 - info + 0xA0) f) | same (through `m_queries`) |
| 0x45937D .. 0x459388 | volume = max(volume, minVolume) | same |
| 0x45938D .. 0x4593AB | applySliders != 0: volume x RW 0x458825 on the view's volume set (manager + 0x12C + view x 0x1C4) | `sliderFactor` |
| 0x458825 .. 0x4588B6 | the slider value is a table row: index = variant + 4 x (positional or sound type 3 + 2 x slider (RW 0x5D90C3)); variant bit 0 = the event is not in the set at + 0xA0, bit 1 = the info has no slider multipliers (+ 0xB8 == + 0xBC); value at + 0xC8 + 4 x index | `sliderFactor` |
| 0x4593AE .. 0x4593DB | event + 0x52 set, volume < 0.01 (0xBE5600) and the info is not of sound type 2 (RW 0x5D9168: type + 0xB0; a multisound, type 5, asks its parts): volume = 0.01 | not in the port; does not apply to a sound effect (type 2) |

## 2. Next (not read yet)

- `getVolume` tail (RW 0x6DB271 ..) and the two shift factors (event + 0x5C / + 0x60) for `Volume` above 100 and the per file shift;
- the owner gates of `getVolume` (object + 0x44A, drawable + 0x98 bit 20) for a structure's ambient (RW 0x67866C start);
- the 3D sample path (RW 0x45BC6D cull, 0x45C047 sample preparation) and the microphone (RW 0x45235B) against the live inputs (`godot/scripts/game.gd` -> `update_microphone`).
