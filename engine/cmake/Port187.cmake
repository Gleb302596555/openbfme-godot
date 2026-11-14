# PORT-187 (fork issue 453): the GIANT_BIRD strike pass — the turn back after a hit (P-114) and the strike on a
# building (P-117); tests for openbfme_tests. The engine change under test is GiantBirdAIUpdate's attack state
# (RW 0x76B668 / RW 0x76BA8B) and the aerial path the repeat pass flies (RW 0x778413 / RW 0x5E91E2), shared by
# every GIANT_BIRD flyer; no second handler is added.

set(OPENBFME_PORT187_TESTS
    tests/test_port_187.cpp)
