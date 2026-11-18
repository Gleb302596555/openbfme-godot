# PORT-265 (fork issue 613): HordeContain's own module slot +0x7C, RW 0x0086DF44 — the once-every-5th-frame split check of the update (RW 0x8730C2)
# gated by SplitHordeNumber (module data + 0x258) and by the horde's rank, plus the EvaEventLastMemberDeath report of the last-member-death path
# (RW 0x86DF99). The lane's engine changes are in files other lists already carry (GameLogic/Object/Contain/HordeContainRuntime.cpp/.h); this file adds
# the lane's tests only.

set(OPENBFME_PORT265_TESTS
    tests/test_port_265.cpp)
