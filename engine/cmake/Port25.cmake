# PORT-25 (issue #97, HordeContain/VisionRearOverride + VisionSideOverride — module data + 0x274 / + 0x278, RW 0x872B9E / 0x872BAB, read by
# the object's look RW 0x694B9B / 0x694B79): tests for the openbfme_tests executable.
# The lane's engine change is in files other lists already carry (src/GameLogic/System/ShroudManager.cpp, the retail look;
# src/GameLogic/Object/Contain/HordeContainRuntime.cpp, the two interface getters).

set(OPENBFME_PORT25_TESTS
    tests/test_port_25.cpp)
