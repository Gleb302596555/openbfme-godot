# PORT-106 (issue #266): W3DScriptedModelDraw MultiPlayerOnly — the module's per-frame body runs only in {LAN, INTERNET}
# (RW 0x4C72E1..0x4C730C of doDrawModule RW 0x4C72BE; docs/ennorath/PORT106_RW_ANALYSIS.md). The tests for the
# openbfme_tests executable; the engine changes are in files other lists already carry (W3DModelDraw, W3DScriptedModelDraw,
# Drawable, DrawableManager, GameLogic).

set(OPENBFME_PORT106_TESTS
    tests/test_port_106.cpp
)
