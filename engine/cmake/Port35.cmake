# PORT-35 (issue #120, HordeContain/ComboHorde — two hordes combining into one): the row RW 0xC5BBA0, the parser RW 0x872654 (module data + 0x198), the entry
# lookup RW 0x87043B (callers RW 0x871EC1 / RW 0x871F67 / RW 0x8765CC) and the combine branch of interface slot 0x74 (RW 0x876481, RW 0x8765B4 .. 0x8769F7).
# The engine change is in HordeContainRuntime.cpp / .h (already in the HORDE-2 source list), so this lane carries tests only.
set(OPENBFME_PORT35_TESTS
    tests/test_port_35.cpp)
