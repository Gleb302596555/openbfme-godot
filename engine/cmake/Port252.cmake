# Port 252 (issue #600): SpecialAbilityUpdate and subclass retail parity (RotWK 2.01):
# ~SpecialAbilityUpdate (RW 0x00854D9A), ParalyzeDurationWhenCompleted/Aborted -> aiBusyFor (RW 0x0085338E),
# SpecialPowerModule::clearUpdateDisabled (RW 0x00896751), handlePackingProcessing 0x1D early finish (RW 0x00851F39),
# OBJECT_STATUS_SUMMONING_REPLACEMENT (0x66) command-abort guard (RW 0x00854F05), SPECIAL_GRAB_CHUNK CHUNK_VENDOR
# model condition 176 (RW 0x0085384C), startPreparation contain->setWeaponStateOfRider (RW 0x0086A43E),
# restartPersistent 0x27 SHRUBBERY/GRAB_AND_KILL gate (RW 0x008524EA), ModelConditionSpecialAbilityUpdate
# PartitionFilterSameMapStatus (RW 0x00660E93), onEffectExpired m_effectExpiry reset (RW 0x00851E9A).
set(OPENBFME_PORT252_TESTS
    tests/test_port_252.cpp
)
