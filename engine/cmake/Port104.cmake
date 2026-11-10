# PORT-104: CreateObjectDie/DebrisPortionOfSelf — the row RW 0xC61130 is parsed and stored at module data +0x3C;
# RotWK 2.01 never reads it (issue #264; docs/ennorath/PORT104_RW_ANALYSIS.md). The tests pin the parse and that a death
# creates the CreationList's objects only — no object of the named debris template; no engine source is added by this lane.

set(OPENBFME_PORT104_TESTS
    tests/test_port_104.cpp
)
