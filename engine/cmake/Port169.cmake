# PORT-169 (issue #398, P-102): CommandButton toggle index (+0xFC) for UnitSpecificSound / SetAutoAbilityUnitSound / UnsetAutoAbilityUnitSound /
# TOGGLE_IMAGE_ON_WEAPONSET / TOGGLE_IMAGE_ON_FORMATION / TOGGLE_NO_AUTO_ACQUIRE.
# No new engine source files: changes sit in ControlBarCommands.{h,cpp}, ControlBar.{h,cpp}, ControlBarCommandProcessing.cpp, HordeContainRuntime.h,
# and WeaponSetToggle.cpp which existing targets already carry.
set(OPENBFME_PORT169_TESTS
    tests/test_port_169.cpp
)
