# Port 214 — RW analysis: what makes one voice event quieter than another (P-134)

Issue: fork issue 504. Game file: vanilla RotWK 2.01 `game.dat`, md5 `daed3668…`. Every address below was read with
`tools/mod/rw.py dis` / `xref` on that file in this session; no fact here comes from the BFME2 decompilation.

## 1. The data side: how the INI numbers become a volume

The `AudioEvent` field table is RW `0xBEFDF0` (26 rows, `{name, parseFn, userData, offset}`):

| field | parse | info offset | units |
|---|---|---|---|
| `Volume` | `0x42EEFA` | `+0x10` | percent → real: `0x42EB18` = `scanReal(token) * 0.01` (`fmul [0xBE5600]` = 0.01) |
| `VolumeShift`, `PerFileVolumeShift`, `MinVolume`, `PlayPercent`, `LowPassCutoff`, `ZoomedInOffscreen*`, `ReverbEffectLevel`, `DryLevel` | `0x42EEFA` | `+0x14`, `+0x18`, `+0x1C`, `+0x30`, `+0x90`, `+0x9C`, `+0xA0`, `+0xA8`, `+0xAC` | percent → real (same function) |
| `MinRange`, `MaxRange` | `0x42ED00` | `+0x94`, `+0x98` | plain real (`0x42EAAD`, no `0.01`) |
| `Limit` | `0x42EC5E` | `+0x3C` | int |
| `SubmixSlider` | `0x42E956` | `+0xB4` | index list `0xD9DB94` |
| `VolumeSliderMultiplier` | `0x5D9620` | `+0xB8` | vector of `{slider, multiplier}` |

So an INI `Volume = 75` is stored as **0.75**, not 75: the port's `INI::parsePercentToReal` is the same rule.

Constructor RW `0x5D992D`: `+0x10` (volume) = `[0xBD88D8]`, `+0x14..+0x2C` = 0, `+0x30` (PlayPercent) = 1.0,
`+0x3C` (Limit) = 0x19 (25), `+0x40` = -1, `+0x44` (Priority) = 2 (`AP_NORMAL`), `+0x94` (MinRange) = `[0xBD88D8]`,
`+0x98` (MaxRange) = `[0xBD4388]`, `+0xB4` (SubmixSlider) = -1.

Post-parse RW `0x5D9FF3`-`0x5DA0DA` (after the fields are read): `RANDOMSTART` is cleared for sound effects and printed to the
debug console, `PLAY_ONE` is always cleared, `Delay` is clamped at 0, `PitchShift` / `PerFilePitchShift` are held at -99 when
they are at or below -100, and `VolumeShift` / `PerFileVolumeShift` outside `[-100, 0]` become 0. **`Volume` and `MinVolume`
are not clamped here** — a value above 100 percent stays above 1.0.

## 2. The runtime side: how an event's volume is computed

`MilesAudioManager::getEffectiveVolume` RW `0x4591D4` (called for one event; third argument = "apply the submix slider"):

1. `0x4591DF`: view gate — `event->[0x30]` (view type) must equal the manager's current view `+0x678` (0 tactical, 1 Living
   World); a view type of 2 or more passes. Otherwise the result is 0.
2. `0x45920E`: `volume = event->getVolume()` (`0x6DB221`: the override `+0x24` when it is set, else
   `info->volume * multiplier(+0x2C)`, 0.5 when there is no info; an object with status bit 20 set, or a drawable whose byte
   `+0x44A` is clear, mutes to 0) `* getVolumeShift()` (`0x6DA664` = `perFileVolumeShift(+0x60) * volumeShift(+0x5C)`).
3. `0x45922A`: only when `isPositionalAudio()` (`0x6DAD89` — a sound effect needs the `WORLD` type bit; ambient streams pass;
   the owner type must resolve) — and then only when `volume > 0` — `volume *= 0x453227(event)`.
