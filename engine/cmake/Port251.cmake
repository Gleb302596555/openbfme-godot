# PORT-251 (issue #599, arena-task 227): SpecialPowerModule missing RotWK 2.01 methods
# (updateRequirementsPause RW 0x896A32 via Player::update RW 0x6ADD22, m_targetObjectID +0x2C RW 0x897EA3 / 0x7B0F25,
# m_updateDisabled +0x28 set RW 0x897FD9..0x897FEB and cleared via slots 0x44/0x4C RW 0x896751 from StopSpecialPower
# RW 0x8CAC5E and SpecialAbilityUpdate::finishAbility RW 0x85314C, SpecialPowerModuleInterface::isPaused RW 0x6519A6 /
# 0x89825A in ControlBar, and SpecialPowerModule::crc / xfer v2 RW 0x89679D).

set(OPENBFME_PORT251_TESTS
    tests/test_port_251.cpp)
