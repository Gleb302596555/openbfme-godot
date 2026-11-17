# PORT-245 (issue #569): the five W3DScriptedModelDraw fields RotWK parses and reads but the port stored without a consumer —
# MinLODRequired (RW 0x4B4262), HighDetailOnly (RW 0x4C7312 / 0x4C0993), UseProducerTexture (RW 0x4C23B1, left: S-5015),
# UseDefaultAnimation (RW 0x4C5235), BirthFadeAdditive (RW 0x4C09F6). The tests for the openbfme_tests executable; the engine
# changes are in files other lists already carry (W3DScriptedModelDraw).

set(OPENBFME_PORT245_TESTS
    tests/test_port_245.cpp
)
