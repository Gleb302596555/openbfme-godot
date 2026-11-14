# PORT-166 (issue #395, P-64 "Erkenbrand's Time to Strike: the cooldown starts on the order, no charge, slow afterwards"): the FIRE_WEAPON charge of a
# CanFireWhileCharging weapon end to end - the reload starts at the strike (RW 0x751A07), the run is the PANIC set (RW 0x7519FE), the speed after the
# charge is the speed before the order (RW 0x74D8C8). Synthetic-INI tests for openbfme_tests, no game files.
set(OPENBFME_PORT166_TESTS
    tests/test_port_166.cpp)
