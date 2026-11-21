# PORT-229 (issue #552): Hordes and contains - garrison, transport, tunnels, horde membership and slaughter, as RotWK.
# Synthetic hordes and containers per item (enter, exit, split, member death, slaughter), expectations from RW with the address in a comment.
# Lane HORDE-1: the HordeContainCore slot table, member <-> slot maps, free list, rank fill-in and slot world positions.
# No engine source changes in this lane: the lane only adds its tests.

set(OPENBFME_PORT229_TESTS
    tests/test_port_229.cpp)
