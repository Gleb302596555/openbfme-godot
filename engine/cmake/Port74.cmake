# PORT-74 (issue #206): W3DLaserDraw - the lightning / beam draw. The module data (constructor RW 0x4C917B, field table RW 0xBE1798,
# 16 rows) and the beam geometry doDrawModule RAW 0x4C8A6C computes (the point pair, width, fan width, colour and texture index of
# every line), with the envelope RW 0x740659. Client / draw only: the source sits under src/GameEngineDevice/W3DDevice/ (excluded
# from the simulation audit by tools/sim/sim_policy.json) and never touches simulation state.

set(OPENBFME_PORT74_SOURCES
    src/GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DLaserDraw.cpp
    src/GameClient/LaserUpdate.cpp)

set(OPENBFME_PORT74_TESTS
    tests/test_port_74.cpp
    tests/test_local_34.cpp)
