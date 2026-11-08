# PORT-44 (issue #148, ExperienceLevel/ShowLevelUpTint — the level-up colour flash): verdict tests, no engine behaviour change. The tint's seven rows of the
# ExperienceLevel field table RW 0xC11BF0 (+0xD8 ShowLevelUpTint, +0xDC LevelUpTintColor, +0xE8 / +0xEC / +0xF0 the three times, +0xF4 / +0xF8 Frequency /
# Amplitude) are parsed and copied (constructor RW 0x689AE1, copy RW 0x689FEA) and read by no code of game.dat: checkLevels RW 0x79D141, onLevelReached
# RW 0x79DA9D and the delayed grant RW 0x821319 never touch them, and every colour-tint entry point of a drawable (RW 0x675749 / 0x671A73 / 0x674818) is
# called from code that holds no level. A level gained shows LEVELED (RW 0x8213DB) and the level's LevelUpFx (RW 0x8211EC) instead. Stop S-7186.
set(OPENBFME_PORT44_TESTS
    tests/test_port_44.cpp)
