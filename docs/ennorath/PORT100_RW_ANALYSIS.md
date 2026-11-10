# Port 100 — LargeGroupAudio HandOffModeDuration: RW analysis (2026-11-10)

Field: `LargeGroupAudioMap/HandOffModeDuration`, table RW 0xC4C610 (field name RW 0xC4C59C), offset 228 (0xE4) in LargeGroupAudioAudioMap.

## Parse (RW)
- Parser RW 0x73B071: scans unsigned ms, converts with float32 0.005 (RW 0xD9F610) and ceil (call through RW 0xBD0588, RW 0x73B09C..0x73B0A7), stores ONE byte at map+0xE4 (`mov byte ptr [ecx], al` RW 0x73B0E3). Above 255 frames it formats "Duration %u must be less than %d frames" (RW 0xC25100) and throws (RW 0x73B0B4..0x73B0DB). So 3000 ms = 15 frames; the port's generic `parseDurationUnsignedInt` already does this ceiling; only the byte range / error was missing.
- Map copy copies the byte (RW 0x7EF5AF); equality compares it (RW 0x7EEE09); a getter exists (RW 0x970186, `mov al,[ecx+0xE4]`) with no direct callers found — the runtime reads the byte through the grid overlay (below), not through the getter.

## Class chain
- Parse creates/finds the map in TheLargeGroupAudio (RW 0x60DD01). `Sound [name]` (RW 0x7EF63F) constructs a LargeGroupAudioSoundKeyPair (ctor RW 0x971AB0), which stores its owner map at pair+0x3C (`mov [esi+0x3C], eax`, RW 0x971AF0). The pair treats the owner map as its "grid": Size at +0x10, MaximumAudioSpeed at +0x14, StartThreshold (word) at +0xE0, StopThreshold at +0xE2, and the byte at +0xE4 is the grid's hand-off mode. DONOR BFME2: `m_grid(reinterpret_cast<Grid*>(owner))` in LargeGroupAudioSoundKeyPair ctor and `Rva005C8876Grid{ startWeight +0xE0, continueWeight +0xE2, mode +0xE4 }` (Code/GameEngine/Source/Common/LargeGroupAudioSoundKeyPair.cpp) — same layout confirmed in RW above.

## Runtime reader (the hand-off)
SoundKeyPair::update (RW, displaced loop at 0x972136..0x972213, cell-start loop 0x972238..):
1. A playing cell whose weight fell below StopThreshold, or with a heavier neighbour, is displaced (RW 0x971EFC): its audio event ref is taken out and saved together with the cell's current byte at cell+0x46 (RW 0x971F1E `mov al,[esi+0x46]`).
2. The best qualifying cell (weight >= StartThreshold, not locked) receives the event: `LargeGroupAudioGridCell::setEvent` RW 0x9D8668 stores the passed byte at newCell+0x46 (RW 0x9D8692..0x9D8697). The byte passed is (RW 0x9721BD..0x9721D0): if the best cell is the displaced element's own host, its saved byte; otherwise `mov eax,[esi+0x3C]; mov al,[eax+0xE4]` — the map's HandOffModeDuration frames. If no cell qualifies, the event is stopped (RW 0x9721F8: audio slot 0x6C removeAudioEvent).
3. Fresh sounds (cell newly qualifying, no displaced event) are started and set with byte 0 (RW 0x972238 loop pushes 0 to RW 0x9D8668).
4. Every playing cell then updates (RW 0x972315: `call 0x9D8018` with the map/grid). GridCell update RW 0x9D8018: while byte cell+0x46 (steps) > 0, the event position moves by `pos += (preferred - pos) * (1.0f/steps)` in x, y and z and steps is decremented (RW 0x9D8063..0x9D80C7) — so the glide finishes exactly on the HandOffModeDuration-th frame. At steps == 0 the position instead moves toward the preferred position clamped to MaximumAudioSpeed per frame (RW 0x9D80CF..).

So HandOffModeDuration is not a threshold delay and not a cross-fade of two sounds: when a crowd/fire/wading loop's cell hands its single audio event to another cell, the event glides from its old position to the new cell's preferred position over that many logic frames (harmonic 1/steps steps), instead of the usual MaximumAudioSpeed clamp. With no field (0), steps is 0 and the event simply follows at MaximumAudioSpeed.

## Port plan
Existing port path is `LiveGameAudio::largeGroupUpdate` (per map/Sound/cell groups, INFERENCE S-248; the full 4-grid RW runtime is not ported). On that path: when a group/cell that was playing stops qualifying and another cell of the same map+Sound qualifies in the same pass, transfer the playing handle to the new cell (no stop/start), keep its position, and set its hand-off steps from the map; while steps > 0 move the sound position by 1/steps toward the cell centre each pass, then fall back to the MaximumAudioSpeed clamp. No field (0): no transfer, current behaviour (stop below threshold / start at the new cell, speed clamp).
