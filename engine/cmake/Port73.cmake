# PORT-73 (issue #205): W3DTreeDraw's DoTopple / MoveOutwardTime / MoveInwardTime / MoveOutwardDistanceFactor / DarkeningFactor — the shrub push aside
# (W3DShrubBuffer's unitMoved RW 0x4E70F5, the push RW 0x4E6C74, the per-frame step RW 0x4E8519, the bend RW 0x4E7B25 / RW 0x4E7B4D). The engine change is
# in files other lists already carry (GameClient/TreeTopple.cpp via Spell.cmake, GameLogic/Map/TerrainTrees.cpp, GodotDevice/GodotGameWorld.cpp).

set(OPENBFME_PORT73_TESTS
    tests/test_port_73.cpp
)
