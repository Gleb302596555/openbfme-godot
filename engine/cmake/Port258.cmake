# PORT-258 (issue #606): RotWK 2.01 alignment for powers on enemy units —
# DominateEnemySpecialPower (PartitionFilterSameMapStatus RW 0x008D10B4 / 0x00660E93),
# CurseSpecialPower (CursePercentage default 1.0f RW 0x008D1192 / 0x00BD1908, UNSELECTABLE status 3 RW 0x008D130B,
# getFinalOverride RW 0x008D138D), DeflectSpecialPower (XFER_CRC early-out RW 0x008C99FB / 0x008C9A0D),
# and SpecialPowerTimerRefreshSpecialPower (RW 0x008C7B26 / 0x0068BDF9 / 0x00896F99).
# The engine changes live in HeroAbilityModules.{h,cpp} and DeflectSpecialPower.{h,cpp}.

set(OPENBFME_PORT258_TESTS
    tests/test_port_258.cpp)
