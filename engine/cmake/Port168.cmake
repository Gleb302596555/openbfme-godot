# PORT-168 (fork issue 397, "Trolls: eating an orc does not heal and the grabbed goblin is never thrown"): synthetic tests for the openbfme_tests
# executable. The engine changes are the grab ability's GrabPassengerHealGainPercent parser row (RW 0xC561C0 -> RW 0x42ED6F, no 0.01 scale;
# INI::parseNonNegativeReal) and the weapon's container ammo: WeaponHost::containedAmmo / ownerHasContainAmmo (RW 0x6CD298 / RW 0x6CD11F, contain slot
# 0x114 RW 0x865CE3) and the projectile deliver's container branch (RW 0x90FA81 .. 0x90FAC9, contain slot 0x13C RW 0x866367).

set(OPENBFME_PORT168_TESTS
    tests/test_port_168.cpp)
