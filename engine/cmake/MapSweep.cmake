# The Ennorath map sweep (owner 2026-11-20): one map per run starts a skirmish and plays; tools/maps/map_sweep.py runs every mounted map
# and writes docs/ennorath/MAP_SWEEP.* (SKIP without OPENBFME_MAP_SWEEP_LIST / OPENBFME_MAP_SWEEP_MAP).

set(OPENBFME_MAPSWEEP_TESTS
    tests/test_ennorath_map_sweep.cpp
)
