# PORT-9 (issue #62, TransportContain/CollidePickup — OpenContainModuleData + 0x81): tests for the openbfme_tests executable. The lane's engine change is in
# files other lists already carry (src/GameLogic/Object/Contain/OpenContainRuntime.h and src/GameLogic/Object/Contain/OpenContainRuntime.cpp,
# OpenContain::onCollide RW 0x8659D8 .. RW 0x865A51 and OpenContain::tryToEvacuate RW 0x867C40).

set(OPENBFME_PORT9_TESTS
    tests/test_port_9.cpp)
