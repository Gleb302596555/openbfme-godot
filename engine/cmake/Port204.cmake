# PORT-204 (issue #471): crush warning path gate (RW 0x6950F3 / 0x69519A: RW 0x664485 + 0x5E2DF4 bypass and RW 0x695159 -> 0x6F5BB0
# Pathfinder::quickDoesPathExist) and HordeContain +0x170 member list (RW 0x8704DA slot 0x10C for HordeNotifyTargetsOfImminentProbableCrushingUpdate
# RW 0x8D3167, plus RW 0x8743A9 -> 0x870C29 / RW 0x86EE52 / RW 0x874084 in HordeEmotion / HordeMemberPass).
# Engine changes live in files existing CMake lists already carry.

set(OPENBFME_PORT204_TESTS
    tests/test_port_204.cpp)
