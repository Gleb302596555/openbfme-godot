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

## 3. The 3D pass of the playing list (RW 0x45BBB3–0x45BD52)

One playing 3D sample per iteration: `esi` is the playing record, `[esi+0x1C]` its event, `[ebp-0x18]` the Miles 3D sample.

| RW | what the branch does |
|---|---|
| 0x45BBB3–0x45BBE1 | the sample has ended and playing+0x48 is set: loops left (0x6DAD59) → requeue 0x45A188; the record leaves the list |
| 0x45BBE6–0x45BBF4 | playing+0x18 == 1, or ended without +0x48: the record is released (0x45BD83) |
| 0x45BBFA | no sample: next record |
| 0x45BC07–0x45BC1E | current position 0x450EAC(event, &valid); not valid → 0x45BD57: loops left → requeue 0x45A188, the sample is ended, the record leaves |
| 0x45BC24–0x45BC4A | shroud test, manager vslot 0x1B0 on the event: true → playing+0x46 = 1 and +0x47 = 0; false with +0x46 set → +0x46 = 0 and +0x47 = 1 |
| 0x45BC4E–0x45BC79 | volume 0x45A10E(playing, 0, 0) against settings+0xB8 (MinSampleVolume) and the "audible anyway" test 0x452726(event) |
| 0x45BC7B–0x45BCA8 | below the cutoff and not audible anyway: loops left → requeue 0x45A188; the sample is ended ([0xBD0AE0]); the record leaves |
| 0x45BCAD–0x45BD04 | still audible: 0x453E1D(playing, position); with manager+0x6A9 set and info+0xA8 above [0xC1B594] → 0x455447; with manager+0x6A8 set or that call's out flag → 0x4527A8 (not read) |
| 0x45BD09–0x45BD4C | volume 0x45A10E(playing, 1, 1); position call [0xBD0B08](sample, x, y, −z); volume call [0xBD0B48](sample, volume) |

Donor guide only: `processPlayingList` (DONOR BFME2 0x5C23C, MilesAudioManager.cpp) has the same shape. Its volume helper (DONOR BFME2 0x5A9F8) is the effective volume of the event, times the fade factor of playing+0x30 when the last argument is 1, then the 0.01 floor. Its fade step (`processFade`) grows playing+0x30 by the frame time while +0x44 or +0x46 is set and shrinks it while the opposite flags are set. The RW twins of the helper (0x45A10E) and of the fade step are not read yet, so this paragraph is INFERENCE until they are.

## 4. Slider volume table (RW 0x4516DE)

0x4516DE(slider, which) fills four floats at manager + 0xC8 + (which + slider·2)·16:

- slider 5: all four are 1.0;
- byte [0xD9902C] clear: all four are 0;
- otherwise [0] = manager[4 + (which + slider·2)·4] × [0xD99014 + slider·4] × manager+0x98, times manager+0x94 when which == 1; [1] = [0] × manager+0x9C; [2] = [0] × manager[0x34 + slider·4] (the product 0x451850 builds from the playing sounds' multipliers); [3] = [2] × manager+0x9C; each is clamped to [0, 1];
- a value that changed marks its dirty byte with 2.

What manager+0x94, +0x98 and +0x9C hold is not read yet.

## 5. Event side (RW 0x6DB221 tail, 0x6DAC64, 0x6DAD59, field table 0xBEFDF0)

- 0x6DB221, owner mute: owner type event+0x38 == 1 → lookup through [0xDE4388] vslot 0x40, byte +0x44A == 0 → 0.0; owner type 2 → lookup 0x449681, dword +0x98 bit 20 set → 0.0; otherwise (override event+0x24, or info+0x10) × event+0x2C. Correction to the row in section 1: owner type 1 is the drawable and 2 the object (the order of the donor's owner enum, INFERENCE; the port's note at `AudioEventRTS.h:26` says the same). The port leaves this mute out (stop S-243).
- 0x6DAC64: delay event+0x64 = random(info+0x34, info+0x38); per-file pitch event+0x58; per-file volume event+0x60 = random(info+0x18 + 1, 1). The port's `rollDelayAndPerFile` matches.
- 0x6DAD59 (loops left): event+0x4C set → no; no info, sound type 0 or 3, or info+0x4C bit 0 → yes.
- Field table 0xBEFDF0: Volume +0x10, VolumeShift +0x14, PerFileVolumeShift +0x18, MinVolume +0x1C (parser 0x42EEFA for all four), Limit +0x3C, Priority +0x44, Type +0x48, Control +0x4C, MinRange +0x94, MaxRange +0x98, ZoomedInOffscreenVolumePercent +0x9C, SubmixSlider +0xB4, VolumeSliderMultiplier +0xB8.

## 6. Port live path (read; the comparison with sections 3 and 4 is the next step)

- `godot/scripts/game.gd` `_update_listener` hands the camera eye and target to `update_microphone` every frame (`GodotGameAudio.cpp` → `AudioManager::updateMicrophone`).
- `LiveGameAudio::update` scans the ambients. `tryStart` defers a non-global, non-critical event while the listener is at or beyond MaxRange, and marks the ambient as tried once it has asked the manager. A tried ambient is not asked again while the wanted event stays the same; its handle is not checked.
- The manager restarts a 3D loop through `completeVoice` and requeues a culled one in `processPlayingList`.
- The event log has a line for a request, a start, a failure, a drop and a cull. It has none for the gain a playing voice gets each frame, for a voice that ends, or for the request a requeue makes.
