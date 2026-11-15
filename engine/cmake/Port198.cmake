# PORT-198 (issue #465, stops S-1425, S-653, S-1101, S-2480, S-272, S-286, S-7387): SpawnBehaviorInterface slots 2..6 / 10,
# SPAWNS_ARE_THE_WEAPONS targeting/orders, WeaponSet::getAbleToUseWeaponAgainstTarget, and Player::onUnitCreated -> SkirmishAIManager::unitCreated.

set(OPENBFME_PORT198_TESTS
    tests/test_port_198.cpp)
