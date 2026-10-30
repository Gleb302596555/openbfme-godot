# MODGAP-2: Ennorath module tests and runtime sources owned by agent B.
list(APPEND OPENBFME_SIM_SOURCES src/GameLogic/Module/BoredUpdate.cpp src/GameLogic/Module/FlingPassengerSpecialAbilityUpdate.cpp)
set(OPENBFME_MODGAP2_TESTS
    tests/test_modgap2_bored.cpp
    tests/test_modgap2_fling.cpp
    tests/test_modgap2_troll_animation.cpp
)
