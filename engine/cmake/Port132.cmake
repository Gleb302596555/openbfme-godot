# PORT-132 (issue #334): Locomotor AeleronCorrectionDegree / AeleronCorrectionRate / ElevatorCorrectionDegree /
# ElevatorCorrectionRate - the ship body of Drawable::calcPhysicsXform (RW 0x6732DC) reads them. The engine change is in
# src/GameClient/Drawable.cpp, which the lists below already carry; only the test file is new.

set(OPENBFME_PORT132_TESTS
    tests/test_port_132.cpp)
