# PORT-130 (issue #332): CommandButton AffectsKindOf — the NEEDS_CASTLE_KINDOF filter in
# ControlBar::getCommandAvailability (RW 0x942993..0x942A1B, ANY intersection RW 0x661359).
# The engine changes are in files other lanes already build (ControlBar, ControlBarCommands);
# this lane adds only its tests.
set(OPENBFME_PORT130_TESTS
    tests/test_port_130.cpp)
