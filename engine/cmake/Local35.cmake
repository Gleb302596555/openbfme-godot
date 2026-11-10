# Lane LOCAL-35 / issue #285: the game's system objects as retail (debris, the map reveal ping, the damage warning).
list(APPEND OPENBFME_SIM_SOURCES
    src/GameLogic/Module/DynamicShroudClearingRangeUpdate.cpp
    src/GameLogic/Module/DelayedLuaEventUpdate.cpp
)
set(OPENBFME_LOCAL35_TESTS
    tests/test_local_35.cpp
)
