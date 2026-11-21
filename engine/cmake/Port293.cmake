# PORT-293 (issue #685): ActiveBody's uncited own functions -- the last-healing timestamp (module + 0xC0, the body interface's slot 0x48) and the
# AutoHealBehavior NonStackable rule that reads it (RW 0x855797). No extra engine source files: the fix is in ActiveBody, BehaviorModule.h (the interface
# default) and HeroModules' healOne, which other lists already carry.

set(OPENBFME_PORT293_TESTS
    tests/test_port_293.cpp)
