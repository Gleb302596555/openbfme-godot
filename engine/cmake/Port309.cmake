# PORT-309 (issue #701, ExperienceLevel level-up tint — ShowLevelUpTint, LevelUpTintColor, LevelUpTintPreColorTime, LevelUpTintSustainColorTime,
# LevelUpTintPostColorTime): re-verification of S-7196 / PORT-44 (issue #148). All 7 tint rows of table RW 0xC11BF0 (0xC11C90..0xC11CF0) are parsed and
# zeroed/copied (RW 0x689A4E / 0x689FEA) and read nowhere in RotWK 2.01 (nor in DONOR BFME2 1.06), including gainExpForLevel RW 0x79DA0A -> checkLevels
# RW 0x79D141 -> onLevelReached RW 0x79DA9D -> grant RW 0x821319 and ExperienceLevelCreate RW 0x8BD350.
set(OPENBFME_PORT309_TESTS
    tests/test_port_309.cpp)
