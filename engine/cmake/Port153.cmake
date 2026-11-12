# PORT-153 (issue #371): the ArmyDefinition fields the skirmish AI reads — and the nine it never reads in RotWK 2.01.
# The three reader proofs are binary-fact tests (they name RW_GAME_DAT, so they SKIP without the retail image);
# the parse, hash and default-value cases run on synthetic INI only.
set(OPENBFME_PORT153_TESTS
    tests/test_port_153.cpp)
