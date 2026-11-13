# Port 157 (issue #375, P-78): boot splash before the main menu.
# Retail's WinMain loads "<Language>Splash.jpg" falling back to ".bmp" into gLoadScreenBitmap
# (donor WinMain 0x00002C39, paint 0x00002??, BFME2 decomp WinMain.cpp:1038..1060) and paints it
# while the engine loads. The port must request that image before the shell starts; a missing
# image is a boot error, not a blank screen.

set(OPENBFME_PORT157_SOURCES
    src/Common/BootSplash.cpp
)

set(OPENBFME_PORT157_TESTS
    tests/test_port_157.cpp
)
