# PORT-87 (issue #235): unit voice `VoiceStartCharging` / `VoiceNoBuild` (messages 0x7EA .. 0x7EB) — RW analysis

RotWK 2.01 `game.dat` (md5 `daed3668006cd90f01c34e5a7da1901f`). Addresses are virtual (image base 0x400000).

## What was read, and from where

The sandbox of this session has no access to the game files (only GitHub / npm / pypi are reachable, so the Drive folder `for-arena`
with `game.dat` could not be downloaded and `workspace/RotWK/game.dat` does not exist): no new instruction of RotWK was read in this
session with `tools/mod/rw.py dis`. Every RotWK address below is therefore taken from **the evidence this repository already holds**,
and each one is named with its writer:

* `docs/ennorath/arena-triage/batch-4.md` row 24 / 25 and section A (the audit of 2026-11-09 read `game.dat` directly) — the
  dispatcher case, the literal string, and the live caller of 0x7EB;
* `docs/ennorath/arena-triage/batch-8.md` row 13 — the literal `VoiceStartCharging` query;
* `engine/src/GameClient/MessageStream/PlaceEventTranslator.h` — the refusal branch of RotWK's translator, address by address
  (a port comment that was written from `rw.py dis` of RW 0x83E486, lane BUILD-5);
* the BFME2 1.06 decompilation (`/tmp/open-bfme-2`, byte-matched to BFME2's `game.dat`) as the **DONOR** only.

Anything that is not in those sources is marked INFERENCE or STOP below; nothing is guessed from a name.

## Flow-control facts (RotWK 2.01)

1. **The voice dispatcher** RW 0x8DEDBB (`UnitVoiceResponse.cpp`'s `pickAndPlay`) has a second switch over the client voice message
   types; its jump table for ids 0x7E4 .. 0x7EC is RW 0x8E023B (batch-4 row 25). Ported cases live in
   `engine/src/GameClient/UnitVoiceResponse.cpp`; the ids this port does not answer are counted in `Stats::unportedMessages` (stop S-703).
2. **`VoiceStartCharging`** (0x7EA): RW 0x8DFD0D .. 0x8DFD2B pushes the literal at RW 0xC77A74, builds the AsciiString
   (RW 0x4374E0) and calls the name lookup RW 0x8DDB2A (batch-8 row 13). Lookup chain: RW 0x8DDB2A → RW 0x825D3A (the entry of the
   crowd key) → RW 0x674FFE → RW 0x67505B (the template of the object) → RW 0x73EDEF .. 0x73EE0E (the `UnitSpecificSounds` map of
   ThingTemplate + 0x388, table RW 0xDA4378, parser RW 0x73F684) → RW 0x8DDA33 stores the sound in the response record. The port's
   `fillName` is exactly this route (`objectNameVoice`).
3. **`VoiceNoBuild`** (0x7EB): the same shape at RW 0x8DFE7B (literal RW 0xC77A64), 21 bytes ahead of the `0x7EB` selector; both cases
   end in RW 0x8DDB2A (batch-4 row 25).
4. **The post of 0x7EB.** RW 0x83EAA0 `push 0x7EB` inside RotWK's `PlaceEventTranslator::translate` (RW 0x83E486): three arguments
   (`[ebp+8]` = the drawable list of the builder alone, `0x7EB`, the `PickAndPlayInfo` built by RW 0x8DD5FF) go to RW 0x8DEDBB
   (batch-4 row 25 "живой вызов"). The port's own reading of the same function (`PlaceEventTranslator.h`) places the branch:
   RW 0x83EA4E `displayCantBuildMessage` (the refusal label) → **RW 0x83EAA0 the voice** → RW 0x83EAC5 the MiscAudio
   `BadBuildingPlacementSound` (+0xE8) → RW 0x83EB02 `setPlacementStart(null)`. So the voice is posted for a **refused placement click**,
   after the label and before the UI beep, for the **builder alone** (not for the whole selection).
   DONOR BFME2 1.06 `PlaceEventTranslator.cpp:614-628` has the same three steps in the same order (`MSG_CANNOT_BUILD_VOICE` = 0x7EB,
   `PickAndPlayInfo::m_position = world`, then the beep, then `setPlacementStart(0)`), which is what identifies the info's only
   content as **the world position of the click**.
5. **The post of 0x7EA is not established in RotWK 2.01 by any source in this repository**, and this session could not read the image.
   What is known:
   * DONOR BFME2 `AIChargeTargetStateOnEnter.cpp` (retail BFME2 0x00352E12): `AIChargeTargetState::onEnter` builds a one-element
     `DrawableList` of the source object and calls `pickAndPlayUnitVoiceResponse(&list, MSG_BFME2_0x7EA, 0)` — **no `PickAndPlayInfo`** —
     after it cleared `+0x48`, copied the goal object's position and locked the primary weapon temporarily.
   * DONOR BFME2 `AIAttackStateCreateAttackMachine.cpp:95-110` (retail BFME2 0x0034B1E9): attack machine **kind 8** is
     `new AttackFireWhileChargingMachine(...)`, whose constructor (BFME2 0x00343838) defines `AIChargeTargetState` as its only state
     (id 0, `defineState(0, state, 0x270E, 0x270F, 0)`).
   * The port has none of that: `AIAttack.h` knows `ATTACK_MACHINE_NORMAL` / `ATTACK_MACHINE_HORDE` only, and there is no
     `AttackFireWhileChargingMachine`, no `AIChargeTargetState`, and no code that enters them. The mod's charge (`SPECIAL_CHARGE_ATTACK`
     0x2A, `SpecialAbilityUpdate::trigger` RW 0x8547C5 .. RW 0x85481A) approaches through `aiAbilityApproachObject` (RW 0x852DC5,
     AI command 0x48) and never posts 0x7EA.
   * Where in RotWK's own code the 0x7EA post sits (`xref` of the dispatcher's 57 callers) is **unread**: this session has no image.

