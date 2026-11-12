# PORT-141 (issue #343, task #117): Isengard defiled-ground ambient sound (SoundAmbient) must play at game start.
#
# RW analysis (workspace/RotWK/game.dat, caveat S-001):
#   * ThingTemplate audio field table (RW 0x00C26720, 56 entries): row 37 = SoundAmbient, fieldOff=0x128,
#     string 0x00C263E0, parser 0x0073AD07 -> 0x0073ACAF -> 0x0073AB45 (NoSound clears, +SOUND: strips prefix,
#     EVA: invalid on sound rows >= 0x21), stored at ThingTemplate + 0x2DC (+0x124 + 0x37*8).
#   * The port reads this row via UnitSpecificSound::templateSound (LiveGameAudio::soundField): the field IS read.
#   * RotWK starts an object's ambient at drawable creation: Drawable::enableAmbientSound (RW 0x002785D6) sets
#     m_448; rva002784AF checks m_447&m_448&m_44A then starts the sound (rva00278440).
#   * The port starts the ambient in LiveGameAudio::update() (ZH Drawable::startAmbientSound): SoundAmbient for
#     the body damage state, started for objects complete + alive + not under construction, range-gated unless
#     global/critical; no engine source is added beyond files other lanes already carry.
#
# Gap: the port relies solely on the LiveGameAudio::update() polling scan. Objects that exist at game start
# (Isengard fortress / citadels) must get their ambient started on that first scan; an ambient whose audio
# event is not yet loaded must not be permanently silenced (retry instead of giving up).
set(OPENBFME_PORT141_TESTS
    tests/test_port_141.cpp
)
