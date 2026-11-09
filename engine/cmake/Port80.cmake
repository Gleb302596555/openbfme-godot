# PORT-80 (issue #212, InvisibilityUpdate voice redirect): the stealthy-area move voice (RW 0x8DDBC4, donor BFME2 0x004D990E; the stealthy test RW 0x81ACAA)
# inside the move voice selection. Synthetic tests, no retail installation. The engine change is the stealthyAreaMoveVoice of GameClient/UnitVoiceResponse.cpp
# and the InvisibilityManager::stealthyAt / nearTreesAt of GameLogic/System/InvisibilityManager.cpp.

set(OPENBFME_PORT80_TESTS
    tests/test_port_80.cpp)