4. `0x453227` (the distance law): `getCurrentPosition` (`0x6DB31E`) failing → 0; `d >= max` → 0; `d <= min` or `max <= min` →
   `[0xBD1908]` = 1.0; otherwise `(max - d) / (max - min)`. `min` / `max` are `info->MinRange` / `MaxRange` (`+0x94` / `+0x98`),
   or for a `GLOBAL` event (type bit 8, `0x453250`) `AudioSettings` `GlobalMinRange` / `GlobalMaxRange`.
5. `0x45924E`: `minVolume = event->getMinVolume()` (`0x6DB2A5`, the same owner mute, else the override `+0x28` when set, else
   `info->minVolume * multiplier`).
6. `0x459258`-`0x45937B`: the zoomed-in off-screen fade, only when the manager's zoom volume `+0x8C` is above 0, the event has
   a position, it is not on screen (virtual slot `+0x1AC`), and `ZoomedInOffscreenVolumePercent` (`+0x9C`) or
   `ZoomedInOffscreenMinVolumePercent` (`+0xA0`) differ from 1.0 — the factor is
   `1 - (1 - percent) * min(1, d_offscreen / ZoomFadeDistanceForMaxEffect) * zoomVolume`, applied to the volume and to the
   minimum volume separately (`0x45933F`-`0x45937B`).
7. `0x45937D`: `if (minVolume > volume) volume = minVolume` — **MinVolume is a floor in the same units, applied before the
   slider factor**.
8. `0x45938D`-`0x4593AB`: when the third argument is non-zero, `volume *= 0x458825(this = &sliderBlock, event)` — the submix
   slider factor.
9. `0x4593AE`: when `event->[0x52]` is set and the volume is below `[0xBE5600]` = 0.01 and the event has an info whose
   `0x5D9168(info, 2)` is false, the volume is raised to 0.01 (a floor for the events that would otherwise be silent and are
   still fading).

`MilesAudioManager::getEffectiveVolume` is also entered through `0x45A10E(event, applySliders, fade)`:
`0x4591D4`, then `volume *= 0x450A18(node->[0x30])` where `0x450A18` = `clamp01(1 - x / (float)AudioSettings->[0x78])`, and the
0.01 floor above. `0x45B093` (2D) and `0x45B4A6` pass 1 / 1; the frame update `0x45EE2B` / `0x45EE7C` multiplies the same
`0x450A18` term and then `0x458825` again.

`0x458825` (the slider factor) picks one of **eight** floats per slider:
`index = 4 * (2 * slider + pos) + bits`, `factor = this[0xC8 + 4 * index]`, where `pos` = 1 for a positional or ambient-stream
event (`0x458832`, `0x45883F`), `bits` bit 0 = "no ducking is running, or this event is in the immune set `0x456726`", bit 1 =
"the event has **no** `VolumeSliderMultiplier` entries" (`0x458888`-`0x458898`). The slider itself comes from `0x5D90C3`.

`0x4516DE` (which fills those eight entries per slider) shows the table's four variants per (slider, pos), each clamped into
`[0, 1]`:

```
base  = systemVolume+4*(pos+2*slider) * [0xD99014 + 4*slider] * this->[0x98]   ; * this->[0x94] when pos == 0
v0 = base                     v1 = this->[0x9C] * base
v2 = scriptSlider[slider] * base   v3 = this->[0x9C] * scriptSlider[slider] * base
slider 5 (NONE) = four 1.0
```

