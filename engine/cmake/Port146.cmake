# PORT-146 (issue #363): the objects on a bone of a moving container (P-82, the Grond's archers; the same pose carries the mumak's riders, P-80).
# OPENBFME_PORT146_SOURCES: the drawn pose of a contained object (the client part S-1104 records as the drawable bone attachment; GameClient/ContainedPose.h
# holds the reasoning and the addresses). The rest of the lane's engine changes are in files other lists already carry
# (GameClient/Drawable.cpp, GameClient/DrawableManager.cpp, GameClient/LogicSnapshot.*, GameLogic/Object/Contain/OpenContainRuntime.cpp).

set(OPENBFME_PORT146_SOURCES
    src/GameClient/ContainedPose.cpp)

set(OPENBFME_PORT146_TESTS
    tests/test_port_146.cpp)
