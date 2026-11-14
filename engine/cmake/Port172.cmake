# PORT-172 (issue #425): the skirmish AI position sampler must not hold an object the game no longer has.
# The lane's engine changes are in files other lists already carry (AIStatCollector, AIPositionSampler).

set(OPENBFME_PORT172_TESTS
    tests/test_port_172.cpp)
