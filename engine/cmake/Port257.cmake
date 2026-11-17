# PORT-257: Flying monsters — GiantBirdAIUpdate (CollideModuleInterface / onCollide RW 0x76B56B + collision push in update RW 0x76CF57,
# states 0x3FD STATE_FLY_OFF_MAP / 0x3FE STATE_DESTROY_SELF RW 0x7695C2 / RW 0x7695A7 and AI command 0x41 RW 0x76A0E6, AIWorld::processGiantBirdContacts),
# FellBeastSwoopPower (AI vslot 0x1D0 RW 0x662B28 -> AIGiantBirdFollowThruState slot 0x34 RW 0x8BD372 in update RW 0x8CB2CE),
# and GiantBirdSlowDeathBehavior (verified against RotWK 2.01). Issue #605.

set(OPENBFME_PORT257_TESTS
    tests/test_port_257.cpp
)
