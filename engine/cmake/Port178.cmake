# PORT-178 (fork issue 431, P-76): a looping sound that loses its channel comes back, as in RotWK.
#
# RW analysis (workspace/RotWK/game.dat; the tables are in docs/ennorath/PORT178_RW_ANALYSIS.md):
#   * RW 0x45FF7F (the channel steal): the victim with loops left (RW 0x6DAD59) goes back to the requests (RW 0x45A188) before it is released.
#   * RW 0x45B4E8 / 0x45B446 / 0x450E81: "lower" is an order on (priority, volume with the sliders); the lowest priority has no exception.
#   * RW 0x45BC4E: the inaudible cull asks the volume without the sliders (RW 0x45A10E(playing, 0, 0)); RW 0x452726 keeps a sound whose
#     MinVolume is at or above MinSampleVolume.
# The engine change is in Common/Audio/GameAudio.cpp, which other lists already carry.
set(OPENBFME_PORT178_TESTS
    tests/test_port_178.cpp)
