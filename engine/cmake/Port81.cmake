# PORT-81: HordeContain/RanksToJustFreeWhenAttacking — the row RW 0xC5BB90 is parsed and stored at module data +0x1C4;
# RotWK 2.01 never reads it (issue #213). The tests pin the parse and that the attack path is the +0x1B8 one only; no engine
# source is added by this lane (docs/ennorath/PORT81_RW_ANALYSIS.md).

set(OPENBFME_PORT81_TESTS
    tests/test_port_81.cpp
)
