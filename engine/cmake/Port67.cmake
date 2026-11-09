# PORT-67 (issue #181, Weapon/CanSwoop and the Locomotor Swoop rows): synthetic-INI tests for openbfme_tests.
# RW verdict, with addresses: the weapon byte +0x83 (table RW 0xC16DD8, parser RW 0x42E558, getter RW 0x6CA152 with
# no callers) and the four Locomotor rows +0x124 / +0x128 / +0x12C / +0x130 (table RW 0xBF4478) are read by no
# simulation code in RotWK 2.01: only the parse table, the template's ctor RW 0x5E4326, its copy RW 0x5E4653 and its
# assignment RW 0x5E31DF touch them. docs/ennorath/port67-swoop-rw.md holds the reading; the tests pin the parse, the
# ctor defaults and the fact that nothing else moves.
set(OPENBFME_PORT67_TESTS
    tests/test_port_67.cpp)
