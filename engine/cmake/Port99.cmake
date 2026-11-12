# PORT-99: PlayerTemplate/LightPointsUpSound — parsed and stored, with its only reader in RotWK 2.01 inside the light-point (War of the Ring) script
# action PLAYER_GIVE_LIGHT_POINTS (RW 0x7BD2D0 -> RW 0x6AB148 -> RW 0x6AB178; issue #259). PORT-113 (issue #280, cmake/Port113.cmake) ported that
# action and its sound. The tests here pin that the skirmish power-point path requests no sound; no engine source is added by this lane
# (docs/ennorath/PORT99_RW_ANALYSIS.md).

set(OPENBFME_PORT99_TESTS
    tests/test_port_99.cpp
)
