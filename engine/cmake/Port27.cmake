# PORT-27 (issue #99, Weapon/ProjectileStreamName — WeaponTemplate + 0x178, RW 0x6CD1FD called from the fire at RW 0x6CF012, the record per shot at
# RW 0x90FB7A .. 0x90FC5A, the destructor at RW 0x6CD0BE): tests for the openbfme_tests executable.
# The lane's engine change is in files other lists already carry (src/GameLogic/Object/WeaponState.cpp, src/GameLogic/Combat/ObjectWeapons.cpp,
# src/GameLogic/Combat/ProjectileLauncher.cpp).

set(OPENBFME_PORT27_TESTS
    tests/test_port_27.cpp)
