# PORT-191 (issue #458, S-1227): AutoAbilityBehavior without a Query (RW 0x85DD3A -> RW 0x701443 flags 0x4A),
# AI + 0x34 (RW 0x85D693 .. 0x85D6A1) and + 0x48 (RW 0x85DD69 .. 0x85DD70), vision range modifiers (RW 0x85DB30 -> RW 0x68E43B),
# and the disabled object's auto-cast exception (RW 0x855830 + RW 0x68E5A3 / 0x68E67A / 0x68E754).
set(OPENBFME_PORT191_TESTS
    tests/test_port_191.cpp
)
