# PORT-427 (issue #427): a porcupine formation's members face outward, each its own radial bearing (RotWK 2.01 RW 0x873FE8 -> the RW 0x8740C1 branch;
# the horde's member pass is src/GameLogic/Object/Contain/HordeMemberPass.cpp, already in OPENBFME_MOVE_SOURCES). The lane's engine change sits in that file.

set(OPENBFME_PORT427_TESTS
    tests/test_port_427.cpp
)
