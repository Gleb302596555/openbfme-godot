# PORT-264 (fork issue 612): AIUpdateInterface's own uncited functions — the AI's state queries (AI vslot 0x1C4 RW 0x00662B50, isBusy RW 0x0066281A) and the
# guard getters (RW 0x0049A4D0, 0x00670317, 0x008F1161). The lane's engine changes are in files other lists already carry (AIUpdate.h, AIUpdateHordeMember.cpp,
# AIMove.h).

set(OPENBFME_PORT264_TESTS
    tests/test_port_264.cpp)
