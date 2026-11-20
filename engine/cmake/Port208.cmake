# PORT-208 (issue #490): force attack out of garrisons and spawners and the clicked map tree as a target (stop S-2480). The engine changes are in
# AIGroup.cpp/.h (RW 0x77208A / 0x77229A), AICommands.cpp (RW 0x77AF18 / 0x77C1C2), ObjectWeapons.cpp (RW 0x6C8652 / 0x68B5FA),
# GarrisonContain.cpp (RW 0x87B7E3 / 0x664CB3), AIUpdateCombat.cpp (RW 0x66DE32 / 0x660E93), HudObjects.cpp (RW 0x48ACF5 .. 0x48ADA0) and
# CommandXlat.cpp (RW 0x81E4B5), which the existing source lists carry.

set(OPENBFME_PORT208_TESTS
    tests/test_port_208.cpp)
