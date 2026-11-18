# PORT-256 (fork issue 604): the two weapon-switching powers, WeaponModeSpecialPowerUpdate and WeaponChangeSpecialPowerModule.
# The lane's engine changes are in files other lists already carry (GameLogic/Module/HeroAbilityModules.cpp, WeaponChangeSpecialPower.cpp); this file
# adds the lane's tests only.

set(OPENBFME_PORT256_TESTS
    tests/test_port_256.cpp)
