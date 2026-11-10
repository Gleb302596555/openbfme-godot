# PORT-72 (issue #186): FactionVictoryData — the per-faction factors of TheVictorySystem. Stop verdict 2026-11-09 (S-7216):
# RotWK 2.01 reads the factors only inside TheVictorySystem, a subsystem the port does not have; no engine source is added
# by this lane (docs/ennorath/PORT72_RW_ANALYSIS.md). The tests pin the verdict's conditions.

set(OPENBFME_PORT72_TESTS
    tests/test_port_72.cpp
)
