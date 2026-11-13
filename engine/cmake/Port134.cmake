# PORT-134 (issue #336): the pick of a model's aligned box (a W3D BOX chunk with W3D_BOX_ATTRIBUTE_ORIENTED clear, an AABoxRenderObjClass): the mod's head on
# a pike is drawn but could not be clicked because its selectable volume (art\w3d\bb\bbox.w3d, Attributes 0x2, no meshes) was cast by nothing.
set(OPENBFME_PORT134_TESTS
    tests/test_port_134.cpp)
