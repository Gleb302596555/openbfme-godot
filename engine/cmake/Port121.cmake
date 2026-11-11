# PORT-121 (issue #305, P-61 "a builder's water flies upward when he puts out a fire"): synthetic-INI tests for openbfme_tests.
# RW 0x4C6514 (the ParticleSysBone creation) reads the bone from the render object - RW 0x4C678F / slot +0xCC Get_Bone_Transform and
# RW 0x4C67B6 setLocalTransform for a FollowBone record, RW 0x4C69BE / RW 0x4C6A31 / RW 0x4C6A4B (position and Get_Z_Rotation) for a
# record without it - i.e. the pose the draw shows, not the bind pose; RW 0x5F4C6B then rotates the emission velocity by that transform,
# which is the direction of the jet of FXParticleSystem PorterFireWater (OrthoEmissionVelocity, the mod's porters' ParticleSysBone = BUCKET).
set(OPENBFME_PORT121_TESTS
    tests/test_port_121.cpp)
