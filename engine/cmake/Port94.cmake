# PORT-94 (issue #242): the tests for the SalvageCrateCollide LevelUpRadius / ResourceChance / BannerChance verdict. The lane changes
# no engine code (the fields are parsed and unread in RotWK 2.01 — docs/ennorath/port94-salvage-crate-rw.md, stop S-7234); the tests
# hold the RW 2.01 behaviour of a picked-up crate against the synthetic crates of test_port_94.cpp.
set(OPENBFME_PORT94_TESTS
    tests/test_port_94.cpp)
