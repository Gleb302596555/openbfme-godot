# PORT-313 (issue #749, S-1983 / S-1992): the hold-fire / auto-acquire button — its image toggle
# (RW 0x940435 case 0x30 -> RW 0x941367 -> RW 0x75D366 -> RW 0x75D22E's 0x800000 arm, RW 0x75D2CF ..
# RW 0x75D2E1) and the logic flag with the related object the member's SlaveAttackNugget weapon names
# (RW 0x77B6C4 .. RW 0x77B82A, RW 0x693C2D). The lane's engine changes are in files other lists
# already carry (GameClient/ControlBar*.cpp, GameLogic/AI/WeaponCommands.cpp).
set(OPENBFME_PORT313_TESTS
    tests/test_port_313.cpp)
