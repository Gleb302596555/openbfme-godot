# PORT-85 (issue #233, UnitSpecificSounds VoicePrimaryWeaponMode / VoiceSecondaryWeaponMode): RotWK 2.01 verdict, pinned.
# The three Voice*WeaponMode rows (RW 0xC77B14 / 0xC77B2C / 0xC77B48) are read only in RW 0x8DF74F (case 0x43A,
# MSG_SWITCH_WEAPONS, posted by GUI_COMMAND_SWITCH_WEAPON RW 0x941204 and GUI_COMMAND_TOGGLE_WEAPON RW 0x941257).
# GUI_COMMAND_TOGGLE_WEAPONSET (RW 0x9412C8 -> 0x457 MSG_WEAPONSET_TOGGLE -> RW 0x941A22 -> RW 0x75D22E, logic RW 0x77B529)
# plays no voice, and RW 0x8DEDBB rejects 0x457 at RW 0x8DF2A6..0x8DF2A9 (0x457 - 0x3E9 = 0x6E > 0x6D -> ja 0x8DFEA4).
# No engine code changed. See docs/ennorath/port85-weapon-mode-voice-rw.md and docs/STOPS.md S-7285.

set(OPENBFME_PORT85_TESTS
    tests/test_port_85.cpp
)
