# PORT-322: GameLogic's open stops around the AI row of phase 5 and the end-of-frame pass. The engine changes are in files other lists already
# carry (GameLogic.cpp/.h, AI/AIWorld.cpp); this lane adds the tests only (docs/ennorath/night_watch.d/PORT-322.md).

set(OPENBFME_PORT322_TESTS
    tests/test_port_322.cpp)
