# PORT-84 (issue #232): the unit voice of the combine-with-horde order (RW 0x8DEDBB case 0x8DF2EB, MSG_COMBINE_HORDES_WITH_OBJECT).
# The engine change is confined to GameClient/UnitVoiceResponse.cpp (the branch, handles(), voiceTargetArgument, the S-703 stop line); the lane adds no sources.

set(OPENBFME_PORT84_TESTS
    tests/test_port_84.cpp)
