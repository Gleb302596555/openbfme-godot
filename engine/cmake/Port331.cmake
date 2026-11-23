# PORT-331: Shell, online and Living World stop audit and fixes (issue #848):
#   - S-923: SpellStoreModel hidden science -> state 2 (_disabled_level, RW 0x8231D4 -> 0x8227BB -> 0x6AC287),
#     TOOLTIP:ScienceDisabled only on disabled/hidden/missing prereqs (RW 0x823057 .. 0x823118 -> RW 0x5FED05),
#     InGameSpellBookModel disabled/hidden owned science -> usable=false (RW 0x942733 case 0x26, RW 0x943343 .. 0x943407).
#   - S-173: WindowManager::hoverPath reads currentButton() (gApt + 0x6C) only, never falling back to hoverClip()
#     (RW 0x624F7F -> RW 0xAE1270 / RW 0xAE1410 -> RW 0xAE13A0).
#   - S-1922: AptPlayerTribute _InitTextEntry sets numericalOnly=true (RW 0x721FA9 0x21) and maxTextLen=5 (RW 0x81606D),
#     and tributeDisplayCash adds AIStatCollector::reserve() for AI players (RW 0x9155C2 -> RW 0x6A950B / 0x6A9999).
#   - S-1364: ScriptCameraDirector::startMissionFadeUp implements MillisecondsAfterStartToStartFadeUp (RW 0x5EACF9 / 0x627062).
#   - S-5016, S-1402, S-1342: verified in RotWK 2.01 game.dat.

set(OPENBFME_PORT331_TESTS
    tests/test_port_331.cpp
)
