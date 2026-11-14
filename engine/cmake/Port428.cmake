# PORT-428 (issue #428, P-111): the aerial path's would-collide filter is RotWK's 3D shape overlap
# (separation RW 0x777F9F -> filter allow RW 0x66139C -> RW 0xAD2CE0), not the 2D footprints of
# S-1020. A GIANT_BIRD flyer above a low NO_COLLIDE supply pile no longer reports a blocked run at
# every altitude, so the swoop strikes instead of circling. Synthetic-INI tests, no game install.
set(OPENBFME_PORT428_TESTS
    tests/test_port_428.cpp)
