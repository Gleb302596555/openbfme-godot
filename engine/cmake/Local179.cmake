# LOCAL-179 (fork issue 960): script engine part 1 binary-map audit —
# Script v>=4 LivingWorldTurnPhase bitmask parsing (RW 0x7B636C / 0x7B616D / 0x7B5038)
# and ScriptEngine::shouldEvaluate turn-phase gating (RW 0x603878).
set(OPENBFME_LOCAL179_TESTS
    tests/test_local_179.cpp)
