# PORT-216: the parse-time weapon contact point of compound/offset geometry (RW 0xAD25B0,
# 0xAD3172). Tests belong to the full openbfme_tests target, like PLAY-2. ObjectGeometry.cpp
# is already carried by the engine source lists; there is no separate test runner.
set(OPENBFME_PORT216_TESTS
    tests/test_port_216.cpp)
