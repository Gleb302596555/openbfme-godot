# PORT-155 (issue #373, task #131): make the port apply RotWK's reverb send, dry level and offscreen
# occlusion from the AudioEventInfo fields ReverbEffectLevel, DryLevel and ZoomedInOffscreenOcclusionPercent
# when playing sounds.
#
# RW analysis (workspace/RotWK/game.dat, caveat S-001):
#   * Reader fn RW 0x7767A9 (sound playback): info ptr = [ecx+4] (RW 0x7767BF).
#   * ZoomedInOffscreenOcclusionPercent: table row T RW 0xBEFF40, parser P RW 0x42EEFA (parsePercentToReal),
#     storage offset +0xA4; reader pushes [edi+0xA4] (RW 0x776AA5) into the audio call RW 0x5DD9EE.
#   * ReverbEffectLevel: table row T RW 0xBEFF50, parser P RW 0x42EEFA, storage offset +0xA8; reader pushes
#     [edi+0xA8] (RW 0x776B1A) into call RW 0x5DD9EE.
#   * DryLevel: table row T RW 0xBEFF60, parser P RW 0x42EEFA, storage offset +0xAC; reader pushes
#     [edi+0xAC] (RW 0x7769D1) into call RW 0x5DD9EE.
#   * Same reader serves audioevent and ambientstream (shared table). Direct PE table inspection on game.dat
#     confirmed T at 0xBEFF40/0xBEFF50/0xBEFF60, parser 0x42EEFA, prevOffsets +0xA4/+0xA8/+0xAC; the table in
#     AudioEventInfo.cpp already has all three rows at the correct offsets.
#
# Gap fixed: the port parsed all three fields (AudioEventInfo.cpp lines 156-158) but never applied them - voices
# played dry. voiceParamsFor (GameAudio.cpp) now fills VoiceParams{reverbSend, dryLevel, occlusion} from the event
# info (reverb send and dry level straight from the info; the offscreen occlusion only while zoomed in and the
# source is offscreen, using the same zoom/offscreen query as the volume fade), and GodotAudioDevice::applyParams
# scales the voice volume by dryLevel.
#
# Synthetic data only: no retail file is read, so the tests pass without the game and the mod.
set(OPENBFME_PORT155_TESTS
    tests/test_port_155.cpp
)
