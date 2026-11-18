# PORT-278 (fork issue 645): the buff powers whose attribute modifier changes nothing — Boromir's horn, the Dorwinion nature mage's harvest blessing
# and Ghan-buri-Ghan's secret paths. The engine is identical to RotWK 2.01 on every step of the apply path (see docs/ennorath/PORT278_ANALYSIS.md); this lane
# carries no engine change, only the synthetic tests that pin the binary's rules (filter shapes, the empty-mask NONE rule, the absent filter, expiry).

set(OPENBFME_PORT278_TESTS
    tests/test_port_278.cpp)
