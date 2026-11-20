# PORT-275 (fork issue 642): the mirror's DissentLocked / WardByMirror buttons outside the pushed
# visible range. The bar's placement is the button slot minus the range start (RW 0x97D718: the
# empty range stack gets (0, InitialVisible); RW 0x71CF0A reads its top), so every button outside
# that window is not on the bar — in RotWK too. The regression port guarantees the by-design rule
# after the deep-search sweep's CommandSetUpgrade (RW 0x8B7C68): the override set is re-evaluated,
# not leaked one client frame. Tests on synthetic INI, no game files.
set(OPENBFME_PORT275_TESTS
    tests/test_port_275.cpp)
