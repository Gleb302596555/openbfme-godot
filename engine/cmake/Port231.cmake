# PORT-231 (issue #554): building, producing, earning layer
# - GameData SellPercentage ctor default 1.0f (RW 0x00643160 -> 0x00643222, S-1981)
# - CastleMemberBehavior::BeingBuiltSound (RW 0x00799F0F -> 0x00798057 -> 0x00797C40, HELD_CASTLE_BEING_BUILT = 6)
# - ProductionUpdate::completeUnit script naming (RW 0x008A253B / 0x008A2624) and hordeTarget migration (RW 0x008A2A67 -> 0x008A171F -> 0x008A07E0)
# - Object::attemptHealingFromSoleBenefactor KeepSelectableWhenDead tail (RW 0x00690584 -> 0x00690626, S-652)

set(OPENBFME_PORT231_TESTS
    tests/test_port_231.cpp)
