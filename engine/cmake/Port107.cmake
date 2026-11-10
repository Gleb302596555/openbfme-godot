# PORT-107 (issue #267, Object UpgradeCameo1..5): RotWK 2.01's only reader of the fields — the
# completeUpgrade dirty mark (RW 0x8A22D5 .. 0x8A2328 -> TheControlBar +0x28) — ported as
# ControlBar::noteUpgradeCompleted; the retail panel never shows the slots (setPortraitByImage
# RW 0x71C56A has no caller; verdict S-7287). Tests pin the parse, the dirty decision, its
# negatives and the consume in update(). No game files needed.
# See docs/ennorath/port107-upgradecameo-rw.md; docs/STOPS.md S-7287.
set(OPENBFME_PORT107_TESTS
    tests/test_port_107.cpp)