So the port's single `m_systemVolume[slider] * m_scriptVolume[slider] * (pos ? zoomVolume : 1) * (no multipliers ? duck : 1)`
model is an approximation of this table (the port's acceptance stop S-243 says so). **This is a shared mechanism, not the
hero's own bug** — see section 4.

## 3. What the port does with the same event (checked, matches RW term by term)

`engine/src/Common/Audio/GameAudio.cpp:2271` `AudioManager::getEffectiveVolume` and
`engine/src/Common/Audio/AudioEventRTS.cpp` `getVolume` / `getMinVolume` / `getVolumeShift` reproduce steps 1-9 above:
the view gate, `volume * shift`, the linear distance factor with the same three branches, the off-screen zoom fade on both the
volume and the minimum volume, the `MinVolume` floor before the slider, and the slider factor last. `parsePercentToReal` for
`Volume` matches RW `0x42EEFA` (see section 1), and the post-parse clamps match RW `0x5D9FF3` except that the port leaves
`Volume`/`MinVolume` unclamped exactly as retail does.

Consequence for the report: the Dain voice events differ from the rest of the roster **only** in their `Volume` /
`MinVolume` pair (75 % / 65 % against the 115 % / 105 % the other heroes' voice events use, both read from the mod's INI) —
every other field (`Priority`, `Type = world player voice`, `SubmixSlider = voice`, and the inherited `MinRange` / `MaxRange`
of `AudioEvent DefaultSoundEffect`) is identical to a hero that sounds right. Both pairs are below 100 %, so the difference
that survives to the device is the ratio of the two `MinVolume` floors times the slider factor; the port computes that ratio
the way RW `0x4591D4` does.

## 4. Answer for the fork issue

The Iron Hills hero's voice events carry their own `Volume` / `MinVolume` pair, and it sits below the pair the rest of the roster
uses (75 / 65 percent against 115 / 105, both read from the mod's `voice.ini`; every other field of those blocks — `Priority`,
`Type = world player voice`, `SubmixSlider = voice`, and the `MinRange` / `MaxRange` inherited from `AudioEvent DefaultSoundEffect`
— is the same as a hero who sounds right). RW 0x4591D4 turns exactly that pair into the loudness: the distance law pushes the
volume down to the `MinVolume` floor at ordinary camera distances, so the hero's line lands on 0.65 while the roster's lands on
1.05, and the only later multiplication (the slider factor, 0x458825) is the same for both. The port reproduces every step of that
chain (section 3), so the hero is quieter in the port **by his own INI numbers, exactly as in retail**. Nothing in the manager
treats one hero differently from another, and no term of the chain can be "fixed" to even the roster out without departing from
RotWK.

The tests of `engine/tests/test_port_214.cpp` (`PORT-214*`) pin that: the chain on two event shapes, the ratio of the two floors
under two different voice sliders, an audit of a synthetic roster against an independent implementation of the formula, and the
constructor defaults / post-parse of the INI side.

## 5. What is still open (stop S-7386, narrower than S-243)

The absolute loudness level of the mixer is not exact yet: the submix slider factor the port multiplies in last
(`AudioManager::sliderFactor`) is its own `system * zoom * script * duck` product, while RW keeps **eight** floats per slider at
MilesAudioManager +0xC8 and refreshes them in 0x4516DE as

```
base = systemVolume(+4 + 4 * (pos + 2 * slider)) * [0xD99014 + 4 * slider] * this->[0x98]   (* this->[0x94] when pos == 0)
v0 = base      v1 = this->[0x9C] * base      v2 = scriptSlider(+0x34 + 4 * slider) * base      v3 = this->[0x9C] * v2
```

each clamped into [0, 1] (slider 5, NONE, is four 1.0). `0x458825` picks the entry with
`index = 4 * (2 * slider + pos) + bits`, `pos` = positional or ambient stream (0x458832 / 0x45883F), bit 1 = "the event has **no**
`VolumeSliderMultiplier` entries" (0x458888-0x458898), bit 0 = "no ducking runs, or the event is in the immune set 0x456726".
What writes +4 (per slider **and** position), +0x34, [0x94] / [0x98] / [0x9C] and 0xD99014 — and when the manager refreshes them —
is not read yet. It is the mixer as a whole (every event, every slider), so it belongs to a follow-up task; the per-event ratio is
what this lane pins.
