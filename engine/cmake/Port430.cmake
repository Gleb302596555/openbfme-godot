# PORT-430 (fork issue 430, P-109): a model texture named `None` (GBFARM_BIB / ANGPORTER.BUCKET / CAH_skull) is a name
# retail's compiled-texture loader never opens (RW 0x530D59: no dot, no candidate), so its 1 x 1 texture (RW 0x53193E)
# is drawn and nothing is reported. The engine change is in Libraries/WWVegas/WW3D2/textureloader.{h,cpp}, which the
# core list already carries; the consumers are GodotW3DMaterial.cpp and GodotW3DModelBuilder.cpp.
set(OPENBFME_PORT430_TESTS
    tests/test_port_430.cpp)
