# PORT-214 (fork issue 504): a hero's voice lines are quieter than the rest of the roster because his own AudioEvent
# entries carry a lower Volume / MinVolume pair — the volume chain of RW 0x4591D4 reproduces exactly that.
# No engine source changes: the lane only adds its tests.

set(OPENBFME_PORT214_TESTS
    tests/test_port_214.cpp)
