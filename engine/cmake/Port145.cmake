# PORT-145 (issue #362, docs/ennorath/PROBLEMS.md P-90): a unit ordered into a SHIP boards it only while the ship sits idle at the shore. The lane's
# engine change is the SHIP branch of AIMoveToPositionAndEnterState (state 56): the destination adjustment in onEnter (RW 0x751A8A, call 0x6FE456)
# and the ship's AI state + shore test in update (RW 0x756452, calls 0x660AC1 / 0x6EFBB8). The file already carries the lane (AIGarrisonStates.cpp).

set(OPENBFME_PORT145_TESTS
    tests/test_port_145.cpp)
