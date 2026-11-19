# PORT-267 (fork issue 626): the floor record's fade on death (W3DFloorDraw FloorFadeRateOnObjectDeath). The lane's engine changes live in files
# other lists already carry: GameClient/FloorFade.h (the record), GameClient/Drawable (the floor entry and updateFloorFades),
# GameClient/DrawableManager (the per-client-frame call) and GodotDevice/GodotGameWorld (the record's opacity on the floor's instance).

set(OPENBFME_PORT267_TESTS
    tests/test_port_267.cpp)
