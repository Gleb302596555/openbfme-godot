# PORT-205 (a HordeAttackNugget's member and a DamageContainedNugget's contain slot 0x1C, the two stop S-1582 inferences of fork issue 472). The lane's
# engine changes are in files other lists already carry (GameLogic/Combat/WeaponDelivery.cpp, GameLogic/Module/BehaviorModule.h,
# GameLogic/Object/Contain/HordeBanner.cpp, GarrisonContainRuntime.h, HordeContainRuntime.h); this file adds the lane's tests only.

set(OPENBFME_PORT205_TESTS
    tests/test_port_205.cpp)
