# PORT-1 (issue #53, Weapon/FinishAttackOnceStarted — WeaponTemplate + 0x161): tests for the openbfme_tests executable. The lane's engine change is in a
# file other lists already carry (src/GameLogic/AI/AIAttack.cpp, AIAttackFireState::update; RW 0x74C6A8 .. RW 0x74C6FB).

set(OPENBFME_PORT1_TESTS
    tests/test_port1_finish_attack.cpp)
