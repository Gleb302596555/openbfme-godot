# PORT-92 (issue #240, GiveUpgradeUpdate/SpawnOutFX): synthetic tests for the openbfme_tests executable. The engine change is in
# src/GameLogic/Module/GiveUpgradeUpdate.cpp (the delivery's closing part RW 0x8A02BB -> RW 0x89FE01, the FX call RW 0x89FE1C..RW 0x89FE35
# through RW 0x494615, the porter's position, no matrix): SpawnOutFX (ModuleData +0xE0, field table RW 0xC67AE8) now reaches the client
# as a POSITION_FX event, like the other FXList::doFXPos call sites.

set(OPENBFME_PORT92_TESTS
    tests/test_port_92.cpp)
