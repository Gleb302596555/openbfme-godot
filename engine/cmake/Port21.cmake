# PORT-21 (issue #77, INAUDIBLE when an object leaves the world — RW 0x690728 / 0x690772 called from RW 0x692313 / 0x6923B7):
# tests for the openbfme_tests executable. The lane's engine change is in files other lists already carry
# (src/GameLogic/Object/Object.cpp setInaudible / setAudible, src/GameLogic/GameLogic.cpp friend_containLeaveWorld /
# friend_containEnterWorld, src/GameLogic/Module/BehaviorModule.h interface 36, src/GameClient Drawable::setAudible and the
# AUDIBLE client event).

set(OPENBFME_PORT21_TESTS
    tests/test_port_21.cpp)