## STOP

* **STOP for 0x7EA (the post).** Porting the dispatcher branch alone is safe and is done (see below), but *who posts 0x7EA* cannot be
  proven for RotWK 2.01 from the sources available here, and Port 87 must not guess. The donor's machine kind 8 is not in RW's ported
  attack-kind reading (`RW 0x74CED6` — kinds 2 melee, and the horde kinds; `AIAttack.cpp`'s `createAttackMachine`), so the port has no
  legitimate place to make the call, and adding an `AttackFireWhileChargingMachine` would be a new AI machine (a second path) built on a
  donor fact alone. Registered as stop **S-7226** in `docs/STOPS.md` with the two reads that would close it:
  1. `rw.py xref 0x8DEDBB` on a machine with `game.dat`, then the caller that pushes `0x7EA` (and its `mov eax,0x7EA`, i.e. the immediate
     in the caller, not the switch of the dispatcher);
  2. the class chain of that caller: `create` of an attack machine (RW's `AIAttackState::createAttackMachine`, BFME2 0x34B1E9's twin) →
     constructor → vtable → the state's slot 4 (`onEnter`), to name the state that posts it.

## What the port does now (this lane)

| RW | what it does | port |
| --- | --- | --- |
| 0x8DFD0D .. 0x8DFD2B | case 0x7EA: `UnitSpecificSounds/VoiceStartCharging` | `UnitVoiceResponse::pickAndPlay`, `VOICE_EVENT_START_CHARGING` |
| 0x8DFE7B | case 0x7EB: `UnitSpecificSounds/VoiceNoBuild` | `UnitVoiceResponse::pickAndPlay`, `VOICE_EVENT_NO_BUILD` |
| 0x8E023B | the jump table of the second switch (0x7E4 .. 0x7EC) | the `VoiceEvent` enum's ids |
| 0x83EAA0 | the refused placement click posts 0x7EB with the builder alone and the click's world position | `PlaceEventTranslator`'s refusal branch, before the MiscAudio beep |

The two branches share the existing `fillName` route (RW 0x8DDB2A) — no second lookup was added; a unit without either row answers
silence (the `Nothing` row of RW's `UnitSpecificSounds`, `fillName`'s empty result).
