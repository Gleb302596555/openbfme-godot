# PORT-228 (fork issue 551): the Locomotor rudder, swoop and thrust fields. RotWK 2.01 stores them in the template,
# copies them with the template and reads none of them (docs/ennorath/PORT228_RW_ANALYSIS.md, stop S-7387).
# No engine source is added by this lane: the tests pin the parse table, the constructor defaults and the copy.

set(OPENBFME_PORT228_TESTS
    tests/test_port_228.cpp
)
