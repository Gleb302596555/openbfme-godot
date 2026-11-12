# PORT-148 (fork issue 365): the GIANT_BIRD flyer's ordered attack on a structure; tests for openbfme_tests.
# The engine change under test is in the existing GiantBirdAIUpdate / weapon-choice path, not a second handler.

set(OPENBFME_PORT148_TESTS
    tests/test_port_148.cpp)
