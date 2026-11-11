# PORT-112: HordeContain/NotComboFormation — the row RW 0xC5B8BC (table RW 0xC5BD50, module data + 0x25C) is read by the getter RW 0x872B91
# (interface slot 136) and by one caller only: AICombineState::onEnter RW 0x751716 -> RW 0x751738 -> slot 0x60 RW 0x86C7F6 (the formation swap). The
# lane's engine change sits in files other lists already carry (GameLogic/Object/Contain/HordeContainRuntime.*, GameLogic/Module/SpecialPowerModules.cpp).

set(OPENBFME_PORT112_TESTS
    tests/test_port_112.cpp
)
