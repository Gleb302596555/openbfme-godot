# PORT-105: TransportContain/ReleaseSnappyness — the field (data +0x17C, row RW 0xC5ADC8) is read on the release path only: TransportContain::onRemoving
# (RW 0x86ADA1) hands it to the rider's drawable (RW 0x67476B), whose modules glide the drawn transform from the release pose to the object's own
# (RW 0x4B6B21 .. 0x4B6BB2). No engine source is added by this lane: the change is in files other lists already carry (the ObjectClientHooks in
# GameLogic.h, ClientEvents, DrawableManager, Drawable, TransportContainRuntime).

set(OPENBFME_PORT105_TESTS
    tests/test_port_105.cpp
)
