# PORT-280 (fork issue 647): ProductionSpeedBonus consumer (Player production time change maps + timers, BuildAssistant::calcTimeToBuild, ProductionUpdate
# SetBonusModelConditionOnSpeedBonus / BonusForType / SpeedBonusAudioLoop). Isolated CMake file so parallel ports touch two lines of CMakeLists.txt at most.
set(OPENBFME_PORT280_TESTS
    tests/test_port_280.cpp
)
