# PORT-263: Emotions — EmotionTrackerUpdate (terror, fear, cheering, idle emotions) and BoredUpdate (fork issue 611). The lane's engine changes are in
# files other lists already carry (GameLogic/Module/EmotionModules.cpp, GameLogic/Module/BoredUpdate.cpp).
#   * EmotionTrackerUpdate: the nugget vector's count (getter RW 0x8B50BE), the destructor RW 0x8B56CF, the +0x20 interface's slot RW 0x8B56EB
#     (no caller: S-7507), the fear scan's distance order (RW 0x8B5B35 -> RW 0x66352C, then the filters RW 0x7640C1), the snapshot RW 0x8B5FD0.
#   * BoredUpdate: the data's defaults (constructor RW 0x899E8C: RW 0xD9F608, RW 0xBD9E90, RW 0x763D11), the update RW 0x899F9D / RW 0x8B66B4.

set(OPENBFME_PORT263_TESTS
    tests/test_port_263.cpp
)
