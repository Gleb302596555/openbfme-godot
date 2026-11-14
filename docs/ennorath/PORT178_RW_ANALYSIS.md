# Port 178: RW analysis (fork issue 431, P-76: Isengard's tainted-ground ambient is not heard)

Status: read. The fixes of section 11 are in the same branch (tests `PORT-178*`, `engine/tests/test_port_178.cpp`); what stays unported is stop S-7343. Every address below was read with `tools/mod/rw.py workspace/RotWK/game.dat dis` (RotWK 2.01 `game.dat`, md5 `daed3668...`).
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

## 2. Reading order

Sections 1 and 3 to 6 were read first (the volume chain, the 3D pass, the sliders, the event side). Sections 7 to 10 were read next: the microphone, the helpers of the 3D pass, the channel steal and the drawable's ambient. Section 11 lists what differs from the port.

## 3. The 3D pass of the playing list (RW 0x45BBB3–0x45BD52)

One playing 3D sample per iteration: `esi` is the playing record, `[esi+0x1C]` its event, `[ebp-0x18]` the Miles 3D sample.

| RW | what the branch does |
|---|---|
| 0x45BB7D | 0x452C50 on the playing record (called before the tests below; not read) |
| 0x45BB82–0x45BBA4 | `[ebp-0xd]` = "fully faded out": playing+0x30 >= (float) AudioSettings+0x78 (TimeToFadeAudio), and neither playing+0x45 nor playing+0x47 is set |
| 0x45BBB3–0x45BBE1 | fully faded out and playing+0x48 is set: loops left (0x6DAD59) → requeue 0x45A188; the record leaves the list |
| 0x45BBE6–0x45BBF4 | playing+0x18 == 1, or fully faded out without +0x48: the record is released (0x45BD83) |
| 0x45BBFA | no sample: next record |
| 0x45BC07–0x45BC1E | current position 0x450EAC(event, &valid); not valid → 0x45BD57: loops left → requeue 0x45A188, the sample is ended, the record leaves |
| 0x45BC24–0x45BC4A | shroud test, manager vslot 0x1B0 on the event: true → playing+0x46 = 1 and +0x47 = 0; false with +0x46 set → +0x46 = 0 and +0x47 = 1 |
| 0x45BC4E–0x45BC79 | volume 0x45A10E(playing, 0, 0) against settings+0xB8 (MinSampleVolume) and the "audible anyway" test 0x452726(event) |
| 0x45BC7B–0x45BCA8 | below the cutoff and not audible anyway: loops left → requeue 0x45A188; the sample is ended ([0xBD0AE0]); the record leaves |
| 0x45BCAD–0x45BD04 | still audible: 0x453E1D(playing, position); with manager+0x6A9 set and info+0xA8 above [0xC1B594] → 0x455447; with manager+0x6A8 set or that call's out flag → 0x4527A8 (not read) |
| 0x45BD09–0x45BD4C | volume 0x45A10E(playing, 1, 1); position call [0xBD0B08](sample, x, y, −z); volume call [0xBD0B48](sample, volume) |

The helpers of this pass are in section 8. The fade step itself (which grows playing+0x30 and sets playing+0x48) is not read in RW; the donor's `processFade` (DONOR BFME2, MilesAudioManager.cpp) is the only guide to it, so what sets +0x48 stays INFERENCE.

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

## 7. The microphone (RW 0x45235B–0x452725)

`ebx` is the audio manager, `esi` the microphone row of the current view (AudioSettings + 0x12C + view x 0x48).

| RW | what the instructions do | port (`AudioManager::updateMicrophone`) |
|---|---|---|
| 0x45237F–0x4523A5 | camera position through manager vslot 0x1A4; a 0x50 byte record (0x451F59) filled through vslot 0x1A8: five entries of 0x10 bytes, a point and a "valid" byte each; entry 0 is the look-at point, entries 1 to 4 are used for the edge lengths below | the caller passes the camera and the look-at (`game.gd` `_update_listener`) |
| 0x4523A8–0x4523FB | camera equal to the cached one (manager+0x30, 0x403270) and the record equal to the cached one (manager+0x3C, 0x4514F9): return, nothing changes | recomputed every call (same result) |
| 0x4523FD–0x45244F | both are cached; d = camera − look-at | same |
| 0x452451–0x4524D8 | row+4 x d² >= row+0x14 → s = row+0x10 / length; else row+0xC >= d² → s = 1.0 (0xBD1908); else row+0xC >= row+4 x d² → s = row+8 / length; else s = row+0 | same four cases |
| 0x4524D8–0x45255B | microphone = camera − d x s; with the look-at valid (manager+0x48): x and y move towards the look-at by row+0x18, z stays; stored at manager+0x18 .. +0x20 | same |
| 0x45255B–0x452599 | d has a horizontal part: face = (−d.x, −d.y, 0) normalised (0x405553), stored at manager+0x24 .. +0x2C; else the old face stays | same |
| 0x45259E–0x452605 | with a listener (manager+0x9E0): orientation call `[0xBD0B04]`(listener, face.x, face.y, −face.z, 0, 0, −1.0 (0xBD19DC)); position call `[0xBD0B08]`(listener, x, y, −z) | `MilesMix` uses the same frame |
| 0x452605–0x45264F | e = camera − microphone; 0x451946(e, row) on the view's volume set (manager + 0x12C + view x 0x1C4) | the zoom volume |
| 0x45264F–0x452701 | the shortest squared edge between neighbouring valid entries 1 to 4; >= row+0x3C → manager+0x8C = 0.0; row+0x44 >= it → 1.0; else (row+0x38 − length) / (row+0x38 − row+0x40) | supplied through `m_queries.zoomFadeFactor` |

