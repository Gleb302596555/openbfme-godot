# PORT-60 (issue #170, EvaAnnounceClientCreate/CreateFakeRadarEvent): the RotWK 2.01 verdict, pinned.
# The flag is parsed (field row RW 0xC75958, parser RW 0x42E558, data +0x1B) and then never read: clientUpdate
# RW 0x8CDF07, create slot RW 0x8CDED6, xfer RW 0x8CDDD7. Tests pin the parse, the flag-independent announce
# decision and xfer bytes, and the neighbour members. No engine behaviour changed.
# See docs/ennorath/port60-evaannounce-rw.md; docs/STOPS.md S-7212.
set(OPENBFME_PORT60_TESTS
    tests/test_port_60.cpp)
