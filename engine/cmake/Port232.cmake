# PORT-232 (issue #555): HordeDispatchSpecialPower::doSpecialPowerAtObject (RW 0x8CCF76) hands the horde's own base call the member's
# kept target (the special power interface's slot 0x50, RW 0x7B0F25; the target id the initiate keeps at RW 0x897E9E .. 0x897EA3) farthest from
# the horde (squared distance RW 0x5E3BE4), not the order's target. The member keep is SpecialPowerModule's + 0x2C target id (a new field here;
# new sim state, xfer version 2 and the crc, in the same commit).
set(OPENBFME_PORT232_TESTS
    tests/test_port_232.cpp
)
