# PORT-247 (fork issue 571): TeleportSpecialAbilityUpdate/MaxDistance cuts the place the ability may be used at (RW 0x896608); the other two
# fields of the lane, FlingPassengerSpecialAbilityUpdate/FlingPassengerVelocity and AutoAbilityBehavior/WorkingRadius, are parsed and never read
# by RotWK 2.01 (docs/ennorath/PORT-247-RW-ANALYSIS.md). The engine change lives in files other lists already carry
# (SpecialAbilityModules, HeroAbilityModules, DeployStyleAIUpdate, SiegeModules, ActionManager): this lane only adds its tests.

set(OPENBFME_PORT247_TESTS
    tests/test_port_247.cpp)
