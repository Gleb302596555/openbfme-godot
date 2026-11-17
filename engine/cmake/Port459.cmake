# Port 459 Test Configuration (Issue #459)
# Goal: Match Ennorath mod behavior in Godot port to RotWK 2.01
# Targets: WeaponModeSpecialPowerUpdate and DualWeaponBehavior

set(PORT459_TEST_SOURCES
    engine/tests/test_port_459.cpp
)

# Include GoogleTest if available in the build environment
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../tests/gtest)

add_executable(openbfme_tests_port459 ${PORT459_TEST_SOURCES})

target_link_libraries(openbfme_tests_port459
    openbfme_engine
    gtest
)

# Add test cases to CTest
add_test(NAME PORT-459-WeaponModeSpecialPowerUpdate COMMAND openbfme_tests_port459 --gtest_filter=WeaponModeSpecialPowerUpdate.*)
add_test(NAME PORT-459-DualWeaponBehavior COMMAND openbfme_tests_port459 --gtest_filter=DualWeaponBehavior.*)