# PORT-158 (issue #376): the loading-screen music plays together with the skirmish menu music. RotWK's AptLoadScreen::init
# (RW 0x81CB08) stops the playing music with a fade on the one TheAudio (RW 0x81D242) before adding the faction's
# LoadScreenMusic (RW 0x81D278); the port's menu music can sit in a second (shell) manager and its fade counted ticks a
# synchronous load drops. The engine change is in files other lists already carry (Common/Audio/AudioEntryPoints,
# Common/Audio/GameAudio, GodotDevice/GodotAptPlayer, GodotDevice/GodotGameAudio, godot/scripts/game.gd).

set(OPENBFME_PORT158_TESTS
    tests/test_port_158.cpp
)
