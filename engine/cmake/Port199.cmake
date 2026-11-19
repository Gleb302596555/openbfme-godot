# PORT-199 (issue #466): GiveUpgradeUpdate delivers to contained targets and through the producer-linked pair, gives to
# a horde's members, deselects the porter and flashes the recipient as RotWK 2.01 does. The lane's engine changes are in
# files other lists already carry (GameLogic/Module/GiveUpgradeUpdate.cpp, GameLogic/Object/Contain/HordeContainRuntime.cpp).

set(OPENBFME_PORT199_TESTS
    tests/test_port_199.cpp)
