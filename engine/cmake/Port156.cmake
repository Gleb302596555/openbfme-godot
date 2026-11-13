# PORT-156: PlayerTemplate SpellStoreCurrentPowerLabel / SpellStoreMaximumPowerLabel, LoadScreenImage, ScoreScreenImage,
# SideIconImage — the five rows of RW 0xBF81A8 that RotWK 2.01 parses (+ 0x1C0 / + 0x1C4 / + 0x164 / + 0x168 / + 0x178) and reads
# nowhere (PlayerTemplate's accessors RW 0x5F3FB5 and RW 0x5FC935 have no caller, there is no accessor for + 0x164 / + 0x168, and the
# spell book and the loading screen touch none of them; issue #374). The lane changes no engine source: it pins the verdict on the
# port's own paths, no second mechanism is added.

set(OPENBFME_PORT156_TESTS
    tests/test_port_156.cpp)
