# LOCAL-177 (fork issue 958): binary map audit of 431 uncited RotWK AI/pathfinding functions
# + ports of AIUpdateInterface::getTurretRotAndPitch (RW 0x6623DB) and AIUpdateInterface::handleEngagedFlag (RW 0x665921).
set(OPENBFME_LOCAL177_TESTS
    tests/test_local_177.cpp)
