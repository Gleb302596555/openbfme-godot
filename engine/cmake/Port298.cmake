# PORT-298 (issue #690): RespawnUpdate — DeathAnimationTime parsed but never read (S-7323) and the four own
# functions the map had uncited (xfer RW 0x8B319D, getModuleNameKey RW 0x8B32C4, name RW 0x8B3309, the deleting
# destructor RW 0x8B332D). Synthetic INI tests, no game files.
set(OPENBFME_PORT298_TESTS
    tests/test_port_298.cpp)
