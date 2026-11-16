# PORT-221 (issue #531, P-140): multi-selected horde abilities, weapon toggles, and mixed-selection HUD/dispatch
# (ControlBar::processCommandUI GUI_COMMAND_SPECIAL_POWER RW 0x9414E7..0x941618 emitting MSG_DO_SPECIAL_POWER with
# source = INVALID_ID (0) so SpellCommands::doSpecialPower RW 0x77A42C..0x77A4F7 dispatches across the issuer's
# selection; ControlBar multi-select context RW 0x71ECC4 / populateMultiSelect RW 0x94475A / addCommonCommands
# RW 0x944534 / updateContextMultiSelect RW 0x944911).
# No new engine source files: changes sit in ControlBar.cpp and ControlBarCommandProcessing.cpp.
set(OPENBFME_PORT221_TESTS
    tests/test_port_221.cpp
)
