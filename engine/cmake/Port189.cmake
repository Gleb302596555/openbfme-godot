# PORT-189 (issue #455): Palantir/side-bar command button click drops the ControlBar help provider
# (ParasiticInGameCommandButton::Impl left/right click RW 0x9D282D / 0x9D28A3 -> RW 0x8079BB -> RW 0x8076CD)
# and ControlBar preserves click-predicted toggle index (+0xFC, RW 0x941367 / 0x941A22 / 0x9414E7 / 0x941C16)
# across HUD frames until authoritative state changes or context/dirty refreshes (RW 0x71FD81).
# No new engine source files: changes sit in InGameHelpBox.{h,cpp}, InGameHud.{h,cpp}, ControlBar.{h,cpp},
# and ControlBarCommandProcessing.cpp which existing targets already carry.
set(OPENBFME_PORT189_TESTS
    tests/test_port_189.cpp
)
