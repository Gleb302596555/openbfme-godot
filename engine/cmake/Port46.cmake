# PORT-46 (issue #150, ToggleMountedSpecialAbilityUpdate/OpacityTarget: the fade while mounting and dismounting):
# synthetic-INI tests for openbfme_tests. RW 0x8B1142 (vtable 0xC6BDA0 slot 0x50) writes the PC24 curve to
# drawable + 0xB0; RW 0x8B1219 / 0x8B125F reset it. The neighbouring tables 0xC05B48 and 0xC05C10 are checked too.
set(OPENBFME_PORT46_TESTS
    tests/test_port_46.cpp)
