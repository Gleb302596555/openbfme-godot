# PORT-151 (fork issue 369): the unit voices no order ever asks — RotWK 2.01 analysis

Addresses read linearly through `tools/mod/rw.py dis|xref` from `game.dat` (MD5 `daed3668006cd90f01c34e5a7da1901f`, caveat S-001);
the BFME2 1.06 decomp (`Open-BFME-2`, files under `Code/GameEngine/Source/...`) is the donor only: every step is confirmed in RW.

Fields of the issue, and whether RotWK reads them:

| field | RotWK reader | verdict |
|---|---|---|
| `VoiceMoveToCamp` / `VoiceRetreatToCastle` (rows 0x11 / 0x10) | `RW 0x8DEABD .. 0x8DEBC6` inside `moveVoices` `RW 0x8DE838`; campness test `RW 0x8DD6F3` (AudioSettings + 0x44 / + 0x48, shroud channel 2) | **ported** (S-560, `TOP20-560 camp voice*`) — message-driven path |
| `VoiceEnterStateMoveToCamp` / `VoiceEnterStateRetreatToCastle` (rows 0x1F / 0x1E) | same rule with rows 30 / 31 for the enter-state messages `0x7E7` (move) / `0x7E8` (attack-move) — `RW 0x8DE883 .. 0x8DE8AD` loads 0x1B, 0x1E, 0x1F, 0x20, 0x1C, 0x1D when `RW 0x8DD63C` says enter state | picker ported, **but nothing posted `0x7E7`/`0x7E8`**: the AI commands never asked (this file, §1) |
| `VoiceEnterStateMove`, `VoiceEnterStateAttack*` (rows 0x1B / 0x16..0x1A) | same `moveVoices` / `attackVoices` `RW 0x8DDF3B` enter-state branches | same as above |
| `VoiceEnterUnitTransportShip` | the only enter-unit reader is the prefix `"VoiceEnterUnit"` (`RW 0xC77BE4`, pushed at `RW 0x8DF4A3`) concatenated with the target template's name (`RW 0x8DF495 .. 0x8DF4D7`, name at template + 0x64) and looked up by `RW 0x8DDB2A` | **no 0x7E6-style literal**: the key is `VoiceEnterUnit` + template. `"VoiceEnterUnitTransportShip"` is asked only for an object template literally named `TransportShip`; Ennorath has none (`ElvenTransportShip`, `EvilMenTransportShip`, `GondorTransportShip`, ... `MordorMumakil` for the Mumakil key). The port already builds the same key (`UnitVoiceResponse.cpp:1200`), so the mod's line stays dead in retail **and** in the port — no code, no test |
| `VoicePrimaryWeaponMode` / `VoiceSecondaryWeaponMode` (+ tertiary) | one `.text` site each: `RW 0x8DF7C9` / `RW 0x8DF7A4` / `RW 0x8DF776`, all in the `0x43A` (`MSG_SWITCH_WEAPONS`) arm `RW 0x8DF74F` of `RW 0x8DEDBB` | reader exists but only `GUI_COMMAND_SWITCH_WEAPON` (`0x1B`) / `TOGGLE_WEAPON` (`0x23`) post `0x43A` (`RW 0x941204` / `RW 0x941257`). The port's picker does not answer `0x43A` (S-703) and Ennorath's 29 templates toggle through `TOGGLE_WEAPONSET` (`0x457`, `RW 0x9412D0`), which no voice call follows (`RW 0x941A22` -> `RW 0x75D22E`, button frame `+0xFC` only). Already established by PORT-85 (`docs/ennorath/port85-weapon-mode-voice-rw.md`, S-7285); nothing to add here |
| `VoiceSpecialAbilityCurseEnemy` | **no literal string in `game.dat`** (`rw.py str "VoiceSpecialAbility"` finds none) | the special-power voice is the template's `UnitSpecificSoundToUseAsInitiateIntendToDoVoice` (+0x3C, `RW 0x8DF890` -> `RW 0x8C72DF`) / `...EnterState...` (+0x40, `RW 0x8DFCE1` -> `RW 0x688D3C`). In Ennorath the two lines naming this key in `specialpower.ini` are commented out; the UnitSpecificSounds entry is only a dictionary key nobody asks -> dead in retail, dead in the port |

## 1. The missing posters of the enter-state messages