No divergence in the microphone position.

## 8. Helpers of the 3D pass

| RW | what the instructions do | port |
|---|---|---|
| 0x45A10E (playing, applySliders, applyFade) | 0x4591D4(event, applySliders); with applyFade == 1 the result is multiplied by 0x450A18(playing+0x30); then the 0.01 floor of section 1 (event+0x52) | `getEffectiveVolume` always applied the sliders |
| 0x45BC4E | the cull calls it with (0, 0): the volume **without** the sliders and without the fade | the port compared the volume with the sliders |
| 0x45BD09 | the volume given to the sample calls it with (1, 1) | sliders yes, fade not ported |
| 0x452726 (event) "audible anyway" | true when info+0x48 has bit 3 (type GLOBAL), or info+0x44 == 4 (priority CRITICAL), or info+0x1C (MinVolume) >= AudioSettings+0xB8 (MinSampleVolume) | the port had the first two tests only |
| 0x452003 (event), manager vslot 0x1B0 | true ("shrouded") only when event+0x30 == 0 (tactical view), info+0x48 has bit 2 (type SHROUDED), the position is valid (0x6DB31E), `[0xDE4928]` and `[0xDE4358]` exist, and 0xB4D9A0(`[0xDE4358]`, index at `[[0xDE4928]+0x10]+0x54`, position) is not 0 | `canPlayNow` asks the same at the request; a playing sound is not asked again |
| 0x450EAC (event, &valid) | not positional (0x6DAD89) → valid = 0 and a zero point; else 0x6DB31E | `getCurrentPosition` |
| 0x45A188 (playing) requeue | 0x6DAC64 (new delay and per file rolls); loops left (0x6DAD59) → 0x6DB1DE, 0x6DAD2E(`[0xD9F620]` + 1.0, 0xBD3A60), event+0x4B = 1, 0x4593EA queues the request | the cull did this inline; now one helper |

Callers of the requeue 0x45A188 (`rw.py xref`): 0x45BA40, 0x45BBD4 (faded out), 0x45BC90 (cull), 0x45BD69 (no position), 0x45BEC7, 0x45FE4B, 0x460084 (channel steal), 0x46179F (no channel at play), 0x461A51. The callers 0x45BA40, 0x45BEC7, 0x45FE4B and 0x461A51 are not read.

## 9. Who gets a channel when the samples are all taken

`canPlayNow` is RW 0x45CB3B; its tail:

| RW | what the instructions do | port |
|---|---|---|
| 0x45CB94–0x45CBE5 | squared distance from the microphone against the range (type GLOBAL: AudioSettings+0x74, else info+0x98): at or beyond → refuse | same |
| 0x45CBE7–0x45CBFA | shroud test vslot 0x1B0: true → event+0x4F = 1, refuse | same refusal; +0x4F not ported |
| 0x45CC02–0x45CC19 | 0x456885 (the object is speaking) → the answer is info+0x4C bit 3 (INTERRUPT) | same |
| 0x45CC1B–0x45CC25 | 0x456D8B (Limit) → refuse | same |
| 0x45CC27–0x45CC37 | INTERRUPT, or not sound type 2 → play | same |
| 0x45CC39–0x45CC60 | positional: playing 3D count (manager+0x688) below the 3D sample count (+0x680) → play; else 2D count (+0x684) below +0x67C → play | same |
| 0x45CC62–0x45CC6C | 0x45B4E8 (a "lower" sound is playing) → play | **different rule, below** |
| 0x45CC72–0x45CC8C | else refuse (INTERRUPT: 0x454095) | same |

The order RW uses, 0x450E81(a, b) on two (priority, volume) pairs: a is lower than b when a.priority < b.priority, or the priorities are equal and b.volume > a.volume.

