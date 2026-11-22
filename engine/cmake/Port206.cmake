# PORT-206 (fork issue 488): the weapon bonus conditions of Object + 0x39C and who writes them. The lane's engine change is the new upgrade module
# WeaponBonusUpgrade (the PLAYER_UPGRADE writer, RW 0x8B980D); the classes it hooks into (UpgradeModule, Object's mask, ExtraModules' registration list)
# live in lists this file does not touch.

list(APPEND OPENBFME_SIM_SOURCES src/GameLogic/Module/WeaponBonusUpgrade.cpp)

set(OPENBFME_PORT206_TESTS
    tests/test_port_206.cpp)
