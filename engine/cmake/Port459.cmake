# PORT-459 (issue #459, stop S-1228): WeaponModeSpecialPowerUpdate's start resets the object's FiringTracker (RW 0x8984AF -> RW 0x8984B7 -> RW 0x8E302A(1))
# and the mode's InitiateSound is answered by the special power interface's slot 0x40 (RW 0xC650F0[16] = RW 0x991626, the unit voice asks it at RW 0x8DF846);
# DualWeaponBehavior's victim without UseRealVictimRange is the worker RW 0x85E0C2 -> RW 0x701443 with flags 0x62 (CAN_ATTACK | UNFOGGED | AUTO_ACQUIRE).
# The lane's engine changes are in a file other lists already carry (GameLogic/Module/HeroAbilityModules.cpp).
set(OPENBFME_PORT459_TESTS
    tests/test_port_459.cpp
)
