# PORT-98 (issue #258): PlayerTemplate LoadScreenMusic — each faction's music on the loading screen. The reader is the players' loading screen's init
# (RotWK AptLoadScreen::init RW 0x81CB08: the local slot's template RW 0x81D1B8 .. 0x81D203, TheAudio 0x81D225 findAudioEventInfo, 0x81D242 stopMusic
# 0x8C(2, 1, 1), 0x81D278 addAudioEvent, 0x81D283 update). The engine change is in files other lists already carry
# (GUI/LoadScreenInfo, GUI/ShellServices, GUI/AptScreens/AptSimpleScreens, GUI/Skirmish/*, GodotDevice/GodotAptPlayer).

set(OPENBFME_PORT98_TESTS
    tests/test_port_98.cpp
)
