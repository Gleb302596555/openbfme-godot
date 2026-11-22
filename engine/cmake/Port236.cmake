# PORT-236: Victory, score and crates (issue #559; OPENBFME_PORT236_TESTS)
list(APPEND OPENBFME_SIM_SOURCES
    src/GameLogic/System/VictorySystem.cpp
    src/GameLogic/System/CrateSystem.cpp
)
set(OPENBFME_PORT236_TESTS
    tests/test_port_236.cpp
)
