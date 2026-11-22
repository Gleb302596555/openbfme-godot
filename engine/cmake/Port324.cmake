# PORT-324 (fork issue #841): sweep and port of client drawing, Drawable, W3D, HUD, and audio stops
# (S-484, S-706, S-830, S-1001, S-1950, S-1951, S-4612, S-4974, S-5204, S-6013).
# Engine changes live in existing client/logic/W3D/Godot files; this file registers the lane's unit tests.

set(OPENBFME_PORT324_TESTS
    tests/test_port_324.cpp)
