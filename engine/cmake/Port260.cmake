# PORT-260 (issue #608): the storm powers — ArrowStormUpdate's own allowsSpecialPowerUse (RW 0x893D61, the special power update interface's slot 8,
# asked by the module's location requirements RW 0x897E01 with the location) and CloudBreakSpecialPower's fire put-out (RW 0x8C8A3B -> RW 0x68F383 ->
# RW 0x8B4E75). The engine changes are in files other lanes already build (HeroAbilityModules, SpellBookPowers, ObjectWeapons,
# SpecialAbilityModules); this file adds the lane's tests only.

set(OPENBFME_PORT260_TESTS
    tests/test_port_260.cpp)
