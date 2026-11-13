# PORT-159 (issue #377, P-83): the mumak and the dwarven battle wagon have no shadow on Ultra.
#
# RW analysis (workspace/RotWK/game.dat, caveat S-001; docs/ennorath/PORT159_RW_ANALYSIS.md):
#   * Shadow type word names RW 0xD99EB0: entry 7/8/9 = SHADOW_VOLUME_NON_SELF_1/2/3 = 0x80/0x100/0x200.
#   * W3DShadowManager::addShadow RW 0x49A2EB: the type is ShadowTypeInfo +8 (RW 0x49A302, default 2);
#     0x4000 / 0x8000 first resolved through GlobalData + 0x60 (RW 0x49A317..0x49A355);
#     types 2 / 0x80 / 0x100 / 0x200 -> W3DVolumetricShadowManager::addShadow RW 0x4F24A8 (RW 0x49A358..0x49A38E);
#     type 4 -> the V2 manager RW 0x507CAA (RW 0x49A39A);
#     types 1 / 0x20 / 0x40 / 0x400 / 0x800 / 0x1000 / 0x10000 -> W3DProjectedShadowManager::addShadow RW 0x50BF6A
#     (RW 0x49A3B2..0x49A3F6); anything else returns NULL (RW 0x49A3DA).
#   * The volumetric manager's gate RW 0x4F24A8: stencil (RW 0x51C760), render object, GlobalData + 0x60
#     (UseShadowVolumes, RW 0x4F24D7), render-object name, shadow geometry, then a 0x4904-byte W3DVolumetricShadow
#     (RW 0x4F2520) storing the render object at + 0x70, tan(ShadowSunAngle * 0.017453) as the length scale
#     (RW 0x4F2577) and the type word at + 0x34 (RW 0x4F25A7).
#   * Ultra clears UseShadowVolumes / UseShadowDecals when UseShadowMapping is set (RW 0x601D59 / 0x601D61).
#
# The engine change is in a file other lists already carry (GameClient/ObjectShadow.h, ObjectShadow::MapMask);
# the client call sites are GodotDevice/GodotGameWorld.cpp and GodotDevice/GodotMapObjectBuilder.cpp.
set(OPENBFME_PORT159_TESTS
    tests/test_port_159.cpp
)
