# PORT-282 (fork issue #649): the repair orders of the dozer and the worker.
# The lane's engine change is in files other lists already carry (GameLogic/Module/DozerAIUpdate.cpp: the dozer interface's slot 0x50
# canAcceptNewRepair RW 0x88CB8A and its first use in DozerAIUpdate::repair RW 0x88BDA2; GameLogic/Module/WorkerAIUpdate.cpp: the worker's own
# slot 0xB0 repair order RW 0x8AD2D3); this file adds the lane's tests only.

set(OPENBFME_PORT282_TESTS
    tests/test_port_282.cpp)
