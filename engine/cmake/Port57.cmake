# PORT-57 (issue #167): RadiateFearUpdate/WhichSpecialPower — the field row RW 0xC67A00 (parseInt RW 0x42EC5E, offset +0xC,
# default -1 in the data constructor RW 0x89F7F6) is parsed-unused: no reachable reader in the image (the update RW 0x89F9A8,
# the constructor RW 0x89F890, the xfer RW 0x89F91F and every vtable slot of the class scan). The same verdict as Port 42 / S-7186.

set(OPENBFME_PORT57_TESTS
    tests/test_port_57.cpp)