`RW 0x8DEDBB` answers `0x7E6`, `0x7E7`, `0x7E8` (low byte table `RW 0x8E01CD`, selectors 2 / 3 / 5 -> `RW 0x8E0191`, bodies `RW 0x8DFAxx`). The only producers in `game.dat` are the
`AIUpdateInterface` voice helpers, each of which takes the owner's drawable (`Object::getDrawable` `RW 0x70E013`, object + 0x84), builds a one-element drawable list and a
`PickAndPlayInfo` (`RW 0x8DD5FF`) and calls `RW 0x8DEDBB`:

| helper | message | body |
|---|---|---|
| `RW 0x66B1D0` (target object) | `0x7E6` | `RW 0x66B24A push 0x7E6`; victim's drawable, victim position (obj + 0x38) and `RW 0x68BEED` (airborne locomotor: AI + 0x260, locomotor + 0x1F0, template surfaces bit 0x8) into the info |
| `RW 0x66B271` (position) | `0x7E6` | `RW 0x66B2CE push 0x7E6` |
| `RW 0x66B2F5` (position) | `0x7E8` | `RW 0x66B352 push 0x7E8` |
| `RW 0x66B379` (position) | `0x7E7` | `RW 0x66B3D6 push 0x7E7` |

Their call sites (each behind the command source test `[ebp+0x10] == 0 || == 1`, i.e. `CMD_FROM_PLAYER` / `CMD_FROM_SCRIPT`):

| caller | RW | voice |
|---|---|---|
| `privateMoveToPosition` `RW 0x66B3FD` | after the state is set (`RW 0x66B48F .. 0x66B4C2`), `RW 0x66B4C4 .. 0x66B4D2` call `RW 0x66B379` | `0x7E7` |
| `privateAttackObject` `RW 0x66BD33` (`RW 0x66BE77` is its tail) | `RW 0x66BE9F .. 0x66BEB0` call `RW 0x66B1D0` with the victim | `0x7E6` |
| `privateForceAttackObject` `RW 0x66BF4C` | `RW 0x66C021 .. 0x66C02F` (and `RW 0x66C0A1 .. 0x66C0AD`) call `RW 0x66B1D0` | `0x7E6` |
| `privateAttackPosition` `RW 0x66DA5F` | `RW 0x66E051 .. 0x66E060` call `RW 0x66B271` with the spot | `0x7E6` |
| attack-move to position, command 0xF (body `RW 0x66C0B9`; vtable slots at `RW 0xC105C8`) | `RW 0x66C128 .. 0x66C136` call `RW 0x66B2F5` | `0x7E8` |
| attack-follow-waypoint, body `RW 0x66C1C9` (slots `RW 0xC10638`) | `RW 0x66C1BC` / `RW 0x66C23C` call `RW 0x66B2F5` | `0x7E8` |

The temporary branch of `privateMoveToPosition` (a busy unit under a `CMD_FROM_AI` move, `RW 0x66B44D .. 0x66B48D`) jumps to `RW 0x66B4D7` and posts nothing: only the ordinary
path speaks. `0x7E6` of `RW 0x66BD33` is skipped when the order is refused (`RW 0x66BD5E`) and when the target is gone (`RW 0x66BF91`).

## 2. What the port does today

`UnitVoiceResponse` (client) already answers `0x7E6` / `0x7E7` / `0x7E8` and fills the enter-state rows (rows 27..32 of `kRowNames`). The logic never posts them: a grep for
`postUnitVoice` finds the creation, fear, stance and capture events only. So a player's move / attack order says `VoiceMove` / `VoiceAttack` (the message path) and never
`VoiceEnterStateMove`, `VoiceEnterStateMoveToCamp`, `VoiceEnterStateRetreatToCastle` or `VoiceEnterStateAttackUnit*`.

## 3. The change (one path, as RW)

`AudioApi::postUnitVoice` already carries exactly RW's helper payload (`objectId`, target, position) and the HUD feeds it into the same `UnitVoiceResponse`; the port only has
to post where RW calls its helpers:

* `AIUpdateInterface::privateMoveToPosition` (`engine/src/GameLogic/Module/AIUpdate.cpp`) — `0x7E7` after the state is set, in the ordinary branch only (`RW 0x66B4D2`);
* `AIUpdateInterface::aiAttackObject` / `aiForceAttackObject` / `aiAttackPosition` (`AIUpdateCombat.cpp`) — `0x7E6` (`RW 0x66BEB0` / `0x66C02F` / `0x66E060`);
* `AIUpdateInterface::aiAttackMoveToPosition` and `aiAttackFollowWaypointPath` — `0x7E8` (`RW 0x66C136` / `0x66C1BC`).

`CommandSourceType`'s `CMD_FROM_PLAYER = 0` / `CMD_FROM_SCRIPT = 1` are the same values the binary compares.
