# PORT-1 (issue #53, Weapon/FinishAttackOnceStarted — WeaponTemplate + 0x161, RW 0x74C6A8 .. RW 0x74C6FB): tests for the openbfme_tests executable. The
# engine change is in src/GameLogic/AI/AIAttack.cpp, which other lists already carry. test_port1_finish_attack.cpp is the fork lane test,
# test_port_1.cpp the upstream merge queue synthetic test of the same change (upstream merge 2026-11-16).

set(OPENBFME_PORT1_TESTS
    tests/test_port1_finish_attack.cpp
    tests/test_port_1.cpp)
