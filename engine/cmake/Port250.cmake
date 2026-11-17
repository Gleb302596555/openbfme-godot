# PORT-250 (fork issue 578): a SPECIAL_POWER button whose power no module of the object carries. The
# availability arm RW 0x943343 of RW 0x942733 answers 1 = available when the power template exists and no
# module carries it (RW 0x9434F7 .. 0x943564 -> RW 0x942FF1), and 3 = hidden when the button's + 0x44 is
# null (RW 0x943356 -> RW 0x942C5D). The port answered both already; the arm's UnitCost gate
# (RW 0x943439 .. 0x943480: the final override's + 0x80 against the contain's horde member count,
# slot 0x188 at RW 0x943477) was missing, so a power spent from the horde showed a live button that only
# ActionManager (RW 0x82E000 / 0x82D605) refused. Tests on synthetic INI, no game files.
set(OPENBFME_PORT250_TESTS
    tests/test_port_250.cpp)
