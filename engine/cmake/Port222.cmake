# PORT-222 (fork issue 532): the experience a LevelGrantSpecialPower gives lands as RotWK 2.01 gives it — Cirdan's King's Favor on a horde
# fills the horde's pool (RW 0x8C6E82 -> the horde contain's addExperience RW 0x873A02), as every experience-granting module of the mod does.
# The engine change under test is LevelGrantSpecialPower::grant (GameLogic/Module/SpecialAbilityModules.cpp); the ability update, the module
# trigger and HordeContain::addExperience are not given a second handler.

set(OPENBFME_PORT222_TESTS
    tests/test_port_222.cpp)
