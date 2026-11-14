# PORT-176 (P-107, issue #429): Mordor's Desecrated earth (FX_TaintSpellFX1 -> DynamicDecal exmordorquake02).
# OPENBFME_PORT176_SOURCES: terrain-conforming DynamicDecal mesh & shader/depth-bias/render-priority parameters
# (W3DProjectedShadowManager::queueDecal / flushDecals RW 0x50C050 .. 0x50D414, 0x509800 .. 0x50A115, 0x5E05EA, 0x4E2F85).
# ParticleTexture.{h,cpp} and GodotDevice/GodotFXPlayer.{h,cpp} + GodotGameWorld.cpp + GodotTerrainScorches.h are in their own lists.

set(OPENBFME_PORT176_SOURCES
    src/GameClient/DynamicDecalMesh.cpp)

set(OPENBFME_PORT176_TESTS
    tests/test_port_176.cpp)
