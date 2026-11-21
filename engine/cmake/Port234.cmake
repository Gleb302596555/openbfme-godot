# PORT-234: the four reward crate overrides. Their results are simulation state.
list(APPEND OPENBFME_SIM_SOURCES
    src/GameLogic/Module/RewardCrates.cpp)
set(OPENBFME_PORT234_TESTS tests/test_port_234.cpp)
