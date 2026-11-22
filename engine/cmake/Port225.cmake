# PORT-225 (issue #548, weapon delivery / FiringTracker): tests for the openbfme_tests executable. The lane's engine change is in
# src/GameLogic/Object/WeaponState.cpp (FiringTracker::shotFired and FiringTracker::update), a file other lists already carry.

set(OPENBFME_PORT225_TESTS
    tests/test_port_225.cpp)
