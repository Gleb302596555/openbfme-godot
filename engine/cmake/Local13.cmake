# Lane LOCAL-13 / Issue #134 (TOP20 #2, Stop S-709): Eva sighting announcements (RW 0x678C69)
# logic half (reads only: partition scan RW 0x672DFB, owner walk RW 0x672D90)
list(APPEND OPENBFME_SIM_SOURCES src/GameLogic/Object/EvaSightingNotice.cpp)
# client presentation
set(OPENBFME_LOCAL13_CLIENT_SOURCES
    src/GameClient/EvaSighting.cpp
)
set(OPENBFME_LOCAL13_TESTS
    tests/test_local_13.cpp
)
