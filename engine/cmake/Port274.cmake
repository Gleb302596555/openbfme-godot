# PORT-274 (issue #641): a FIRE_WEAPON command button whose Options need an object target (NEED_TARGET_ENEMY_OBJECT and its
# neutral / ally siblings) through the player's path: the press enters the GUI command mode (RW 0x940877 .. 0x94089C) and the left
# click is turned into MSG_DO_WEAPON_AT_OBJECT (RW 0x83D41A translator, RW 0x77A34C logic) with the button's WeaponSlot (+0x80),
# the picked target and MaxShotsToFire (+0xA0). The lane's engine change is in src/GameClient/MessageStream/GUICommandTranslator.cpp
# (a file other lists already carry); this file adds the lane's tests only.

set(OPENBFME_PORT274_TESTS
    tests/test_port_274.cpp)
