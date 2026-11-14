# PORT-188 (issue #454, P-115): SpecialAbilityUpdate::initiateIntentToDoSpecialPower (RW 0x853076..0x85309A,
# BFME2 0x004500A3) only aborts another SpecialAbilityUpdate when its isPowerCurrentlyInUse(nullptr) (RW 0x851C92)
# is true, so a companion UpdateModuleStartsAttack = Yes / ActivateModuleSpecialPower triggered via CommandTrigger
# (Command_SpecialAbilityRadagastConvertEnemyAdler -> Command_SpecialAbilityConvertEnemyTracker) does not cancel
# ReplaceObjectUpdate before it unpacks, replaces enemies, and starts its recharge timer.
# No new engine source files: the fix sits in SpecialAbilityModules.cpp which libopenbfme_core already carries.
set(OPENBFME_PORT188_TESTS
    tests/test_port_188.cpp
)
