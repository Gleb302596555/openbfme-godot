# PORT-439 (issue #439, Port 186): the script action FOCAL_LENGTH_CAMERA (ordinal 389, RW 0x7CAFA5 case 0x185 -> RW 0x7CB6FB -> RW 0x7BB627 ->
# TheTacticalView vslot 0xF0 = View::setFocalLength RW 0x48885E, stepped by RW 0x4866A7). The engine files are already carried by the core lists.
set(OPENBFME_PORT439_TESTS
    tests/test_port_439.cpp)
