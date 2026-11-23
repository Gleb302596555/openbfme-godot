# PORT-323 (issue #840): the Object class' open stops — the 25 named rows. This lane re-read every row at the branch point, set the
# verdict (left / narrowed / closed) and narrowed the stale S-1363 note; the engine code change is the campaign stop line of S-1363.
set(OPENBFME_PORT323_TESTS
    tests/test_port_323.cpp)
