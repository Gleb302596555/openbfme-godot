# PORT-314 (issue #750, S-561 / S-4970): the ghost objects — a destroyed IMMOBILE object keeps its shroud record for the players that saw it fogged
# (RW 0xB4E190, the retention branch of RW 0xB4F510; the ghost itself RW 0x691602 / TheGhostObjectManager RW 0xDE4BD0). The engine changes are in
# GameLogic/System/ShroudManager.{h,cpp} (the record's ghost and retained bytes, registerObject's eligibility, unregisterObject's retention and update()'s
# release), GameLogic/System/ShroudManagerSnapshot.cpp (the two bytes, version 1.2) and GameClient/LogicSnapshot.{h,cpp} + GameClient/Drawable.cpp (the frozen
# record the client draws the ghost from).

set(OPENBFME_PORT314_TESTS
    tests/test_port_314.cpp)
