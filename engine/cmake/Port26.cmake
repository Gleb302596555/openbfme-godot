# Port 26 (issue #98): AODHordeContain member jitter, rank path history, large-unit height lift, and oath-fulfilled scatter (RW 0x87D55F..0x87EBEE).

# simulation sources (sim audit manifest)
list(APPEND OPENBFME_SIM_SOURCES
    src/GameLogic/Object/Contain/AODHordeContain.cpp
)

set(OPENBFME_PORT26_TESTS
    tests/test_port_26.cpp
)
