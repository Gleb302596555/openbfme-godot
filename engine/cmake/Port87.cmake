# PORT-87 (issue #235): the unit voice's VoiceStartCharging (0x7EA) and VoiceNoBuild (0x7EB) on synthetic templates: the dispatcher's two
# branches (RW 0x8DFD0D / RW 0x8DFE7B) and the refused placement click's post (RW 0x83EAA0). The lane's engine changes are in files other
# lists already carry (UnitVoiceResponse, PlaceEventTranslator, HudContext, InGameHud).

set(OPENBFME_PORT87_TESTS
    tests/test_port_87.cpp)
