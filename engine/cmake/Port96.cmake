# PORT-96 (issue #244): SlavedUpdate's client fade on the master's guard point (RW 0x8A6024 .. 0x8A6097).
# Synthetic INI only; no engine source is changed by this lane beyond SlavedUpdate::update() itself.

set(OPENBFME_PORT96_TESTS
    tests/test_port_96.cpp
)
