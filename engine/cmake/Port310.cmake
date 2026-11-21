# PORT-310 (issue #702): the ArmyDefinition settings of the skirmish AI — the reader proof and the one field whose chain is closed.
# The binary-fact tests name RW_GAME_DAT in their title, so they print SKIP when the retail image is unset; the parse, default and hash
# cases run on synthetic INI only.
set(OPENBFME_PORT310_TESTS
    tests/test_port_310.cpp)
