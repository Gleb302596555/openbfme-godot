# PORT-75 (issue #207): BannerCarrierUpdate's MorphCondition (RW 0x89A90F) and BannerMorphFX (data + 0x30): a banner carrier that joins a horde takes
# the look of the horde it leads (RW 0x89A5C5 through the contain's accept path). The engine change is in files other lists already carry
# (Module/BannerCarrierUpdate.h, Object/Contain/HordeBanner.cpp, HordeContainRuntime.cpp/.h).

set(OPENBFME_PORT75_TESTS
    tests/test_port_75.cpp)
