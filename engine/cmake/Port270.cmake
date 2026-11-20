# PORT-270 (issue #637, P-152): several units selected — a bought upgrade they all have stays active
# until reselected. RotWK's updateContextMultiSelect (RW 0x944911 .. 0x944B87) re-evaluates every command
# window for every selected object each refresh (getCommandAvailability RW 0x942733; a window with 0
# available objects is disabled at RW 0x944B45), so an OBJECT upgrade queued for the whole selection greys
# the button on the same client frame (RW 0x71FD81 refresh; the port's ControlBar::update() is unconditional).
# No new engine source files: the change sits in ControlBar.cpp.
set(OPENBFME_PORT270_TESTS
    tests/test_port_270.cpp
)
