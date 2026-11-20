# Port 283 (issue #650): HordeAIUpdate (RW 0x89DE6C..0x89E4A8) + HorseHordeContain (RW 0x879849..0x87A27E).

# simulation sources (sim audit manifest)
list(APPEND OPENBFME_SIM_SOURCES
    src/GameLogic/Object/Contain/HorseHordeContain.cpp
)

set(OPENBFME_PORT283_TESTS
    tests/test_port_283.cpp
)
