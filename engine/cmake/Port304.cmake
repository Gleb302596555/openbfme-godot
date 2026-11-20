# PORT-304 (fork issue 696): GiantBirdSlowDeathBehavior's CrashAvoidStrength and NeedToMaintainFlailingHeight. RotWK 2.01 parses them and reads neither
# (the chain is in GameLogic/Module/GiantBirdSlowDeathBehavior.h; stop S-5606 narrowed). No engine source is added by this lane: the tests pin the parse,
# the constructor defaults and that the crash does not depend on them.

set(OPENBFME_PORT304_TESTS
    tests/test_port_304.cpp
)
