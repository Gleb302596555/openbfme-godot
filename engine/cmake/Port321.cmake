# PORT-321 (issue #757, task 292): the wall line build from the HUD - the release's restricted-area
# test (RW 0x707141, code 8, through PlaceEventTranslator::releaseLineBuild) and the line ghost
# (RW 0x6A2D38 ff: WallSpan::ghostSegments; the drawing, GodotPlacementGhost.cpp).
# The lane's engine change is in files other lists already carry (GameClient/MessageStream/PlaceEventTranslator.h/.cpp,
# GodotDevice/GodotPlacementGhost.cpp).

set(OPENBFME_PORT321_TESTS
    tests/test_port_321.cpp)
