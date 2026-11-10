# PORT-88 (issue #236, Unit voice: VoiceMoveOverWalls / RW 0x8DE628): synthetic tests for openbfme_tests.
# Engine changes live in src/GameClient/UnitVoiceResponse.{h,cpp} (lane AUDIO-2, already in Audio.cmake):
# moveRequiresClimbingWalls (RW 0x8DE628) and the moveVoices dispatch (RW 0x8DECBB .. 0x8DED2F) for rows 5 / 29.

set(OPENBFME_PORT88_TESTS
    tests/test_port_88.cpp)
