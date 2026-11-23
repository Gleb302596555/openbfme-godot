# PORT-296 (fork issue #688): the castle's crew — the preparation of an owned crew member (RW 0x798397), its release (RW 0x798344 in RW 0x79A237) and the
# crew's clock in the unpacked state (RW 0x79B0F1), plus CastleMemberBehavior's BeingBuiltSound (RW 0x798057, the hook of
# CastleBehavior::setMembersModelCondition RW 0x799F0F). The engine changes live in files other lists already carry (CastleModules, ActiveBody,
# AudioRequests / AudioEntryPoints); this file is only the test list. Synthetic INI, no game files.
set(OPENBFME_PORT296_TESTS
    tests/test_port_296.cpp)