| RW | what the instructions do | port before this change |
|---|---|---|
| 0x45B4E8 | new = (info+0x44, 0x4591D4(event, 1)); true when any playing record of the event's list (positional: manager+0xA44, else +0xA40) has (its priority, 0x45A10E(playing, 1, 1)) lower than new. No test for the lowest priority | ZH rule: priority only, and a request of the lowest priority never takes a channel |
| 0x45B446 | the same walk; keeps the lowest record seen (each candidate must be lower than the best so far, which starts as the new event's pair); returns its event or null | ZH rule: the lowest priority below the new one |
| 0x45FF7F | positional: 0x45E98F (it walks the 0x48 byte records at manager+0xBD4, count +0xBD8, and calls 0x45E1A5 on each one 0x450907 accepts; those two are not read), then a non-empty list at manager+0xA3C returns true at once; victim = 0x45B446; none → false; the victim's playing record: **loops left (0x6DAD59) → requeue 0x45A188**; release (0x45F4E0), unlink (0x4542FD); true | the victim was released for good, with no log line |
| 0x4616B0–0x4616D1 | play, 3D: sample = 0x453DF2; none → 0x45FF7F(event), then 0x453DF2 again | same shape |
| 0x461786–0x46179F | no sample after all: loops left → requeue 0x45A188 | same |

## 10. The drawable's ambient sound

`esi` is the drawable; +0x100 its id, +0xFC its object, +0x110 the ambient event's info, +0x144 the playing handle.

| RW | what the instructions do |
|---|---|
| 0x6746FD | stop: TheAudio (`[0xDE42FC]`) vslot 0x6C on the handle at +0x144; the handle becomes 1; the info at +0x110 is dropped (0x4A86CB) |
| 0x67866C (damage state, onlyIf) | stop (0x6746FD); the state's ambient info (0x678578); with one, and when onlyIf is set only if 0x5D8FDD(info) is true: an event owned by the drawable id (0x6DB62D), TheAudio vslot 0x64, the handle goes to +0x144 and the info to +0x110; at the end +0x44B = 0. **No distance test here**: the request always goes to the audio manager |
| 0x678721 (onlyIf) | needs +0x447, +0x448 and +0x44A all set; stop; the damage state from the object's body (+0xFC → +0x25C, vslot 0x24); 0x67866C(state, onlyIf) |
| 0x67876B (state) / 0x6787DA | same three flags; a state whose info differs from +0x110 → 0x67866C(state, 0) |
| 0x678901 / 0x678926 / 0x678957 | setters of +0x447 / +0x448 / +0x449: on → 0x678721(0) (0x678926 also 0x678816), off → stop |
| 0x67896F | +0x44A was 0: it becomes 1, 0x678816, 0x678721(0), then the modules of +0x154 |
| 0x674727 | +0x44A was 1: it becomes 0, 0x6717C4, stop, then the modules of +0x154 |
| 0x678F96 | +0x44B set → 0x678721(0). +0x44B is 1 from the constructor (0x679E66) and is cleared only by 0x67866C. Called from 0x6285E3 for every drawable (first: `[0xDE4388]` vslot 0x8C, next: +0x104) |
| 0x690728 (object) | object+0x98 bit 20 set; the drawable (+0x84) → 0x674727. Callers: 0x6923A5 (which then sets status 0x33), 0x990F94, 0x990FDD |
| 0x690772 (object) | object+0x98 bit 20 cleared; the drawable → 0x67896F. Caller: 0x692409 (which then clears status 0x33) |

So the two owner mutes of `getVolume` (section 1: drawable+0x44A == 0, object+0x98 bit 20) are one state, switched by 0x690728 / 0x690772. What the callers mean (status 0x33) is not read.

## 11. What differs from the port

| # | RW | port before | effect | this change |
|---|---|---|---|---|
| 1 | 0x45FF7F: a looping sound that loses its channel is requeued | released for good, no log line; the victim was the first sound of the lowest priority in the list (the ZH search returns at once on it), that is the oldest one | an object's looping ambient is requested once (the drawable starts it once), so one steal silenced it for the rest of the game; an ambient of the lowest priority started on the first frame is the first victim | fixed |
| 2 | 0x45B4E8 / 0x45B446 / 0x450E81: (priority, volume) order, no exception for the lowest priority | priority only; the lowest priority never takes a channel | with the samples all taken, a looping ambient of the lowest priority waited for ever | fixed |
| 3 | 0x45BC4E: the cull uses the volume without sliders; 0x452726 has three tests | with sliders; two tests | a low sound slider culled sounds RW keeps | fixed |
| 4 | 0x45BD57: a playing 3D sound without a position is requeued when loops are left | released, no log line | the port then drops a loop whose owner is gone at its next check (lane AUDIO-3) | log line only, S-7343 |
| 5 | 0x45BC24 with 0x452C50 and 0x45BBB3: a playing sound that becomes shrouded fades out and is requeued | not ported | the port keeps playing it (louder than RW, not a silence) | open, S-7343 |
| 6 | 0x6DB221 owner mute (section 10) | not ported (stop S-243) | the port plays what RW mutes | open |
| 7 | 0x67866C: no distance test at the drawable | `LiveGameAudio` waits until the listener is inside MaxRange, then requests once | same result once requested | open |
| 8 | 0x45FF96: 0x45E98F before the steal (a walk over the records at manager+0xBD4) | not ported | its two callees are not read; by its place it can free a sample so that no victim is needed (INFERENCE) | open, S-7343 |

Not measured here: the live voice of the owner's game (this checkout has no Godot build and no game audio). The audio log now writes the steal, the end of a voice and the level of every looping 3D voice, so the next owner log shows which of these paths the citadel's ambient takes.
