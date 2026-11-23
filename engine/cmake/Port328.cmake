# PORT-328 (issue #845): the open stops of Player, PlayerTemplate, GameInfo, SidesList and GlobalData — the mechanisms this lane ports
# are in Common/PlayerTemplate.cpp (S-521: IntrinsicSciences checked at parse time, RW 0x73B4A0 / 0x73A386) and GameLogic/Economy.cpp
# (S-482: the upgrade walk goes through the player's team prototypes and teams, RW 0x6AE483 / RW 0x6AE546).

set(OPENBFME_PORT328_TESTS
    tests/test_port_328.cpp)
