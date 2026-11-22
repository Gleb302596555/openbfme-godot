# PORT-224 (issue #534, P-143): destroying a creep lair ruin or worker-rebuilt
# structure while it is rebuilding cancels the build, fades the masked hole
# without treasure or bounty, and prevents the hole from respawning.
set(OPENBFME_PORT224_TESTS
    tests/test_port_224.cpp)
