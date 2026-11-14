# PORT-63 / issue #173: W3DSailModelDraw's matrix hook (RW 0x4B6B26 -> 0x4D00CD).
# The sail runtime is client-only; no simulation state or xfer format changes.
set(OPENBFME_PORT63_SOURCES
    src/GameEngineDevice/W3DDevice/GameClient/Drawable/Draw/W3DSailModelDraw.cpp)
set(OPENBFME_PORT63_TESTS
    tests/test_port_63.cpp)
