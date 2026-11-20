# PORT-136 (issue #338: the summoned giant bird of Ennorath's spell book does not attack on its own): tests for the
# openbfme_tests executable. The lane's engine change is in AI/AIAttack.cpp — AIAttackState::createAttackMachine asks its AI for
# the bird's swoop-then-idle attack machine (RW 0x769C31 -> RW 0x769335) before it makes the ground machine, whose move (RW
# 0x6668CA -> the pathfinder's ground search RW 0x6FC18E) never gives a flyer a path. See docs/ennorath/PORT136_RW_ANALYSIS.md.

set(OPENBFME_PORT136_TESTS
    tests/test_port_136.cpp)
