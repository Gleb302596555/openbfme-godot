# PORT-34 (issue #119, GameData/DefaultStructureRubbleHeight + Object/StructureRubbleHeight — ActiveBody::setCorrectDamageState RW 0x8C2D87..0x8C2E13 -> Object::setGeometryMaxHeight RW 0x68B2CB -> GeometryInfo::setMaxHeightAbovePosition RW 0xAD2C60 + calcBoundingStuff RW 0xAD2860; S-342 narrowed).
set(OPENBFME_PORT34_TESTS
    tests/test_port_34.cpp)
