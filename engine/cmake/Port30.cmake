# PORT-30 (issue #102, OneRingPenaltyUpdate/TimeFrozenFromPenalty — row RW 0xC66B68 (table RW 0xC66B18) +0x1C, parser RW 0x73A429, defaults RW 0x89D1C1, never read: ctor 0x89D21F / vtables C669C8/C669B8 / update 0x89D647 / helpers 0x89D0E3..0x89D712 only +0xC/+0x10/+0x14/+0x18; 0x8CA072/0x8CA12A is deploy RaiseDelay, no other lookup): verdict tests (no code change, S-5604).
set(OPENBFME_PORT30_TESTS
    tests/test_port_30.cpp)
