# PORT-530 (Local 72, fork issue 530): the button matrix meets each grey button's conditions and presses again
# (GREY-BUG / DISABLED: needs ...). The sweep's fixture planning and verdicts live in engine/tests/GreyFixture.h
# (header-only, no game files); the sweep itself (test_hud_factb_units.cpp) and tools/matrix_run.py carry the rest.
# No engine source changes: the lane only adds its tests.

set(OPENBFME_PORT530_TESTS
    tests/test_port_530.cpp)
