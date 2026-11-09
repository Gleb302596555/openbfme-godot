# PORT-50 (issue #154): W3DModelDraw/ParticlesAttachedToAnimatedBones - the ParticleSysBone systems follow the animated bones
# (the gate RW 0x4C7795 of doDrawModule RW 0x4C72BE and the per-frame walk RW 0x4B6D5F it calls through the draw module's +0xC
# object, slot 27). The engine change is client only: src/GameClient/ParticleBoneFollow.{h,cpp} and the follow call in
# src/GameClient/LiveFX.cpp (both already listed by cmake/Fx2.cmake).

set(OPENBFME_PORT50_TESTS
    tests/test_port_50.cpp)
