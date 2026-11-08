# PORT-16 (issue #72): synthetic supply condition and prerequisite tests.
list(APPEND OPENBFME_SIM_SOURCES
    src/GameLogic/AI/PlayerSupplyAttack.cpp)
set(OPENBFME_PORT16_TESTS
    tests/test_port_16.cpp)
