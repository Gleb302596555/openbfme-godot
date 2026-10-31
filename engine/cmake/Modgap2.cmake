# MODGAP-2: Ennorath module tests and runtime sources owned by agent B.
list(APPEND OPENBFME_SIM_SOURCES src/GameLogic/Module/FlingPassengerSpecialAbilityUpdate.cpp)
list(APPEND OPENBFME_SIM_SOURCES src/GameLogic/Module/LocomotorSetUpgrade.cpp)
list(APPEND OPENBFME_SIM_SOURCES src/GameLogic/Module/ReplaceObjectUpdate.cpp)
list(APPEND OPENBFME_SIM_SOURCES src/GameLogic/Module/ShipSlowDeathBehavior.cpp)
list(APPEND OPENBFME_SIM_SOURCES src/GameLogic/Module/StoreObjectsSpecialPower.cpp)
list(APPEND OPENBFME_SIM_SOURCES src/GameLogic/Module/SplitHordeSpecialPower.cpp)
list(APPEND OPENBFME_SIM_SOURCES src/GameLogic/Module/TeleportToCasterSpecialPower.cpp)
set(OPENBFME_MODGAP2_TESTS
    tests/test_modgap2_bored.cpp
    tests/test_modgap2_fling.cpp
    tests/test_modgap2_locomotor_upgrade.cpp
    tests/test_modgap2_replace.cpp
    tests/test_modgap2_ring_chain.cpp
    tests/test_modgap2_ship_death.cpp
    tests/test_modgap2_split.cpp
    tests/test_modgap2_store_teleport.cpp
    tests/test_modgap2_store_objects.cpp
    tests/test_modgap2_troll_animation.cpp
)
