# TheTaintManager (the Taint / Elven Wood ground grid and the radius clear; stops S-5260 / S-921 / S-856): sources and tests of the openbfme_core library
# and the test executable. Included from engine/CMakeLists.txt so the file lists stay out of the shared list.

# simulation sources (sim audit manifest): the grid is logic state (the aura's RequiredConditions read it)
list(APPEND OPENBFME_SIM_SOURCES
    src/GameLogic/System/TaintManager.cpp
)

set(OPENBFME_TAINT_TESTS
    tests/test_taint_manager.cpp
)
