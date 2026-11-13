# PORT-125 (P-52, issue #309): roads and burn scars are drawn over smoke.
# The terrain decal render priorities (roads, scorches, tracks) are moved below the particle
# priority range (-128 .. -9) so that smoke draws on top, as in RotWK (RW 0x4E2E00 render order).

set(OPENBFME_PORT125_TESTS
    tests/test_port_125.cpp)
