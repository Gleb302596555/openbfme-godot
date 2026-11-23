# Port 330 (issue #847) — ScriptEngine, Radar, FXList / FXPlayer, GameWorld: the 27 open stops

Branch `arena/cedf59fc-openbfme-godot`, base `ca639a4`. Everything below was checked against the RotWK 2.01 `game.dat` on this branch; only
`RW 0x...` addresses are quoted (no dossier lines). The stops' rows in `docs/STOPS.md` carry the outcome of each one.

## What was ported

One mechanism: **the FXList block's parse-time name checks** (stop S-190). RotWK validates the two kinds of name while the block parses, and the
registries it uses are the *world's* tables, not the FX store's:

| name | RW path | answer |
|---|---|---|
| `Sound` nugget `Name` | `RW 0x73B217` → `RW 0x73AA94` (also called from the contain / weapon audio parsers `RW 0x73AB45`, `RW 0x73AAA9`) | `NoSound` (any case, `_strcmpi` through `RW 0x42DC9F`) clears the name; otherwise TheAudio (`RW 0xDE42FC`) `findAudioEventInfo` (vslot `0x12C`, case-sensitive key); a miss is `INIException(3, "Invalid Sound '%s'")` — the throw at `RW 0x73AB1A..0x73AB3F`, format string `RW 0xC2500C` |
| `EvaEventOwner` / `EvaEventAlly` / `EvaEventEnemy` | `RW 0x5DE588` → `RW 0x5DE0D8` (TheEva `RW 0xDE3670`, base name map at `+0x48`) | `None` is -1 without a lookup (`RW 0x5DE0E3`); an unknown name is -1, and `RW 0x5DE14E` throws code 3 `Expected a recognized Eva event name or 'None'; got '%s'` (string `RW 0xBF2678`) |

The two subsystems' inits (`TheAudio RW 0x4538DA`, `TheEva RW 0x5DE837`) run before TheFXListStore's entry in `GameEngine::init`
(`RW 0x63AF2D` onward), so retail can never read an FXList block before both tables exist — the checks are never skipped in a loaded game.

Port shape:

* `GameClient/FXList.h` — `FXListParseServices` becomes `audioEventExists(name)` + `evaEventIndex(name)` (the second mirrors RW's index answer, so
  "unknown" and "None" are told apart), plus `MakeWorldFXListParseServices(const RetailObjectWorld &)`.
* `GameClient/FXList.cpp` — `parseEvaName` throws on a negative index (RW `0x5DE14E`); the store keeps counting the *unchecked* names, and
  `unverified()` now says the store simply had no world's tables.
* `GameClient/FXListParseServices.cpp` (new) — the services object: it holds the **world**, not the world's state, and answers from the current
  `AudioIniState`, so `RetailObjectWorld::reload()` (MAPRESET-1, which destroys the state) cannot leave a dangling answer behind.
* `GameLogic/Object/RetailObjectWorld.{h,cpp}` — the world builds the services (`installFxParseServices`, in the constructor and after every
  reload) and installs them on its own `FXListStore`; `fxListParseServices()` exposes them.
* `GameClient/FXPlayback.h`, `GodotDevice/GodotFXPlayer.{h,cpp}`, `GodotDevice/GodotGameWorld.cpp` — the client's FX player parses the FX files
  again, so it takes the same answers before that parse (`setParseServices`, `GameWorld::setup`); retail has one TheAudio / TheEva for both loads.

No simulation state is added: nothing to hash (`crc`) or save (`xfer`); the pins are untouched.

## Data check (Ennorath 1.9.8.3)

Every name an FXList block writes, over the mod's INI tree: **2669 `Sound` names and 150 `EvaEvent*` names**, all of them resolving against the
mod's own `AudioEvent` / `NewEvaEvent` tables, and **none** differing only by case. So switching the checks on rejects nothing the mod ships; it
rejects a typo, as retail does. (The earlier figure of 1795 was a count of `AudioEvent =` references anywhere, not of FXList nuggets.)

## The 27 stops

