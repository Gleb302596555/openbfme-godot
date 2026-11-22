# PORT-213 (issue #503): Multi-select Palantir context picks highest-rank qualifying HERO
# (`ControlBar::evaluateContextUI` RW 0x71EBDA..0x71EF9F -> `ControlBar::populateMultiSelect` RW 0x94475A..0x944910,
# `AptPalantir::setDrawable` RW 0x9443BD / 0x9448FC -> 0x6D4671, `PalantirCommandUI` RW 0x92FE33 / 0x9305CE / 0x92FA83 / 0x92FAA5).
# No new engine source files: changes live in `ControlBar.{h,cpp}`, `PalantirCommandUI.cpp`, and `InGameHud.cpp`.
set(OPENBFME_PORT213_TESTS
    tests/test_port_213.cpp
)
