# PORT-209 (fork issue 491, stops S-7317 / S-6028 / S-587): the horde's banner carrier made at build complete (the tail of RotWK's HordeContain
# onBuildComplete RW 0x8718F5 .. 0x87190E -> the carrier spawn RW 0x870204), the carrier's hand-over RW 0x876406 and the banner tie-break of two combining
# hordes (RW 0x8768A0 .. 0x8768FC inside RW 0x876481). The lane's engine changes are in files another list already carries
# (GameLogic/Object/Contain/HordeBanner.cpp, HordeContainRuntime.cpp, GameLogic/Construction.cpp).

set(OPENBFME_PORT209_TESTS
    tests/test_port_209.cpp)
