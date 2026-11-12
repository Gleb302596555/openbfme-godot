# PORT-133 (issue #335, P-64 "Erkenbrand's Time to Strike does not work"): RotWK's attack kind 8 (RW 0x74D01B .. 0x74D02A), the charge attack machine of a
# weapon with CanFireWhileCharging (RW 0x74523F, AIChargeTargetState RW 0xC29708). Synthetic-INI tests for openbfme_tests, no game files.
list(APPEND OPENBFME_SIM_SOURCES
    src/GameLogic/AI/AIAttackCharge.cpp)
set(OPENBFME_PORT133_TESTS
    tests/test_port_133.cpp)
