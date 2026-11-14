# PORT-436 (the weapon aim at the victim's contact point, RW 0x6CB85A flag 1: the fire FX aim
# 0x6CCAC7, the Bezier launch 0x85EC51 and homing step 0x85F46A, the scatter aim on structures
# 0x6CC3EA; issue #436): tests for the openbfme_tests executable. The lane's engine changes are in
# files other lists already carry (GameLogic/Combat/WeaponDelivery.cpp, GameLogic/Module/
# ProjectileModules.cpp, GameLogic/Combat/CombatLineOfSight.cpp, GameLogic/Object/ObjectGeometry.cpp).

set(OPENBFME_PORT436_TESTS
    tests/test_port_436.cpp)
