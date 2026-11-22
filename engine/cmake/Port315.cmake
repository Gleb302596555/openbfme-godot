# PORT-315 (fork issue 751): AI hunt state (0x11, RW 0x741133) attack priority info (AI + 0x70, RW 0x66F084 / RW 0x74809D) and crate pick-up verdict
# (AI + 0x238 / state 0x27 unreachable in RotWK 2.01 because RW 0x66810B clears AI + 0x238 before findObjectByID(0) RW 0x449681, and sole writer CreateCrateDie
# RW 0x888A24 has 0 INI uses S-7211; skirmish AI hordes collect crates during hunt via PickupStuffUpdate RW 0x8957B7, S-1422). Stop S-1188 narrowed.

set(OPENBFME_PORT315_TESTS
    tests/test_port_315.cpp
)
