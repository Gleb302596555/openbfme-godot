# PORT-261 (fork issue #609, upstream #609): the player-side powers PlayerHealSpecialPower (heal / repair the player's objects) and
# StoreObjectsSpecialPower (store and release units). No engine sources are added by this lane: the port of both classes already matches
# RotWK 2.01 (see the lane's PR); the tests pin that. tests only.
set(OPENBFME_PORT261_TESTS
    tests/test_port_261.cpp)
