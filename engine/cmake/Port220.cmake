# PORT-220 (issue #529): Object + 0x45B, the single-use command flag — getCommandAvailability's gate (RW 0x942826) and processCommandUI's writer
# (RW 0x94066B). The lane's engine changes are in files other lists already carry (src/GameLogic/Object/Object.h, src/GameLogic/Object/Object.cpp,
# src/GameClient/ControlBar.cpp, src/GameClient/ControlBarCommandProcessing.cpp, src/GameClient/ControlBarCommands.h); this file adds the lane's tests only.

set(OPENBFME_PORT220_TESTS
    tests/test_port_220.cpp)