| stop | area | outcome |
|---|---|---|
| S-190 | FXList parse-time references | **closed (FXList name checks against TheAudio / TheEva)** |
| S-684 | Old particle systems | already RESOLVED on main (FX-3) — nothing to do |
| S-5205 | FXList DynamicDecal drawn | already superseded on main (FEAT-BLDFX-1 / S-687) — nothing to do |
| S-291 | Radar picture without map art | left: W3DRadar's terrain-texture build (`RW 0x44F4AD`) is a whole render feature, not a name check; no part of it is in this port's files |
| S-761 | Radar art and shroud | left: the shroud's compiled-texture format (`RW 0x44EF41..0x44EFDC`, `RW 0x532875`) is the render lane's |
| S-1674 | Radar events (Space) | left: needs `Radar::createEvent`'s remaining callers and the TheEva window fields `+0x90` / `+0x94` (`RW 0x5DD1F3`), a radar mechanism, not a parse-time check |
| S-2450 | Radar object order | left: the snapshot's add/remove events (RADAR-1's client snapshot) — the SMOOTH-1 lane's |
| S-3950 | Radar transform | left: needs a retail screenshot pair from the tester (the stop's own cheapest resolution) |
| S-340 | Structure collapse | left: the FX list / OCL stores a phase fires belong to the OCL lane |
| S-345 | Structure topple | left: `NAMED_SET_TOPPLE_DIRECTION` and `ScriptEngine::adjustToppleDirection` belong to the script lane (and onDie's address is unconfirmed) |
| S-683 | Effects not played | left: the model-condition FX loop and FXList's timed particle list (`RW 0x4BCE68`'s second loop) are FX-2's |
| S-711 | In-game music random draws | left: needs RotWK's `setTimer` for script action 151 decoded (AUDIO-4) |
| S-810 | Frame pacing, logic worker | left: HUD snapshot reads — SMOOTH-1's |
| S-814 | Audio beside the logic worker | left: the deferred `playSoundForObject` handle is AUDIO-2 / SMOOTH-1's |
| S-831 | Gamma-space compositing | left: RENDER-3's; needs a retail capture |
| S-1060 | End sequence (END-1) | left: a client-side script engine for the player libraries — END-1's own lane |
| S-1125 | Replays of every game | left: replay save/camera/controls — MP-2's |
| S-1187 | HAS_FINISHED_AUDIO by a logic length model | left: needs the audio generator's state in the logic (SCRIPT-3) |
| S-1440 | Particle sprite composite | left: how the dust looks on screen; needs a retail frame capture (QA-1) |
| S-1651 | Map fog on models and particles | left: RENDER-4's; needs the RotWK render states |
| S-1921 | End-game timer | left: a simulation change (pins and review), the stop's own note |
| S-1953 | Players screen Status page | left: the network session's disconnect state must reach the slot states |
| S-4002 | Campaign mission saves | left: retail save import (SAVE-7) |
| S-4075 | Online War of the Ring | left: WOTR2 milestone 9's (phase replay driver, relay drop) |
| S-4878 | Game records | left: reading old replays' headers into records |
| S-4972 | Breeze | left: `ScriptEngine::xfer` does not carry the breeze; the freeze gate needs a script-time freeze that does not exist yet |
| S-5306 | Mod data errors | left: classification of each corpus error (MODS-3) — a data/analysis task, no port change |

The ones left are, by mechanism, other lanes: the radar/render work (S-291, S-761, S-1674, S-2450, S-3950, S-1440, S-1651, S-831), the script
engine's own state (S-1060, S-1187, S-1921, S-345, S-4972), the client/netside (S-810, S-814, S-1125, S-1953, S-4002, S-4075, S-4878), FX-2/FX-3
(S-340, S-683) and the audio lanes (S-711). None of them is a parse-time name check, which is why S-190 was the one that closed here.

## Found next door, not ported

* `GameLogic/Module/AttachUpdate.cpp:63` `parseEva` (stop **S-1361**) takes an Eva name without asking TheEva, unlike RW `0x5DE588`. The world's
  answers now exist (`RetailObjectWorld::fxListParseServices`, and the `TheContainParseHooks()` chain the contain parsers use), so this is a small
  follow-up in AttachUpdate's own row — left as S-1361 says, since the stop's remaining work also covers a horde anchor and is another lane's row.
* `GameLogic/Weapon.h:177` / `GameLogic/Object/Weapon.cpp:378` `WeaponReferenceHost::evaEventExists`: no host implementation exists in-tree yet, so a
  weapon's `EVA:` voice falls back to the recorded-reference path (`S-181`). Once a host is installed it can reuse the same answers.

## Tests

`PORT-330*` — `engine/tests/test_port_330.cpp`, wired by `engine/cmake/Port330.cmake` (5 cases, 52 assertions):

1. a `Sound` name is checked while the block parses (the code-3 text, `NoSound` clearing, the replaced list);
2. an `EvaEvent*` name is checked the same way (`None` = none, an unknown name throws, the table is the world's and case-sensitive);
3. the neighbour behaviour: a bare store parses unvalidated and reports the counts (FX-1's fallback);
4. a loaded world's own store is validated (this is the case that fails without the wiring — see the red check below);
5. the answers survive `RetailObjectWorld::reload()`, which replaces the tables they read.

Red check: with `installFxParseServices`'s `setParseServices` line reverted, 4 of the 5 cases fail (12 assertions); restored, 5/5 pass.
Neighbouring suites: `FX*` 53/53, `FXParticleSystem*` 6/6, `FXList*` 3/3, `mapreset1*` 7/7, `upgrade*` 16/16, `camp1*` 31/31, `visual2*` 16/16,
`PORT-318*` 4/4, `perf1*` 11/11, `uber1*` 7/7. `test_fx_particle_ini.cpp:647-648` keeps asserting the *bare* store's report
(`unvalidatedSoundNames() > 0u`, `unverified().size() == 2u`) and still passes — it uses `FxHarness`, which has no world.
